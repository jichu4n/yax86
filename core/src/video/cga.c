#ifndef YAX86_IMPLEMENTATION
#include "fonts.h"
#include "internal.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

enum {
  Y_VIDEO_CGA40_COLUMN_COUNT = 40,
  Y_VIDEO_CGA40_COLUMN_SCALE = 2,
  Y_VIDEO_CGA_320X200_HORIZONTAL_SCALE = 2,
  // Number of pixels per byte in 320x200 graphics mode.
  Y_VIDEO_CGA_PIXELS_PER_BYTE_320X200 = 4,
  // Number of bits per pixel in 320x200 graphics mode.
  Y_VIDEO_CGA_BITS_PER_PIXEL_320X200 = 2,
  // Mask of a single pixel in 320x200 graphics mode.
  Y_VIDEO_CGA_PIXEL_MASK_320X200 = 0x03,
  // Number of frame buffer pixels per byte in 320x200 graphics mode, after
  // each pixel is doubled horizontally.
  Y_VIDEO_CGA_FRAME_BUFFER_PIXELS_PER_BYTE_320X200 =
          Y_VIDEO_CGA_PIXELS_PER_BYTE_320X200 *
      Y_VIDEO_CGA_320X200_HORIZONTAL_SCALE,
  // Number of pixels per byte in 640x200 graphics mode.
  Y_VIDEO_CGA_PIXELS_PER_BYTE_640X200 = 8,
  // Mask of a graphics mode address within the half of VRAM holding either the
  // even or the odd scan lines.
  Y_VIDEO_CGA_GRAPHICS_SCAN_LINE_ADDRESS_MASK =
          Y_VIDEO_CGA_GRAPHICS_ODD_SCAN_LINE_OFFSET -
      1,
};

// The three 320x200 graphics palettes, each holding colors 1 to 3. Color 0
// comes from the color select register instead. The palette-select bit chooses
// the first two, unless black-and-white mode selects the third.
Y_FILE_PRIVATE const uint8_t YVideoCGAGraphicsPalettes[3][3] = {
    // Palette 0: green, red, brown.
    {2, 4, 6},
    // Palette 1: cyan, magenta, light gray.
    {3, 5, 7},
    // Black and white: cyan, red, light gray.
    {3, 4, 7},
};

Y_FILE_PRIVATE inline YVideoRGB YVideoCGAGetColor(
    const YVideoState* video, uint8_t color) {
  return video->config.cga_palette[color & (Y_VIDEO_NUM_CGA_COLORS - 1)];
}

// Address of the first byte of a graphics mode scan line. Even and odd scan
// lines occupy separate halves of CGA VRAM.
Y_FILE_PRIVATE inline uint32_t YVideoCGAGetScanLineAddress(
    const YVideoState* video, uint16_t y) {
  uint32_t address =
      ((uint32_t)YVideoGetStartAddress(video) * 2 +
       (uint32_t)(y / 2) * Y_VIDEO_CGA_GRAPHICS_BYTES_PER_SCAN_LINE) &
      Y_VIDEO_CGA_GRAPHICS_SCAN_LINE_ADDRESS_MASK;
  return address + (y & 1 ? Y_VIDEO_CGA_GRAPHICS_ODD_SCAN_LINE_OFFSET : 0);
}

Y_FILE_PRIVATE void YVideoCGARenderTextRegion(
    YVideoState* video, YVideoPixelRun* run, const YVideoModeMetadata* metadata,
    uint8_t start_column, uint8_t end_column, uint16_t first_y,
    uint16_t end_y) {
  // A 40-column cell is drawn twice as wide, so it spans two dirty columns
  // and the range converts back to character cells by halving. Invalidation
  // always scales a character column by the same factor, so the range is
  // aligned to whole cells and neither end truncates.
  uint8_t scale = metadata->columns == Y_VIDEO_CGA40_COLUMN_COUNT
                      ? Y_VIDEO_CGA40_COLUMN_SCALE
                      : 1;
  uint8_t first_char = scale == Y_VIDEO_CGA40_COLUMN_SCALE
                           ? start_column / Y_VIDEO_CGA40_COLUMN_SCALE
                           : start_column;
  uint8_t end_char = scale == Y_VIDEO_CGA40_COLUMN_SCALE
                         ? end_column / Y_VIDEO_CGA40_COLUMN_SCALE
                         : end_column;

  // The cursor is resolved once for the region rather than per cell: it can
  // only be in one place, so the loop just tests each cell against it.
  uint16_t start_address = YVideoGetStartAddress(video);
  uint16_t cursor_offset = 0;
  bool cursor_visible = YVideoGetVisibleCursorOffset(
      video, metadata, start_address, &cursor_offset);
  uint16_t cursor_start = YVideoScaleTextScanLineStart(
      video, metadata, YVideoGetCursorStartScanLine(video));
  uint16_t cursor_end = YVideoScaleTextScanLineEnd(
      video, metadata, YVideoGetCursorEndScanLine(video));

  // The adapter's own 8x8 cell, or a host text font's.
  const uint8_t cell_width = YVideoGetTextCellWidth(video, metadata);
  const uint8_t cell_height = YVideoGetTextCellHeight(video, metadata);

  // Scan lines are the outer loop so that pixels leave in row-major order,
  // which is what a display addressed by transfer window expects. The cost is
  // re-reading a cell's character and attribute once per scan line it covers.
  for (uint16_t y = first_y; y < end_y; ++y) {
    // A 40-column cell is twice as wide but there are half as many, so a row
    // starts at the same pixel either way.
    YVideoPixelRunBegin(run, (uint16_t)start_column * cell_width, y);
    uint8_t row = (uint8_t)(y / cell_height);
    uint8_t char_scan_line = (uint8_t)(y % cell_height);
    for (uint8_t col = first_char; col < end_char; ++col) {
      // Character and attribute occupy consecutive bytes of a cell.
      uint16_t cell_offset = (uint16_t)row * metadata->columns + col;
      uint32_t char_address = ((uint32_t)start_address + cell_offset) * 2;
      uint8_t char_value = YVideoReadVRAMByte(video, char_address);
      uint8_t attr_value = YVideoReadVRAMByte(video, char_address + 1);

      uint8_t foreground_color =
          attr_value & (Y_VIDEO_ATTRIBUTE_FOREGROUND_MASK |
                        Y_VIDEO_ATTRIBUTE_INTENSE_FOREGROUND);
      // The cursor uses the cell's foreground even when that cell's blinking
      // character is currently hidden.
      YVideoRGB cursor_foreground = YVideoCGAGetColor(video, foreground_color);
      uint8_t background_color =
          (attr_value & Y_VIDEO_ATTRIBUTE_BACKGROUND_MASK) >>
          Y_VIDEO_ATTRIBUTE_BACKGROUND_SHIFT;
      // Bit 7 is the blink attribute only while blinking is enabled. With it
      // disabled the same bit is the background's intensity, which is how the
      // CGA reaches all 16 background colors.
      if (video->control_register & Y_VIDEO_CONTROL_ENABLE_BLINK) {
        if ((attr_value & Y_VIDEO_ATTRIBUTE_BLINK) &&
            !YVideoIsTextBlinkOn(video)) {
          foreground_color = background_color;
        }
      } else if (attr_value & Y_VIDEO_ATTRIBUTE_BLINK) {
        background_color |= Y_VIDEO_NUM_CGA_COLORS / 2;
      }

      YVideoRGB foreground = YVideoCGAGetColor(video, foreground_color);
      YVideoRGB background = YVideoCGAGetColor(video, background_color);
      bool cursor_scan_line = cursor_visible && cursor_offset == cell_offset &&
                              char_scan_line >= cursor_start &&
                              char_scan_line <= cursor_end;
      // The glyph row is a bitmap with the leftmost pixel in the high bit.
      uint8_t row_bitmap =
          video->has_text_font
              ? YVideoGetTextFontRow(video, char_value, char_scan_line)
              : YVideoFontCGA8x8Bitmap[char_value][char_scan_line];
      for (uint8_t x = 0; x < cell_width; ++x) {
        bool is_foreground = (row_bitmap & (1 << (cell_width - 1 - x))) != 0;
        YVideoRGB rgb = cursor_scan_line
                            ? cursor_foreground
                            : (is_foreground ? foreground : background);
        // In 40-column mode each glyph pixel is emitted twice, which is how
        // the adapter fills the same 640 dot line with half the characters.
        for (uint8_t i = 0; i < scale; ++i) {
          YVideoPixelRunPush(run, rgb);
        }
      }
    }
  }
}

Y_FILE_PRIVATE void YVideoCGAResolve320x200Palette(
    YVideoState* video, YVideoRGB palette[4]) {
  const uint8_t* palette_colors;
  if (video->control_register & Y_VIDEO_CONTROL_BLACK_AND_WHITE) {
    palette_colors = YVideoCGAGraphicsPalettes[2];
  } else if (video->color_select_register & Y_VIDEO_CGA_COLOR_SELECT_PALETTE) {
    palette_colors = YVideoCGAGraphicsPalettes[1];
  } else {
    palette_colors = YVideoCGAGraphicsPalettes[0];
  }
  uint8_t intensity = (video->color_select_register &
                       Y_VIDEO_CGA_COLOR_SELECT_PALETTE_INTENSITY)
                          ? Y_VIDEO_NUM_CGA_COLORS / 2
                          : 0;
  palette[0] = YVideoCGAGetColor(
      video,
      video->color_select_register & (Y_VIDEO_CGA_COLOR_SELECT_COLOR_MASK |
                                      Y_VIDEO_CGA_COLOR_SELECT_INTENSITY));
  for (uint8_t i = 0; i < 3; ++i) {
    palette[i + 1] = YVideoCGAGetColor(video, palette_colors[i] | intensity);
  }
}

Y_FILE_PRIVATE void YVideoCGARenderGraphics320x200Region(
    YVideoState* video, YVideoPixelRun* run, uint8_t start_column,
    uint8_t end_column, uint16_t first_y, uint16_t end_y) {
  YVideoRGB palette[4];
  YVideoCGAResolve320x200Palette(video, palette);

  for (uint16_t y = first_y; y < end_y; ++y) {
    YVideoPixelRunBegin(
        run,
        (uint16_t)start_column *
            Y_VIDEO_CGA_FRAME_BUFFER_PIXELS_PER_BYTE_320X200,
        y);
    uint32_t scan_line_address = YVideoCGAGetScanLineAddress(video, y);
    for (uint8_t byte_column = start_column; byte_column < end_column;
         ++byte_column) {
      uint8_t byte_value =
          YVideoReadVRAMByte(video, scan_line_address + byte_column);
      for (uint8_t i = 0; i < Y_VIDEO_CGA_PIXELS_PER_BYTE_320X200; ++i) {
        uint8_t shift =
            (uint8_t)((Y_VIDEO_CGA_PIXELS_PER_BYTE_320X200 - 1 - i) *
                      Y_VIDEO_CGA_BITS_PER_PIXEL_320X200);
        uint8_t color = (byte_value >> shift) & Y_VIDEO_CGA_PIXEL_MASK_320X200;
        for (uint8_t scale_x = 0;
             scale_x < Y_VIDEO_CGA_320X200_HORIZONTAL_SCALE; ++scale_x) {
          YVideoPixelRunPush(run, palette[color]);
        }
      }
    }
  }
}

Y_FILE_PRIVATE void YVideoCGARenderGraphics640x200Region(
    YVideoState* video, YVideoPixelRun* run, uint8_t start_column,
    uint8_t end_column, uint16_t first_y, uint16_t end_y) {
  YVideoRGB foreground = YVideoCGAGetColor(
      video,
      video->color_select_register & (Y_VIDEO_CGA_COLOR_SELECT_COLOR_MASK |
                                      Y_VIDEO_CGA_COLOR_SELECT_INTENSITY));
  YVideoRGB background = YVideoCGAGetColor(video, 0);

  for (uint16_t y = first_y; y < end_y; ++y) {
    YVideoPixelRunBegin(
        run, (uint16_t)start_column * Y_VIDEO_CGA_PIXELS_PER_BYTE_640X200, y);
    uint32_t scan_line_address = YVideoCGAGetScanLineAddress(video, y);
    for (uint8_t byte_column = start_column; byte_column < end_column;
         ++byte_column) {
      uint8_t byte_value =
          YVideoReadVRAMByte(video, scan_line_address + byte_column);
      for (uint8_t i = 0; i < Y_VIDEO_CGA_PIXELS_PER_BYTE_640X200; ++i) {
        uint8_t shift = (uint8_t)(Y_VIDEO_CGA_PIXELS_PER_BYTE_640X200 - 1 - i);
        bool is_foreground = (byte_value >> shift) & 1;
        YVideoPixelRunPush(run, is_foreground ? foreground : background);
      }
    }
  }
}

Y_MODULE_PRIVATE void YVideoCGARenderRegion(
    YVideoState* video, YVideoPixelRun* run, uint8_t start_column,
    uint8_t end_column, uint16_t first_y, uint16_t end_y) {
  const YVideoModeMetadata* metadata = YVideoGetModeMetadata(video);
  switch (metadata->mode) {
    case Y_VIDEO_MODE_CGA_TEXT_40X25_MONO:
    case Y_VIDEO_MODE_CGA_TEXT_40X25_COLOR:
    case Y_VIDEO_MODE_CGA_TEXT_80X25_MONO:
    case Y_VIDEO_MODE_CGA_TEXT_80X25_COLOR:
      YVideoCGARenderTextRegion(
          video, run, metadata, start_column, end_column, first_y, end_y);
      break;
    case Y_VIDEO_MODE_CGA_GRAPHICS_320X200:
    case Y_VIDEO_MODE_CGA_GRAPHICS_320X200_ALT:
      YVideoCGARenderGraphics320x200Region(
          video, run, start_column, end_column, first_y, end_y);
      break;
    case Y_VIDEO_MODE_CGA_GRAPHICS_640X200:
      YVideoCGARenderGraphics640x200Region(
          video, run, start_column, end_column, first_y, end_y);
      break;
    default:
      break;
  }
}
