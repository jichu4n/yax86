#include "menu_browse.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "core/hdc.h"
#include "file_list.h"
#include "floppy.h"
#include "fs.h"
#include "ui_box.h"
#include "vm_config.h"
#include "vm_folder.h"

enum {
  // A floppy image's size: the browser lists only files this long.
  kFloppyImageSize = 360 * 1024,
  // The browser's rows of entries, which with its edges fill the most rows a
  // box can have.
  kBrowseRows = kDisplayMenuMaxRows - 2,
  // Where an entry's note starts on its row: a folder's <DIR>, and which
  // drive an image is in.
  kBrowseNoteColumn = 18,
};

// What the browser is finding an image for: a floppy for a drive, an image of
// either kind to put on USB, from the list, or an image for a new VM's drive.
typedef enum BrowsePurpose {
  kBrowseForDrive = 0,
  kBrowseForUSB,
  kBrowseForNewVM,
} BrowsePurpose;

static MenuBrowseConfig g_config;
// The folder the browser shows, and the entry at the top of its rows.
static FileList g_list;
static uint8_t g_list_top;
static BrowsePurpose g_browsing_for;
// The drive an image is being found for: a floppy drive, or one of a new
// VM's, with the image chosen for it so far, if any.
static FloppyDrive g_drive;
static uint8_t g_new_drive;
static const char* g_new_drive_image;

void MenuBrowseInit(const MenuBrowseConfig* config) {
  g_config = *config;
}

// What the browser says of a folder with nothing to choose in it.
static const char* BrowseEmpty(void) {
  if (g_browsing_for == kBrowseForUSB) {
    return "No folders or disk images";
  }
  if (g_browsing_for == kBrowseForNewVM && g_new_drive >= kVMDriveHD1) {
    return "No folders or hard disk images";
  }
  return "No folders or floppy images";
}

// Show the browser on a folder, with the image in the drive chosen if it is
// there.
static void Browse(const char* path) {
  // A floppy drive takes a floppy, drive C a hard disk, and USB either.
  static const uint32_t kImageSizes[] = {
      kFloppyImageSize, Y_HDC_GEOMETRY_10MB_IMAGE_SIZE};
  const uint32_t* sizes = kImageSizes;
  uint8_t num_sizes = 1;
  if (g_browsing_for == kBrowseForUSB) {
    num_sizes = 2;
  } else if (g_browsing_for == kBrowseForNewVM && g_new_drive >= kVMDriveHD1) {
    sizes = &kImageSizes[1];
  }
  FileListRead(&g_list, path, sizes, num_sizes);
  g_list_top = 0;
  uint8_t chosen_index = 0;
  // The image to choose: the one in the drive, or chosen for the new VM's.
  // The drives have no images yet at power on.
  const char* chosen = NULL;
  if (g_browsing_for == kBrowseForNewVM) {
    chosen = g_new_drive_image;
  } else if (g_browsing_for == kBrowseForDrive) {
    const DiskImage* image = g_config.drive_fn(g_drive);
    if (image->status == kDiskImageOpen) {
      chosen = image->path;
    }
  }
  if (chosen != NULL && chosen[0] != '\0') {
    char path_of_entry[kFSMaxPath + 1];
    for (uint8_t i = 0; i < g_list.count; ++i) {
      if (g_list.entries[i].kind == kFileListImage &&
          FileListEntryPath(&g_list, &g_list.entries[i], path_of_entry) &&
          FSSamePath(path_of_entry, chosen)) {
        chosen_index = i;
      }
    }
  }
  UIBoxScrollTo(chosen_index, &g_list_top, kBrowseRows);
  g_config.show_page_fn(chosen_index);
}

// Show the browser on the folder above, with the folder just left chosen.
static void BrowseUp(void) {
  char left[kFSMaxPath + 1];
  char parent[kFSMaxPath + 1];
  FSCopyPath(g_list.path, left);
  FSFolderOf(left, parent);
  Browse(parent);
  char path_of_entry[kFSMaxPath + 1];
  for (uint8_t i = 0; i < g_list.count; ++i) {
    if (g_list.entries[i].kind == kFileListFolder &&
        FileListEntryPath(&g_list, &g_list.entries[i], path_of_entry) &&
        FSSamePath(path_of_entry, left)) {
      UIBoxScrollTo(i, &g_list_top, kBrowseRows);
      g_config.set_chosen_fn(i);
    }
  }
}

void MenuBrowseForDrive(FloppyDrive drive) {
  g_browsing_for = kBrowseForDrive;
  g_drive = drive;
  // The folder the browser starts in: that of the image in the drive, or else
  // of the one in the other drive - the next disk of a set is likely beside
  // it - or else yax86.
  const DiskImage* image = g_config.drive_fn(drive);
  if (image->status != kDiskImageOpen) {
    image = g_config.drive_fn(
        drive == kFloppyDriveA ? kFloppyDriveB : kFloppyDriveA);
  }
  char folder[kFSMaxPath + 1];
  if (image->status == kDiskImageOpen) {
    FSFolderOf(image->path, folder);
  } else {
    FSCopyPath("yax86", folder);
  }
  Browse(folder);
}

void MenuBrowseForUSB(void) {
  g_browsing_for = kBrowseForUSB;
  Browse("yax86");
}

void MenuBrowseForNewVM(uint8_t new_drive, const char* image) {
  g_browsing_for = kBrowseForNewVM;
  g_new_drive = new_drive;
  g_new_drive_image = image;
  char folder[kFSMaxPath + 1];
  if (image[0] != '\0') {
    FSFolderOf(image, folder);
  } else {
    FSCopyPath("yax86", folder);
  }
  Browse(folder);
}

uint8_t MenuBrowseItemCount(void) {
  return g_list.count;
}

void MenuBrowseDraw(DisplayMenu* menu, uint8_t chosen) {
  UIBoxPlace(menu, kUIBoxNarrowColumns, kBrowseRows + 2);
  // The folder, from the card's root, keeping its end if it is too long.
  char title[kUIBoxNarrowColumns - 6 + 1];
  const uint8_t room = sizeof(title) - 1;
  char full[kFSMaxPath + 2];
  snprintf(full, sizeof(full), "/%s", g_list.path);
  for (char* c = full; *c != '\0'; ++c) {
    *c = (char)toupper((unsigned char)*c);
  }
  const size_t length = strlen(full);
  if (length > room) {
    snprintf(title, sizeof(title), "..%s", &full[length - room + 2]);
  } else {
    snprintf(title, sizeof(title), "%s", full);
  }
  UIBoxEdgeRow(&menu->text[0], menu->columns, true, title);
  for (uint8_t i = 0; i < kBrowseRows; ++i) {
    DisplayTextRow* row = &menu->text[1 + i];
    const uint8_t index = (uint8_t)(g_list_top + i);
    if (index >= g_list.count) {
      UIBoxBlankRow(row, menu->columns);
      if (i == 0 && !g_list.readable) {
        UIBoxPutText(
            row, menu->columns, 3, "The card cannot be read", kUIBoxAttrText);
      } else if (i == 1 && g_list.count <= 1) {
        UIBoxPutText(row, menu->columns, 3, BrowseEmpty(), kUIBoxAttrText);
      }
      continue;
    }
    const FileListEntry* entry = &g_list.entries[index];
    UIBoxItemRow(row, menu->columns, entry->name, index == chosen);
    const uint8_t attr = index == chosen ? kUIBoxAttrChosen : kUIBoxAttrText;
    char path[kFSMaxPath + 1];
    if (entry->kind != kFileListImage) {
      UIBoxPutText(row, menu->columns, kBrowseNoteColumn, "<DIR>", attr);
      continue;
    }
    if (g_browsing_for == kBrowseForUSB) {
      UIBoxPutText(
          row, menu->columns, kBrowseNoteColumn,
          entry->size == kFloppyImageSize ? "floppy" : "hard disk", attr);
      continue;
    }
    if (g_browsing_for == kBrowseForNewVM) {
      continue;
    }
    if (!FileListEntryPath(&g_list, entry, path)) {
      continue;
    }
    for (uint8_t drive = 0; drive < kFloppyDrives; ++drive) {
      const DiskImage* image = g_config.drive_fn((FloppyDrive)drive);
      if (image->status == kDiskImageOpen && FSSamePath(path, image->path)) {
        char note[16];
        snprintf(note, sizeof(note), "in %s", kVMDriveLabels[drive]);
        UIBoxPutText(row, menu->columns, kBrowseNoteColumn, note, attr);
      }
    }
  }
  // A folder with more entries than a list holds says so on the bottom edge.
  UIBoxEdgeRow(
      &menu->text[kBrowseRows + 1], menu->columns, false,
      g_list.truncated ? "the first 64 entries" : NULL);
  UIBoxDrawScrollArrows(menu, g_list_top, kBrowseRows, g_list.count);
}

MenuAction MenuBrowseChoose(uint8_t chosen) {
  if (chosen >= g_list.count) {
    return kMenuActionNone;
  }
  const FileListEntry* entry = &g_list.entries[chosen];
  char path[kFSMaxPath + 1];
  if (!FileListEntryPath(&g_list, entry, path)) {
    return kMenuActionNone;
  }
  if (entry->kind == kFileListParent) {
    BrowseUp();
    return kMenuActionNone;
  }
  if (entry->kind == kFileListFolder) {
    Browse(path);
    return kMenuActionNone;
  }
  if (g_browsing_for == kBrowseForNewVM) {
    g_config.chose_for_new_vm_fn(g_new_drive, path);
    return kMenuActionNone;
  }
  if (g_browsing_for == kBrowseForUSB) {
    g_config.chose_for_usb_fn(path, entry->size);
    return kMenuActionShareImage;
  }
  g_config.insert_drive_fn(g_drive, path);
  g_config.close_menu_fn();
  return kMenuActionClosed;
}

MenuAction MenuBrowseBack(void) {
  if (g_browsing_for == kBrowseForUSB) {
    g_config.back_to_usb_fn();
  } else if (g_browsing_for == kBrowseForNewVM) {
    g_config.back_to_new_vm_fn();
  } else {
    g_config.back_to_drive_fn(g_drive);
  }
  return kMenuActionNone;
}

bool MenuBrowseHandleBackspace(void) {
  if (g_list.path[0] != '\0') {
    BrowseUp();
    return true;
  }
  return false;
}

void MenuBrowseScrollTo(uint8_t chosen) {
  UIBoxScrollTo(chosen, &g_list_top, kBrowseRows);
}
