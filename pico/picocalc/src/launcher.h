// Pre-boot supervisor and VM manager for the PicoCalc runtime.
//
// Shows the VM selection list at power on, manages VMs (create, edit, clone,
// rename, delete), handles USB disk mode (card and image sharing), and font
// settings before the guest VM starts.
#ifndef YAX86_PICOCALC_LAUNCHER_H
#define YAX86_PICOCALC_LAUNCHER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vm_config.h"
#include "vm_folder.h"

// What the start after a reboot is to do, left by the menu's Reset and Power
// off.
typedef enum StartIntent {
  // Start the VM again at once, without the list.
  kStartIntentRestart = 0,
  // Show the list, with the VM highlighted, and wait for a key.
  kStartIntentList,
} StartIntent;

typedef struct LauncherConfig {
  VMList* vms;
  VMSettings* settings;
  VMConfig* vm;
  bool* has_vm;
  // Guest RAM, of which copying a VM's files borrows the first 32KB before
  // the VM starts.
  uint8_t* guest_ram;
} LauncherConfig;

// Run the pre-boot supervisor to choose a VM or start the default.
void LauncherRun(const LauncherConfig* config);

// Restart the board, leaving a note for the start after it.
void LauncherLeaveStartNote(StartIntent intent, const char* name);

// Status queries used by report.c / menu.c.
bool LauncherIsOpen(void);
const char* LauncherPageName(void);

#endif  // YAX86_PICOCALC_LAUNCHER_H
