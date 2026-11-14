#include "emulator_status_bar.h"

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

  // Where things start on the devices row: the drives after the longest
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
  uint32_t num_sectors;
  uint64_t lit_until_us;
} DriveLight;

static char g_messages[kMessageQueueSize][kTextColumns + 1];
static uint8_t g_message_head;
static uint8_t g_num_messages;
// When the message at the head of the queue was first shown, or 0 if it has
// not been yet.
static uint64_t g_message_shown_us;

static DriveLight g_lights[kNumVMDrives];

// What the rows show, worked out from the inputs. The rows are composed
// again only when this changes. Composing takes a few snprintf() calls and
// the rows' writes, which ten times a second cost a boot about 0.4%. Between
// a disk's bursts of reads, nothing here changes.
typedef struct RowContents {
  // Whether each drive's light is on, and whether its image is open.
  bool is_lit[kNumVMDrives];
  DiskImageStatus status[kNumVMDrives];
  // The VM's name, or NULL without a VM.
  const char* vm;
  // The guest BIOS's keyboard flags, which hold its lock states.
  uint8_t keyboard_flags;
  // The battery's charge.
  SouthbridgeBattery battery;
  // The keys shown in place of messages, or NULL.
  const char* hint;
  // The problem showing, or NULL. A new problem is a new pointer.
  const char* problem;
  // The message showing, or NULL. It points into the queue, so a new message
  // is a new pointer.
  const char* message;
} RowContents;

// What the rows showed when they were last composed, and whether they have
// been.
static RowContents g_last_contents;
static bool g_has_last_contents;

void EmulatorStatusBarPostMessage(const char* text) {
  if (g_num_messages == kMessageQueueSize) {
    return;
  }
  char* slot =
      g_messages[(g_message_head + g_num_messages) % kMessageQueueSize];
  snprintf(slot, kTextColumns + 1, "%s", text);
  ++g_num_messages;
}

// The message to show at now_us, or NULL, moving on from one that has been
// shown long enough.
static const char* CurrentMessage(uint64_t now_us) {
  while (g_num_messages > 0) {
    if (g_message_shown_us == 0) {
      g_message_shown_us = now_us;
    }
    if (now_us - g_message_shown_us < kMessageUs) {
      return g_messages[g_message_head];
    }
    g_message_head = (uint8_t)((g_message_head + 1) % kMessageQueueSize);
    --g_num_messages;
    g_message_shown_us = 0;
  }
  return NULL;
}

// Whether a drive's light is on at now_us, which it is for a while after the
// image last read or wrote a sector.
static bool IsDriveLit(
    DriveLight* light, const DiskImage* image, uint64_t now_us) {
  if (image->status != kDiskImageOpen) {
    return false;
  }
  const uint32_t num_sectors =
      image->stats.num_sector_reads + image->stats.num_sector_writes;
  if (num_sectors != light->num_sectors) {
    light->num_sectors = num_sectors;
    light->lit_until_us = now_us + kDriveLightUs;
  }
  return now_us < light->lit_until_us;
}

// A drive: a space and its label in grey, or while it is in use, a green
// block and its label bright white - after a space, so that a light does not
// run into the drive before it. Returns the column after it.
static uint8_t PutDrive(
    DisplayTextRow* row, uint8_t column, const char* label, bool is_lit) {
  ++column;
  const char light[] = {is_lit ? (char)kCharFullBlock : ' ', '\0'};
  column = StatusBarPut(
      row, column, light, is_lit ? kStatusBarAttrLit : kStatusBarAttrNormal);
  return StatusBarPut(
      row, column, label, is_lit ? kStatusBarAttrBright : kStatusBarAttrNormal);
}

// The messages row: a problem, the menu's keys, a message, or with nothing
// else the menu's own key, dim, it being always there.
static void ComposeMessages(const RowContents* contents, DisplayTextRow* row) {
  if (contents->problem != NULL) {
    StatusBarPut(row, 0, contents->problem, kStatusBarAttrBright);
  } else if (contents->hint != NULL) {
    StatusBarPutKeys(
        row, 0, contents->hint, kStatusBarAttrNormal, kStatusBarAttrBright);
  } else if (contents->message != NULL) {
    StatusBarPut(row, 0, contents->message, kStatusBarAttrBright);
  } else {
    StatusBarPutKeys(
        row, 0, "Shift+Shift=Menu", kStatusBarAttrDim, kStatusBarAttrNormal);
  }
}

// The drives the machine has, one after another: a floppy drive always, and
// a hard disk while it has an image.
static void ComposeDrives(
    const EmulatorStatusBarInputs* inputs, const bool is_lit[kNumVMDrives],
    DisplayTextRow* row) {
  uint8_t column = kDrivesColumn;
  for (uint8_t drive = 0; drive < kNumVMDrives; ++drive) {
    const DiskImage* image = inputs->drives[drive];
    if (image == NULL ||
        (drive >= kVMDriveHD1 && image->status != kDiskImageOpen)) {
      continue;
    }
    column = PutDrive(row, column, kVMDriveLabels[drive], is_lit[drive]);
  }
}

// The devices row: the VM's name, its drives, the guest's locks that are
// on, and the battery.
static void ComposeDevices(
    const EmulatorStatusBarInputs* inputs, const bool is_lit[kNumVMDrives],
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
  if (inputs->vm != NULL) {
    StatusBarPut(row, 0, inputs->vm, kStatusBarAttrNormal);
  }
  ComposeDrives(inputs, is_lit, row);
  uint8_t column = kLocksColumn;
  for (uint8_t i = 0; i < sizeof(kLocks) / sizeof(kLocks[0]); ++i) {
    if (inputs->keyboard_flags & kLocks[i].flag) {
      column = StatusBarPut(row, column, kLocks[i].name, kStatusBarAttrBright);
      ++column;
    }
  }
  StatusBarPutBattery(&inputs->battery, row);
}

static bool AreRowContentsEqual(const RowContents* a, const RowContents* b) {
  for (uint8_t drive = 0; drive < kNumVMDrives; ++drive) {
    if (a->is_lit[drive] != b->is_lit[drive] ||
        a->status[drive] != b->status[drive]) {
      return false;
    }
  }
  return a->vm == b->vm && a->keyboard_flags == b->keyboard_flags &&
         a->battery.is_known == b->battery.is_known &&
         a->battery.percent == b->battery.percent &&
         a->battery.is_charging == b->battery.is_charging &&
         a->hint == b->hint && a->problem == b->problem &&
         a->message == b->message;
}

void EmulatorStatusBarUpdate(const EmulatorStatusBarInputs* inputs) {
  const uint64_t now_us = time_us_64();
  RowContents contents;
  for (uint8_t drive = 0; drive < kNumVMDrives; ++drive) {
    const DiskImage* image = inputs->drives[drive];
    contents.is_lit[drive] =
        image != NULL && IsDriveLit(&g_lights[drive], image, now_us);
    contents.status[drive] = image != NULL ? image->status : kDiskImageEjected;
  }
  contents.vm = inputs->vm;
  contents.keyboard_flags = inputs->keyboard_flags;
  contents.battery = inputs->battery;
  contents.hint = inputs->hint;
  contents.problem = StatusBarProblem();
  contents.message = CurrentMessage(now_us);
  const bool did_change =
      !g_has_last_contents || !AreRowContentsEqual(&contents, &g_last_contents);
  if (did_change) {
    g_last_contents = contents;
    g_has_last_contents = true;
    DisplayStatus* rows = StatusBarRows();
    for (uint8_t row = 0; row < kDisplayNumStatusRows; ++row) {
      StatusBarClearRow(&rows->rows[row]);
    }
    ComposeMessages(&contents, &rows->rows[kStatusBarRowMessages]);
    ComposeDevices(inputs, contents.is_lit, &rows->rows[kStatusBarRowDevices]);
  }
  StatusBarSend(did_change);
}
