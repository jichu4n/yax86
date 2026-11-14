#include "file_browser.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "box.h"
#include "core/hdc.h"
#include "list_box.h"
#include "vm_config.h"

enum {
  // Where an entry's note starts on its row: a folder's <DIR>, and which
  // drive an image is in.
  kBrowseNoteColumn = 18,
};

uint8_t FileBrowserOpen(FileBrowser* browser, const char* folder) {
  // A floppy drive takes a floppy, drive C a hard disk, and USB either.
  static const uint32_t kImageSizes[] = {
      Y_FDC_FORMAT_360KB_IMAGE_SIZE, Y_HDC_GEOMETRY_10MB_IMAGE_SIZE};
  const uint32_t* sizes = kImageSizes;
  uint8_t num_sizes = 1;
  if (browser->images == kFileBrowserEither) {
    num_sizes = 2;
  } else if (browser->images == kFileBrowserHardDisks) {
    sizes = &kImageSizes[1];
  }
  FileListRead(&browser->list, folder, sizes, num_sizes);
  browser->top = 0;
  uint8_t selected = 0;
  // The image to select: the one in the drive, or selected for the new VM's.
  // The drives have no images yet at power on.
  const char* current = browser->current;
  if (current == NULL || current[0] == '\0') {
    return selected;
  }
  char path_of_entry[kFSMaxPath + 1];
  for (uint8_t i = 0; i < browser->list.num_entries; ++i) {
    if (browser->list.entries[i].kind == kFileListImage &&
        FileListEntryPath(
            &browser->list, &browser->list.entries[i], path_of_entry) &&
        FSIsSamePath(path_of_entry, current)) {
      selected = i;
      FileBrowserScrollTo(browser, selected);
    }
  }
  return selected;
}

bool FileBrowserIsAtRoot(const FileBrowser* browser) {
  return browser->list.path[0] == '\0';
}

uint8_t FileBrowserUp(FileBrowser* browser) {
  char left[kFSMaxPath + 1];
  char parent[kFSMaxPath + 1];
  FSCopyPath(browser->list.path, left);
  FSFolderOf(left, parent);
  uint8_t selected = FileBrowserOpen(browser, parent);
  char path_of_entry[kFSMaxPath + 1];
  for (uint8_t i = 0; i < browser->list.num_entries; ++i) {
    if (browser->list.entries[i].kind == kFileListFolder &&
        FileListEntryPath(
            &browser->list, &browser->list.entries[i], path_of_entry) &&
        FSIsSamePath(path_of_entry, left)) {
      selected = i;
      FileBrowserScrollTo(browser, selected);
    }
  }
  return selected;
}

uint8_t FileBrowserNumEntries(const FileBrowser* browser) {
  return browser->list.num_entries;
}

FileBrowserSelection FileBrowserSelect(
    FileBrowser* browser, uint8_t* selected, char path[kFSMaxPath + 1],
    uint32_t* size) {
  if (*selected >= browser->list.num_entries) {
    return kFileBrowserNothing;
  }
  const FileListEntry* entry = &browser->list.entries[*selected];
  if (!FileListEntryPath(&browser->list, entry, path)) {
    return kFileBrowserNothing;
  }
  if (entry->kind == kFileListParent) {
    *selected = FileBrowserUp(browser);
    return kFileBrowserOpenedFolder;
  }
  if (entry->kind == kFileListFolder) {
    *selected = FileBrowserOpen(browser, path);
    return kFileBrowserOpenedFolder;
  }
  *size = entry->size;
  return kFileBrowserSelectedImage;
}

void FileBrowserScrollTo(FileBrowser* browser, uint8_t selected) {
  ListBoxScrollTo(selected, &browser->top);
}

// What the browser says of a folder with nothing to select in it.
static const char* BrowseEmpty(const FileBrowser* browser) {
  if (browser->images == kFileBrowserEither) {
    return "No folders or disk images";
  }
  if (browser->images == kFileBrowserHardDisks) {
    return "No folders or hard disk images";
  }
  return "No folders or floppy images";
}

void FileBrowserDraw(
    const FileBrowser* browser, DisplayBox* box, uint8_t selected) {
  const FileList* list = &browser->list;
  BoxPlace(box, kListBoxNarrowColumns, kListBoxRows + 2);
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
  BoxEdgeRow(&box->text[0], box->columns, true, title);
  for (uint8_t i = 0; i < kListBoxRows; ++i) {
    DisplayTextRow* row = &box->text[1 + i];
    const uint8_t index = (uint8_t)(browser->top + i);
    if (index >= list->num_entries) {
      BoxBlankRow(row, box->columns);
      if (i == 0 && !list->is_readable) {
        BoxPutText(
            row, box->columns, 3, "The card cannot be read", kBoxAttrText);
      } else if (i == 1 && list->num_entries <= 1) {
        BoxPutText(row, box->columns, 3, BrowseEmpty(browser), kBoxAttrText);
      }
      continue;
    }
    const FileListEntry* entry = &list->entries[index];
    BoxItemTextRow(row, box->columns, entry->name, index == selected);
    const uint8_t attr = index == selected ? kBoxAttrSelected : kBoxAttrText;
    char path[kFSMaxPath + 1];
    if (entry->kind != kFileListImage) {
      BoxPutText(row, box->columns, kBrowseNoteColumn, "<DIR>", attr);
      continue;
    }
    if (browser->images == kFileBrowserEither) {
      BoxPutText(
          row, box->columns, kBrowseNoteColumn,
          entry->size == Y_FDC_FORMAT_360KB_IMAGE_SIZE ? "floppy" : "hard disk",
          attr);
      continue;
    }
    if (!FileListEntryPath(list, entry, path)) {
      continue;
    }
    for (uint8_t drive = 0; drive < kNumFloppyDrives; ++drive) {
      const char* in_drive = browser->in_drives[drive];
      if (in_drive != NULL && FSIsSamePath(path, in_drive)) {
        char note[16];
        snprintf(note, sizeof(note), "in %s", kVMDriveLabels[drive]);
        BoxPutText(row, box->columns, kBrowseNoteColumn, note, attr);
      }
    }
  }
  // A folder with more entries than a list holds says so on the bottom edge.
  BoxEdgeRow(
      &box->text[kListBoxRows + 1], box->columns, false,
      list->is_truncated ? "the first 64 entries" : NULL);
  ListBoxDrawScrollArrows(box, browser->top, list->num_entries);
}
