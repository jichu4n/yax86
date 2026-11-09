// Text on the LCD in the 4x10 font, on an 80 by 32 grid of character cells.
//
// This is the runtime's own text - the banner and its status lines - and not
// the guest's screen, which the core renders.
#ifndef YAX86_PICOCALC_TEXT_H
#define YAX86_PICOCALC_TEXT_H

#include <stdint.h>

#include "font_4x10.h"
#include "lcd.h"

enum {
  kTextColumns = kLcdWidth / kFont4x10Width,
  kTextRows = kLcdHeight / kFont4x10Height,
};

// Draw a string of code page 437 characters starting at a cell, clipped at the
// right edge of the screen.
void TextDrawString(
    uint8_t column, uint8_t row, const char* text, uint16_t foreground,
    uint16_t background);

// Draw length cells starting at a cell, each with its own colors: attrs holds
// a CGA attribute for each, the foreground in the low nibble and the
// background in the high, as the guest's text modes have them, and palette
// the 16 colors they index. Clipped at the right edge of the screen.
void TextDrawCells(
    uint8_t column, uint8_t row, const char* chars, const uint8_t* attrs,
    uint8_t length, const uint16_t palette[16]);

#endif  // YAX86_PICOCALC_TEXT_H
