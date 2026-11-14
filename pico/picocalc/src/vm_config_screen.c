#include "vm_config_screen.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "box.h"
#include "disk_image.h"
#include "display.h"
#include "list_box.h"
#include "pico/stdlib.h"
#include "runtime_log.h"
#include "status_bar.h"
#include "vm_drive_config_screen.h"

enum {
  kMicrosecondsPerMillisecond = 1000,
};

typedef enum FormItem {
  kFormName = 0,
  // The drives' items, in the order of the VM's drives.
  kFormDriveA,
  kFormDriveB,
  kFormDriveC,
  kFormDriveD,
  kFormCreate,
  kNumFormItems,
} FormItem;

extern const Screen kVMConfigScreen;

static VMManager* g_vm_manager;
// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;
// Whether the form configures the existing VM at g_vm, an index in the list,
// rather than making a new one; it then has no name, and saves rather than
// creates.
static bool g_is_existing;
static uint8_t g_vm;
// A new VM's name, and what each drive is to hold.
static char g_name[kFSMaxFolderName + 1];
static uint8_t g_name_length;
static VMDriveChoice g_drives[kNumVMDrives];

// The form's first item: its name for a new VM; for an existing one, at
// the drives. g_list_box.selected counts from it.
static uint8_t FirstItem(void) {
  return g_is_existing ? kFormDriveA : kFormName;
}

static uint8_t SelectedItem(void) {
  return (uint8_t)(g_list_box.selected + FirstItem());
}

static void SelectItem(uint8_t item) {
  g_list_box.selected = (uint8_t)(item - FirstItem());
  g_list_box.is_dirty = true;
}

ScreenResult VMConfigScreenOpenNew(VMManager* vm_manager) {
  g_vm_manager = vm_manager;
  g_is_existing = false;
  g_name[0] = '\0';
  g_name_length = 0;
  for (uint8_t drive = 0; drive < kNumVMDrives; ++drive) {
    g_drives[drive].path[0] = '\0';
    g_drives[drive].is_blank = drive == kVMDriveHD1;
    g_drives[drive].is_present = drive == kVMDriveFD1 || drive == kVMDriveHD1;
  }
  SelectItem(kFormName);
  return ScreenOpen(&kVMConfigScreen);
}

ScreenResult VMConfigScreenOpenExisting(VMManager* vm_manager, uint8_t index) {
  g_vm_manager = vm_manager;
  g_is_existing = true;
  g_vm = index;
  static VMConfig config;
  char problem[kVMProblemLength + 1];
  VMConfigRead(&vm_manager->vms->vms[index], &config, problem);
  for (uint8_t drive = 0; drive < kNumVMDrives; ++drive) {
    FSCopyPath(config.drives[drive], g_drives[drive].path);
    g_drives[drive].is_blank = false;
    g_drives[drive].is_present =
        drive == kVMDriveFD1 ||
        (drive == kVMDriveFD2 ? config.is_present[drive]
                              : config.drives[drive][0] != '\0');
  }
  SelectItem(kFormDriveA);
  return ScreenOpen(&kVMConfigScreen);
}

// The drives as the form has them, its strings the form's.
static void FormDrives(VMListDrives* drives) {
  for (uint8_t drive = 0; drive < kNumVMDrives; ++drive) {
    drives->paths[drive] =
        g_drives[drive].is_present ? g_drives[drive].path : NULL;
    drives->is_blank[drive] = g_drives[drive].is_blank;
  }
}

static __noinline ScreenResult Create(void) {
  VMListDrives drives;
  FormDrives(&drives);
  VMEntry created;
  char problem[kVMProblemLength + 1];
  char what[kListBoxNarrowColumns];
  snprintf(what, sizeof(what), "Writing %s's blank disks...", g_name);
  const uint64_t start_us = time_us_64();
  // Blank disks are written through the guest's RAM, which is zeroed after.
  FSTransfer transfer = VMManagerBeginCopy(g_vm_manager, "New VM", what);
  const VMListResult result =
      VMListCreate(g_name, &drives, &transfer, &created, problem);
  VMManagerEndCopy(g_vm_manager);
  if (result == kVMListRefused) {
    StatusBarSetProblem(problem);
    return ScreenStays();
  }
  VMManagerNoteChange(g_vm_manager, created.name, false);
  if (result == kVMListDone) {
    SYSTEM_INFO(
        "vm_created", Y_LOG_STRING("name", created.name),
        Y_LOG_BOOL("is_fd1_blank", drives.is_blank[kVMDriveFD1]),
        Y_LOG_BOOL("is_fd2_blank", drives.is_blank[kVMDriveFD2]),
        Y_LOG_BOOL("is_hd1_blank", drives.is_blank[kVMDriveHD1]),
        Y_LOG_BOOL("is_hd2_blank", drives.is_blank[kVMDriveHD2]),
        Y_LOG_UINT(
            "ms", (time_us_64() - start_us) / kMicrosecondsPerMillisecond));
  } else {
    StatusBarSetProblem(problem);
    SYSTEM_WARN(
        "vm_not_created", Y_LOG_STRING("name", created.name),
        Y_LOG_STRING("problem", problem));
  }
  return ScreenBack();
}

static __noinline ScreenResult Save(void) {
  const VMEntry vm = g_vm_manager->vms->vms[g_vm];
  VMListDrives drives;
  FormDrives(&drives);
  char problem[kVMProblemLength + 1];
  char what[kListBoxNarrowColumns];
  snprintf(what, sizeof(what), "Writing %s's blank disks...", vm.name);
  FSTransfer transfer = VMManagerBeginCopy(g_vm_manager, "Configure", what);
  const VMListResult result = VMListEdit(&vm, &drives, &transfer, problem);
  VMManagerEndCopy(g_vm_manager);
  if (result == kVMListRefused) {
    StatusBarSetProblem(problem);
    return ScreenStays();
  }
  VMManagerNoteChange(g_vm_manager, vm.name, true);
  if (result == kVMListDone) {
    SYSTEM_INFO("vm_edited", Y_LOG_STRING("name", vm.name));
  } else {
    VMManagerShowProblem("vm_not_edited", &vm, problem);
  }
  return ScreenBack();
}

// A key while the name is selected: a character a name can have to add, and
// Backspace to take the last off. Returns whether the key was the name's.
static bool TypeName(uint8_t key) {
  if (key == kKeyCodeBackspace) {
    if (g_name_length > 0) {
      g_name[--g_name_length] = '\0';
      g_list_box.is_dirty = true;
    }
    return true;
  }
  if (key >= 0x80 || !FSIsFolderNameCharacter((char)key)) {
    return false;
  }
  if (g_name_length < kFSMaxFolderName) {
    g_name[g_name_length++] = (char)toupper(key);
    g_name[g_name_length] = '\0';
    g_list_box.is_dirty = true;
  }
  return true;
}

// Carry out the item selected: create or save the VM, move on from the
// name to the first drive, or open a drive's choices.
static ScreenResult OnSelect(void) {
  const uint8_t item = SelectedItem();
  if (item == kFormCreate) {
    return g_is_existing ? Save() : Create();
  }
  if (item == kFormName) {
    SelectItem(kFormDriveA);
    return ScreenStays();
  }
  const uint8_t drive = (uint8_t)(item - kFormDriveA);
  return VMDriveConfigScreenOpen(drive, &g_drives[drive]);
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  if (!ListBoxIsKeyPress(event)) {
    return ScreenStays();
  }
  // Backspace erases the name rather than leaving the form, wherever it is:
  // Esc leaves it.
  if ((SelectedItem() == kFormName && TypeName(event.key)) ||
      event.key == kKeyCodeBackspace) {
    return ScreenStays();
  }
  const uint8_t num_items = (uint8_t)(kNumFormItems - FirstItem());
  switch (ListBoxHandleKeyCode(&g_list_box, event.key, num_items)) {
    case kListBoxActionSelect:
      return OnSelect();
    case kListBoxActionBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

// A new VM's name, typed in place with a cursor while it is selected, and the
// drives, with what each holds - or for an existing VM, the drives alone,
// under its name.
static void OnDraw(DisplayBox* box) {
  char rows[kNumFormItems][kListBoxNarrowColumns];
  BoxItem items[kNumFormItems];
  const char* cursor =
      SelectedItem() == kFormName && g_name_length < kFSMaxFolderName ? "_"
                                                                      : "";
  snprintf(
      rows[kFormName], sizeof(rows[kFormName]), "%-15s %s%s", "Name", g_name,
      cursor);
  items[kFormName] = (BoxItem){rows[kFormName], false};
  for (uint8_t drive = 0; drive < kNumVMDrives; ++drive) {
    const uint8_t item = (uint8_t)(kFormDriveA + drive);
    const char* what = "empty";
    char name[kDiskImageNameLength + 1];
    if (!g_drives[drive].is_present) {
      what = "disabled";
    } else if (g_drives[drive].is_blank) {
      what = drive >= kVMDriveHD1 ? "new disk image" : "new floppy image";
    } else if (g_drives[drive].path[0] != '\0') {
      DiskImageName(g_drives[drive].path, name);
      what = name;
    }
    snprintf(
        rows[item], sizeof(rows[item]), "%-15s %s", kVMDriveNames[drive], what);
    items[item] = (BoxItem){rows[item], true};
  }
  items[kFormCreate] = (BoxItem){g_is_existing ? "Save" : "Create", false};
  ListBoxDrawItems(
      box, g_is_existing ? g_vm_manager->vms->vms[g_vm].name : "New VM", NULL,
      0, &items[FirstItem()], (uint8_t)(kNumFormItems - FirstItem()),
      g_list_box.selected, true);
}

static const char* Hint(void) {
  return SelectedItem() == kFormName
             ? "Type the name  \x18\x19=Move  Backspace=Erase  Esc=Cancel"
             : "\x18\x19=Move  Enter=Select  Esc=Cancel";
}

static const char* PageName(void) {
  return g_is_existing ? "vm_configure" : "vm_create";
}

const Screen kVMConfigScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .hint = Hint,
    .page_name = PageName,
    .is_dirty = &g_list_box.is_dirty,
};
