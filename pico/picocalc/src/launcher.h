// The list of VMs at power on, and what can be done from it: starting a VM,
// managing VMs, sharing the card or an image over USB, and choosing a font.
// main.c runs the list's loop, answering commands, updating the status rows
// and drawing the menu, and the launcher decides what the list does: which
// VM starts, when the countdown ends, and what a key leads to.
#ifndef YAX86_PICOCALC_LAUNCHER_H
#define YAX86_PICOCALC_LAUNCHER_H

#include <stdbool.h>
#include <stdint.h>

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

// The status rows while the list of VMs shows: the list's keys, or why
// a VM did not start, and the battery. There are no drives yet.
void LauncherUpdateRows(void);

// Lend the launcher copy_buffer, kCopyBufferSize bytes of guest RAM, which
// copying a VM's files borrows before the VM starts.
void LauncherInit(uint8_t* copy_buffer);

// The VMs on the card, and the card's settings, as last read.
const VMList* LauncherVMs(void);
const VMSettings* LauncherSettings(void);

// Read the card's VMs and settings, and the note a reset or power off left,
// and either take a VM at once into *vm or show the list.
LauncherStartResult LauncherStart(VMConfig* vm);

// A key pressed while the list shows: stops the countdown, and carries out
// what the menu makes of it. Returns whether it chose a VM to start.
bool LauncherHandleKey(SouthbridgeKeyEvent key);

// Count down to the default VM, showing the seconds left. Returns whether
// the countdown has run out, which chooses it.
bool LauncherCountDown(void);

// Take the VM chosen into *vm, if its configuration reads, and stop showing
// the list. Returns false, with its problem shown on the list, if it does
// not.
bool LauncherTakeChosen(VMConfig* vm);

// Whether the list, or a page reached from it, shows.
bool LauncherIsOpen(void);

// Draw what shows if it has changed. Call every pass of the loop while the
// list shows.
void LauncherDraw(uint64_t now_us);

// The page showing, as the report gives it.
const char* LauncherPageName(void);

// For the list's pages and what is opened from them: show why something
// could not be done, in place of the keys, until another key is pressed;
// and clear it, as a key does, returning whether there was one. The text is
// copied.
void LauncherSetProblem(const char* problem);
bool LauncherClearProblem(void);

// For Manage VMs, after it has changed the card: read the card's VMs and
// settings afresh.
void LauncherReadCard(void);

// Restart the board, leaving a note for the start after it, with name the
// VM's: to start it again at once, or to show the list with it highlighted.
void LauncherLeaveStartNote(StartIntent intent, const char* name);

#endif  // YAX86_PICOCALC_LAUNCHER_H
