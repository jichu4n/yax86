// The list of VMs at power on, drawn as a box the size of the list. Up and
// Down move, and Enter or Space chooses; nothing closes it but starting a
// VM, so Esc and Backspace do nothing. The default VM can start by itself
// after a countdown, which any key stops.
//
// Under the VMs are three more rows: Manage VMs, from which VMs are made in
// yax86/vm, and renamed, cloned and deleted; USB mass storage; and Settings.
// Each opens a screen of its own over the list.
//
// The list only chooses: what is done with the card, starting a VM and
// reading the card again, is its caller's, once the list has closed.
#ifndef YAX86_PICOCALC_LAUNCHER_MENU_SCREEN_H
#define YAX86_PICOCALC_LAUNCHER_MENU_SCREEN_H

#include <stdint.h>

#include "screen.h"
#include "settings.h"
#include "vm_list.h"

// Why the list closed.
typedef enum LauncherMenuExitReason {
  // A VM was chosen to start, or reached by the countdown.
  kLauncherMenuExitStartVM = 0,
  // USB mass storage gave the card to the host, which may have changed its
  // VMs and settings, so the card is to be read again.
  kLauncherMenuExitReadCard,
} LauncherMenuExitReason;

typedef struct LauncherMenuExit {
  LauncherMenuExitReason reason;
  // The VM to start, an index in the list, for kLauncherMenuExitStartVM.
  uint8_t vm;
} LauncherMenuExit;

// Show the list of vms, read from the card with settings, with the VM at
// highlighted chosen. With countdown_ms, the chosen VM starts by itself once
// that long has passed without a key. Manage VMs and USB mass storage change
// vms and settings, which must outlive the screen, and Manage VMs copies
// through copy_buffer, copy_buffer_size bytes of guest RAM. Why the list
// closes, which ends the screens, is written to *exit.
ScreenResult LauncherMenuScreenOpen(
    VMList* vms, Settings* settings, uint8_t* copy_buffer,
    uint32_t copy_buffer_size, uint8_t highlighted, uint32_t countdown_ms,
    LauncherMenuExit* exit);

#endif  // YAX86_PICOCALC_LAUNCHER_MENU_SCREEN_H
