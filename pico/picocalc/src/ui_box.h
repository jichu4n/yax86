// The boxes the menu is drawn in over the guest's screen: code page 437's
// double lined frame, rows of text and items inside it, and the colors they
// are drawn in.
#ifndef YAX86_PICOCALC_UI_BOX_H
#define YAX86_PICOCALC_UI_BOX_H

#include <stdbool.h>
#include <stdint.h>

#include "display.h"

enum {
  // CGA attributes: white on blue for the box, and black on light gray for
  // the item chosen.
  kUIBoxAttrFrame = 0x1F,
  kUIBoxAttrText = 0x17,
  kUIBoxAttrChosen = 0x70,
  kUIBoxAttrTitle = 0x1E,

  // Code page 437's up and down arrows, which mark more of a list above or
  // below what shows.
  kUIBoxArrowUp = 0x18,
  kUIBoxArrowDown = 0x19,
};

// An item of a page, and whether choosing it opens another menu, which a
// chevron at its end says.
typedef struct UIBoxItem {
  const char* text;
  bool opens_menu;
} UIBoxItem;

// Fill a row of the box: its border, and its inside blank.
void UIBoxBlankRow(DisplayTextRow* row, uint8_t columns);

// The top or bottom edge of the box, with a title centered in the top one.
void UIBoxEdgeRow(
    DisplayTextRow* row, uint8_t columns, bool top, const char* title);

// A horizontal line across the box, separating groups of items.
void UIBoxSeparatorRow(DisplayTextRow* row, uint8_t columns);

// Put text inside a row of the box from column, clipped at its border.
void UIBoxPutText(
    DisplayTextRow* row, uint8_t columns, uint8_t column, const char* text,
    uint8_t attr);

// An item's row: its text, and the whole width inside the border marked if
// it is the item chosen.
void UIBoxItemRow(
    DisplayTextRow* row, uint8_t columns, const char* text, bool chosen);

// A menu item's row, with a chevron near the right border if it opens
// another menu.
void UIBoxMenuItemRow(
    DisplayTextRow* row, uint8_t columns, const UIBoxItem* item, bool chosen);

// Lay out a box of rows, centered on the guest's screen.
void UIBoxPlace(DisplayMenu* menu, uint8_t columns, uint8_t rows);

#endif  // YAX86_PICOCALC_UI_BOX_H
