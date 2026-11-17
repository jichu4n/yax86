// The floppies in drives A and B.
//
// At power on each drive holds the image its VM's configuration names, a
// 360KB, 720KB, 1.2MB or 1.44MB floppy on the SD card that its drive type
// accepts, or nothing. With no VM to start, drive A holds the 360KB boot
// floppy built into flash and drive B is empty. What the guest writes to an
// image on the card is written to the card. The built-in floppy cannot be
// written, and what the guest writes to it is dropped. The menu can eject
// either drive's disk, and insert any floppy image on the card that fits the
// drive's type in its place.
#ifndef YAX86_PICOCALC_FLOPPY_H
#define YAX86_PICOCALC_FLOPPY_H

#include "core/fdc.h"
#include "disk_image.h"

typedef enum FloppyDrive {
  kFloppyDriveA = 0,
  kFloppyDriveB,
  kNumFloppyDrives,
} FloppyDrive;

// Open each drive's image on the SD card: a path from the card's root, or
// NULL or empty for none, checked against drive_types. A drive whose image
// does not open or does not fit its drive type is left empty. If is_built_in,
// drive A's path is to be empty, and it holds the built-in floppy instead, its
// image's status saying why there is none on the card: no card, or
// kDiskImageNotFound. Fills opened with how each drive's open went. Call once,
// before FloppyAttach().
void FloppyOpen(
    const char* const paths[kNumFloppyDrives],
    const YFDCDriveType drive_types[kNumFloppyDrives], bool is_built_in,
    DiskImageStatus opened[kNumFloppyDrives]);

// Hand the drives to the floppy controller, with their disks in.
void FloppyAttach(YFDCState* fdc);

// A drive's image on the card: whether it is open, and what it has cost.
// Drive A holds the built-in floppy if there was no VM at power on; a
// drive whose disk was ejected, and drive B until a disk goes in, is
// kDiskImageEjected.
const DiskImage* FloppyGetImage(FloppyDrive drive);

// The drive's mechanism type (360KB, 720KB, 1.2MB or 1.44MB).
YFDCDriveType FloppyGetDriveType(FloppyDrive drive);

// The drive whose image is the one at path, or kNumFloppyDrives if none is.
FloppyDrive FloppyDriveHolding(const char* path);

// Take the disk out of a drive, writing back what the guest was part way
// through writing.
void FloppyEject(YFDCState* fdc, FloppyDrive drive);

// Put the image at path on the card in a drive, in place of what was there.
// Returns kDiskImageOpen if it went in, or why not, in which case the drive
// is left empty. Only formats accepted by the drive's type fit.
DiskImageStatus FloppyInsertImage(
    YFDCState* fdc, FloppyDrive drive, const char* path);

#endif  // YAX86_PICOCALC_FLOPPY_H
