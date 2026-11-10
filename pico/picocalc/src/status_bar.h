// The status rows at the bottom of the panel, composed on core 0 and drawn by
// core 1:
//
//   row 29  messages, a few seconds each, and the menu's keys while it is open
//   row 30  the drives: each one's letter, lit while it is in use, and image
//   row 31  the machine's name, the guest's lock flags, its time of day, the
//           menu key, the battery
//
//   Inserted GAMES.IMG in drive A
//   A:DISK01.IMG  B:empty  C:HDD.IMG
//   DOS33             CAPS NUM SCRL INS     12:34:56     ↑+↑ Menu  [■■■ ] 78%+
#ifndef YAX86_PICOCALC_STATUS_BAR_H
#define YAX86_PICOCALC_STATUS_BAR_H

#include <stdbool.h>
#include <stdint.h>

#include "disk_image.h"
#include "display.h"
#include "southbridge.h"

enum {
  // Drives A, B, C and D.
  kStatusBarDrives = 4,
  kStatusBarDriveC = 2,
  kStatusBarDriveD = 3,
};

// What the status rows show, gathered by the caller.
typedef struct StatusBarState {
  // The drives' images, A to D, which are shown when they are open. Without
  // one, drive A has the floppy built into flash or is empty, drive B is
  // empty, and drives C and D are not shown. NULL, before the machine has
  // drives, shows nothing.
  const DiskImage* drives[kStatusBarDrives];
  // The machine's name, or NULL without a machine.
  const char* machine;
  // The guest BIOS's keyboard flags, at 0040:0017, which hold its lock and
  // insert states.
  uint8_t keyboard_flags;
  // The guest's time of day, in seconds since midnight.
  uint32_t guest_seconds;
  SouthbridgeBattery battery;
  // How fast the machine has run against real time over the last second, in
  // percent, while it has been falling behind; 0 otherwise.
  uint32_t slow_percent;
  // Keys to show on the message row in place of messages, such as the menu's
  // while it is open, or NULL.
  const char* hint;
} StatusBarState;

// Queue a message for the message row, shown for a few seconds after the
// ones before it. A message that does not fit in the queue is dropped.
void StatusBarPostMessage(const char* text);

// Compose the rows for state at now_us into rows, and return whether they
// differ from the last ones composed. Call often - every 100ms - so that a
// drive's light comes on while it is in use.
bool StatusBarCompose(
    const StatusBarState* state, uint64_t now_us, DisplayStatus* rows);

#endif  // YAX86_PICOCALC_STATUS_BAR_H
