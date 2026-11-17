#ifndef YAX86_SDL_FLOPPY_H
#define YAX86_SDL_FLOPPY_H

#include <stdbool.h>

#include "core/platform.h"

// Default floppy image mounted when none is given on the command line. Under
// Emscripten this is the path the image is packaged at; natively it is
// relative to the working directory.
extern const char* const kDefaultFloppyImagePath;

// Loads a floppy image from path and mounts it in drive A. Returns false if
// the image cannot be read, is not a supported size, or is not accepted by
// drive A's configured drive type, in which case the platform is left with an
// empty drive and the BIOS will report no bootable device.
//
// The image is copied into memory. Guest writes go to that copy and are never
// written back, so booting cannot damage the file it booted from.
bool FloppyMount(YPlatformState* platform, const char* path);

// Parses a drive type argument (360k, 720k, 1.2m, 1.44m, or with a b suffix).
bool FloppyParseDriveType(const char* value, YFDCDriveType* drive_type);

#endif  // YAX86_SDL_FLOPPY_H
