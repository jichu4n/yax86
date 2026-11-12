#include "ui_box.h"

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

void UIBoxBlankRow(DisplayTextRow* row, uint8_t columns) {
  row->chars[0] = (char)kBoxVertical;
  row->attrs[0] = kUIBoxAttrFrame;
  for (uint8_t i = 1; i + 1 < columns; ++i) {
    row->chars[i] = ' ';
    row->attrs[i] = kUIBoxAttrText;
  }
  row->chars[columns - 1] = (char)kBoxVertical;
  row->attrs[columns - 1] = kUIBoxAttrFrame;
}

void UIBoxEdgeRow(
    DisplayTextRow* row, uint8_t columns, bool top, const char* title) {
  row->chars[0] = (char)(top ? kBoxTopLeft : kBoxBottomLeft);
  for (uint8_t i = 1; i + 1 < columns; ++i) {
    row->chars[i] = (char)kBoxHorizontal;
  }
  row->chars[columns - 1] = (char)(top ? kBoxTopRight : kBoxBottomRight);
  for (uint8_t i = 0; i < columns; ++i) {
    row->attrs[i] = kUIBoxAttrFrame;
  }
  if (title == NULL) {
    return;
  }
  const uint8_t length = (uint8_t)strlen(title);
  // The title, with a space either side, over the middle of the edge.
  uint8_t column = (uint8_t)((columns - length - 2) / 2);
  row->chars[column] = ' ';
  row->attrs[column++] = kUIBoxAttrTitle;
  for (uint8_t i = 0; i < length; ++i) {
    row->chars[column] = title[i];
    row->attrs[column++] = kUIBoxAttrTitle;
  }
  row->chars[column] = ' ';
  row->attrs[column] = kUIBoxAttrTitle;
}

void UIBoxSeparatorRow(DisplayTextRow* row, uint8_t columns) {
  row->chars[0] = (char)kBoxSeparatorLeft;
  for (uint8_t i = 1; i + 1 < columns; ++i) {
    row->chars[i] = (char)kBoxSeparator;
  }
  row->chars[columns - 1] = (char)kBoxSeparatorRight;
  for (uint8_t i = 0; i < columns; ++i) {
    row->attrs[i] = kUIBoxAttrFrame;
  }
}

void UIBoxPutText(
    DisplayTextRow* row, uint8_t columns, uint8_t column, const char* text,
    uint8_t attr) {
  while (*text != '\0' && column + 1 < columns) {
    row->chars[column] = *text++;
    row->attrs[column++] = attr;
  }
}

void UIBoxItemRow(
    DisplayTextRow* row, uint8_t columns, const char* text, bool chosen) {
  UIBoxBlankRow(row, columns);
  if (chosen) {
    for (uint8_t i = 1; i + 1 < columns; ++i) {
      row->attrs[i] = kUIBoxAttrChosen;
    }
  }
  UIBoxPutText(
      row, columns, 3, text, chosen ? kUIBoxAttrChosen : kUIBoxAttrText);
}

void UIBoxMenuItemRow(
    DisplayTextRow* row, uint8_t columns, const UIBoxItem* item, bool chosen) {
  UIBoxItemRow(row, columns, item->text, chosen);
  if (item->opens_menu) {
    row->chars[columns - 3] = '>';
  }
}

void UIBoxPlace(DisplayMenu* menu, uint8_t columns, uint8_t rows) {
  menu->columns = columns;
  menu->rows = rows;
  menu->column = (uint8_t)((kTextColumns - columns) / 2);
  menu->row = (uint8_t)((kGuestScreenRows - rows) / 2);
}
