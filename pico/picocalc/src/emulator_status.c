#include "emulator_status.h"

#include <stddef.h>
#include <stdio.h>

#include "display.h"
#include "pico/time.h"
#include "status_bar.h"
#include "vm_config.h"

enum {
  // How long a drive's light stays on after it was last used, so that a
  // single sector shows.
  kDriveLightUs = 200 * 1000,

  // Where things start on the machine's row: the drives after the longest
  // VM name, and the locks.
  kDrivesColumn = 14,
  kLocksColumn = 40,

  // The guest BIOS's keyboard flags.
  kFlagInsert = 0x80,
  kFlagCapsLock = 0x40,
  kFlagNumLock = 0x20,
  kFlagScrollLock = 0x10,

  // Messages waiting to be shown, and how long each is.
  kMessageQueueSize = 4,
  kMessageUs = 4 * 1000 * 1000,

  // Code page 437's full block, a busy drive's light.
  kCharFullBlock = 0xDB,
};

// A drive's light: how much it had done at the last look, and until when the
// light stays on.
typedef struct DriveLight {
  uint32_t sectors;
  uint64_t lit_until_us;
} DriveLight;

static char g_messages[kMessageQueueSize][kTextColumns + 1];
static uint8_t g_message_head;
static uint8_t g_message_count;
// When the message at the head of the queue was first shown, or 0 if it has
// not been yet.
static uint64_t g_message_shown_us;

static DriveLight g_lights[kEmulatorStatusDrives];

// What the rows were last composed from. Composing takes a few snprintf()
// calls and the rows' writes, which ten times a second cost a boot about
// 0.4%, so it is skipped while none of this changes - which between a disk's
// bursts of reads is nearly always.
typedef struct ComposedFrom {
  bool lit[kEmulatorStatusDrives];
  DiskImageStatus status[kEmulatorStatusDrives];
  const char* vm;
  uint8_t keyboard_flags;
  SouthbridgeBattery battery;
  const char* hint;
  const char* problem;
  const char* message;
} ComposedFrom;

static ComposedFrom g_composed_from;
static bool g_composed;

void EmulatorStatusPostMessage(const char* text) {
  if (g_message_count == kMessageQueueSize) {
    return;
  }
  char* slot =
      g_messages[(g_message_head + g_message_count) % kMessageQueueSize];
  snprintf(slot, kTextColumns + 1, "%s", text);
  ++g_message_count;
}

// The message to show at now_us, or NULL, moving on from one that has been
// shown long enough.
static const char* CurrentMessage(uint64_t now_us) {
  while (g_message_count > 0) {
    if (g_message_shown_us == 0) {
      g_message_shown_us = now_us;
    }
    if (now_us - g_message_shown_us < kMessageUs) {
      return g_messages[g_message_head];
    }
    g_message_head = (uint8_t)((g_message_head + 1) % kMessageQueueSize);
    --g_message_count;
    g_message_shown_us = 0;
  }
  return NULL;
}

// Whether a drive's light is on at now_us, which it is for a while after the
// image last read or wrote a sector.
static bool DriveLit(
    DriveLight* light, const DiskImage* image, uint64_t now_us) {
  if (image->status != kDiskImageOpen) {
    return false;
  }
  const uint32_t sectors =
      image->stats.sector_reads + image->stats.sector_writes;
  if (sectors != light->sectors) {
    light->sectors = sectors;
    light->lit_until_us = now_us + kDriveLightUs;
  }
  return now_us < light->lit_until_us;
}

// A drive: a space and its label in grey, or while it is in use, a green
// block and its label bright white - after a space, so that a light does not
// run into the drive before it. Returns the column after it.
static uint8_t PutDrive(
    DisplayTextRow* row, uint8_t column, const char* label, bool lit) {
  ++column;
  const char light[] = {lit ? (char)kCharFullBlock : ' ', '\0'};
  column = StatusBarPut(
      row, column, light, lit ? kStatusBarAttrLit : kStatusBarAttrNormal);
  return StatusBarPut(
      row, column, label, lit ? kStatusBarAttrBright : kStatusBarAttrNormal);
}

// The messages row: a problem, the menu's keys, a message, or with nothing
// else the menu's own key, dim, it being always there.
static void ComposeMessages(const ComposedFrom* from, DisplayTextRow* row) {
  if (from->problem != NULL) {
    StatusBarPut(row, 0, from->problem, kStatusBarAttrBright);
  } else if (from->hint != NULL) {
    StatusBarPutKeys(
        row, 0, from->hint, kStatusBarAttrNormal, kStatusBarAttrBright);
  } else if (from->message != NULL) {
    StatusBarPut(row, 0, from->message, kStatusBarAttrBright);
  } else {
    StatusBarPutKeys(
        row, 0, "Shift+Shift=Menu", kStatusBarAttrDim, kStatusBarAttrNormal);
  }
}

// The drives the machine has, one after another: a floppy drive always, and
// a hard disk while it has an image.
static void ComposeDrives(
    const EmulatorStatusState* state, const bool lit[kEmulatorStatusDrives],
    DisplayTextRow* row) {
  uint8_t column = kDrivesColumn;
  for (uint8_t drive = 0; drive < kEmulatorStatusDrives; ++drive) {
    const DiskImage* image = state->drives[drive];
    if (image == NULL ||
        (drive >= kEmulatorStatusDriveHD1 && image->status != kDiskImageOpen)) {
      continue;
    }
    column = PutDrive(row, column, kVMDriveLabels[drive], lit[drive]);
  }
}

// The machine's row: the VM's name, its drives, the guest's locks that are
// on, and the battery.
static void ComposeMachine(
    const EmulatorStatusState* state, const bool lit[kEmulatorStatusDrives],
    DisplayTextRow* row) {
  static const struct {
    const char* name;
    uint8_t flag;
  } kLocks[] = {
      {"CAPS", kFlagCapsLock},
      {"NUM", kFlagNumLock},
      {"SCRL", kFlagScrollLock},
      {"INS", kFlagInsert},
  };
  if (state->vm != NULL) {
    StatusBarPut(row, 0, state->vm, kStatusBarAttrNormal);
  }
  ComposeDrives(state, lit, row);
  uint8_t column = kLocksColumn;
  for (uint8_t i = 0; i < sizeof(kLocks) / sizeof(kLocks[0]); ++i) {
    if (state->keyboard_flags & kLocks[i].flag) {
      column = StatusBarPut(row, column, kLocks[i].name, kStatusBarAttrBright);
      ++column;
    }
  }
  StatusBarPutBattery(&state->battery, row);
}

static bool ComposedFromEqual(const ComposedFrom* a, const ComposedFrom* b) {
  for (uint8_t drive = 0; drive < kEmulatorStatusDrives; ++drive) {
    if (a->lit[drive] != b->lit[drive] ||
        a->status[drive] != b->status[drive]) {
      return false;
    }
  }
  return a->vm == b->vm && a->keyboard_flags == b->keyboard_flags &&
         a->battery.known == b->battery.known &&
         a->battery.percent == b->battery.percent &&
         a->battery.charging == b->battery.charging && a->hint == b->hint &&
         a->problem == b->problem && a->message == b->message;
}

void EmulatorStatusOffer(const EmulatorStatusState* state) {
  const uint64_t now_us = time_us_64();
  ComposedFrom from;
  for (uint8_t drive = 0; drive < kEmulatorStatusDrives; ++drive) {
    const DiskImage* image = state->drives[drive];
    from.lit[drive] =
        image != NULL && DriveLit(&g_lights[drive], image, now_us);
    from.status[drive] = image != NULL ? image->status : kDiskImageEjected;
  }
  from.vm = state->vm;
  from.keyboard_flags = state->keyboard_flags;
  from.battery = state->battery;
  from.hint = state->hint;
  from.problem = StatusBarProblem();
  from.message = CurrentMessage(now_us);
  const bool composed =
      !g_composed || !ComposedFromEqual(&from, &g_composed_from);
  if (composed) {
    g_composed_from = from;
    g_composed = true;
    DisplayStatus* rows = StatusBarRows();
    for (uint8_t row = 0; row < kDisplayStatusRows; ++row) {
      StatusBarClearRow(&rows->rows[row]);
    }
    ComposeMessages(&from, &rows->rows[kStatusBarRowMessages]);
    ComposeMachine(state, from.lit, &rows->rows[kStatusBarRowMachine]);
  }
  StatusBarHandOver(composed);
}
