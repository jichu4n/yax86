#include "launcher.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "disk_image.h"
#include "display.h"
#include "font_4x10.h"
#include "fs.h"
#include "hardware/watchdog.h"
#include "launcher_menu.h"
#include "pico/stdlib.h"
#include "runtime_log.h"
#include "status_bar.h"
#include "usb_disk.h"
#include "vm_folder.h"
#include "vm_store.h"

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

// Guest RAM, lent before the VM starts - see LauncherInit().
static uint8_t* g_copy_buffer;
// When the default VM starts by itself, and whether it is still counting
// down to it: a key stops it.
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

// Make the VM named name the one to start at the next power on, saying on the
// message row if it could not be.
static void SaveBootDefault(const char* name) {
  char problem[kVMProblemLength + 1];
  if (VMStoreSaveBootDefault(name, problem)) {
    return;
  }
  char message[kTextColumns + 1];
  snprintf(
      message, sizeof(message), "Could not save boot_default: %.50s", problem);
  StatusBarPostMessage(message);
}

static __noinline bool TakeVM(uint8_t index, VMConfig* chosen) {
  const VMEntry* vm = &VMStoreList()->vms[index];
  char problem[kVMProblemLength + 1];
  if (VMConfigRead(vm, chosen, problem)) {
    SYSTEM_INFO("vm_chosen", Y_LOG_STRING("path", vm->path));
    SaveBootDefault(vm->name);
    return true;
  }
  char message[kTextColumns + 1];
  snprintf(message, sizeof(message), "%s: %s", vm->name, problem);
  LauncherMenuSetProblem(message);
  SYSTEM_WARN(
      "vm_config_problem", Y_LOG_STRING("path", vm->path),
      Y_LOG_STRING("problem", problem));
  return false;
}

// Read the card's VMs and settings afresh, and use the font they name, with
// *highlighted the default VM's index in the list. Returns false, with the
// problem shown on the list, if the settings had one. Kept out of line so
// that its problem and message are on core 0's stack only while it runs,
// rather than in each of its callers' frames.
static __noinline bool ReadCard(uint8_t* highlighted) {
  char problem[kVMProblemLength + 1];
  const bool read = VMStoreRead(highlighted, problem);
  DisplaySetFont(VMStoreSettings()->font);
  if (!read) {
    char message[kTextColumns + 1];
    snprintf(message, sizeof(message), "YAX86.CFG: %s", problem);
    LauncherMenuSetProblem(message);
  }
  return read;
}

static __noinline void ChooseFont(void) {
  const Font4x10 font = LauncherMenuChosenFont();
  DisplaySetFont(font);
  char problem[kVMProblemLength + 1];
  if (VMStoreSaveFont(font, problem)) {
    return;
  }
  char message[kTextColumns + 1];
  snprintf(message, sizeof(message), "Could not save font: %.50s", problem);
  LauncherMenuSetProblem(message);
}

// Share the SD card over USB: the card is unmounted first, as
// nothing here may touch it while the computer has it.
static void ShareCard(void) {
  DiskImageUnmountCard();
  UsbDiskShareCard();
  LauncherMenuShowSharing();
  SYSTEM_INFO("usb_card_shared");
}

// The disk image on USB, while one is, and its path.
static DiskImage g_usb_image;
static char g_usb_image_path[kFSMaxPath + 1];

// Put the disk image chosen in the browser on USB, the card staying mounted
// to read it through; or say why it could not be, staying in the browser.
static __noinline void ShareImage(void) {
  uint32_t size = 0;
  FSCopyPath(LauncherMenuChosenImage(&size), g_usb_image_path);
  const DiskImageStatus status =
      DiskImageOpen(&g_usb_image, g_usb_image_path, size);
  if (status != kDiskImageOpen) {
    char name[kDiskImageNameLength + 1];
    DiskImageName(g_usb_image_path, name);
    char message[kTextColumns + 1];
    snprintf(
        message, sizeof(message), "Could not open %s: %s", name,
        DiskImageDescribeStatus(status));
    LauncherMenuSetProblem(message);
    SYSTEM_WARN(
        "usb_image_not_shared", Y_LOG_STRING("path", g_usb_image_path),
        Y_LOG_STRING("error", DiskImageDescribeStatus(status)));
    return;
  }
  UsbDiskShareImage(&g_usb_image);
  LauncherMenuShowSharing();
  SYSTEM_INFO("usb_image_shared", Y_LOG_STRING("path", g_usb_image_path));
}

// Take the card or image back from USB and read the card afresh - the
// computer may have added VMs, removed them, or changed the settings -
// showing the list again without counting down. An image is closed, writing
// back the sector it holds.
static __noinline void StopSharing(void) {
  const UsbDiskStats stats = UsbDiskGetStats();
  UsbDiskStopSharing();
  if (stats.image_path != NULL) {
    DiskImageClose(&g_usb_image);
  }
  SYSTEM_INFO(
      "usb_returned",
      Y_LOG_STRING("image", stats.image_path != NULL ? stats.image_path : ""),
      Y_LOG_BOOL("ejected", stats.ejected),
      Y_LOG_UINT("blocks_read", stats.blocks_read),
      Y_LOG_UINT("blocks_written", stats.blocks_written),
      Y_LOG_UINT("errors", stats.errors));
  uint8_t highlighted = 0;
  ReadCard(&highlighted);
  LauncherMenuOpen(highlighted);
}

// After Manage VMs has changed the card, read its VMs afresh, and the
// settings, and show Manage VMs again with the VM named name chosen - and
// with show_vm, its page.
static __noinline void ShowManagedVMs(const char* name, bool show_vm) {
  // name may be in the list read over.
  char chosen[kVMNameLength + 1];
  snprintf(chosen, sizeof(chosen), "%s", name);
  uint8_t highlighted = 0;
  ReadCard(&highlighted);
  LauncherMenuShowManagedVMs(chosen, show_vm);
}

// Show why Manage VMs could not do something, and log it.
static void ShowManageProblem(
    const char* event, const VMEntry* vm, const char* problem) {
  LauncherMenuSetProblem(problem);
  SYSTEM_WARN(
      event, Y_LOG_STRING("path", vm->path), Y_LOG_STRING("problem", problem));
}

static __noinline void RenameVM(void) {
  const VMEntry vm = VMStoreList()->vms[LauncherMenuManagedVM()];
  VMEntry renamed;
  char problem[kVMProblemLength + 1];
  const VMFolderResult result = VMFolderRename(
      VMStoreList(), &vm, LauncherMenuTypedName(), &renamed, problem);
  if (result == kVMFolderRefused) {
    LauncherMenuSetProblem(problem);
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
  LauncherMenuDraw(time_us_64());
}

static __noinline void CloneVM(void) {
  const VMEntry vm = VMStoreList()->vms[LauncherMenuManagedVM()];
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
    LauncherMenuSetProblem(problem);
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
    LauncherMenuSetProblem(problem);
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
    LauncherMenuSetProblem(problem);
    SYSTEM_WARN(
        "vm_not_created", Y_LOG_STRING("name", created.name),
        Y_LOG_STRING("problem", problem));
  }
}

static __noinline void EditVM(void) {
  const VMEntry vm = VMStoreList()->vms[LauncherMenuManagedVM()];
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
    LauncherMenuSetProblem(problem);
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
  const VMEntry vm = VMStoreList()->vms[LauncherMenuManagedVM()];
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
  state.hint = LauncherMenuHint();
  state.problem = LauncherMenuProblem();
  StatusBarOffer(&state);
}

void LauncherInit(uint8_t* copy_buffer) { g_copy_buffer = copy_buffer; }

LauncherStartResult LauncherStart(VMConfig* vm) {
  StartNote note = {0};
  const bool noted = TakeStartNote(&note);
  uint8_t highlighted = 0;
  const bool settings_read = ReadCard(&highlighted);
  // Without a card there is nothing to choose, or to put on USB.
  if (DiskImageMountCard() != kDiskImageOpen) {
    return kLauncherNoCard;
  }
  if (noted) {
    SYSTEM_INFO(
        note.intent == kStartIntentRestart ? "restarting" : "powered_off",
        Y_LOG_STRING("name", note.name));
    const uint8_t index = VMListFind(VMStoreList(), note.name);
    if (index < kVMListMax) {
      highlighted = index;
      if (note.intent == kStartIntentRestart && TakeVM(index, vm)) {
        return kLauncherTookVM;
      }
    }
  } else if (
      settings_read && VMStoreSettings()->boot_delay == 0 &&
      VMStoreList()->count > 0 && TakeVM(highlighted, vm)) {
    return kLauncherTookVM;
  }
  LauncherMenuOpen(highlighted);
  const VMSettings* settings = VMStoreSettings();
  g_start_at =
      make_timeout_time_ms(settings->boot_delay * kMillisecondsPerSecond);
  g_counting = !noted && settings->boot_delay > 0 && VMStoreList()->count > 0;
  return kLauncherShowsList;
}

bool LauncherHandleKey(SouthbridgeKeyEvent key) {
  g_counting = false;
  LauncherMenuSetCountdown(0);
  bool chosen = false;
  switch (LauncherMenuHandleKey(key)) {
    case kLauncherMenuActionVMChosen:
      chosen = true;
      break;
    case kLauncherMenuActionShareCard:
      ShareCard();
      break;
    case kLauncherMenuActionShareImage:
      ShareImage();
      break;
    case kLauncherMenuActionStopSharing:
      StopSharing();
      break;
    case kLauncherMenuActionFontChosen:
      ChooseFont();
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
  return TakeVM(LauncherMenuChosenVM(), vm);
}
