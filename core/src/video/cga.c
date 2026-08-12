#ifndef YAX86_IMPLEMENTATION
#include "fonts.h"
#include "internal.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

enum {
  // Number of pixels per byte in 320x200 graphics mode.
  kCGAPixelsPerByte320x200 = 4,
  // Number of bits per pixel in 320x200 graphics mode.
  kCGABitsPerPixel320x200 = 2,
  // Mask of a single pixel in 320x200 graphics mode.
  kCGAPixelMask320x200 = 0x03,
  // Number of pixels per byte in 640x200 graphics mode.
  kCGAPixelsPerByte640x200 = 8,
  // Mask of a graphics mode address within the half of VRAM holding either the
  // even or the odd scan lines.
  kCGAGraphicsScanLineAddressMask = kCGAGraphicsOddScanLineOffset - 1,
};

// Look up a color in the configured CGA palette.
static inline RGB CGAGetColor(const VideoState* video, uint8_t color) {
  return video->config->cga_palette[color & (kNumCGAColors - 1)];
}

// Write a pixel, expanding it horizontally to fill the frame buffer when the
// mode's horizontal resolution is lower than the frame buffer's. The CGA scans
// the same number of dots across the screen in every mode, so a 320 pixel wide
// mode is drawn with each pixel twice as wide.
static void CGAWritePixels(
    VideoState* video, uint16_t x, uint16_t y, uint8_t scale, RGB rgb) {
  for (uint8_t i = 0; i < scale; ++i) {
    Position pixel_pos = {.x = x + i, .y = y};
    VideoWritePixel(video, pixel_pos, rgb);
  }
}

// ============================================================================
// Text modes 0x00 - 0x03
// ============================================================================

// Write a character to display in a CGA text mode. char_address is the address
// of the character's first byte in VRAM, and scale is the horizontal pixel
// scaling factor for the mode.
static void CGAWriteChar(
    VideoState* video, const VideoModeMetadata* metadata, TextPosition char_pos,
    uint32_t char_address, uint8_t scale) {
  uint8_t char_value = VideoReadVRAMByte(video, char_address);
  uint8_t attr_value = VideoReadVRAMByte(video, char_address + 1);
  const uint8_t* char_bitmap = kFontCGA8x8Bitmap[char_value];

  uint8_t foreground_color = attr_value & (kVideoAttributeForegroundMask |
                                           kVideoAttributeIntenseForeground);
  uint8_t background_color = (attr_value & kVideoAttributeBackgroundMask) >>
                             kVideoAttributeBackgroundShift;
  bool blink_enabled =
      (video->control_register & kVideoControlEnableBlink) != 0;
  if (blink_enabled) {
    // Attribute bit 7 means blinking, so only eight background colors are
    // available.
    if ((attr_value & kVideoAttributeBlink) && !VideoIsTextBlinkOn(video)) {
      foreground_color = background_color;
    }
  } else {
    // Attribute bit 7 is the background intensity instead, giving all sixteen
    // background colors.
    if (attr_value & kVideoAttributeBlink) {
      background_color |= kNumCGAColors / 2;
    }
  }

  RGB foreground = CGAGetColor(video, foreground_color);
  RGB background = CGAGetColor(video, background_color);
  Position origin_pixel_pos = {
      .x = char_pos.col * metadata->char_width * scale,
      .y = char_pos.row * metadata->char_height,
  };
  for (uint8_t y = 0; y < metadata->char_height; ++y) {
    uint8_t row_bitmap = char_bitmap[y];
    for (uint8_t x = 0; x < metadata->char_width; ++x) {
      bool is_foreground =
          (row_bitmap & (1 << (metadata->char_width - 1 - x))) != 0;
      CGAWritePixels(
          video, origin_pixel_pos.x + x * scale, origin_pixel_pos.y + y, scale,
          is_foreground ? foreground : background);
    }
  }
}

// Draw the text mode cursor over the character cell it occupies.
static void CGADrawCursor(
    VideoState* video, const VideoModeMetadata* metadata,
    uint16_t start_address, uint8_t scale) {
  if (!VideoIsCursorEnabled(video) || !VideoIsCursorBlinkOn(video)) {
    return;
  }
  uint16_t cursor_offset = (VideoGetCursorAddress(video) - start_address) &
                           (metadata->vram_size / 2 - 1);
  uint16_t num_cells = (uint16_t)metadata->columns * metadata->rows;
  if (cursor_offset >= num_cells) {
    return;
  }

  uint8_t cursor_start = VideoGetCursorStartScanLine(video);
  uint8_t cursor_end = VideoGetCursorEndScanLine(video);
  if (cursor_start >= metadata->char_height) {
    return;
  }
  if (cursor_end >= metadata->char_height) {
    cursor_end = metadata->char_height - 1;
  }

  // The cursor is drawn in the foreground color of the cell it occupies.
  uint32_t char_address = ((uint32_t)start_address + cursor_offset) * 2;
  uint8_t attr_value = VideoReadVRAMByte(video, char_address + 1);
  RGB foreground = CGAGetColor(
      video, attr_value & (kVideoAttributeForegroundMask |
                           kVideoAttributeIntenseForeground));

  uint16_t origin_x =
      (cursor_offset % metadata->columns) * metadata->char_width * scale;
  uint16_t origin_y =
      (cursor_offset / metadata->columns) * metadata->char_height;
  for (uint8_t y = cursor_start; y <= cursor_end; ++y) {
    for (uint8_t x = 0; x < metadata->char_width; ++x) {
      CGAWritePixels(
          video, origin_x + x * scale, origin_y + y, scale, foreground);
    }
  }
}

// Render the current display in a CGA text mode.
static void CGARenderText(
    VideoState* video, const VideoModeMetadata* metadata) {
  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(video);
  uint8_t scale = (uint8_t)(adapter->frame_buffer_width / metadata->width);
  uint16_t start_address = VideoGetStartAddress(video);

  for (uint8_t row = 0; row < metadata->rows; ++row) {
    for (uint8_t col = 0; col < metadata->columns; ++col) {
      TextPosition char_pos = {.col = col, .row = row};
      // Each character takes 2 bytes (char + attr).
      uint32_t char_address =
          ((uint32_t)start_address + row * metadata->columns + col) * 2;
      CGAWriteChar(video, metadata, char_pos, char_address, scale);
    }
  }
  CGADrawCursor(video, metadata, start_address, scale);
}

// ============================================================================
// Graphics modes 0x04 - 0x06
// ============================================================================

// The three 320x200 graphics palettes, each holding colors 1 to 3. Color 0
// comes from the color select register instead. The palette is chosen by the
// palette bit of the color select register, except that the black and white bit
// of the mode control register overrides both.
static const uint8_t kCGAGraphicsPalettes[3][3] = {
    // Palette 0: green, red, brown
    {2, 4, 6},
    // Palette 1: cyan, magenta, light gray
    {3, 5, 7},
    // Black and white: cyan, red, light gray
    {3, 4, 7},
};

// Address of the first byte of a graphics mode scan line. Graphics VRAM is
// interleaved: even scan lines live in the first half of VRAM and odd scan
// lines in the second.
//
// The 6845 counts in character units, and a graphics mode fetches two bytes per
// unit, so the start address it holds is doubled to get a byte address. It also
// only addresses one half of VRAM - the scan line's parity picks the half - so
// it wraps within that half rather than across the whole of VRAM. The BIOS
// zeroes the start address on a mode set, so this only matters to software that
// page flips or scrolls by writing it directly.
static inline uint32_t CGAGetScanLineAddress(
    const VideoState* video, uint16_t y) {
  uint32_t address = ((uint32_t)VideoGetStartAddress(video) * 2 +
                      (uint32_t)(y / 2) * kCGAGraphicsBytesPerScanLine) &
                     kCGAGraphicsScanLineAddressMask;
  return address + (y & 1 ? kCGAGraphicsOddScanLineOffset : 0);
}

// Render the current display in 320x200 graphics mode.
static void CGARenderGraphics320x200(
    VideoState* video, const VideoModeMetadata* metadata) {
  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(video);
  uint8_t scale = (uint8_t)(adapter->frame_buffer_width / metadata->width);

  // Resolve the four palette entries once up front.
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
  RGB palette[4];
  palette[0] = CGAGetColor(
      video, video->color_select_register &
                 (kCGAColorSelectColorMask | kCGAColorSelectIntensity));
  for (uint8_t i = 0; i < 3; ++i) {
    palette[i + 1] = CGAGetColor(video, palette_colors[i] | intensity);
  }

  // Each VRAM byte holds several pixels, so the loop runs over bytes and
  // unpacks each one, rather than re-reading the same byte once per pixel.
  uint16_t bytes_per_scan_line = metadata->width / kCGAPixelsPerByte320x200;
  for (uint16_t y = 0; y < metadata->height; ++y) {
    uint32_t scan_line_address = CGAGetScanLineAddress(video, y);
    for (uint16_t byte_index = 0; byte_index < bytes_per_scan_line;
         ++byte_index) {
      uint8_t byte_value =
          VideoReadVRAMByte(video, scan_line_address + byte_index);
      uint16_t first_x = byte_index * kCGAPixelsPerByte320x200;
      for (uint8_t i = 0; i < kCGAPixelsPerByte320x200; ++i) {
        // The leftmost pixel is in the most significant bits.
        uint8_t shift = (uint8_t)((kCGAPixelsPerByte320x200 - 1 - i) *
                                  kCGABitsPerPixel320x200);
        uint8_t color = (byte_value >> shift) & kCGAPixelMask320x200;
        CGAWritePixels(video, (first_x + i) * scale, y, scale, palette[color]);
      }
    }
  }
}

// Render the current display in 640x200 graphics mode.
static void CGARenderGraphics640x200(
    VideoState* video, const VideoModeMetadata* metadata) {
  // The foreground color comes from the color select register, and the
  // background is always black.
  RGB foreground = CGAGetColor(
      video, video->color_select_register &
                 (kCGAColorSelectColorMask | kCGAColorSelectIntensity));
  RGB background = CGAGetColor(video, 0);

  // As in 320x200 mode, each VRAM byte is fetched once and unpacked into the
  // eight pixels it holds.
  uint16_t bytes_per_scan_line = metadata->width / kCGAPixelsPerByte640x200;
  for (uint16_t y = 0; y < metadata->height; ++y) {
    uint32_t scan_line_address = CGAGetScanLineAddress(video, y);
    for (uint16_t byte_index = 0; byte_index < bytes_per_scan_line;
         ++byte_index) {
      uint8_t byte_value =
          VideoReadVRAMByte(video, scan_line_address + byte_index);
      uint16_t first_x = byte_index * kCGAPixelsPerByte640x200;
      for (uint8_t i = 0; i < kCGAPixelsPerByte640x200; ++i) {
        // The leftmost pixel is in the most significant bit.
        uint8_t shift = (uint8_t)(kCGAPixelsPerByte640x200 - 1 - i);
        bool is_foreground = (byte_value >> shift) & 1;
        Position pixel_pos = {.x = first_x + i, .y = y};
        VideoWritePixel(
            video, pixel_pos, is_foreground ? foreground : background);
      }
    }
  }
}

// Render the current display on the CGA.
YAX86_PRIVATE void CGARenderScreen(VideoState* video) {
  const VideoModeMetadata* metadata = VideoGetModeMetadata(video);
  switch (metadata->mode) {
    case kVideoModeCGAText40x25Mono:
    case kVideoModeCGAText40x25Color:
    case kVideoModeCGAText80x25Mono:
    case kVideoModeCGAText80x25Color:
      CGARenderText(video, metadata);
      break;
    case kVideoModeCGAGraphics320x200:
    case kVideoModeCGAGraphics320x200Alt:
      CGARenderGraphics320x200(video, metadata);
      break;
    case kVideoModeCGAGraphics640x200:
      CGARenderGraphics640x200(video, metadata);
      break;
    default:
      // Not reachable - VideoGetMode() only ever returns a CGA mode on the CGA.
      // The branch exists so that the switch covers the enum.
      break;
  }
}
