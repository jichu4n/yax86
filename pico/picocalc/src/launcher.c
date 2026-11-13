#include "launcher.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "disk_image.h"
#include "display.h"
#include "fs.h"
#include "hardware/watchdog.h"
#include "launcher_menu.h"
#include "manage_screen.h"
#include "pico/stdlib.h"
#include "runtime_log.h"
#include "settings_screen.h"
#include "status_bar.h"
#include "ui_status.h"
#include "usb_storage_screen.h"
#include "vm_list.h"

enum {
  // The part of the guest's RAM a clone's files are copied through, before a
  // VM starts and the RAM is the guest's: 64 of the card's blocks, which go to
  // and from it a run at a time.
  kCopyBufferSize = 32 * 1024,
  // Written to g_start_note's marker by a reset or a power off, for the
  // start after the reboot - see LeaveStartNote().
  kStartNoteMarker = 0x5747A27E,

  kMicrosecondsPerSecond = 1000 * 1000,
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
// When the default VM starts by itself, and whether it is still counting
// down to it: a key stops it.
// Where the VM chosen from the list goes.
static VMConfig* g_chosen;
// Whether the card or an image has been on USB, so the card is read again.
static bool g_exported;
static absolute_time_t g_start_at;
static bool g_counting;

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

static __noinline bool TakeVM(uint8_t index, VMConfig* chosen) {
  const VMEntry* vm = &g_vms.vms[index];
  char problem[kVMProblemLength + 1];
  if (VMConfigRead(vm, chosen, problem)) {
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

// The card's settings, and the index of the default VM in the list.
// Returns false, with the problem shown on the list, if the settings had one.
// Kept out of line, so that its message is on core 0's stack only while it
// runs rather than in the frames of the callers it would be inlined into.
static __noinline bool ReadSettings(Settings* settings, uint8_t* highlighted) {
  char problem[kVMProblemLength + 1];
  bool read = SettingsRead(settings, problem);
  DisplaySetFont(settings->font);
  *highlighted = 0;
  if (read && settings->boot_default[0] != '\0') {
    // Past the list's end, the first is highlighted instead.
    const uint8_t index = VMListFind(&g_vms, settings->boot_default);
    if (index < kVMListMax) {
      *highlighted = index;
    }
  }
  if (!read) {
    char message[kTextColumns + 1];
    snprintf(message, sizeof(message), "YAX86.CFG: %s", problem);
    UIStatusSetProblem(message);
  }
  return read;
}

// Read the card afresh once it is back from USB, and show the list again
// without counting down: the computer may have added VMs, removed them, or
// changed the settings.
static __noinline void ReadCardAgain(void) {
  VMListRead(&g_vms);
  uint8_t highlighted = 0;
  ReadSettings(&g_settings, &highlighted);
  LauncherMenuOpen(&g_vms, highlighted);
}

void LauncherInit(uint8_t* copy_buffer) { g_copy_buffer = copy_buffer; }

const VMList* LauncherVMs(void) { return &g_vms; }

const Settings* LauncherSettings(void) { return &g_settings; }

LauncherStartResult LauncherStart(VMConfig* vm) {
  StartNote note = {0};
  const bool noted = TakeStartNote(&note);
  VMListRead(&g_vms);
  uint8_t highlighted = 0;
  const bool settings_read = ReadSettings(&g_settings, &highlighted);
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
      if (note.intent == kStartIntentRestart && TakeVM(index, vm)) {
        return kLauncherTookVM;
      }
    }
  } else if (
      settings_read && g_settings.boot_delay == 0 && g_vms.count > 0 &&
      TakeVM(highlighted, vm)) {
    return kLauncherTookVM;
  }
  LauncherMenuOpen(&g_vms, highlighted);
  g_start_at =
      make_timeout_time_ms(g_settings.boot_delay * kMillisecondsPerSecond);
  g_counting = !noted && g_settings.boot_delay > 0 && g_vms.count > 0;
  return kLauncherShowsList;
}

// Take the VM chosen, or highlighted, into *g_chosen, which closes the
// screens. If its configuration does not read, the list stays, with its
// problem shown.
static ScreenResult Choose(void) {
  if (TakeVM(LauncherMenuChosenVM(), g_chosen)) {
    return ScreenClose();
  }
  return ScreenStays();
}

// A key pressed while the list shows: stops the countdown, and carries out
// what the menu makes of it.
static ScreenResult HandleKey(SouthbridgeKeyEvent key) {
  g_counting = false;
  LauncherMenuSetCountdown(0);
  ScreenResult result = ScreenStays();
  switch (LauncherMenuHandleKey(key)) {
    case kLauncherMenuActionVMChosen:
      result = Choose();
      break;
    case kLauncherMenuActionOpenSettings:
      result = SettingsScreenOpen(&g_settings);
      break;
    case kLauncherMenuActionOpenManage:
      result =
          ManageScreenOpen(&g_vms, &g_settings, g_copy_buffer, kCopyBufferSize);
      break;
    case kLauncherMenuActionOpenUSB:
      result = USBStorageScreenOpen(&g_exported);
      break;
    default:
      break;
  }
  return result;
}

// Count down to the default VM, showing the seconds left, which chooses it
// once they run out.
static ScreenResult CountDown(void) {
  ScreenResult result = ScreenStays();
  if (g_counting) {
    const int64_t left_us =
        absolute_time_diff_us(get_absolute_time(), g_start_at);
    if (left_us <= 0) {
      g_counting = false;
      LauncherMenuSetCountdown(0);
      result = Choose();
    } else {
      LauncherMenuSetCountdown(
          (uint8_t)((left_us + kMicrosecondsPerSecond - 1) /
                    kMicrosecondsPerSecond));
    }
  }
  return result;
}

// Back from a screen opened over the list: after USB mass storage has exported
// the card or an image, read the card again.
static ScreenResult Resume(void) {
  if (g_exported) {
    g_exported = false;
    ReadCardAgain();
  } else {
    LauncherMenuResume();
  }
  return ScreenStays();
}

static const Screen kLauncherScreen = {
    .handle_key = HandleKey,
    .tick = CountDown,
    .draw = LauncherMenuDraw,
    .resume = Resume,
    .hint = LauncherMenuHint,
    .page_name = LauncherMenuPageName,
};

ScreenResult LauncherScreenOpen(VMConfig* chosen) {
  g_chosen = chosen;
  return ScreenOpen(&kLauncherScreen);
}
