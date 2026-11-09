// The hard disk: `yax86/hdd.img` on the SD card, as the hard disk
// controller's master drive, if the card has one.
//
// The image is the core's 10MB drive, 306 cylinders of 4 heads and 17
// sectors, and what the guest writes to it is written to the card. Without
// one there is no hard disk, and the machine is the one `dos-boot` measures.
#ifndef YAX86_PICOCALC_HARDDISK_H
#define YAX86_PICOCALC_HARDDISK_H

#include "core/hdc.h"
#include "disk_image.h"

// Look for the image on the SD card. Call once, before HardDiskAttach().
void HardDiskOpen(void);

// Attach the image to the hard disk controller, if it opened.
void HardDiskAttach(YHDCState* hdc);

// The image on the card: whether it opened, and what it has cost.
const DiskImage* HardDiskGetImage(void);

#endif  // YAX86_PICOCALC_HARDDISK_H
