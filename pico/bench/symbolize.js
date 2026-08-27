#!/usr/bin/env node

const fs = require('fs/promises');
const {spawnSync} = require('child_process');

// Turns a captured profile into a list of function names.
//
//   ./run.js <image>.uf2 --out capture.txt
//   ./symbolize.js ../../build-pico/O3/yax86_pico_bench.elf capture.txt
//
// The firmware prints buckets as addresses because carrying a symbol table on
// the board would cost more flash than the profiler is worth. Each bucket is
// attributed to whichever function contains its base address.
//
// Read the top few entries and check anything below them before acting on it.
// The tail is not reliable: a bucket lands on the function that starts before
// it, so where several small functions share a bucket only the first is named.

// A bucket line, as the firmware prints it: an address and a sample count.
const kBucketPattern = /^p ([0-9a-f]{8}) (\d+)$/;
// How many symbols to print. Beyond this the shares are too small to mean
// anything, for the reason above.
const kTopSymbols = 26;
// nm's type letters for code: text, and weak symbols that resolve to it.
const kCodeSymbolTypes = 'tw';

// Function symbols from the ELF, sorted by address.
function loadSymbols(elfFilePath) {
  const nm = spawnSync('arm-none-eabi-nm', ['-nS', elfFilePath], {
    encoding: 'utf8',
  });
  if (nm.status !== 0) {
    throw new Error(`arm-none-eabi-nm failed on ${elfFilePath}`);
  }
  const symbols = [];
  for (const line of nm.stdout.split('\n')) {
    const parts = line.split(/\s+/).filter((part) => part.length > 0);
    // nm prints a size only for the symbols that have one.
    let address, size, type, name;
    if (parts.length === 4) {
      [address, size, type, name] = parts;
    } else if (parts.length === 3) {
      [address, type, name] = parts;
      size = '0';
    } else {
      continue;
    }
    if (!kCodeSymbolTypes.includes(type.toLowerCase())) {
      continue;
    }
    symbols.push({
      address: parseInt(address, 16),
      size: parseInt(size, 16),
      name,
    });
  }
  symbols.sort((a, b) => a.address - b.address);
  return symbols;
}

// The last symbol starting at or before an address, by binary search.
function findSymbol(symbols, address) {
  let low = 0;
  let high = symbols.length;
  while (low < high) {
    const middle = (low + high) >> 1;
    if (symbols[middle].address <= address) {
      low = middle + 1;
    } else {
      high = middle;
    }
  }
  return low > 0 ? symbols[low - 1] : null;
}

function symbolNameAt(symbols, address) {
  const symbol = findSymbol(symbols, address);
  if (symbol === null) {
    return '?';
  }
  // Past the end of the symbol it follows, so it belongs to no function this
  // ELF names - padding, or a section the profiler covers but nm does not
  // describe.
  if (symbol.size > 0 && address >= symbol.address + symbol.size) {
    return `(gap after ${symbol.name})`;
  }
  return symbol.name;
}

async function symbolize(elfFilePath, captureFilePath) {
  const symbols = loadSymbols(elfFilePath);
  const samplesByName = new Map();
  let total = 0;
  const capture = await fs.readFile(captureFilePath, 'utf8');
  for (const line of capture.split('\n')) {
    const match = kBucketPattern.exec(line.trim());
    if (match === null) {
      continue;
    }
    const name = symbolNameAt(symbols, parseInt(match[1], 16));
    const count = Number(match[2]);
    samplesByName.set(name, (samplesByName.get(name) ?? 0) + count);
    total += count;
  }
  if (total === 0) {
    throw new Error(`no profile buckets in ${captureFilePath}`);
  }

  console.log(
    '  ' + 'symbol'.padEnd(46) + 'samples'.padStart(9) + 'share'.padStart(8)
  );
  const ranked = [...samplesByName.entries()].sort((a, b) => b[1] - a[1]);
  for (const [name, count] of ranked.slice(0, kTopSymbols)) {
    const share = ((100 * count) / total).toFixed(1) + '%';
    console.log(
      '  ' +
        name.slice(0, 46).padEnd(46) +
        String(count).padStart(9) +
        share.padStart(8)
    );
  }
  console.log(`\n  attributed samples: ${total}`);
}

if (require.main === module) {
  const args = process.argv.slice(2);
  if (args.length !== 2) {
    console.error('Usage: symbolize.js IMAGE.elf capture.txt');
    process.exit(2);
  }
  const [elfFilePath, captureFilePath] = args;

  (async () => {
    try {
      await symbolize(elfFilePath, captureFilePath);
    } catch (e) {
      console.error(e.message);
      process.exit(1);
    }
  })();
}
