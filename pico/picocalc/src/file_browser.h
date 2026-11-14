// The file browser: the folders and disk images on the card, a page of the
// menu for finding an image. It shows a folder, goes into folders and up out
// of them, and says which image was selected; what to do with it is its
// user's. Floppy drives, a new VM's drives and USB mass storage use it.
#ifndef YAX86_PICOCALC_FILE_BROWSER_H
#define YAX86_PICOCALC_FILE_BROWSER_H

#include <stdbool.h>
#include <stdint.h>

#include "display.h"
#include "file_list.h"
#include "floppy.h"
#include "fs.h"

// The images a browser lists: a floppy drive takes a floppy, a hard disk
// drive a hard disk, and USB either. Folders are listed whatever it is.
typedef enum FileBrowserImages {
  kFileBrowserFloppies = 0,
  kFileBrowserHardDisks,
  kFileBrowserEither,
} FileBrowserImages;

typedef struct FileBrowser {
  // The folder showing, and the entry at the top of its rows.
  FileList list;
  uint8_t top;
  FileBrowserImages images;
  // The image selected when a folder that holds it opens: the one in the
  // drive, or selected for it before. NULL or empty for none. The string is
  // its user's, and must stay as it is while the browser shows.
  const char* current;
  // The images in the floppy drives, which their rows say: in FD1, in FD2.
  // NULL for an empty drive, or when the browser is not finding a floppy for
  // a drive.
  const char* in_drives[kNumFloppyDrives];
} FileBrowser;

// What selecting an entry did.
typedef enum FileBrowserSelection {
  // Nothing: no entry was selected.
  kFileBrowserNothing = 0,
  // A folder, or the folder above, opened, and *selected is now the entry
  // to select in it.
  kFileBrowserOpenedFolder,
  // An image was selected: path is where it is, and *size how long.
  kFileBrowserSelectedImage,
} FileBrowserSelection;

// Show folder, with the browser's current image selected if it is there.
// Returns the entry selected.
uint8_t FileBrowserOpen(FileBrowser* browser, const char* folder);

// Whether the browser is at the card's root, with no folder above.
bool FileBrowserIsAtRoot(const FileBrowser* browser);

// Show the folder above, with the folder just left selected. Returns that
// entry.
uint8_t FileBrowserUp(FileBrowser* browser);

// How many entries the folder showing has.
uint8_t FileBrowserNumEntries(const FileBrowser* browser);

// Open the entry at selected: a folder to look in, or an image, which is
// returned in path and size.
FileBrowserSelection FileBrowserSelect(
    FileBrowser* browser, uint8_t* selected, char path[kFSMaxPath + 1],
    uint32_t* size);

// Keep the entry at selected among the browser's rows.
void FileBrowserScrollTo(FileBrowser* browser, uint8_t selected);

// The browser: the folder as the title, and a row for each entry that
// shows, with arrows on the border where there is more above or below.
void FileBrowserDraw(
    const FileBrowser* browser, DisplayBox* box, uint8_t selected);

#endif  // YAX86_PICOCALC_FILE_BROWSER_H
