#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "bios_rom_data.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

YAX86_PUBLIC uint32_t YBIOSGetROMSize(void) { return Y_BIOS_ROM_DATA_SIZE; }

YAX86_PUBLIC const uint8_t* YBIOSGetROMData(void) { return YBIOSROMData; }
