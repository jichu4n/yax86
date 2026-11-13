// The launcher: what happens from power on until a VM starts.
//
// At power on it reads the card: the VMs, the settings, and the start note,
// which the in-VM menu's Reset and Power off leave across the reboot to say
// what to do next. Then it does one of two things. It loads a VM to start at
// once: the one reset, or the default when the settings ask for no
// countdown. Or it sets up the launcher menu, the list of VMs to choose
// from, which counts down to the default.
//
// The launcher menu (launcher_menu_screen.c) only chooses. Everything done
// with the card is here: loading the VM chosen, which makes it the default,
// and reading the card again after USB mass storage has given it to the
// host. For now the launcher holds the VM list and the settings, and main.c
// runs the launcher menu's loop, answering commands and updating the status
// rows.
#ifndef YAX86_PICOCALC_LAUNCHER_H
#define YAX86_PICOCALC_LAUNCHER_H

#include <stdbool.h>
#include <stdint.h>

#include "screen.h"
#include "settings.h"
#include "southbridge.h"
#include "vm_config.h"
#include "vm_list.h"

// What the start after a reboot is to do, left by the menu's Reset and Power
// off.
typedef enum StartIntent {
  // Start the VM again at once, without the launcher menu.
  kStartIntentRestart = 0,
  // Show the launcher menu with the VM chosen, and wait for a key.
  kStartIntentMenu,
} StartIntent;

// What LauncherStart() found to do.
typedef enum LauncherStartResult {
  // A VM was loaded without the launcher menu: the one restarted, or the
  // default with no wait.
  kLauncherLoadedVM = 0,
  // There is no card, so the built-in floppy boots.
  kLauncherNoCard,
  // The launcher menu shows, and waits for a key or the countdown.
  kLauncherShowsMenu,
} LauncherStartResult;

// Lend the launcher copy_buffer, kCopyBufferSize bytes of guest RAM, which
// copying a VM's files borrows before the VM starts.
void LauncherInit(uint8_t* copy_buffer);

// The VMs on the card, and the card's settings, as last read.
const VMList* LauncherVMs(void);
const Settings* LauncherSettings(void);

// Read the card's VMs and settings, and the note a reset or power off left,
// and either load a VM to start at once into *vm or set the launcher menu up.
LauncherStartResult LauncherStart(VMConfig* vm);

// Show the launcher menu as the launcher has set it up, with its VMs and
// settings. The launcher menu closes once it has chosen.
ScreenResult LauncherOpenMenu(void);

// Act on why the launcher menu closed.
//
// Returns true when a VM is to start: the one chosen, loaded into *vm.
//
// Returns false for LauncherOpenMenu() to show the menu again, without
// counting down, in two cases. USB mass storage gave the card to the host:
// the card has been read again. Or the chosen VM's vm.cfg does not read: its
// problem shows, and it stays chosen.
bool LauncherHandleMenuExit(VMConfig* vm);

// Restart the board, leaving a note for the start after it, with name the
// VM's: to start it again at once, or to show the launcher menu with it
// chosen.
void LauncherLeaveStartNote(StartIntent intent, const char* name);

#endif  // YAX86_PICOCALC_LAUNCHER_H
