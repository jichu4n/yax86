// 4x10 CP437 fonts, which fit 80 columns of text across the PicoCalc's 320
// pixels. See font_4x10.c for where each comes from.
#ifndef YAX86_PICOCALC_FONT_4X10_H
#define YAX86_PICOCALC_FONT_4X10_H

#include <stdbool.h>
#include <stdint.h>

enum {
  kFont4x10Width = 4,
  kFont4x10Height = 10,
  kFont4x10Glyphs = 256,
};

typedef enum Font4x10 {
  kFont4x10Ymini = 0,
  kFont4x10Atari800,
  kFont4x10Miniwi,
  kFont4x10Creep,
  kFont4x10Count,
  kFont4x10Default = kFont4x10Ymini,
} Font4x10;

// One byte per row, top to bottom, in the low four bits: bit 3 is the leftmost
// pixel. This is the layout the core's own fonts use, where bit (width - 1) is
// the leftmost pixel of a row.
typedef uint8_t Font4x10Table[kFont4x10Glyphs][kFont4x10Height];

// The font's glyphs.
const Font4x10Table* Font4x10Glyphs(Font4x10 font);

// The font's name in yax86.cfg, and in the menu.
const char* Font4x10Name(Font4x10 font);
const char* Font4x10Label(Font4x10 font);

// Find a font by its name in yax86.cfg, in any capitals.
bool Font4x10FromName(const char* name, Font4x10* font);

#endif  // YAX86_PICOCALC_FONT_4X10_H
