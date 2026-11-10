// The floppy in drive A: an image on the SD card if there is one, or else the
// boot floppy built into flash.
//
// At power on the image on the card is `yax86/floppy_a.img`, a 360KB floppy,
// and what the guest writes to it is written to the card. The built-in floppy
// cannot be written, and what the guest writes to it is dropped. The menu can
// eject either, and insert any 360KB image on the card in its place.
#ifndef YAX86_PICOCALC_FLOPPY_H
#define YAX86_PICOCALC_FLOPPY_H

#include "core/fdc.h"
#include "disk_image.h"

enum {
  // The longest path of an image that can be inserted.
  kFloppyMaxPath = 127,
};

// Look for the image on the SD card. Call once, before FloppyInsert().
void FloppyOpen(void);

// Hand the floppy to the floppy controller, in drive A.
void FloppyInsert(YFDCState* fdc);

// The image on the card: whether it opened, and what it has cost. If it did
// not open at power on, drive A holds the built-in floppy; once ejected, it is
// kDiskImageEjected and the drive is empty.
const DiskImage* FloppyGetImage(void);

// Take the disk out of drive A, writing back what the guest was part way
// through writing.
void FloppyEject(YFDCState* fdc);

// Put the image at path on the card in drive A, in place of what was there.
// Returns kDiskImageOpen if it went in, or why not, in which case the drive
// is left empty. A 360KB image is the only size that fits.
DiskImageStatus FloppyInsertImage(YFDCState* fdc, const char* path);

#endif  // YAX86_PICOCALC_FLOPPY_H
