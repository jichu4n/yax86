// The PicoCalc's LCD: a 320x320 IPS panel with an ST7365P controller, on a
// serial interface driven by PIO.
//
// Written to rather than scanned out: the controller keeps its own frame
// memory, so a host only sends what changed, as a rectangular window followed
// by that window's pixels in row-major order.
#ifndef YAX86_PICOCALC_LCD_H
#define YAX86_PICOCALC_LCD_H

#include <stdbool.h>
#include <stdint.h>

enum {
  kLcdWidth = 320,
  kLcdHeight = 320,
};

// Bring the panel up in RGB565 and clear it to black. Returns the serial clock
// actually achieved, in Hz, which is the requested one rounded down to what
// the system clock divides to.
uint32_t LcdInit(void);

// Begin a write to the rectangle at (x, y), width by height pixels. Pixels
// written afterwards fill it left to right, top to bottom, until
// LcdEndWrite(). No other LCD call may come in between.
void LcdBeginWrite(uint16_t x, uint16_t y, uint16_t width, uint16_t height);

// Write pixels into the window LcdBeginWrite() opened. A window may be filled
// over any number of calls, and each one costs only the transfer: the bus is
// set up for pixels once, when the write begins.
void LcdWritePixels(const uint16_t* pixels, uint32_t num_pixels);

// End the write LcdBeginWrite() began.
void LcdEndWrite(void);

// Begin reading the rectangle at (x, y), width by height pixels, back from
// the panel's frame memory: what it is showing, or would show if it were on.
// Pixels read afterwards come left to right, top to bottom, until
// LcdEndRead(). No other LCD call may come in between.
//
// A read is slow, about 2.7us a pixel, since the panel answers at no more than
// 6.67MHz. It is for debugging, not for drawing.
void LcdBeginRead(uint16_t x, uint16_t y, uint16_t width, uint16_t height);

// Read pixels from the window LcdBeginRead() opened, in RGB565, exactly as
// they were written.
void LcdReadPixels(uint16_t* pixels, uint32_t num_pixels);

// End the read LcdBeginRead() began.
void LcdEndRead(void);

// Turn the panel's output on or off. Off shows a blank page and puts the
// panel to sleep, at its lowest power; what it last held is kept, so turning
// it back on shows that again. A static image left showing for hours can
// leave its mark on the panel, which is what turning it off is for.
void LcdSetDisplayOn(bool is_on);

// Fill a rectangle with one color.
void LcdFillRect(
    uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);

// An 8-bit-per-channel color in the panel's RGB565.
static inline uint16_t LcdRGB565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

#endif  // YAX86_PICOCALC_LCD_H
