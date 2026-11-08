// ST7365P driver for the PicoCalc's LCD, over the RP2040's hardware SPI.
//
// The register values are ClockworkPi's, from the PicoCalc sample code
// (Code/picocalc_helloworld/lcdspi/lcdspi.c), with one change: the pixel
// format is RGB565 rather than the samples' 18-bit RGB666. The ST7365P's serial
// interface accepts both (3Ah = 55h in the datasheet's serial interface
// section), so it is two bytes a pixel rather than three.

#include "lcd.h"

#include <stddef.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/time.h"

// The SPI block the panel is wired to.
static spi_inst_t* const kLcdSPI = spi1;

enum {
  kLcdPinSCK = 10,
  kLcdPinMOSI = 11,
  kLcdPinCS = 13,
  kLcdPinDC = 14,
  kLcdPinReset = 15,

  // ClockworkPi's samples run the panel at 25MHz, which is what this starts
  // from. The datasheet rates writes at a 66ns cycle, about 15MHz, so that is
  // already above the rating. How far it will go is a measurement for when
  // rendering is on the screen.
  kLcdSPIHz = 25 * 1000 * 1000,

  // Commands, from the ST7365P datasheet.
  kCmdSoftwareReset = 0x01,
  kCmdSleepOut = 0x11,
  kCmdInversionOn = 0x21,
  kCmdDisplayOn = 0x29,
  kCmdColumnAddressSet = 0x2A,
  kCmdRowAddressSet = 0x2B,
  kCmdMemoryWrite = 0x2C,
  kCmdMemoryAccessControl = 0x36,
  kCmdPixelFormat = 0x3A,
  kCmdInterfaceModeControl = 0xB0,
  kCmdFrameRateControl = 0xB1,
  kCmdInversionControl = 0xB4,
  kCmdDisplayFunctionControl = 0xB6,
  kCmdEntryModeSet = 0xB7,
  kCmdPowerControl1 = 0xC0,
  kCmdPowerControl2 = 0xC1,
  kCmdVCOMControl = 0xC5,
  kCmdPositiveGamma = 0xE0,
  kCmdNegativeGamma = 0xE1,
  kCmdSetImageFunction = 0xE9,
  kCmdAdjustControl3 = 0xF7,

  // Memory access control: mirror the columns, and BGR panel order. The same
  // value ClockworkPi uses.
  kMemoryAccessColumnMirror = 0x40,
  kMemoryAccessBGR = 0x08,
  // 16 bits a pixel on both the RGB and the serial interface.
  kPixelFormatRGB565 = 0x55,

  // Delays the datasheet asks for: 120ms after a reset or leaving sleep before
  // the next command.
  kResetPulseMs = 10,
  kResetRecoveryMs = 120,
  kSleepOutMs = 120,
};

// A command and its parameters. Kept as a table so that the init sequence
// reads as the list of register values it is.
typedef struct LcdCommand {
  uint8_t command;
  uint8_t num_params;
  uint8_t params[15];
} LcdCommand;

static const LcdCommand kInitSequence[] = {
    {kCmdPositiveGamma,
     15,
     {0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08,
      0x16, 0x1A, 0x0F}},
    {kCmdNegativeGamma,
     15,
     {0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D,
      0x35, 0x37, 0x0F}},
    {kCmdPowerControl1, 2, {0x17, 0x15}},
    {kCmdPowerControl2, 1, {0x41}},
    {kCmdVCOMControl, 3, {0x00, 0x12, 0x80}},
    {kCmdMemoryAccessControl,
     1,
     {kMemoryAccessColumnMirror | kMemoryAccessBGR}},
    {kCmdPixelFormat, 1, {kPixelFormatRGB565}},
    {kCmdInterfaceModeControl, 1, {0x00}},
    {kCmdFrameRateControl, 1, {0xA0}},
    {kCmdInversionOn, 0, {0}},
    {kCmdInversionControl, 1, {0x02}},
    {kCmdDisplayFunctionControl, 3, {0x02, 0x02, 0x3B}},
    {kCmdEntryModeSet, 1, {0xC6}},
    {kCmdSetImageFunction, 1, {0x00}},
    {kCmdAdjustControl3, 4, {0xA9, 0x51, 0x2C, 0x82}},
};

// One row of the panel, for fills.
static uint16_t g_fill_row[kLcdWidth];

// Send a command byte and then its parameters. The SPI write functions return
// only once the last bit has left the shifter, so the control lines can change
// as soon as they return.
static void LcdSendCommand(
    uint8_t command, const uint8_t* params, size_t num_params) {
  gpio_put(kLcdPinDC, 0);
  gpio_put(kLcdPinCS, 0);
  spi_write_blocking(kLcdSPI, &command, 1);
  if (num_params > 0) {
    gpio_put(kLcdPinDC, 1);
    spi_write_blocking(kLcdSPI, params, num_params);
  }
  gpio_put(kLcdPinCS, 1);
}

uint32_t LcdInit(void) {
  const uint32_t actual_hz = spi_init(kLcdSPI, kLcdSPIHz);
  gpio_set_function(kLcdPinSCK, GPIO_FUNC_SPI);
  gpio_set_function(kLcdPinMOSI, GPIO_FUNC_SPI);

  const uint32_t control_pins[] = {kLcdPinCS, kLcdPinDC, kLcdPinReset};
  for (size_t i = 0; i < sizeof(control_pins) / sizeof(control_pins[0]); ++i) {
    gpio_init(control_pins[i]);
    gpio_set_dir(control_pins[i], GPIO_OUT);
    gpio_put(control_pins[i], 1);
  }

  gpio_put(kLcdPinReset, 0);
  sleep_ms(kResetPulseMs);
  gpio_put(kLcdPinReset, 1);
  sleep_ms(kResetRecoveryMs);

  LcdSendCommand(kCmdSoftwareReset, NULL, 0);
  sleep_ms(kResetRecoveryMs);
  for (size_t i = 0; i < sizeof(kInitSequence) / sizeof(kInitSequence[0]);
       ++i) {
    LcdSendCommand(
        kInitSequence[i].command, kInitSequence[i].params,
        kInitSequence[i].num_params);
  }
  LcdSendCommand(kCmdSleepOut, NULL, 0);
  sleep_ms(kSleepOutMs);

  // Cleared before the display is switched on, so that whatever the frame
  // memory held at power on is never shown.
  LcdFillRect(0, 0, kLcdWidth, kLcdHeight, 0);
  LcdSendCommand(kCmdDisplayOn, NULL, 0);
  return actual_hz;
}

void LcdSetWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
  const uint16_t right = (uint16_t)(x + width - 1);
  const uint16_t bottom = (uint16_t)(y + height - 1);
  const uint8_t columns[] = {
      (uint8_t)(x >> 8), (uint8_t)x, (uint8_t)(right >> 8), (uint8_t)right};
  const uint8_t rows[] = {
      (uint8_t)(y >> 8), (uint8_t)y, (uint8_t)(bottom >> 8), (uint8_t)bottom};
  LcdSendCommand(kCmdColumnAddressSet, columns, sizeof(columns));
  LcdSendCommand(kCmdRowAddressSet, rows, sizeof(rows));
  LcdSendCommand(kCmdMemoryWrite, NULL, 0);
}

// Raising CS between transfers does not end a memory write - only the next
// command does - so each call can be a transfer of its own.
void LcdWritePixels(const uint16_t* pixels, uint32_t count) {
  // Sent as 16-bit frames, which go out most significant bit first: the byte
  // order the panel expects, with no swapping in between.
  spi_set_format(kLcdSPI, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
  gpio_put(kLcdPinDC, 1);
  gpio_put(kLcdPinCS, 0);
  spi_write16_blocking(kLcdSPI, pixels, count);
  gpio_put(kLcdPinCS, 1);
  spi_set_format(kLcdSPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

void LcdFillRect(
    uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
  if (width == 0 || height == 0) {
    return;
  }
  for (uint16_t i = 0; i < width; ++i) {
    g_fill_row[i] = color;
  }
  LcdSetWindow(x, y, width, height);
  for (uint16_t row = 0; row < height; ++row) {
    LcdWritePixels(g_fill_row, width);
  }
}
