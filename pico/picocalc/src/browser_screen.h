// The file browser as a screen: finding a disk image on the card, for
// whichever screen opened it. It goes into folders and up out of them, and
// writes the image chosen where its parent said, then goes back; what to do
// with the image is the parent's.
#ifndef YAX86_PICOCALC_BROWSER_SCREEN_H
#define YAX86_PICOCALC_BROWSER_SCREEN_H

#include <stdint.h>

#include "fs.h"
#include "screen.h"
#include "ui_browser.h"

// Show the browser on folder, listing images, with the image at current
// chosen if it is there; NULL or empty for none. current must stay as it is
// while the browser shows. path is emptied, and an image chosen is written
// to path, a path from the card's root, and its length to *size, before the
// browser goes back. Both must outlive the screen.
ScreenResult BrowserScreenOpen(
    UIBrowserImages images, const char* folder, const char* current,
    char path[kFSMaxPath + 1], uint32_t* size);

// Show the browser again as it was left, with its folder and entry chosen:
// for a parent that could not use the image chosen, to choose another.
ScreenResult BrowserScreenReopen(void);

#endif  // YAX86_PICOCALC_BROWSER_SCREEN_H
