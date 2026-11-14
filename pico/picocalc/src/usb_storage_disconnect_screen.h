// The question before taking the card or image back from a host that has
// not ejected the volume: an unsafe disconnect.
#ifndef YAX86_PICOCALC_USB_STORAGE_DISCONNECT_SCREEN_H
#define YAX86_PICOCALC_USB_STORAGE_DISCONNECT_SCREEN_H

#include <stdbool.h>

#include "screen.h"

// Ask whether to disconnect anyway. *is_confirmed is set if so, before the
// screen goes back; Cancel goes back leaving it as it is. is_confirmed must
// outlive the screen.
ScreenResult USBStorageDisconnectScreenOpen(bool* is_confirmed);

#endif  // YAX86_PICOCALC_USB_STORAGE_DISCONNECT_SCREEN_H
