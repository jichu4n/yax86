#ifndef YAX86_SDL_FLOPPY_H
#define YAX86_SDL_FLOPPY_H

#include <stdbool.h>

#include "core/platform.h"

// Default floppy image mounted when none is given on the command line. Under
// Emscripten this is the path the image is packaged at; natively it is
// relative to the working directory.
extern const char* const kDefaultFloppyImagePath;

// Loads a floppy image from path and mounts it in drive A. Returns false if
// the image cannot be read or is not a supported size, in which case the
// platform is left with an empty drive and the BIOS will report no bootable
// device.
//
// The image is copied into memory. Guest writes go to that copy and are never
// written back, so booting cannot damage the file it booted from.
bool FloppyMount(PlatformState* platform, const char* path);

#endif  // YAX86_SDL_FLOPPY_H
