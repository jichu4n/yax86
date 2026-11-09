// The board's USB serial port, for run.js and debug.js.
//
// The port must be opened exactly once. The firmware waits for DTR before it
// prints anything and every open asserts it, so configuring the line with a
// separate stty first starts the run, and the first lines are gone before a
// reader attaches. That is why the line is configured through a tty.ReadStream
// wrapped around the descriptor this already holds, rather than by shelling
// out to something that would open the device again.

const fs = require('fs/promises');
const fsSync = require('fs');
const path = require('path');
const tty = require('tty');

// How long to wait for a serial port to appear, and how long to let it settle
// once it has. Enumeration creates the node slightly before it is usable.
const kPortWaitMs = 25000;
const kPortSettleMs = 800;
const kPortPollMs = 200;
// How long to keep retrying a write the port has no room for yet, and how
// long to wait between tries.
const kWriteTimeoutMs = 2000;
const kWriteRetryMs = 5;

const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

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

// Open the port, in raw mode: no echo, no line discipline, and no translation
// in either direction. The baud rate is ignored by a CDC port.
//
// onLine is called with each line as it arrives, its CRLF made LF - the SDK's
// stdio ends every line with CRLF - and onClose once the port goes. Returns
// a function that writes a line to the board, resolving once it is all
// written, and one that closes the port.
function openPort(port, onLine, onClose = () => {}) {
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
  stream.setRawMode(true);
  // The part of a line that has arrived without its end yet.
  let pending = '';
  stream.on('data', (chunk) => {
    const lines = (pending + chunk.toString('latin1')).split('\n');
    pending = lines.pop();
    for (const line of lines) {
      onLine(line.endsWith('\r') ? line.slice(0, -1) : line);
    }
  });
  stream.on('error', () => onClose());
  stream.on('close', () => onClose());
  // The stream puts the descriptor in non-blocking mode, so a write the port
  // has no room for fails with EAGAIN rather than waiting, and is tried again.
  //
  // A line is sent with a newline before it as well as after, so that it
  // starts a line of its own on the board whatever came before it. The port
  // opens with echo on, as a tty does, until it is made raw: what the board
  // says while it is open that way - to this, or to anything else that opens
  // it, such as stty - is echoed back, and lands in the board's command line
  // without the end of the line. The next command would be read as the rest
  // of that line and lost. The board ignores an empty line, and answers the
  // echo with an error under id 0, which nothing waits for.
  const write = async (line) => {
    let data = Buffer.from('\n' + line + '\n', 'latin1');
    const end = Date.now() + kWriteTimeoutMs;
    while (data.length > 0) {
      try {
        data = data.subarray(fsSync.writeSync(fd, data));
      } catch (e) {
        if (e.code !== 'EAGAIN' || Date.now() > end) {
          throw e;
        }
        await delay(kWriteRetryMs);
      }
    }
  };
  return {write, close: () => stream.destroy()};
}

module.exports = {delay, waitForPort, openPort};
