// The status rows in emulator mode, while a VM runs:
//
//   Inserted GAMES.IMG in FD2
//   DOS330        FD1  FD2 ■HD1            CAPS                     ▮▮▮▮▯ 78%+
//
// The messages row has a few seconds each of what happened, the menu's keys
// while it is open, or a problem in their place; and otherwise the menu's
// own key, dim. The machine's row has the VM's name, its drives, the guest's
// locks that are on, and the battery.
//
// A drive is its label alone, grey, with a green block before it and the
// label bright white while it is in use, as a PC's front panel lights a
// drive. A drive the machine does not have is left out. The images' names
// are on the status page and Edit drives.
#ifndef YAX86_PICOCALC_EMULATOR_STATUS_H
#define YAX86_PICOCALC_EMULATOR_STATUS_H

#include <stdbool.h>
#include <stdint.h>

#include "disk_image.h"
#include "southbridge.h"

enum {
  // Floppy disks 1 and 2 and hard disks 1 and 2, in the order of a VM's
  // drives.
  kEmulatorStatusDrives = 4,
  kEmulatorStatusDriveHD1 = 2,
  kEmulatorStatusDriveHD2 = 3,
};

// What the rows show, gathered by the emulator.
typedef struct EmulatorStatusState {
  // The drives' images, in the order of a VM's drives, or NULL for a drive
  // the machine does not have - floppy disk 2 disabled. A hard disk is shown
  // only while it has an image open, as one with none is not there either;
  // a floppy drive always.
  const DiskImage* drives[kEmulatorStatusDrives];
  // The VM's name, or NULL without a VM.
  const char* vm;
  // The guest BIOS's keyboard flags, at 0040:0017, which hold its lock and
  // insert states.
  uint8_t keyboard_flags;
  SouthbridgeBattery battery;
  // Keys to show on the message row in place of messages, such as the
  // menu's while it is open, or NULL.
  const char* hint;
} EmulatorStatusState;

// Queue a message for the message row, shown for a few seconds after the
// ones before it. A message that does not fit in the queue is dropped.
void EmulatorStatusPostMessage(const char* text);

// Compose the rows from state, and hand them to core 1 if they changed. Call
// often - every 100ms - so that a drive's light comes on while it is in use.
void EmulatorStatusOffer(const EmulatorStatusState* state);

#endif  // YAX86_PICOCALC_EMULATOR_STATUS_H
