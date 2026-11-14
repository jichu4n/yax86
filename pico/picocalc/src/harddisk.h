// The hard disks: images on the SD card as the hard disk controller's master,
// drive C, and slave, drive D.
//
// An image is the core's 10MB drive, 306 cylinders of 4 heads and 17
// sectors, and what the guest writes to it is written to the card. A drive
// with no image is not attached, and with no VM to start neither is.
#ifndef YAX86_PICOCALC_HARDDISK_H
#define YAX86_PICOCALC_HARDDISK_H

#include "core/hdc.h"
#include "disk_image.h"

typedef enum HardDiskDrive {
  kHardDisk1 = 0,
  kHardDisk2,
  kNumHardDisks,
} HardDiskDrive;

// Open each drive's image on the SD card: a path from the card's root, or
// NULL or empty for none, which leaves the drive kDiskImageEjected. Call
// once, before HardDiskAttach(). The paths are copied.
void HardDiskOpen(const char* const paths[kNumHardDisks]);

// Attach the drives whose images opened to the hard disk controller.
void HardDiskAttach(YHDCState* hdc);

// A drive's image on the card: whether it opened, and what it has cost.
const DiskImage* HardDiskGetImage(HardDiskDrive drive);

#endif  // YAX86_PICOCALC_HARDDISK_H
