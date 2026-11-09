// ST7365P driver for the PicoCalc's LCD.
//
// The register values are ClockworkPi's, from the PicoCalc sample code
// (Code/picocalc_helloworld/lcdspi/lcdspi.c), with one change: the pixel
// format is RGB565 rather than the samples' 18-bit RGB666. The ST7365P's serial
// interface accepts both (3Ah = 55h in the datasheet's serial interface
// section), so it is two bytes a pixel rather than three.
//
// The serial interface is driven by a PIO state machine rather than the
// RP2040's SPI block. The SPI block runs from the peripheral clock, which the
// SDK moves to the 48MHz USB PLL on an overclock, capping the LCD at 24MHz.
// Keeping it fast would mean overclocking the peripheral clock as well, and
// every peripheral on it with it. PIO runs from the system clock, which is
// overclocked already, and it is what ClockworkPi's own NES build drives this
// panel with at 250MHz.

#include "lcd.h"

#include <stddef.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/spi.h"
#include "lcd.pio.h"
#include "pico/time.h"

// The PIO block the serial interface runs on.
static PIO const kLcdPIO = pio0;
// The SPI block the panel's frame memory is read back through. SCK, MOSI and
// MISO are SPI1's pins, so a read hands them to it and back.
static spi_inst_t* const kLcdReadSPI = spi1;

enum {
  kLcdPinSCK = 10,
  kLcdPinMOSI = 11,
  kLcdPinMISO = 12,
  kLcdPinCS = 13,
  kLcdPinDC = 14,
  kLcdPinReset = 15,

  // The serial clock: a bit every two system clock cycles at the stock
  // 125MHz, and every four at 250MHz. The datasheet rates writes at a 66ns
  // cycle, about 15MHz, and ClockworkPi's samples run at 25MHz. The panel
  // draws cleanly at 62.5MHz, and at 125MHz against 20.83MHz it takes drawing
  // the guest's screen during dos-boot from 7.4% of the run to 5.2%.
  kLcdSPIHz = 62500 * 1000,
  // The state machine takes two cycles a bit.
  kLcdCyclesPerBit = 2,
  // How long the control lines are held around a transfer, which covers every
  // write timing in the datasheet's serial interface section: CS set up 15ns
  // before the first clock and held 15ns after the last, CS high for 40ns
  // between transfers, and DC set up and held 10ns either side of a clock.
  // At 250MHz the code between two control line changes can take as little
  // as a few nanoseconds, and without the hold the panel misreads writes: grey
  // text comes out green, as it would with every pixel's bytes swapped, and
  // through the SPI block it took no new image at all.
  kLcdGuardNs = 50,
  // The serial clock for reads. The datasheet's read cycle is at least 150ns,
  // under 6.67MHz, and ClockworkPi's samples read at 6MHz. The SPI block runs
  // from the 48MHz peripheral clock, which divides to 6MHz exactly.
  kLcdReadHz = 6000 * 1000,
  // Bytes a pixel read back. A read answers in the pixel format writes are
  // in, RGB565 high byte first, so a pixel reads back exactly as it was
  // written. ClockworkPi's samples read three bytes a pixel because they
  // write in RGB666.
  kLcdReadBytesPerPixel = 2,
  // How many pixels LcdReadPixels() reads from the SPI block at once.
  kLcdReadChunkPixels = 32,

  // Commands, from the ST7365P datasheet.
  kCmdSoftwareReset = 0x01,
  kCmdSleepIn = 0x10,
  kCmdSleepOut = 0x11,
  kCmdInversionOn = 0x21,
  kCmdDisplayOff = 0x28,
  kCmdDisplayOn = 0x29,
  kCmdColumnAddressSet = 0x2A,
  kCmdRowAddressSet = 0x2B,
  kCmdMemoryWrite = 0x2C,
  kCmdMemoryRead = 0x2E,
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
  // the next command, and 5ms after entering sleep.
  kResetPulseMs = 10,
  kResetRecoveryMs = 120,
  kSleepOutMs = 120,
  kSleepInMs = 5,
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

static uint g_lcd_sm;
// kLcdGuardNs in system clock cycles.
static uint32_t g_lcd_guard_cycles;

// Hold the control lines where they are for kLcdGuardNs.
static inline void LcdGuard(void) {
  busy_wait_at_least_cycles(g_lcd_guard_cycles);
}

// Queue a byte for the panel. The state machine pulls 8 bits at a time from
// the top of the word, and a byte-wide write to the FIFO is replicated across
// all four bytes of it, so the byte lands where it is taken from.
static inline void LcdPutByte(uint8_t value) {
  while (pio_sm_is_tx_fifo_full(kLcdPIO, g_lcd_sm)) {
  }
  *(volatile uint8_t*)&kLcdPIO->txf[g_lcd_sm] = value;
}

// Wait until the last bit queued has been clocked out, so that the control
// lines can change. The state machine stalls on an empty FIFO only once it has
// finished with what it had - but it was already stalled before the last byte
// was queued, and it can still be until it pulls that byte, so the stall flag
// is cleared only once the FIFO is empty. Cleared any earlier, it can be set
// again by the old stall and end the wait while the byte is still going out.
static void LcdWaitIdle(void) {
  while (!pio_sm_is_tx_fifo_empty(kLcdPIO, g_lcd_sm)) {
  }
  const uint32_t stall_mask = 1u << (PIO_FDEBUG_TXSTALL_LSB + g_lcd_sm);
  kLcdPIO->fdebug = stall_mask;
  while (!(kLcdPIO->fdebug & stall_mask)) {
  }
}

// Send a command byte and then its parameters.
static void LcdSendCommand(
    uint8_t command, const uint8_t* params, size_t num_params) {
  gpio_put(kLcdPinDC, 0);
  gpio_put(kLcdPinCS, 0);
  LcdGuard();
  LcdPutByte(command);
  LcdWaitIdle();
  LcdGuard();
  if (num_params > 0) {
    gpio_put(kLcdPinDC, 1);
    LcdGuard();
    for (size_t i = 0; i < num_params; ++i) {
      LcdPutByte(params[i]);
    }
    LcdWaitIdle();
    LcdGuard();
  }
  gpio_put(kLcdPinCS, 1);
  LcdGuard();
}

// Start the state machine, at the fastest clock no faster than kLcdSPIHz
// that the system clock divides to, and return that clock.
static uint32_t LcdInitSerial(void) {
  const uint32_t sys_hz = clock_get_hz(clk_sys);
  g_lcd_guard_cycles = sys_hz / (1000 * 1000) * kLcdGuardNs / 1000 + 1;
  const uint32_t cycles_per_bit_at_rate = kLcdCyclesPerBit * kLcdSPIHz;
  uint32_t divider =
      (sys_hz + cycles_per_bit_at_rate - 1) / cycles_per_bit_at_rate;
  if (divider == 0) {
    divider = 1;
  }

  const uint offset = (uint)pio_add_program(kLcdPIO, &lcd_serial_program);
  g_lcd_sm = (uint)pio_claim_unused_sm(kLcdPIO, true);
  pio_sm_config config = lcd_serial_program_get_default_config(offset);
  sm_config_set_out_pins(&config, kLcdPinMOSI, 1);
  sm_config_set_sideset_pins(&config, kLcdPinSCK);
  sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_TX);
  // Shifted out of the top of the word, most significant bit first, pulled
  // automatically a byte at a time.
  sm_config_set_out_shift(&config, false, true, 8);
  sm_config_set_clkdiv_int_frac8(&config, divider, 0);
  pio_gpio_init(kLcdPIO, kLcdPinMOSI);
  pio_gpio_init(kLcdPIO, kLcdPinSCK);
  pio_sm_set_consecutive_pindirs(kLcdPIO, g_lcd_sm, kLcdPinMOSI, 1, true);
  pio_sm_set_consecutive_pindirs(kLcdPIO, g_lcd_sm, kLcdPinSCK, 1, true);
  pio_sm_init(kLcdPIO, g_lcd_sm, offset, &config);
  pio_sm_set_enabled(kLcdPIO, g_lcd_sm, true);
  return sys_hz / (kLcdCyclesPerBit * divider);
}

uint32_t LcdInit(void) {
  const uint32_t actual_hz = LcdInitSerial();

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

void LcdSetDisplayOn(bool on) {
  if (on) {
    LcdSendCommand(kCmdSleepOut, NULL, 0);
    sleep_ms(kSleepOutMs);
    LcdSendCommand(kCmdDisplayOn, NULL, 0);
  } else {
    // Blank first, so that the panel never shows what it looks like while it
    // goes to sleep.
    LcdSendCommand(kCmdDisplayOff, NULL, 0);
    LcdSendCommand(kCmdSleepIn, NULL, 0);
    sleep_ms(kSleepInMs);
  }
}

// Set the window the next memory write or read covers.
static void LcdSetWindow(
    uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
  const uint16_t right = (uint16_t)(x + width - 1);
  const uint16_t bottom = (uint16_t)(y + height - 1);
  const uint8_t columns[] = {
      (uint8_t)(x >> 8), (uint8_t)x, (uint8_t)(right >> 8), (uint8_t)right};
  const uint8_t rows[] = {
      (uint8_t)(y >> 8), (uint8_t)y, (uint8_t)(bottom >> 8), (uint8_t)bottom};
  LcdSendCommand(kCmdColumnAddressSet, columns, sizeof(columns));
  LcdSendCommand(kCmdRowAddressSet, rows, sizeof(rows));
}

void LcdBeginWrite(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
  LcdSetWindow(x, y, width, height);
  LcdSendCommand(kCmdMemoryWrite, NULL, 0);

  // DC and CS are set once here rather than per call, since the core hands
  // pixels over 32 at a time.
  gpio_put(kLcdPinDC, 1);
  gpio_put(kLcdPinCS, 0);
  LcdGuard();
}

void LcdWritePixels(const uint16_t* pixels, uint32_t count) {
  // High byte first, which is the order the panel expects.
  for (uint32_t i = 0; i < count; ++i) {
    LcdPutByte((uint8_t)(pixels[i] >> 8));
    LcdPutByte((uint8_t)pixels[i]);
  }
}

void LcdEndWrite(void) {
  LcdWaitIdle();
  LcdGuard();
  gpio_put(kLcdPinCS, 1);
  LcdGuard();
}

void LcdBeginRead(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
  LcdSetWindow(x, y, width, height);
  // The command goes out through the state machine like any other, and CS
  // stays low after it: what follows is the command's answer.
  gpio_put(kLcdPinDC, 0);
  gpio_put(kLcdPinCS, 0);
  LcdGuard();
  LcdPutByte(kCmdMemoryRead);
  LcdWaitIdle();
  LcdGuard();
  gpio_put(kLcdPinDC, 1);
  // The answer is read through the SPI block, at a clock slow enough for a
  // read. Both idle with the clock low, so handing the pins over moves
  // nothing the panel sees.
  spi_init(kLcdReadSPI, kLcdReadHz);
  gpio_set_function(kLcdPinSCK, GPIO_FUNC_SPI);
  gpio_set_function(kLcdPinMOSI, GPIO_FUNC_SPI);
  gpio_set_function(kLcdPinMISO, GPIO_FUNC_SPI);
  LcdGuard();
  // The first byte of the answer is a dummy.
  uint8_t dummy;
  spi_read_blocking(kLcdReadSPI, 0xFF, &dummy, 1);
}

void LcdReadPixels(uint16_t* pixels, uint32_t count) {
  uint8_t bytes[kLcdReadChunkPixels * kLcdReadBytesPerPixel];
  while (count > 0) {
    const uint32_t chunk =
        count < kLcdReadChunkPixels ? count : kLcdReadChunkPixels;
    spi_read_blocking(kLcdReadSPI, 0xFF, bytes, chunk * kLcdReadBytesPerPixel);
    const uint8_t* byte = bytes;
    for (uint32_t i = 0; i < chunk; ++i) {
      *pixels++ = (uint16_t)((byte[0] << 8) | byte[1]);
      byte += kLcdReadBytesPerPixel;
    }
    count -= chunk;
  }
}

void LcdEndRead(void) {
  LcdGuard();
  gpio_put(kLcdPinCS, 1);
  // Back to the state machine, and MISO back to an input nothing drives.
  pio_gpio_init(kLcdPIO, kLcdPinSCK);
  pio_gpio_init(kLcdPIO, kLcdPinMOSI);
  gpio_set_function(kLcdPinMISO, GPIO_FUNC_NULL);
  spi_deinit(kLcdReadSPI);
  LcdGuard();
}

void LcdFillRect(
    uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
  if (width == 0 || height == 0) {
    return;
  }
  for (uint16_t i = 0; i < width; ++i) {
    g_fill_row[i] = color;
  }
  LcdBeginWrite(x, y, width, height);
  for (uint16_t row = 0; row < height; ++row) {
    LcdWritePixels(g_fill_row, width);
  }
  LcdEndWrite();
}
