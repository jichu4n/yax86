#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "bios_rom_data.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

static uint8_t PlatformReadBIOSROMByte(
    YAX86_UNUSED MemoryMapEntry* entry, uint32_t address) {
  if (address >= kBIOSROMDataSize) {
    return 0xFF;
  }
  return kBIOSROMData[address];
}

// Register memory map.
bool BIOSSetup(PlatformState* platform) {
  MemoryMapEntry bios_rom = {
      .context = NULL,
      .entry_type = kMemoryMapEntryBIOSROM,
      .start = 0xF0000,
      .end = 0xF0000 + kBIOSROMDataSize - 1,
      .read_byte = PlatformReadBIOSROMByte,
      .write_byte = NULL,  // BIOS ROM is read-only.
  };
  return MemoryMapAppend(&platform->memory_map, &bios_rom);
}
