#include "file_list.h"

#include "disk_image.h"
#include "ff.h"

static bool NamesInOrder(const char* a, const char* b) {
  while (*a != '\0' && *a == *b) {
    ++a;
    ++b;
  }
  return (unsigned char)*a <= (unsigned char)*b;
}

// Whether a belongs before b: the way up first, then folders, then images,
// each by name.
static bool EntriesInOrder(const FileListEntry* a, const FileListEntry* b) {
  if (a->kind != b->kind) {
    return a->kind < b->kind;
  }
  return NamesInOrder(a->name, b->name);
}

static void CopyName(const char* from, char to[kFileListMaxName + 1]) {
  uint8_t i = 0;
  while (from[i] != '\0' && i < kFileListMaxName) {
    to[i] = from[i];
    ++i;
  }
  to[i] = '\0';
}

// Add an entry where it belongs, keeping the list sorted. An insertion sort:
// a folder's worth of entries is few enough that nothing cleverer pays.
static void Insert(FileList* list, const FileListEntry* entry) {
  if (list->count == kFileListMaxEntries) {
    list->truncated = true;
    return;
  }
  uint8_t i = list->count;
  while (i > 0 && !EntriesInOrder(&list->entries[i - 1], entry)) {
    list->entries[i] = list->entries[i - 1];
    --i;
  }
  list->entries[i] = *entry;
  ++list->count;
}

void FileListCopyPath(const char* from, char to[kFileListMaxPath + 1]) {
  uint16_t i = 0;
  while (from[i] != '\0' && i < kFileListMaxPath) {
    to[i] = from[i];
    ++i;
  }
  to[i] = '\0';
}

void FileListRead(FileList* list, const char* path, uint32_t image_size) {
  FileListCopyPath(path, list->path);
  list->count = 0;
  list->truncated = false;
  list->readable = false;
  if (list->path[0] != '\0') {
    const FileListEntry parent = {.kind = kFileListParent, .name = ".."};
    Insert(list, &parent);
  }
  if (DiskImageMountCard() != kDiskImageOpen) {
    return;
  }
  DIR folder;
  if (f_opendir(&folder, list->path) != FR_OK) {
    return;
  }
  list->readable = true;
  FILINFO info;
  while (f_readdir(&folder, &info) == FR_OK && info.fname[0] != '\0') {
    // The dot entries too, should FatFs ever hand them over: the list has
    // its own way up.
    if ((info.fattrib & (AM_HID | AM_SYS)) || info.fname[0] == '.') {
      continue;
    }
    FileListEntry entry;
    if (info.fattrib & AM_DIR) {
      entry.kind = kFileListFolder;
    } else if (info.fsize == image_size) {
      entry.kind = kFileListImage;
    } else {
      continue;
    }
    CopyName(info.fname, entry.name);
    Insert(list, &entry);
  }
  f_closedir(&folder);
}

static char Upper(char c) {
  return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

bool FileListSamePath(const char* a, const char* b) {
  while (*a != '\0' && Upper(*a) == Upper(*b)) {
    ++a;
    ++b;
  }
  return *a == '\0' && *b == '\0';
}

void FileListFolderOf(const char* path, char folder[kFileListMaxPath + 1]) {
  FileListCopyPath(path, folder);
  uint16_t last_slash = 0;
  bool has_slash = false;
  for (uint16_t i = 0; folder[i] != '\0'; ++i) {
    if (folder[i] == '/') {
      last_slash = i;
      has_slash = true;
    }
  }
  folder[has_slash ? last_slash : 0] = '\0';
}

bool FileListEntryPath(
    const FileList* list, const FileListEntry* entry,
    char path[kFileListMaxPath + 1]) {
  if (entry->kind == kFileListParent) {
    FileListFolderOf(list->path, path);
    return true;
  }
  uint16_t length = 0;
  for (const char* c = list->path; *c != '\0'; ++c) {
    path[length++] = *c;
  }
  if (length > 0) {
    if (length == kFileListMaxPath) {
      return false;
    }
    path[length++] = '/';
  }
  for (const char* c = entry->name; *c != '\0'; ++c) {
    if (length == kFileListMaxPath) {
      return false;
    }
    path[length++] = *c;
  }
  path[length] = '\0';
  return true;
}
