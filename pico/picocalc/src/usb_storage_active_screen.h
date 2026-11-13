// The card, or an image, while it is exported: what it is, the host's state
// and how much has moved, drawn again every second, and Disconnect, which
// asks first if the host has the volume mounted.
#ifndef YAX86_PICOCALC_USB_STORAGE_ACTIVE_SCREEN_H
#define YAX86_PICOCALC_USB_STORAGE_ACTIVE_SCREEN_H

#include <stdbool.h>

#include "disk_image.h"
#include "screen.h"

// Show what is exported, once the caller has exported it: image, or NULL for
// the entire card. On Disconnect it is taken back from the host, an image
// closed, writing back the sector it holds, and *stopped set before the screen
// goes back. Both must outlive the screen.
ScreenResult USBStorageActiveScreenOpen(DiskImage* image, bool* stopped);

#endif  // YAX86_PICOCALC_USB_STORAGE_ACTIVE_SCREEN_H
