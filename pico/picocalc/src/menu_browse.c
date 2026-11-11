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
  kFloppyImageSize = 360 * 1024,
  kBrowseRows = kMenuBrowseRows,
};

typedef enum BrowsePurpose {
  kBrowseForDrive = 0,
  kBrowseForUSB,
  kBrowseForNewVM,
} BrowsePurpose;

static MenuBrowseConfig g_config;
static FileList g_list;
static uint8_t g_list_top;
static BrowsePurpose g_browsing_for;
static FloppyDrive g_drive;
static uint8_t g_new_drive;

void MenuBrowseInit(const MenuBrowseConfig* config) {
  g_config = *config;
}

static const char* BrowseEmpty(void) {
  if (g_browsing_for == kBrowseForUSB) {
    return "No folders or disk images";
  }
  if (g_browsing_for == kBrowseForNewVM && g_new_drive >= kVMDriveHD1) {
    return "No folders or hard disk images";
  }
  return "No folders or floppy images";
}

static void Browse(const char* path, const char* chosen_target) {
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
  uint8_t chosen = 0;
  if (chosen_target != NULL && chosen_target[0] != '\0') {
    char path_of_entry[kFSMaxPath + 1];
    for (uint8_t i = 0; i < g_list.count; ++i) {
      if (g_list.entries[i].kind == kFileListImage &&
          FileListEntryPath(&g_list, &g_list.entries[i], path_of_entry) &&
          FSSamePath(path_of_entry, chosen_target)) {
        chosen = i;
        break;
      }
    }
  }
  UIBoxScrollTo(chosen, &g_list_top, kBrowseRows);
  g_config.show_browse_page(chosen);
}

static void BrowseUp(void) {
  char left[kFSMaxPath + 1];
  char parent[kFSMaxPath + 1];
  FSCopyPath(g_list.path, left);
  FSFolderOf(left, parent);
  Browse(parent, NULL);
  char path_of_entry[kFSMaxPath + 1];
  for (uint8_t i = 0; i < g_list.count; ++i) {
    if (g_list.entries[i].kind == kFileListFolder &&
        FileListEntryPath(&g_list, &g_list.entries[i], path_of_entry) &&
        FSSamePath(path_of_entry, left)) {
      UIBoxScrollTo(i, &g_list_top, kBrowseRows);
      g_config.set_chosen(i);
      break;
    }
  }
}

void MenuBrowseForDrive(FloppyDrive drive) {
  g_browsing_for = kBrowseForDrive;
  g_drive = drive;
  const DiskImage* image = g_config.drive(drive);
  if (image->status != kDiskImageOpen) {
    image = g_config.drive(
        drive == kFloppyDriveA ? kFloppyDriveB : kFloppyDriveA);
  }
  char folder[kFSMaxPath + 1];
  const char* chosen = NULL;
  if (image->status == kDiskImageOpen) {
    FSFolderOf(image->path, folder);
    chosen = image->path;
  } else {
    FSCopyPath("yax86", folder);
  }
  Browse(folder, chosen);
}

void MenuBrowseForUSB(void) {
  g_browsing_for = kBrowseForUSB;
  Browse("yax86", NULL);
}

void MenuBrowseForNewVM(uint8_t new_drive, const char* current_path) {
  g_browsing_for = kBrowseForNewVM;
  g_new_drive = new_drive;
  char folder[kFSMaxPath + 1];
  const char* chosen = NULL;
  if (current_path != NULL && current_path[0] != '\0') {
    FSFolderOf(current_path, folder);
    chosen = current_path;
  } else {
    FSCopyPath("yax86", folder);
  }
  Browse(folder, chosen);
}

uint8_t MenuBrowseItemCount(void) {
  return g_list.count;
}

void MenuBrowseDraw(DisplayMenu* menu, uint8_t chosen) {
  UIBoxPlace(menu, kUIBoxNarrowColumns, kBrowseRows + 2);
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
      UIBoxPutText(row, menu->columns, kMenuBrowseNoteColumn, "<DIR>", attr);
      continue;
    }
    if (g_browsing_for == kBrowseForUSB) {
      UIBoxPutText(
          row, menu->columns, kMenuBrowseNoteColumn,
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
      const DiskImage* image = g_config.drive((FloppyDrive)drive);
      if (image->status == kDiskImageOpen && FSSamePath(path, image->path)) {
        char note[16];
        snprintf(note, sizeof(note), "in %s", kVMDriveLabels[drive]);
        UIBoxPutText(row, menu->columns, kMenuBrowseNoteColumn, note, attr);
      }
    }
  }
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
    Browse(path, NULL);
    return kMenuActionNone;
  }
  if (g_browsing_for == kBrowseForNewVM) {
    g_config.on_selected_for_new_vm(g_new_drive, path);
    return kMenuActionNone;
  }
  if (g_browsing_for == kBrowseForUSB) {
    g_config.on_selected_for_usb(path, entry->size);
    return kMenuActionShareImage;
  }
  g_config.insert_drive(g_drive, path);
  g_config.close_menu();
  return kMenuActionClosed;
}

MenuAction MenuBrowseBack(void) {
  if (g_browsing_for == kBrowseForUSB) {
    g_config.on_back_for_usb();
  } else if (g_browsing_for == kBrowseForNewVM) {
    g_config.on_back_for_new_vm();
  } else {
    g_config.on_back_for_drive(g_drive);
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
