#include "floppy_drive_screen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "box.h"
#include "browser_screen.h"
#include "disk_image.h"
#include "display.h"
#include "emulator_status_bar.h"
#include "fs.h"
#include "list_box.h"
#include "runtime_log.h"
#include "vm_config.h"

typedef enum DriveItem {
  kDriveInsert = 0,
  // Shown only while there is a disk to eject.
  kDriveEject,
  kDriveItemCount,
} DriveItem;

extern const Screen kFloppyDriveScreen;

// Each floppy drive's name in the logs, and its page's name.
static const char* const kDriveNames[kFloppyDrives] = {
    [kFloppyDriveA] = "fd1",
    [kFloppyDriveB] = "fd2",
};

// The caller's emulator session, and the drive whose page this is.
static EmulatorSession* g_session;
static FloppyDrive g_drive;
// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;
// The images in the drives, which the browser marks, and where it writes the
// image selected and its length.
static const char* g_in_drives[kFloppyDrives];
static char g_found_path[kFSMaxPath + 1];
static uint32_t g_found_size;

ScreenResult FloppyDriveScreenOpen(
    EmulatorSession* session, FloppyDrive drive) {
  g_session = session;
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

// Save what the drive now holds to the VM's vm.cfg, so that the next start
// has it. Without a VM there is nothing to save to.
static void SaveDrive(void) {
  VMConfig* config = g_session->config;
  if (config == NULL) {
    return;
  }
  const DiskImage* image = FloppyGetImage(g_drive);
  const char* path = image->status == kDiskImageOpen ? image->path : "";
  char problem[kVMProblemLength + 1];
  if (!VMConfigSaveDrive(config, kVMDriveFD1 + g_drive, path, problem)) {
    char message[kTextColumns + 1];
    snprintf(
        message, sizeof(message), "Could not save %s: %.50s",
        kVMDriveLabels[g_drive], problem);
    EmulatorStatusBarPostMessage(message);
    SYSTEM_WARN(
        "drive_not_saved", Y_LOG_STRING("drive", kDriveNames[g_drive]),
        Y_LOG_STRING("problem", problem));
    return;
  }
  FSCopyPath(path, config->drives[kVMDriveFD1 + g_drive]);
  SYSTEM_INFO(
      "drive_saved", Y_LOG_STRING("drive", kDriveNames[g_drive]),
      Y_LOG_STRING("path", path));
}

// Take the disk out of the drive, say so, and save the empty drive.
static void Eject(void) {
  FloppyEject(g_session->fdc, g_drive);
  char message[kTextColumns + 1];
  snprintf(
      message, sizeof(message), "Ejected the disk from %s",
      kVMDriveLabels[g_drive]);
  EmulatorStatusBarPostMessage(message);
  DISK_INFO("ejected", Y_LOG_STRING("drive", kDriveNames[g_drive]));
  SaveDrive();
}

// Put the image at path, a path from the card's root, in the drive, say
// whether it went in, and save what the drive now holds. An image already in
// the other drive stays there: two drives writing one image would each
// overwrite the other's sectors.
static void Insert(const char* path) {
  char name[kDiskImageNameLength + 1];
  DiskImageName(path, name);
  char message[kTextColumns + 1];
  const FloppyDrive holding = FloppyDriveHolding(path);
  if (holding != kFloppyDrives) {
    snprintf(
        message, sizeof(message), "%s is already in %s", name,
        kVMDriveLabels[holding]);
    EmulatorStatusBarPostMessage(message);
    DISK_WARN(
        "insert_failed", Y_LOG_STRING("drive", kDriveNames[g_drive]),
        Y_LOG_STRING("path", path), Y_LOG_STRING("error", "in_other_drive"));
    return;
  }
  const DiskImageStatus status =
      FloppyInsertImage(g_session->fdc, g_drive, path);
  if (status == kDiskImageOpen) {
    snprintf(
        message, sizeof(message), "Inserted %s in %s", name,
        kVMDriveLabels[g_drive]);
    DISK_INFO(
        "inserted", Y_LOG_STRING("drive", kDriveNames[g_drive]),
        Y_LOG_STRING("path", path));
  } else {
    snprintf(
        message, sizeof(message), "Could not insert %s: %s", name,
        DiskImageDescribeStatus(status));
    DISK_WARN(
        "insert_failed", Y_LOG_STRING("drive", kDriveNames[g_drive]),
        Y_LOG_STRING("path", path),
        Y_LOG_STRING("error", DiskImageDescribeStatus(status)));
  }
  EmulatorStatusBarPostMessage(message);
  // The drive is empty after an insert that failed.
  SaveDrive();
}

// Carry out the item selected: open the browser, or eject the disk, which
// leaves Eject gone and the first item selected.
static ScreenResult OnSelect(void) {
  if (g_list_box.selected == kDriveInsert) {
    return Browse();
  }
  Eject();
  g_list_box.selected = kDriveInsert;
  g_list_box.dirty = true;
  return ScreenStays();
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxHandleKeyEvent(&g_list_box, event, ItemCount())) {
    case kListBoxActionSelect:
      return OnSelect();
    case kListBoxActionBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

// Back from the browser: an image selected goes in the drive, and closes the
// menu. Without one, the page shows again with the first item selected.
static ScreenResult OnResume(void) {
  if (g_found_path[0] != '\0') {
    Insert(g_found_path);
    return ScreenClose();
  }
  g_list_box.selected = kDriveInsert;
  g_list_box.dirty = true;
  return ScreenStays();
}

// What the drive holds, over its items.
static bool OnDraw(DisplayBox* box) {
  if (!ListBoxTestAndClearDirty(&g_list_box)) {
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
  const BoxItem items[kDriveItemCount] = {
      [kDriveInsert] = {empty ? "Choose disk image" : "Swap disk image", true},
      [kDriveEject] = {"Eject", false},
  };
  ListBoxDrawItems(
      box, kVMDriveNames[kVMDriveFD1 + g_drive], lines,
      sizeof(lines) / sizeof(lines[0]), items, ItemCount(), g_list_box.selected,
      false);
  return true;
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return kDriveNames[g_drive]; }

const Screen kFloppyDriveScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .on_resume = OnResume,
    .hint = Hint,
    .page_name = PageName,
};
