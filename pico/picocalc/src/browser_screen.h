// The file browser as a screen: finding a disk image on the card, for
// whichever screen opened it. It goes into folders and up out of them. When
// an image is selected, it writes the image's path where its parent said,
// and goes back. What to do with the image is the parent's.
#ifndef YAX86_PICOCALC_BROWSER_SCREEN_H
#define YAX86_PICOCALC_BROWSER_SCREEN_H

#include <stdint.h>

#include "file_browser.h"
#include "fs.h"
#include "screen.h"

// Show the browser on folder, listing images, with the image at current
// selected if it is there. current is NULL or empty for none, and must stay
// as it is while the browser shows. path is emptied. When an image is
// selected, its path from the card's root is written to path and its length
// to *size, and the browser goes back. Both must outlive the screen.
ScreenResult BrowserScreenOpen(
    FileBrowserImages images, const char* folder, const char* current,
    char path[kFSMaxPath + 1], uint32_t* size);

// Show the browser again as it was left, with its folder and entry
// selected: for a parent that could not use the image selected, so that
// another can be selected.
ScreenResult BrowserScreenReopen(void);

#endif  // YAX86_PICOCALC_BROWSER_SCREEN_H
