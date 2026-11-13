// The list of VMs at power on, drawn as a box the size of the list. Up and
// Down move, and Enter or Space chooses; nothing closes it but starting a
// VM, so Esc and Backspace do nothing.
//
// Under the VMs are three more rows: Manage VMs, from which VMs are made in
// yax86/vm, and renamed, cloned and deleted; USB disk mode; and Settings.
// Each is a screen of its own, which the caller opens over the list.
//
// This holds the list's state and draws it; what is chosen on it is
// launcher.c's to carry out, from the actions LauncherMenuHandleKey()
// returns.
#ifndef YAX86_PICOCALC_LAUNCHER_MENU_H
#define YAX86_PICOCALC_LAUNCHER_MENU_H

#include <stdbool.h>
#include <stdint.h>

#include "southbridge.h"
#include "vm_list.h"

// What a key did on the list.
typedef enum LauncherMenuAction {
  // Nothing the caller need act on.
  kLauncherMenuActionNone = 0,
  // A VM was chosen from the list: see LauncherMenuChosenVM().
  kLauncherMenuActionVMChosen,
  // Manage VMs, USB disk mode or Settings was chosen, for the caller to
  // open over the list.
  kLauncherMenuActionOpenManage,
  kLauncherMenuActionOpenUSB,
  kLauncherMenuActionOpenSettings,
} LauncherMenuAction;

// Show the list of VMs, to choose one to start, with the one at index
// highlighted. list must outlive the choosing.
void LauncherMenuOpen(const VMList* list, uint8_t highlighted);

// The index in the list of the VM chosen, or highlighted.
uint8_t LauncherMenuChosenVM(void);

// Show the list again once a screen opened over it is closed, with the row
// that opened it chosen: the VMs may have changed under it.
void LauncherMenuResume(void);

// Show the seconds left before the highlighted VM starts by itself, or
// stop showing them with 0.
void LauncherMenuSetCountdown(uint8_t seconds);

// Hand the list a key event, while it shows.
LauncherMenuAction LauncherMenuHandleKey(SouthbridgeKeyEvent event);

// Draw the list if it has changed. Call every pass of the loop while the
// list shows.
void LauncherMenuDraw(uint64_t now_us);

// The keys to show on the status bar's message row, as Key=Action pairs.
const char* LauncherMenuHint(void);

// The page showing, as the report gives it.
const char* LauncherMenuPageName(void);

#endif  // YAX86_PICOCALC_LAUNCHER_MENU_H
