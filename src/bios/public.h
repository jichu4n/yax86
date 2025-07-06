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

// Memory region types.
enum {
  // Conventional memory - first 640KB of physical memory, mapped to 0x00000 to
  // 0x9FFFF (640KB).
  kMemoryRegionConventional = 0,
  // Video RAM. Mapping depends on the video mode.
  kMemoryRegionVideo = 1,

  // Maximum number of memory region entries.
  kMaxMemoryRegions = 8,
};

// A memory region in the BIOS memory map. Memory regions should not overlap.
typedef struct MemoryRegion {
  // The memory region type, such as kMemoryRegionConventional.
  uint8_t region_type;
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
MemoryRegion* GetMemoryRegionForAddress(
    struct BIOSState* bios, uint32_t address);
// Look up a memory region by type. Returns NULL if no region found with the
// specified type.
MemoryRegion* GetMemoryRegionByType(
    struct BIOSState* bios, uint8_t region_type);

// Read a byte from a logical memory address.
//
// On the 8086, accessing an invalid memory address will yield garbage data
// rather than causing a page fault. This callback interface mirrors that
// behavior.
uint8_t ReadMemoryByte(struct BIOSState* bios, uint32_t address);
// Read a word from a logical memory address.
uint16_t ReadMemoryWord(struct BIOSState* bios, uint32_t address);

// Write a byte to a logical memory address.
//
// On the 8086, accessing an invalid memory address will yield garbage data
// rather than causing a page fault. This callback interface mirrors that
// behavior.
void WriteMemoryByte(struct BIOSState* bios, uint32_t address, uint8_t value);
// Write a word to a logical memory address.
void WriteMemoryWord(struct BIOSState* bios, uint32_t address, uint16_t value);

// ============================================================================
// Video.
// ============================================================================

// RGB pixel value.
typedef struct RGB {
  // Red component (0-255).
  uint8_t r;
  // Green component (0-255).
  uint8_t g;
  // Blue component (0-255).
  uint8_t b;
} RGB;

// Position in 2D space.
typedef struct Position {
  // X coordinate.
  uint16_t x;
  // Y coordinate.
  uint16_t y;
} Position;

// Video modes.
typedef enum VideoMode {
  // CGA text mode 0x00: Text, 40×25, grayscale, 320x200, 8x8
  kVideoTextModeCGA00 = 0x00,
  // CGA text mode 0x01: Text, 40×25, 16 colors, 320x200, 8x8
  kVideoTextModeCGA01 = 0x01,
  // CGA text mode 0x02: Text, 80×25, grayscale, 640x200, 8x8
  kVideoTextModeCGA02 = 0x02,
  // CGA text mode 0x03: Text, 80×25, 16 colors, 640x200, 8x8
  kVideoTextModeCGA03 = 0x03,
  // CGA graphics mode 0x04: Graphics, 4 colors, 320×200
  kVideoGraphicsModeCGA04 = 0x04,
  // CGA graphics mode 0x05: Graphics, grayscale, 320×200
  kVideoGraphicsModeCGA05 = 0x05,
  // CGA graphics mode 0x06: Graphics, monochrome, 640×200
  kVideoGraphicsModeCGA06 = 0x06,
  // MDA text mode 0x07: Text, 80×25, monochrome, 720x350, 9x14
  kVideoTextModeMDA07 = 0x07,

  // Invalid video mode value.
  kInvalidVideoMode = 0xFF,

  // Number of video modes supported.
  kNumVideoModes = 8,
} VideoMode;

// Text vs graphics modes.
typedef enum VideoModeType {
  // Invalid video mode. This is needed due to gap in the list of video mode
  // values.
  kVideoModeUnsupported = 0,
  // Text mode.
  kVideoTextMode,
  // Graphics mode.
  kVideoGraphicsMode,
} VideoModeType;

// Metadata for each video mode.
typedef struct VideoModeMetadata {
  // The video mode.
  VideoMode mode;
  // Type of the video mode (text or graphics).
  VideoModeType type;
  // Mapped memory address of video RAM.
  uint32_t vram_address;
  // Video RAM size in bytes.
  uint32_t vram_size;
  // Resolution width in pixels.
  uint16_t width;
  // Resolution height in pixels.
  uint16_t height;

  // Text mode - number of columns.
  uint8_t columns;
  // Text mode - number of rows.
  uint8_t rows;
  // Text mode - character width in pixels.
  uint8_t char_width;
  // Text mode - character height in pixels.
  uint8_t char_height;
} VideoModeMetadata;

// Character width and height for text modes.
enum {
  // CGA text mode character width.
  kCGACharWidth = 8,
  // CGA text mode character height.
  kCGACharHeight = 8,
  // MDA text mode character width.
  kMDACharWidth = 9,
  // MDA text mode character height.
  kMDACharHeight = 14,
};

// Check if video mode is valid and supported.
extern bool IsSupportedVideoMode(uint8_t mode);

// Get current video mode. Returns kInvalidVideoMode if the video mode in the
// BIOS Data Area (BDA) is invalid.
extern VideoMode GetCurrentVideoMode(struct BIOSState* bios);
// Get current video mode metadata, or NULL if the video mode in the BIOS Data
// Area (BDA) is invalid.
extern const VideoModeMetadata* GetCurrentVideoModeMetadata(
    struct BIOSState* bios);
// Switch video mode.
extern bool SwitchVideoMode(struct BIOSState* bios, VideoMode mode);

// Text mode - clear screen.
extern void TextClearScreen(struct BIOSState* bios);

// Render the current page in the emulated video RAM to the real display.
// Invokes the write_pixel callback to do the actual pixel rendering.
extern bool RenderCurrentVideoPage(struct BIOSState* bios);

// Caller-provided configuration for MDA text mode rendering.
typedef struct MDAConfig {
  // Foreground color.
  RGB foreground;
  // Intense foreground color.
  RGB intense_foreground;
  // Background color.
  RGB background;
} MDAConfig;

// Default MDA config.
static const MDAConfig kDefaultMDAConfig = {
    .foreground = {.r = 0x00, .g = 0xAA, .b = 0x00},
    .intense_foreground = {.r = 0x55, .g = 0xFF, .b = 0x55},
    .background = {.r = 0x00, .g = 0x00, .b = 0x00}};

// Bitmasks for MDA character attributes.
enum {
  // Bit 7 - blink (0 = normal, 1 = blink).
  kMDABlink = 0x80,
  // Bits 6-4 - reverse video (000 = normal, 111 = reverse).
  kMDABackground = 0x70,
  // Bit 3 - intense foreground (0 = normal, 1 = intense).
  kMDAIntenseForeground = 0x08,
  // Bits 2-0 - underline (000 = normal, 001 = underline).
  kMDAForeground = 0x07,
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

  // MDA text mode configuration.
  MDAConfig mda_config;

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

  // Callback to read a byte from the emulated video RAM.
  uint8_t (*read_vram_byte)(struct BIOSState* bios, uint32_t address);

  // Callback to write a byte to the emulated video RAM.
  void (*write_vram_byte)(
      struct BIOSState* bios, uint32_t address, uint8_t value);

  // Callback to write an RGB pixel value to the real display, invoked from
  // RenderCurrentVideoPage().
  void (*write_pixel)(struct BIOSState* bios, Position position, RGB rgb);
} BIOSConfig;

STATIC_VECTOR_TYPE(MemoryRegions, MemoryRegion, kMaxMemoryRegions)

// State of the BIOS.
typedef struct BIOSState {
  // Pointer to caller-provided runtime configuration
  BIOSConfig* config;

  // Memory map.
  MemoryRegions memory_regions;
} BIOSState;

// ============================================================================
// BIOS Data Area (BDA)
// ============================================================================

enum {
  // Address of the BIOS Data Area.
  kBDAAddress = 0x0040,

  // 0x00: Base I/O address for serial ports.
  kBDASerialPortAddress = 0x00,
  // 0x08: Base I/O address for parallel ports.
  kBDAParallelPortAddress = 0x08,
  // 0x10: Equipment word.
  kBDAEquipmentWord = 0x10,
  // 0x12: POST status / Manufacturing test initialization flags
  kBDAPOSTStatus = 0x12,
  // 0x13: Base memory size in kilobytes (0-640)
  kBDAMemorySize = 0x13,
  // 0x15: Manufacturing test scratch pad
  kBDAManufacturingTest1 = 0x15,
  // 0x16: Manufacturing test scratch pad / BIOS control flags
  kBDAManufacturingTest2 = 0x16,
  // 0x17: Keyboard status flags 1
  kBDAKeyboardStatus1 = 0x17,
  // 0x18: Keyboard status flags 2
  kBDAKeyboardStatus2 = 0x18,
  // 0x19: Keyboard: Alt-nnn keypad workspace
  kBDAKeyboardAltNumpad = 0x19,
  // 0x1A: Keyboard: ptr to next character in keyboard buffer
  kBDAKeyboardBufferHead = 0x1A,
  // 0x1C: Keyboard: ptr to first free slot in keyboard buffer
  kBDAKeyboardBufferTail = 0x1C,
  // 0x1E: Keyboard circular buffer (16 words)
  kBDAKeyboardBuffer = 0x1E,
  // 0x3E: Diskette recalibrate status
  kBDADisketteRecalibrateStatus = 0x3E,
  // 0x3F: Diskette motor status
  kBDADisketteMotorStatus = 0x3F,
  // 0x40: Diskette motor turn-off time-out count
  kBDADisketteMotorTimeout = 0x40,
  // 0x41: Diskette last operation status
  kBDADisketteLastStatus = 0x41,
  // 0x42: Diskette/Fixed disk status/command bytes (7 bytes)
  kBDADisketteStatusCommand = 0x42,
  // 0x49: Video current mode
  kBDAVideoMode = 0x49,
  // 0x4A: Video columns on screen
  kBDAVideoColumns = 0x4A,
  // 0x4C: Video page (regen buffer) size in bytes
  kBDAVideoPageSize = 0x4C,
  // 0x4E: Video current page start address in regen buffer
  kBDAVideoPageOffset = 0x4E,
  // 0x50: Video cursor position (col, row) for eight pages
  kBDAVideoCursorPos = 0x50,
  // 0x60: Video cursor type, 6845 compatible
  kBDAVideoCursorType = 0x60,
  // 0x62: Video current page number
  kBDAVideoCurrentPage = 0x62,
  // 0x63: Video CRT controller base address
  kBDAVideoCRTBaseAddress = 0x63,
  // 0x65: Video current setting of mode select register
  kBDAVideoModeSelect = 0x65,
  // 0x66: Video current setting of CGA palette register
  kBDAVideoCGAPalette = 0x66,
  // 0x67: POST real mode re-entry point after certain resets
  kBDAPostReentryPoint = 0x67,
  // 0x6B: POST last unexpected interrupt
  kBDAPostLastInterrupt = 0x6B,
  // 0x6C: Timer ticks since midnight
  kBDATimerTicks = 0x6C,
  // 0x70: Timer overflow, non-zero if has counted past midnight
  kBDATimerOverflow = 0x70,
  // 0x71: Ctrl-Break flag
  kBDACtrlBreakFlag = 0x71,
  // 0x72: POST reset flag
  kBDAPostResetFlag = 0x72,
  // 0x74: Fixed disk last operation status
  kBDAFixedDiskStatus = 0x74,
  // 0x75: Fixed disk: number of fixed disk drives
  kBDAFixedDiskCount = 0x75,
  // 0x76: Fixed disk: control byte
  kBDAFixedDiskControl = 0x76,
  // 0x77: Fixed disk: I/O port offset
  kBDAFixedDiskPortOffset = 0x77,
  // 0x78: Parallel devices 1-3 time-out counters
  kBDAParallelTimeout = 0x78,
  // 0x7C: Serial devices 1-4 time-out counters
  kBDASerialTimeout = 0x7C,
  // 0x80: Keyboard buffer start offset
  kBDAKeyboardBufferStart = 0x80,
  // 0x82: Keyboard buffer end+1 offset
  kBDAKeyboardBufferEnd = 0x82,
  // 0x84: Video EGA/MCGA/VGA rows on screen minus one
  kBDAVideoRows = 0x84,
  // 0x85: Video EGA/MCGA/VGA character height in scan-lines
  kBDAVideoCharHeight = 0x85,
  // 0x87: Video EGA/VGA control
  kBDAVideoEGAControl = 0x87,
  // 0x88: Video EGA/VGA switches
  kBDAVideoEGASwitches = 0x88,
  // 0x89: Video MCGA/VGA mode-set option control
  kBDAVideoVGAControl = 0x89,
  // 0x8A: Video index into Display Combination Code table
  kBDAVideoDCCIndex = 0x8A,
  // 0x8B: Diskette media control
  kBDADisketteMediaControl = 0x8B,
  // 0x8C: Fixed disk controller status
  kBDAFixedDiskControllerStatus = 0x8C,
  // 0x8D: Fixed disk controller Error Status
  kBDAFixedDiskErrorStatus = 0x8D,
  // 0x8E: Fixed disk Interrupt Control
  kBDAFixedDiskInterruptControl = 0x8E,
  // 0x8F: Diskette controller information
  kBDADisketteControllerInfo = 0x8F,
  // 0x90: Diskette drive 0 media state
  kBDADisketteDrive0MediaState = 0x90,
  // 0x91: Diskette drive 1 media state
  kBDADisketteDrive1MediaState = 0x91,
  // 0x92: Diskette drive 0 media state at start of operation
  kBDADisketteDrive0StartState = 0x92,
  // 0x93: Diskette drive 1 media state at start of operation
  kBDADisketteDrive1StartState = 0x93,
  // 0x94: Diskette drive 0 current track number
  kBDADisketteDrive0Track = 0x94,
  // 0x95: Diskette drive 1 current track number
  kBDADisketteDrive1Track = 0x95,
  // 0x96: Keyboard status byte 3
  kBDAKeyboardStatus3 = 0x96,
  // 0x97: Keyboard status byte 4
  kBDAKeyboardStatus4 = 0x97,
  // 0x98: Timer2: ptr to user wait-complete flag
  kBDATimer2WaitFlagPtr = 0x98,
  // 0x9C: Timer2: user wait count in microseconds
  kBDATimer2WaitCount = 0x9C,
  // 0xA0: Timer2: Wait active flag
  kBDATimer2WaitActive = 0xA0,
  // 0xA1: Reserved for network adapters (7 bytes)
  kBDANetworkReserved = 0xA1,
  // 0xA8: Video: EGA/MCGA/VGA ptr to Video Save Pointer Table
  kBDAVideoSavePointerTable = 0xA8,
  // 0xAC: Reserved (4 bytes)
  kBDAReservedAC = 0xAC,
  // 0xB0: ptr to 3363 Optical disk driver or BIOS entry point
  kBDAOpticalDiskPtr = 0xB0,
  // 0xB4: Reserved (2 bytes)
  kBDAReservedB4 = 0xB4,
  // 0xB6: Reserved for POST (3 bytes)
  kBDAReservedPost = 0xB6,
  // 0xB9: Unknown (7 bytes)
  kBDAUnknownB9 = 0xB9,
  // 0xC0: Reserved (14 bytes)
  kBDAReservedC0 = 0xC0,
  // 0xCE: Count of days since last boot
  kBDADaysSinceBoot = 0xCE,
  // 0xD0: Reserved (32 bytes)
  kBDAReservedD0 = 0xD0,
  // 0xF0: Reserved for user (16 bytes)
  kBDAUserReserved = 0xF0,
};

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
