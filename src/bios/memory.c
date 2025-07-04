#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Look up the memory region corresponding to an address. Returns NULL if the
// address is not mapped to a known memory region.
MemoryRegion* GetMemoryRegion(struct BIOSState* bios, uint32_t address) {
  for (uint8_t i = 0; i < MemoryRegionsLength(&bios->memory_regions); ++i) {
    MemoryRegion* region = MemoryRegionsGet(&bios->memory_regions, i);
    if (address >= region->start && address < region->start + region->size) {
      return region;
    }
  }
  return NULL;
}

// Read a byte from a logical memory address.
uint8_t ReadLogicalMemoryByte(struct BIOSState* bios, uint32_t address) {
  MemoryRegion* region = GetMemoryRegion(bios, address);
  if (!region || !region->read_memory_byte) {
    return 0xFF;
  }
  return region->read_memory_byte(bios, address - region->start);
}

// Read a word from a logical memory address.
uint16_t ReadLogicalMemoryWord(struct BIOSState* bios, uint32_t address) {
  uint8_t low_byte = ReadLogicalMemoryByte(bios, address);
  uint8_t high_byte = ReadLogicalMemoryByte(bios, address + 1);
  return (high_byte << 8) | low_byte;
}

// Write a byte to a logical memory address.
void WriteLogicalMemoryByte(
    struct BIOSState* bios, uint32_t address, uint8_t value) {
  MemoryRegion* region = GetMemoryRegion(bios, address);
  if (!region || !region->write_memory_byte) {
    return;
  }
  region->write_memory_byte(bios, address - region->start, value);
}

// Write a word to a logical memory address.
void WriteLogicalMemoryWord(
    struct BIOSState* bios, uint32_t address, uint16_t value) {
  WriteLogicalMemoryByte(bios, address, value & 0xFF);
  WriteLogicalMemoryByte(bios, address + 1, (value >> 8) & 0xFF);
}
