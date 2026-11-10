// The floppies in drives A and B.
//
// At power on each drive holds the image its machine's configuration names,
// a 360KB floppy on the SD card, or nothing. Without a configuration drive A
// holds `yax86/floppy_a.img`, or else the boot floppy built into flash, and
// drive B is empty. What
// the guest writes to an image on the card is written to the card. The
// built-in floppy cannot be written, and what the guest writes to it is
// dropped. The menu can eject either drive's disk, and insert any 360KB image
// on the card in its place.
#ifndef YAX86_PICOCALC_FLOPPY_H
#define YAX86_PICOCALC_FLOPPY_H

#include "core/fdc.h"
#include "disk_image.h"

typedef enum FloppyDrive {
  kFloppyDriveA = 0,
  kFloppyDriveB,
  kFloppyDrives,
} FloppyDrive;

// The image drive A looks for without a machine configuration.
extern const char kFloppyDefaultPath[];

// Open each drive's image on the SD card: a path from the card's root, or
// NULL or empty for none. A drive whose image does not open is left empty,
// but if built_in_fallback, drive A holds the built-in floppy instead, its
// image's status saying why. Fills opened with how each drive's open went.
// Call once, before FloppyAttach().
void FloppyOpen(
    const char* const paths[kFloppyDrives], bool built_in_fallback,
    DiskImageStatus opened[kFloppyDrives]);

// Hand the drives to the floppy controller, with their disks in.
void FloppyAttach(YFDCState* fdc);

// A drive's image on the card: whether it is open, and what it has cost.
// Drive A holds the built-in floppy if its image did not open at power on; a
// drive whose disk was ejected, and drive B until a disk goes in, is
// kDiskImageEjected.
const DiskImage* FloppyGetImage(FloppyDrive drive);

// The drive whose image is the one at path, or kFloppyDrives if none is.
FloppyDrive FloppyDriveHolding(const char* path);

// Take the disk out of a drive, writing back what the guest was part way
// through writing.
void FloppyEject(YFDCState* fdc, FloppyDrive drive);

// Put the image at path on the card in a drive, in place of what was there.
// Returns kDiskImageOpen if it went in, or why not, in which case the drive
// is left empty. A 360KB image is the only size that fits.
DiskImageStatus FloppyInsertImage(
    YFDCState* fdc, FloppyDrive drive, const char* path);

#endif  // YAX86_PICOCALC_FLOPPY_H
