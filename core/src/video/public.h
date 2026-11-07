// Public interface for the Video module.
#ifndef Y_VIDEO_PUBLIC_H
#define Y_VIDEO_PUBLIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef Y_VIDEO_BUNDLE_H
#include "../util/log.h"
#include "../util/static_vector.h"
#endif  // Y_VIDEO_BUNDLE_H

enum {
  // Log module ID for the Video.
  Y_VIDEO_LOG_MODULE_ID = 8,
};

// Log module for the Video.
Y_PUBLIC_HEADER const YLogModule YVideoLogModule = {
    .id = Y_VIDEO_LOG_MODULE_ID,
    .name = "VIDEO",
};

// ============================================================================
// General
// ============================================================================

// RGB pixel value.
typedef struct YVideoRGB {
  // Red component (0-255).
  uint8_t r;
  // Green component (0-255).
  uint8_t g;
  // Blue component (0-255).
  uint8_t b;
} YVideoRGB;

// Position in 2D space.
typedef struct YVideoPosition {
  // X coordinate.
  uint16_t x;
  // Y coordinate.
  uint16_t y;
} YVideoPosition;

// A rectangular portion of the host frame buffer, in pixels.
typedef struct YVideoRegion {
  YVideoPosition origin;
  uint16_t width;
  uint16_t height;
} YVideoRegion;

// Text mode character position. We use a different structure to avoid confusion
// with YVideoPosition, which is used for pixel coordinates.
typedef struct YVideoTextPosition {
  // Column (0-based).
  uint8_t col;
  // Row (0-based).
  uint8_t row;
} YVideoTextPosition;

// Video adapters supported by this module. Only one adapter is present in a
// machine at a time, selected before initialization.
typedef enum YVideoAdapter {
  // Monochrome Display and Printer Adapter.
  Y_VIDEO_ADAPTER_MDA = 0,
  // Color Graphics Adapter.
  Y_VIDEO_ADAPTER_CGA = 1,

  // Number of adapters supported.
  Y_VIDEO_NUM_ADAPTERS = 2,
} YVideoAdapter;

// Video modes, numbered as in the BIOS INT 10h mode numbers.
typedef enum YVideoMode {
  // CGA text mode 0x00: Text, 40x25, color burst off, 8x8
  Y_VIDEO_MODE_CGA_TEXT_40X25_MONO = 0x00,
  // CGA text mode 0x01: Text, 40x25, 16 colors, 8x8
  Y_VIDEO_MODE_CGA_TEXT_40X25_COLOR = 0x01,
  // CGA text mode 0x02: Text, 80x25, color burst off, 8x8
  Y_VIDEO_MODE_CGA_TEXT_80X25_MONO = 0x02,
  // CGA text mode 0x03: Text, 80x25, 16 colors, 8x8
  Y_VIDEO_MODE_CGA_TEXT_80X25_COLOR = 0x03,
  // CGA graphics mode 0x04: 320x200, 4 colors
  Y_VIDEO_MODE_CGA_GRAPHICS_320X200 = 0x04,
  // CGA graphics mode 0x05: 320x200, 4 colors, alternate palette
  Y_VIDEO_MODE_CGA_GRAPHICS_320X200_ALT = 0x05,
  // CGA graphics mode 0x06: 640x200, 2 colors
  Y_VIDEO_MODE_CGA_GRAPHICS_640X200 = 0x06,
  // MDA text mode 0x07: Text, 80×25, monochrome, 720x350, 9x14
  Y_VIDEO_MODE_MDA_TEXT_80X25 = 0x07,

  // Number of video modes supported.
  Y_VIDEO_NUM_MODES = 8,
} YVideoMode;

// Text vs graphics modes.
typedef enum YVideoModeType {
  // Invalid video mode. This is needed due to gap in the list of video mode
  // values.
  Y_VIDEO_MODE_UNSUPPORTED = 0,
  // Text mode.
  Y_VIDEO_MODE_TEXT,
  // Graphics mode.
  Y_VIDEO_MODE_GRAPHICS,
} YVideoModeType;

// Metadata for video modes.
typedef struct YVideoModeMetadata {
  // The video mode.
  YVideoMode mode;
  // Type of the video mode (text or graphics).
  YVideoModeType type;
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
} YVideoModeMetadata;

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
  Y_VIDEO_MDA_PORT_REGISTER_INDEX = 0x3B4,
  Y_VIDEO_MDA_PORT_REGISTER_DATA = 0x3B5,
  Y_VIDEO_MDA_PORT_CONTROL = 0x3B8,
  Y_VIDEO_MDA_PORT_STATUS = 0x3BA,
  Y_VIDEO_MDA_PORT_PRINTER_DATA = 0x3BC,
  Y_VIDEO_MDA_PORT_PRINTER_STATUS = 0x3BD,
  Y_VIDEO_MDA_PORT_PRINTER_CONTROL = 0x3BE,

  // Start of the MDA I/O port range.
  Y_VIDEO_MDA_PORT_START = 0x3B0,
  // Inclusive end of the MDA I/O port range.
  Y_VIDEO_MDA_PORT_END = 0x3BF,
};

enum {
  // MDA VRAM address.
  Y_VIDEO_MDA_VRAM_ADDRESS = 0xB0000,
  // MDA VRAM size.
  Y_VIDEO_MDA_VRAM_SIZE = 4 * 1024,  // 4K
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
  Y_VIDEO_CGA_PORT_REGISTER_INDEX = 0x3D4,
  Y_VIDEO_CGA_PORT_REGISTER_DATA = 0x3D5,
  Y_VIDEO_CGA_PORT_CONTROL = 0x3D8,
  Y_VIDEO_CGA_PORT_COLOR_SELECT = 0x3D9,
  Y_VIDEO_CGA_PORT_STATUS = 0x3DA,
  Y_VIDEO_CGA_PORT_CLEAR_LIGHT_PEN = 0x3DB,
  Y_VIDEO_CGA_PORT_PRESET_LIGHT_PEN = 0x3DC,

  // Start of the CGA I/O port range.
  Y_VIDEO_CGA_PORT_START = 0x3D0,
  // Inclusive end of the CGA I/O port range.
  Y_VIDEO_CGA_PORT_END = 0x3DF,
};

enum {
  // CGA VRAM address.
  Y_VIDEO_CGA_VRAM_ADDRESS = 0xB8000,
  // CGA VRAM size.
  Y_VIDEO_CGA_VRAM_SIZE = 16 * 1024,  // 16K
  // Byte offset of the odd scan lines in CGA graphics modes. Graphics VRAM is
  // interleaved - even scan lines live in the first half and odd scan lines in
  // the second.
  Y_VIDEO_CGA_GRAPHICS_ODD_SCAN_LINE_OFFSET = 0x2000,
  // Number of bytes per scan line in CGA graphics modes.
  Y_VIDEO_CGA_GRAPHICS_BYTES_PER_SCAN_LINE = 80,
  // Number of colors in the CGA palette.
  Y_VIDEO_NUM_CGA_COLORS = 16,
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
  Y_VIDEO_CRTC_REGISTER_HORIZONTAL_TOTAL = 0,
  Y_VIDEO_CRTC_REGISTER_HORIZONTAL_DISPLAYED,
  Y_VIDEO_CRTC_REGISTER_HORIZONTAL_SYNC_POSITION,
  Y_VIDEO_CRTC_REGISTER_HORIZONTAL_SYNC_WIDTH,
  Y_VIDEO_CRTC_REGISTER_VERTICAL_TOTAL,
  Y_VIDEO_CRTC_REGISTER_VERTICAL_TOTAL_ADJUST,
  Y_VIDEO_CRTC_REGISTER_VERTICAL_DISPLAYED,
  Y_VIDEO_CRTC_REGISTER_VERTICAL_SYNC_POSITION,
  Y_VIDEO_CRTC_REGISTER_INTERLACE_MODE,
  Y_VIDEO_CRTC_REGISTER_MAXIMUM_SCAN_LINE,
  Y_VIDEO_CRTC_REGISTER_CURSOR_START,
  Y_VIDEO_CRTC_REGISTER_CURSOR_END,
  Y_VIDEO_CRTC_REGISTER_START_ADDRESS_H,
  Y_VIDEO_CRTC_REGISTER_START_ADDRESS_L,
  Y_VIDEO_CRTC_REGISTER_CURSOR_H,
  Y_VIDEO_CRTC_REGISTER_CURSOR_L,
  Y_VIDEO_CRTC_REGISTER_RESERVED16,
  Y_VIDEO_CRTC_REGISTER_RESERVED17,

  // Total number of 6845 CRT controller registers.
  Y_VIDEO_NUM_CRTC_REGISTERS,
};

// ============================================================================
// Register bits
// ============================================================================

// Bits in the mode control register - I/O port 3B8 on MDA, 3D8 on CGA.
enum {
  // CGA only - 80x25 text mode. MDA uses the same bit for high resolution mode,
  // which it always is.
  Y_VIDEO_CONTROL_HIGH_RESOLUTION = 1 << 0,
  // CGA only - graphics mode.
  Y_VIDEO_CONTROL_GRAPHICS = 1 << 1,
  // CGA only - black and white mode, i.e. the color burst is disabled. This has
  // no effect on an RGB monitor in text modes, but selects the third palette in
  // 320x200 graphics mode.
  Y_VIDEO_CONTROL_BLACK_AND_WHITE = 1 << 2,
  // Video signal enabled. When clear, the display is blank.
  Y_VIDEO_CONTROL_VIDEO_ENABLE = 1 << 3,
  // CGA only - 640x200 high resolution graphics mode.
  Y_VIDEO_CONTROL_HIGH_RESOLUTION_GRAPHICS = 1 << 4,
  // Attribute bit 7 means blinking rather than intense background.
  Y_VIDEO_CONTROL_ENABLE_BLINK = 1 << 5,
};

// Bits in the CGA color select register - I/O port 3D9.
enum {
  // Background / border color, or the foreground color in 640x200 graphics.
  Y_VIDEO_CGA_COLOR_SELECT_COLOR_MASK = 0x07,
  // Intensity of the background / border color.
  Y_VIDEO_CGA_COLOR_SELECT_INTENSITY = 1 << 3,
  // Intensity of the 320x200 graphics palette.
  Y_VIDEO_CGA_COLOR_SELECT_PALETTE_INTENSITY = 1 << 4,
  // 320x200 graphics palette selection.
  Y_VIDEO_CGA_COLOR_SELECT_PALETTE = 1 << 5,
};

// Bits in the status register - I/O port 3BA on MDA, 3DA on CGA.
enum {
  // Display is disabled, i.e. a horizontal or vertical retrace is in progress,
  // so VRAM can be accessed without causing snow.
  Y_VIDEO_STATUS_DISPLAY_DISABLED = 1 << 0,
  // Light pen trigger set. Always clear, as no light pen is emulated.
  Y_VIDEO_STATUS_LIGHT_PEN_TRIGGER = 1 << 1,
  // Light pen switch is off. Always set, as no light pen is emulated.
  Y_VIDEO_STATUS_LIGHT_PEN_SWITCH_OFF = 1 << 2,
  // Vertical retrace in progress.
  Y_VIDEO_STATUS_VERTICAL_RETRACE = 1 << 3,
};

// Bits in a text mode attribute byte.
enum {
  // Foreground color.
  Y_VIDEO_ATTRIBUTE_FOREGROUND_MASK = 0x07,
  // Intense foreground.
  Y_VIDEO_ATTRIBUTE_INTENSE_FOREGROUND = 1 << 3,
  // Background color.
  Y_VIDEO_ATTRIBUTE_BACKGROUND_MASK = 0x70,
  // Number of bits to shift right to get the background color.
  Y_VIDEO_ATTRIBUTE_BACKGROUND_SHIFT = 4,
  // Blinking foreground, or intense background if blinking is disabled in the
  // mode control register.
  Y_VIDEO_ATTRIBUTE_BLINK = 1 << 7,
};

// ============================================================================
// Video mode and adapter metadata
// ============================================================================

// Metadata for each video mode, indexed by mode number.
Y_PUBLIC_HEADER const YVideoModeMetadata
    YVideoModeMetadataTable[Y_VIDEO_NUM_MODES] = {
        // 0x00: CGA text, 40x25, color burst off
        {
            .mode = Y_VIDEO_MODE_CGA_TEXT_40X25_MONO,
            .type = Y_VIDEO_MODE_TEXT,
            .vram_address = Y_VIDEO_CGA_VRAM_ADDRESS,
            .vram_size = Y_VIDEO_CGA_VRAM_SIZE,
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
            .mode = Y_VIDEO_MODE_CGA_TEXT_40X25_COLOR,
            .type = Y_VIDEO_MODE_TEXT,
            .vram_address = Y_VIDEO_CGA_VRAM_ADDRESS,
            .vram_size = Y_VIDEO_CGA_VRAM_SIZE,
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
            .mode = Y_VIDEO_MODE_CGA_TEXT_80X25_MONO,
            .type = Y_VIDEO_MODE_TEXT,
            .vram_address = Y_VIDEO_CGA_VRAM_ADDRESS,
            .vram_size = Y_VIDEO_CGA_VRAM_SIZE,
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
            .mode = Y_VIDEO_MODE_CGA_TEXT_80X25_COLOR,
            .type = Y_VIDEO_MODE_TEXT,
            .vram_address = Y_VIDEO_CGA_VRAM_ADDRESS,
            .vram_size = Y_VIDEO_CGA_VRAM_SIZE,
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
            .mode = Y_VIDEO_MODE_CGA_GRAPHICS_320X200,
            .type = Y_VIDEO_MODE_GRAPHICS,
            .vram_address = Y_VIDEO_CGA_VRAM_ADDRESS,
            .vram_size = Y_VIDEO_CGA_VRAM_SIZE,
            .width = 320,
            .height = 200,
            .num_pages = 1,
        },
        // 0x05: CGA graphics, 320x200, 4 colors, alternate palette
        {
            .mode = Y_VIDEO_MODE_CGA_GRAPHICS_320X200_ALT,
            .type = Y_VIDEO_MODE_GRAPHICS,
            .vram_address = Y_VIDEO_CGA_VRAM_ADDRESS,
            .vram_size = Y_VIDEO_CGA_VRAM_SIZE,
            .width = 320,
            .height = 200,
            .num_pages = 1,
        },
        // 0x06: CGA graphics, 640x200, 2 colors
        {
            .mode = Y_VIDEO_MODE_CGA_GRAPHICS_640X200,
            .type = Y_VIDEO_MODE_GRAPHICS,
            .vram_address = Y_VIDEO_CGA_VRAM_ADDRESS,
            .vram_size = Y_VIDEO_CGA_VRAM_SIZE,
            .width = 640,
            .height = 200,
            .num_pages = 1,
        },
        // 0x07: MDA text, 80x25, monochrome
        {
            .mode = Y_VIDEO_MODE_MDA_TEXT_80X25,
            .type = Y_VIDEO_MODE_TEXT,
            .vram_address = Y_VIDEO_MDA_VRAM_ADDRESS,
            .vram_size = Y_VIDEO_MDA_VRAM_SIZE,
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
typedef struct YVideoAdapterMetadata {
  // The adapter.
  YVideoAdapter adapter;

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
} YVideoAdapterMetadata;

// Metadata for each video adapter, indexed by YVideoAdapter.
//
// The timing values are derived from the same 14.318MHz master crystal the CPU
// clock comes from, so they are exact in CPU cycles. The MDA scans 882 dots per
// line at 16.257MHz over 370 lines, which is 259 cycles per line and just under
// 50Hz. The CGA scans 912 dots per line at 14.318MHz over 262 lines, which is
// 304 cycles per line and just under 60Hz.
Y_PUBLIC_HEADER const YVideoAdapterMetadata
    YVideoAdapterMetadataTable[Y_VIDEO_NUM_ADAPTERS] = {
        // MDA
        {
            .adapter = Y_VIDEO_ADAPTER_MDA,
            .frame_buffer_width = 720,
            .frame_buffer_height = 350,
            .vram_address = Y_VIDEO_MDA_VRAM_ADDRESS,
            .vram_size = Y_VIDEO_MDA_VRAM_SIZE,
            .port_start = Y_VIDEO_MDA_PORT_START,
            .port_end = Y_VIDEO_MDA_PORT_END,
            // High resolution mode, video enable, blink enable.
            .default_control_register = 0x29,
            .cycles_per_scan_line = 259,
            .display_cycles_per_scan_line = 211,
            .scan_lines_per_frame = 370,
            .displayed_scan_lines = 350,
        },
        // CGA
        {
            .adapter = Y_VIDEO_ADAPTER_CGA,
            .frame_buffer_width = 640,
            .frame_buffer_height = 200,
            .vram_address = Y_VIDEO_CGA_VRAM_ADDRESS,
            .vram_size = Y_VIDEO_CGA_VRAM_SIZE,
            .port_start = Y_VIDEO_CGA_PORT_START,
            .port_end = Y_VIDEO_CGA_PORT_END,
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
  Y_VIDEO_FRAMES_PER_CURSOR_BLINK_PHASE = 8,
  // Number of frames between character blink phase changes. Characters blink
  // half as fast as the cursor: the 6845 generates the cursor blink itself,
  // while the blink attribute is decoded by the adapter from a separate
  // divider running at a thirty-secondth of the field rate.
  Y_VIDEO_FRAMES_PER_TEXT_BLINK_PHASE = 16,
};

enum {
  // Every supported mode divides horizontally into 80 natural rendering
  // units: text columns on the MDA and 80-column CGA, half-character cells in
  // 40-column CGA text modes, and VRAM bytes in CGA graphics modes.
  Y_VIDEO_DIRTY_COLUMNS = 80,
  // Vertically, one horizontal range is tracked per dirty row. A text mode's
  // dirty row is one character row, which covers exactly the scan lines a
  // changed cell occupies. Graphics modes have no character rows, so
  // neighboring scan lines share a range instead.
  Y_VIDEO_DIRTY_SCAN_LINES_PER_GROUP = 4,
  // The tallest graphics mode is 200 lines.
  Y_VIDEO_MAX_GRAPHICS_HEIGHT = 200,
  // The most character rows in any text mode.
  Y_VIDEO_MAX_TEXT_ROWS = 25,
  // Dirty rows needed to cover the tallest graphics mode.
  Y_VIDEO_DIRTY_GRAPHICS_ROW_COUNT = (
      Y_VIDEO_MAX_GRAPHICS_HEIGHT + Y_VIDEO_DIRTY_SCAN_LINES_PER_GROUP - 1) /
      Y_VIDEO_DIRTY_SCAN_LINES_PER_GROUP,
  // The tracker is statically sized for whichever mode needs the most rows.
  // Graphics modes do, at 50 against a text mode's 25.
  Y_VIDEO_DIRTY_ROW_COUNT =
              Y_VIDEO_DIRTY_GRAPHICS_ROW_COUNT > Y_VIDEO_MAX_TEXT_ROWS
      ? Y_VIDEO_DIRTY_GRAPHICS_ROW_COUNT
      : Y_VIDEO_MAX_TEXT_ROWS,
  // Pixels are transferred to the host in small horizontal spans. This removes
  // most callback overhead without requiring a frame or scan-line buffer.
  Y_VIDEO_PIXEL_BATCH_SIZE = 32,
};

// Half-open horizontal dirty range [start_column, end_column). Both fields are
// zero when the range is empty.
typedef struct YVideoDirtyRange {
  uint8_t start_column;
  uint8_t end_column;
} YVideoDirtyRange;

// What the next render has to do. The three states are mutually exclusive, so
// they are one value rather than a pair of flags that could disagree.
typedef enum YVideoDirtyStatus {
  // The host display already matches video state, so a render does nothing.
  // This is the zero value, which makes clearing the tracker a zero
  // initialization.
  Y_VIDEO_CLEAN = 0,
  // Some rows carry a dirty range, and only those need to be redrawn.
  Y_VIDEO_DIRTY,
  // The next render must replace the complete frame buffer. Used when a change
  // cannot be represented as local VRAM damage, such as a mode or palette
  // change.
  Y_VIDEO_FULL_REDRAW,
} YVideoDirtyStatus;

typedef struct YVideoDirtyState {
  // One horizontal range per dirty row, indexed by dirty row. Only meaningful
  // when status is Y_VIDEO_DIRTY.
  YVideoDirtyRange ranges[Y_VIDEO_DIRTY_ROW_COUNT];
  // What the next render has to do.
  YVideoDirtyStatus status;
} YVideoDirtyState;

// ============================================================================
// Video state
// ============================================================================

struct YVideoState;

// Caller-provided configuration for video rendering.
typedef struct YVideoConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger for this module. May be NULL.
  YLogger* logger;

  // The video adapter to emulate.
  YVideoAdapter adapter;

  // MDA - foreground color.
  YVideoRGB foreground;
  // MDA - intense foreground color.
  YVideoRGB intense_foreground;
  // MDA - background color.
  YVideoRGB background;

  // CGA - the 16 color RGBI palette.
  YVideoRGB cga_palette[Y_VIDEO_NUM_CGA_COLORS];

  // The emulated video RAM, at least the adapter's vram_size bytes of it.
  // Required - the adapter reads and writes it directly.
  //
  // Rendering is the heaviest reader of video RAM, so a callback per byte
  // would cost an indirect call in its inner loops. Writes still go through
  // YVideoWriteVRAM(), which marks the corresponding display region dirty.
  //
  // The caller owns the buffer and it must outlive the video state.
  uint8_t* vram;

  // Callback to write a horizontal span of RGB pixels to the real display,
  // invoked from YVideoRender(). The pixels begin at origin, remain on one scan
  // line, and are valid only for the duration of the synchronous callback.
  // count is between 1 and Y_VIDEO_PIXEL_BATCH_SIZE, inclusive.
  void (*write_pixels)(
      struct YVideoState* video, YVideoPosition origin, const YVideoRGB* pixels,
      uint8_t count);
  // Optional callbacks surrounding each dirty rectangular region emitted by
  // YVideoRender(). A retained display can use them to set a transfer window
  // before the row-major pixel stream begins.
  void (*begin_render_region)(struct YVideoState* video, YVideoRegion region);
  void (*end_render_region)(struct YVideoState* video);
} YVideoConfig;

// Default video config.
Y_PUBLIC_HEADER const YVideoConfig YVideoDefaultConfig = {
    .context = NULL,

    .adapter = Y_VIDEO_ADAPTER_MDA,

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
    .write_pixels = NULL,
    .begin_render_region = NULL,
    .end_render_region = NULL,
};

// Video state.
typedef struct YVideoState {
  // Caller-provided runtime configuration.
  YVideoConfig config;

  // The video adapter being emulated, copied from the config at init time.
  YVideoAdapter adapter;

  // Motorola 6845 CRT controller registers.
  uint8_t registers[Y_VIDEO_NUM_CRTC_REGISTERS];
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
  YVideoDirtyState dirty_state;

  // Pixels emitted so far for the region currently being rendered. A windowed
  // display places pixels by count rather than by coordinate, so a region that
  // emits a different number of pixels than it declared would corrupt it; this
  // is what the check at the end of each region compares.
  uint32_t num_pixels_emitted_for_region;
} YVideoState;

// Initialize video state with the provided configuration.
Y_PUBLIC void YVideoInit(YVideoState* video);

// Metadata for the adapter being emulated.
Y_PUBLIC const YVideoAdapterMetadata* YVideoGetAdapterMetadata(
    const YVideoState* video);

// The current video mode, derived from the mode control register.
Y_PUBLIC YVideoMode YVideoGetMode(const YVideoState* video);

// Metadata for the current video mode.
Y_PUBLIC const YVideoModeMetadata* YVideoGetModeMetadata(
    const YVideoState* video);

// Read a byte from a video I/O port.
Y_PUBLIC uint8_t YVideoReadPort(YVideoState* video, uint16_t port);
// Write a byte to a video I/O port.
Y_PUBLIC void YVideoWritePort(YVideoState* video, uint16_t port, uint8_t value);

// Read a byte from video RAM.
Y_PUBLIC uint8_t YVideoReadVRAM(YVideoState* video, uint32_t address);
// Write a byte to video RAM.
Y_PUBLIC void YVideoWriteVRAM(
    YVideoState* video, uint32_t address, uint8_t value);

// Advance the CRT beam by the given number of CPU cycles. This drives the
// retrace bits in the status register and the blink phase.
//
// The cost does not depend on how many cycles are passed, so a caller that
// advances the beam only when something is about to look at it may pass a
// whole frame's worth at once.
Y_PUBLIC void YVideoTick(YVideoState* video, uint32_t cycles);

// Bring dirty portions of the retained host display up to date. Each region is
// bracketed by the optional region callbacks and its pixels are passed to
// write_pixels in row-major horizontal spans. Does nothing when the display is
// unchanged.
Y_PUBLIC void YVideoRender(YVideoState* video);

#endif  // Y_VIDEO_PUBLIC_H
