#include "status_bar.h"

#include <stdio.h>

#include "vm_config.h"

enum {
  kRowMessages = 0,
  kRowMachine,

  // CGA attributes: the foreground in the low nibble, on black.
  kAttrNormal = 0x07,
  kAttrBright = 0x0F,
  kAttrDim = 0x08,
  kAttrLit = 0x0A,
  kAttrWarning = 0x0E,
  kAttrBatteryHigh = 0x0A,
  kAttrBatteryLow = 0x0C,

  // How long a drive's light stays on after it was last used, so that a
  // single sector shows.
  kDriveLightUs = 200 * 1000,

  // Where things start on the machine's row: the drives after the longest
  // VM name, the locks, and the battery, whose widest - five cells, a space,
  // "100%" and "+" - ends at the row's end.
  kDrivesColumn = 14,
  kLocksColumn = 40,
  kBatteryCells = 5,
  kBatteryColumn = kTextColumns - kBatteryCells - 6,
  kBatteryHighPercent = 50,
  kBatteryLowPercent = 20,

  // The guest BIOS's keyboard flags.
  kFlagInsert = 0x80,
  kFlagCapsLock = 0x40,
  kFlagNumLock = 0x20,
  kFlagScrollLock = 0x10,

  // Messages waiting to be shown, and how long each is.
  kMessageQueueSize = 4,
  kMessageUs = 4 * 1000 * 1000,

  kPercent = 100,

  // Code page 437's full block, a busy drive's light; and its left half
  // block, a battery cell, which in the 4x10 font leaves two columns between
  // one cell and the next, where full blocks or small squares run together.
  kCharFullBlock = 0xDB,
  kCharBatteryCell = 0xDD,
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

static DriveLight g_lights[kStatusBarDrives];

// What the rows were last composed from. Composing takes a few snprintf()
// calls and the rows' writes, which ten times a second cost a boot about
// 0.4%, so it is skipped while none of this changes - which between a disk's
// bursts of reads is nearly always.
typedef struct ComposedFrom {
  bool lit[kStatusBarDrives];
  DiskImageStatus status[kStatusBarDrives];
  const char* vm;
  bool running;
  uint8_t keyboard_flags;
  SouthbridgeBattery battery;
  const char* hint;
  const char* problem;
  const char* message;
} ComposedFrom;

static ComposedFrom g_composed_from;
static bool g_composed;

void StatusBarPostMessage(const char* text) {
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

static void ClearRow(DisplayTextRow* row) {
  for (uint8_t i = 0; i < kTextColumns; ++i) {
    row->chars[i] = ' ';
    row->attrs[i] = kAttrNormal;
  }
}

// Put text on a row from column, clipped at its end, and return the column
// after it.
static uint8_t Put(
    DisplayTextRow* row, uint8_t column, const char* text, uint8_t attr) {
  while (*text != '\0' && column < kTextColumns) {
    row->chars[column] = *text++;
    row->attrs[column] = attr;
    ++column;
  }
  return column;
}

// Put Key=Action pairs, two spaces apart, on a row from column: each key and
// its = in key_attr, each action in action_attr. Returns the column after.
static uint8_t PutKeys(
    DisplayTextRow* row, uint8_t column, const char* text, uint8_t key_attr,
    uint8_t action_attr) {
  uint8_t attr = key_attr;
  for (const char* c = text; *c != '\0' && column < kTextColumns; ++c) {
    if (c[0] == ' ' && c[1] == ' ') {
      attr = key_attr;
    }
    row->chars[column] = *c;
    row->attrs[column] = attr;
    ++column;
    if (*c == '=') {
      attr = action_attr;
    }
  }
  return column;
}

static uint8_t PutChar(
    DisplayTextRow* row, uint8_t column, char c, uint8_t attr) {
  const char text[] = {c, '\0'};
  return Put(row, column, text, attr);
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
  column = PutChar(
      row, column, lit ? (char)kCharFullBlock : ' ',
      lit ? kAttrLit : kAttrNormal);
  return Put(row, column, label, lit ? kAttrBright : kAttrNormal);
}

// The messages row: a problem, the menu's keys, a message, or with nothing
// else and a VM running, the menu's own key, dim, it being always there.
static void ComposeMessages(
    const StatusBarState* state, const char* message, DisplayTextRow* row) {
  if (state->problem != NULL) {
    Put(row, 0, state->problem, kAttrBright);
  } else if (state->hint != NULL) {
    PutKeys(row, 0, state->hint, kAttrNormal, kAttrBright);
  } else if (message != NULL) {
    Put(row, 0, message, kAttrBright);
  } else if (state->running) {
    PutKeys(row, 0, "Shift+Shift=Menu", kAttrDim, kAttrNormal);
  }
}

// The drives the machine has, one after another: a floppy drive always, and
// a hard disk while it has an image.
static void ComposeDrives(
    const StatusBarState* state, const bool lit[kStatusBarDrives],
    DisplayTextRow* row) {
  uint8_t column = kDrivesColumn;
  for (uint8_t drive = 0; drive < kStatusBarDrives; ++drive) {
    const DiskImage* image = state->drives[drive];
    if (image == NULL ||
        (drive >= kStatusBarDriveHD1 && image->status != kDiskImageOpen)) {
      continue;
    }
    column = PutDrive(row, column, kVMDriveLabels[drive], lit[drive]);
  }
}

static void ComposeBattery(
    const SouthbridgeBattery* battery, DisplayTextRow* row) {
  if (!battery->known) {
    return;
  }
  const uint8_t attr = battery->percent > kBatteryHighPercent ? kAttrBatteryHigh
                       : battery->percent >= kBatteryLowPercent
                           ? kAttrWarning
                           : kAttrBatteryLow;
  // A cell for each fifth of a charge begun, so at least one for any charge.
  const uint8_t filled =
      (uint8_t)((battery->percent * kBatteryCells + kPercent - 1) / kPercent);
  uint8_t column = kBatteryColumn;
  for (uint8_t i = 0; i < kBatteryCells; ++i) {
    column = PutChar(
        row, column, (char)kCharBatteryCell, i < filled ? attr : kAttrDim);
  }
  char text[8];
  snprintf(
      text, sizeof(text), " %u%%%s", (unsigned)battery->percent,
      battery->charging ? "+" : "");
  Put(row, column, text, attr);
}

// The machine's row: the VM's name, its drives, the guest's locks that are
// on, and the battery.
static void ComposeMachine(
    const StatusBarState* state, const bool lit[kStatusBarDrives],
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
    Put(row, 0, state->vm, kAttrNormal);
  }
  ComposeDrives(state, lit, row);
  if (state->running) {
    uint8_t column = kLocksColumn;
    for (uint8_t i = 0; i < sizeof(kLocks) / sizeof(kLocks[0]); ++i) {
      if (state->keyboard_flags & kLocks[i].flag) {
        column = Put(row, column, kLocks[i].name, kAttrBright);
        ++column;
      }
    }
  }
  ComposeBattery(&state->battery, row);
}

static bool ComposedFromEqual(const ComposedFrom* a, const ComposedFrom* b) {
  for (uint8_t drive = 0; drive < kStatusBarDrives; ++drive) {
    if (a->lit[drive] != b->lit[drive] ||
        a->status[drive] != b->status[drive]) {
      return false;
    }
  }
  return a->vm == b->vm && a->running == b->running &&
         a->keyboard_flags == b->keyboard_flags &&
         a->battery.known == b->battery.known &&
         a->battery.percent == b->battery.percent &&
         a->battery.charging == b->battery.charging && a->hint == b->hint &&
         a->problem == b->problem && a->message == b->message;
}

bool StatusBarCompose(
    const StatusBarState* state, uint64_t now_us, DisplayStatus* rows) {
  ComposedFrom from;
  for (uint8_t drive = 0; drive < kStatusBarDrives; ++drive) {
    const DiskImage* image = state->drives[drive];
    from.lit[drive] =
        image != NULL && DriveLit(&g_lights[drive], image, now_us);
    from.status[drive] = image != NULL ? image->status : kDiskImageEjected;
  }
  from.vm = state->vm;
  from.running = state->running;
  from.keyboard_flags = state->keyboard_flags;
  from.battery = state->battery;
  from.hint = state->hint;
  from.problem = state->problem;
  from.message = CurrentMessage(now_us);
  if (g_composed && ComposedFromEqual(&from, &g_composed_from)) {
    return false;
  }
  g_composed_from = from;
  g_composed = true;
  for (uint8_t row = 0; row < kDisplayStatusRows; ++row) {
    ClearRow(&rows->rows[row]);
  }
  ComposeMessages(state, from.message, &rows->rows[kRowMessages]);
  ComposeMachine(state, from.lit, &rows->rows[kRowMachine]);
  return true;
}
