// The list of VMs at power on, and what can be done from it: starting a VM,
// managing VMs, sharing the card or an image over USB, and choosing a font.
// main.c runs the list's loop, answering commands, updating the status rows
// and drawing the menu, and the launcher decides what the list does: which
// VM starts, when the countdown ends, and what a key leads to.
#ifndef YAX86_PICOCALC_LAUNCHER_H
#define YAX86_PICOCALC_LAUNCHER_H

#include <stdbool.h>
#include <stdint.h>

#include "screen.h"
#include "southbridge.h"
#include "vm_config.h"

// What the start after a reboot is to do, left by the menu's Reset and Power
// off.
typedef enum StartIntent {
  // Start the VM again at once, without the list.
  kStartIntentRestart = 0,
  // Show the list, with the VM highlighted, and wait for a key.
  kStartIntentList,
} StartIntent;

// What LauncherStart() found to do.
typedef enum LauncherStartResult {
  // A VM was taken without the list: the one restarted, or the default with
  // no wait.
  kLauncherTookVM = 0,
  // There is no card, so the built-in floppy boots.
  kLauncherNoCard,
  // The list shows, and waits for a key or the countdown.
  kLauncherShowsList,
} LauncherStartResult;

// Lend the launcher copy_buffer, kCopyBufferSize bytes of guest RAM, which
// copying a VM's files borrows before the VM starts.
void LauncherInit(uint8_t* copy_buffer);

// Read the card's VMs and settings, and the note a reset or power off left,
// and either take a VM at once into *vm or show the list.
LauncherStartResult LauncherStart(VMConfig* vm);

// The list and every page reached from it, as one screen. Its keys stop
// the countdown, and its time runs it.
extern const Screen kLauncherScreen;

// Take the VM chosen into *vm, if its configuration reads. Returns false,
// with its problem shown on the list, if it does not.
bool LauncherTakeChosen(VMConfig* vm);

// Restart the board, leaving a note for the start after it, with name the
// VM's: to start it again at once, or to show the list with it highlighted.
void LauncherLeaveStartNote(StartIntent intent, const char* name);

#endif  // YAX86_PICOCALC_LAUNCHER_H
