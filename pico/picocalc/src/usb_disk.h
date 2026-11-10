// The PicoCalc's card reader over USB: a mass storage function beside the
// serial port - see usb_descriptors.c.
//
// It is always there, so the serial port and picotool's reset interface stay
// up whatever it holds, and it is empty until the list of VMs at power on
// shares the SD card through it. A computer then sees the card's own blocks -
// its partitions and FAT volume - and reads and writes them as it would in a
// card reader of its own. The firmware's own view of the card must be put away
// first: nothing open, the volume unmounted, nothing touching the card until
// sharing stops.
//
// The answers come from TinyUSB's callbacks, which pico_stdio_usb's background
// task runs from a low priority interrupt. A read or write goes to the card
// there and then: TinyUSB's task drains its queue before it returns, so a
// callback that answered "not yet" to leave the card to the main loop would be
// asked again at once, and the main loop would never run.
#ifndef YAX86_PICOCALC_USB_DISK_H
#define YAX86_PICOCALC_USB_DISK_H

#include <stdbool.h>
#include <stdint.h>

// What sharing has come to, for the menu and the report.
typedef struct UsbDiskStats {
  // Whether the card is shared at all.
  bool sharing;
  // Whether a computer has the PicoCalc's USB device configured.
  bool connected;
  // Whether the computer has seen the card since it was shared - it asked
  // whether the reader was ready, and was told yes.
  bool seen;
  // Whether the computer has ejected it, after which the reader is empty to
  // it again.
  bool ejected;
  uint32_t blocks_read;
  uint32_t blocks_written;
  // Reads and writes the card failed, or that were past its end.
  uint32_t errors;
} UsbDiskStats;

// Share the SD card, which the caller has unmounted, with the computer. The
// computer is told the medium has changed, so it reads it afresh.
void UsbDiskShareCard(void);

// Stop sharing: the reader is empty again, and the card the caller's.
void UsbDiskStopSharing(void);

UsbDiskStats UsbDiskGetStats(void);

#endif  // YAX86_PICOCALC_USB_DISK_H
