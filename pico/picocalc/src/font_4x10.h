// A 4x10 CP437 font, which is what fits 80 columns of text across the
// PicoCalc's 320 pixels. See font_4x10.c for where it comes from.
#ifndef YAX86_PICOCALC_FONT_4X10_H
#define YAX86_PICOCALC_FONT_4X10_H

#include <stdint.h>

enum {
  kFont4x10Width = 4,
  kFont4x10Height = 10,
  kFont4x10Glyphs = 256,
};

// One byte per row, top to bottom, in the low four bits: bit 3 is the leftmost
// pixel. This is the layout the core's own fonts use, where bit (width - 1) is
// the leftmost pixel of a row.
extern const uint8_t kFont4x10[kFont4x10Glyphs][kFont4x10Height];

#endif  // YAX86_PICOCALC_FONT_4X10_H
