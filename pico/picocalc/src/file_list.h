// A folder on the SD card, read for the menu's browser: the way up, the
// folders in it, and the disk images in it, each sorted by name. Paths are
// as fs.h has them.
#ifndef YAX86_PICOCALC_FILE_LIST_H
#define YAX86_PICOCALC_FILE_LIST_H

#include <stdbool.h>
#include <stdint.h>

#include "fs.h"

enum {
  // The most entries a list holds. A folder with more lists the first ones
  // found and says it was cut short.
  kFileListMaxEntries = 64,
};

typedef enum FileListKind {
  // "..", the folder above.
  kFileListParent = 0,
  kFileListFolder,
  kFileListImage,
} FileListKind;

typedef struct FileListEntry {
  FileListKind kind;
  char name[kFSMaxName + 1];
  // An image's size in bytes, one of the sizes listed.
  uint32_t size;
} FileListEntry;

typedef struct FileList {
  // The folder listed.
  char path[kFSMaxPath + 1];
  FileListEntry entries[kFileListMaxEntries];
  uint8_t count;
  // Whether the folder had more than kFileListMaxEntries.
  bool truncated;
  // Whether the folder could be read at all.
  bool readable;
} FileList;

// Read the folder at path, listing the files exactly one of image_sizes long
// as images, and every folder that is not hidden or a system folder.
void FileListRead(
    FileList* list, const char* path, const uint32_t* image_sizes,
    uint8_t num_sizes);

// The path of an entry of the list: the folder above for kFileListParent.
// Returns false if it would not fit in kFSMaxPath.
bool FileListEntryPath(
    const FileList* list, const FileListEntry* entry,
    char path[kFSMaxPath + 1]);

#endif  // YAX86_PICOCALC_FILE_LIST_H
