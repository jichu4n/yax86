#!/usr/bin/env node

const fs = require('fs/promises');
const fsSync = require('fs');
const path = require('path');
const tty = require('tty');
const {parseArgs} = require('util');
const {spawnSync} = require('child_process');

// Flashes a benchmark image onto an attached Pico and captures what it prints.
//
//   ./run.js ../../build-pico/O3/yax86_pico_bench.uf2
//
// The board is rebooted into BOOTSEL with picotool rather than by hand, so a
// run needs no button press:
//
//   picotool reboot -u -f && sleep 3 && picotool load -x <image>.uf2
//
// The serial port must then be opened exactly once. The firmware waits for DTR
// before it prints anything and every open asserts it, so configuring the line
// with a separate stty first starts the run, and the header is gone before a
// reader attaches. That is why the line is configured through a tty.ReadStream
// wrapped around the descriptor this already holds, rather than by shelling
// out to something that would open the device again.

// How long to give the board to re-enumerate, in milliseconds. It comes back
// as a USB mass storage device after the reboot and as a CDC port after the
// load, and picotool fails outright if it is asked too early.
const kEnumerateDelayMs = 3000;
// How long to wait for a serial port to appear, and how long to let it settle
// once it has. Enumeration creates the node slightly before it is usable.
const kPortWaitMs = 25000;
const kPortSettleMs = 800;
const kPortPollMs = 200;
// What the firmware prints when it has nothing left to say.
const kDoneMarker = '\ndone\n';

const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

// Reboot the board into BOOTSEL and load an image onto it.
async function flash(uf2FilePath) {
  // A board already in BOOTSEL has no serial connection to reboot through, so
  // a failure here is not fatal - the load below is what has to work.
  spawnSync('picotool', ['reboot', '-u', '-f'], {stdio: 'ignore'});
  await delay(kEnumerateDelayMs);
  const load = spawnSync('picotool', ['load', '-x', uf2FilePath], {
    stdio: ['ignore', 'ignore', 'inherit'],
  });
  if (load.status !== 0) {
    throw new Error(`picotool load failed with status ${load.status}`);
  }
}

// Find the board's CDC port. Not hard-coded: the port re-enumerates after
// every flash and after a watchdog reset, and can come back on a different
// node than it left on.
async function waitForPort(timeoutMs = kPortWaitMs) {
  const end = Date.now() + timeoutMs;
  while (Date.now() < end) {
    const ports = (await fs.readdir('/dev'))
      .filter((name) => name.startsWith('ttyACM'))
      .sort();
    if (ports.length > 0) {
      await delay(kPortSettleMs);
      return path.join('/dev', ports[ports.length - 1]);
    }
    await delay(kPortPollMs);
  }
  return null;
}

// Read from the port until the firmware says it is done, or time runs out.
// Returns the captured text, which is echoed as it arrives so that a long run
// can be watched.
function capture(port, budgetMs) {
  const fd = fsSync.openSync(
    port,
    fsSync.constants.O_RDWR | fsSync.constants.O_NOCTTY
  );
  let stream;
  try {
    stream = new tty.ReadStream(fd);
  } catch (e) {
    fsSync.closeSync(fd);
    throw e;
  }
  // Raw mode on this descriptor: no echo, no line discipline, and no
  // translation in either direction. The baud rate is ignored by a CDC port.
  stream.setRawMode(true);

  return new Promise((resolve) => {
    let out = '';
    let timer = null;
    const finish = (timedOut) => {
      if (timer !== null) {
        clearTimeout(timer);
        timer = null;
      }
      stream.destroy();
      if (timedOut) {
        process.stdout.write(
          `\n[timed out after ${(budgetMs / 1000).toFixed(0)}s; ` +
            `${out.length} bytes]\n`
        );
      }
      resolve(out);
    };
    timer = setTimeout(() => finish(true), budgetMs);
    stream.on('data', (chunk) => {
      // The SDK's stdio ends every line with CRLF, so the newlines are
      // normalized here rather than left for every reader of the capture to
      // cope with.
      const text = chunk.toString('utf8').replaceAll('\r\n', '\n');
      out += text;
      process.stdout.write(text);
      if (out.includes(kDoneMarker)) {
        finish(false);
      }
    });
    stream.on('error', () => finish(false));
  });
}

async function main() {
  const {values, positionals} = parseArgs({
    options: {
      // Seconds to wait for the run to finish.
      timeout: {type: 'string', default: '180'},
      // Also write the captured output to this file.
      out: {type: 'string'},
      // Capture from a board that is already running the image.
      'no-flash': {type: 'boolean', default: false},
    },
    allowPositionals: true,
  });
  if (positionals.length !== 1) {
    console.error(
      'Usage: run.js [--timeout SECONDS] [--out FILE] [--no-flash] IMAGE.uf2'
    );
    process.exit(2);
  }
  const [uf2FilePath] = positionals;

  if (!values['no-flash']) {
    await flash(uf2FilePath);
  }
  const port = await waitForPort();
  if (port === null) {
    throw new Error('no /dev/ttyACM* appeared');
  }
  const out = await capture(port, Number(values.timeout) * 1000);
  if (values.out) {
    await fs.writeFile(values.out, out, 'utf8');
  }
  return out.includes(kDoneMarker) ? 0 : 1;
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
