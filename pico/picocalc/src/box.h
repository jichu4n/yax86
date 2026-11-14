// A box: what every screen in both modes draws over the guest's screen,
// which display.h calls a DisplayMenu. It has code page 437's double-lined
// border, a title in its top edge, and rows of text and items inside. This has the colors a box is drawn in, draws its rows, and centers
// it on the guest's screen. list_box.h builds a list to select from on it.
#ifndef YAX86_PICOCALC_BOX_H
#define YAX86_PICOCALC_BOX_H

#include <stdbool.h>
#include <stdint.h>

#include "display.h"

enum {
  // CGA attributes, a background color and a foreground color each. The
  // border is white on blue, text light gray on blue, the item selected
  // black on light gray, and the title yellow on blue.
  kBoxAttrBorder = 0x1F,
  kBoxAttrText = 0x17,
  kBoxAttrSelected = 0x70,
  kBoxAttrTitle = 0x1E,

  // Code page 437's up and down arrows, which mark more of a list above or
  // below what shows.
  kBoxArrowUp = 0x18,
  kBoxArrowDown = 0x19,
};

// An item in a box.
typedef struct BoxItem {
  // The item's text.
  const char* text;
  // Whether selecting it opens another box, which a chevron at its end says.
  bool opens_box;
} BoxItem;

// Fill a row of the box: its border, and its inside blank.
void BoxBlankRow(DisplayTextRow* row, uint8_t columns);

// The top or bottom edge of the box, with a title centered in the top one.
void BoxEdgeRow(
    DisplayTextRow* row, uint8_t columns, bool top, const char* title);

// A horizontal line across the box, separating groups of items.
void BoxSeparatorRow(DisplayTextRow* row, uint8_t columns);

// Put text inside a row of the box from column, clipped at its border.
void BoxPutText(
    DisplayTextRow* row, uint8_t columns, uint8_t column, const char* text,
    uint8_t attr);

// An item's row: its text, and the whole width inside the border marked if
// it is the item selected.
void BoxItemTextRow(
    DisplayTextRow* row, uint8_t columns, const char* text, bool selected);

// An item's row, with a chevron near the right border if selecting it opens
// another box.
void BoxItemRow(
    DisplayTextRow* row, uint8_t columns, const BoxItem* item, bool selected);

// Lay out a box of rows, centered on the guest's screen.
void BoxPlace(DisplayMenu* menu, uint8_t columns, uint8_t rows);

#endif  // YAX86_PICOCALC_BOX_H
