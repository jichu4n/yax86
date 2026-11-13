// The file browser: the folders and disk images on the card, a page of the
// menu for finding an image. It shows a folder, goes into folders and up out
// of them, and says which image was chosen; what to do with it is its
// user's. Floppy drives, a new VM's drives and USB mass storage use it.
#ifndef YAX86_PICOCALC_UI_BROWSER_H
#define YAX86_PICOCALC_UI_BROWSER_H

#include <stdbool.h>
#include <stdint.h>

#include "display.h"
#include "file_list.h"
#include "floppy.h"
#include "fs.h"

// The images a browser lists: a floppy drive takes a floppy, a hard disk
// drive a hard disk, and USB either. Folders are listed whatever it is.
typedef enum UIBrowserImages {
  kUIBrowserFloppies = 0,
  kUIBrowserHardDisks,
  kUIBrowserEither,
} UIBrowserImages;

typedef struct UIBrowser {
  // The folder showing, and the entry at the top of its rows.
  FileList list;
  uint8_t top;
  UIBrowserImages images;
  // The image chosen when a folder that holds it opens: the one in the
  // drive, or chosen for it before. NULL or empty for none. The string is
  // its user's, and must stay as it is while the browser shows.
  const char* current;
  // The images in the floppy drives, which their rows say: in FD1, in FD2.
  // NULL for an empty drive, or when the browser is not finding a floppy for
  // a drive.
  const char* in_drives[kFloppyDrives];
} UIBrowser;

// What choosing an entry did.
typedef enum UIBrowserChoice {
  // Nothing: no entry was chosen.
  kUIBrowserNothing = 0,
  // A folder, or the folder above, opened, with *chosen the entry to choose.
  kUIBrowserOpened,
  // An image was chosen: path is where it is, and *size how long.
  kUIBrowserChoseImage,
} UIBrowserChoice;

// Show folder, with the browser's current image chosen if it is there.
// Returns the entry to choose.
uint8_t UIBrowserOpen(UIBrowser* browser, const char* folder);

// Whether the browser is at the card's root, with no folder above.
bool UIBrowserAtRoot(const UIBrowser* browser);

// Show the folder above, with the folder just left chosen. Returns the entry
// to choose.
uint8_t UIBrowserUp(UIBrowser* browser);

// How many entries the folder showing has.
uint8_t UIBrowserCount(const UIBrowser* browser);

// Open the entry at chosen: a folder to look in, or an image, which is
// returned in path and size.
UIBrowserChoice UIBrowserChoose(
    UIBrowser* browser, uint8_t* chosen, char path[kFSMaxPath + 1],
    uint32_t* size);

// Keep the entry at chosen among the browser's rows.
void UIBrowserScrollTo(UIBrowser* browser, uint8_t chosen);

// The browser: the folder as the title, and a row for each entry that
// shows, with arrows on the border where there is more above or below.
void UIBrowserDraw(const UIBrowser* browser, DisplayMenu* menu, uint8_t chosen);

#endif  // YAX86_PICOCALC_UI_BROWSER_H
