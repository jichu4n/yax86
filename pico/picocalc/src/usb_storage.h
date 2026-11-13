// The PicoCalc's card reader over USB: a mass storage function beside the
// serial port - see usb_descriptors.c.
//
// It is always there, so the serial port and picotool's reset interface stay
// up whatever it holds, and it is empty until the list of VMs at power on
// exports something through it:
//
// - The entire SD card. The host sees the card's own blocks - its partitions
//   and FAT volume - and reads and writes them as it would in a card reader of
//   its own. The firmware's own view of the card must be put away first:
//   nothing open, the volume unmounted, nothing touching the card until
//   exporting stops.
// - A disk image on the card: a floppy, a FAT volume with no partition table,
//   or a hard disk, with its own. Its blocks are the image's, read and written
//   through DiskImage while the card stays mounted, and nothing else may use
//   the image or the card's volume until exporting stops.
//
// The answers come from TinyUSB's callbacks, which pico_stdio_usb's background
// task runs from a low priority interrupt. A read or write goes to the card
// there and then: TinyUSB's task drains its queue before it returns, so a
// callback that answered "not yet" to leave the card to the main loop would be
// asked again at once, and the main loop would never run.
#ifndef YAX86_PICOCALC_USB_STORAGE_H
#define YAX86_PICOCALC_USB_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "disk_image.h"

// What exporting has come to, for the menu and the report.
typedef struct UsbStorageStats {
  // Whether anything is exported, and the image's path if that is what it is
  // - NULL for the entire card.
  bool exporting;
  const char* image_path;
  // Whether a host has the PicoCalc's USB device configured.
  bool connected;
  // Whether the host has seen the volume since it was exported - it asked
  // whether the reader was ready, and was told yes - which it does as it
  // mounts it.
  bool seen;
  // Whether the host has ejected it, after which the reader is empty to
  // it again.
  bool ejected;
  uint32_t blocks_read;
  uint32_t blocks_written;
  // The time the reads and writes took on the card, of all the time the
  // host took to move them: the rest is USB's.
  uint64_t read_us;
  uint64_t write_us;
  // Reads and writes the card failed, or that were past its end.
  uint32_t errors;
} UsbStorageStats;

// Export the SD card, which the caller has unmounted, to the host. The host
// is told the medium has changed, so it reads it afresh.
void UsbStorageExportCard(void);

// Export an image the caller has opened, whose size is a whole number of
// blocks, in the same way. The image must stay open until exporting stops.
void UsbStorageExportImage(DiskImage* image);

// Stop exporting: the reader is empty again, and the card or image the
// caller's.
void UsbStorageStopExporting(void);

UsbStorageStats UsbStorageGetStats(void);

#endif  // YAX86_PICOCALC_USB_STORAGE_H
