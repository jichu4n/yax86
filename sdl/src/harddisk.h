#ifndef YAX86_SDL_HARDDISK_H
#define YAX86_SDL_HARDDISK_H

#include <stdbool.h>

#include "core/platform.h"

// Attaches a hard disk as the controller's master drive. Does nothing about
// the drive's contents - that is the caller's business - so until an image is
// wired up the guest sees a drive it can identify but not read.
//
// The controller's option ROM, which is what gives the guest INT 13h support
// for hard disks at all, is compiled into the core library and needs nothing
// from the host.
void HardDiskAttach(PlatformState* platform);

#endif  // YAX86_SDL_HARDDISK_H
