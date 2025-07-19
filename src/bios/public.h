// Public interface for the BIOS module.
#ifndef YAX86_BIOS_PUBLIC_H
#define YAX86_BIOS_PUBLIC_H

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"

// Memory region types.
enum {
  // BIOS ROM memory map entry type - mapped to 0xF0000 to up to 0xFFFFF (64KB).
  kMemoryMapEntryBIOSROM = 0x01,
};

// Register memory map.
bool BIOSSetup(PlatformState* platform);

#endif  // YAX86_BIOS_PUBLIC_H
