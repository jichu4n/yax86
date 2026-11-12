// Manage VMs, a page from the list at power on: the VMs in yax86/vm, and for
// each, its page, from which its drives are edited and it is renamed, cloned
// or deleted; and New VM, a form for a new one's name and drives. Copying a
// clone's files, or writing a new VM's blank disks, shows how far it has come.
//
// This holds the pages and does what is chosen on them; launcher.c opens it
// from the list and goes back to the list when it says so. After changing
// the card it has the launcher read it afresh, with LauncherReadCard().
#ifndef YAX86_PICOCALC_LAUNCHER_MANAGE_H
#define YAX86_PICOCALC_LAUNCHER_MANAGE_H

#include <stdint.h>

#include "southbridge.h"
#include "vm_config.h"

// What a key did in Manage VMs.
typedef enum LauncherManageResult {
  // It stays open.
  kLauncherManageStays = 0,
  // Esc on Manage VMs itself: back to the list.
  kLauncherManageLeft,
} LauncherManageResult;

// Give Manage VMs the launcher's list of the card's VMs, which it reads while
// it shows, and copy_buffer, kCopyBufferSize bytes of guest RAM, which
// copying a VM's files borrows before the VM starts. Both must outlive it.
void LauncherManageInit(const VMList* vms, uint8_t* copy_buffer);

// Show Manage VMs, with the first VM chosen.
void LauncherManageOpen(void);

// Hand Manage VMs a key event, while it shows.
LauncherManageResult LauncherManageHandleKey(SouthbridgeKeyEvent event);

// Draw Manage VMs if it has changed. Call every pass of the loop while it
// shows.
void LauncherManageDraw(void);

// The keys to show on the status bar's message row, as Key=Action pairs.
const char* LauncherManageHint(void);

// The page showing, as the report gives it.
const char* LauncherManagePageName(void);

#endif  // YAX86_PICOCALC_LAUNCHER_MANAGE_H
