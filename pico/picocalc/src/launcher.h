// Launcher mode, from power on until a VM is chosen: reading the card's VMs
// and settings and the note a reset or power off left, then either loading a
// VM to start at once or showing the launcher menu to choose one, and the
// screens reached from it. Its loop answers commands, and composes the status
// rows from the keys of the screen showing.
#ifndef YAX86_PICOCALC_LAUNCHER_H
#define YAX86_PICOCALC_LAUNCHER_H

#include <stdbool.h>
#include <stdint.h>

#include "commands.h"
#include "settings.h"
#include "southbridge.h"
#include "vm_config.h"
#include "vm_list.h"

// What the start after a reboot is to do, left by the menu's Reset and Power
// off.
typedef enum StartIntent {
  // Start the VM again at once, without the launcher menu.
  kStartIntentRestart = 0,
  // Show the launcher menu, with the VM highlighted, and wait for a key.
  kStartIntentMenu,
} StartIntent;

// Run launcher mode until a VM is chosen, and return whether one was, read
// into *chosen. vms and settings are read from the card into, and kept by
// the screens as they change it; settings stays as read for the VM. A VM's
// files are copied through copy_buffer, guest RAM that is lent until the VM
// starts. commands answers what the host asks that is not the launcher's.
//
// With a card, the launcher menu of its VMs shows until one is chosen whose
// configuration reads, or until the default starts by itself, the countdown
// running until a key is pressed. Settings that ask for no wait start the
// default at once, without the launcher menu, unless there is a problem to
// show. With no VMs the launcher menu offers Manage VMs, USB mass storage and
// Settings, and nothing starts by itself. Without a card it returns false at
// once, and the built-in floppy boots in drive A.
//
// After a reset the VM reset starts again at once; after a power off the
// launcher menu waits for a key, with the VM turned off highlighted. Either
// way, a VM that no longer reads leaves the launcher menu waiting with its
// problem.
bool LauncherRun(
    VMList* vms, Settings* settings, uint8_t* copy_buffer,
    const CommandsHandlers* commands, VMConfig* chosen);

// Restart the board, leaving a note for the start after it, with name the
// VM's: to start it again at once, or to show the launcher menu with it
// highlighted.
void LauncherLeaveStartNote(StartIntent intent, const char* name);

#endif  // YAX86_PICOCALC_LAUNCHER_H
