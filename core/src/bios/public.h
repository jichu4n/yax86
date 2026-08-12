// Public interface for the BIOS module.
#ifndef YAX86_BIOS_PUBLIC_H
#define YAX86_BIOS_PUBLIC_H

#include <stdbool.h>
#include <stdint.h>

// Memory region types.
enum {
  // BIOS ROM memory map entry type - mapped to 0xF0000 to up to 0xFFFFF (64KB).
  kMemoryMapEntryBIOSROM = 0x01,
  // Start address of the BIOS ROM.
  kBIOSROMStartAddress = 0xFE000,
};

// Get size of BIOS ROM data.
uint32_t BIOSGetROMSize(void);

// Get a pointer to the BIOS ROM image, BIOSGetROMSize() bytes of it. The image
// is a constant array compiled into the library, so the platform maps it
// directly rather than reading it a byte at a time through a callback.
const uint8_t* BIOSGetROMData(void);

#endif  // YAX86_BIOS_PUBLIC_H
