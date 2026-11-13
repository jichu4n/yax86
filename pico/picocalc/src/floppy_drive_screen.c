#include "floppy_drive_screen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "browser_screen.h"
#include "disk_image.h"
#include "display.h"
#include "fs.h"
#include "list_box.h"
#include "menu_box.h"
#include "vm_config.h"

typedef enum DriveItem {
  kDriveInsert = 0,
  // Shown only while there is a disk to eject.
  kDriveEject,
  kDriveItemCount,
} DriveItem;

extern const Screen kFloppyDriveScreen;

static RunningVM* g_vm;
static FloppyDrive g_drive;
// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;
// The images in the drives, which the browser marks, and where it writes the
// image selected and its length.
static const char* g_in_drives[kFloppyDrives];
static char g_found_path[kFSMaxPath + 1];
static uint32_t g_found_size;

ScreenResult FloppyDriveScreenOpen(RunningVM* vm, FloppyDrive drive) {
  g_vm = vm;
  g_drive = drive;
  g_list_box.selected = kDriveInsert;
  g_list_box.dirty = true;
  return ScreenOpen(&kFloppyDriveScreen);
}

static bool DriveEmpty(void) {
  return FloppyGetImage(g_drive)->status == kDiskImageEjected;
}

static uint8_t ItemCount(void) {
  return DriveEmpty() ? kDriveEject : kDriveItemCount;
}

// Open the browser to find an image for the drive. It starts in the folder
// of the image in the drive, or else of the one in the other drive, since
// the next disk of a set is likely beside it, or else in yax86. The images
// in the drives are marked, and the one in this drive selected. Kept out of
// line, so that ejecting a disk does not carry its folder on the stack.
static __noinline ScreenResult Browse(void) {
  for (uint8_t drive = 0; drive < kFloppyDrives; ++drive) {
    const DiskImage* image = FloppyGetImage((FloppyDrive)drive);
    g_in_drives[drive] = image->status == kDiskImageOpen ? image->path : NULL;
  }
  const DiskImage* image = FloppyGetImage(g_drive);
  if (image->status != kDiskImageOpen) {
    image = FloppyGetImage(
        g_drive == kFloppyDriveA ? kFloppyDriveB : kFloppyDriveA);
  }
  char folder[kFSMaxPath + 1];
  if (image->status == kDiskImageOpen) {
    FSFolderOf(image->path, folder);
  } else {
    FSCopyPath("yax86", folder);
  }
  return BrowserScreenOpen(
      kFileBrowserFloppies, folder, g_in_drives[g_drive], g_in_drives,
      g_found_path, &g_found_size);
}

// Carry out the item selected: open the browser, or eject the disk, which
// leaves Eject gone and the first item selected.
static ScreenResult OnSelect(void) {
  if (g_list_box.selected == kDriveInsert) {
    return Browse();
  }
  RunningVMEjectFloppy(g_vm, g_drive);
  g_list_box.selected = kDriveInsert;
  g_list_box.dirty = true;
  return ScreenStays();
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxTakeEvent(&g_list_box, event, ItemCount())) {
    case kListBoxKeySelect:
      return OnSelect();
    case kListBoxKeyBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

// Back from the browser: an image selected goes in the drive, and closes the
// menu. Without one, the page shows again with the first item selected.
static ScreenResult OnResume(void) {
  if (g_found_path[0] != '\0') {
    RunningVMInsertFloppy(g_vm, g_drive, g_found_path);
    return ScreenClose();
  }
  g_list_box.selected = kDriveInsert;
  g_list_box.dirty = true;
  return ScreenStays();
}

// What the drive holds, over its items.
static bool OnDraw(DisplayMenu* menu, Y_UNUSED uint64_t now_us) {
  if (!ListBoxTakeDirty(&g_list_box)) {
    return false;
  }
  const DiskImage* image = FloppyGetImage(g_drive);
  const bool empty = DriveEmpty();
  const char* in_drive = "the built-in boot floppy";
  char name[kDiskImageNameLength + 1];
  if (image->status == kDiskImageOpen) {
    DiskImageName(image->path, name);
    in_drive = name;
  } else if (empty) {
    in_drive = "empty";
  }
  char line[kListBoxNarrowColumns];
  snprintf(line, sizeof(line), "In the drive: %s", in_drive);
  const char* const lines[] = {
      line,
      "Change disks only while DOS is",
      "not using the drive.",
  };
  const MenuBoxItem items[kDriveItemCount] = {
      [kDriveInsert] = {empty ? "Choose disk image" : "Swap disk image", true},
      [kDriveEject] = {"Eject", false},
  };
  ListBoxDrawItems(
      menu, kVMDriveNames[kVMDriveFD1 + g_drive], lines,
      sizeof(lines) / sizeof(lines[0]), items, ItemCount(), g_list_box.selected,
      false);
  return true;
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) {
  return g_drive == kFloppyDriveA ? "fd1" : "fd2";
}

const Screen kFloppyDriveScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .on_resume = OnResume,
    .hint = Hint,
    .page_name = PageName,
};
