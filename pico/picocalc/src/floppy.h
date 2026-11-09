// The floppy in drive A: an image on the SD card if there is one, or else the
// boot floppy built into flash.
//
// The image on the card is `yax86/floppy_a.img`, a 360KB floppy, and what the
// guest writes to it is written to the card. The built-in floppy cannot be
// written, and what the guest writes to it is dropped.
#ifndef YAX86_PICOCALC_FLOPPY_H
#define YAX86_PICOCALC_FLOPPY_H

#include "core/fdc.h"
#include "disk_image.h"

// Look for the image on the SD card. Call once, before FloppyInsert().
void FloppyOpen(void);

// Hand the floppy to the floppy controller, in drive A.
void FloppyInsert(YFDCState* fdc);

// The image on the card: whether it opened, and what it has cost. If it did
// not open, drive A holds the built-in floppy.
const DiskImage* FloppyGetImage(void);

#endif  // YAX86_PICOCALC_FLOPPY_H
