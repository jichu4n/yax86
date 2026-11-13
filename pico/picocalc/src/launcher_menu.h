// The list of VMs at power on, and the pages reached from it, drawn as a box
// the size of the list. Up and Down move, Enter or Space chooses, and Esc or
// Backspace goes back a page, though no further than the list: nothing
// closes it but starting a VM.
//
// Under the VMs are three more rows: Manage VMs, from which VMs are made in
// yax86/vm, and renamed, cloned and deleted; and USB mass storage and Settings,
// screens of their own that the caller opens.
//
// This holds the pages' state and draws them; what is chosen on them is
// launcher.c's to carry out, from the actions LauncherMenuHandleKey()
// returns.
#ifndef YAX86_PICOCALC_LAUNCHER_MENU_H
#define YAX86_PICOCALC_LAUNCHER_MENU_H

#include <stdbool.h>
#include <stdint.h>

#include "disk_image.h"
#include "southbridge.h"
#include "vm_config.h"
#include "vm_list.h"

// What a key did on the list.
typedef enum LauncherMenuAction {
  // Nothing the caller need act on.
  kLauncherMenuActionNone = 0,
  // A VM was chosen from the list: see LauncherMenuChosenVM().
  kLauncherMenuActionVMChosen,
  // USB mass storage was chosen from the list, for the caller to open over it.
  kLauncherMenuActionOpenUSB,
  // From Manage VMs: rename the VM LauncherMenuManagedVM() says to
  // LauncherMenuTypedName(), clone it as that name, or delete it. Then show
  // Manage VMs again with LauncherMenuShowManagedVMs(), or for a name that
  // will not do, UIStatusSetProblem() alone, which leaves the name to be
  // typed again.
  kLauncherMenuActionRenameVM,
  kLauncherMenuActionCloneVM,
  kLauncherMenuActionDeleteVM,
  // From Manage VMs' New VM: make the VM named LauncherMenuTypedName() with
  // the drives LauncherMenuFormDrives() describes; and from a VM's Edit
  // drives, give LauncherMenuManagedVM() those drives. Then as for a rename -
  // for an edit, showing the VM's page with LauncherMenuShowManagedVMs().
  kLauncherMenuActionCreateVM,
  kLauncherMenuActionEditVM,
  // Settings was chosen from the list, for the caller to open over it.
  kLauncherMenuActionOpenSettings,
} LauncherMenuAction;

// Show the list of VMs, to choose one to start, with the one at index
// highlighted. list must outlive the choosing.
void LauncherMenuOpen(const VMList* list, uint8_t highlighted);

// The index in the list of the VM chosen, or highlighted.
uint8_t LauncherMenuChosenVM(void);

// Draw the list again, as it was, once a screen opened over it is closed.
void LauncherMenuResume(void);

// The index in the list of the VM Manage VMs is acting on, and the name typed
// for it.
uint8_t LauncherMenuManagedVM(void);
const char* LauncherMenuTypedName(void);

// The drives filled in on the form, New VM's or Edit drives'. Its strings
// are the list's, until the form is next shown.
void LauncherMenuFormDrives(VMListDrives* drives);

// Show Manage VMs, once the caller has read the list afresh, with the VM
// named name chosen, or the first if it is not there - and with show_vm, the
// VM's own page.
void LauncherMenuShowManagedVMs(const char* name, bool show_vm);

// Show how far a clone has been copied, or a new VM's blank hard disk
// written, in KB: the first call shows the page, which takes no keys until
// the caller shows another.
void LauncherMenuSetCopyProgress(uint32_t copied_kb, uint32_t total_kb);

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
