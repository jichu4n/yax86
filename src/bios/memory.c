#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Look up the memory region corresponding to an address. Returns NULL if the
// address is not mapped to a known memory region.
MemoryRegion* GetMemoryRegionForAddress(
    struct BIOSState* bios, uint32_t address) {
  // TODO: Use a more efficient data structure for lookups, such as a sorted
  // array with binary search.
  for (uint8_t i = 0; i < MemoryRegionsLength(&bios->memory_regions); ++i) {
    MemoryRegion* region = MemoryRegionsGet(&bios->memory_regions, i);
    if (address >= region->start && address < region->start + region->size) {
      return region;
    }
  }
  return NULL;
}

// Look up a memory region by type. Returns NULL if no region found with the
// specified type.
MemoryRegion* GetMemoryRegionByType(
    struct BIOSState* bios, uint8_t region_type) {
  for (uint8_t i = 0; i < MemoryRegionsLength(&bios->memory_regions); ++i) {
    MemoryRegion* region = MemoryRegionsGet(&bios->memory_regions, i);
    if (region->region_type == region_type) {
      return region;
    }
  }
  return NULL;
}

// Read a byte from a logical memory address.
uint8_t ReadMemoryByte(struct BIOSState* bios, uint32_t address) {
  MemoryRegion* region = GetMemoryRegionForAddress(bios, address);
  if (!region || !region->read_memory_byte) {
    return 0xFF;
  }
  return region->read_memory_byte(bios, address - region->start);
}

// Read a word from a logical memory address.
uint16_t ReadMemoryWord(struct BIOSState* bios, uint32_t address) {
  uint8_t low_byte = ReadMemoryByte(bios, address);
  uint8_t high_byte = ReadMemoryByte(bios, address + 1);
  return (high_byte << 8) | low_byte;
}

// Write a byte to a logical memory address.
void WriteMemoryByte(struct BIOSState* bios, uint32_t address, uint8_t value) {
  MemoryRegion* region = GetMemoryRegionForAddress(bios, address);
  if (!region || !region->write_memory_byte) {
    return;
  }
  region->write_memory_byte(bios, address - region->start, value);
}

// Write a word to a logical memory address.
void WriteMemoryWord(struct BIOSState* bios, uint32_t address, uint16_t value) {
  WriteMemoryByte(bios, address, value & 0xFF);
  WriteMemoryByte(bios, address + 1, (value >> 8) & 0xFF);
}
