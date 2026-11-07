// Internal interface shared between the video module's source files.
#ifndef Y_VIDEO_INTERNAL_H
#define Y_VIDEO_INTERNAL_H

#ifndef Y_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // Y_IMPLEMENTATION

// Read a byte from the emulated video RAM, or 0xFF if no callback is installed.
// VRAM is aliased throughout the adapter's window, so an address past the end
// wraps around.
Y_MODULE_PRIVATE uint8_t
YVideoReadVRAMByte(YVideoState* video, uint32_t address);

// Write a byte to the emulated video RAM, ignored if no callback is installed.
// The address wraps in the same way as for reads.
Y_MODULE_PRIVATE void YVideoWriteVRAMByte(
    YVideoState* video, uint32_t address, uint8_t value);

// A horizontal run of pixels on its way to the host, buffered so that the
// callback is crossed once per batch rather than once per pixel.
//
// Every renderer walks a region's rows left to right, so a run is opened at
// the start of a row and pixels are pushed in order. That makes the position
// of each pixel implicit - it is wherever the run has got to - so nothing
// here compares coordinates, and a renderer has no way to name a position at
// all, let alone the wrong one.
typedef struct YVideoPixelRun {
  YVideoState* video;
  // Where the pixels currently buffered begin. Advances past each batch.
  YVideoPosition origin;
  YVideoRGB pixels[Y_VIDEO_PIXEL_BATCH_SIZE];
  uint8_t count;
} YVideoPixelRun;

// Hand over what has been buffered and continue the run after it.
Y_MODULE_PRIVATE_HEADER inline void YVideoPixelRunFlush(YVideoPixelRun* run) {
  if (run->count == 0) {
    return;
  }
  // Batching lets the region's pixel count be accumulated once per batch
  // rather than once per pixel.
  run->video->num_pixels_emitted_for_region += run->count;
  run->video->config.write_pixels(
      run->video, run->origin, run->pixels, run->count);
  run->origin.x += run->count;
  run->count = 0;
}

// Open a run at the leftmost pixel of a row. Any partial batch is handed over
// first, so one call both ends the previous row and begins the next.
Y_MODULE_PRIVATE_HEADER inline void YVideoPixelRunBegin(
    YVideoPixelRun* run, uint16_t x, uint16_t y) {
  YVideoPixelRunFlush(run);
  run->origin.x = x;
  run->origin.y = y;
}

Y_MODULE_PRIVATE_HEADER inline void YVideoPixelRunPush(
    YVideoPixelRun* run, YVideoRGB rgb) {
  run->pixels[run->count] = rgb;
  ++run->count;
  if (run->count == Y_VIDEO_PIXEL_BATCH_SIZE) {
    YVideoPixelRunFlush(run);
  }
}

// Mark a half-open rectangle dirty. Horizontal coordinates use the mode's 80
// natural columns; vertical coordinates are dirty rows - a character row in
// text modes, a group of Y_VIDEO_DIRTY_SCAN_LINES_PER_GROUP scan lines in
// graphics modes.
Y_MODULE_PRIVATE void YVideoInvalidateRows(
    YVideoState* video, uint8_t start_column, uint8_t end_column,
    uint8_t first_row, uint8_t end_row);

// Whether the text mode cursor is currently in the visible half of its blink
// cycle.
Y_MODULE_PRIVATE bool YVideoIsCursorBlinkOn(const YVideoState* video);

// Whether characters carrying the blink attribute are currently visible. This
// runs at half the cursor's rate, so the two drift in and out of phase.
Y_MODULE_PRIVATE bool YVideoIsTextBlinkOn(const YVideoState* video);

// Whether the text mode cursor is enabled in the 6845 registers.
Y_MODULE_PRIVATE bool YVideoIsCursorEnabled(const YVideoState* video);

// The first scan line of the character cell covered by the text mode cursor,
// from the 6845 cursor start register. May be out of range for the current
// character height.
Y_MODULE_PRIVATE uint8_t YVideoGetCursorStartScanLine(const YVideoState* video);

// The last scan line of the character cell covered by the text mode cursor,
// from the 6845 cursor end register. May be out of range for the current
// character height.
Y_MODULE_PRIVATE uint8_t YVideoGetCursorEndScanLine(const YVideoState* video);

// The address of the first displayed character, in character units, from the
// 6845 start address registers.
Y_MODULE_PRIVATE uint16_t YVideoGetStartAddress(const YVideoState* video);

// The address of the text mode cursor, in character units, from the 6845 cursor
// address registers.
Y_MODULE_PRIVATE uint16_t YVideoGetCursorAddress(const YVideoState* video);

// Whether the text mode cursor is currently drawn, and if so where. The offset
// is relative to start_address in character cells, and is only written when
// this returns true. The cursor is not drawn when it is disabled, when its
// blink phase is off, or when it addresses a cell outside the display.
Y_MODULE_PRIVATE bool YVideoGetVisibleCursorOffset(
    const YVideoState* video, const YVideoModeMetadata* metadata,
    uint16_t start_address, uint16_t* cursor_offset);

// Render a dirty region of the current MDA text display. Pixels are emitted in
// row-major order.
Y_MODULE_PRIVATE void YVideoMDARenderRegion(
    YVideoState* video, YVideoPixelRun* run, uint8_t start_column,
    uint8_t end_column, uint16_t first_y, uint16_t end_y);

// Render a dirty region of the current CGA display. Pixels are emitted in
// row-major order.
Y_MODULE_PRIVATE void YVideoCGARenderRegion(
    YVideoState* video, YVideoPixelRun* run, uint8_t start_column,
    uint8_t end_column, uint16_t first_y, uint16_t end_y);

#endif  // Y_VIDEO_INTERNAL_H
