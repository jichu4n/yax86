#!/usr/bin/env node

// Drives the PicoCalc runtime over USB serial: reads its screen, presses keys
// for the guest and asks for its report, through the commands in
// src/command.h.
//
//   ./debug.js screen [--attrs]
//   ./debug.js screenshot screen.png
//   ./debug.js type 'DIR C:\r'
//   ./debug.js key f6 [--state press|release]
//   ./debug.js wait 'A>' [--timeout SECONDS]
//   ./debug.js report
//   ./debug.js ping
//   ./debug.js run SCRIPT
//
// The board takes a key at a time, as the PicoCalc's keyboard sends them, so
// text to type is sent as the keys that type it: a character's own key, with
// Shift held around the ones that need it, as a hand would. It takes \r, \n,
// \t, \b, \e and \\ escapes, since a shell makes it awkward to pass the
// characters themselves. A pattern to wait for is a
// JavaScript regular expression, matched against the screen's rows joined by
// newlines, each row padded with spaces to the screen's width, and with ^ and
// $ matching at the start and end of each row.
//
// A script is the same commands a line each - "type DIR\r", "wait A>",
// "sleep 1.5" - with # for a comment, run in order in one session. A wait
// that times out stops the script and exits non-zero, which is what makes a
// script a test.
//
// A wait matches whatever is on the screen, and a key is answered once it is
// queued, not once the guest has it, so a wait straight after typing can see
// the screen from before. Wait for something only the new input can produce:
// after "type CLS\r", the prompt alone on a blank screen -
// "wait ^A> *\n(?![\s\S]*\S)" - rather than any prompt.
//
// A screenshot is the panel read back, all 320x320 of it - the guest's screen
// and the status lines below it, as the panel holds them - written as a PNG.
//
// The port is opened once for the whole session, for the reason in
// ../bench/serial.js. Lines the board sends unasked - its logs - are shown as
// they come.

const fs = require('fs/promises');
const {parseArgs} = require('util');
const zlib = require('zlib');
const {parseLine, formatLine, formatValue, toNumber} = require('../bench/logfmt');
const {delay, waitForPort, openPort} = require('../bench/serial');

// Code page 437, which the guest's text is in, as Unicode.
const kCP437 =
  ' ☺☻♥♦♣♠•◘○◙♂♀♪♫☼►◄↕‼¶§▬↨↑↓→←∟↔▲▼' +
  ' !"#$%&\'()*+,-./0123456789:;<=>?' +
  '@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_' +
  '`abcdefghijklmnopqrstuvwxyz{|}~⌂' +
  'ÇüéâäàåçêëèïîìÄÅÉæÆôöòûùÿÖÜ¢£¥₧ƒ' +
  'áíóúñÑªº¿⌐¬½¼¡«»░▒▓│┤╡╢╖╕╣║╗╝╜╛┐' +
  '└┴┬├─┼╞╟╚╔╩╦╠═╬╧╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀' +
  'αßΓπΣσµτΦΘΩδ∞φε∩≡±≥≤⌠⌡÷≈°∙·√ⁿ²■\u00A0';
if ([...kCP437].length !== 256) {
  throw new Error('the code page 437 table is not 256 characters');
}
const kCP437Characters = [...kCP437];

// How long to wait before sending again a key the board had no room for.
const kKeyRetryMs = 50;
// The keys for the characters text can have that are not printable, and for
// the space, which a key name cannot be.
const kTextKeyNames = {
  '\r': 'enter',
  '\n': 'enter',
  '\t': 'tab',
  '\b': 'backspace',
  '\x1b': 'esc',
  ' ': 'space',
};
// The characters typed with Shift held, other than the capital letters.
const kShiftedSymbols = '~!@#$%^&*()_+{}|:"<>?';

// How long a command may take to be answered, and how often a wait looks at
// the screen.
const kReplyTimeoutMs = 5000;
const kWaitPollMs = 250;
const kDefaultWaitSeconds = 30;
// Reading the panel back and sending it takes about two seconds.
const kScreenshotTimeoutMs = 30000;

// Undo the escapes text to type is given with.
function unescape(text) {
  return text.replace(/\\(.)/g, (match, code) => {
    switch (code) {
      case 'r':
        return '\r';
      case 'n':
        return '\n';
      case 't':
        return '\t';
      case 'b':
        return '\b';
      case 'e':
        return '\x1b';
      default:
        return code;
    }
  });
}

// An image as a PNG: 8-bit RGB, from rows of RGB triples.
function encodePNG(width, height, rgb) {
  const chunk = (type, data) => {
    const length = Buffer.alloc(4);
    length.writeUInt32BE(data.length);
    const body = Buffer.concat([Buffer.from(type, 'latin1'), data]);
    const crc = Buffer.alloc(4);
    crc.writeUInt32BE(zlib.crc32(body));
    return Buffer.concat([length, body, crc]);
  };
  const header = Buffer.alloc(13);
  header.writeUInt32BE(width, 0);
  header.writeUInt32BE(height, 4);
  // 8 bits a channel, truecolor, and the standard compression, filter and
  // (no) interlace methods.
  header.set([8, 2, 0, 0, 0], 8);
  // Each row is led by its filter type, 0 for none.
  const stride = width * 3;
  const raw = Buffer.alloc((stride + 1) * height);
  for (let y = 0; y < height; ++y) {
    rgb.copy(raw, y * (stride + 1) + 1, y * stride, (y + 1) * stride);
  }
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunk('IHDR', header),
    chunk('IDAT', zlib.deflateSync(raw)),
    chunk('IEND', Buffer.alloc(0)),
  ]);
}

// A connection to the board: sends commands and collects their replies.
class Board {
  constructor(port, quiet) {
    this.nextId = 1;
    this.waiting = new Map();
    this.connection = openPort(port, (line) => this.onLine(line, quiet));
  }

  onLine(line, quiet) {
    const record = parseLine(line);
    if (record !== null && record.kind === 'reply') {
      const request = this.waiting.get(record.id);
      if (request !== undefined) {
        request.lines.push(record);
        if (record.status !== undefined) {
          this.waiting.delete(record.id);
          clearTimeout(request.timer);
          request.resolve(request.lines);
        }
        return;
      }
    }
    if (!quiet) {
      process.stderr.write(line + '\n');
    }
  }

  // Send a command and resolve with its reply's lines, the last of which has
  // its status. Rejects if the status is not ok, or if no reply comes.
  command(name, args = {}, timeoutMs = kReplyTimeoutMs) {
    const id = String(this.nextId++);
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.waiting.delete(id);
        reject(new Error(`no reply to ${name}`));
      }, timeoutMs);
      this.waiting.set(id, {
        lines: [],
        timer,
        resolve: (lines) => {
          const last = lines[lines.length - 1];
          if (last.status !== 'ok') {
            reject(new Error(`${name}: ${last.error ?? last.status}`));
          } else {
            resolve(lines);
          }
        },
      });
      this.connection.write(formatLine({id, cmd: name, ...args})).catch((e) => {
        clearTimeout(timer);
        this.waiting.delete(id);
        reject(e);
      });
    });
  }

  // The screen, as the guest shows it: a row a line, in Unicode.
  async screen(withAttributes = false) {
    const lines = await this.command(
      'screen',
      withAttributes ? {attrs: 'true'} : {}
    );
    const header = lines[0];
    const rows = [];
    const attributes = [];
    for (const line of lines) {
      if (line.chars !== undefined) {
        rows[toNumber(line.row)] = Buffer.from(line.chars, 'hex');
      } else if (line.attrs !== undefined) {
        attributes[toNumber(line.row)] = line.attrs;
      }
    }
    return {
      header,
      text: rows.map((bytes) => [...bytes].map((b) => kCP437Characters[b]).join('')),
      attributes,
    };
  }

  // The panel, as a PNG.
  async screenshot() {
    const lines = await this.command('screenshot', {}, kScreenshotTimeoutMs);
    const width = toNumber(lines[0].width);
    const height = toNumber(lines[0].height);
    const rgb = Buffer.alloc(width * height * 3);
    let received = 0;
    for (const line of lines) {
      if (line.pixels === undefined) {
        continue;
      }
      const pixels = Buffer.from(line.pixels, 'hex');
      let offset = (toNumber(line.row) * width + toNumber(line.x)) * 3;
      for (let i = 0; i + 1 < pixels.length; i += 2) {
        // RGB565, each channel widened to 8 bits by repeating its top bits.
        const pixel = pixels.readUInt16BE(i);
        const r = pixel >> 11;
        const g = (pixel >> 5) & 0x3f;
        const b = pixel & 0x1f;
        rgb[offset++] = (r << 3) | (r >> 2);
        rgb[offset++] = (g << 2) | (g >> 4);
        rgb[offset++] = (b << 3) | (b >> 2);
        ++received;
      }
    }
    if (received !== width * height) {
      throw new Error(
        `screenshot: ${received} of ${width * height} pixels arrived`
      );
    }
    return encodePNG(width, height, rgb);
  }

  // Press, release or tap a key by name. A key the board has no room for
  // is refused and not queued, so it is sent again until there is room.
  async key(name, state) {
    const args = state === undefined ? {name} : {name, state};
    for (;;) {
      try {
        await this.command('key', args);
        return;
      } catch (e) {
        if (!e.message.endsWith('queue_full')) {
          throw e;
        }
        await delay(kKeyRetryMs);
      }
    }
  }

  // Type text, as the keys that type it.
  async type(text) {
    for (const character of text) {
      const name = kTextKeyNames[character] ?? character;
      const shifted =
        (character >= 'A' && character <= 'Z') ||
        kShiftedSymbols.includes(character);
      if (name === character && (character < ' ' || character > '~')) {
        throw new Error(`cannot type ${formatValue(character)}`);
      }
      if (shifted) {
        await this.key('shift', 'press');
      }
      await this.key(name);
      if (shifted) {
        await this.key('shift', 'release');
      }
    }
  }

  close() {
    this.connection.close();
  }
}

function printScreen({header, text, attributes}) {
  const cursor =
    header.cursor_row !== undefined
      ? `, cursor at row ${header.cursor_row} column ${header.cursor_column}`
      : '';
  console.log(
    `mode ${header.mode}, ${header.columns}x${header.rows}${cursor}`
  );
  const width = text.length > 0 ? text[0].length : 0;
  console.log('+' + '-'.repeat(width) + '+');
  text.forEach((row, i) => {
    console.log('|' + row + '|' + (attributes[i] ? ' ' + attributes[i] : ''));
  });
  console.log('+' + '-'.repeat(width) + '+');
}

// Carry out one step: a command and what follows it, as given on the command
// line or a line of a script. Text to type and a pattern to wait for are the
// whole of what follows, spaces and all.
async function runStep(board, verb, argument, options) {
  const rest = argument.split(/\s+/).filter((word) => word.length > 0);
  switch (verb) {
    case 'screen':
      printScreen(await board.screen(options.attrs));
      return true;
    case 'screenshot': {
      if (rest.length !== 1) {
        throw new Error('screenshot needs a file to write the PNG to');
      }
      await fs.writeFile(rest[0], await board.screenshot());
      return true;
    }
    case 'type':
      await board.type(unescape(argument));
      return true;
    case 'key':
      await board.key(rest[0], rest[1] ?? options.state);
      return true;
    case 'wait': {
      const pattern = new RegExp(argument, 'm');
      const seconds = Number(options.timeout ?? kDefaultWaitSeconds);
      const end = Date.now() + seconds * 1000;
      while (Date.now() < end) {
        // A graphics mode has no text to match, which is no match yet: the
        // keys that leave it may still be on their way.
        let text = null;
        try {
          ({text} = await board.screen());
        } catch (e) {
          if (!e.message.endsWith('not_text_mode')) {
            throw e;
          }
        }
        if (text !== null && pattern.test(text.join('\n'))) {
          return true;
        }
        await delay(kWaitPollMs);
      }
      console.error(`timed out waiting for ${formatValue(argument)}`);
      return false;
    }
    case 'report':
      for (const line of await board.command('report')) {
        if (line.group !== undefined) {
          const {kind, id, cmd, group, ...fields} = line;
          console.log(`[${group}] ${formatLine(fields)}`);
        }
      }
      return true;
    case 'ping': {
      const lines = await board.command('ping');
      console.log(formatLine(lines[lines.length - 1]));
      return true;
    }
    case 'sleep':
      await delay(Number(rest[0]) * 1000);
      return true;
    default:
      throw new Error(`unknown command ${verb}`);
  }
}

async function main() {
  const {values, positionals} = parseArgs({
    options: {
      attrs: {type: 'boolean', default: false},
      state: {type: 'string'},
      timeout: {type: 'string'},
      // Leave out the logs the board sends unasked.
      quiet: {type: 'boolean', default: false},
    },
    allowPositionals: true,
  });
  if (positionals.length === 0) {
    console.error(
      'Usage: debug.js [--quiet] screen [--attrs] | screenshot FILE.png | ' +
        'type TEXT | key NAME [--state press|release] | ' +
        'wait PATTERN [--timeout SECONDS] | report | ping | run SCRIPT'
    );
    return 2;
  }
  const port = await waitForPort();
  if (port === null) {
    throw new Error('no /dev/ttyACM* appeared');
  }
  const board = new Board(port, values.quiet);
  try {
    if (positionals[0] !== 'run') {
      const [verb, ...rest] = positionals;
      return (await runStep(board, verb, rest.join(' '), values)) ? 0 : 1;
    }
    const script = await fs.readFile(positionals[1], 'utf8');
    for (const raw of script.split('\n')) {
      const line = raw.trim();
      if (line === '' || line.startsWith('#')) {
        continue;
      }
      console.log(`> ${line}`);
      const space = line.indexOf(' ');
      const verb = space < 0 ? line : line.slice(0, space);
      const argument = space < 0 ? '' : line.slice(space + 1);
      if (!(await runStep(board, verb, argument, {}))) {
        return 1;
      }
    }
    return 0;
  } finally {
    board.close();
  }
}

if (require.main === module) {
  (async () => {
    try {
      process.exit(await main());
    } catch (e) {
      console.error(e.message);
      process.exit(2);
    }
  })();
}
