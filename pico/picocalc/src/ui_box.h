#ifndef YAX86_PICOCALC_UI_BOX_H
#define YAX86_PICOCALC_UI_BOX_H

#include <stdbool.h>
#include <stdint.h>

#include "display.h"

enum {
  // Common box dimensions.
  kUIBoxGuestScreenRows = 25,
  kUIBoxNarrowColumns = 40,
  kUIBoxWideColumns = kTextColumns,

  // CGA attributes: white on blue for the box, and black on light gray for
  // the item chosen.
  kUIBoxAttrFrame = 0x1F,
  kUIBoxAttrText = 0x17,
  kUIBoxAttrChosen = 0x70,
  kUIBoxAttrTitle = 0x1E,

  // Code page 437's double lined box, and the single line that separates
  // sections inside it.
  kUIBoxTopLeft = 0xC9,
  kUIBoxTopRight = 0xBB,
  kUIBoxBottomLeft = 0xC8,
  kUIBoxBottomRight = 0xBC,
  kUIBoxHorizontal = 0xCD,
  kUIBoxVertical = 0xBA,
  kUIBoxSeparatorLeft = 0xC7,
  kUIBoxSeparatorRight = 0xB6,
  kUIBoxSeparator = 0xC4,

  // Code page 437's up and down arrows, which mark more of a list above or
  // below what shows.
  kUIBoxArrowUp = 0x18,
  kUIBoxArrowDown = 0x19,
};

// An item of a page, and whether choosing it opens another menu, which a
// chevron at its end says.
typedef struct MenuItem {
  const char* text;
  bool opens_menu;
} MenuItem;

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
    DisplayTextRow* row, uint8_t columns, const MenuItem* item, bool chosen);

// Lay out a box of rows, centered on the guest's screen.
void UIBoxPlace(DisplayMenu* menu, uint8_t columns, uint8_t rows);

// A page of items, under lines of text, if any.
void UIBoxDrawItems(
    DisplayMenu* menu, const char* title, const char* const* lines,
    uint8_t line_count, const MenuItem* items, uint8_t count, uint8_t chosen,
    bool choosing);

// Adjust *top so that row stays visible within visible_rows.
void UIBoxScrollTo(uint8_t row, uint8_t* top, uint8_t visible_rows);

// Draw up/down arrows on the box right border when scrollable content exists.
void UIBoxDrawScrollArrows(
    DisplayMenu* menu, uint8_t top, uint8_t visible_rows, uint8_t total_rows);

#endif  // YAX86_PICOCALC_UI_BOX_H
