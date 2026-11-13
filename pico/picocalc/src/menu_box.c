#include "menu_box.h"

#include <stddef.h>
#include <string.h>

enum {
  // Code page 437's double lined box, and the single line that separates
  // sections inside it.
  kBoxTopLeft = 0xC9,
  kBoxTopRight = 0xBB,
  kBoxBottomLeft = 0xC8,
  kBoxBottomRight = 0xBC,
  kBoxHorizontal = 0xCD,
  kBoxVertical = 0xBA,
  kBoxSeparatorLeft = 0xC7,
  kBoxSeparatorRight = 0xB6,
  kBoxSeparator = 0xC4,
  // The guest's screen's rows, which a box is centered on.
  kGuestScreenRows = 25,
};

void MenuBoxBlankRow(DisplayTextRow* row, uint8_t columns) {
  row->chars[0] = (char)kBoxVertical;
  row->attrs[0] = kMenuBoxAttrFrame;
  for (uint8_t i = 1; i + 1 < columns; ++i) {
    row->chars[i] = ' ';
    row->attrs[i] = kMenuBoxAttrText;
  }
  row->chars[columns - 1] = (char)kBoxVertical;
  row->attrs[columns - 1] = kMenuBoxAttrFrame;
}

void MenuBoxEdgeRow(
    DisplayTextRow* row, uint8_t columns, bool top, const char* title) {
  row->chars[0] = (char)(top ? kBoxTopLeft : kBoxBottomLeft);
  for (uint8_t i = 1; i + 1 < columns; ++i) {
    row->chars[i] = (char)kBoxHorizontal;
  }
  row->chars[columns - 1] = (char)(top ? kBoxTopRight : kBoxBottomRight);
  for (uint8_t i = 0; i < columns; ++i) {
    row->attrs[i] = kMenuBoxAttrFrame;
  }
  if (title == NULL) {
    return;
  }
  const uint8_t length = (uint8_t)strlen(title);
  // The title, with a space either side, over the middle of the edge.
  uint8_t column = (uint8_t)((columns - length - 2) / 2);
  row->chars[column] = ' ';
  row->attrs[column++] = kMenuBoxAttrTitle;
  for (uint8_t i = 0; i < length; ++i) {
    row->chars[column] = title[i];
    row->attrs[column++] = kMenuBoxAttrTitle;
  }
  row->chars[column] = ' ';
  row->attrs[column] = kMenuBoxAttrTitle;
}

void MenuBoxSeparatorRow(DisplayTextRow* row, uint8_t columns) {
  row->chars[0] = (char)kBoxSeparatorLeft;
  for (uint8_t i = 1; i + 1 < columns; ++i) {
    row->chars[i] = (char)kBoxSeparator;
  }
  row->chars[columns - 1] = (char)kBoxSeparatorRight;
  for (uint8_t i = 0; i < columns; ++i) {
    row->attrs[i] = kMenuBoxAttrFrame;
  }
}

void MenuBoxPutText(
    DisplayTextRow* row, uint8_t columns, uint8_t column, const char* text,
    uint8_t attr) {
  while (*text != '\0' && column + 1 < columns) {
    row->chars[column] = *text++;
    row->attrs[column++] = attr;
  }
}

void MenuBoxItemTextRow(
    DisplayTextRow* row, uint8_t columns, const char* text, bool selected) {
  MenuBoxBlankRow(row, columns);
  if (selected) {
    for (uint8_t i = 1; i + 1 < columns; ++i) {
      row->attrs[i] = kMenuBoxAttrSelected;
    }
  }
  MenuBoxPutText(
      row, columns, 3, text,
      selected ? kMenuBoxAttrSelected : kMenuBoxAttrText);
}

void MenuBoxItemRow(
    DisplayTextRow* row, uint8_t columns, const MenuBoxItem* item,
    bool selected) {
  MenuBoxItemTextRow(row, columns, item->text, selected);
  if (item->opens_menu) {
    row->chars[columns - 3] = '>';
  }
}

void MenuBoxPlace(DisplayMenu* menu, uint8_t columns, uint8_t rows) {
  menu->columns = columns;
  menu->rows = rows;
  menu->column = (uint8_t)((kTextColumns - columns) / 2);
  menu->row = (uint8_t)((kGuestScreenRows - rows) / 2);
}
