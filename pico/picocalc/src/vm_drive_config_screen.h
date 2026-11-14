// One drive's configuration, from a VM's configuration: disabled, empty, a
// new blank image, or an existing image found in the browser, as the drive
// offers them.
#ifndef YAX86_PICOCALC_VM_DRIVE_CONFIG_SCREEN_H
#define YAX86_PICOCALC_VM_DRIVE_CONFIG_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "fs.h"
#include "screen.h"

// What a VM's configuration has chosen for a drive, which can be a new
// blank image that vm.cfg could not yet name.
typedef struct VMDriveChoice {
  // The image in it, a path from the card's root, or empty for none.
  char path[kFSMaxPath + 1];
  // Whether it is to have a new blank image of its own instead.
  bool is_blank;
  // Whether the drive is there: false for floppy disk 2 or a hard disk
  // disabled.
  bool is_present;
} VMDriveChoice;

// Show the choices for drive, one of the VM's, with what *chosen has. A
// choice is written to *chosen before the screen goes back; an image is
// found in the browser first, starting in the folder of the one chosen, if
// one is. chosen must outlive the screen.
ScreenResult VMDriveConfigScreenOpen(uint8_t drive, VMDriveChoice* chosen);

#endif  // YAX86_PICOCALC_VM_DRIVE_CONFIG_SCREEN_H
