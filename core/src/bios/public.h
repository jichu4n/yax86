// Public interface for the BIOS module.
#ifndef Y_BIOS_PUBLIC_H
#define Y_BIOS_PUBLIC_H

#include <stdbool.h>
#include <stdint.h>

#ifndef Y_BIOS_BUNDLE_H
#include "../util/common.h"
#endif  // Y_BIOS_BUNDLE_H

enum {
  // Start address of the BIOS ROM.
  Y_BIOS_ROM_START_ADDRESS = 0xFE000,
};

// Get size of BIOS ROM data.
Y_PUBLIC uint32_t YBIOSGetROMSize(void);

// Get a pointer to the BIOS ROM image, YBIOSGetROMSize() bytes of it. The image
// is a constant array compiled into the library, so the platform maps it
// directly rather than reading it a byte at a time through a callback.
Y_PUBLIC const uint8_t* YBIOSGetROMData(void);

#endif  // Y_BIOS_PUBLIC_H
