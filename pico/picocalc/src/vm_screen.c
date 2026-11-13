#include "vm_screen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "disk_image.h"
#include "display.h"
#include "ui_box.h"
#include "ui_page.h"
#include "ui_status.h"
#include "vm_config_screen.h"
#include "vm_delete_screen.h"
#include "vm_name_screen.h"

typedef enum VMItem {
  kVMConfigure = 0,
  kVMRename,
  kVMClone,
  kVMDelete,
  kVMItemCount,
} VMItem;

static const UIBoxItem kVMItems[kVMItemCount] = {
    [kVMConfigure] = {"Configure", true},
    [kVMRename] = {"Rename", true},
    [kVMClone] = {"Clone", true},
    [kVMDelete] = {"Delete", false},
};

extern const Screen kVMScreen;

static VMManager* g_vm_manager;
static uint8_t g_chosen;
// Whether what is on the panel is out of date.
static bool g_dirty;
// The VM, an index in the list; the names of the images in its drives,
// empty for none; whether each drive is there; and whether its vm.cfg read.
static uint8_t g_vm;
static char g_drives[kVMDrives][kDiskImageNameLength + 1];
static bool g_present[kVMDrives];
static bool g_readable;

ScreenResult VMScreenOpen(VMManager* vm_manager, uint8_t index) {
  g_vm_manager = vm_manager;
  g_vm = index;
  VMConfig config;
  char problem[kVMProblemLength + 1];
  g_readable = VMConfigRead(&vm_manager->vms->vms[index], &config, problem);
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    g_drives[drive][0] = '\0';
    if (config.drives[drive][0] != '\0') {
      DiskImageName(config.drives[drive], g_drives[drive]);
    }
    g_present[drive] = drive == kVMDriveFD1 ||
                       (drive == kVMDriveFD2 ? config.present[drive]
                                             : config.drives[drive][0] != '\0');
  }
  g_chosen = kVMConfigure;
  g_dirty = true;
  return ScreenOpen(&kVMScreen);
}

static ScreenResult Choose(void) {
  switch (g_chosen) {
    case kVMConfigure:
      return VMConfigScreenOpenExisting(g_vm_manager, g_vm);
    case kVMDelete:
      return VMDeleteScreenOpen(g_vm_manager, g_vm);
    default:
      return VMNameScreenOpen(g_vm_manager, g_vm, g_chosen == kVMClone);
  }
}

static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  // A key held down repeats, which moves through the items as a press does.
  if (event.state == kSouthbridgeKeyReleased) {
    return ScreenStays();
  }
  UIStatusClearProblem();
  if (UIPageIsMoveKey(event.key)) {
    if (UIPageMove(event.key, kVMItemCount, &g_chosen)) {
      g_dirty = true;
    }
    return ScreenStays();
  }
  switch (event.key) {
    case kKeyCodeEnter:
    case kKeyCodeSpace:
      return Choose();
    case kKeyCodeBackspace:
    case kKeyCodeEsc:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

// Back from a screen opened from the page: to Manage VMs too, if it changed
// the card.
static ScreenResult Resume(void) {
  if (g_vm_manager->changed) {
    return ScreenBack();
  }
  g_dirty = true;
  return ScreenStays();
}

// What a drive holds, for the page: an image's name, "empty" for a floppy
// drive with none, or "disabled" for a drive not there.
static const char* DriveShown(uint8_t drive, const char* name) {
  if (name[0] != '\0') {
    return name;
  }
  return g_present[drive] ? "empty" : "disabled";
}

// The images in the VM's drives, over what can be done to it.
static void Draw(Y_UNUSED uint64_t now_us) {
  if (!g_dirty) {
    return;
  }
  DisplayMenu* menu = DisplayBeginMenu();
  if (menu == NULL) {
    return;
  }
  // A line for each pair of drives - FD1 and FD2, HD1 and HD2 - that holds
  // anything,
  // with room for any name, which the box cuts to its width.
  char lines[kVMDrives / 2][64];
  const char* line_pointers[kVMDrives / 2];
  uint8_t line_count = 0;
  for (uint8_t first = 0; first < kVMDrives; first += 2) {
    char label_a[8];
    char label_b[8];
    snprintf(label_a, sizeof(label_a), "%s:", kVMDriveLabels[first]);
    snprintf(label_b, sizeof(label_b), "%s:", kVMDriveLabels[first + 1]);
    const char* a = g_drives[first];
    const char* b = g_drives[first + 1];
    if (a[0] == '\0' && b[0] == '\0') {
      continue;
    }
    snprintf(
        lines[line_count], sizeof(lines[line_count]),
        "%-4s %-12.12s %-4s %.12s", label_a, DriveShown(first, a), label_b,
        DriveShown((uint8_t)(first + 1), b));
    line_pointers[line_count] = lines[line_count];
    ++line_count;
  }
  if (!g_readable) {
    line_pointers[0] = "Its vm.cfg has a problem.";
    line_count = 1;
  } else if (line_count == 0) {
    line_pointers[0] = "No disks.";
    line_count = 1;
  }
  UIPageDrawItems(
      menu, g_vm_manager->vms->vms[g_vm].name, line_pointers, line_count,
      kVMItems, kVMItemCount, g_chosen, true);
  DisplayEndMenu();
  g_dirty = false;
}

static const char* Hint(void) {
  return "\x18\x19=Move  Enter=Select  Esc=Back";
}

static const char* PageName(void) { return "vm"; }

const Screen kVMScreen = {
    .handle_key = HandleKey,
    .draw = Draw,
    .resume = Resume,
    .hint = Hint,
    .page_name = PageName,
};
