// The list of VMs at power on, drawn as a box the size of the list. Up and
// Down move, and Enter or Space chooses; Esc and Backspace go nowhere, since
// nothing closes it but starting a VM.
//
// Under the VMs are three more rows, each opening a page of its own: Manage
// VMs, launcher_manage.h's, from which VMs are made in yax86/vm, and renamed,
// cloned and deleted; USB disk mode, launcher_usb.h's, from which the SD card
// or an image is shared over USB; and Settings, launcher_settings.h's, which
// chooses the 4x10 font.
//
// This holds the list's state and draws it; what is chosen on it is
// launcher.c's to carry out, from the actions LauncherMenuHandleKey()
// returns.
#ifndef YAX86_PICOCALC_LAUNCHER_MENU_H
#define YAX86_PICOCALC_LAUNCHER_MENU_H

#include <stdbool.h>
#include <stdint.h>

#include "southbridge.h"
#include "vm_config.h"

// What a key did on the list.
typedef enum LauncherMenuAction {
  // Nothing the caller need act on.
  kLauncherMenuActionNone = 0,
  // A VM was chosen from the list: see LauncherMenuChosenVM().
  kLauncherMenuActionVMChosen,
  // A row under the VMs was chosen. Show the list again after what it opens
  // with LauncherMenuShowManageRow(), LauncherMenuShowUSBRow() or
  // LauncherMenuShowSettingsRow().
  kLauncherMenuActionOpenManage,
  kLauncherMenuActionOpenUSB,
  kLauncherMenuActionOpenSettings,
} LauncherMenuAction;

// Show the list of VMs, to choose one to start, with the one at index
// highlighted. list must outlive the choosing.
void LauncherMenuOpen(const VMList* list, uint8_t highlighted);

// Stop showing the list, once a VM chosen has started.
void LauncherMenuClose(void);

bool LauncherMenuIsOpen(void);

// The index in the list of the VM chosen, or highlighted.
uint8_t LauncherMenuChosenVM(void);

// Show the list again with a row under the VMs chosen, when what it opened
// is left.
void LauncherMenuShowManageRow(void);
void LauncherMenuShowUSBRow(void);
void LauncherMenuShowSettingsRow(void);

// Show the seconds left before the highlighted VM starts by itself, or
// stop showing them with 0.
void LauncherMenuSetCountdown(uint8_t seconds);

// Hand the list a key event, while it shows.
LauncherMenuAction LauncherMenuHandleKey(SouthbridgeKeyEvent event);

// Draw the list if it has changed. Call every pass of the loop while the
// list shows.
void LauncherMenuDraw(void);

// The keys to show on the status bar's message row, as Key=Action pairs.
const char* LauncherMenuHint(void);

// The page showing, as the report gives it.
const char* LauncherMenuPageName(void);

#endif  // YAX86_PICOCALC_LAUNCHER_MENU_H
