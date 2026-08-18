#ifndef YAX86_IMPLEMENTATION
#include "fonts.h"
#include "internal.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

enum {
  kCGACharHeight = 8,
  kCGA40ColumnCount = 40,
  kCGA40ColumnScale = 2,
  kCGA320x200HorizontalScale = 2,
  // Number of pixels per byte in 320x200 graphics mode.
  kCGAPixelsPerByte320x200 = 4,
  // Number of bits per pixel in 320x200 graphics mode.
  kCGABitsPerPixel320x200 = 2,
  // Mask of a single pixel in 320x200 graphics mode.
  kCGAPixelMask320x200 = 0x03,
  // Number of frame buffer pixels per byte in 320x200 graphics mode, after
  // each pixel is doubled horizontally.
  kCGAFrameBufferPixelsPerByte320x200 =
      kCGAPixelsPerByte320x200 * kCGA320x200HorizontalScale,
  // Number of pixels per byte in 640x200 graphics mode.
  kCGAPixelsPerByte640x200 = 8,
  // Mask of a graphics mode address within the half of VRAM holding either the
  // even or the odd scan lines.
  kCGAGraphicsScanLineAddressMask = kCGAGraphicsOddScanLineOffset - 1,
};

// The three 320x200 graphics palettes, each holding colors 1 to 3. Color 0
// comes from the color select register instead. The palette-select bit chooses
// the first two, unless black-and-white mode selects the third.
static const uint8_t kCGAGraphicsPalettes[3][3] = {
    // Palette 0: green, red, brown.
    {2, 4, 6},
    // Palette 1: cyan, magenta, light gray.
    {3, 5, 7},
    // Black and white: cyan, red, light gray.
    {3, 4, 7},
};

static inline RGB CGAGetColor(const VideoState* video, uint8_t color) {
  return video->config->cga_palette[color & (kNumCGAColors - 1)];
}

// Address of the first byte of a graphics mode scan line. Even and odd scan
// lines occupy separate halves of CGA VRAM.
static inline uint32_t CGAGetScanLineAddress(
    const VideoState* video, uint16_t y) {
  uint32_t address = ((uint32_t)VideoGetStartAddress(video) * 2 +
                      (uint32_t)(y / 2) * kCGAGraphicsBytesPerScanLine) &
                     kCGAGraphicsScanLineAddressMask;
  return address + (y & 1 ? kCGAGraphicsOddScanLineOffset : 0);
}

static void CGARenderTextRegion(
    VideoState* video, const VideoModeMetadata* metadata, uint8_t start_column,
    uint8_t end_column, uint16_t first_y, uint16_t end_y) {
  // A 40-column cell is drawn twice as wide, so it spans two dirty columns
  // and the range converts back to character cells by halving. Invalidation
  // always scales a character column by the same factor, so the range is
  // aligned to whole cells and neither end truncates.
  uint8_t scale =
      metadata->columns == kCGA40ColumnCount ? kCGA40ColumnScale : 1;
  uint8_t first_char = scale == kCGA40ColumnScale
                           ? start_column / kCGA40ColumnScale
                           : start_column;
  uint8_t end_char =
      scale == kCGA40ColumnScale ? end_column / kCGA40ColumnScale : end_column;

  // The cursor is resolved once for the region rather than per cell: it can
  // only be in one place, so the loop just tests each cell against it.
  uint16_t start_address = VideoGetStartAddress(video);
  uint16_t cursor_offset = 0;
  bool cursor_visible = VideoGetVisibleCursorOffset(
      video, metadata, start_address, &cursor_offset);
  uint8_t cursor_start = VideoGetCursorStartScanLine(video);
  uint8_t cursor_end = VideoGetCursorEndScanLine(video);

  // Scan lines are the outer loop so that pixels leave in row-major order,
  // which is what a display addressed by transfer window expects. The cost is
  // re-reading a cell's character and attribute once per scan line it covers.
  for (uint16_t y = first_y; y < end_y; ++y) {
    uint8_t row = (uint8_t)(y / kCGACharHeight);
    uint8_t char_scan_line = (uint8_t)(y % kCGACharHeight);
    for (uint8_t col = first_char; col < end_char; ++col) {
      // Character and attribute occupy consecutive bytes of a cell.
      uint16_t cell_offset = (uint16_t)row * metadata->columns + col;
      uint32_t char_address = ((uint32_t)start_address + cell_offset) * 2;
      uint8_t char_value = VideoReadVRAMByte(video, char_address);
      uint8_t attr_value = VideoReadVRAMByte(video, char_address + 1);

      uint8_t foreground_color =
          attr_value &
          (kVideoAttributeForegroundMask | kVideoAttributeIntenseForeground);
      // The cursor uses the cell's foreground even when that cell's blinking
      // character is currently hidden.
      RGB cursor_foreground = CGAGetColor(video, foreground_color);
      uint8_t background_color = (attr_value & kVideoAttributeBackgroundMask) >>
                                 kVideoAttributeBackgroundShift;
      // Bit 7 is the blink attribute only while blinking is enabled. With it
      // disabled the same bit is the background's intensity, which is how the
      // CGA reaches all 16 background colors.
      if (video->control_register & kVideoControlEnableBlink) {
        if ((attr_value & kVideoAttributeBlink) && !VideoIsTextBlinkOn(video)) {
          foreground_color = background_color;
        }
      } else if (attr_value & kVideoAttributeBlink) {
        background_color |= kNumCGAColors / 2;
      }

      RGB foreground = CGAGetColor(video, foreground_color);
      RGB background = CGAGetColor(video, background_color);
      bool cursor_scan_line = cursor_visible && cursor_offset == cell_offset &&
                              char_scan_line >= cursor_start &&
                              char_scan_line <= cursor_end;
      // The glyph row is a bitmap with the leftmost pixel in the high bit.
      uint8_t row_bitmap = kFontCGA8x8Bitmap[char_value][char_scan_line];
      for (uint8_t x = 0; x < metadata->char_width; ++x) {
        bool is_foreground =
            (row_bitmap & (1 << (metadata->char_width - 1 - x))) != 0;
        RGB rgb = cursor_scan_line ? cursor_foreground
                                   : (is_foreground ? foreground : background);
        // In 40-column mode each glyph pixel is emitted twice, which is how
        // the adapter fills the same 640 dot line with half the characters.
        uint16_t pixel_x = ((uint16_t)col * metadata->char_width + x) * scale;
        for (uint8_t i = 0; i < scale; ++i) {
          Position position = {.x = pixel_x + i, .y = y};
          VideoWritePixel(video, position, rgb);
        }
      }
    }
  }
}

static void CGAResolve320x200Palette(VideoState* video, RGB palette[4]) {
  const uint8_t* palette_colors;
  if (video->control_register & kVideoControlBlackAndWhite) {
    palette_colors = kCGAGraphicsPalettes[2];
  } else if (video->color_select_register & kCGAColorSelectPalette) {
    palette_colors = kCGAGraphicsPalettes[1];
  } else {
    palette_colors = kCGAGraphicsPalettes[0];
  }
  uint8_t intensity =
      (video->color_select_register & kCGAColorSelectPaletteIntensity)
          ? kNumCGAColors / 2
          : 0;
  palette[0] = CGAGetColor(
      video, video->color_select_register &
                 (kCGAColorSelectColorMask | kCGAColorSelectIntensity));
  for (uint8_t i = 0; i < 3; ++i) {
    palette[i + 1] = CGAGetColor(video, palette_colors[i] | intensity);
  }
}

static void CGARenderGraphics320x200Region(
    VideoState* video, uint8_t start_column, uint8_t end_column,
    uint16_t first_y, uint16_t end_y) {
  RGB palette[4];
  CGAResolve320x200Palette(video, palette);

  for (uint16_t y = first_y; y < end_y; ++y) {
    uint32_t scan_line_address = CGAGetScanLineAddress(video, y);
    for (uint8_t byte_column = start_column; byte_column < end_column;
         ++byte_column) {
      uint8_t byte_value =
          VideoReadVRAMByte(video, scan_line_address + byte_column);
      uint16_t first_x = byte_column * kCGAFrameBufferPixelsPerByte320x200;
      for (uint8_t i = 0; i < kCGAPixelsPerByte320x200; ++i) {
        uint8_t shift = (uint8_t)((kCGAPixelsPerByte320x200 - 1 - i) *
                                  kCGABitsPerPixel320x200);
        uint8_t color = (byte_value >> shift) & kCGAPixelMask320x200;
        for (uint8_t scale_x = 0; scale_x < kCGA320x200HorizontalScale;
             ++scale_x) {
          Position position = {
              .x = first_x + i * kCGA320x200HorizontalScale + scale_x,
              .y = y,
          };
          VideoWritePixel(video, position, palette[color]);
        }
      }
    }
  }
}

static void CGARenderGraphics640x200Region(
    VideoState* video, uint8_t start_column, uint8_t end_column,
    uint16_t first_y, uint16_t end_y) {
  RGB foreground = CGAGetColor(
      video, video->color_select_register &
                 (kCGAColorSelectColorMask | kCGAColorSelectIntensity));
  RGB background = CGAGetColor(video, 0);

  for (uint16_t y = first_y; y < end_y; ++y) {
    uint32_t scan_line_address = CGAGetScanLineAddress(video, y);
    for (uint8_t byte_column = start_column; byte_column < end_column;
         ++byte_column) {
      uint8_t byte_value =
          VideoReadVRAMByte(video, scan_line_address + byte_column);
      uint16_t first_x = byte_column * kCGAPixelsPerByte640x200;
      for (uint8_t i = 0; i < kCGAPixelsPerByte640x200; ++i) {
        uint8_t shift = (uint8_t)(kCGAPixelsPerByte640x200 - 1 - i);
        bool is_foreground = (byte_value >> shift) & 1;
        Position position = {.x = first_x + i, .y = y};
        VideoWritePixel(
            video, position, is_foreground ? foreground : background);
      }
    }
  }
}

YAX86_PRIVATE void CGARenderRegion(
    VideoState* video, uint8_t start_column, uint8_t end_column,
    uint16_t first_y, uint16_t end_y) {
  const VideoModeMetadata* metadata = VideoGetModeMetadata(video);
  switch (metadata->mode) {
    case kVideoModeCGAText40x25Mono:
    case kVideoModeCGAText40x25Color:
    case kVideoModeCGAText80x25Mono:
    case kVideoModeCGAText80x25Color:
      CGARenderTextRegion(
          video, metadata, start_column, end_column, first_y, end_y);
      break;
    case kVideoModeCGAGraphics320x200:
    case kVideoModeCGAGraphics320x200Alt:
      CGARenderGraphics320x200Region(
          video, start_column, end_column, first_y, end_y);
      break;
    case kVideoModeCGAGraphics640x200:
      CGARenderGraphics640x200Region(
          video, start_column, end_column, first_y, end_y);
      break;
    default:
      break;
  }
}
