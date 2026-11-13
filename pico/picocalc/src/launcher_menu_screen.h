// The list of VMs at power on, drawn as a box the size of the list. Up and
// Down move, and Enter or Space chooses; nothing closes it but starting a
// VM, so Esc and Backspace do nothing. The default VM can start by itself
// after a countdown, which any key stops.
//
// Under the VMs are three more rows: Manage VMs, from which VMs are made in
// yax86/vm, and renamed, cloned and deleted; USB disk mode; and Settings.
// Each opens a screen of its own over the list.
#ifndef YAX86_PICOCALC_LAUNCHER_MENU_SCREEN_H
#define YAX86_PICOCALC_LAUNCHER_MENU_SCREEN_H

#include <stdint.h>

#include "screen.h"
#include "settings.h"
#include "vm_config.h"
#include "vm_list.h"

// Show the list of vms, read from the card with settings, with the VM at
// highlighted chosen. With countdown_ms, the chosen VM starts by itself once
// that long has passed without a key. Manage VMs and USB disk mode change
// vms and settings, which must outlive the screen, and Manage VMs copies
// through copy_buffer, copy_buffer_size bytes of guest RAM. A VM chosen is
// read into *chosen, and closes the screens; one whose configuration does
// not read leaves the list showing its problem.
ScreenResult LauncherMenuScreenOpen(
    VMList* vms, Settings* settings, uint8_t* copy_buffer,
    uint32_t copy_buffer_size, uint8_t highlighted, uint32_t countdown_ms,
    VMConfig* chosen);

#endif  // YAX86_PICOCALC_LAUNCHER_MENU_SCREEN_H
