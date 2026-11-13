// The choices for one drive on the New VM or Edit drives form: disabled,
// empty, a new blank image, or an existing image found in the browser, as
// the drive offers them.
#ifndef YAX86_PICOCALC_VM_DRIVE_SCREEN_H
#define YAX86_PICOCALC_VM_DRIVE_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "fs.h"
#include "screen.h"

// What the form has chosen for a drive.
typedef struct VMFormDrive {
  // The image in it, a path from the card's root, or empty for none.
  char path[kFSMaxPath + 1];
  // Whether it is to have a new blank image of its own instead.
  bool blank;
  // Whether the drive is there: false for floppy disk 2 or a hard disk
  // disabled.
  bool present;
} VMFormDrive;

// Show the choices for drive, one of the VM's, with what *chosen has. A
// choice is written to *chosen before the screen goes back; an image is
// found in the browser first, starting in the folder of the one chosen, if
// one is. chosen must outlive the screen.
ScreenResult VMDriveScreenOpen(uint8_t drive, VMFormDrive* chosen);

#endif  // YAX86_PICOCALC_VM_DRIVE_SCREEN_H
