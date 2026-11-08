// The PicoCalc's LCD: a 320x320 IPS panel with an ST7365P controller, on SPI1.
//
// Written to rather than scanned out: the controller keeps its own frame
// memory, so a host only sends what changed, as a rectangular window followed
// by that window's pixels in row-major order.
#ifndef YAX86_PICOCALC_LCD_H
#define YAX86_PICOCALC_LCD_H

#include <stdint.h>

enum {
  kLcdWidth = 320,
  kLcdHeight = 320,
};

// Bring the panel up in RGB565 and clear it to black. Returns the SPI clock
// actually achieved, in Hz, which is the requested one rounded down to what
// the peripheral clock divides to.
uint32_t LcdInit(void);

// Begin a write to the rectangle at (x, y), width by height pixels. Pixels
// written afterwards fill it left to right, top to bottom, until
// LcdEndWrite(). No other LCD call may come in between.
void LcdBeginWrite(uint16_t x, uint16_t y, uint16_t width, uint16_t height);

// Write pixels into the window LcdBeginWrite() opened. A window may be filled
// over any number of calls, and each one costs only the transfer: the bus is
// set up for pixels once, when the write begins.
void LcdWritePixels(const uint16_t* pixels, uint32_t count);

// End the write LcdBeginWrite() began.
void LcdEndWrite(void);

// Fill a rectangle with one color.
void LcdFillRect(
    uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);

// An 8-bit-per-channel color in the panel's RGB565.
static inline uint16_t LcdRGB565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

#endif  // YAX86_PICOCALC_LCD_H
