// Pre-boot supervisor and VM manager for the PicoCalc runtime.

#include "launcher.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "command.h"
#include "disk_image.h"
#include "display.h"
#include "font_4x10.h"
#include "fs.h"
#include "harddisk.h"
#include "hardware/watchdog.h"
#include "menu_browse.h"
#include "pico/time.h"
#include "report.h"
#include "runtime_log.h"
#include "southbridge.h"
#include "status_bar.h"
#include "text.h"
#include "typing.h"
#include "ui_box.h"
#include "usb_disk.h"

enum {
  kKeyLeftShift = 0xA2,
  kKeyRightShift = 0xA3,
  kKeyUp = 0xB5,
  kKeyDown = 0xB6,
  kKeyEnter = 0x0A,
  kKeySpace = ' ',
  kKeyEsc = 0xB1,
  kKeyBackspace = 0x08,
  kKeyPageUp = 0xD6,
  kKeyPageDown = 0xD7,

  kListRows = kDisplayMenuMaxRows - 2,
  kNarrowColumns = kUIBoxNarrowColumns,
  kWideColumns = kUIBoxWideColumns,

  kStatusRefreshUs = 1000 * 1000,
  kStatusUpdateMs = 100,
  kPausedPollMs = 2,
  kBytesPerKB = 1024,
  kMicrosecondsPerMillisecond = 1000,
  kMillisecondsPerSecond = 1000,
  kMicrosecondsPerSecond = 1000 * 1000,

  // The part of the guest's RAM a clone's files are copied through, before a
  // VM starts and the RAM is the guest's: 64 of the card's blocks, which go to
  // and from it a run at a time.
  kCopyBufferSize = 32 * 1024,

  // Written to g_start_note's marker by a reset or a power off, for the
  // start after the reboot - see LauncherLeaveStartNote().
  kStartNoteMarker = 0x5747A27E,
};

typedef enum LauncherPage {
  kLauncherPageVMs = 0,
  kLauncherPageUSB,
  kLauncherPageSharing,
  kLauncherPageStopSharing,
  kLauncherPageManage,
  kLauncherPageVM,
  kLauncherPageName,
  kLauncherPageDelete,
  kLauncherPageCopying,
  kLauncherPageNew,
  kLauncherPageNewDrive,
  kLauncherPageSettings,
  kLauncherPageFont,
  kLauncherPageBrowse,
  kLauncherPageCount,
} LauncherPage;

typedef enum USBItem {
  kUSBCard = 0,
  kUSBImage,
  kUSBItemCount,
} USBItem;

typedef enum StopSharingItem {
  kStopSharingConfirm = 0,
  kStopSharingCancel,
  kStopSharingItemCount,
} StopSharingItem;

typedef enum VMItem {
  kVMEdit = 0,
  kVMClone,
  kVMRename,
  kVMDelete,
  kVMItemCount,
} VMItem;

typedef enum DeleteItem {
  kDeleteConfirm = 0,
  kDeleteCancel,
  kDeleteItemCount,
} DeleteItem;

typedef enum NewItem {
  kNewName = 0,
  kNewDriveA,
  kNewDriveB,
  kNewDriveC,
  kNewDriveD,
  kNewCreate,
  kNewItemCount,
} NewItem;

typedef enum NewDriveItem {
  kNewDriveNone = 0,
  kNewDriveDisabled,
  kNewDriveBlank,
  kNewDriveImage,
  kNewDriveItemCount,
} NewDriveItem;

typedef enum SettingsItem {
  kSettingsFont = 0,
  kSettingsItemCount,
} SettingsItem;

typedef struct StartNoteRecord {
  uint32_t marker;
  StartIntent intent;
  char name[kVMNameLength + 1];
} StartNoteRecord;

static StartNoteRecord __uninitialized_ram(g_start_note);

typedef struct LauncherPageDef {
  const char* name;
  const char* (*get_name)(void);
  const char* hint;
  const char* (*get_hint)(void);
  uint8_t (*item_count)(void);
  void (*draw)(DisplayMenu* menu);
  void (*choose)(void);
  void (*back)(void);
  bool (*handle_key)(uint8_t key);
} LauncherPageDef;

static const LauncherPageDef kPageDefs[kLauncherPageCount];

static const MenuItem kManageRow = {"Manage VMs...", true};
static const MenuItem kUSBRow = {"USB disk mode...", true};
static const MenuItem kSettingsRow = {"Settings...", true};
static const MenuItem kNewVMRow = {"New VM...", true};

static const MenuItem kUSBItems[kUSBItemCount] = {
    {"Share card", false},
    {"Share disk image...", true},
};

static const MenuItem kStopSharingItems[kStopSharingItemCount] = {
    {"Stop sharing", false},
    {"Keep sharing", false},
};

static const MenuItem kVMItems[kVMItemCount] = {
    {"Edit drives...", true},
    {"Clone...", true},
    {"Rename...", true},
    {"Delete...", true},
};

static const MenuItem kDeleteItems[kDeleteItemCount] = {
    {"Delete", false},
    {"Keep VM", false},
};

static const MenuItem kNewHardDiskItems[kNewDriveItemCount] = {
    {"(keep empty)", false},
    {"(disabled)", false},
    {"new disk image", false},
    {"existing disk image...", true},
};

static const MenuItem kNewFloppyItems[kNewDriveItemCount] = {
    {"(keep empty)", false},
    {"(disabled)", false},
    {"new floppy image", false},
    {"existing floppy image...", true},
};

static const MenuItem kSettingsItems[kSettingsItemCount] = {
    {"Font", true},
};

static LauncherConfig g_config;
static bool g_open;
static LauncherPage g_page;
static uint8_t g_chosen;
static bool g_dirty;
static uint64_t g_status_drawn_us;

static uint8_t g_vms_top;
static uint8_t g_countdown;
static Font4x10 g_font = kFont4x10Default;

static uint8_t g_vm;
static char g_vm_drives[kVMDrives][kDiskImageNameLength + 1];
static bool g_vm_present[kVMDrives];
static bool g_vm_readable;

static char g_name[kFSMaxFolderName + 1];
static uint8_t g_name_length;
static bool g_cloning;

static VMFolderContents g_contents;
static uint32_t g_copied_kb;
static uint32_t g_copy_kb;
static bool g_creating;

static char g_new_drives[kVMDrives][kFSMaxPath + 1];
static bool g_new_blank[kVMDrives];
static bool g_new_present[kVMDrives];
static uint8_t g_new_drive;
static bool g_editing;

static DiskImage g_usb_image;
static char g_usb_image_path[kFSMaxPath + 1];
static uint32_t g_usb_image_size;

static char g_problems[2][kTextColumns + 1];
static char* g_problem = g_problems[0];

static bool g_vm_chosen;
static uint8_t g_chosen_vm;

static DisplayStatus g_status_rows;
static bool g_status_rows_pending;

bool LauncherIsOpen(void) {
  return g_open;
}

const char* LauncherPageName(void) {
  const LauncherPageDef* def = &kPageDefs[g_page];
  return def->get_name != NULL ? def->get_name() : def->name;
}

static const char* LauncherHint(void) {
  const LauncherPageDef* def = &kPageDefs[g_page];
  return def->get_hint != NULL ? def->get_hint() : def->hint;
}

static const char* LauncherProblem(void) {
  return g_problem[0] != '\0' ? g_problem : NULL;
}

static void SetProblem(const char* problem) {
  g_problem = g_problem == g_problems[0] ? g_problems[1] : g_problems[0];
  snprintf(g_problem, kTextColumns + 1, "%s", problem != NULL ? problem : "");
}

static void ShowPage(LauncherPage page, uint8_t chosen) {
  g_page = page;
  g_chosen = chosen;
  g_dirty = true;
}

static void ScrollToChosen(void) {
  if (g_page == kLauncherPageBrowse) {
    MenuBrowseScrollTo(g_chosen);
    return;
  }
  uint8_t row = g_chosen;
  if ((g_page == kLauncherPageVMs || g_page == kLauncherPageManage) &&
      g_config.vms->count > 0 && g_chosen >= g_config.vms->count) {
    ++row;
  }
  UIBoxScrollTo(row, &g_vms_top, kListRows);
}

static void OfferStatusRows(const StatusBarState* state) {
  if (StatusBarCompose(state, time_us_64(), &g_status_rows)) {
    g_status_rows_pending = true;
  }
  if (!g_status_rows_pending) {
    return;
  }
  DisplayStatus* rows = DisplayBeginStatus();
  if (rows == NULL) {
    return;
  }
  *rows = g_status_rows;
  DisplayEndStatus();
  g_status_rows_pending = false;
}

static void UpdateChoosingRows(void) {
  StatusBarState state = {0};
  state.battery = SouthbridgeGetBattery();
  state.hint = LauncherHint();
  state.problem = LauncherProblem();
  OfferStatusRows(&state);
}

void LauncherLeaveStartNote(StartIntent intent, const char* name) {
  g_start_note.intent = intent;
  snprintf(
      g_start_note.name, sizeof(g_start_note.name), "%s",
      name != NULL ? name : "");
  g_start_note.marker = kStartNoteMarker;
  watchdog_reboot(0, 0, 0);
  for (;;) {
    tight_loop_contents();
  }
}

static bool TakeStartNote(StartNoteRecord* note) {
  if (g_start_note.marker != kStartNoteMarker) {
    return false;
  }
  g_start_note.marker = 0;
  note->intent = g_start_note.intent;
  g_start_note.name[kVMNameLength] = '\0';
  memcpy(note->name, g_start_note.name, sizeof(note->name));
  return true;
}

static void SaveBootDefault(const char* name) {
  if (strcasecmp(g_config.settings->boot_default, name) == 0) {
    return;
  }
  char problem[kVMProblemLength + 1];
  if (VMSettingsSaveBootDefault(name, problem)) {
    snprintf(
        g_config.settings->boot_default,
        sizeof(g_config.settings->boot_default), "%s", name);
    SYSTEM_INFO("boot_default_saved", Y_LOG_STRING("name", name));
    return;
  }
  char message[kTextColumns + 1];
  snprintf(
      message, sizeof(message), "Could not save boot_default: %.50s", problem);
  StatusBarPostMessage(message);
  SYSTEM_WARN("boot_default_not_saved", Y_LOG_STRING("problem", problem));
}

static __noinline bool TakeVM(uint8_t index) {
  const VMEntry* vm = &g_config.vms->vms[index];
  char problem[kVMProblemLength + 1];
  if (VMConfigRead(vm, g_config.vm, problem)) {
    *g_config.has_vm = true;
    SYSTEM_INFO("vm_chosen", Y_LOG_STRING("path", vm->path));
    SaveBootDefault(vm->name);
    return true;
  }
  char message[kTextColumns + 1];
  snprintf(message, sizeof(message), "%s: %s", vm->name, problem);
  SetProblem(message);
  SYSTEM_WARN(
      "vm_config_problem", Y_LOG_STRING("path", vm->path),
      Y_LOG_STRING("problem", problem));
  return false;
}

static bool ReadSettings(VMSettings* settings, uint8_t* highlighted) {
  char problem[kVMProblemLength + 1];
  bool read = VMSettingsRead(settings, problem);
  DisplaySetFont(settings->font);
  g_font = settings->font;
  *highlighted = 0;
  if (read && settings->boot_default[0] != '\0') {
    const uint8_t index = VMListFind(g_config.vms, settings->boot_default);
    if (index < kVMListMax) {
      *highlighted = index;
    }
  }
  if (!read) {
    char message[kTextColumns + 1];
    snprintf(message, sizeof(message), "YAX86.CFG: %s", problem);
    SetProblem(message);
    SYSTEM_WARN("settings_problem", Y_LOG_STRING("problem", problem));
  }
  return read;
}

static uint8_t VMsItemCount(void) {
  return (uint8_t)(g_config.vms->count + 3);
}

static uint8_t VMsRowCount(void) {
  return (uint8_t)(g_config.vms->count + (g_config.vms->count > 0 ? 4 : 3));
}

static uint8_t ManageItemIndex(void) {
  return g_config.vms->count;
}

static uint8_t USBItemIndex(void) {
  return (uint8_t)(g_config.vms->count + 1);
}

static uint8_t SettingsItemIndex(void) {
  return (uint8_t)(g_config.vms->count + 2);
}

static uint8_t ManageRowCount(void) {
  return (uint8_t)(g_config.vms->count + (g_config.vms->count > 0 ? 2 : 1));
}

static uint8_t FormFirstItem(void) {
  return g_editing ? kNewDriveA : kNewName;
}

static uint8_t FormItem(void) {
  return (uint8_t)(g_chosen + FormFirstItem());
}

static void ShowFormItem(uint8_t item) {
  ShowPage(kLauncherPageNew, (uint8_t)(item - FormFirstItem()));
}

static uint8_t DriveChoiceRow(NewDriveItem choice) {
  return (choice == kNewDriveNone && !g_editing) ? 0 : (uint8_t)(choice - 1);
}

static const NewDriveItem* DriveChoices(uint8_t* count) {
  static const NewDriveItem kNewChoices[] = {
      kNewDriveBlank, kNewDriveImage, kNewDriveDisabled};
  static const NewDriveItem kEditChoices[] = {
      kNewDriveNone, kNewDriveBlank, kNewDriveImage, kNewDriveDisabled};
  if (g_editing) {
    *count = sizeof(kEditChoices) / sizeof(kEditChoices[0]);
    return kEditChoices;
  }
  *count = sizeof(kNewChoices) / sizeof(kNewChoices[0]);
  return kNewChoices;
}

static void ShowNewDrive(uint8_t drive) {
  g_new_drive = drive;
  uint8_t chosen = kNewDriveNone;
  if (!g_new_present[drive]) {
    chosen = kNewDriveDisabled;
  } else if (g_new_blank[drive]) {
    chosen = kNewDriveBlank;
  } else if (g_new_drives[drive][0] != '\0') {
    chosen = kNewDriveImage;
  }
  ShowPage(kLauncherPageNewDrive, DriveChoiceRow((NewDriveItem)chosen));
}

static void ChooseNewDrive(void) {
  uint8_t count = 0;
  const NewDriveItem choice = DriveChoices(&count)[g_chosen];
  char* path = g_new_drives[g_new_drive];
  if (choice == kNewDriveImage) {
    MenuBrowseForNewVM(g_new_drive, path);
    return;
  }
  path[0] = '\0';
  g_new_blank[g_new_drive] = choice == kNewDriveBlank;
  g_new_present[g_new_drive] = choice != kNewDriveDisabled;
  ShowFormItem((uint8_t)(kNewDriveA + g_new_drive));
}

static bool TypeNewName(uint8_t key) {
  if (key == kKeyBackspace) {
    if (g_name_length > 0) {
      g_name[--g_name_length] = '\0';
      g_dirty = true;
    }
    return true;
  }
  if (key >= 0x80 || !FSIsFolderNameCharacter((char)key)) {
    return false;
  }
  if (g_name_length < kFSMaxFolderName) {
    g_name[g_name_length++] = (char)key;
    g_name[g_name_length] = '\0';
    g_dirty = true;
  }
  return true;
}

static void StartNewVM(void) {
  g_editing = false;
  g_name[0] = '\0';
  g_name_length = 0;
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    g_new_drives[drive][0] = '\0';
    g_new_blank[drive] = drive == kVMDriveHD1;
    g_new_present[drive] = drive == kVMDriveFD1 || drive == kVMDriveHD1;
  }
  ShowFormItem(kNewName);
}

static void StartEditVM(void) {
  g_editing = true;
  static VMConfig config;
  char problem[kVMProblemLength + 1];
  VMConfigRead(&g_config.vms->vms[g_vm], &config, problem);
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    FSCopyPath(config.drives[drive], g_new_drives[drive]);
    g_new_blank[drive] = false;
    g_new_present[drive] =
        drive == kVMDriveFD1 ||
        (drive == kVMDriveFD2 ? config.present[drive]
                              : config.drives[drive][0] != '\0');
  }
  ShowFormItem(kNewDriveA);
}

static void ShowVM(uint8_t index) {
  g_vm = index;
  const VMEntry* vm = &g_config.vms->vms[index];
  static VMConfig config;
  char problem[kVMProblemLength + 1];
  g_vm_readable = VMConfigRead(vm, &config, problem);
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    DiskImageName(config.drives[drive], g_vm_drives[drive]);
    g_vm_present[drive] = config.present[drive];
  }
  ShowPage(kLauncherPageVM, 0);
}

static void StartNaming(bool cloning) {
  g_cloning = cloning;
  const char* current = g_config.vms->vms[g_vm].name;
  snprintf(g_name, sizeof(g_name), "%s", current);
  g_name_length = (uint8_t)strlen(g_name);
  ShowPage(kLauncherPageName, 0);
}

static void ShowManaged(uint8_t index) {
  g_vms_top = 0;
  ShowPage(kLauncherPageManage, index);
  ScrollToChosen();
}

static __noinline void ShowManagedVMs(const char* name, bool show_vm) {
  char chosen[kVMNameLength + 1];
  snprintf(chosen, sizeof(chosen), "%s", name);
  VMListRead(g_config.vms);
  uint8_t highlighted = 0;
  ReadSettings(g_config.settings, &highlighted);
  const uint8_t index = VMListFind(g_config.vms, chosen);
  if (show_vm && index < g_config.vms->count) {
    ShowVM(index);
    return;
  }
  ShowManaged(index < g_config.vms->count ? index : 0);
}

static void ShowManageProblem(
    const char* event, const VMEntry* vm, const char* problem) {
  SetProblem(problem);
  SYSTEM_WARN(
      event, Y_LOG_STRING("path", vm->path), Y_LOG_STRING("problem", problem));
}

static __noinline void RenameVM(void) {
  const VMEntry vm = g_config.vms->vms[g_vm];
  VMEntry renamed;
  char problem[kVMProblemLength + 1];
  const VMFolderResult result =
      VMFolderRename(g_config.vms, &vm, g_name, &renamed, problem);
  if (result == kVMFolderRefused) {
    SetProblem(problem);
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

static void LauncherDraw(uint64_t now_us);

static void ShowCopyProgress(uint64_t done, uint64_t total) {
  if (g_page != kLauncherPageCopying) {
    g_creating = g_page == kLauncherPageNew;
    ShowPage(kLauncherPageCopying, 0);
  }
  g_copied_kb = (uint32_t)(done / kBytesPerKB);
  g_copy_kb = (uint32_t)(total / kBytesPerKB);
  g_dirty = true;
  UpdateChoosingRows();
  LauncherDraw(time_us_64());
}

static __noinline void CloneVM(void) {
  const VMEntry vm = g_config.vms->vms[g_vm];
  VMEntry copy;
  char problem[kVMProblemLength + 1];
  const uint64_t start_us = time_us_64();
  FSTransfer transfer = {
      .buffer = g_config.guest_ram,
      .buffer_size = kCopyBufferSize,
      .progress = ShowCopyProgress,
  };
  const VMFolderResult result =
      VMFolderClone(&vm, g_name, &transfer, &copy, problem);
  memset(g_config.guest_ram, 0, kCopyBufferSize);
  if (result == kVMFolderRefused) {
    SetProblem(problem);
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
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    drives.paths[drive] = g_new_present[drive] ? g_new_drives[drive] : NULL;
    drives.blank[drive] = g_new_blank[drive];
  }
  VMEntry created;
  char problem[kVMProblemLength + 1];
  const uint64_t start_us = time_us_64();
  FSTransfer transfer = {
      .buffer = g_config.guest_ram,
      .buffer_size = kCopyBufferSize,
      .progress = ShowCopyProgress,
  };
  const VMFolderResult result =
      VMFolderCreate(g_name, &drives, &transfer, &created, problem);
  memset(g_config.guest_ram, 0, kCopyBufferSize);
  if (result == kVMFolderRefused) {
    SetProblem(problem);
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
    SetProblem(problem);
    SYSTEM_WARN(
        "vm_not_created", Y_LOG_STRING("name", created.name),
        Y_LOG_STRING("problem", problem));
  }
}

static __noinline void EditVM(void) {
  const VMEntry vm = g_config.vms->vms[g_vm];
  VMFolderDrives drives;
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    drives.paths[drive] = g_new_present[drive] ? g_new_drives[drive] : NULL;
    drives.blank[drive] = g_new_blank[drive];
  }
  char problem[kVMProblemLength + 1];
  FSTransfer transfer = {
      .buffer = g_config.guest_ram,
      .buffer_size = kCopyBufferSize,
      .progress = ShowCopyProgress,
  };
  const VMFolderResult result = VMFolderEdit(&vm, &drives, &transfer, problem);
  memset(g_config.guest_ram, 0, kCopyBufferSize);
  if (result == kVMFolderRefused) {
    SetProblem(problem);
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
  const VMEntry vm = g_config.vms->vms[g_vm];
  char problem[kVMProblemLength + 1];
  const bool deleted = VMFolderDelete(&vm, problem);
  ShowManagedVMs(vm.name, false);
  if (deleted) {
    SYSTEM_INFO("vm_deleted", Y_LOG_STRING("name", vm.name));
  } else {
    ShowManageProblem("vm_not_deleted", &vm, problem);
  }
}

static void ShareCard(void) {
  DiskImageUnmountCard();
  UsbDiskShareCard();
  ShowPage(kLauncherPageSharing, 0);
  SYSTEM_INFO("usb_card_shared");
}

static __noinline void ShareImage(const char* path, uint32_t size) {
  FSCopyPath(path, g_usb_image_path);
  g_usb_image_size = size;
  const DiskImageStatus status =
      DiskImageOpen(&g_usb_image, g_usb_image_path, size);
  if (status != kDiskImageOpen) {
    char name[kDiskImageNameLength + 1];
    DiskImageName(g_usb_image_path, name);
    char message[kTextColumns + 1];
    snprintf(
        message, sizeof(message), "Could not open %s: %s", name,
        DiskImageDescribeStatus(status));
    SetProblem(message);
    SYSTEM_WARN(
        "usb_image_not_shared", Y_LOG_STRING("path", g_usb_image_path),
        Y_LOG_STRING("error", DiskImageDescribeStatus(status)));
    return;
  }
  UsbDiskShareImage(&g_usb_image);
  ShowPage(kLauncherPageSharing, 0);
  SYSTEM_INFO("usb_image_shared", Y_LOG_STRING("path", g_usb_image_path));
}

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
  VMListRead(g_config.vms);
  uint8_t highlighted = 0;
  ReadSettings(g_config.settings, &highlighted);
  ShowPage(kLauncherPageVMs, highlighted);
}

static __noinline void ChooseFont(Font4x10 font) {
  g_font = font;
  DisplaySetFont(font);
  if (font == g_config.settings->font) {
    ShowPage(kLauncherPageSettings, 0);
    return;
  }
  g_config.settings->font = font;
  char problem[kVMProblemLength + 1];
  if (VMSettingsSaveFont(font, problem)) {
    SYSTEM_INFO("font_saved", Y_LOG_STRING("font", Font4x10Name(font)));
    ShowPage(kLauncherPageSettings, 0);
    return;
  }
  char message[kTextColumns + 1];
  snprintf(message, sizeof(message), "Could not save font: %.50s", problem);
  SetProblem(message);
  SYSTEM_WARN("font_not_saved", Y_LOG_STRING("problem", problem));
  ShowPage(kLauncherPageSettings, 0);
}

static void OnBrowseSelectedForNewVM(uint8_t new_drive, const char* path) {
  FSCopyPath(path, g_new_drives[new_drive]);
  g_new_blank[new_drive] = false;
  g_new_present[new_drive] = true;
  ShowFormItem((uint8_t)(kNewDriveA + new_drive));
}

static void OnBrowseSelectedForUSB(const char* path, uint32_t size) {
  ShareImage(path, size);
}

static void OnBrowseBackForNewVM(void) {
  ShowPage(kLauncherPageNewDrive, DriveChoiceRow(kNewDriveImage));
}

static void OnBrowseBackForUSB(void) {
  ShowPage(kLauncherPageUSB, kUSBImage);
}

static void OnBrowseSetChosen(uint8_t chosen) {
  g_chosen = chosen;
  g_dirty = true;
}

static void OnBrowseSetDirty(void) {
  g_dirty = true;
}

static void OnBrowseShowPage(uint8_t chosen) {
  ShowPage(kLauncherPageBrowse, chosen);
}

static bool ComputerHasCard(void) {
  const UsbDiskStats stats = UsbDiskGetStats();
  return stats.connected && stats.seen && !stats.ejected;
}

static void Disconnect(void) {
  if (ComputerHasCard()) {
    ShowPage(kLauncherPageStopSharing, kStopSharingCancel);
  } else {
    StopSharing();
  }
}

static uint8_t USBItemCount(void) {
  return kUSBItemCount;
}

static uint8_t SharingItemCount(void) {
  return 1;
}

static uint8_t StopSharingItemCount(void) {
  return kStopSharingItemCount;
}

static uint8_t ManageItemCount(void) {
  return (uint8_t)(g_config.vms->count + 1);
}

static uint8_t VMItemCount(void) {
  return kVMItemCount;
}

static uint8_t DeleteItemCount(void) {
  return kDeleteItemCount;
}

static uint8_t NewItemCount(void) {
  return (uint8_t)(kNewItemCount - FormFirstItem());
}

static uint8_t NewDriveItemCount(void) {
  return DriveChoiceRow(kNewDriveItemCount);
}

static uint8_t SettingsItemCount(void) {
  return kSettingsItemCount;
}

static uint8_t FontItemCount(void) {
  return kFont4x10Count;
}

static uint8_t PageItemCount(void) {
  const LauncherPageDef* def = &kPageDefs[g_page];
  return def->item_count != NULL ? def->item_count() : 0;
}

static void DrawItems(
    DisplayMenu* menu, const char* title, const char* const* lines,
    uint8_t line_count, const MenuItem* items, uint8_t count) {
  UIBoxDrawItems(
      menu, title, lines, line_count, items, count, g_chosen, true);
}

static void DrawVMsPage(DisplayMenu* menu) {
  UIBoxPlace(menu, kNarrowColumns, kListRows + 2);
  UIBoxEdgeRow(&menu->text[0], menu->columns, true, "YAX86");
  for (uint8_t i = 0; i < kListRows; ++i) {
    DisplayTextRow* row = &menu->text[1 + i];
    const uint8_t index = (uint8_t)(g_vms_top + i);
    if (index >= VMsRowCount()) {
      UIBoxBlankRow(row, menu->columns);
    } else if (index < g_config.vms->count) {
      UIBoxItemRow(
          row, menu->columns, g_config.vms->vms[index].name,
          index == g_chosen);
    } else if (g_config.vms->count > 0 && index == g_config.vms->count) {
      UIBoxSeparatorRow(row, menu->columns);
    } else {
      const uint8_t item =
          g_config.vms->count > 0 ? (uint8_t)(index - 1) : index;
      if (item == ManageItemIndex()) {
        UIBoxMenuItemRow(row, menu->columns, &kManageRow, item == g_chosen);
      } else if (item == USBItemIndex()) {
        UIBoxMenuItemRow(row, menu->columns, &kUSBRow, item == g_chosen);
      } else {
        UIBoxMenuItemRow(row, menu->columns, &kSettingsRow, item == g_chosen);
      }
    }
  }
  char countdown[kNarrowColumns];
  if (g_countdown > 0) {
    snprintf(
        countdown, sizeof(countdown), "starting in %u s",
        (unsigned)g_countdown);
  }
  UIBoxEdgeRow(
      &menu->text[kListRows + 1], menu->columns, false,
      g_countdown > 0 ? countdown : NULL);
  UIBoxDrawScrollArrows(menu, g_vms_top, kListRows, VMsRowCount());
}

static void DrawManagePage(DisplayMenu* menu) {
  UIBoxPlace(menu, kNarrowColumns, kListRows + 2);
  UIBoxEdgeRow(&menu->text[0], menu->columns, true, "Manage VMs");
  for (uint8_t i = 0; i < kListRows; ++i) {
    DisplayTextRow* row = &menu->text[1 + i];
    const uint8_t index = (uint8_t)(g_vms_top + i);
    if (index >= ManageRowCount()) {
      UIBoxBlankRow(row, menu->columns);
    } else if (index < g_config.vms->count) {
      const MenuItem item = {g_config.vms->vms[index].name, true};
      UIBoxMenuItemRow(row, menu->columns, &item, index == g_chosen);
    } else if (g_config.vms->count > 0 && index == g_config.vms->count) {
      UIBoxSeparatorRow(row, menu->columns);
    } else {
      UIBoxMenuItemRow(
          row, menu->columns, &kNewVMRow, g_chosen == g_config.vms->count);
    }
  }
  UIBoxEdgeRow(&menu->text[kListRows + 1], menu->columns, false, NULL);
  UIBoxDrawScrollArrows(menu, g_vms_top, kListRows, ManageRowCount());
}

static const char* DriveShown(uint8_t drive, const char* name) {
  if (name[0] != '\0') {
    return name;
  }
  return g_vm_present[drive] ? "empty" : "disabled";
}

static void DrawVMPage(DisplayMenu* menu) {
  char lines[kVMDrives / 2][64];
  const char* line_pointers[kVMDrives / 2];
  uint8_t line_count = 0;
  if (!g_vm_readable) {
    line_pointers[line_count++] = "The VM's configuration cannot be read";
  } else {
    for (uint8_t pair = 0; pair < kVMDrives / 2; ++pair) {
      const uint8_t first = (uint8_t)(pair * 2);
      char label_a[16];
      char label_b[16];
      snprintf(label_a, sizeof(label_a), "%s:", kVMDriveLabels[first]);
      snprintf(label_b, sizeof(label_b), "%s:", kVMDriveLabels[first + 1]);
      snprintf(
          lines[line_count], sizeof(lines[line_count]),
          "%-4s %-12.12s %-4s %.12s", label_a,
          DriveShown(first, g_vm_drives[first]), label_b,
          DriveShown((uint8_t)(first + 1), g_vm_drives[first + 1]));
      line_pointers[line_count] = lines[line_count];
      ++line_count;
    }
  }
  const char* title = g_config.vms->vms[g_vm].name;
  DrawItems(
      menu, title, line_pointers, line_count, kVMItems,
      sizeof(kVMItems) / sizeof(kVMItems[0]));
}

static void DrawNamePage(DisplayMenu* menu) {
  const char* action = g_cloning ? "Clone" : "Rename";
  char line1[64];
  snprintf(line1, sizeof(line1), "%s %s as:", action, g_config.vms->vms[g_vm].name);
  const char* lines[] = {line1};
  UIBoxPlace(menu, kNarrowColumns, 5);
  UIBoxEdgeRow(&menu->text[0], menu->columns, true, action);
  DisplayTextRow* row = &menu->text[1];
  UIBoxBlankRow(row, menu->columns);
  UIBoxPutText(row, menu->columns, 3, lines[0], kUIBoxAttrText);
  row = &menu->text[2];
  UIBoxBlankRow(row, menu->columns);
  UIBoxPutText(row, menu->columns, 3, ">", kUIBoxAttrText);
  UIBoxPutText(row, menu->columns, 5, g_name, kUIBoxAttrChosen);
  UIBoxPutText(
      row, menu->columns, (uint8_t)(5 + g_name_length), "_", kUIBoxAttrChosen);
  UIBoxBlankRow(&menu->text[3], menu->columns);
  UIBoxEdgeRow(&menu->text[4], menu->columns, false, NULL);
}

// Join names into text as "A, B and C", or with fewer shown, "A, B and 2
// more": as many of the first named as fit with the rest counted after.
static void JoinNames(
    char* text, uint8_t size, char (*names)[kFSMaxName + 1], uint8_t named,
    uint8_t count) {
  for (uint8_t shown = named;; --shown) {
    size_t length = 0;
    text[0] = '\0';
    for (uint8_t i = 0; i < shown && length < size; ++i) {
      const char* separator = i == 0 ? "" : (i + 1 == count ? " and " : ", ");
      length += (size_t)snprintf(
          text + length, size - length, "%s%s", separator, names[i]);
    }
    if (shown < count && length < size) {
      length += (size_t)snprintf(
          text + length, size - length, "%s%u more", shown > 0 ? " and " : "",
          (unsigned)(count - shown));
    }
    if (length < size || shown == 0) {
      return;
    }
  }
}

// The question before deleting a VM, saying what goes with it, and which VMs
// lose a disk.
static void DrawDeletePage(DisplayMenu* menu) {
  const uint8_t named = g_contents.file_count < kVMFolderNamed
                            ? g_contents.file_count
                            : kVMFolderNamed;
  char question[kNarrowColumns];
  snprintf(
      question, sizeof(question), "Delete %s?", g_config.vms->vms[g_vm].name);
  char files[kNarrowColumns - 4];
  JoinNames(
      files, sizeof(files), g_contents.files, named, g_contents.file_count);
  char users[64];
  const char* lines[5] = {question, ""};
  uint8_t line_count = 1;
  if (g_contents.file_count > 0 || g_contents.user_count > 0) {
    ++line_count;
  }
  if (g_contents.file_count > 0) {
    lines[line_count++] = "Deleted with it:";
    lines[line_count++] = files;
  }
  if (g_contents.user_count == 1) {
    snprintf(users, sizeof(users), "%s will lose a disk.", g_contents.users[0]);
    lines[line_count++] = users;
  } else if (g_contents.user_count > 1) {
    snprintf(
        users, sizeof(users), "%s and %u more VMs will lose disks.",
        g_contents.users[0], (unsigned)(g_contents.user_count - 1));
    lines[line_count++] = users;
  }
  DrawItems(menu, "Delete", lines, line_count, kDeleteItems, kDeleteItemCount);
}

static void DrawCopyProgress(DisplayMenu* menu) {
  const uint8_t width = (uint8_t)(menu->columns - 6);
  const uint8_t filled =
      g_copy_kb > 0
          ? (uint8_t)((uint64_t)g_copied_kb * width / g_copy_kb)
          : 0;
  DisplayTextRow* row = &menu->text[3];
  UIBoxBlankRow(row, menu->columns);
  for (uint8_t col = 0; col < width; ++col) {
    const uint8_t c = (uint8_t)(3 + col);
    row->chars[c] = col < filled ? '\xDB' : '\xB0';
    row->attrs[c] = kUIBoxAttrText;
  }
}

static void DrawCopyingPage(DisplayMenu* menu) {
  UIBoxPlace(menu, kNarrowColumns, 5);
  UIBoxEdgeRow(
      &menu->text[0], menu->columns, true,
      g_creating ? "New VM" : "Clone VM");
  DisplayTextRow* row = &menu->text[1];
  UIBoxBlankRow(row, menu->columns);
  char line[64];
  if (g_creating) {
    snprintf(line, sizeof(line), "Formatting disks for %s...", g_name);
  } else {
    snprintf(
        line, sizeof(line), "Copying %s to %s...",
        g_config.vms->vms[g_vm].name, g_name);
  }
  UIBoxPutText(row, menu->columns, 3, line, kUIBoxAttrText);
  DrawCopyProgress(menu);
  UIBoxBlankRow(&menu->text[2], menu->columns);
  UIBoxEdgeRow(&menu->text[4], menu->columns, false, NULL);
}

static void DrawNewPage(DisplayMenu* menu) {
  static const char* const kItems[] = {
      "Name", "Floppy disk 1", "Floppy disk 2", "Hard disk 1", "Hard disk 2",
      "Create",
  };
  const uint8_t rows =
      (uint8_t)(sizeof(kItems) / sizeof(kItems[0]) - FormFirstItem());
  UIBoxPlace(menu, kNarrowColumns, rows + 2);
  UIBoxEdgeRow(
      &menu->text[0], menu->columns, true,
      g_editing ? "Edit Drives" : "New VM");
  for (uint8_t item = FormFirstItem();
       item < sizeof(kItems) / sizeof(kItems[0]); ++item) {
    const uint8_t i = (uint8_t)(item - FormFirstItem());
    DisplayTextRow* row = &menu->text[1 + i];
    const bool chosen = i == g_chosen;
    const uint8_t attr = chosen ? kUIBoxAttrChosen : kUIBoxAttrText;
    if (item == kNewCreate) {
      const MenuItem submit = {g_editing ? "Save" : "Create", false};
      UIBoxMenuItemRow(row, menu->columns, &submit, chosen);
      continue;
    }
    const MenuItem menu_item = {kItems[item], true};
    UIBoxMenuItemRow(row, menu->columns, &menu_item, chosen);
    if (item == kNewName) {
      if (g_name[0] != '\0') {
        UIBoxPutText(row, menu->columns, 8, g_name, attr);
      }
      if (chosen) {
        UIBoxPutText(
            row, menu->columns, (uint8_t)(8 + g_name_length), "_", attr);
      }
      continue;
    }
    const uint8_t drive = (uint8_t)(item - kNewDriveA);
    const char* what = "(empty)";
    char name[kDiskImageNameLength + 1];
    if (!g_new_present[drive]) {
      what = "(disabled)";
    } else if (g_new_blank[drive]) {
      what = drive >= kVMDriveHD1 ? "new disk image" : "new floppy image";
    } else if (g_new_drives[drive][0] != '\0') {
      DiskImageName(g_new_drives[drive], name);
      what = name;
    }
    UIBoxPutText(row, menu->columns, 17, what, attr);
  }
  UIBoxEdgeRow(&menu->text[rows + 1], menu->columns, false, NULL);
}

static void DrawNewDrivePage(DisplayMenu* menu) {
  uint8_t count = 0;
  const NewDriveItem* choices = DriveChoices(&count);
  MenuItem items[kNewDriveItemCount];
  const MenuItem* choice_items =
      g_new_drive >= kVMDriveHD1 ? kNewHardDiskItems : kNewFloppyItems;
  for (uint8_t i = 0; i < count; ++i) {
    items[i] = choice_items[choices[i]];
  }
  const char* title = kVMDriveNames[g_new_drive];
  DrawItems(menu, title, NULL, 0, items, count);
}

static void DrawUSBPage(DisplayMenu* menu) {
  DrawItems(
      menu, "USB Disk Mode", NULL, 0, kUSBItems,
      sizeof(kUSBItems) / sizeof(kUSBItems[0]));
}

static void DrawSharingPage(DisplayMenu* menu) {
  const UsbDiskStats stats = UsbDiskGetStats();
  char line1[64];
  if (stats.image_path != NULL) {
    char name[kDiskImageNameLength + 1];
    DiskImageName(stats.image_path, name);
    snprintf(line1, sizeof(line1), "Sharing %s", name);
  } else {
    snprintf(line1, sizeof(line1), "Sharing the SD card");
  }
  const char* line2 = "Waiting for a computer...";
  if (stats.ejected) {
    line2 = "The computer has ejected it.";
  } else if (stats.seen) {
    line2 = "A computer is using it.";
  } else if (stats.connected) {
    line2 = "Connected to a computer.";
  }
  const char* lines[] = {line1, line2};
  static const MenuItem kItem[] = {{"Disconnect", false}};
  DrawItems(menu, "USB Disk Mode", lines, 2, kItem, 1);
}

static void DrawStopSharingPage(DisplayMenu* menu) {
  static const char* const kLines[] = {
      "A computer is using it.", "Disconnect anyway?",
  };
  DrawItems(
      menu, "Disconnect?", kLines, sizeof(kLines) / sizeof(kLines[0]),
      kStopSharingItems,
      sizeof(kStopSharingItems) / sizeof(kStopSharingItems[0]));
}

static void DrawSettingsPage(DisplayMenu* menu) {
  UIBoxPlace(menu, kNarrowColumns, 3);
  UIBoxEdgeRow(&menu->text[0], menu->columns, true, "Settings");
  DisplayTextRow* row = &menu->text[1];
  UIBoxMenuItemRow(row, menu->columns, &kSettingsItems[0], g_chosen == 0);
  const uint8_t attr = g_chosen == 0 ? kUIBoxAttrChosen : kUIBoxAttrText;
  UIBoxPutText(row, menu->columns, 9, Font4x10Name(g_font), attr);
  UIBoxEdgeRow(&menu->text[2], menu->columns, false, NULL);
}

static void DrawFontPage(DisplayMenu* menu) {
  MenuItem items[kFont4x10Count];
  for (uint8_t i = 0; i < kFont4x10Count; ++i) {
    items[i] = (MenuItem){Font4x10Label((Font4x10)i), false};
  }
  DrawItems(menu, "Font", NULL, 0, items, kFont4x10Count);
}

static void DrawBrowsePage(DisplayMenu* menu) {
  MenuBrowseDraw(menu, g_chosen);
}

static void BackUSB(void) {
  ShowPage(kLauncherPageVMs, USBItemIndex());
  ScrollToChosen();
}

static void BackStopSharing(void) {
  ShowPage(kLauncherPageSharing, 0);
}

static void BackManage(void) {
  ShowPage(kLauncherPageVMs, ManageItemIndex());
  ScrollToChosen();
}

static void BackVM(void) {
  ShowManaged(g_vm);
}

static void BackName(void) {
  ShowPage(kLauncherPageVM, g_cloning ? kVMClone : kVMRename);
}

static void BackDelete(void) {
  ShowPage(kLauncherPageVM, kVMDelete);
}

static void BackNew(void) {
  if (g_editing) {
    ShowPage(kLauncherPageVM, kVMEdit);
  } else {
    ShowManaged(g_config.vms->count);
  }
}

static void BackNewDrive(void) {
  ShowFormItem((uint8_t)(kNewDriveA + g_new_drive));
}

static void BackSettings(void) {
  ShowPage(kLauncherPageVMs, SettingsItemIndex());
  ScrollToChosen();
}

static void BackFont(void) {
  ShowPage(kLauncherPageSettings, 0);
}

static void Back(void) {
  const LauncherPageDef* def = &kPageDefs[g_page];
  if (def->back != NULL) {
    def->back();
  }
}

static void ChooseVMs(void) {
  if (g_chosen == SettingsItemIndex()) {
    ShowPage(kLauncherPageSettings, 0);
    return;
  }
  if (g_chosen == USBItemIndex()) {
    ShowPage(kLauncherPageUSB, kUSBCard);
    return;
  }
  if (g_chosen == ManageItemIndex()) {
    ShowManaged(0);
    return;
  }
  g_chosen_vm = g_chosen;
  g_vm_chosen = true;
}

static void ChooseSettings(void) {
  ShowPage(kLauncherPageFont, g_font);
}

static void ChooseFontPage(void) {
  ChooseFont((Font4x10)g_chosen);
}

static void ChooseManage(void) {
  if (g_chosen == g_config.vms->count) {
    StartNewVM();
  } else {
    ShowVM(g_chosen);
  }
}

static void ChooseNew(void) {
  if (FormItem() == kNewCreate) {
    if (g_editing) {
      EditVM();
    } else {
      CreateVM();
    }
    return;
  }
  if (FormItem() == kNewName) {
    ShowFormItem(kNewDriveA);
  } else {
    ShowNewDrive((uint8_t)(FormItem() - kNewDriveA));
  }
}

static void ChooseNewDriveOption(void) {
  ChooseNewDrive();
}

static void ChooseVM(void) {
  if (g_chosen == kVMEdit) {
    StartEditVM();
  } else if (g_chosen == kVMDelete) {
    VMFolderDescribe(g_config.vms, &g_config.vms->vms[g_vm], &g_contents);
    ShowPage(kLauncherPageDelete, kDeleteCancel);
  } else {
    StartNaming(g_chosen == kVMClone);
  }
}

static void ChooseName(void) {
  if (g_cloning) {
    CloneVM();
  } else {
    RenameVM();
  }
}

static void ChooseDelete(void) {
  if (g_chosen == kDeleteConfirm) {
    DeleteVM();
  } else {
    BackDelete();
  }
}

static void ChooseUSB(void) {
  if (g_chosen == kUSBImage) {
    MenuBrowseForUSB();
    return;
  }
  ShareCard();
}

static void ChooseStopSharing(void) {
  if (g_chosen == kStopSharingConfirm) {
    StopSharing();
  } else {
    BackStopSharing();
  }
}

static void ChooseBrowse(void) {
  MenuBrowseChoose(g_chosen);
}

static void BackBrowse(void) {
  MenuBrowseBack();
}

static void Choose(void) {
  const LauncherPageDef* def = &kPageDefs[g_page];
  if (def->choose != NULL) {
    def->choose();
  }
}

static bool HandleKeyName(uint8_t key) {
  return TypeNewName(key);
}

static bool HandleKeyCopying(uint8_t key) {
  (void)key;
  return true;
}

static bool HandleKeyNew(uint8_t key) {
  if (FormItem() == kNewName) {
    return TypeNewName(key);
  }
  return false;
}

static const char* NamePageHint(void) {
  return g_cloning ? "Enter=Clone  Esc=Cancel" : "Enter=Save  Esc=Cancel";
}

static const char* NewPageHint(void) {
  return FormItem() == kNewCreate ? "Enter=Save  Esc=Cancel"
                                  : "Enter=Next  Esc=Cancel";
}

static const char* VMPageName(void) {
  return g_config.vms->vms[g_vm].name;
}

static const LauncherPageDef kPageDefs[kLauncherPageCount] = {
    [kLauncherPageVMs] = {
        .name = "vms",
        .hint = "\x18\x19=Move  Enter=Select",
        .item_count = VMsItemCount,
        .draw = DrawVMsPage,
        .choose = ChooseVMs,
    },
    [kLauncherPageUSB] = {
        .name = "usb",
        .item_count = USBItemCount,
        .draw = DrawUSBPage,
        .choose = ChooseUSB,
        .back = BackUSB,
    },
    [kLauncherPageSharing] = {
        .name = "usb_sharing",
        .hint = "Enter=Disconnect",
        .item_count = SharingItemCount,
        .draw = DrawSharingPage,
        .choose = Disconnect,
        .back = Disconnect,
    },
    [kLauncherPageStopSharing] = {
        .name = "usb_stop",
        .item_count = StopSharingItemCount,
        .draw = DrawStopSharingPage,
        .choose = ChooseStopSharing,
        .back = BackStopSharing,
    },
    [kLauncherPageManage] = {
        .name = "manage",
        .item_count = ManageItemCount,
        .draw = DrawManagePage,
        .choose = ChooseManage,
        .back = BackManage,
    },
    [kLauncherPageVM] = {
        .get_name = VMPageName,
        .item_count = VMItemCount,
        .draw = DrawVMPage,
        .choose = ChooseVM,
        .back = BackVM,
    },
    [kLauncherPageName] = {
        .name = "vm_name",
        .get_hint = NamePageHint,
        .draw = DrawNamePage,
        .choose = ChooseName,
        .back = BackName,
        .handle_key = HandleKeyName,
    },
    [kLauncherPageDelete] = {
        .name = "vm_delete",
        .item_count = DeleteItemCount,
        .draw = DrawDeletePage,
        .choose = ChooseDelete,
        .back = BackDelete,
    },
    [kLauncherPageCopying] = {
        .name = "vm_copying",
        .hint = "",
        .draw = DrawCopyingPage,
        .handle_key = HandleKeyCopying,
    },
    [kLauncherPageNew] = {
        .name = "new_vm",
        .get_hint = NewPageHint,
        .item_count = NewItemCount,
        .draw = DrawNewPage,
        .choose = ChooseNew,
        .back = BackNew,
        .handle_key = HandleKeyNew,
    },
    [kLauncherPageNewDrive] = {
        .name = "new_vm_drive",
        .item_count = NewDriveItemCount,
        .draw = DrawNewDrivePage,
        .choose = ChooseNewDriveOption,
        .back = BackNewDrive,
    },
    [kLauncherPageSettings] = {
        .name = "settings",
        .item_count = SettingsItemCount,
        .draw = DrawSettingsPage,
        .choose = ChooseSettings,
        .back = BackSettings,
    },
    [kLauncherPageFont] = {
        .name = "font",
        .item_count = FontItemCount,
        .draw = DrawFontPage,
        .choose = ChooseFontPage,
        .back = BackFont,
    },
    [kLauncherPageBrowse] = {
        .name = "browse",
        .hint = "\x18\x19=Move  Enter=Open  Backspace=Up a folder  Esc=Back",
        .item_count = MenuBrowseItemCount,
        .draw = DrawBrowsePage,
        .choose = ChooseBrowse,
        .back = BackBrowse,
    },
};

static void HandleKey(SouthbridgeKeyEvent event) {
  if (event.state != kSouthbridgeKeyPressed) {
    return;
  }
  const uint8_t count = PageItemCount();
  const LauncherPageDef* def = &kPageDefs[g_page];
  if (def->handle_key != NULL && def->handle_key(event.key)) {
    return;
  }
  switch (event.key) {
    case kKeyUp:
      if (count > 0) {
        g_chosen = (uint8_t)((g_chosen + count - 1) % count);
        g_dirty = true;
      }
      ScrollToChosen();
      break;
    case kKeyDown:
      if (count > 0) {
        g_chosen = (uint8_t)((g_chosen + 1) % count);
        g_dirty = true;
      }
      ScrollToChosen();
      break;
    case kKeyPageUp:
      if (count > 0) {
        g_chosen =
            g_chosen > kListRows ? (uint8_t)(g_chosen - kListRows) : 0;
        g_dirty = true;
      }
      ScrollToChosen();
      break;
    case kKeyPageDown:
      if (count > 0) {
        g_chosen = g_chosen + kListRows < count
                       ? (uint8_t)(g_chosen + kListRows)
                       : (uint8_t)(count - 1);
        g_dirty = true;
      }
      ScrollToChosen();
      break;
    case kKeyEnter:
    case kKeySpace:
      Choose();
      break;
    case kKeyBackspace:
      if (g_page == kLauncherPageBrowse && MenuBrowseHandleBackspace()) {
        break;
      }
      Back();
      break;
    case kKeyEsc:
      Back();
      break;
    default:
      break;
  }
}

static void LauncherDraw(uint64_t now_us) {
  if (!g_open) {
    return;
  }
  if (g_page == kLauncherPageSharing &&
      now_us - g_status_drawn_us >= kStatusRefreshUs) {
    g_dirty = true;
  }
  if (!g_dirty) {
    return;
  }
  DisplayMenu* menu = DisplayBeginMenu();
  if (menu == NULL) {
    return;
  }
  const LauncherPageDef* def = &kPageDefs[g_page];
  if (def->draw != NULL) {
    def->draw(menu);
  }
  if (g_page == kLauncherPageSharing) {
    g_status_drawn_us = now_us;
  }
  DisplayEndMenu();
  g_dirty = false;
}

void LauncherRun(const LauncherConfig* config) {
  g_config = *config;
  const MenuBrowseConfig browse_config = {
      .show_browse_page = OnBrowseShowPage,
      .on_selected_for_new_vm = OnBrowseSelectedForNewVM,
      .on_selected_for_usb = OnBrowseSelectedForUSB,
      .on_back_for_new_vm = OnBrowseBackForNewVM,
      .on_back_for_usb = OnBrowseBackForUSB,
      .set_chosen = OnBrowseSetChosen,
      .set_dirty = OnBrowseSetDirty,
  };
  MenuBrowseInit(&browse_config);

  StartNoteRecord note = {0};
  const bool noted = TakeStartNote(&note);
  VMListRead(g_config.vms);
  uint8_t highlighted = 0;
  const bool settings_read = ReadSettings(g_config.settings, &highlighted);
  if (DiskImageMountCard() != kDiskImageOpen) {
    return;
  }
  if (noted) {
    SYSTEM_INFO(
        note.intent == kStartIntentRestart ? "restarting" : "powered_off",
        Y_LOG_STRING("name", note.name));
    const uint8_t index = VMListFind(g_config.vms, note.name);
    if (index < kVMListMax) {
      highlighted = index;
      if (note.intent == kStartIntentRestart && TakeVM(index)) {
        return;
      }
    }
  } else if (
      settings_read && g_config.settings->boot_delay == 0 &&
      g_config.vms->count > 0 && TakeVM(highlighted)) {
    return;
  }

  g_open = true;
  g_vms_top = 0;
  ShowPage(kLauncherPageVMs, highlighted);
  ScrollToChosen();

  const absolute_time_t start_at = make_timeout_time_ms(
      g_config.settings->boot_delay * kMillisecondsPerSecond);
  bool counting = !noted && g_config.settings->boot_delay > 0 &&
                  g_config.vms->count > 0;
  absolute_time_t next_status_at = get_absolute_time();

  for (;;) {
    Command command;
    if (CommandPoll(&command)) {
      ReportRunCommand(&command);
    }
    g_vm_chosen = false;
    SouthbridgeKeyEvent key;
    if (SouthbridgePollKey(&key) || TypingNext(&key)) {
      counting = false;
      g_countdown = 0;
      g_dirty = true;
      HandleKey(key);
      UpdateChoosingRows();
    }
    if (counting) {
      const int64_t left_us =
          absolute_time_diff_us(get_absolute_time(), start_at);
      if (left_us <= 0) {
        counting = false;
        g_countdown = 0;
        g_dirty = true;
        g_chosen_vm = g_chosen;
        g_vm_chosen = true;
      } else {
        const uint8_t seconds = (uint8_t)(
            (left_us + kMicrosecondsPerSecond - 1) / kMicrosecondsPerSecond);
        if (seconds != g_countdown) {
          g_countdown = seconds;
          g_dirty = true;
        }
      }
    }
    if (g_vm_chosen && TakeVM(g_chosen_vm)) {
      g_open = false;
      return;
    }
    if (time_reached(next_status_at)) {
      next_status_at = make_timeout_time_ms(kStatusUpdateMs);
      UpdateChoosingRows();
    }
    LauncherDraw(time_us_64());
    sleep_ms(kPausedPollMs);
  }
}
