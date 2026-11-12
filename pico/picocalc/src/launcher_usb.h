// USB disk mode, a page from the list at power on: the whole SD card, or a
// disk image on it found with the browser, shared with a computer over USB,
// until Disconnect takes it back. The card's page says who has it, and asks
// first if a computer is still using it.
//
// This holds the pages and does what is chosen on them; launcher.c opens it
// from the list and goes back to the list when it says so.
#ifndef YAX86_PICOCALC_LAUNCHER_USB_H
#define YAX86_PICOCALC_LAUNCHER_USB_H

#include <stdint.h>

#include "southbridge.h"

// What a key did in USB disk mode.
typedef enum LauncherUsbResult {
  // It stays open.
  kLauncherUsbStays = 0,
  // Esc on its first page: back to the list.
  kLauncherUsbLeft,
  // The card or image is back from USB. The computer may have added VMs,
  // removed them, or changed the settings, so the list is read afresh.
  kLauncherUsbReturned,
} LauncherUsbResult;

// Show USB disk mode's first page, with the whole SD card chosen.
void LauncherUsbOpen(void);

// Hand USB disk mode a key event, while it shows.
LauncherUsbResult LauncherUsbHandleKey(SouthbridgeKeyEvent event);

// Draw USB disk mode if it has changed, and the card's page again if a second
// has passed. Call every pass of the loop while it shows.
void LauncherUsbDraw(uint64_t now_us);

// The keys to show on the status bar's message row, as Key=Action pairs.
const char* LauncherUsbHint(void);

// The page showing, as the report gives it.
const char* LauncherUsbPageName(void);

#endif  // YAX86_PICOCALC_LAUNCHER_USB_H
