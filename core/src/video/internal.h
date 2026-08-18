// Internal interface shared between the video module's source files.
#ifndef YAX86_VIDEO_INTERNAL_H
#define YAX86_VIDEO_INTERNAL_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Read a byte from the emulated video RAM, or 0xFF if no callback is installed.
// VRAM is aliased throughout the adapter's window, so an address past the end
// wraps around.
YAX86_PRIVATE uint8_t VideoReadVRAMByte(VideoState* video, uint32_t address);

// Write a byte to the emulated video RAM, ignored if no callback is installed.
// The address wraps in the same way as for reads.
YAX86_PRIVATE void VideoWriteVRAMByte(
    VideoState* video, uint32_t address, uint8_t value);

// A horizontal run of pixels on its way to the host, buffered so that the
// callback is crossed once per batch rather than once per pixel.
//
// Every renderer walks a region's rows left to right, so a run is opened at
// the start of a row and pixels are pushed in order. That makes the position
// of each pixel implicit - it is wherever the run has got to - so nothing
// here compares coordinates, and a renderer has no way to name a position at
// all, let alone the wrong one.
typedef struct VideoPixelRun {
  VideoState* video;
  // Where the pixels currently buffered begin. Advances past each batch.
  Position origin;
  RGB pixels[kVideoPixelBatchSize];
  uint8_t count;
} VideoPixelRun;

// Hand over what has been buffered and continue the run after it.
static inline void VideoPixelRunFlush(VideoPixelRun* run) {
  if (run->count == 0) {
    return;
  }
  // Batching lets the region's pixel count be accumulated once per batch
  // rather than once per pixel.
  run->video->num_pixels_emitted_for_region += run->count;
  run->video->config->write_pixels(
      run->video, run->origin, run->pixels, run->count);
  run->origin.x += run->count;
  run->count = 0;
}

// Open a run at the leftmost pixel of a row. Any partial batch is handed over
// first, so one call both ends the previous row and begins the next.
static inline void VideoPixelRunBegin(
    VideoPixelRun* run, uint16_t x, uint16_t y) {
  VideoPixelRunFlush(run);
  run->origin.x = x;
  run->origin.y = y;
}

static inline void VideoPixelRunPush(VideoPixelRun* run, RGB rgb) {
  run->pixels[run->count] = rgb;
  ++run->count;
  if (run->count == kVideoPixelBatchSize) {
    VideoPixelRunFlush(run);
  }
}

// Mark a half-open rectangle dirty. Horizontal coordinates use the mode's 80
// natural columns; vertical coordinates are dirty rows - a character row in
// text modes, a group of kVideoDirtyScanLinesPerGroup scan lines in graphics
// modes.
YAX86_PRIVATE void VideoInvalidateRows(
    VideoState* video, uint8_t start_column, uint8_t end_column,
    uint8_t first_row, uint8_t end_row);

// Whether the text mode cursor is currently in the visible half of its blink
// cycle.
YAX86_PRIVATE bool VideoIsCursorBlinkOn(const VideoState* video);

// Whether characters carrying the blink attribute are currently visible. This
// runs at half the cursor's rate, so the two drift in and out of phase.
YAX86_PRIVATE bool VideoIsTextBlinkOn(const VideoState* video);

// Whether the text mode cursor is enabled in the 6845 registers.
YAX86_PRIVATE bool VideoIsCursorEnabled(const VideoState* video);

// The first scan line of the character cell covered by the text mode cursor,
// from the 6845 cursor start register. May be out of range for the current
// character height.
YAX86_PRIVATE uint8_t VideoGetCursorStartScanLine(const VideoState* video);

// The last scan line of the character cell covered by the text mode cursor,
// from the 6845 cursor end register. May be out of range for the current
// character height.
YAX86_PRIVATE uint8_t VideoGetCursorEndScanLine(const VideoState* video);

// The address of the first displayed character, in character units, from the
// 6845 start address registers.
YAX86_PRIVATE uint16_t VideoGetStartAddress(const VideoState* video);

// The address of the text mode cursor, in character units, from the 6845 cursor
// address registers.
YAX86_PRIVATE uint16_t VideoGetCursorAddress(const VideoState* video);

// Whether the text mode cursor is currently drawn, and if so where. The offset
// is relative to start_address in character cells, and is only written when
// this returns true. The cursor is not drawn when it is disabled, when its
// blink phase is off, or when it addresses a cell outside the display.
YAX86_PRIVATE bool VideoGetVisibleCursorOffset(
    const VideoState* video, const VideoModeMetadata* metadata,
    uint16_t start_address, uint16_t* cursor_offset);

// Render a dirty region of the current MDA text display. Pixels are emitted in
// row-major order.
YAX86_PRIVATE void MDARenderRegion(
    VideoState* video, VideoPixelRun* run, uint8_t start_column,
    uint8_t end_column, uint16_t first_y, uint16_t end_y);

// Render a dirty region of the current CGA display. Pixels are emitted in
// row-major order.
YAX86_PRIVATE void CGARenderRegion(
    VideoState* video, VideoPixelRun* run, uint8_t start_column,
    uint8_t end_column, uint16_t first_y, uint16_t end_y);

#endif  // YAX86_VIDEO_INTERNAL_H
