#include "ui_browser.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "core/hdc.h"
#include "list_box.h"
#include "ui_box.h"
#include "vm_config.h"

enum {
  // Where an entry's note starts on its row: a folder's <DIR>, and which
  // drive an image is in.
  kBrowseNoteColumn = 18,
};

uint8_t UIBrowserOpen(UIBrowser* browser, const char* folder) {
  // A floppy drive takes a floppy, drive C a hard disk, and USB either.
  static const uint32_t kImageSizes[] = {
      Y_FDC_FORMAT_360KB_IMAGE_SIZE, Y_HDC_GEOMETRY_10MB_IMAGE_SIZE};
  const uint32_t* sizes = kImageSizes;
  uint8_t num_sizes = 1;
  if (browser->images == kUIBrowserEither) {
    num_sizes = 2;
  } else if (browser->images == kUIBrowserHardDisks) {
    sizes = &kImageSizes[1];
  }
  FileListRead(&browser->list, folder, sizes, num_sizes);
  browser->top = 0;
  uint8_t chosen = 0;
  // The image to choose: the one in the drive, or chosen for the new VM's.
  // The drives have no images yet at power on.
  const char* current = browser->current;
  if (current == NULL || current[0] == '\0') {
    return chosen;
  }
  char path_of_entry[kFSMaxPath + 1];
  for (uint8_t i = 0; i < browser->list.count; ++i) {
    if (browser->list.entries[i].kind == kFileListImage &&
        FileListEntryPath(
            &browser->list, &browser->list.entries[i], path_of_entry) &&
        FSSamePath(path_of_entry, current)) {
      chosen = i;
      UIBrowserScrollTo(browser, chosen);
    }
  }
  return chosen;
}

bool UIBrowserAtRoot(const UIBrowser* browser) {
  return browser->list.path[0] == '\0';
}

uint8_t UIBrowserUp(UIBrowser* browser) {
  char left[kFSMaxPath + 1];
  char parent[kFSMaxPath + 1];
  FSCopyPath(browser->list.path, left);
  FSFolderOf(left, parent);
  uint8_t chosen = UIBrowserOpen(browser, parent);
  char path_of_entry[kFSMaxPath + 1];
  for (uint8_t i = 0; i < browser->list.count; ++i) {
    if (browser->list.entries[i].kind == kFileListFolder &&
        FileListEntryPath(
            &browser->list, &browser->list.entries[i], path_of_entry) &&
        FSSamePath(path_of_entry, left)) {
      chosen = i;
      UIBrowserScrollTo(browser, chosen);
    }
  }
  return chosen;
}

uint8_t UIBrowserCount(const UIBrowser* browser) { return browser->list.count; }

UIBrowserChoice UIBrowserChoose(
    UIBrowser* browser, uint8_t* chosen, char path[kFSMaxPath + 1],
    uint32_t* size) {
  if (*chosen >= browser->list.count) {
    return kUIBrowserNothing;
  }
  const FileListEntry* entry = &browser->list.entries[*chosen];
  if (!FileListEntryPath(&browser->list, entry, path)) {
    return kUIBrowserNothing;
  }
  if (entry->kind == kFileListParent) {
    *chosen = UIBrowserUp(browser);
    return kUIBrowserOpened;
  }
  if (entry->kind == kFileListFolder) {
    *chosen = UIBrowserOpen(browser, path);
    return kUIBrowserOpened;
  }
  *size = entry->size;
  return kUIBrowserChoseImage;
}

void UIBrowserScrollTo(UIBrowser* browser, uint8_t chosen) {
  ListBoxScrollTo(chosen, &browser->top);
}

// What the browser says of a folder with nothing to choose in it.
static const char* BrowseEmpty(const UIBrowser* browser) {
  if (browser->images == kUIBrowserEither) {
    return "No folders or disk images";
  }
  if (browser->images == kUIBrowserHardDisks) {
    return "No folders or hard disk images";
  }
  return "No folders or floppy images";
}

void UIBrowserDraw(
    const UIBrowser* browser, DisplayMenu* menu, uint8_t chosen) {
  const FileList* list = &browser->list;
  UIBoxPlace(menu, kListBoxNarrowColumns, kListBoxRows + 2);
  // The folder, from the card's root, keeping its end if it is too long.
  char title[kListBoxNarrowColumns - 6 + 1];
  const uint8_t room = sizeof(title) - 1;
  char full[kFSMaxPath + 2];
  snprintf(full, sizeof(full), "/%s", list->path);
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
  for (uint8_t i = 0; i < kListBoxRows; ++i) {
    DisplayTextRow* row = &menu->text[1 + i];
    const uint8_t index = (uint8_t)(browser->top + i);
    if (index >= list->count) {
      UIBoxBlankRow(row, menu->columns);
      if (i == 0 && !list->readable) {
        UIBoxPutText(
            row, menu->columns, 3, "The card cannot be read", kUIBoxAttrText);
      } else if (i == 1 && list->count <= 1) {
        UIBoxPutText(
            row, menu->columns, 3, BrowseEmpty(browser), kUIBoxAttrText);
      }
      continue;
    }
    const FileListEntry* entry = &list->entries[index];
    UIBoxItemRow(row, menu->columns, entry->name, index == chosen);
    const uint8_t attr = index == chosen ? kUIBoxAttrChosen : kUIBoxAttrText;
    char path[kFSMaxPath + 1];
    if (entry->kind != kFileListImage) {
      UIBoxPutText(row, menu->columns, kBrowseNoteColumn, "<DIR>", attr);
      continue;
    }
    if (browser->images == kUIBrowserEither) {
      UIBoxPutText(
          row, menu->columns, kBrowseNoteColumn,
          entry->size == Y_FDC_FORMAT_360KB_IMAGE_SIZE ? "floppy" : "hard disk",
          attr);
      continue;
    }
    if (!FileListEntryPath(list, entry, path)) {
      continue;
    }
    for (uint8_t drive = 0; drive < kFloppyDrives; ++drive) {
      const char* in_drive = browser->in_drives[drive];
      if (in_drive != NULL && FSSamePath(path, in_drive)) {
        char note[16];
        snprintf(note, sizeof(note), "in %s", kVMDriveLabels[drive]);
        UIBoxPutText(row, menu->columns, kBrowseNoteColumn, note, attr);
      }
    }
  }
  // A folder with more entries than a list holds says so on the bottom edge.
  UIBoxEdgeRow(
      &menu->text[kListBoxRows + 1], menu->columns, false,
      list->truncated ? "the first 64 entries" : NULL);
  ListBoxDrawScrollArrows(menu, browser->top, list->count);
}
