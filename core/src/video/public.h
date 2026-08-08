// Public interface for the Video module.
#ifndef YAX86_VIDEO_PUBLIC_H
#define YAX86_VIDEO_PUBLIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef YAX86_VIDEO_BUNDLE_H
#include "../util/log.h"
#include "../util/static_vector.h"
#endif  // YAX86_VIDEO_BUNDLE_H

enum {
  // Log module ID for the Video.
  kLogModuleIDVideo = 8,
};

// Log module for the Video.
static const LogModule kLogModuleVideo = {
    .id = kLogModuleIDVideo,
    .name = "VIDEO",
};

// ============================================================================
// General
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

// Text mode character position. We use a different structure to avoid confusion
// with Position, which is used for pixel coordinates.
typedef struct TextPosition {
  // Column (0-based).
  uint8_t col;
  // Row (0-based).
  uint8_t row;
} TextPosition;

// Video modes.
typedef enum VideoMode {
  // CGA text mode 0x00: Text, 40×25, 16 colors, 320x200, 8x8
  kCGAText00 = 0x00,
  // CGA text mode 0x01: Text, 40×25, 16 colors, 320x200, 8x8
  kCGAText01 = 0x01,
  // CGA text mode 0x02: Text, 80×25, 16 colors, 640x200, 8x8
  kCGAText02 = 0x02,
  // CGA text mode 0x03: Text, 80×25, 16 colors, 640x200, 8x8
  kCGAText03 = 0x03,
  // CGA graphics mode 0x04: Graphics, 320×200, 4 colors
  kCGAGraphics04 = 0x04,
  // CGA graphics mode 0x05: Graphics, 320×200, 4 colors (no color burst)
  kCGAGraphics05 = 0x05,
  // CGA graphics mode 0x06: Graphics, 640×200, 2 colors
  kCGAGraphics06 = 0x06,
  // MDA text mode 0x07: Text, 80×25, monochrome, 720x350, 9x14
  kMDAText07 = 0x07,

  // Number of video modes supported.
  kNumVideoModes = 8,
} VideoMode;

// Text vs graphics modes.
typedef enum VideoModeType {
  // Invalid video mode. This is needed due to gap in the list of video mode
  // values.
  kVideoModeUnsupported = 0,
  // Text mode.
  kVideoModeText,
  // Graphics mode.
  kVideoModeGraphics,
} VideoModeType;

// Metadata for video modes.
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
  // Number of pages in the video mode.
  uint8_t num_pages;

  // Text mode - number of columns.
  uint8_t columns;
  // Text mode - number of rows.
  uint8_t rows;
  // Text mode - character width in pixels.
  uint8_t char_width;
  // Text mode - character height in pixels.
  uint8_t char_height;
} VideoModeMetadata;

// ============================================================================
// Monochrome Display and Printer Adapter (MDA)
// ============================================================================

// MDA I/O ports
// ========================================
// I/O Register |
// Address      |  Function
// -------------|--------------------------
// 3B0          | Not Used
// 3B1          | Not Used
// 3B2          | Not Used
// 3B3          | Not Used
// 3B4          | 6845 Index Register
// 3B5          | 6845 Data Register
// 3B6          | Not Used
// 3B7          | Not Used
// 3B8          | CRT Control Port 1
// 3B9          | Reserved
// 3BA          | CRT Status Port
// 3BB          | Reserved
// 3BC          | Parallel Data Port
// 3BD          | Printer Status Port
// 3BE          | Printer Control Port
// 3BF          | Not Used
// ========================================

// CRT Control Port 1 (I/O port 3B8) - write only
// ========================================
// Bit Number | Function
//------------|-------------------------
// 0          | + High Resolution Mode
// 1          | Not Used
// 2          | Not Used
// 3          | + Video Enable
// 4          | Not Used
// 5          | + Enable Blink
// 6,7        | Not Used
// ========================================

// CRT Status Port (I/O port 3BA) - read only
// ========================================
// Bit Number | Function
//------------|-------------------------
// 0          | + Horizontal Drive
// 1          | Reserved
// 2          | Reserved
// 3          | + Black/White Video
// ========================================

// The MDA contains a Motorola 6845 CRT controller. I/O port 3B4 is used to
// select a register, and port I/O port 3B5 is used to read or write the data
// for that register. Below are the registers and their default values for the
// IBM Monochrome Display.
// =============================================================================
// Register | Register File              | Program Unit     | IBM Monochrome
// Number   |                            |                  | Display
// ---------|----------------------------|------------------|------------------
// R0       | Horizontal Total           | Characters       | 0x61
// R1       | Horizontal Displayed       | Characters       | 0x50
// R2       | Horizontal Sync Position   | Characters       | 0x52
// R3       | Horizontal Sync Width      | Characters       | 0x0F
// R4       | Vertical Total             | Character Rows   | 0x19
// R5       | Vertical Total Adjust      | Scan Line        | 0x06
// R6       | Vertical Displayed         | Character Row    | 0x19
// R7       | Vertical Sync Position     | Character Row    | 0x19
// R8       | Interlace Mode             | --------         | 0x02
// R9       | Maximum Scan Line          | Scan Line        | 0x0D
// R10      | Cursor Start               | Scan Line        | 0x0B
// R11      | Cursor End                 | Scan Line        | 0x0C
// R12      | Start Address (H)          | --------         | 0x00
// R13      | Start Address (L)          | --------         | 0x00
// R14      | Cursor (H)                 | --------         | 0x00
// R15      | Cursor (L)                 | --------         | 0x00
// R16      | Reserved                   | --------         | --
// R17      | Reserved                   | --------         | --
// =============================================================================

// MDA registers
enum {
  kMDARegisterHorizontalTotal = 0,
  kMDARegisterHorizontalDisplayed,
  kMDARegisterHorizontalSyncPosition,
  kMDARegisterHorizontalSyncWidth,
  kMDARegisterVerticalTotal,
  kMDARegisterVerticalTotalAdjust,
  kMDARegisterVerticalDisplayed,
  kMDARegisterVerticalSyncPosition,
  kMDARegisterInterlaceMode,
  kMDARegisterMaximumScanLine,
  kMDARegisterCursorStart,
  kMDARegisterCursorEnd,
  kMDARegisterStartAddressH,
  kMDARegisterStartAddressL,
  kMDARegisterCursorH,
  kMDARegisterCursorL,
  kMDARegisterReserved16,
  kMDARegisterReserved17,

  // Total number of MDA registers.
  kMDANumRegisters,
};

// MDA I/O ports.
enum {
  kMDAPortRegisterIndex = 0x3B4,
  kMDAPortRegisterData = 0x3B5,
  kMDAPortControl = 0x3B8,
  kMDAPortStatus = 0x3BA,
  kMDAPortPrinterData = 0x3BC,
  kMDAPortPrinterStatus = 0x3BD,
  kMDAPortPrinterControl = 0x3BE,
};

enum {
  // MDA memory map entry type.
  kMemoryMapEntryMDAVRAM = 0x10,
  // MDA VRAM size.
  kMDAVRAMSize = 4 * 1024,  // 4K

  // MDA port map entry type.
  kPortMapEntryMDA = 0x10,
};

// MDA text mode 0x07: Text, 80×25, monochrome, 720x350, 9x14
static const VideoModeMetadata kMDAModeMetadata = {
    .mode = kMDAText07,
    .type = kVideoModeText,
    .vram_address = 0xB0000,
    .vram_size = kMDAVRAMSize,
    .width = 720,
    .height = 350,
    .num_pages = 1,
    .columns = 80,
    .rows = 25,
    .char_width = 9,
    .char_height = 14,
};

struct MDAState;

// Caller-provided configuration for MDA text mode rendering.
typedef struct MDAConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger for this module. May be NULL.
  Logger* logger;

  // Foreground color.
  RGB foreground;
  // Intense foreground color.
  RGB intense_foreground;
  // Background color.
  RGB background;

  // Callback to read a byte from the emulated video RAM.
  uint8_t (*read_vram_byte)(struct MDAState* mda, uint32_t address);
  // Callback to write a byte to the emulated video RAM.
  void (*write_vram_byte)(
      struct MDAState* mda, uint32_t address, uint8_t value);

  // Callback to write an RGB pixel value to the real display, invoked from
  // MDARender().
  void (*write_pixel)(struct MDAState* mda, Position position, RGB rgb);
} MDAConfig;

// Default MDA config.
static const MDAConfig kDefaultMDAConfig = {
    .context = NULL,

    .foreground = {.r = 0xAA, .g = 0xAA, .b = 0xAA},
    .intense_foreground = {.r = 0xFF, .g = 0xFF, .b = 0xFF},
    .background = {.r = 0x00, .g = 0x00, .b = 0x00},

    .read_vram_byte = NULL,
    .write_vram_byte = NULL,
    .write_pixel = NULL,
};

// MDA state.
typedef struct MDAState {
  // Caller-provided runtime configuration.
  MDAConfig* config;

  // Motorola 6845 CRT controller registers.
  uint8_t registers[kMDANumRegisters];
  // Currently selected 6845 CRT controller register index (I/O port 3B4).
  uint8_t selected_register;
  // Control port value (I/O port 3B8).
  uint8_t control_port;
  // Status port value (I/O port 3BA).
  uint8_t status_port;
} MDAState;

// Initialize MDA state with the provided configuration.
void MDAInit(MDAState* mda, MDAConfig* config);

// Read a byte from an MDA I/O port.
uint8_t MDAReadPort(MDAState* mda, uint16_t port);
// Write a byte to an MDA I/O port.
void MDAWritePort(MDAState* mda, uint16_t port, uint8_t value);

// Read a byte from MDA VRAM.
uint8_t MDAReadVRAM(MDAState* mda, uint32_t address);
// Write a byte to MDA VRAM.
void MDAWriteVRAM(MDAState* mda, uint32_t address, uint8_t value);

// Render the current display. Invokes the write_pixel callback to do the actual
// pixel rendering.
void MDARender(MDAState* mda);

// ============================================================================
// Color Graphics Adapter (CGA)
// ============================================================================

// CGA I/O ports
// ========================================
// I/O Register |
// Address      |  Function
// -------------|--------------------------
// 3D0          | Not Used
// 3D1          | Not Used
// 3D2          | Not Used
// 3D3          | Not Used
// 3D4          | 6845 Index Register
// 3D5          | 6845 Data Register
// 3D6          | Not Used
// 3D7          | Not Used
// 3D8          | Mode Control Register
// 3D9          | Color Select Register
// 3DA          | Status Register
// 3DB          | Clear Light Pen Latch
// 3DC          | Set Light Pen Latch
// 3DD-3DF      | Not Used
// ========================================

// Mode Control Register (I/O port 3D8) - write only
// ========================================
// Bit Number | Function
//------------|-------------------------
// 0          | + 80x25 Text Mode
// 1          | + Graphics Mode
// 2          | + Black & White Mode
// 3          | + Video Enable
// 4          | + 640x200 Graphics
// 5          | + Enable Blink
// 6,7        | Not Used
// ========================================

// Color Select Register (I/O port 3D9) - write only
// ========================================
// Bit Number | Function
//------------|-------------------------
// 0          | + Blue
// 1          | + Green
// 2          | + Red
// 3          | + Intensity
// 4          | + Alternate Intensity
// 5          | + Palette Select
// 6,7        | Not Used
// ========================================

// CGA Status Register (I/O port 3DA) - read only
// ========================================
// Bit Number | Function
//------------|-------------------------
// 0          | + Display Enable
// 1          | + Light Pen Trigger
// 2          | + Light Pen Switch
// 3          | + Vertical Retrace
// ========================================

// The CGA contains a Motorola 6845 CRT controller. I/O port 3D4 is used to
// select a register, and I/O port 3D5 is used to read or write the data
// for that register. Below are the registers and their default values for the
// IBM Color/Graphics Monitor Adapter in 80x25 text mode.
// =============================================================================
// Register | Register File              | Program Unit     | CGA 80x25
// Number   |                            |                  | Default
// ---------|----------------------------|------------------|------------------
// R0       | Horizontal Total           | Characters       | 0x71
// R1       | Horizontal Displayed       | Characters       | 0x50
// R2       | Horizontal Sync Position   | Characters       | 0x5A
// R3       | Horizontal Sync Width      | Characters       | 0x0A
// R4       | Vertical Total             | Character Rows   | 0x1F
// R5       | Vertical Total Adjust      | Scan Line        | 0x06
// R6       | Vertical Displayed         | Character Row    | 0x19
// R7       | Vertical Sync Position     | Character Row    | 0x1C
// R8       | Interlace Mode             | --------         | 0x02
// R9       | Maximum Scan Line          | Scan Line        | 0x07
// R10      | Cursor Start               | Scan Line        | 0x06
// R11      | Cursor End                 | Scan Line        | 0x07
// R12      | Start Address (H)          | --------         | 0x00
// R13      | Start Address (L)          | --------         | 0x00
// R14      | Cursor (H)                 | --------         | 0x00
// R15      | Cursor (L)                 | --------         | 0x00
// R16      | Reserved                   | --------         | --
// R17      | Reserved                   | --------         | --
// =============================================================================

// CGA I/O ports.
enum {
  kCGAPortRegisterIndex = 0x3D4,
  kCGAPortRegisterData = 0x3D5,
  kCGAPortModeControl = 0x3D8,
  kCGAPortColorSelect = 0x3D9,
  kCGAPortStatus = 0x3DA,
};

// CGA Mode Control Register (0x3D8) bit masks.
enum {
  // 80-column text mode (0 = 40-column).
  kCGAModeControl80Column = 0x01,
  // Graphics mode (0 = text mode).
  kCGAModeControlGraphics = 0x02,
  // Black & white mode (disable color burst).
  kCGAModeControlBW = 0x04,
  // Video enable.
  kCGAModeControlVideoEnable = 0x08,
  // High-resolution graphics (640x200).
  kCGAModeControlHiRes = 0x10,
  // Blink enable (text mode).
  kCGAModeControlBlink = 0x20,
};

// CGA Status Register (0x3DA) bit masks.
enum {
  // Display enable (1 = retrace active, safe to access VRAM).
  kCGAStatusDisplayEnable = 0x01,
  // Vertical retrace active.
  kCGAStatusVSync = 0x08,
};

enum {
  // CGA memory map entry type.
  kMemoryMapEntryCGAVRAM = 0x11,
  // CGA VRAM size (16KB).
  kCGAVRAMSize = 16 * 1024,

  // CGA port map entry type.
  kPortMapEntryCGA = 0x11,

  // Number of CGA RGBI palette colors.
  kCGANumColors = 16,

  // CGA 6845 registers (same layout as MDA).
  kCGANumRegisters = 18,
};

// CGA 16-color RGBI palette.
static const RGB kCGAPalette[kCGANumColors] = {
    {.r = 0x00, .g = 0x00, .b = 0x00},
    {.r = 0x00, .g = 0x00, .b = 0xAA},
    {.r = 0x00, .g = 0xAA, .b = 0x00},
    {.r = 0x00, .g = 0xAA, .b = 0xAA},
    {.r = 0xAA, .g = 0x00, .b = 0x00},
    {.r = 0xAA, .g = 0x00, .b = 0xAA},
    {.r = 0xAA, .g = 0x55, .b = 0x00},
    {.r = 0xAA, .g = 0xAA, .b = 0xAA},
    {.r = 0x55, .g = 0x55, .b = 0x55},
    {.r = 0x55, .g = 0x55, .b = 0xFF},
    {.r = 0x55, .g = 0xFF, .b = 0x55},
    {.r = 0x55, .g = 0xFF, .b = 0xFF},
    {.r = 0xFF, .g = 0x55, .b = 0x55},
    {.r = 0xFF, .g = 0x55, .b = 0xFF},
    {.r = 0xFF, .g = 0xFF, .b = 0x55},
    {.r = 0xFF, .g = 0xFF, .b = 0xFF},
};

// CGA video mode metadata for all 7 modes.
static const VideoModeMetadata kCGAModeMetadata[] = {
    // Mode 0: Text 40x25, 320x200, 8x8
    {.mode = kCGAText00,
     .type = kVideoModeText,
     .vram_address = 0xB8000,
     .vram_size = kCGAVRAMSize,
     .width = 320,
     .height = 200,
     .num_pages = 8,
     .columns = 40,
     .rows = 25,
     .char_width = 8,
     .char_height = 8},
    // Mode 1: Text 40x25, 320x200, 8x8
    {.mode = kCGAText01,
     .type = kVideoModeText,
     .vram_address = 0xB8000,
     .vram_size = kCGAVRAMSize,
     .width = 320,
     .height = 200,
     .num_pages = 8,
     .columns = 40,
     .rows = 25,
     .char_width = 8,
     .char_height = 8},
    // Mode 2: Text 80x25, 640x200, 8x8
    {.mode = kCGAText02,
     .type = kVideoModeText,
     .vram_address = 0xB8000,
     .vram_size = kCGAVRAMSize,
     .width = 640,
     .height = 200,
     .num_pages = 4,
     .columns = 80,
     .rows = 25,
     .char_width = 8,
     .char_height = 8},
    // Mode 3: Text 80x25, 640x200, 8x8
    {.mode = kCGAText03,
     .type = kVideoModeText,
     .vram_address = 0xB8000,
     .vram_size = kCGAVRAMSize,
     .width = 640,
     .height = 200,
     .num_pages = 4,
     .columns = 80,
     .rows = 25,
     .char_width = 8,
     .char_height = 8},
    // Mode 4: Graphics 320x200, 4-color
    {.mode = kCGAGraphics04,
     .type = kVideoModeGraphics,
     .vram_address = 0xB8000,
     .vram_size = kCGAVRAMSize,
     .width = 320,
     .height = 200,
     .num_pages = 1,
     .columns = 40,
     .rows = 25,
     .char_width = 8,
     .char_height = 8},
    // Mode 5: Graphics 320x200, 4-color (no color burst)
    {.mode = kCGAGraphics05,
     .type = kVideoModeGraphics,
     .vram_address = 0xB8000,
     .vram_size = kCGAVRAMSize,
     .width = 320,
     .height = 200,
     .num_pages = 1,
     .columns = 40,
     .rows = 25,
     .char_width = 8,
     .char_height = 8},
    // Mode 6: Graphics 640x200, 2-color
    {.mode = kCGAGraphics06,
     .type = kVideoModeGraphics,
     .vram_address = 0xB8000,
     .vram_size = kCGAVRAMSize,
     .width = 640,
     .height = 200,
     .num_pages = 1,
     .columns = 80,
     .rows = 25,
     .char_width = 8,
     .char_height = 8},
};

struct CGAState;

// Caller-provided configuration for CGA rendering.
typedef struct CGAConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger for this module. May be NULL.
  Logger* logger;

  // Callback to read a byte from the emulated video RAM.
  uint8_t (*read_vram_byte)(struct CGAState* cga, uint32_t address);
  // Callback to write a byte to the emulated video RAM.
  void (*write_vram_byte)(
      struct CGAState* cga, uint32_t address, uint8_t value);

  // Callback to write an RGB pixel value to the real display, invoked from
  // CGARender().
  void (*write_pixel)(struct CGAState* cga, Position position, RGB rgb);
} CGAConfig;

// Default CGA config.
static const CGAConfig kDefaultCGAConfig = {
    .context = NULL,

    .read_vram_byte = NULL,
    .write_vram_byte = NULL,
    .write_pixel = NULL,
};

// CGA state.
typedef struct CGAState {
  // Caller-provided runtime configuration.
  CGAConfig* config;

  // Motorola 6845 CRT controller registers.
  uint8_t registers[kCGANumRegisters];
  // Currently selected 6845 CRT controller register index (I/O port 0x3D4).
  uint8_t selected_register;
  // Mode control register value (I/O port 0x3D8).
  uint8_t mode_control;
  // Color select register value (I/O port 0x3D9).
  uint8_t color_select;
  // Status register value (I/O port 0x3DA).
  uint8_t status;
} CGAState;

// Initialize CGA state with the provided configuration.
void CGAInit(CGAState* cga, CGAConfig* config);

// Read a byte from a CGA I/O port.
uint8_t CGAReadPort(CGAState* cga, uint16_t port);
// Write a byte to a CGA I/O port.
void CGAWritePort(CGAState* cga, uint16_t port, uint8_t value);

// Read a byte from CGA VRAM.
uint8_t CGAReadVRAM(CGAState* cga, uint32_t address);
// Write a byte to CGA VRAM.
void CGAWriteVRAM(CGAState* cga, uint32_t address, uint8_t value);

// Render the current display. Invokes the write_pixel callback to do the
// actual pixel rendering. The pixel resolution of the rendered frame depends
// on the current CGA mode.
void CGARender(CGAState* cga);

// Get the metadata for the currently active CGA video mode, derived from
// the mode control register.
const VideoModeMetadata* CGAGetCurrentModeMetadata(const CGAState* cga);

#endif  // YAX86_VIDEO_PUBLIC_H

