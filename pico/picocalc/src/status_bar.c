#include "status_bar.h"

#include <stdio.h>

enum {
  kRowMessages = 0,
  kRowDrives,
  kRowIndicators,

  // CGA attributes: the foreground in the low nibble, on black.
  kAttrNormal = 0x07,
  kAttrBright = 0x0F,
  kAttrDim = 0x08,
  kAttrLit = 0x0A,
  kAttrWarning = 0x0E,
  kAttrBatteryHigh = 0x0A,
  kAttrBatteryLow = 0x0C,

  // The drives follow each other along their row, this many columns apart.
  kDriveGapColumns = 2,
  // How long a drive's light stays on after it was last used, so that a
  // single sector shows.
  kDriveLightUs = 200 * 1000,

  // Where the indicators start on their row.
  kLocksColumn = 18,
  kClockColumn = 40,
  kMenuKeyColumn = 56,
  kBatteryColumn = 67,
  kBatteryCells = 4,
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

  kSecondsPerMinute = 60,
  kSecondsPerHour = 60 * 60,
  kSecondsPerDay = 24 * 60 * 60,
  kPercent = 100,

  // Code page 437: the full block and light shade the battery is drawn in,
  // and the up arrow that stands for Shift.
  kCharFullBlock = 0xDB,
  kCharLightShade = 0xB0,
  kCharShift = 0x18,
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
// calls and three rows' worth of writes, which ten times a second cost a boot
// about 0.4%, so it is skipped while none of this changes - which between a
// disk's bursts of reads is everything but the clock.
typedef struct ComposedFrom {
  bool lit[kStatusBarDrives];
  DiskImageStatus status[kStatusBarDrives];
  uint8_t keyboard_flags;
  uint32_t guest_seconds;
  SouthbridgeBattery battery;
  uint32_t slow_percent;
  const char* hint;
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
  uint8_t length = 0;
  while (text[length] != '\0' && length < kTextColumns) {
    slot[length] = text[length];
    ++length;
  }
  slot[length] = '\0';
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

// A drive: its letter, lit while in use, and its image's name, or what it has
// instead. Returns the column after it.
static uint8_t PutDrive(
    DisplayTextRow* row, uint8_t column, char letter, const DiskImage* image,
    bool lit) {
  const bool open = image->status == kDiskImageOpen;
  column = PutChar(row, column, letter, lit ? kAttrLit : kAttrNormal);
  column = PutChar(row, column, ':', kAttrNormal);
  if (open) {
    char name[kDiskImageNameLength + 1];
    DiskImageName(image->path, name);
    return Put(row, column, name, kAttrNormal);
  }
  return Put(
      row, column, image->status == kDiskImageEjected ? "empty" : "built-in",
      kAttrNormal);
}

static void ComposeMessages(
    const StatusBarState* state, const char* message, DisplayTextRow* row) {
  if (state->hint != NULL) {
    Put(row, 0, state->hint, kAttrNormal);
  } else if (message != NULL) {
    Put(row, 0, message, kAttrBright);
  } else if (state->slow_percent > 0) {
    char text[kTextColumns + 1];
    snprintf(
        text, sizeof(text), "SLOW %lu%% - running behind real time",
        (unsigned long)state->slow_percent);
    Put(row, 0, text, kAttrWarning);
  }
}

static void ComposeDrives(
    const StatusBarState* state, const bool lit[kStatusBarDrives],
    DisplayTextRow* row) {
  uint8_t column = 0;
  for (uint8_t drive = 0; drive < kStatusBarDrives; ++drive) {
    const DiskImage* image = state->drives[drive];
    if (image == NULL ||
        (drive >= kStatusBarDriveC && image->status != kDiskImageOpen)) {
      continue;
    }
    if (column > 0) {
      column = (uint8_t)(column + kDriveGapColumns);
    }
    column = PutDrive(row, column, (char)('A' + drive), image, lit[drive]);
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
  // At least one cell for any charge at all.
  const uint8_t filled =
      (uint8_t)((battery->percent * kBatteryCells + kPercent - 1) / kPercent);
  uint8_t column = PutChar(row, kBatteryColumn, '[', kAttrNormal);
  for (uint8_t i = 0; i < kBatteryCells; ++i) {
    column = PutChar(
        row, column, (char)(i < filled ? kCharFullBlock : kCharLightShade),
        attr);
  }
  column = PutChar(row, column, ']', kAttrNormal);
  char text[8];
  snprintf(
      text, sizeof(text), " %u%%%s", (unsigned)battery->percent,
      battery->charging ? "+" : "");
  Put(row, column, text, attr);
}

static void ComposeIndicators(
    const StatusBarState* state, DisplayTextRow* row) {
  static const struct {
    const char* name;
    uint8_t flag;
  } kLocks[] = {
      {"CAPS", kFlagCapsLock},
      {"NUM", kFlagNumLock},
      {"SCRL", kFlagScrollLock},
      {"INS", kFlagInsert},
  };
  uint8_t column = kLocksColumn;
  for (uint8_t i = 0; i < sizeof(kLocks) / sizeof(kLocks[0]); ++i) {
    const bool on = (state->keyboard_flags & kLocks[i].flag) != 0;
    column = Put(row, column, kLocks[i].name, on ? kAttrBright : kAttrDim);
    ++column;
  }

  const uint32_t seconds = state->guest_seconds % kSecondsPerDay;
  char clock[9];
  snprintf(
      clock, sizeof(clock), "%02lu:%02lu:%02lu",
      (unsigned long)(seconds / kSecondsPerHour),
      (unsigned long)(seconds / kSecondsPerMinute % kSecondsPerMinute),
      (unsigned long)(seconds % kSecondsPerMinute));
  Put(row, kClockColumn, clock, kAttrNormal);

  const char menu_key[] = {
      (char)kCharShift, '+', (char)kCharShift, ' ', 'M', 'e', 'n', 'u', '\0'};
  Put(row, kMenuKeyColumn, menu_key, kAttrDim);

  ComposeBattery(&state->battery, row);
}

static bool ComposedFromEqual(const ComposedFrom* a, const ComposedFrom* b) {
  for (uint8_t drive = 0; drive < kStatusBarDrives; ++drive) {
    if (a->lit[drive] != b->lit[drive] ||
        a->status[drive] != b->status[drive]) {
      return false;
    }
  }
  return a->keyboard_flags == b->keyboard_flags &&
         a->guest_seconds == b->guest_seconds &&
         a->battery.known == b->battery.known &&
         a->battery.percent == b->battery.percent &&
         a->battery.charging == b->battery.charging &&
         a->slow_percent == b->slow_percent && a->hint == b->hint &&
         a->message == b->message;
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
  from.keyboard_flags = state->keyboard_flags;
  from.guest_seconds = state->guest_seconds;
  from.battery = state->battery;
  from.slow_percent = state->slow_percent;
  from.hint = state->hint;
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
  ComposeDrives(state, from.lit, &rows->rows[kRowDrives]);
  ComposeIndicators(state, &rows->rows[kRowIndicators]);
  return true;
}
