#!/usr/bin/env node

const fs = require('fs/promises');
const {parseArgs} = require('util');
const {spawnSync} = require('child_process');
const {parseLine, toNumber, formatLine} = require('./logfmt');
const {delay, waitForPort, openPort} = require('./serial');

// Flashes an image onto an attached Pico and captures what it prints.
//
//   ./run.js ../../build-pico/O3/yax86_pico_bench.uf2
//
// What the firmware prints is logfmt, a line per record - see "util —
// logging" in AGENTS.md. Reports are shown as tables, and the bench's result
// with the figures a PR reports worked out from it; --json prints each record
// as a JSON object instead. --out saves the lines as they came.
//
// The run is over at the line with event=done. The PicoCalc's report is the
// answer to a command, so at that point it is asked for one, which ends the
// capture once it is in; the bench says everything unasked, and is done.
//
// The board is rebooted into BOOTSEL with picotool rather than by hand, so a
// run needs no button press:
//
//   picotool reboot -u -f && sleep 3 && picotool load -x <image>.uf2

// How long to give the board to re-enumerate, in milliseconds. It comes back
// as a USB mass storage device after the reboot and as a CDC port after the
// load, and picotool fails outright if it is asked too early.
const kEnumerateDelayMs = 3000;
// A real 8088's clock, which "vs a real 8088" is against.
const k8088Hz = 4.77e6;
// The pairs a log or a reply line starts with, which a table leaves out.
const kPrefixKeys = new Set([
  'kind',
  'level',
  'module',
  'event',
  'tick',
  'id',
  'cmd',
  'group',
]);
// The id run.js asks for the PicoCalc's report with.
const kReportCommandId = '1';

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

// The figures a performance PR reports, from a bench result's integers:
// seconds, emulated MHz, MIPS and the speed against a real 8088.
function benchFigures(record) {
  const us = toNumber(record.elapsed_us);
  const cycles = toNumber(record.cycles);
  const instructions = toNumber(record.instructions);
  if (!(us > 0)) {
    return [];
  }
  const seconds = us / 1e6;
  return [
    ['seconds', seconds.toFixed(6)],
    ['emulated_mhz', (cycles / us).toFixed(3)],
    ['mips', (instructions / us).toFixed(3)],
    ['vs_8088', `${((100 * cycles) / k8088Hz / seconds).toFixed(1)}%`],
  ];
}

// How a record is shown: a group of a report, or the bench's header or
// result, as a table of its fields, and anything else as one line.
function render(record) {
  const fields = Object.entries(record).filter(
    ([key]) => !kPrefixKeys.has(key)
  );
  let title = null;
  if (record.kind === 'reply' && record.group !== undefined) {
    title = `[${record.cmd} ${record.group}]`;
  } else if (record.event === 'header' || record.event === 'result') {
    title = `[${record.module} ${record.event}]`;
  }
  if (title === null) {
    const head =
      record.kind === 'reply'
        ? `reply ${record.id} ${record.cmd}`
        : [record.level, record.module, record.event]
            .filter((part) => part !== undefined)
            .join(' ');
    const rest =
      fields.length > 0 ? '  ' + formatLine(Object.fromEntries(fields)) : '';
    return `${head}${rest}\n`;
  }
  if (record.module === 'bench' && record.event === 'result') {
    fields.push(...benchFigures(record));
  }
  const width = Math.max(...fields.map(([key]) => key.length));
  return (
    `${title}\n` +
    fields.map(([key, value]) => `  ${key.padEnd(width)}  ${value}\n`).join('')
  );
}

// Read from the port until the run is over, or time runs out. Returns the
// captured lines, which are shown as they arrive so that a long run can be
// watched: rendered, or as JSON if json is set. A line that is not logfmt is
// shown as it is.
function capture(port, budgetMs, json) {
  return new Promise((resolve) => {
    const lines = [];
    let done = false;
    let finished = false;
    let timer = null;
    let connection = null;
    const finish = (timedOut) => {
      if (finished) {
        return;
      }
      finished = true;
      clearTimeout(timer);
      connection.close();
      if (timedOut) {
        process.stdout.write(
          `\n[timed out after ${(budgetMs / 1000).toFixed(0)}s; ` +
            `${lines.length} lines]\n`
        );
      }
      resolve({lines, done});
    };
    connection = openPort(
      port,
      (line) => {
        lines.push(line);
        const record = parseLine(line);
        if (json) {
          if (record !== null) {
            process.stdout.write(JSON.stringify(record) + '\n');
          }
        } else {
          process.stdout.write(record !== null ? render(record) : line + '\n');
        }
        if (record === null) {
          return;
        }
        if (record.event === 'done') {
          if (record.module === 'bench') {
            done = true;
            finish(false);
          } else {
            connection
              .write(`id=${kReportCommandId} cmd=report`)
              .catch(() => finish(false));
          }
        } else if (
          record.kind === 'reply' &&
          record.id === kReportCommandId &&
          record.status !== undefined
        ) {
          done = record.status === 'ok';
          finish(false);
        }
      },
      () => finish(false)
    );
    timer = setTimeout(() => finish(true), budgetMs);
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
      // Print each record as a JSON object rather than rendering it.
      json: {type: 'boolean', default: false},
    },
    allowPositionals: true,
  });
  if (positionals.length !== 1) {
    console.error(
      'Usage: run.js [--timeout SECONDS] [--out FILE] [--no-flash] [--json] ' +
        'IMAGE.uf2'
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
  const {lines, done} = await capture(
    port,
    Number(values.timeout) * 1000,
    values.json
  );
  if (values.out) {
    await fs.writeFile(values.out, lines.join('\n') + '\n', 'utf8');
  }
  return done ? 0 : 1;
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
