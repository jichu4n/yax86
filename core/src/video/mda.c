#ifndef YAX86_IMPLEMENTATION
#include "fonts.h"
#include "internal.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

enum {
  // Position of the underline within an MDA character cell.
  kMDAUnderlinePosition = 12,
  // Attribute values are only meaningful in these three-bit fields, so the
  // documented combinations are compared against them directly.
  kMDAAttributeNormal = 0x07,
  kMDAAttributeInverse = 0x00,
  kMDAAttributeUnderline = 0x01,
};

// Colors to draw a character cell with.
typedef struct MDACellColors {
  const RGB* foreground;
  const RGB* background;
  bool underline;
} MDACellColors;

// Decode an MDA attribute byte. We only support the officially documented
// combinations of values.
//
// Attribute byte structure:
//   - Bit 7: blink (0 = normal, 1 = blink)
//   - Bits 6-4: background
//   - Bit 3: intense foreground (0 = normal, 1 = intense)
//   - Bits 2-0: foreground
//
// Valid MDA character background and foreground attribute combinations:
//   - Normal: background = 000, foreground = 111
//   - Inverse video: background = 111, foreground = 000
//   - Invisible: background = 000, foreground = 000
//   - Underline: background = 000, foreground = 001
//
// Other combinations are undefined, but we will treat them as normal.
// TODO: Support blinking.
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

  return colors;
}

// Write a character to display in MDA text mode. char_address is the address of
// the character's first byte in VRAM.
static void MDAWriteChar(
    VideoState* video, TextPosition char_pos, uint32_t char_address) {
  const VideoModeMetadata* metadata = &kMDAModeMetadata;
  uint8_t char_value = VideoReadVRAMByte(video, char_address);
  uint8_t attr_value = VideoReadVRAMByte(video, char_address + 1);
  const uint16_t* char_bitmap = kFontMDA9x14Bitmap[char_value];
  MDACellColors colors = MDADecodeAttribute(video, attr_value);

  Position origin_pixel_pos = {
      .x = char_pos.col * metadata->char_width,
      .y = char_pos.row * metadata->char_height,
  };
  for (uint8_t y = 0; y < metadata->char_height; ++y) {
    uint16_t row_bitmap;
    // If underline, set entire underline row to foreground color.
    if (y == kMDAUnderlinePosition && colors.underline) {
      row_bitmap = 0xFFFF;
    } else {
      row_bitmap = char_bitmap[y];
    }
    for (uint8_t x = 0; x < metadata->char_width; ++x) {
      Position pixel_pos = {
          .x = origin_pixel_pos.x + x,
          .y = origin_pixel_pos.y + y,
      };
      bool is_foreground =
          (row_bitmap & (1 << (metadata->char_width - 1 - x))) != 0;
      const RGB* pixel_rgb =
          is_foreground ? colors.foreground : colors.background;
      VideoWritePixel(video, pixel_pos, *pixel_rgb);
    }
  }
}

// Render the current display in MDA text mode.
YAX86_PRIVATE void MDARenderScreen(VideoState* video) {
  const VideoModeMetadata* metadata = &kMDAModeMetadata;
  for (uint8_t row = 0; row < metadata->rows; ++row) {
    for (uint8_t col = 0; col < metadata->columns; ++col) {
      TextPosition char_pos = {.col = col, .row = row};
      // Each character takes 2 bytes (char + attr).
      uint32_t char_address = ((uint32_t)row * metadata->columns + col) * 2;
      MDAWriteChar(video, char_pos, char_address);
    }
  }
}
