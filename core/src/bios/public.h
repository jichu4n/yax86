// Public interface for the BIOS module.
#ifndef YAX86_BIOS_PUBLIC_H
#define YAX86_BIOS_PUBLIC_H

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_BIOS_BUNDLE_H
#include "../util/common.h"
#endif  // YAX86_BIOS_BUNDLE_H

enum {
  // Start address of the BIOS ROM.
  kBIOSROMStartAddress = 0xFE000,
};

// Get size of BIOS ROM data.
YAX86_PUBLIC uint32_t BIOSGetROMSize(void);

// Get a pointer to the BIOS ROM image, BIOSGetROMSize() bytes of it. The image
// is a constant array compiled into the library, so the platform maps it
// directly rather than reading it a byte at a time through a callback.
YAX86_PUBLIC const uint8_t* BIOSGetROMData(void);

#endif  // YAX86_BIOS_PUBLIC_H
