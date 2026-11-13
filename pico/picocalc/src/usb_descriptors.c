// What the PicoCalc is to a computer over USB: one device with three
// functions.
//
//   interfaces 0 and 1  the serial port, CDC, which pico_stdio_usb drives
//   interface 2         picotool's reset interface, from pico_usb_reset
//   interface 3         a card reader, mass storage - see usb_storage.c
//
// pico_stdio_usb describes the first two itself, but not once an application
// links TinyUSB, as a third function needs, so all three are described here.
// The serial port and the reset interface are as pico_stdio_usb has them, so
// hosts find the board as they did: a /dev/ttyACM port, and picotool's
// `reboot -f`. The vendor and product IDs are the SDK's.
#include "pico/unique_id.h"
#include "pico/usb_reset_interface.h"
#include "pico/usb_reset_tusb.h"
#include "tusb.h"

enum {
  kVendorID = 0x2E8A,
  // The Pico SDK's ID for a device of its on an RP2040.
  kProductID = 0x000A,
  // A device release number, in binary coded decimal: 1.00.
  kDeviceRelease = 0x0100,
  // USB 2.1, which says there is a binary object store to ask for, where the
  // reset interface's Microsoft OS 2.0 descriptor is - without one Windows
  // will not use it without a driver.
  kUSBVersion = 0x0210,
  kMaxPowerMilliamps = 250,

  kInterfaceCDC = 0,
  kInterfaceReset = 2,
  kInterfaceMSC = 3,
  kInterfaces = 4,

  kEndpointCDCNotify = 0x81,
  kEndpointCDCOut = 0x02,
  kEndpointCDCIn = 0x82,
  kEndpointMSCOut = 0x03,
  kEndpointMSCIn = 0x83,
  kCDCNotifySize = 8,
  // Full speed's largest bulk packet.
  kBulkPacketSize = 64,

  kStringLanguage = 0,
  kStringManufacturer,
  kStringProduct,
  kStringSerial,
  kStringCDC,
  kStringReset,
  kStringMSC,
  kStrings,

  // US English, the only language the strings are in.
  kLanguageEnglishUS = 0x0409,
  // The longest string sent, in characters.
  kStringMaxLength = 31,

  kConfigurationLength = TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN +
      TUD_RPI_RESET_DESC_LEN + TUD_MSC_DESC_LEN,
};

_Static_assert(
    kInterfaceReset == PICO_USB_RESET_MS_OS_20_DESCRIPTOR_ITF,
    "the reset interface's Microsoft OS 2.0 descriptor names its number");

static const tusb_desc_device_t kDevice = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = kUSBVersion,
    // Functions with more than one interface say so with an interface
    // association descriptor, which this class and protocol announce.
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = kVendorID,
    .idProduct = kProductID,
    .bcdDevice = kDeviceRelease,
    .iManufacturer = kStringManufacturer,
    .iProduct = kStringProduct,
    .iSerialNumber = kStringSerial,
    .bNumConfigurations = 1,
};

static const uint8_t kConfiguration[kConfigurationLength] = {
    TUD_CONFIG_DESCRIPTOR(
        1, kInterfaces, 0, kConfigurationLength, 0, kMaxPowerMilliamps),
    TUD_CDC_DESCRIPTOR(
        kInterfaceCDC, kStringCDC, kEndpointCDCNotify, kCDCNotifySize,
        kEndpointCDCOut, kEndpointCDCIn, kBulkPacketSize),
    TUD_RPI_RESET_DESCRIPTOR(kInterfaceReset, kStringReset),
    TUD_MSC_DESCRIPTOR(
        kInterfaceMSC, kStringMSC, kEndpointMSCOut, kEndpointMSCIn,
        kBulkPacketSize),
};

// The board's unique ID in hex, filled in when first asked for.
static char g_serial[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];

static const char* const kStringText[kStrings] = {
    [kStringManufacturer] = "Raspberry Pi",
    [kStringProduct] = "Pico",
    [kStringSerial] = g_serial,
    [kStringCDC] = "Board CDC",
    [kStringReset] = "Reset",
    [kStringMSC] = "PicoCalc SD",
};

const uint8_t* tud_descriptor_device_cb(void) {
  return (const uint8_t*)&kDevice;
}

const uint8_t* tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
  return kConfiguration;
}

const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t language) {
  (void)language;
  // A string descriptor: its length in bytes and type, then UTF-16.
  static uint16_t descriptor[1 + kStringMaxLength];
  uint8_t length = 0;
  if (index == kStringLanguage) {
    descriptor[1] = kLanguageEnglishUS;
    length = 1;
  } else {
    if (index >= kStrings || kStringText[index] == NULL) {
      return NULL;
    }
    if (g_serial[0] == '\0') {
      pico_get_unique_board_id_string(g_serial, sizeof(g_serial));
    }
    const char* text = kStringText[index];
    while (length < kStringMaxLength && text[length] != '\0') {
      descriptor[1 + length] = (uint8_t)text[length];
      ++length;
    }
  }
  descriptor[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * length + 2));
  return descriptor;
}
