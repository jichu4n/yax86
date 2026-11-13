// The boxes drawn over the guest's screen, display.h's DisplayMenu, by the
// in-VM menu and by every screen in both modes: code page 437's double lined
// frame, rows of text and items inside it, and the colors they are drawn
// in.
#ifndef YAX86_PICOCALC_MENU_BOX_H
#define YAX86_PICOCALC_MENU_BOX_H

#include <stdbool.h>
#include <stdint.h>

#include "display.h"

enum {
  // CGA attributes: white on blue for the box, and black on light gray for
  // the item selected.
  kMenuBoxAttrFrame = 0x1F,
  kMenuBoxAttrText = 0x17,
  kMenuBoxAttrSelected = 0x70,
  kMenuBoxAttrTitle = 0x1E,

  // Code page 437's up and down arrows, which mark more of a list above or
  // below what shows.
  kMenuBoxArrowUp = 0x18,
  kMenuBoxArrowDown = 0x19,
};

// An item in a box.
typedef struct MenuBoxItem {
  // The item's text.
  const char* text;
  // Whether selecting it opens another box, which a chevron at its end says.
  bool opens_menu;
} MenuBoxItem;

// Fill a row of the box: its border, and its inside blank.
void MenuBoxBlankRow(DisplayTextRow* row, uint8_t columns);

// The top or bottom edge of the box, with a title centered in the top one.
void MenuBoxEdgeRow(
    DisplayTextRow* row, uint8_t columns, bool top, const char* title);

// A horizontal line across the box, separating groups of items.
void MenuBoxSeparatorRow(DisplayTextRow* row, uint8_t columns);

// Put text inside a row of the box from column, clipped at its border.
void MenuBoxPutText(
    DisplayTextRow* row, uint8_t columns, uint8_t column, const char* text,
    uint8_t attr);

// An item's row: its text, and the whole width inside the border marked if
// it is the item selected.
void MenuBoxItemTextRow(
    DisplayTextRow* row, uint8_t columns, const char* text, bool selected);

// A menu item's row, with a chevron near the right border if it opens
// another menu.
void MenuBoxItemRow(
    DisplayTextRow* row, uint8_t columns, const MenuBoxItem* item,
    bool selected);

// Lay out a box of rows, centered on the guest's screen.
void MenuBoxPlace(DisplayMenu* menu, uint8_t columns, uint8_t rows);

#endif  // YAX86_PICOCALC_MENU_BOX_H
