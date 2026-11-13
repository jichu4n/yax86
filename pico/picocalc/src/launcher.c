#include "launcher.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "disk_image.h"
#include "hardware/watchdog.h"
#include "launcher_card.h"
#include "launcher_menu_screen.h"
#include "pico/stdlib.h"
#include "runtime_log.h"
#include "vm_list.h"
#include "vm_manager_screen.h"

enum {
  // The part of the guest's RAM a clone's files are copied through, before a
  // VM starts and the RAM is the guest's: 64 of the card's blocks, which go to
  // and from it a run at a time.
  kCopyBufferSize = 32 * 1024,
  // Written to g_start_note's marker by a reset or a power off, for the
  // start after the reboot - see LeaveStartNote().
  kStartNoteMarker = 0x5747A27E,

  kMillisecondsPerSecond = 1000,
};

typedef struct StartNote {
  // kStartNoteMarker while the note is to be acted on.
  uint32_t marker;
  StartIntent intent;
  // The VM's name.
  char name[kVMNameLength + 1];
} StartNote;

// Deliberately not zeroed at startup, so that it survives the reboot - see
// LeaveStartNote().
static StartNote __uninitialized_ram(g_start_note);

// The VMs on the card, and the card's settings.
static VMList g_vms;
static Settings g_settings;
// Guest RAM, lent before the VM starts - see LauncherInit().
static uint8_t* g_copy_buffer;
// The VM the list shows chosen, and how long it counts down to starting it
// by itself, 0 for no countdown.
static uint8_t g_highlighted;
static uint32_t g_countdown_ms;

// Restart the board, leaving a note for the start after it, which takes it
// - see TakeStartNote(). Every start, whether from power on, a reset or a power
// off, begins from the same state. Nothing is lost: the keyboard controller
// keeps its power, and the guest's disks have every sector it wrote on the
// card.
//
// The note is in RAM that startup does not clear, as g_overclock_marker is: it
// survives the reboot but not a power cycle. It is only ever set across the
// reboot, and the start after takes it whatever it does next, so a later
// reboot - picotool's, or the power button's - never finds one left from
// before.
void LauncherLeaveStartNote(StartIntent intent, const char* name) {
  g_start_note.intent = intent;
  memcpy(g_start_note.name, name, sizeof(g_start_note.name));
  g_start_note.marker = kStartNoteMarker;
  watchdog_reboot(0, 0, 0);
  for (;;) {
    tight_loop_contents();
  }
}

// Take the note a reset or power off left, if one did: returns whether there
// was one.
static bool TakeStartNote(StartNote* note) {
  if (g_start_note.marker != kStartNoteMarker) {
    return false;
  }
  g_start_note.marker = 0;
  note->intent = g_start_note.intent;
  // The name may have been left unterminated by RAM it never wrote.
  g_start_note.name[kVMNameLength] = '\0';
  memcpy(note->name, g_start_note.name, sizeof(note->name));
  return true;
}

void LauncherInit(uint8_t* copy_buffer) { g_copy_buffer = copy_buffer; }

const VMList* LauncherVMs(void) { return &g_vms; }

const Settings* LauncherSettings(void) { return &g_settings; }

LauncherStartResult LauncherStart(VMConfig* vm) {
  StartNote note = {0};
  const bool noted = TakeStartNote(&note);
  VMListRead(&g_vms);
  uint8_t highlighted = 0;
  const bool settings_read =
      LauncherCardReadSettings(&g_vms, &g_settings, &highlighted);
  // Without a card there is nothing to choose, or to put on USB.
  if (DiskImageMountCard() != kDiskImageOpen) {
    return kLauncherNoCard;
  }
  if (noted) {
    SYSTEM_INFO(
        note.intent == kStartIntentRestart ? "restarting" : "powered_off",
        Y_LOG_STRING("name", note.name));
    const uint8_t index = VMListFind(&g_vms, note.name);
    if (index < kVMListMax) {
      highlighted = index;
      if (note.intent == kStartIntentRestart &&
          LauncherCardTakeVM(&g_vms, &g_settings, index, vm)) {
        return kLauncherTookVM;
      }
    }
  } else if (
      settings_read && g_settings.boot_delay == 0 && g_vms.count > 0 &&
      LauncherCardTakeVM(&g_vms, &g_settings, highlighted, vm)) {
    return kLauncherTookVM;
  }
  g_highlighted = highlighted;
  const bool counting = !noted && g_settings.boot_delay > 0 && g_vms.count > 0;
  g_countdown_ms =
      counting ? (uint32_t)g_settings.boot_delay * kMillisecondsPerSecond : 0;
  return kLauncherShowsList;
}

ScreenResult LauncherScreenOpen(VMConfig* chosen) {
  return LauncherMenuScreenOpen(
      &g_vms, &g_settings, g_copy_buffer, kCopyBufferSize, g_highlighted,
      g_countdown_ms, chosen);
}
