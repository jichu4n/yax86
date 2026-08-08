#ifndef YAX86_IMPLEMENTATION
#include "fonts.h"
#include "internal.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

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

// Fill the entire frame buffer with a single color.
static void CGAFillScreen(VideoState* video, RGB rgb) {
  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(video);
  for (uint16_t y = 0; y < adapter->frame_buffer_height; ++y) {
    for (uint16_t x = 0; x < adapter->frame_buffer_width; ++x) {
      Position pixel_pos = {.x = x, .y = y};
      VideoWritePixel(video, pixel_pos, rgb);
    }
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
    case kVideoModeCGAGraphics640x200:
      // Graphics modes are not yet implemented - render blank rather than
      // stale or garbage content.
      CGAFillScreen(video, CGAGetColor(video, 0));
      break;
    default:
      // Not reachable - VideoGetMode() only ever returns a CGA mode on the CGA.
      CGAFillScreen(video, CGAGetColor(video, 0));
      break;
  }
}
