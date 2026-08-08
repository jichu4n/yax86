// Internal interface shared between the video module's source files.
#ifndef YAX86_VIDEO_INTERNAL_H
#define YAX86_VIDEO_INTERNAL_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Read a byte from the emulated video RAM, or 0xFF if the address is out of
// range or no callback is installed.
YAX86_PRIVATE uint8_t VideoReadVRAMByte(VideoState* video, uint32_t address);

// Write a byte to the emulated video RAM, ignored if the address is out of
// range or no callback is installed.
YAX86_PRIVATE void VideoWriteVRAMByte(
    VideoState* video, uint32_t address, uint8_t value);

// Write an RGB pixel value to the real display, ignored if no callback is
// installed.
YAX86_PRIVATE void VideoWritePixel(
    VideoState* video, Position position, RGB rgb);

// Render the current display in MDA text mode.
YAX86_PRIVATE void MDARenderScreen(VideoState* video);

#endif  // YAX86_VIDEO_INTERNAL_H
