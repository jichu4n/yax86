// The PicoCalc's card reader over USB: a mass storage function beside the
// serial port - see usb_descriptors.c.
//
// It is always there, so the serial port and picotool's reset interface stay
// up whatever it holds, and for now it holds nothing: a computer sees an empty
// card reader, removable, and asks it again now and then whether a card has
// gone in. The answers come from TinyUSB's callbacks in usb_disk.c, which run
// in pico_stdio_usb's background task.
#ifndef YAX86_PICOCALC_USB_DISK_H
#define YAX86_PICOCALC_USB_DISK_H

#endif  // YAX86_PICOCALC_USB_DISK_H
