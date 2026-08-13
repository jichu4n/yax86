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

// Write an RGB pixel value to the real display, ignored if no callback is
// installed.
YAX86_PRIVATE void VideoWritePixel(
    VideoState* video, Position position, RGB rgb);

// Mark a half-open rectangle dirty. Horizontal coordinates use the mode's 80
// natural columns; vertical coordinates are physical frame-buffer scan lines.
YAX86_PRIVATE void VideoInvalidateRegion(
    VideoState* video, uint8_t first_column, uint8_t end_column,
    uint16_t first_y, uint16_t end_y);

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
    VideoState* video, uint8_t first_column, uint8_t end_column,
    uint16_t first_y, uint16_t end_y);

// Render a dirty region of the current CGA display. Pixels are emitted in
// row-major order.
YAX86_PRIVATE void CGARenderRegion(
    VideoState* video, uint8_t first_column, uint8_t end_column,
    uint16_t first_y, uint16_t end_y);

#endif  // YAX86_VIDEO_INTERNAL_H
