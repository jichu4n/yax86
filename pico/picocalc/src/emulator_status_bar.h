// The status rows in emulator mode, while a VM runs:
//
//   Inserted GAMES.IMG in FD2
//   DOS330        FD1  FD2 ■HD1            CAPS                     ▮▮▮▮▯ 78%+
//
// The messages row has a few seconds each of what happened, the menu's keys
// while it is open, or a problem in their place; and otherwise the menu's
// own key, dim. The devices row has the VM's name, its drives, the guest's
// locks that are on, and the battery.
//
// A drive is its label alone, grey, with a green block before it and the
// label bright white while it is in use, as a PC's front panel lights a
// drive. A drive the machine does not have is left out. The images' names
// are on the status page and in a VM's configuration.
#ifndef YAX86_PICOCALC_EMULATOR_STATUS_BAR_H
#define YAX86_PICOCALC_EMULATOR_STATUS_BAR_H

#include <stdbool.h>
#include <stdint.h>

#include "disk_image.h"
#include "southbridge.h"
#include "vm_config.h"

// What the emulator gathers for the rows each time it updates them.
typedef struct EmulatorStatusBarInputs {
  // The drives' images, in the order of a VM's drives, or NULL for a drive
  // the machine does not have, such as floppy disk 2 when it is disabled. A
  // hard disk is shown only while it has an image open, since one with none
  // is not there either. A floppy drive is always shown.
  const DiskImage* drives[kNumVMDrives];
  // The VM's name, or NULL without a VM.
  const char* vm;
  // The guest BIOS's keyboard flags, at 0040:0017, which hold its lock and
  // insert states.
  uint8_t keyboard_flags;
  // The battery's charge, as the keyboard controller reports it.
  SouthbridgeBattery battery;
  // Keys to show on the message row in place of messages, such as the
  // menu's while it is open, or NULL.
  const char* hint;
} EmulatorStatusBarInputs;

// Queue a message for the message row, shown for a few seconds after the
// ones before it. A message that does not fit in the queue is dropped.
void EmulatorStatusBarPostMessage(const char* text);

// Bring the rows up to date with inputs: compose them again if what they
// show has changed, and send them to core 1. Call it often, every 100ms, so
// that a drive's light comes on while the drive is in use.
void EmulatorStatusBarUpdate(const EmulatorStatusBarInputs* inputs);

#endif  // YAX86_PICOCALC_EMULATOR_STATUS_BAR_H
