// Launcher mode: what happens from power on until a VM starts.
//
// At power on it reads the card: the VMs, the settings, and the start note,
// which the in-VM menu's Reset and Power off leave across the reboot to say
// what to do next. Then it does one of two things. It loads a VM to start at
// once: the one reset, or the default when the settings ask for no
// countdown. Or it shows the launcher menu, the list of VMs to choose from,
// which counts down to the default, and runs it until a VM is loaded,
// answering commands and updating the status rows.
//
// The launcher menu (launcher_menu_screen.c) only chooses. Everything done
// with the card is here: loading the VM chosen, which makes it the default,
// and reading the card again after USB mass storage has given it to the
// host.
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

// Run launcher mode, and return whether a VM was loaded into *chosen to
// start. main.c holds what it works on: vms and settings, which it reads from
// the card and the screens change; and copy_buffer, guest RAM that copying a
// VM's files borrows until the VM starts. commands answers the commands that
// are not the launcher's.
//
// Without a card it returns false at once, and the emulator boots the floppy
// built into flash. Otherwise:
//   - After a Reset, the VM reset starts again at once.
//   - After a Power off, the launcher menu waits for a key, with the VM
//     powered off highlighted.
//   - At power on, the default starts at once if the settings ask for no
//     countdown and have no problem to show; otherwise the launcher menu
//     counts down to it, until a key is pressed.
// With no VMs the launcher menu offers Manage VMs, USB mass storage and
// Settings, and nothing starts by itself. A VM whose vm.cfg does not read
// leaves the launcher menu showing its problem.
bool LauncherRun(
    VMList* vms, Settings* settings, uint8_t* copy_buffer,
    const CommandsHandlers* commands, VMConfig* chosen);

// Restart the board, leaving a note for the start after it, with name the
// VM's: to start it again at once, or to show the launcher menu with it
// highlighted.
void LauncherLeaveStartNote(StartIntent intent, const char* name);

#endif  // YAX86_PICOCALC_LAUNCHER_H
