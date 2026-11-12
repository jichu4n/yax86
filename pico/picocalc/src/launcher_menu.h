// The list of VMs at power on, and the pages reached from it, drawn as a box
// the size of the list. Up and Down move, Enter or Space chooses, and Esc or
// Backspace goes back a page, though no further than the list: nothing
// closes it but starting a VM.
//
// Under the VMs are three more rows: Manage VMs, from which VMs are made in
// yax86/vm, and renamed, cloned and deleted; USB disk mode, a page from which
// the SD card or an image is shared over USB; and Settings, which chooses the
// 4x10 font.
//
// This holds the pages' state and draws them; what is chosen on them is
// launcher.c's to carry out, from the actions LauncherMenuHandleKey()
// returns.
#ifndef YAX86_PICOCALC_LAUNCHER_MENU_H
#define YAX86_PICOCALC_LAUNCHER_MENU_H

#include <stdbool.h>
#include <stdint.h>

#include "disk_image.h"
#include "font_4x10.h"
#include "southbridge.h"
#include "vm_config.h"
#include "vm_folder.h"

// What a key did on the list.
typedef enum LauncherMenuAction {
  // Nothing the caller need act on.
  kLauncherMenuActionNone = 0,
  // A VM was chosen from the list: see LauncherMenuChosenVM().
  kLauncherMenuActionVMChosen,
  // From the list's USB disk page: put the SD card on USB, then show it with
  // LauncherMenuShowSharing(); and take it back, then show the list again.
  kLauncherMenuActionShareCard,
  kLauncherMenuActionStopSharing,
  // From the same page, through the browser: put a disk image on USB - see
  // LauncherMenuChosenImage() - then show it with LauncherMenuShowSharing().
  kLauncherMenuActionShareImage,
  // From Manage VMs: rename the VM LauncherMenuManagedVM() says to
  // LauncherMenuTypedName(), clone it as that name, or delete it. Then show
  // Manage VMs again with LauncherMenuShowManagedVMs(), or for a name that
  // will not do, LauncherMenuSetProblem() alone, which leaves the name to be
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
  // From Settings' Font page: use and save LauncherMenuChosenFont().
  kLauncherMenuActionFontChosen,
} LauncherMenuAction;

// Show the list of the card's VMs, as VMStoreList() has them, to choose one to
// start, with the one at index highlighted.
void LauncherMenuOpen(uint8_t highlighted);

// Stop showing the list, once a VM chosen has started.
void LauncherMenuClose(void);

bool LauncherMenuIsOpen(void);

// The index in the list of the VM chosen, or highlighted.
uint8_t LauncherMenuChosenVM(void);

// The font chosen on the Font page. The page opens on the settings' font.
Font4x10 LauncherMenuChosenFont(void);

// Show the SD card's page, once the caller has put it on USB: who has it,
// and Disconnect, which asks first if a computer is still using it.
void LauncherMenuShowSharing(void);

// The disk image chosen to put on USB, a path from the card's root, and its
// size, which is a floppy's or the hard disk's.
const char* LauncherMenuChosenImage(uint32_t* size);

// The index in the list of the VM Manage VMs is acting on, and the name typed
// for it.
uint8_t LauncherMenuManagedVM(void);
const char* LauncherMenuTypedName(void);

// The drives filled in on the form, New VM's or Edit drives'. Its strings
// are the list's, until the form is next shown.
void LauncherMenuFormDrives(VMFolderDrives* drives);

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

// Set why the VM chosen could not start, for LauncherMenuProblem(), until
// another key is pressed; NULL for nothing. The text is copied.
void LauncherMenuSetProblem(const char* problem);

// Hand the list a key event, while it shows.
LauncherMenuAction LauncherMenuHandleKey(SouthbridgeKeyEvent event);

// Draw the list if it has changed, and the sharing page again if a second
// has passed. Call every pass of the loop while the list shows.
void LauncherMenuDraw(uint64_t now_us);

// The keys to show on the status bar's message row, as Key=Action pairs.
const char* LauncherMenuHint(void);

// Why the VM chosen could not start, to show in place of the keys, or
// NULL.
const char* LauncherMenuProblem(void);

// The page showing, as the report gives it.
const char* LauncherMenuPageName(void);

#endif  // YAX86_PICOCALC_LAUNCHER_MENU_H
