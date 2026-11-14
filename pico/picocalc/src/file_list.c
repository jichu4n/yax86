#include "file_list.h"

#include <stdio.h>
#include <string.h>

#include "disk_image.h"

// Whether a belongs before b: the way up first, then folders, then images,
// each by name.
static bool AreEntriesInOrder(const FileListEntry* a, const FileListEntry* b) {
  if (a->kind != b->kind) {
    return a->kind < b->kind;
  }
  return strcmp(a->name, b->name) <= 0;
}

// Add an entry where it belongs, keeping the list sorted. An insertion sort:
// a folder's worth of entries is few enough that nothing cleverer pays.
static void Insert(FileList* list, const FileListEntry* entry) {
  if (list->num_entries == kFileListMaxEntries) {
    list->is_truncated = true;
    return;
  }
  uint8_t i = list->num_entries;
  while (i > 0 && !AreEntriesInOrder(&list->entries[i - 1], entry)) {
    list->entries[i] = list->entries[i - 1];
    --i;
  }
  list->entries[i] = *entry;
  ++list->num_entries;
}

// A listing under way: the list, and the sizes an image may have.
typedef struct Reading {
  FileList* list;
  const uint32_t* image_sizes;
  uint8_t num_sizes;
} Reading;

static bool TakeEntry(const FILINFO* info, void* context) {
  Reading* reading = context;
  if (info->fattrib & (AM_HID | AM_SYS)) {
    return true;
  }
  FileListEntry entry = {.kind = kFileListFolder, .size = 0};
  if (!(info->fattrib & AM_DIR)) {
    uint8_t i = 0;
    while (i < reading->num_sizes && info->fsize != reading->image_sizes[i]) {
      ++i;
    }
    if (i == reading->num_sizes) {
      return true;
    }
    entry.kind = kFileListImage;
    entry.size = reading->image_sizes[i];
  }
  snprintf(entry.name, sizeof(entry.name), "%s", info->fname);
  Insert(reading->list, &entry);
  return true;
}

void FileListRead(
    FileList* list, const char* path, const uint32_t* image_sizes,
    uint8_t num_sizes) {
  FSCopyPath(path, list->path);
  list->num_entries = 0;
  list->is_truncated = false;
  list->is_readable = false;
  if (list->path[0] != '\0') {
    const FileListEntry parent = {
        .kind = kFileListParent, .name = "..", .size = 0};
    Insert(list, &parent);
  }
  if (DiskImageMountCard() != kDiskImageOpen) {
    return;
  }
  Reading reading = {
      .list = list, .image_sizes = image_sizes, .num_sizes = num_sizes};
  list->is_readable = FSListFolder(list->path, TakeEntry, &reading) == FR_OK;
}

bool FileListEntryPath(
    const FileList* list, const FileListEntry* entry,
    char path[kFSMaxPath + 1]) {
  if (entry->kind == kFileListParent) {
    FSFolderOf(list->path, path);
    return true;
  }
  return FSJoin(list->path, entry->name, path);
}
