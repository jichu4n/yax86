#include "launcher.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "disk_image.h"
#include "display.h"
#include "hardware/watchdog.h"
#include "launcher_menu_screen.h"
#include "pico/stdlib.h"
#include "runtime_log.h"
#include "status_bar.h"
#include "text.h"
#include "ui_status.h"
#include "vm_list.h"

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
// How the launcher menu is to open: the VM chosen in it, and how long it
// counts down to starting that VM by itself, 0 for no countdown. And why it
// last closed.
static uint8_t g_chosen;
static uint32_t g_countdown_ms;
static LauncherMenuExit g_exit;

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

// Read the card's settings, use their font, and set *default_vm to the
// index of the default VM in g_vms, or 0 without one. Returns false, with the
// problem shown on the launcher menu, if the settings had one. Kept out of
// line, so that its message is on core 0's stack only while it runs rather than
// in the frames of the callers it would be inlined into.
static __noinline bool ReadSettings(uint8_t* default_vm) {
  Settings* settings = &g_settings;
  char problem[kVMProblemLength + 1];
  bool read = SettingsRead(settings, problem);
  DisplaySetFont(settings->font);
  *default_vm = 0;
  if (read && settings->boot_default[0] != '\0') {
    // Past the list's end, the first is the default instead.
    const uint8_t index = VMListFind(&g_vms, settings->boot_default);
    if (index < kVMListMax) {
      *default_vm = index;
    }
  }
  if (!read) {
    char message[kTextColumns + 1];
    snprintf(message, sizeof(message), "YAX86.CFG: %s", problem);
    UIStatusSetProblem(message);
  }
  return read;
}

// Make the VM named name the one to start at the next power on, if it is not
// already. Writing the card only when it changes keeps a board that starts
// its default every time from writing at all.
static void SaveBootDefault(const char* name) {
  if (strcasecmp(g_settings.boot_default, name) == 0) {
    return;
  }
  char problem[kVMProblemLength + 1];
  if (SettingsSaveBootDefault(&g_settings, name, problem)) {
    SYSTEM_INFO("boot_default_saved", Y_LOG_STRING("name", name));
    return;
  }
  char message[kTextColumns + 1];
  snprintf(
      message, sizeof(message), "Could not save boot_default: %.50s", problem);
  StatusBarPostMessage(message);
  SYSTEM_WARN("boot_default_not_saved", Y_LOG_STRING("problem", problem));
}

// Load the VM at index in g_vms to start it: read its vm.cfg into *config,
// and make it the default. Returns false, with its problem shown on the
// list, if it does not read.
static __noinline bool LoadChosenVM(uint8_t index, VMConfig* config) {
  const VMEntry* vm = &g_vms.vms[index];
  char problem[kVMProblemLength + 1];
  if (VMConfigRead(vm, config, problem)) {
    SYSTEM_INFO("vm_chosen", Y_LOG_STRING("path", vm->path));
    SaveBootDefault(vm->name);
    return true;
  }
  char message[kTextColumns + 1];
  snprintf(message, sizeof(message), "%s: %s", vm->name, problem);
  UIStatusSetProblem(message);
  SYSTEM_WARN(
      "vm_config_problem", Y_LOG_STRING("path", vm->path),
      Y_LOG_STRING("problem", problem));
  return false;
}

LauncherStartResult LauncherStart(VMConfig* vm) {
  StartNote note = {0};
  const bool noted = TakeStartNote(&note);
  VMListRead(&g_vms);
  uint8_t chosen = 0;
  const bool settings_read = ReadSettings(&chosen);
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
      chosen = index;
      if (note.intent == kStartIntentRestart && LoadChosenVM(index, vm)) {
        return kLauncherLoadedVM;
      }
    }
  } else if (
      settings_read && g_settings.boot_delay == 0 && g_vms.count > 0 &&
      LoadChosenVM(chosen, vm)) {
    return kLauncherLoadedVM;
  }
  g_chosen = chosen;
  const bool counting = !noted && g_settings.boot_delay > 0 && g_vms.count > 0;
  g_countdown_ms =
      counting ? (uint32_t)g_settings.boot_delay * kMillisecondsPerSecond : 0;
  return kLauncherShowsMenu;
}

ScreenResult LauncherOpenMenu(void) {
  return LauncherMenuScreenOpen(
      &g_vms, &g_settings, g_copy_buffer, kCopyBufferSize, g_chosen,
      g_countdown_ms, &g_exit);
}

bool LauncherHandleMenuExit(VMConfig* vm) {
  // The launcher menu opens again without counting down.
  g_countdown_ms = 0;
  if (g_exit.reason == kLauncherMenuExitReadCard) {
    // The host may have added VMs, removed them, or changed the settings.
    VMListRead(&g_vms);
    ReadSettings(&g_chosen);
    return false;
  }
  g_chosen = g_exit.vm;
  return LoadChosenVM(g_exit.vm, vm);
}
