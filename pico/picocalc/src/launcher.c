#include "launcher.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "disk_image.h"
#include "display.h"
#include "font_4x10.h"
#include "fs.h"
#include "hardware/watchdog.h"
#include "launcher_menu.h"
#include "launcher_settings.h"
#include "launcher_usb.h"
#include "pico/stdlib.h"
#include "runtime_log.h"
#include "status_bar.h"
#include "vm_folder.h"

enum {
  // The part of the guest's RAM a clone's files are copied through, before a
  // VM starts and the RAM is the guest's: 64 of the card's blocks, which go to
  // and from it a run at a time.
  kCopyBufferSize = 32 * 1024,
  // Written to g_start_note's marker by a reset or a power off, for the
  // start after the reboot - see LeaveStartNote().
  kStartNoteMarker = 0x5747A27E,

  kBytesPerKB = 1024,
  kMicrosecondsPerMillisecond = 1000,
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
static VMSettings g_settings;
// Guest RAM, lent before the VM starts - see LauncherInit().
static uint8_t* g_copy_buffer;
// When the default VM starts by itself, and whether it is still counting
// down to it: a key stops it.
static absolute_time_t g_start_at;
static bool g_counting;
// Whether the list shows, and what is open from it, if anything.
typedef enum Feature {
  kFeatureNone = 0,
  kFeatureUsb,
  kFeatureSettings,
} Feature;
static bool g_open;
static Feature g_feature;
// Why something on the list could not be done, shown in place of the keys
// until another key is pressed. It is kept in two buffers, used in turn, so
// that a new one is a new pointer: the status rows tell a change of hint by
// its pointer.
static char g_problems[2][kTextColumns + 1];
static char* g_problem = g_problems[0];

void LauncherSetProblem(const char* problem) {
  g_problem = g_problem == g_problems[0] ? g_problems[1] : g_problems[0];
  snprintf(g_problem, kTextColumns + 1, "%s", problem != NULL ? problem : "");
}

bool LauncherClearProblem(void) {
  if (g_problem[0] == '\0') {
    return false;
  }
  g_problem[0] = '\0';
  return true;
}

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
  if (VMSettingsSaveBootDefault(name, problem)) {
    snprintf(
        g_settings.boot_default, sizeof(g_settings.boot_default), "%s", name);
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
  LauncherSetProblem(message);
  SYSTEM_WARN(
      "vm_config_problem", Y_LOG_STRING("path", vm->path),
      Y_LOG_STRING("problem", problem));
  return false;
}

// The card's settings, and the index of the default VM in the list.
// Returns false, with the problem shown on the list, if the settings had one.
static bool ReadSettings(VMSettings* settings, uint8_t* highlighted) {
  char problem[kVMProblemLength + 1];
  bool read = VMSettingsRead(settings, problem);
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
    LauncherSetProblem(message);
    SYSTEM_WARN("settings_problem", Y_LOG_STRING("problem", problem));
  }
  return read;
}

// After Manage VMs has changed the card, read its VMs afresh, and the
// settings, and show Manage VMs again with the VM named name chosen - and
// with show_vm, its page.
static __noinline void ShowManagedVMs(const char* name, bool show_vm) {
  // name may be in the list read over.
  char chosen[kVMNameLength + 1];
  snprintf(chosen, sizeof(chosen), "%s", name);
  VMListRead(&g_vms);
  uint8_t highlighted = 0;
  ReadSettings(&g_settings, &highlighted);
  LauncherMenuShowManagedVMs(chosen, show_vm);
}

// Show why Manage VMs could not do something, and log it.
static void ShowManageProblem(
    const char* event, const VMEntry* vm, const char* problem) {
  LauncherSetProblem(problem);
  SYSTEM_WARN(
      event, Y_LOG_STRING("path", vm->path), Y_LOG_STRING("problem", problem));
}

static __noinline void RenameVM(void) {
  const VMEntry vm = g_vms.vms[LauncherMenuManagedVM()];
  VMEntry renamed;
  char problem[kVMProblemLength + 1];
  const VMFolderResult result =
      VMFolderRename(&g_vms, &vm, LauncherMenuTypedName(), &renamed, problem);
  if (result == kVMFolderRefused) {
    LauncherSetProblem(problem);
    return;
  }
  ShowManagedVMs(result == kVMFolderDone ? renamed.name : vm.name, false);
  if (result == kVMFolderDone) {
    SYSTEM_INFO(
        "vm_renamed", Y_LOG_STRING("from", vm.name),
        Y_LOG_STRING("to", renamed.name));
  } else {
    ShowManageProblem("vm_not_renamed", &vm, problem);
  }
}

// Show how far a clone has come, at once: the main loop, which would draw it,
// is waiting for the clone.
static void ShowCopyProgress(uint64_t done, uint64_t total) {
  LauncherMenuSetCopyProgress(
      (uint32_t)(done / kBytesPerKB), (uint32_t)(total / kBytesPerKB));
  LauncherUpdateRows();
  LauncherDraw(time_us_64());
}

static __noinline void CloneVM(void) {
  const VMEntry vm = g_vms.vms[LauncherMenuManagedVM()];
  VMEntry copy;
  char problem[kVMProblemLength + 1];
  const uint64_t start_us = time_us_64();
  FSTransfer transfer = {
      .buffer = g_copy_buffer,
      .buffer_size = kCopyBufferSize,
      .progress = ShowCopyProgress,
  };
  const VMFolderResult result =
      VMFolderClone(&vm, LauncherMenuTypedName(), &transfer, &copy, problem);
  // Left as the guest would find RAM at power on, whatever it held.
  memset(g_copy_buffer, 0, kCopyBufferSize);
  if (result == kVMFolderRefused) {
    LauncherSetProblem(problem);
    return;
  }
  ShowManagedVMs(result == kVMFolderDone ? copy.name : vm.name, false);
  if (result == kVMFolderDone) {
    SYSTEM_INFO(
        "vm_cloned", Y_LOG_STRING("from", vm.name),
        Y_LOG_STRING("to", copy.name),
        Y_LOG_UINT(
            "ms", (time_us_64() - start_us) / kMicrosecondsPerMillisecond));
  } else {
    ShowManageProblem("vm_not_cloned", &vm, problem);
  }
}

static __noinline void CreateVM(void) {
  VMFolderDrives drives;
  LauncherMenuFormDrives(&drives);
  VMEntry created;
  char problem[kVMProblemLength + 1];
  const uint64_t start_us = time_us_64();
  // Blank disks are written through the guest's RAM, which is zeroed after.
  FSTransfer transfer = {
      .buffer = g_copy_buffer,
      .buffer_size = kCopyBufferSize,
      .progress = ShowCopyProgress,
  };
  const VMFolderResult result = VMFolderCreate(
      LauncherMenuTypedName(), &drives, &transfer, &created, problem);
  memset(g_copy_buffer, 0, kCopyBufferSize);
  if (result == kVMFolderRefused) {
    LauncherSetProblem(problem);
    return;
  }
  ShowManagedVMs(created.name, false);
  if (result == kVMFolderDone) {
    SYSTEM_INFO(
        "vm_created", Y_LOG_STRING("name", created.name),
        Y_LOG_BOOL("blank_fd1", drives.blank[kVMDriveFD1]),
        Y_LOG_BOOL("blank_fd2", drives.blank[kVMDriveFD2]),
        Y_LOG_BOOL("blank_hd1", drives.blank[kVMDriveHD1]),
        Y_LOG_BOOL("blank_hd2", drives.blank[kVMDriveHD2]),
        Y_LOG_UINT(
            "ms", (time_us_64() - start_us) / kMicrosecondsPerMillisecond));
  } else {
    LauncherSetProblem(problem);
    SYSTEM_WARN(
        "vm_not_created", Y_LOG_STRING("name", created.name),
        Y_LOG_STRING("problem", problem));
  }
}

static __noinline void EditVM(void) {
  const VMEntry vm = g_vms.vms[LauncherMenuManagedVM()];
  VMFolderDrives drives;
  LauncherMenuFormDrives(&drives);
  char problem[kVMProblemLength + 1];
  FSTransfer transfer = {
      .buffer = g_copy_buffer,
      .buffer_size = kCopyBufferSize,
      .progress = ShowCopyProgress,
  };
  const VMFolderResult result = VMFolderEdit(&vm, &drives, &transfer, problem);
  memset(g_copy_buffer, 0, kCopyBufferSize);
  if (result == kVMFolderRefused) {
    LauncherSetProblem(problem);
    return;
  }
  ShowManagedVMs(vm.name, true);
  if (result == kVMFolderDone) {
    SYSTEM_INFO("vm_edited", Y_LOG_STRING("name", vm.name));
  } else {
    ShowManageProblem("vm_not_edited", &vm, problem);
  }
}

static __noinline void DeleteVM(void) {
  const VMEntry vm = g_vms.vms[LauncherMenuManagedVM()];
  char problem[kVMProblemLength + 1];
  const bool deleted = VMFolderDelete(&vm, problem);
  // Gone from the list, it leaves the first VM chosen.
  ShowManagedVMs(vm.name, false);
  if (deleted) {
    SYSTEM_INFO("vm_deleted", Y_LOG_STRING("name", vm.name));
  } else {
    ShowManageProblem("vm_not_deleted", &vm, problem);
  }
}

void LauncherUpdateRows(void) {
  StatusBarState state = {0};
  state.battery = SouthbridgeGetBattery();
  state.hint = g_feature == kFeatureUsb        ? LauncherUsbHint()
               : g_feature == kFeatureSettings ? LauncherSettingsHint()
                                               : LauncherMenuHint();
  state.problem = g_problem[0] != '\0' ? g_problem : NULL;
  StatusBarOffer(&state);
}

void LauncherInit(uint8_t* copy_buffer) { g_copy_buffer = copy_buffer; }

const VMList* LauncherVMs(void) { return &g_vms; }

const VMSettings* LauncherSettings(void) { return &g_settings; }

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
  g_open = true;
  g_start_at =
      make_timeout_time_ms(g_settings.boot_delay * kMillisecondsPerSecond);
  g_counting = !noted && g_settings.boot_delay > 0 && g_vms.count > 0;
  return kLauncherShowsList;
}

// A key while USB disk mode is open: back to the list when it is left, and
// the card read afresh when it is back from USB.
static void HandleUsbKey(SouthbridgeKeyEvent key) {
  switch (LauncherUsbHandleKey(key)) {
    case kLauncherUsbStays:
      break;
    case kLauncherUsbLeft:
      g_feature = kFeatureNone;
      LauncherMenuShowUSBRow();
      break;
    case kLauncherUsbReturned: {
      g_feature = kFeatureNone;
      VMListRead(&g_vms);
      uint8_t highlighted = 0;
      ReadSettings(&g_settings, &highlighted);
      LauncherMenuOpen(&g_vms, highlighted);
      break;
    }
  }
}

// A key while Settings is open: back to the list when it is left, and the
// settings kept as they are saved when another font is chosen.
static void HandleSettingsKey(SouthbridgeKeyEvent key) {
  switch (LauncherSettingsHandleKey(key)) {
    case kLauncherSettingsStays:
      break;
    case kLauncherSettingsLeft:
      g_feature = kFeatureNone;
      LauncherMenuShowSettingsRow();
      break;
    case kLauncherSettingsFontChanged:
      g_settings.font = LauncherSettingsFont();
      break;
  }
}

bool LauncherHandleKey(SouthbridgeKeyEvent key) {
  g_counting = false;
  LauncherMenuSetCountdown(0);
  if (g_feature == kFeatureUsb) {
    HandleUsbKey(key);
    return false;
  }
  if (g_feature == kFeatureSettings) {
    HandleSettingsKey(key);
    return false;
  }
  bool chosen = false;
  switch (LauncherMenuHandleKey(key)) {
    case kLauncherMenuActionVMChosen:
      chosen = true;
      break;
    case kLauncherMenuActionOpenUSB:
      g_feature = kFeatureUsb;
      LauncherUsbOpen();
      break;
    case kLauncherMenuActionOpenSettings:
      g_feature = kFeatureSettings;
      LauncherSettingsOpen(g_settings.font);
      break;
    case kLauncherMenuActionRenameVM:
      RenameVM();
      break;
    case kLauncherMenuActionCloneVM:
      CloneVM();
      break;
    case kLauncherMenuActionDeleteVM:
      DeleteVM();
      break;
    case kLauncherMenuActionCreateVM:
      CreateVM();
      break;
    case kLauncherMenuActionEditVM:
      EditVM();
      break;
    default:
      break;
  }
  return chosen;
}

bool LauncherCountDown(void) {
  bool chosen = false;
  if (g_counting) {
    const int64_t left_us =
        absolute_time_diff_us(get_absolute_time(), g_start_at);
    if (left_us <= 0) {
      g_counting = false;
      LauncherMenuSetCountdown(0);
      chosen = true;
    } else {
      LauncherMenuSetCountdown(
          (uint8_t)((left_us + kMicrosecondsPerSecond - 1) /
                    kMicrosecondsPerSecond));
    }
  }
  return chosen;
}

bool LauncherTakeChosen(VMConfig* vm) {
  if (!TakeVM(LauncherMenuChosenVM(), vm)) {
    return false;
  }
  g_open = false;
  LauncherMenuClose();
  return true;
}

bool LauncherIsOpen(void) { return g_open; }

void LauncherDraw(uint64_t now_us) {
  switch (g_feature) {
    case kFeatureUsb:
      LauncherUsbDraw(now_us);
      break;
    case kFeatureSettings:
      LauncherSettingsDraw();
      break;
    case kFeatureNone:
      LauncherMenuDraw();
      break;
  }
}

const char* LauncherPageName(void) {
  switch (g_feature) {
    case kFeatureUsb:
      return LauncherUsbPageName();
    case kFeatureSettings:
      return LauncherSettingsPageName();
    case kFeatureNone:
      break;
  }
  return LauncherMenuPageName();
}
