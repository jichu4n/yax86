// A folder on the SD card, read for the menu's browser: the way up, the
// folders in it, and the disk images in it, each sorted by name.
//
// Paths are FatFs's, without a leading slash: "" is the card's root, and
// "yax86/MSDOS330" a folder two down. The card keeps 8.3 names, so a name is
// at most 12 characters.
#ifndef YAX86_PICOCALC_FILE_LIST_H
#define YAX86_PICOCALC_FILE_LIST_H

#include <stdbool.h>
#include <stdint.h>

enum {
  // The most entries a list holds. A folder with more lists the first ones
  // found and says it was cut short.
  kFileListMaxEntries = 64,
  kFileListMaxName = 12,
  kFileListMaxPath = 127,
};

typedef enum FileListKind {
  // "..", the folder above.
  kFileListParent = 0,
  kFileListFolder,
  kFileListImage,
} FileListKind;

typedef struct FileListEntry {
  FileListKind kind;
  char name[kFileListMaxName + 1];
} FileListEntry;

typedef struct FileList {
  // The folder listed.
  char path[kFileListMaxPath + 1];
  FileListEntry entries[kFileListMaxEntries];
  uint8_t count;
  // Whether the folder had more than kFileListMaxEntries.
  bool truncated;
  // Whether the folder could be read at all.
  bool readable;
} FileList;

// Read the folder at path, listing the files exactly image_size bytes long as
// images, and every folder that is not hidden or a system folder.
void FileListRead(FileList* list, const char* path, uint32_t image_size);

// The path of an entry of the list: the folder above for kFileListParent.
// Returns false if it would not fit in kFileListMaxPath.
bool FileListEntryPath(
    const FileList* list, const FileListEntry* entry,
    char path[kFileListMaxPath + 1]);

// Copy a path, cut to kFileListMaxPath.
void FileListCopyPath(const char* from, char to[kFileListMaxPath + 1]);

// Whether two paths name the same file. The card's file system ignores case.
bool FileListSamePath(const char* a, const char* b);

// The folder a path is in: "yax86" for "yax86/floppy_a.img", "" for a file at
// the root.
void FileListFolderOf(const char* path, char folder[kFileListMaxPath + 1]);

#endif  // YAX86_PICOCALC_FILE_LIST_H
