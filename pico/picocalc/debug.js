#!/usr/bin/env node

// Drives the PicoCalc runtime over USB serial: reads its screen, types into
// the guest and asks for its report, through the commands in src/command.h.
//
//   ./debug.js screen [--attrs]
//   ./debug.js type 'DIR C:\r'
//   ./debug.js key f6 [--state press|release]
//   ./debug.js scancode 0x52
//   ./debug.js wait 'A>' [--timeout SECONDS]
//   ./debug.js report
//   ./debug.js ping
//   ./debug.js run SCRIPT
//
// Text to type takes \r, \n, \t, \e, \\ and \xNN escapes, since a shell makes
// it awkward to pass the characters themselves. A pattern to wait for is a
// JavaScript regular expression, matched against the screen's rows joined by
// newlines, each row padded with spaces to the screen's width, and with ^ and
// $ matching at the start and end of each row.
//
// A script is the same commands a line each - "type DIR\r", "wait A>",
// "sleep 1.5" - with # for a comment, run in order in one session. A wait
// that times out stops the script and exits non-zero, which is what makes a
// script a test. A wait matches whatever is on the screen, output from before
// the script included, so a script clears the screen first - "type CLS\r" -
// if what it waits for could already be there.
//
// The port is opened once for the whole session, for the reason in
// ../bench/serial.js. Lines the board sends unasked - its logs - are shown as
// they come.

const fs = require('fs/promises');
const {parseArgs} = require('util');
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

// How long a command may take to be answered, and how often a wait looks at
// the screen.
const kReplyTimeoutMs = 5000;
const kWaitPollMs = 250;
const kDefaultWaitSeconds = 30;

// Undo the escapes text to type is given with.
function unescape(text) {
  return text.replace(/\\(x[0-9A-Fa-f]{2}|.)/g, (match, code) => {
    switch (code[0]) {
      case 'r':
        return '\r';
      case 'n':
        return '\n';
      case 't':
        return '\t';
      case 'e':
        return '\x1b';
      case 'x':
        return String.fromCharCode(parseInt(code.slice(1), 16));
      default:
        return code;
    }
  });
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

  // Wait for the keys a type or key command queued to have been sent.
  async drainKeys() {
    for (;;) {
      const lines = await this.command('ping');
      if (toNumber(lines[lines.length - 1].keys_queued) === 0) {
        return;
      }
      await delay(kWaitPollMs);
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
    case 'type':
      await board.command('type', {text: unescape(argument)});
      await board.drainKeys();
      return true;
    case 'key': {
      const args = {name: rest[0]};
      const state = rest[1] ?? options.state;
      if (state !== undefined) {
        args.state = state;
      }
      await board.command('key', args);
      await board.drainKeys();
      return true;
    }
    case 'scancode':
      await board.command('key', {scancode: rest[0]});
      return true;
    case 'wait': {
      const pattern = new RegExp(argument, 'm');
      const seconds = Number(options.timeout ?? kDefaultWaitSeconds);
      const end = Date.now() + seconds * 1000;
      while (Date.now() < end) {
        const {text} = await board.screen();
        if (pattern.test(text.join('\n'))) {
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
      'Usage: debug.js [--quiet] screen [--attrs] | type TEXT | ' +
        'key NAME [--state press|release] | scancode CODE | ' +
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
