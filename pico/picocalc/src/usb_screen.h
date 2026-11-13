// USB disk mode, from the list: put the whole SD card, or a disk image found
// in the browser, on USB for a computer to read and write, until it is
// disconnected.
#ifndef YAX86_PICOCALC_USB_SCREEN_H
#define YAX86_PICOCALC_USB_SCREEN_H

#include <stdbool.h>

#include "screen.h"

// Show USB disk mode. Once the card or an image has been on USB and is taken
// back, *shared is set and the screen goes back: the computer may have
// changed the card, so its parent reads it again. shared must outlive the
// screen.
ScreenResult USBScreenOpen(bool* shared);

#endif  // YAX86_PICOCALC_USB_SCREEN_H
