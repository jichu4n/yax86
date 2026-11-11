// The SD card's file system, as the runtime uses it beyond reading and
// writing a file: paths, the names it gives what it makes, listing a folder,
// and whole folders - their size, a copy, and deleting them.
//
// Paths are FatFs's, without a leading slash: "" is the card's root, and
// "yax86/vm/DOS330" a folder two down. The card keeps 8.3 names - long names
// are off in `ffconf.h` - so a name is at most 12 characters, and FAT
// ignores case, so two paths differing only in it name the same file.
#ifndef YAX86_PICOCALC_FS_H
#define YAX86_PICOCALC_FS_H

#include <stdbool.h>
#include <stdint.h>

#include "ff.h"

enum {
  kFSMaxName = 12,
  kFSMaxPath = 127,
  // The longest name the runtime gives a folder it makes: 8.3's name, with
  // no extension.
  kFSMaxFolderName = 8,
};

// Copy a path, cut to kFSMaxPath.
void FSCopyPath(const char* from, char to[kFSMaxPath + 1]);

// Whether two paths name the same file.
bool FSSamePath(const char* a, const char* b);

// Whether path is a file or folder inside folder, at any depth.
bool FSInFolder(const char* path, const char* folder);

// The folder a path is in: "yax86/vm/dos330" for "yax86/vm/dos330/hdd.img",
// "" for a file at the root.
void FSFolderOf(const char* path, char folder[kFSMaxPath + 1]);

// A name in a folder, "" being the root. Returns false if it would not fit.
bool FSJoin(const char* folder, const char* name, char path[kFSMaxPath + 1]);

// Write path, from the card's root, as a file in folder would name it - a
// configuration file, say: relative to folder, climbing out with "..", when the
// two share a folder from the root, and from the root with a leading / when
// they share none - a path under `yax86` stays relative when the file moves
// within it. In lower case, FAT names being the same in either. Returns false
// if it would not fit.
bool FSRelativePath(
    const char* folder, const char* path, char out[kFSMaxPath + 2]);

// Resolve path against folder, both without a leading slash, into resolved:
// a path with a leading / starts at the card's root instead, and "." and
// ".." are followed. Returns false if the result would not fit, or climbs
// above the root.
bool FSResolvePath(
    const char* folder, const char* path, char resolved[kFSMaxPath + 1]);

// Whether path is a file or a folder on the card.
bool FSExists(const char* path);

// Whether c can be in the name of a folder the runtime makes: a letter, a
// digit, - or _. Other characters FAT allows are left out, so that a name
// typed on the keyboard is one a computer takes as well.
bool FSIsFolderNameCharacter(char c);

// Whether name is one DOS gives a device - CON, LPT1 and the rest - which DOS
// cannot open a file or folder as.
bool FSIsDeviceName(const char* name);

// Why the card would not do something, as a problem says it: "the card is
// full".
const char* FSDescribeResult(FRESULT result);

// What a listing hands each entry of a folder: the entry, and the caller's
// context. Returns false to stop the listing.
typedef bool (*FSVisit)(const FILINFO* info, void* context);

// Hand every entry of the folder at path to visit, in the order the card
// keeps them - "." and ".." left out, should FatFs ever list them. Returns
// how reading the folder went.
FRESULT FSListFolder(const char* path, FSVisit visit, void* context);

// How a copy is going, in bytes, called after each piece is written.
typedef void (*FSProgress)(uint64_t done, uint64_t total);

// A copy or a write of whole files under way: the buffer they go through,
// whose length is a whole number of the card's blocks, so that each piece
// goes to and from the card a run of blocks at a time; and how far it has
// come of how much.
typedef struct FSTransfer {
  uint8_t* buffer;
  uint32_t buffer_size;
  FSProgress progress;
  uint64_t done;
  uint64_t total;
} FSTransfer;

// The bytes of every file in the folder at path, at any depth.
uint64_t FSFolderSize(const char* path);

// Make the folder to, and copy everything in the folder from into it, at any
// depth. On a failure what was made of to is deleted.
FRESULT FSCopyFolder(const char* from, const char* to, FSTransfer* transfer);

// Write a new file at path of size zeros, through transfer's buffer, which it
// zeroes first, calling its progress as it goes.
FRESULT FSWriteZeros(const char* path, uint32_t size, FSTransfer* transfer);

// Delete the folder at path and everything in it, at any depth. last, if not
// NULL, is a file directly in it deleted after everything else, so that a
// delete cut short still leaves it.
FRESULT FSDeleteFolder(const char* path, const char* last);

#endif  // YAX86_PICOCALC_FS_H
