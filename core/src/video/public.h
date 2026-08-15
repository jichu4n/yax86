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

// A rectangular portion of the host frame buffer, in pixels.
typedef struct VideoRegion {
  Position origin;
  uint16_t width;
  uint16_t height;
} VideoRegion;

// Text mode character position. We use a different structure to avoid confusion
// with Position, which is used for pixel coordinates.
typedef struct TextPosition {
  // Column (0-based).
  uint8_t col;
  // Row (0-based).
  uint8_t row;
} TextPosition;

// Video adapters supported by this module. Only one adapter is present in a
// machine at a time, selected before initialization.
typedef enum VideoAdapter {
  // Monochrome Display and Printer Adapter.
  kVideoAdapterMDA = 0,
  // Color Graphics Adapter.
  kVideoAdapterCGA = 1,

  // Number of adapters supported.
  kNumVideoAdapters = 2,
} VideoAdapter;

// Video modes, numbered as in the BIOS INT 10h mode numbers.
typedef enum VideoMode {
  // CGA text mode 0x00: Text, 40x25, color burst off, 8x8
  kVideoModeCGAText40x25Mono = 0x00,
  // CGA text mode 0x01: Text, 40x25, 16 colors, 8x8
  kVideoModeCGAText40x25Color = 0x01,
  // CGA text mode 0x02: Text, 80x25, color burst off, 8x8
  kVideoModeCGAText80x25Mono = 0x02,
  // CGA text mode 0x03: Text, 80x25, 16 colors, 8x8
  kVideoModeCGAText80x25Color = 0x03,
  // CGA graphics mode 0x04: 320x200, 4 colors
  kVideoModeCGAGraphics320x200 = 0x04,
  // CGA graphics mode 0x05: 320x200, 4 colors, alternate palette
  kVideoModeCGAGraphics320x200Alt = 0x05,
  // CGA graphics mode 0x06: 640x200, 2 colors
  kVideoModeCGAGraphics640x200 = 0x06,
  // MDA text mode 0x07: Text, 80×25, monochrome, 720x350, 9x14
  kVideoModeMDAText80x25 = 0x07,

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

// MDA I/O ports.
enum {
  kMDAPortRegisterIndex = 0x3B4,
  kMDAPortRegisterData = 0x3B5,
  kMDAPortControl = 0x3B8,
  kMDAPortStatus = 0x3BA,
  kMDAPortPrinterData = 0x3BC,
  kMDAPortPrinterStatus = 0x3BD,
  kMDAPortPrinterControl = 0x3BE,

  // Start of the MDA I/O port range.
  kMDAPortStart = 0x3B0,
  // Inclusive end of the MDA I/O port range.
  kMDAPortEnd = 0x3BF,
};

enum {
  // MDA VRAM address.
  kMDAVRAMAddress = 0xB0000,
  // MDA VRAM size.
  kMDAVRAMSize = 4 * 1024,  // 4K
};

// ============================================================================
// Color Graphics Adapter (CGA)
// ============================================================================

// CGA I/O ports
// ========================================
// I/O Register |
// Address      |  Function
// -------------|--------------------------
// 3D0          | 6845 Index Register (alias of 3D4)
// 3D1          | 6845 Data Register (alias of 3D5)
// 3D2          | 6845 Index Register (alias of 3D4)
// 3D3          | 6845 Data Register (alias of 3D5)
// 3D4          | 6845 Index Register
// 3D5          | 6845 Data Register
// 3D6          | 6845 Index Register (alias of 3D4)
// 3D7          | 6845 Data Register (alias of 3D5)
// 3D8          | Mode Control Register
// 3D9          | Color Select Register
// 3DA          | CRT Status Register
// 3DB          | Clear Light Pen Latch
// 3DC          | Preset Light Pen Latch
// 3DD - 3DF    | Not Used
// ========================================

// Mode Control Register (I/O port 3D8) - write only
// ========================================
// Bit Number | Function
//------------|-------------------------
// 0          | + 80x25 Text Mode (0 = 40x25)
// 1          | + Graphics Mode
// 2          | + Black/White Mode (disables the color burst)
// 3          | + Video Enable
// 4          | + 640x200 High Resolution Graphics Mode
// 5          | + Enable Blink
// 6,7        | Not Used
// ========================================

// Color Select Register (I/O port 3D9) - write only
// ========================================
// Bit Number | Function
//------------|-------------------------
// 0-2        | Background / border RGB. In 640x200 graphics, this is the
//            | foreground color instead.
// 3          | + Intensity of the above
// 4          | + Intensity of the 320x200 graphics palette
// 5          | 320x200 graphics palette (0 = green/red/brown,
//            | 1 = cyan/magenta/white)
// 6,7        | Not Used
// ========================================

// CRT Status Register (I/O port 3DA) - read only
// ========================================
// Bit Number | Function
//------------|-------------------------
// 0          | + Display disabled - VRAM access is safe
// 1          | + Light pen trigger set
// 2          | - Light pen switch is on
// 3          | + Vertical retrace in progress
// ========================================

// CGA I/O ports.
enum {
  kCGAPortRegisterIndex = 0x3D4,
  kCGAPortRegisterData = 0x3D5,
  kCGAPortControl = 0x3D8,
  kCGAPortColorSelect = 0x3D9,
  kCGAPortStatus = 0x3DA,
  kCGAPortClearLightPen = 0x3DB,
  kCGAPortPresetLightPen = 0x3DC,

  // Start of the CGA I/O port range.
  kCGAPortStart = 0x3D0,
  // Inclusive end of the CGA I/O port range.
  kCGAPortEnd = 0x3DF,
};

enum {
  // CGA VRAM address.
  kCGAVRAMAddress = 0xB8000,
  // CGA VRAM size.
  kCGAVRAMSize = 16 * 1024,  // 16K
  // Byte offset of the odd scan lines in CGA graphics modes. Graphics VRAM is
  // interleaved - even scan lines live in the first half and odd scan lines in
  // the second.
  kCGAGraphicsOddScanLineOffset = 0x2000,
  // Number of bytes per scan line in CGA graphics modes.
  kCGAGraphicsBytesPerScanLine = 80,
  // Number of colors in the CGA palette.
  kNumCGAColors = 16,
};

// ============================================================================
// Motorola 6845 CRT controller
// ============================================================================

// Both the MDA and the CGA are built around a Motorola 6845 CRT controller. The
// index port (3B4 on MDA, 3D4 on CGA) selects a register, and the data port
// (3B5 / 3D5) is used to read or write the data for that register. Below are
// the registers and their default values for the IBM Monochrome Display and for
// the CGA in 80x25 text mode, as programmed by GLaBIOS.
// =============================================================================
// Register | Register File              | Program Unit     | MDA  | CGA 80x25
// ---------|----------------------------|------------------|------|-----------
// R0       | Horizontal Total           | Characters       | 0x61 | 0x71
// R1       | Horizontal Displayed       | Characters       | 0x50 | 0x50
// R2       | Horizontal Sync Position   | Characters       | 0x52 | 0x5A
// R3       | Horizontal Sync Width      | Characters       | 0x0F | 0x0A
// R4       | Vertical Total             | Character Rows   | 0x19 | 0x1F
// R5       | Vertical Total Adjust      | Scan Line        | 0x06 | 0x06
// R6       | Vertical Displayed         | Character Row    | 0x19 | 0x19
// R7       | Vertical Sync Position     | Character Row    | 0x19 | 0x1C
// R8       | Interlace Mode             | --------         | 0x02 | 0x02
// R9       | Maximum Scan Line          | Scan Line        | 0x0D | 0x07
// R10      | Cursor Start               | Scan Line        | 0x0B | 0x06
// R11      | Cursor End                 | Scan Line        | 0x0C | 0x07
// R12      | Start Address (H)          | --------         | 0x00 | 0x00
// R13      | Start Address (L)          | --------         | 0x00 | 0x00
// R14      | Cursor (H)                 | --------         | 0x00 | 0x00
// R15      | Cursor (L)                 | --------         | 0x00 | 0x00
// R16      | Reserved                   | --------         | --   | --
// R17      | Reserved                   | --------         | --   | --
// =============================================================================

// 6845 CRT controller registers.
enum {
  kCRTCRegisterHorizontalTotal = 0,
  kCRTCRegisterHorizontalDisplayed,
  kCRTCRegisterHorizontalSyncPosition,
  kCRTCRegisterHorizontalSyncWidth,
  kCRTCRegisterVerticalTotal,
  kCRTCRegisterVerticalTotalAdjust,
  kCRTCRegisterVerticalDisplayed,
  kCRTCRegisterVerticalSyncPosition,
  kCRTCRegisterInterlaceMode,
  kCRTCRegisterMaximumScanLine,
  kCRTCRegisterCursorStart,
  kCRTCRegisterCursorEnd,
  kCRTCRegisterStartAddressH,
  kCRTCRegisterStartAddressL,
  kCRTCRegisterCursorH,
  kCRTCRegisterCursorL,
  kCRTCRegisterReserved16,
  kCRTCRegisterReserved17,

  // Total number of 6845 CRT controller registers.
  kNumCRTCRegisters,
};

// ============================================================================
// Register bits
// ============================================================================

// Bits in the mode control register - I/O port 3B8 on MDA, 3D8 on CGA.
enum {
  // CGA only - 80x25 text mode. MDA uses the same bit for high resolution mode,
  // which it always is.
  kVideoControlHighResolution = 1 << 0,
  // CGA only - graphics mode.
  kVideoControlGraphics = 1 << 1,
  // CGA only - black and white mode, i.e. the color burst is disabled. This has
  // no effect on an RGB monitor in text modes, but selects the third palette in
  // 320x200 graphics mode.
  kVideoControlBlackAndWhite = 1 << 2,
  // Video signal enabled. When clear, the display is blank.
  kVideoControlVideoEnable = 1 << 3,
  // CGA only - 640x200 high resolution graphics mode.
  kVideoControlHighResolutionGraphics = 1 << 4,
  // Attribute bit 7 means blinking rather than intense background.
  kVideoControlEnableBlink = 1 << 5,
};

// Bits in the CGA color select register - I/O port 3D9.
enum {
  // Background / border color, or the foreground color in 640x200 graphics.
  kCGAColorSelectColorMask = 0x07,
  // Intensity of the background / border color.
  kCGAColorSelectIntensity = 1 << 3,
  // Intensity of the 320x200 graphics palette.
  kCGAColorSelectPaletteIntensity = 1 << 4,
  // 320x200 graphics palette selection.
  kCGAColorSelectPalette = 1 << 5,
};

// Bits in the status register - I/O port 3BA on MDA, 3DA on CGA.
enum {
  // Display is disabled, i.e. a horizontal or vertical retrace is in progress,
  // so VRAM can be accessed without causing snow.
  kVideoStatusDisplayDisabled = 1 << 0,
  // Light pen trigger set. Always clear, as no light pen is emulated.
  kVideoStatusLightPenTrigger = 1 << 1,
  // Light pen switch is off. Always set, as no light pen is emulated.
  kVideoStatusLightPenSwitchOff = 1 << 2,
  // Vertical retrace in progress.
  kVideoStatusVerticalRetrace = 1 << 3,
};

// Bits in a text mode attribute byte.
enum {
  // Foreground color.
  kVideoAttributeForegroundMask = 0x07,
  // Intense foreground.
  kVideoAttributeIntenseForeground = 1 << 3,
  // Background color.
  kVideoAttributeBackgroundMask = 0x70,
  // Number of bits to shift right to get the background color.
  kVideoAttributeBackgroundShift = 4,
  // Blinking foreground, or intense background if blinking is disabled in the
  // mode control register.
  kVideoAttributeBlink = 1 << 7,
};

// ============================================================================
// Video mode and adapter metadata
// ============================================================================

// Metadata for each video mode, indexed by mode number.
static const VideoModeMetadata kVideoModeMetadata[kNumVideoModes] = {
    // 0x00: CGA text, 40x25, color burst off
    {
        .mode = kVideoModeCGAText40x25Mono,
        .type = kVideoModeText,
        .vram_address = kCGAVRAMAddress,
        .vram_size = kCGAVRAMSize,
        .width = 320,
        .height = 200,
        .num_pages = 8,
        .columns = 40,
        .rows = 25,
        .char_width = 8,
        .char_height = 8,
    },
    // 0x01: CGA text, 40x25, 16 colors
    {
        .mode = kVideoModeCGAText40x25Color,
        .type = kVideoModeText,
        .vram_address = kCGAVRAMAddress,
        .vram_size = kCGAVRAMSize,
        .width = 320,
        .height = 200,
        .num_pages = 8,
        .columns = 40,
        .rows = 25,
        .char_width = 8,
        .char_height = 8,
    },
    // 0x02: CGA text, 80x25, color burst off
    {
        .mode = kVideoModeCGAText80x25Mono,
        .type = kVideoModeText,
        .vram_address = kCGAVRAMAddress,
        .vram_size = kCGAVRAMSize,
        .width = 640,
        .height = 200,
        .num_pages = 4,
        .columns = 80,
        .rows = 25,
        .char_width = 8,
        .char_height = 8,
    },
    // 0x03: CGA text, 80x25, 16 colors
    {
        .mode = kVideoModeCGAText80x25Color,
        .type = kVideoModeText,
        .vram_address = kCGAVRAMAddress,
        .vram_size = kCGAVRAMSize,
        .width = 640,
        .height = 200,
        .num_pages = 4,
        .columns = 80,
        .rows = 25,
        .char_width = 8,
        .char_height = 8,
    },
    // 0x04: CGA graphics, 320x200, 4 colors
    {
        .mode = kVideoModeCGAGraphics320x200,
        .type = kVideoModeGraphics,
        .vram_address = kCGAVRAMAddress,
        .vram_size = kCGAVRAMSize,
        .width = 320,
        .height = 200,
        .num_pages = 1,
    },
    // 0x05: CGA graphics, 320x200, 4 colors, alternate palette
    {
        .mode = kVideoModeCGAGraphics320x200Alt,
        .type = kVideoModeGraphics,
        .vram_address = kCGAVRAMAddress,
        .vram_size = kCGAVRAMSize,
        .width = 320,
        .height = 200,
        .num_pages = 1,
    },
    // 0x06: CGA graphics, 640x200, 2 colors
    {
        .mode = kVideoModeCGAGraphics640x200,
        .type = kVideoModeGraphics,
        .vram_address = kCGAVRAMAddress,
        .vram_size = kCGAVRAMSize,
        .width = 640,
        .height = 200,
        .num_pages = 1,
    },
    // 0x07: MDA text, 80x25, monochrome
    {
        .mode = kVideoModeMDAText80x25,
        .type = kVideoModeText,
        .vram_address = kMDAVRAMAddress,
        .vram_size = kMDAVRAMSize,
        .width = 720,
        .height = 350,
        .num_pages = 1,
        .columns = 80,
        .rows = 25,
        .char_width = 9,
        .char_height = 14,
    },
};

// Metadata for a video adapter - the facts about an adapter that do not depend
// on the current video mode.
typedef struct VideoAdapterMetadata {
  // The adapter.
  VideoAdapter adapter;

  // Width of the frame buffer the host must provide, in pixels. This does not
  // change with the video mode: the CGA always scans the same number of dots
  // across the screen, and modes with a lower horizontal resolution are drawn
  // with each pixel doubled horizontally, just as on real hardware.
  uint16_t frame_buffer_width;
  // Height of the frame buffer the host must provide, in pixels.
  uint16_t frame_buffer_height;

  // Mapped memory address of video RAM.
  uint32_t vram_address;
  // Video RAM size in bytes.
  uint32_t vram_size;

  // Start of the adapter's I/O port range.
  uint16_t port_start;
  // Inclusive end of the adapter's I/O port range.
  uint16_t port_end;

  // Mode control register value at power-on. The power-on video mode follows
  // from it, so it is not stored separately.
  uint8_t default_control_register;

  // Number of CPU cycles per scan line.
  uint16_t cycles_per_scan_line;
  // Number of CPU cycles at the start of a scan line during which the display
  // is active. The remainder of the scan line is the horizontal retrace.
  uint16_t display_cycles_per_scan_line;
  // Number of scan lines per frame, including the vertical retrace.
  uint16_t scan_lines_per_frame;
  // Number of scan lines at the start of a frame that are displayed. The
  // remainder of the frame is the vertical retrace.
  uint16_t displayed_scan_lines;
} VideoAdapterMetadata;

// Metadata for each video adapter, indexed by VideoAdapter.
//
// The timing values are derived from the same 14.318MHz master crystal the CPU
// clock comes from, so they are exact in CPU cycles. The MDA scans 882 dots per
// line at 16.257MHz over 370 lines, which is 259 cycles per line and just under
// 50Hz. The CGA scans 912 dots per line at 14.318MHz over 262 lines, which is
// 304 cycles per line and just under 60Hz.
static const VideoAdapterMetadata kVideoAdapterMetadata[kNumVideoAdapters] = {
    // MDA
    {
        .adapter = kVideoAdapterMDA,
        .frame_buffer_width = 720,
        .frame_buffer_height = 350,
        .vram_address = kMDAVRAMAddress,
        .vram_size = kMDAVRAMSize,
        .port_start = kMDAPortStart,
        .port_end = kMDAPortEnd,
        // High resolution mode, video enable, blink enable.
        .default_control_register = 0x29,
        .cycles_per_scan_line = 259,
        .display_cycles_per_scan_line = 211,
        .scan_lines_per_frame = 370,
        .displayed_scan_lines = 350,
    },
    // CGA
    {
        .adapter = kVideoAdapterCGA,
        .frame_buffer_width = 640,
        .frame_buffer_height = 200,
        .vram_address = kCGAVRAMAddress,
        .vram_size = kCGAVRAMSize,
        .port_start = kCGAPortStart,
        .port_end = kCGAPortEnd,
        // 80x25 text mode, video enable, blink enable.
        .default_control_register = 0x29,
        .cycles_per_scan_line = 304,
        .display_cycles_per_scan_line = 213,
        .scan_lines_per_frame = 262,
        .displayed_scan_lines = 200,
    },
};

enum {
  // Number of frames between cursor blink phase changes. At roughly 50Hz this
  // gives a blink rate of about 3.1Hz, close to what the 6845 produces when it
  // is configured to blink at a sixteenth of the field rate.
  kVideoFramesPerCursorBlinkPhase = 8,
  // Number of frames between character blink phase changes. Characters blink
  // half as fast as the cursor: the 6845 generates the cursor blink itself,
  // while the blink attribute is decoded by the adapter from a separate
  // divider running at a thirty-secondth of the field rate.
  kVideoFramesPerTextBlinkPhase = 16,
};

enum {
  // Video memory map entry type.
  kMemoryMapEntryVRAM = 0x10,
  // Video port map entry type.
  kPortMapEntryVideo = 0x10,
};

enum {
  // Every supported mode divides horizontally into 80 natural rendering
  // units: text columns on the MDA and 80-column CGA, half-character cells in
  // 40-column CGA text modes, and VRAM bytes in CGA graphics modes.
  kVideoDirtyColumns = 80,
  // Vertically, one horizontal range is tracked per dirty row. A text mode's
  // dirty row is one character row, which covers exactly the scan lines a
  // changed cell occupies. Graphics modes have no character rows, so
  // neighboring scan lines share a range instead.
  kVideoDirtyScanLinesPerGroup = 4,
  // The tallest graphics mode is 200 lines.
  kVideoMaxGraphicsHeight = 200,
  // The most character rows in any text mode.
  kVideoMaxTextRows = 25,
  // Dirty rows needed to cover the tallest graphics mode.
  kVideoDirtyGraphicsRowCount =
      (kVideoMaxGraphicsHeight + kVideoDirtyScanLinesPerGroup - 1) /
      kVideoDirtyScanLinesPerGroup,
  // The tracker is statically sized for whichever mode needs the most rows.
  // Graphics modes do, at 50 against a text mode's 25.
  kVideoDirtyRowCount = kVideoDirtyGraphicsRowCount > kVideoMaxTextRows
                            ? kVideoDirtyGraphicsRowCount
                            : kVideoMaxTextRows,
};

// Half-open horizontal dirty range [first_column, end_column). Both fields are
// zero when the range is empty.
typedef struct VideoDirtyRange {
  uint8_t first_column;
  uint8_t end_column;
} VideoDirtyRange;

typedef struct VideoDirtyState {
  // One horizontal range per dirty row, indexed by dirty row.
  VideoDirtyRange ranges[kVideoDirtyRowCount];
  // At least one range is dirty. Avoids scanning the ranges on an unchanged
  // frame.
  bool any_dirty;
  // The next render must replace the complete frame buffer. Used when a change
  // cannot be represented as local VRAM damage, such as a mode or palette
  // change.
  bool full_redraw;
} VideoDirtyState;

// ============================================================================
// Video state
// ============================================================================

struct VideoState;

// Caller-provided configuration for video rendering.
typedef struct VideoConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger for this module. May be NULL.
  Logger* logger;

  // The video adapter to emulate.
  VideoAdapter adapter;

  // MDA - foreground color.
  RGB foreground;
  // MDA - intense foreground color.
  RGB intense_foreground;
  // MDA - background color.
  RGB background;

  // CGA - the 16 color RGBI palette.
  RGB cga_palette[kNumCGAColors];

  // The emulated video RAM, at least the adapter's vram_size bytes of it.
  // Required - the adapter reads and writes it directly.
  //
  // Rendering is the heaviest reader of video RAM, so a callback per byte
  // would cost an indirect call in its inner loops. Writes still go through
  // VideoWriteVRAM(), which marks the corresponding display region dirty.
  //
  // The caller owns the buffer and it must outlive the video state.
  uint8_t* vram;

  // Callback to write an RGB pixel value to the real display, invoked from
  // VideoRender().
  void (*write_pixel)(struct VideoState* video, Position position, RGB rgb);
  // Optional callbacks surrounding each dirty rectangular region emitted by
  // VideoRender(). A retained display can use them to set a transfer window
  // before the row-major pixel stream begins.
  void (*begin_render_region)(struct VideoState* video, VideoRegion region);
  void (*end_render_region)(struct VideoState* video);
} VideoConfig;

// Default video config.
static const VideoConfig kDefaultVideoConfig = {
    .context = NULL,

    .adapter = kVideoAdapterMDA,

    .foreground = {.r = 0xAA, .g = 0xAA, .b = 0xAA},
    .intense_foreground = {.r = 0xFF, .g = 0xFF, .b = 0xFF},
    .background = {.r = 0x00, .g = 0x00, .b = 0x00},

    .cga_palette =
        {
            {.r = 0x00, .g = 0x00, .b = 0x00},  // 0  black
            {.r = 0x00, .g = 0x00, .b = 0xAA},  // 1  blue
            {.r = 0x00, .g = 0xAA, .b = 0x00},  // 2  green
            {.r = 0x00, .g = 0xAA, .b = 0xAA},  // 3  cyan
            {.r = 0xAA, .g = 0x00, .b = 0x00},  // 4  red
            {.r = 0xAA, .g = 0x00, .b = 0xAA},  // 5  magenta
            {.r = 0xAA, .g = 0x55, .b = 0x00},  // 6  brown
            {.r = 0xAA, .g = 0xAA, .b = 0xAA},  // 7  light gray
            {.r = 0x55, .g = 0x55, .b = 0x55},  // 8  dark gray
            {.r = 0x55, .g = 0x55, .b = 0xFF},  // 9  light blue
            {.r = 0x55, .g = 0xFF, .b = 0x55},  // 10 light green
            {.r = 0x55, .g = 0xFF, .b = 0xFF},  // 11 light cyan
            {.r = 0xFF, .g = 0x55, .b = 0x55},  // 12 light red
            {.r = 0xFF, .g = 0x55, .b = 0xFF},  // 13 light magenta
            {.r = 0xFF, .g = 0xFF, .b = 0x55},  // 14 yellow
            {.r = 0xFF, .g = 0xFF, .b = 0xFF},  // 15 white
        },

    .vram = NULL,
    .write_pixel = NULL,
    .begin_render_region = NULL,
    .end_render_region = NULL,
};

// Video state.
typedef struct VideoState {
  // Caller-provided runtime configuration.
  VideoConfig* config;

  // The video adapter being emulated, copied from the config at init time.
  VideoAdapter adapter;

  // Motorola 6845 CRT controller registers.
  uint8_t registers[kNumCRTCRegisters];
  // Currently selected 6845 CRT controller register index (I/O port 3B4/3D4).
  uint8_t selected_register;
  // Mode control register value (I/O port 3B8/3D8).
  uint8_t control_register;
  // CGA color select register value (I/O port 3D9). Unused on MDA.
  uint8_t color_select_register;

  // Current scan line within the frame, including the vertical retrace.
  uint16_t scan_line;
  // CPU cycles elapsed within the current scan line.
  uint32_t scan_line_cycles;
  // Number of frames since initialization. Drives blinking.
  uint32_t frames;

  // Portions of the retained host display that no longer match video state.
  VideoDirtyState dirty;
} VideoState;

// Initialize video state with the provided configuration.
void VideoInit(VideoState* video, VideoConfig* config);

// Metadata for the adapter being emulated.
const VideoAdapterMetadata* VideoGetAdapterMetadata(const VideoState* video);

// The current video mode, derived from the mode control register.
VideoMode VideoGetMode(const VideoState* video);

// Metadata for the current video mode.
const VideoModeMetadata* VideoGetModeMetadata(const VideoState* video);

// Read a byte from a video I/O port.
uint8_t VideoReadPort(VideoState* video, uint16_t port);
// Write a byte to a video I/O port.
void VideoWritePort(VideoState* video, uint16_t port, uint8_t value);

// Read a byte from video RAM.
uint8_t VideoReadVRAM(VideoState* video, uint32_t address);
// Write a byte to video RAM.
void VideoWriteVRAM(VideoState* video, uint32_t address, uint8_t value);

// Advance the CRT beam by the given number of CPU cycles. This drives the
// retrace bits in the status register and the blink phase.
//
// The cost does not depend on how many cycles are passed, so a caller that
// advances the beam only when something is about to look at it may pass a
// whole frame's worth at once.
void VideoTick(VideoState* video, uint32_t cycles);

// Bring dirty portions of the retained host display up to date. Each region is
// bracketed by the optional region callbacks and its pixels are passed to
// write_pixel in row-major order. Does nothing when the display is unchanged.
void VideoRender(VideoState* video);

#endif  // YAX86_VIDEO_PUBLIC_H
