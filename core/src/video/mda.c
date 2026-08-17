#ifndef YAX86_IMPLEMENTATION
#include "fonts.h"
#include "internal.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

enum {
  kMDACharHeight = 14,
  // Position of the underline within an MDA character cell.
  kMDAUnderlinePosition = 12,
  // Documented three-bit foreground and background combinations.
  kMDAAttributeNormal = 0x07,
  kMDAAttributeInverse = 0x00,
  kMDAAttributeUnderline = 0x01,
};

typedef struct MDACellColors {
  const RGB* foreground;
  const RGB* background;
  bool underline;
} MDACellColors;

// Decode the documented normal, inverse, invisible, underline, intensity and
// blink combinations. Undefined combinations are rendered as normal text.
static MDACellColors MDADecodeAttribute(VideoState* video, uint8_t attr_value) {
  const VideoConfig* config = video->config;
  MDACellColors colors = {
      .foreground = &config->foreground,
      .background = &config->background,
      .underline = false,
  };

  bool intense = (attr_value & kVideoAttributeIntenseForeground) != 0;
  const RGB* intense_aware_foreground =
      intense ? &config->intense_foreground : &config->foreground;
  uint8_t background_attr = (attr_value & kVideoAttributeBackgroundMask) >>
                            kVideoAttributeBackgroundShift;
  uint8_t foreground_attr = attr_value & kVideoAttributeForegroundMask;

  if (background_attr == kMDAAttributeInverse &&
      foreground_attr == kMDAAttributeNormal) {
    // Normal video mode.
    colors.foreground = intense_aware_foreground;
  } else if (
      background_attr == kMDAAttributeNormal &&
      foreground_attr == kMDAAttributeInverse) {
    // Inverse video mode.
    colors.foreground = &config->background;
    colors.background = &config->foreground;
  } else if (
      background_attr == kMDAAttributeInverse &&
      foreground_attr == kMDAAttributeInverse) {
    // Invisible mode.
    colors.foreground = &config->background;
  } else if (
      background_attr == kMDAAttributeInverse &&
      foreground_attr == kMDAAttributeUnderline) {
    // Underline mode.
    colors.underline = true;
    colors.foreground = intense_aware_foreground;
  } else {
    // Other combinations are treated as normal.
    colors.foreground = intense_aware_foreground;
  }

  // Blinking characters alternate between the character and blank. The blink
  // attribute only applies if blinking is enabled in the mode control register.
  if ((attr_value & kVideoAttributeBlink) &&
      (video->control_register & kVideoControlEnableBlink) &&
      !VideoIsTextBlinkOn(video)) {
    colors.foreground = colors.background;
    colors.underline = false;
  }
  return colors;
}

// Render a rectangular slice directly from text VRAM. Iterating scan lines
// first makes write_pixel a row-major stream suitable for an SPI window.
YAX86_PRIVATE void MDARenderRegion(
    VideoState* video, uint8_t start_column, uint8_t end_column,
    uint16_t first_y, uint16_t end_y) {
  // The MDA has exactly one mode, so its metadata is looked up directly
  // rather than derived from the mode control register.
  const VideoModeMetadata* metadata =
      &kVideoModeMetadata[kVideoModeMDAText80x25];

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
    uint8_t row = (uint8_t)(y / kMDACharHeight);
    uint8_t char_scan_line = (uint8_t)(y % kMDACharHeight);
    for (uint8_t col = start_column; col < end_column; ++col) {
      // Character and attribute occupy consecutive bytes of a cell. The 6845
      // start address can push a cell past the end of VRAM, which wraps.
      uint16_t cell_offset = (uint16_t)row * metadata->columns + col;
      uint32_t char_address = ((uint32_t)start_address + cell_offset) * 2;
      char_address &= metadata->vram_size - 1;
      uint8_t char_value = VideoReadVRAMByte(video, char_address);
      uint8_t attr_value = VideoReadVRAMByte(video, char_address + 1);
      MDACellColors colors = MDADecodeAttribute(video, attr_value);

      // The underline is a solid rule across the cell, so it replaces the
      // glyph row outright on the one scan line it occupies. The glyph row is
      // a bitmap with the leftmost pixel in the high bit, and is 16 bits wide
      // because an MDA cell is 9 pixels across.
      uint16_t row_bitmap =
          char_scan_line == kMDAUnderlinePosition && colors.underline
              ? 0xFFFF
              : kFontMDA9x14Bitmap[char_value][char_scan_line];
      bool cursor_scan_line = cursor_visible && cursor_offset == cell_offset &&
                              char_scan_line >= cursor_start &&
                              char_scan_line <= cursor_end;
      for (uint8_t x = 0; x < metadata->char_width; ++x) {
        bool is_foreground =
            (row_bitmap & (1 << (metadata->char_width - 1 - x))) != 0;
        // The cursor overrides the cell entirely, including a blinking
        // character that is currently hidden.
        const RGB* rgb = cursor_scan_line ? &video->config->foreground
                                          : (is_foreground ? colors.foreground
                                                           : colors.background);
        Position position = {
            .x = (uint16_t)col * metadata->char_width + x,
            .y = y,
        };
        VideoWritePixel(video, position, *rgb);
      }
    }
  }
}
