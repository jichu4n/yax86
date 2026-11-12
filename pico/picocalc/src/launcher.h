// The list of VMs at power on, and what can be done from it: starting a VM,
// managing VMs, sharing the card or an image over USB, and choosing a font.
// main.c runs it before a VM starts, and again on the start after the menu's
// Reset or Power off, which leave a note for it.
#ifndef YAX86_PICOCALC_LAUNCHER_H
#define YAX86_PICOCALC_LAUNCHER_H

#include <stdbool.h>
#include <stdint.h>

#include "vm_config.h"

// What the start after a reboot is to do, left by the menu's Reset and Power
// off.
typedef enum StartIntent {
  // Start the VM again at once, without the list.
  kStartIntentRestart = 0,
  // Show the list, with the VM highlighted, and wait for a key.
  kStartIntentList,
} StartIntent;
// main.c's state the launcher reads and fills in, all of it owned by main.c.
typedef struct LauncherConfig {
  // The VMs on the card, the card's settings, and the VM started, which
  // *has_vm says there is one of.
  VMList* vms;
  VMSettings* settings;
  VMConfig* vm;
  bool* has_vm;
  // Guest RAM, of which copying a VM's files borrows the first 32KB before
  // the VM starts.
  uint8_t* guest_ram;
} LauncherConfig;

void LauncherInit(const LauncherConfig* config);

// Show the list of VMs, and carry out what is chosen from it, until a VM is
// to start, or the board has no card and boots the built-in floppy.
void LauncherChooseVM(void);

// Restart the board, leaving a note for the start after it: to start the VM
// again at once, or to show the list with it highlighted.
void LauncherLeaveStartNote(StartIntent intent);

#endif  // YAX86_PICOCALC_LAUNCHER_H
