// Public interface for the Video module.
#ifndef YAX86_VIDEO_PUBLIC_H
#define YAX86_VIDEO_PUBLIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef YAX86_VIDEO_BUNDLE_H
#include "../util/static_vector.h"
#endif  // YAX86_VIDEO_BUNDLE_H

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
struct PlatformState;

// Caller-provided configuration for MDA text mode rendering.
typedef struct MDAConfig {
  // Custom data passed through to callbacks.
  void* context;

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
// Register memory map and I/O ports.
bool MDASetup(MDAState* mda, struct PlatformState* platform);

// Render the current display. Invokes the write_pixel callback to do the actual
// pixel rendering.
void MDARender(MDAState* mda);

#endif  // YAX86_VIDEO_PUBLIC_H
