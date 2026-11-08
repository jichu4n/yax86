#include "text.h"

// One scan line of the string being drawn. A string is sent as a single window
// a line at a time, rather than a window per glyph, which would be three
// commands for every 40 pixels.
static uint16_t g_text_line[kLcdWidth];

void TextDrawString(
    uint8_t column, uint8_t row, const char* text, uint16_t foreground,
    uint16_t background) {
  if (column >= kTextColumns || row >= kTextRows) {
    return;
  }
  uint8_t length = 0;
  while (text[length] != '\0' && column + length < kTextColumns) {
    ++length;
  }
  if (length == 0) {
    return;
  }

  const uint16_t width = (uint16_t)(length * kFont4x10Width);
  LcdBeginWrite(
      (uint16_t)(column * kFont4x10Width), (uint16_t)(row * kFont4x10Height),
      width, kFont4x10Height);
  for (uint8_t y = 0; y < kFont4x10Height; ++y) {
    uint16_t* pixel = g_text_line;
    for (uint8_t i = 0; i < length; ++i) {
      const uint8_t bits = kFont4x10[(uint8_t)text[i]][y];
      for (uint8_t x = 0; x < kFont4x10Width; ++x) {
        *pixel++ =
            (bits & (1u << (kFont4x10Width - 1 - x))) ? foreground : background;
      }
    }
    LcdWritePixels(g_text_line, width);
  }
  LcdEndWrite();
}
