#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "bios_rom_data.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

YAX86_PUBLIC uint32_t BIOSGetROMSize(void) {
  return kBIOSROMDataSize;
}

YAX86_PUBLIC const uint8_t* BIOSGetROMData(void) {
  return kBIOSROMData;
}
