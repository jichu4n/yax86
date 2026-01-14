#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "bios_rom_data.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

uint32_t BIOSGetROMSize(void) {
  return kBIOSROMDataSize;
}

uint8_t BIOSReadROMByte(uint32_t offset) {
  if (offset >= kBIOSROMDataSize) {
    return 0xFF;
  }
  return kBIOSROMData[offset];
}
