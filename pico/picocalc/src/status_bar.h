// The status rows at the bottom of the panel, composed on core 0 and drawn by
// core 1 - two, with no more on them than is worth a glance:
//
//   row 30  messages, a few seconds each; the menu's keys while it is open, or
//           a problem in their place; and otherwise the menu's own key
//   row 31  the VM's name, its drives, the guest's locks that are on, and the
//           battery
//
//   Inserted GAMES.IMG in FD2
//   DOS330        FD1  FD2 ■HD1            CAPS                     ▮▮▮▮▯ 78%+
//
// A drive is its label alone, grey, with a green block before it and the
// label bright white while it is in use, as a PC's front panel lights a
// drive. A drive the machine does not have is left out. The images' names
// are on the status page and in a VM's configuration.
#ifndef YAX86_PICOCALC_STATUS_BAR_H
#define YAX86_PICOCALC_STATUS_BAR_H

#include <stdbool.h>
#include <stdint.h>

#include "disk_image.h"
#include "display.h"
#include "southbridge.h"

enum {
  // Floppy disks 1 and 2 and hard disks 1 and 2, in the order of a VM's
  // drives.
  kStatusBarDrives = 4,
  kStatusBarDriveHD1 = 2,
  kStatusBarDriveHD2 = 3,
};

// What the status rows show, gathered by the caller.
typedef struct StatusBarState {
  // The drives' images, in the order of a VM's drives, or NULL for a drive
  // the machine does not have - floppy disk 2 disabled, and before a VM has
  // drives. A hard disk is shown only while it has an image open, as one with
  // none is not there either; a floppy drive always.
  const DiskImage* drives[kStatusBarDrives];
  // The VM's name, or NULL without a VM.
  const char* vm;
  // Whether a VM is running. Its locks and the menu key show only while one
  // is: the list at power on has no guest, and the menu key does nothing
  // there.
  bool running;
  // The guest BIOS's keyboard flags, at 0040:0017, which hold its lock and
  // insert states.
  uint8_t keyboard_flags;
  SouthbridgeBattery battery;
  // Keys to show on the message row in place of messages, such as the menu's
  // while it is open, or NULL: Key=Action pairs two spaces apart, the way
  // MS-DOS's own programs label theirs - "\x18\x19=Move  Enter=Select".
  const char* hint;
  // A problem to show in place of the keys, or NULL.
  const char* problem;
} StatusBarState;

// Queue a message for the message row, shown for a few seconds after the
// ones before it. A message that does not fit in the queue is dropped.
void StatusBarPostMessage(const char* text);

// Compose the rows for state at now_us into rows, and return whether they
// differ from the last ones composed. Call often - every 100ms - so that a
// drive's light comes on while it is in use.
bool StatusBarCompose(
    const StatusBarState* state, uint64_t now_us, DisplayStatus* rows);

// Compose the status rows from state, and hand them to core 1 if they
// changed, or are still waiting from the last time it was busy.
void StatusBarOffer(const StatusBarState* state);

#endif  // YAX86_PICOCALC_STATUS_BAR_H
