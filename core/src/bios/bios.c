#ifndef Y_IMPLEMENTATION
#include "../util/common.h"
#include "bios_rom_data.h"
#include "public.h"
#endif  // Y_IMPLEMENTATION

Y_PUBLIC uint32_t YBIOSGetROMSize(void) { return Y_BIOS_ROM_DATA_SIZE; }

Y_PUBLIC const uint8_t* YBIOSGetROMData(void) { return YBIOSROMData; }
