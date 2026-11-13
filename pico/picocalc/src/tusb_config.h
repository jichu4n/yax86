// TinyUSB's configuration: the USB device the PicoCalc is to a computer - see
// usb_descriptors.c. TinyUSB includes it as tusb_config.h, and it is found
// ahead of the one pico_stdio_usb carries, which configures nothing once an
// application links TinyUSB itself.
#ifndef YAX86_PICOCALC_TUSB_CONFIG_H
#define YAX86_PICOCALC_TUSB_CONFIG_H

#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE)

// The serial port pico_stdio_usb writes to and commands come in on, with the
// buffers pico_stdio_usb's own configuration gives it.
#define CFG_TUD_CDC (1)
#define CFG_TUD_CDC_RX_BUFSIZE (64)
#define CFG_TUD_CDC_TX_BUFSIZE (64)
#define CFG_TUD_CDC_EP_BUFSIZE (64)

// The card reader - see usb_storage.c. A read or write is handed over 8KB -
// sixteen blocks, one command to the card - at a time. 16KB writes 4.5%
// faster for another 8KB of RAM; reads are bound by USB at either.
#define CFG_TUD_MSC (1)
#define CFG_TUD_MSC_EP_BUFSIZE (8192)

// picotool's reset interface is vendor specific, with pico_usb_reset's own
// driver rather than TinyUSB's.
#define CFG_TUD_VENDOR (0)

#endif  // YAX86_PICOCALC_TUSB_CONFIG_H
