// ==============================================================================
// YAX86 BIOS MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_BIOS_BUNDLE_H
#define YAX86_BIOS_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/util/static_vector.h start
// ==============================================================================

#line 1 "./src/util/static_vector.h"
// Static vector library.
//
// A static vector is a vector backed by a fixed-size array. It's essentially
// a vector, but whose underlying storage is statically allocated and does not
// rely on dynamic memory allocation.

#ifndef YAX86_UTIL_STATIC_VECTOR_H
#define YAX86_UTIL_STATIC_VECTOR_H

#include <stddef.h>
#include <stdint.h>

// Header structure at the beginning of a static vector.
typedef struct StaticVectorHeader {
  // Element size in bytes.
  size_t element_size;
  // Maximum number of elements the vector can hold.
  size_t max_length;
  // Number of elements currently in the vector.
  size_t length;
} StaticVectorHeader;

// Define a static vector type with an element type.
#define STATIC_VECTOR_TYPE(name, element_type, max_length_value)          \
  typedef struct name {                                                   \
    StaticVectorHeader header;                                            \
    element_type elements[max_length_value];                              \
  } name;                                                                 \
  static void name##Init(name* vector) __attribute__((unused));           \
  static void name##Init(name* vector) {                                  \
    static const StaticVectorHeader header = {                            \
        .element_size = sizeof(element_type),                             \
        .max_length = (max_length_value),                                 \
        .length = 0,                                                      \
    };                                                                    \
    vector->header = header;                                              \
  }                                                                       \
  static size_t name##Length(const name* vector) __attribute__((unused)); \
  static size_t name##Length(const name* vector) {                        \
    return vector->header.length;                                         \
  }                                                                       \
  static element_type* name##Get(name* vector, size_t index)              \
      __attribute__((unused));                                            \
  static element_type* name##Get(name* vector, size_t index) {            \
    if (index >= (max_length_value)) {                                    \
      return NULL;                                                        \
    }                                                                     \
    return &(vector->elements[index]);                                    \
  }                                                                       \
  static bool name##Append(name* vector, const element_type* element)     \
      __attribute__((unused));                                            \
  static bool name##Append(name* vector, const element_type* element) {   \
    if (vector->header.length >= (max_length_value)) {                    \
      return false;                                                       \
    }                                                                     \
    vector->elements[vector->header.length++] = *element;                 \
    return true;                                                          \
  }                                                                       \
  static bool name##Insert(                                               \
      name* vector, size_t index, const element_type* element)            \
      __attribute__((unused));                                            \
  static bool name##Insert(                                               \
      name* vector, size_t index, const element_type* element) {          \
    if (index > vector->header.length ||                                  \
        vector->header.length >= (max_length_value)) {                    \
      return false;                                                       \
    }                                                                     \
    for (size_t i = vector->header.length; i > index; --i) {              \
      vector->elements[i] = vector->elements[i - 1];                      \
    }                                                                     \
    vector->elements[index] = *element;                                   \
    ++vector->header.length;                                              \
    return true;                                                          \
  }                                                                       \
  static bool name##Remove(name* vector, size_t index)                    \
      __attribute__((unused));                                            \
  static bool name##Remove(name* vector, size_t index) {                  \
    if (index >= vector->header.length) {                                 \
      return false;                                                       \
    }                                                                     \
    for (size_t i = index; i < vector->header.length - 1; ++i) {          \
      vector->elements[i] = vector->elements[i + 1];                      \
    }                                                                     \
    --vector->header.length;                                              \
    return true;                                                          \
  }

#endif  // YAX86_UTIL_STATIC_VECTOR_H


// ==============================================================================
// src/util/static_vector.h end
// ==============================================================================

// ==============================================================================
// src/bios/public.h start
// ==============================================================================

#line 1 "./src/bios/public.h"
// Public interface for the BIOS module.
#ifndef YAX86_BIOS_PUBLIC_H
#define YAX86_BIOS_PUBLIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef YAX86_IMPLEMENTATION
#include "../util/static_vector.h"
#endif  // YAX86_IMPLEMENTATION

struct BIOSState;

// ============================================================================
// Memory
// ============================================================================

enum {
  // First 640KB of memory, mapped to 0x00000 to 0x9FFFF (640KB).
  kMemoryRegionConventional = 0,
  // Text mode framebuffer, mapped to 0xB8000 to 0xB8F9F (80x25x2 bytes).
  kMemoryRegionTextModeFramebuffer = 1,

  // Maximum number of memory region entries.
  kMaxMemoryRegions = 8,
};

// A memory region in the BIOS memory map. Memory regions should not overlap.
typedef struct MemoryRegion {
  // The memory region, such as kMemoryRegionConventional.
  uint8_t region;
  // Start address of the memory region.
  uint32_t start;
  // Size of the memory region in bytes.
  uint32_t size;
  // Callback to read a byte from the memory region, where address is relative
  // to the start of the region.
  uint8_t (*read_memory_byte)(
      struct BIOSState* bios, uint32_t relative_address);
  // Callback to write a byte to memory, where address is relative to the start
  // of the region.
  void (*write_memory_byte)(
      struct BIOSState* bios, uint32_t relative_address, uint8_t value);
} MemoryRegion;

// Look up the memory region corresponding to an address. Returns NULL if the
// address is not mapped to a known memory region.
MemoryRegion* GetMemoryRegion(struct BIOSState* bios, uint32_t address);

// Read a byte from a logical memory address.
//
// On the 8086, accessing an invalid memory address will yield garbage data
// rather than causing a page fault. This callback interface mirrors that
// behavior.
uint8_t ReadLogicalMemoryByte(struct BIOSState* bios, uint32_t address);
// Read a word from a logical memory address.
uint16_t ReadLogicalMemoryWord(struct BIOSState* bios, uint32_t address);

// Write a byte to a logical memory address.
//
// On the 8086, accessing an invalid memory address will yield garbage data
// rather than causing a page fault. This callback interface mirrors that
// behavior.
void WriteLogicalMemoryByte(
    struct BIOSState* bios, uint32_t address, uint8_t value);
// Write a word to a logical memory address.
void WriteLogicalMemoryWord(
    struct BIOSState* bios, uint32_t address, uint16_t value);

// ============================================================================
// Text mode
// ============================================================================

enum {
  // Text mode framebuffer address.
  kTextModeFramebufferAddress = 0xB8000,
  // Number of columns in text mode.
  kTextModeColumns = 80,
  // Number of rows in text mode.
  kTextModeRows = 25,
  // Size of the text mode framebuffer in bytes. 2 bytes per character (char +
  // attribute).
  kTextModeFramebufferSize = kTextModeColumns * kTextModeRows * 2,
};

// ============================================================================
// BIOS state
// ============================================================================

// Caller-provided runtime configuration.
typedef struct BIOSConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Physical memory size in KB (1024 bytes). Must be between 64 and 640.
  uint16_t memory_size_kb;

  // Callback to read a byte from physical memory.
  //
  // On the 8086, accessing an invalid memory address will yield garbage data
  // rather than causing a page fault. This callback interface mirrors that
  // behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  uint8_t (*read_memory_byte)(struct BIOSState* bios, uint32_t address);

  // Callback to write a byte to physical memory.
  //
  // On the 8086, accessing an invalid memory address will yield garbage data
  // rather than causing a page fault. This callback interface mirrors that
  // behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  void (*write_memory_byte)(
      struct BIOSState* bios, uint32_t address, uint8_t value);
} BIOSConfig;

STATIC_VECTOR_TYPE(MemoryRegions, MemoryRegion, kMaxMemoryRegions)

// State of the BIOS.
typedef struct BIOSState {
  // Pointer to caller-provided runtime configuration
  BIOSConfig* config;

  // Memory map.
  MemoryRegions memory_regions;

  // Text mode framebuffer, located at kTextModeFramebufferAddress (0xB8000).
  uint8_t text_framebuffer[kTextModeFramebufferSize];
} BIOSState;

// ============================================================================
// BIOS Data Area (BDA)
// ============================================================================

enum {
  // Address of the BIOS Data Area.
  kBDAAddress = 0x0040,
  // Size of the BIOS Data Area in bytes.
  kBDASize = 0x100,
};

// Fields in the BIOS Data Area.
typedef enum BDAField {
  // 0x00: Base I/O address for serial ports.
  kBDASerialPortAddress = 0,
  // 0x08: Base I/O address for parallel ports.
  kBDAParallelPortAddress,
  // 0x10: Equipment word.
  kBDAEquipmentWord,
  // 0x12: POST status / Manufacturing test initialization flags
  kBDAPOSTStatus,
  // 0x13: Base memory size in kilobytes (0-640)
  kBDAMemorySize,
  // 0x15: Manufacturing test scratch pad
  kBDAManufacturingTest1,
  // 0x16: Manufacturing test scratch pad / BIOS control flags
  kBDAManufacturingTest2,
  // 0x17: Keyboard status flags 1
  kBDAKeyboardStatus1,
  // 0x18: Keyboard status flags 2
  kBDAKeyboardStatus2,
  // 0x19: Keyboard: Alt-nnn keypad workspace
  kBDAKeyboardAltNumpad,
  // 0x1A: Keyboard: ptr to next character in keyboard buffer
  kBDAKeyboardBufferHead,
  // 0x1C: Keyboard: ptr to first free slot in keyboard buffer
  kBDAKeyboardBufferTail,
  // 0x1E: Keyboard circular buffer (16 words)
  kBDAKeyboardBuffer,
  // 0x3E: Diskette recalibrate status
  kBDADisketteRecalibrateStatus,
  // 0x3F: Diskette motor status
  kBDADisketteMotorStatus,
  // 0x40: Diskette motor turn-off time-out count
  kBDADisketteMotorTimeout,
  // 0x41: Diskette last operation status
  kBDADisketteLastStatus,
  // 0x42: Diskette/Fixed disk status/command bytes (7 bytes)
  kBDADisketteStatusCommand,
  // 0x49: Video current mode
  kBDAVideoMode,
  // 0x4A: Video columns on screen
  kBDAVideoColumns,
  // 0x4C: Video page (regen buffer) size in bytes
  kBDAVideoPageSize,
  // 0x4E: Video current page start address in regen buffer
  kBDAVideoPageOffset,
  // 0x50: Video cursor position (col, row) for eight pages
  kBDAVideoCursorPos,
  // 0x60: Video cursor type, 6845 compatible
  kBDAVideoCursorType,
  // 0x62: Video current page number
  kBDAVideoCurrentPage,
  // 0x63: Video CRT controller base address
  kBDAVideoCRTBaseAddress,
  // 0x65: Video current setting of mode select register
  kBDAVideoModeSelect,
  // 0x66: Video current setting of CGA palette register
  kBDAVideoCGAPalette,
  // 0x67: POST real mode re-entry point after certain resets
  kBDAPostReentryPoint,
  // 0x6B: POST last unexpected interrupt
  kBDAPostLastInterrupt,
  // 0x6C: Timer ticks since midnight
  kBDATimerTicks,
  // 0x70: Timer overflow, non-zero if has counted past midnight
  kBDATimerOverflow,
  // 0x71: Ctrl-Break flag
  kBDACtrlBreakFlag,
  // 0x72: POST reset flag
  kBDAPostResetFlag,
  // 0x74: Fixed disk last operation status
  kBDAFixedDiskStatus,
  // 0x75: Fixed disk: number of fixed disk drives
  kBDAFixedDiskCount,
  // 0x76: Fixed disk: control byte
  kBDAFixedDiskControl,
  // 0x77: Fixed disk: I/O port offset
  kBDAFixedDiskPortOffset,
  // 0x78: Parallel devices 1-3 time-out counters
  kBDAParallelTimeout,
  // 0x7C: Serial devices 1-4 time-out counters
  kBDASerialTimeout,
  // 0x80: Keyboard buffer start offset
  kBDAKeyboardBufferStart,
  // 0x82: Keyboard buffer end+1 offset
  kBDAKeyboardBufferEnd,
  // 0x84: Video EGA/MCGA/VGA rows on screen minus one
  kBDAVideoRows,
  // 0x85: Video EGA/MCGA/VGA character height in scan-lines
  kBDAVideoCharHeight,
  // 0x87: Video EGA/VGA control
  kBDAVideoEGAControl,
  // 0x88: Video EGA/VGA switches
  kBDAVideoEGASwitches,
  // 0x89: Video MCGA/VGA mode-set option control
  kBDAVideoVGAControl,
  // 0x8A: Video index into Display Combination Code table
  kBDAVideoDCCIndex,
  // 0x8B: Diskette media control
  kBDADisketteMediaControl,
  // 0x8C: Fixed disk controller status
  kBDAFixedDiskControllerStatus,
  // 0x8D: Fixed disk controller Error Status
  kBDAFixedDiskErrorStatus,
  // 0x8E: Fixed disk Interrupt Control
  kBDAFixedDiskInterruptControl,
  // 0x8F: Diskette controller information
  kBDADisketteControllerInfo,
  // 0x90: Diskette drive 0 media state
  kBDADisketteDrive0MediaState,
  // 0x91: Diskette drive 1 media state
  kBDADisketteDrive1MediaState,
  // 0x92: Diskette drive 0 media state at start of operation
  kBDADisketteDrive0StartState,
  // 0x93: Diskette drive 1 media state at start of operation
  kBDADisketteDrive1StartState,
  // 0x94: Diskette drive 0 current track number
  kBDADisketteDrive0Track,
  // 0x95: Diskette drive 1 current track number
  kBDADisketteDrive1Track,
  // 0x96: Keyboard status byte 3
  kBDAKeyboardStatus3,
  // 0x97: Keyboard status byte 4
  kBDAKeyboardStatus4,
  // 0x98: Timer2: ptr to user wait-complete flag
  kBDATimer2WaitFlagPtr,
  // 0x9C: Timer2: user wait count in microseconds
  kBDATimer2WaitCount,
  // 0xA0: Timer2: Wait active flag
  kBDATimer2WaitActive,
  // 0xA1: Reserved for network adapters (7 bytes)
  kBDANetworkReserved,
  // 0xA8: Video: EGA/MCGA/VGA ptr to Video Save Pointer Table
  kBDAVideoSavePointerTable,
  // 0xAC: Reserved (4 bytes)
  kBDAReservedAC,
  // 0xB0: ptr to 3363 Optical disk driver or BIOS entry point
  kBDAOpticalDiskPtr,
  // 0xB4: Reserved (2 bytes)
  kBDAReservedB4,
  // 0xB6: Reserved for POST (3 bytes)
  kBDAReservedPost,
  // 0xB9: Unknown (7 bytes)
  kBDAUnknownB9,
  // 0xC0: Reserved (14 bytes)
  kBDAReservedC0,
  // 0xCE: Count of days since last boot
  kBDADaysSinceBoot,
  // 0xD0: Reserved (32 bytes)
  kBDAReservedD0,
  // 0xF0: Reserved for user (16 bytes)
  kBDAUserReserved,

  // Total number of fields in the BDA.
  kBDANumFields,
} BDAField;

// Metadata about each BIOS Data Area field.
typedef struct BDAFieldMetadata {
  // The BDA field.
  BDAField field;
  // Offset of the field in the BIOS Data Area.
  uint16_t offset;
  // Size of the field in bytes.
  uint8_t size;
} BDAFieldMetadata;

// BDAFieldMetadata array, indexed by BDAField enum.
extern const BDAFieldMetadata BDAFieldMetadataTable[kBDANumFields];

// Structure of the equipment word in the BDA at offset 0x10.
typedef struct EquipmentWord {
  // bits 15-14: number of parallel devices
  uint8_t parallel_devices : 2;
  // bit 13: Internal modem
  uint8_t reserved_13 : 1;
  // bit 12: reserved
  uint8_t reserved_12 : 1;
  // bits 11-9: number of serial devices
  uint8_t serial_devices : 3;
  // bit 8: reserved
  uint8_t reserved_8 : 1;
  // bits 7-6: number of diskette drives minus one
  uint8_t diskette_drives : 2;
  // bits 5-4: Initial video mode:
  //     00b = EGA,VGA,PGA
  //     01b = 40 x 25 color
  //     10b = 80 x 25 color
  //     11b = 80 x 25 mono
  uint8_t video_mode : 2;
  // bit 3: reserved
  uint8_t reserved_3 : 1;
  // bit 2: 1 if pointing device
  uint8_t pointing_device : 1;
  // bit 1: 1 if math co-processor
  uint8_t math_coprocessor : 1;
  // bit 0: 1 if diskette available for boot
  uint8_t diskette_boot_available : 1;
} EquipmentWord;

// Parse uint16_t as EquipmentWord.
EquipmentWord ParseEquipmentWord(uint16_t raw_equipment_word);
// Convert EquipmentWord to uint16_t.
uint16_t SerializeEquipmentWord(EquipmentWord equipment);

// Initialize BIOS state with the provided configuration.
void InitBIOS(BIOSState* bios, BIOSConfig* config);

#endif  // YAX86_BIOS_PUBLIC_H


// ==============================================================================
// src/bios/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/util/common.h start
// ==============================================================================

#line 1 "./src/util/common.h"
#ifndef YAX86_UTIL_COMMON_H
#define YAX86_UTIL_COMMON_H

// Macro that expands to `static` when bundled. Use for variables and functions
// that need to be visible to other files within the same module, but not
// publicly to users of the bundled library.
//
// This enables better IDE integration as it allows each source file to be
// compiled independently in unbundled form, but still keeps the symbols private
// when bundled.
#ifdef YAX86_IMPLEMENTATION
// When bundled, static linkage so that the symbol is only visible within the
// implementation file.
#define YAX86_PRIVATE static
#else
// When unbundled, use default linkage.
#define YAX86_PRIVATE
#endif  // YAX86_IMPLEMENTATION

#endif  // YAX86_UTIL_COMMON_H


// ==============================================================================
// src/util/common.h end
// ==============================================================================

// ==============================================================================
// src/bios/memory.c start
// ==============================================================================

#line 1 "./src/bios/memory.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Look up the memory region corresponding to an address. Returns NULL if the
// address is not mapped to a known memory region.
MemoryRegion* GetMemoryRegion(struct BIOSState* bios, uint32_t address) {
  for (uint8_t i = 0; i < MemoryRegionsLength(&bios->memory_regions); ++i) {
    MemoryRegion* region = MemoryRegionsGet(&bios->memory_regions, i);
    if (address >= region->start && address < region->start + region->size) {
      return region;
    }
  }
  return NULL;
}

// Read a byte from a logical memory address.
uint8_t ReadLogicalMemoryByte(struct BIOSState* bios, uint32_t address) {
  MemoryRegion* region = GetMemoryRegion(bios, address);
  if (!region || !region->read_memory_byte) {
    return 0xFF;
  }
  return region->read_memory_byte(bios, address - region->start);
}

// Read a word from a logical memory address.
uint16_t ReadLogicalMemoryWord(struct BIOSState* bios, uint32_t address) {
  uint8_t low_byte = ReadLogicalMemoryByte(bios, address);
  uint8_t high_byte = ReadLogicalMemoryByte(bios, address + 1);
  return (high_byte << 8) | low_byte;
}

// Write a byte to a logical memory address.
void WriteLogicalMemoryByte(
    struct BIOSState* bios, uint32_t address, uint8_t value) {
  MemoryRegion* region = GetMemoryRegion(bios, address);
  if (!region || !region->write_memory_byte) {
    return;
  }
  region->write_memory_byte(bios, address - region->start, value);
}

// Write a word to a logical memory address.
void WriteLogicalMemoryWord(
    struct BIOSState* bios, uint32_t address, uint16_t value) {
  WriteLogicalMemoryByte(bios, address, value & 0xFF);
  WriteLogicalMemoryByte(bios, address + 1, (value >> 8) & 0xFF);
}


// ==============================================================================
// src/bios/memory.c end
// ==============================================================================

// ==============================================================================
// src/bios/display_text.h start
// ==============================================================================

#line 1 "./src/bios/display_text.h"
#ifndef YAX86_BIOS_DISPLAY_TEXT_H
#define YAX86_BIOS_DISPLAY_TEXT_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"

// Initialize the display.
void InitDisplayText(BIOSState* bios);

// Read a byte from the display text buffer.
uint8_t ReadDisplayTextByte(BIOSState* bios, uint32_t address);
// Write a byte to the display text buffer.
void WriteDisplayTextByte(BIOSState* bios, uint32_t address, uint8_t value);

#endif  // YAX86_IMPLEMENTATION

#endif  // YAX86_BIOS_DISPLAY_TEXT_H


// ==============================================================================
// src/bios/display_text.h end
// ==============================================================================

// ==============================================================================
// src/bios/display_text.c start
// ==============================================================================

#line 1 "./src/bios/display_text.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

YAX86_PRIVATE void InitDisplayText(BIOSState* bios) {
  // TODO: Set display to text mode.

  // Initialize the text mode framebuffer to a blank state.
  for (int i = 0; i < kTextModeFramebufferSize; i += 2) {
    bios->text_framebuffer[i] = 0;
    bios->text_framebuffer[i + 1] = 0x07;
  }
}

uint8_t ReadDisplayTextByte(BIOSState* bios, uint32_t address) {
  if (address >= kTextModeFramebufferSize) {
    return 0xFF;  // Out of bounds, return garbage data.
  }
  return bios->text_framebuffer[address];
}

void WriteDisplayTextByte(BIOSState* bios, uint32_t address, uint8_t value) {
  if (address >= kTextModeFramebufferSize) {
    return;
  }
  bios->text_framebuffer[address] = value;
}


// ==============================================================================
// src/bios/display_text.c end
// ==============================================================================

// ==============================================================================
// src/bios/bios.c start
// ==============================================================================

#line 1 "./src/bios/bios.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "display_text.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

void InitBIOS(BIOSState* bios, BIOSConfig* config) {
  // Zero out the BIOS state.
  BIOSState empty_bios = {0};
  *bios = empty_bios;

  bios->config = config;

  MemoryRegionsInit(&bios->memory_regions);
  MemoryRegion conventional_memory = {
      .region = kMemoryRegionConventional,
      .start = 0x0000,
      .size = config->memory_size_kb * (2 << 10),
      .read_memory_byte = config->read_memory_byte,
      .write_memory_byte = config->write_memory_byte,
  };
  MemoryRegionsAppend(&bios->memory_regions, &conventional_memory);
  MemoryRegion text_mode_framebuffer = {
      .region = kMemoryRegionTextModeFramebuffer,
      .start = kTextModeFramebufferAddress,
      .size = kTextModeFramebufferSize,
      .read_memory_byte = ReadDisplayTextByte,
      .write_memory_byte = WriteDisplayTextByte,
  };
  MemoryRegionsAppend(&bios->memory_regions, &text_mode_framebuffer);

  InitDisplayText(bios);

  // TODO: Set BDA values.
}


// ==============================================================================
// src/bios/bios.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_BIOS_BUNDLE_H

