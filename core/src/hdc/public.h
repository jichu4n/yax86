// Public interface for the Hard Disk Controller (HDC) module.
#ifndef YAX86_HDC_PUBLIC_H
#define YAX86_HDC_PUBLIC_H

// This module emulates an XT-IDE rev 2 hard disk controller - an 8-bit ATA
// task file operated in PIO mode, with no DMA and no interrupt line.
//
// The controller card also carries an option ROM, which is where the guest's
// INT 13h support for drives 80h and up actually comes from: GLaBIOS itself
// implements INT 13h for floppies only and rejects drive numbers above 3. Its
// POST scans C800:0000 and up for option ROMs and calls into whatever it
// finds, so the ROM this module maps is what makes a hard disk visible to DOS.
//
// The ROM is the XTIDE Universal BIOS, compiled into the library the same way
// the system BIOS is. Reading it from a file at run time would not work on
// targets like the Raspberry Pi Pico, which have no file system.

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_HDC_BUNDLE_H
#include "../util/log.h"
#endif  // YAX86_HDC_BUNDLE_H

enum {
  // Log module ID for the HDC.
  kLogModuleIDHDC = 9,
};

// Log module for the HDC.
static const LogModule kLogModuleHDC = {
    .id = kLogModuleIDHDC,
    .name = "HDC",
};

// Memory region types.
enum {
  // Option ROM memory map entry type.
  kMemoryMapEntryHDCOptionROM = 0x0C,

  // Start address of the option ROM. GLaBIOS scans for option ROMs starting
  // here, on 2KB boundaries.
  kHDCOptionROMStartAddress = 0xC8000,
};

// Value returned for reads that no device answers.
enum {
  kHDCOpenBusValue = 0xFF,
};

// Caller-provided runtime configuration for the HDC.
typedef struct HDCConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger, or NULL to disable logging.
  Logger* logger;
} HDCConfig;

// State of the HDC.
typedef struct HDCState {
  // Pointer to caller-provided runtime configuration.
  HDCConfig* config;
} HDCState;

// Initializes the HDC to its power-on state.
void HDCInit(HDCState* hdc, HDCConfig* config);

// Returns the size of the option ROM in bytes.
uint32_t HDCGetOptionROMSize(void);

// Reads a byte from the option ROM, where offset is relative to
// kHDCOptionROMStartAddress.
uint8_t HDCReadOptionROMByte(uint32_t offset);

#endif  // YAX86_HDC_PUBLIC_H
