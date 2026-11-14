// USB mass storage, from the list: export the entire SD card, or a disk
// image found in the browser, to the host as a USB mass storage device, for
// it to read and write until it is disconnected.
#ifndef YAX86_PICOCALC_USB_STORAGE_SCREEN_H
#define YAX86_PICOCALC_USB_STORAGE_SCREEN_H

#include <stdbool.h>

#include "screen.h"

// Show USB mass storage. Once the card or an image has been exported and is
// taken back, *did_export is set and the screen goes back: the host may have
// changed the card, so its parent reads it again. did_export must outlive
// the screen.
ScreenResult USBStorageScreenOpen(bool* did_export);

#endif  // YAX86_PICOCALC_USB_STORAGE_SCREEN_H
