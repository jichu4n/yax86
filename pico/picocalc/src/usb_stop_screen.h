// The question before taking the card back from a computer that has not
// ejected it.
#ifndef YAX86_PICOCALC_USB_STOP_SCREEN_H
#define YAX86_PICOCALC_USB_STOP_SCREEN_H

#include <stdbool.h>

#include "screen.h"

// Ask whether to disconnect anyway. *confirmed is set if so, before the
// screen goes back; Cancel goes back leaving it as it is. confirmed must
// outlive the screen.
ScreenResult USBStopScreenOpen(bool* confirmed);

#endif  // YAX86_PICOCALC_USB_STOP_SCREEN_H
