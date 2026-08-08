#ifndef YAX86_SDL_HARDDISK_H
#define YAX86_SDL_HARDDISK_H

#include <stdbool.h>

#include "core/platform.h"

// Attaches a hard disk as the controller's master drive, backed by the image
// at path. A NULL path attaches a blank disk, which the guest can partition
// and format for itself.
//
// The image is copied into memory. Guest writes go to that copy and are never
// written back, so the disk starts out the same way on every run - the same
// arrangement the floppy drive uses. Returns false if the image cannot be read
// or is not the expected size, in which case no drive is attached.
//
// The controller's option ROM, which is what gives the guest INT 13h support
// for hard disks at all, is compiled into the core library and needs nothing
// from the host.
bool HardDiskAttach(PlatformState* platform, const char* path);

#endif  // YAX86_SDL_HARDDISK_H
