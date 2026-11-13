// The card, or an image, while it is on USB: who has it and how much has
// moved, drawn again every second, and Disconnect, which asks first if a
// computer is still using it.
#ifndef YAX86_PICOCALC_USB_SHARING_SCREEN_H
#define YAX86_PICOCALC_USB_SHARING_SCREEN_H

#include <stdbool.h>

#include "disk_image.h"
#include "screen.h"

// Show what is on USB, once the caller has put it there: image, or NULL for
// the whole card. On Disconnect it is taken back from USB, an image closed,
// writing back the sector it holds, and *stopped set before the screen goes
// back. Both must outlive the screen.
ScreenResult SharingScreenOpen(DiskImage* image, bool* stopped);

#endif  // YAX86_PICOCALC_USB_SHARING_SCREEN_H
