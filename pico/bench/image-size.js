#!/usr/bin/env node

const fs = require('fs');

// Reports how much space a firmware image needs, for build.sh's summary table.
//
//   ./image-size.js IMAGE.elf [OBJECT.o]
//
// Prints three space separated numbers: bytes of flash, bytes of SRAM, and the
// text size of the object if one was named, or 0 if not.
//
// This reads the ELF itself rather than parsing the output of readelf and size.
// Those two are perfectly good tools, but recovering numbers from their output
// needs hex parsing, and awk's strtonum() is a GNU extension - the same script
// silently produces nothing under mawk, which is the default awk on Debian and
// Ubuntu, and under the BSD awk on macOS. Reading the headers is both shorter
// and portable, and it drops two toolchain dependencies from the build script.

// ELF32 header field offsets. See elf(5).
const kPhoff = 28;
const kPhentsize = 42;
const kPhnum = 44;
const kShoff = 32;
const kShentsize = 46;
const kShnum = 48;
const kShstrndx = 50;

// Program header field offsets, and the one type that occupies memory.
const kPhdrType = 0;
const kPhdrVaddr = 8;
const kPhdrPaddr = 12;
const kPhdrFilesz = 16;
const kPhdrMemsz = 20;
const kPtLoad = 1;

// Section header field offsets and flags.
const kShdrName = 0;
const kShdrType = 4;
const kShdrFlags = 8;
const kShdrAddr = 12;
const kShdrOffset = 16;
const kShdrSize = 20;
const kShtNobits = 8;
const kShfWrite = 0x1;
const kShfAlloc = 0x2;
const kShfExecinstr = 0x4;

// SRAM on the RP2040 (264KB, to 0x20042000) and RP2350 (520KB, to 0x20082000).
// Everything below it in a physical address is in flash.
const kSRAMBase = 0x20000000;
const kSRAMEnd = 0x20082000;

function readElf(filePath) {
  const elf = fs.readFileSync(filePath);
  // Only the one shape is ever produced here, and misreading another as this
  // one would report plausible nonsense rather than fail.
  const isElf32LE =
    elf.length > 52 &&
    elf.readUInt32BE(0) === 0x7f454c46 &&
    elf[4] === 1 &&
    elf[5] === 1;
  if (!isElf32LE) {
    throw new Error(`${filePath} is not a little-endian 32-bit ELF file`);
  }
  return elf;
}

// The SDK's linker scripts emit NOLOAD .stack_dummy / .stack1_dummy sections
// in the scratch banks to reserve each core's stack, and ld gives each its own
// PT_LOAD segment inside [kSRAMBase, kSRAMEnd). Subtract them so SRAM matches
// the linker's RAM figure and excludes the stacks on both chips.
function stackDummySize(elf) {
  const shoff = elf.readUInt32LE(kShoff);
  const shentsize = elf.readUInt16LE(kShentsize);
  const shnum = elf.readUInt16LE(kShnum);
  const shstrndx = elf.readUInt16LE(kShstrndx);
  if (shnum === 0 || shstrndx >= shnum) {
    return 0;
  }
  const shstrHdr = shoff + shstrndx * shentsize;
  const shstrOff = elf.readUInt32LE(shstrHdr + kShdrOffset);
  const shstrEnd = shstrOff + elf.readUInt32LE(shstrHdr + kShdrSize);
  let total = 0;
  for (let i = 0; i < shnum; ++i) {
    const shdr = shoff + i * shentsize;
    if (elf.readUInt32LE(shdr + kShdrType) !== kShtNobits) {
      continue;
    }
    const addr = elf.readUInt32LE(shdr + kShdrAddr);
    if (addr < kSRAMBase || addr >= kSRAMEnd) {
      continue;
    }
    const start = shstrOff + elf.readUInt32LE(shdr + kShdrName);
    let end = start;
    while (end < shstrEnd && elf[end] !== 0) {
      ++end;
    }
    const name = elf.subarray(start, end).toString('ascii');
    if (/^\.stack\d*_dummy$/.test(name)) {
      total += elf.readUInt32LE(shdr + kShdrSize);
    }
  }
  return total;
}

// Flash is what is loaded from it, which includes the initializers for
// everything in .data; SRAM is what is reserved in it, which includes .bss and
// the heap but not the stack, which lives in a scratch bank of its own.
function loadedSize(elf) {
  const phoff = elf.readUInt32LE(kPhoff);
  const phentsize = elf.readUInt16LE(kPhentsize);
  const phnum = elf.readUInt16LE(kPhnum);
  let flash = 0;
  let sram = 0;
  for (let i = 0; i < phnum; ++i) {
    const phdr = phoff + i * phentsize;
    if (elf.readUInt32LE(phdr + kPhdrType) !== kPtLoad) {
      continue;
    }
    const vaddr = elf.readUInt32LE(phdr + kPhdrVaddr);
    const paddr = elf.readUInt32LE(phdr + kPhdrPaddr);
    if (paddr < kSRAMBase) {
      flash += elf.readUInt32LE(phdr + kPhdrFilesz);
    }
    if (vaddr >= kSRAMBase && vaddr < kSRAMEnd) {
      sram += elf.readUInt32LE(phdr + kPhdrMemsz);
    }
  }
  sram -= stackDummySize(elf);
  return {flash, sram};
}

// The same figure `size` prints in its text column: allocated sections that
// occupy space in the file and are either executable or read-only. For the
// core's object that is its code plus its constant data, which is the number
// to line up against a desktop measurement of the same object.
function textSize(elf) {
  const shoff = elf.readUInt32LE(kShoff);
  const shentsize = elf.readUInt16LE(kShentsize);
  const shnum = elf.readUInt16LE(kShnum);
  let text = 0;
  for (let i = 0; i < shnum; ++i) {
    const shdr = shoff + i * shentsize;
    const flags = elf.readUInt32LE(shdr + kShdrFlags);
    if (!(flags & kShfAlloc)) {
      continue;
    }
    if (elf.readUInt32LE(shdr + kShdrType) === kShtNobits) {
      continue;
    }
    if (flags & kShfExecinstr || !(flags & kShfWrite)) {
      text += elf.readUInt32LE(shdr + kShdrSize);
    }
  }
  return text;
}

if (require.main === module) {
  const args = process.argv.slice(2);
  if (args.length < 1 || args.length > 2) {
    console.error('Usage: image-size.js IMAGE.elf [OBJECT.o]');
    process.exit(2);
  }
  const [imageFilePath, objectFilePath] = args;
  try {
    const {flash, sram} = loadedSize(readElf(imageFilePath));
    const text = objectFilePath ? textSize(readElf(objectFilePath)) : 0;
    console.log(`${flash} ${sram} ${text}`);
  } catch (e) {
    console.error(e.message);
    process.exit(1);
  }
}
