#include "vm_form_screen.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "disk_image.h"
#include "display.h"
#include "pico/stdlib.h"
#include "runtime_log.h"
#include "status_bar.h"
#include "ui_box.h"
#include "ui_page.h"
#include "vm_drive_screen.h"

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
  kFormItemCount,
} FormItem;

extern const Screen kVMFormScreen;

static ManageState* g_manage;
// The item chosen, and whether the page is out of date.
static UIPage g_page;
// Whether the form is editing the drives of the VM at g_vm, an index in the
// list, rather than making a new one; it then has no name, and saves rather
// than creates.
static bool g_editing;
static uint8_t g_vm;
// A new VM's name, and what each drive is to hold.
static char g_name[kFSMaxFolderName + 1];
static uint8_t g_name_length;
static VMFormDrive g_drives[kVMDrives];

// The form's first item: its name for a new VM; editing one, it starts at
// the drives. g_page.chosen counts from it.
static uint8_t FirstItem(void) { return g_editing ? kFormDriveA : kFormName; }

static uint8_t ChosenItem(void) {
  return (uint8_t)(g_page.chosen + FirstItem());
}

static void ChooseItem(uint8_t item) {
  g_page.chosen = (uint8_t)(item - FirstItem());
  g_page.dirty = true;
}

ScreenResult VMFormScreenOpenNew(ManageState* manage) {
  g_manage = manage;
  g_editing = false;
  g_name[0] = '\0';
  g_name_length = 0;
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    g_drives[drive].path[0] = '\0';
    g_drives[drive].blank = drive == kVMDriveHD1;
    g_drives[drive].present = drive == kVMDriveFD1 || drive == kVMDriveHD1;
  }
  ChooseItem(kFormName);
  return ScreenOpen(&kVMFormScreen);
}

ScreenResult VMFormScreenOpenEdit(ManageState* manage, uint8_t index) {
  g_manage = manage;
  g_editing = true;
  g_vm = index;
  static VMConfig config;
  char problem[kVMProblemLength + 1];
  VMConfigRead(&manage->vms->vms[index], &config, problem);
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    FSCopyPath(config.drives[drive], g_drives[drive].path);
    g_drives[drive].blank = false;
    g_drives[drive].present =
        drive == kVMDriveFD1 ||
        (drive == kVMDriveFD2 ? config.present[drive]
                              : config.drives[drive][0] != '\0');
  }
  ChooseItem(kFormDriveA);
  return ScreenOpen(&kVMFormScreen);
}

// The drives as the form has them, its strings the form's.
static void FormDrives(VMListDrives* drives) {
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    drives->paths[drive] =
        g_drives[drive].present ? g_drives[drive].path : NULL;
    drives->blank[drive] = g_drives[drive].blank;
  }
}

static __noinline ScreenResult Create(void) {
  VMListDrives drives;
  FormDrives(&drives);
  VMEntry created;
  char problem[kVMProblemLength + 1];
  char what[kUIPageNarrowColumns];
  snprintf(what, sizeof(what), "Writing %s's blank disks...", g_name);
  const uint64_t start_us = time_us_64();
  // Blank disks are written through the guest's RAM, which is zeroed after.
  FSTransfer transfer = ManageBeginCopy(g_manage, "New VM", what);
  const VMListResult result =
      VMListCreate(g_name, &drives, &transfer, &created, problem);
  ManageEndCopy(g_manage);
  if (result == kVMListRefused) {
    StatusBarSetProblem(problem);
    return ScreenStays();
  }
  ManageNoteChange(g_manage, created.name, false);
  if (result == kVMListDone) {
    SYSTEM_INFO(
        "vm_created", Y_LOG_STRING("name", created.name),
        Y_LOG_BOOL("blank_fd1", drives.blank[kVMDriveFD1]),
        Y_LOG_BOOL("blank_fd2", drives.blank[kVMDriveFD2]),
        Y_LOG_BOOL("blank_hd1", drives.blank[kVMDriveHD1]),
        Y_LOG_BOOL("blank_hd2", drives.blank[kVMDriveHD2]),
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
  const VMEntry vm = g_manage->vms->vms[g_vm];
  VMListDrives drives;
  FormDrives(&drives);
  char problem[kVMProblemLength + 1];
  char what[kUIPageNarrowColumns];
  snprintf(what, sizeof(what), "Writing %s's blank disks...", vm.name);
  FSTransfer transfer = ManageBeginCopy(g_manage, "Edit drives", what);
  const VMListResult result = VMListEdit(&vm, &drives, &transfer, problem);
  ManageEndCopy(g_manage);
  if (result == kVMListRefused) {
    StatusBarSetProblem(problem);
    return ScreenStays();
  }
  ManageNoteChange(g_manage, vm.name, true);
  if (result == kVMListDone) {
    SYSTEM_INFO("vm_edited", Y_LOG_STRING("name", vm.name));
  } else {
    ManageShowProblem("vm_not_edited", &vm, problem);
  }
  return ScreenBack();
}

// A key while the name is chosen: a character a name can have to add, and
// Backspace to take the last off. Returns whether the key was the name's.
static bool TypeName(uint8_t key) {
  if (key == kKeyCodeBackspace) {
    if (g_name_length > 0) {
      g_name[--g_name_length] = '\0';
      g_page.dirty = true;
    }
    return true;
  }
  if (key >= 0x80 || !FSIsFolderNameCharacter((char)key)) {
    return false;
  }
  if (g_name_length < kFSMaxFolderName) {
    g_name[g_name_length++] = (char)toupper(key);
    g_name[g_name_length] = '\0';
    g_page.dirty = true;
  }
  return true;
}

static ScreenResult Choose(void) {
  const uint8_t item = ChosenItem();
  if (item == kFormCreate) {
    return g_editing ? Save() : Create();
  }
  if (item == kFormName) {
    ChooseItem(kFormDriveA);
    return ScreenStays();
  }
  const uint8_t drive = (uint8_t)(item - kFormDriveA);
  return VMDriveScreenOpen(drive, &g_drives[drive]);
}

static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  if (!UIPageKeyPressed(event)) {
    return ScreenStays();
  }
  // Backspace erases the name rather than leaving the form, wherever it is:
  // Esc leaves it.
  if ((ChosenItem() == kFormName && TypeName(event.key)) ||
      event.key == kKeyCodeBackspace) {
    return ScreenStays();
  }
  const uint8_t count = (uint8_t)(kFormItemCount - FirstItem());
  switch (UIPageTakeKey(&g_page, event.key, count)) {
    case kUIPageKeyChoose:
      return Choose();
    case kUIPageKeyBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

// Back from a drive's choices, with the drive chosen still.
static ScreenResult Resume(void) {
  g_page.dirty = true;
  return ScreenStays();
}

// A new VM's name, typed in place with a cursor while it is chosen, and the
// drives, with what each holds - or editing a VM's drives, the drives alone,
// under its name.
static void Draw(Y_UNUSED uint64_t now_us) {
  DisplayMenu* menu = UIPageBeginDraw(&g_page);
  if (menu == NULL) {
    return;
  }
  char rows[kFormItemCount][kUIPageNarrowColumns];
  UIBoxItem items[kFormItemCount];
  const char* cursor =
      ChosenItem() == kFormName && g_name_length < kFSMaxFolderName ? "_" : "";
  snprintf(
      rows[kFormName], sizeof(rows[kFormName]), "%-15s %s%s", "Name", g_name,
      cursor);
  items[kFormName] = (UIBoxItem){rows[kFormName], false};
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    const uint8_t item = (uint8_t)(kFormDriveA + drive);
    const char* what = "empty";
    char name[kDiskImageNameLength + 1];
    if (!g_drives[drive].present) {
      what = "disabled";
    } else if (g_drives[drive].blank) {
      what = drive >= kVMDriveHD1 ? "new disk image" : "new floppy image";
    } else if (g_drives[drive].path[0] != '\0') {
      DiskImageName(g_drives[drive].path, name);
      what = name;
    }
    snprintf(
        rows[item], sizeof(rows[item]), "%-15s %s", kVMDriveNames[drive], what);
    items[item] = (UIBoxItem){rows[item], true};
  }
  items[kFormCreate] = (UIBoxItem){g_editing ? "Save" : "Create", false};
  UIPageDrawItems(
      menu, g_editing ? g_manage->vms->vms[g_vm].name : "New VM", NULL, 0,
      &items[FirstItem()], (uint8_t)(kFormItemCount - FirstItem()),
      g_page.chosen, true);
  UIPageEndDraw(&g_page);
}

static const char* Hint(void) {
  return ChosenItem() == kFormName
             ? "Type the name  \x18\x19=Move  Backspace=Erase  Esc=Cancel"
             : "\x18\x19=Move  Enter=Select  Esc=Cancel";
}

static const char* PageName(void) { return "new_vm"; }

const Screen kVMFormScreen = {
    .handle_key = HandleKey,
    .draw = Draw,
    .resume = Resume,
    .hint = Hint,
    .page_name = PageName,
};
