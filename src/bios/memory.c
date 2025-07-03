#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Add a region to the memory map. Returns true on success, false if we've
// exceeded the maximum number of memory regions.
bool AddMemoryRegion(struct BIOSState* bios, const MemoryRegion* metadata) {
  if (bios->num_memory_regions >= kMaxMemoryRegions) {
    return false;
  }
  bios->memory_regions[bios->num_memory_regions++] = *metadata;
  return true;
}

// Look up the memory region corresponding to an address. Returns NULL if the
// address is not mapped to a known memory region.
MemoryRegion* GetMemoryRegion(struct BIOSState* bios, uint16_t address) {
  for (uint8_t i = 0; i < bios->num_memory_regions; ++i) {
    MemoryRegion* region = &bios->memory_regions[i];
    if (address >= region->start && address < region->start + region->size) {
      return region;
    }
  }
  return NULL;
}

// Read a byte from a logical memory address.
uint8_t ReadMemoryByte(struct BIOSState* bios, uint16_t address) {
  MemoryRegion* region = GetMemoryRegion(bios, address);
  if (!region || !region->read_memory_byte) {
    return 0xFF;
  }
  return region->read_memory_byte(bios, address - region->start);
}

// Read a word from a logical memory address.
uint16_t ReadMemoryWord(struct BIOSState* bios, uint16_t address) {
  uint8_t low_byte = ReadMemoryByte(bios, address);
  uint8_t high_byte = ReadMemoryByte(bios, address + 1);
  return (high_byte << 8) | low_byte;
}

// Write a byte to a logical memory address.
void WriteMemoryByte(struct BIOSState* bios, uint16_t address, uint8_t value) {
  MemoryRegion* region = GetMemoryRegion(bios, address);
  if (!region || !region->write_memory_byte) {
    return;
  }
  region->write_memory_byte(bios, address - region->start, value);
}

// Write a word to a logical memory address.
void WriteMemoryWord(struct BIOSState* bios, uint16_t address, uint16_t value) {
  WriteMemoryByte(bios, address, value & 0xFF);
  WriteMemoryByte(bios, address + 1, (value >> 8) & 0xFF);
}
