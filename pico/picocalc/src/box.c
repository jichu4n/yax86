#include "box.h"

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

void BoxBlankRow(DisplayTextRow* row, uint8_t columns) {
  row->chars[0] = (char)kBoxVertical;
  row->attrs[0] = kBoxAttrBorder;
  for (uint8_t i = 1; i + 1 < columns; ++i) {
    row->chars[i] = ' ';
    row->attrs[i] = kBoxAttrText;
  }
  row->chars[columns - 1] = (char)kBoxVertical;
  row->attrs[columns - 1] = kBoxAttrBorder;
}

void BoxEdgeRow(
    DisplayTextRow* row, uint8_t columns, bool is_top, const char* title) {
  row->chars[0] = (char)(is_top ? kBoxTopLeft : kBoxBottomLeft);
  for (uint8_t i = 1; i + 1 < columns; ++i) {
    row->chars[i] = (char)kBoxHorizontal;
  }
  row->chars[columns - 1] = (char)(is_top ? kBoxTopRight : kBoxBottomRight);
  for (uint8_t i = 0; i < columns; ++i) {
    row->attrs[i] = kBoxAttrBorder;
  }
  if (title == NULL) {
    return;
  }
  const uint8_t length = (uint8_t)strlen(title);
  // The title, with a space either side, over the middle of the edge.
  uint8_t column = (uint8_t)((columns - length - 2) / 2);
  row->chars[column] = ' ';
  row->attrs[column++] = kBoxAttrTitle;
  for (uint8_t i = 0; i < length; ++i) {
    row->chars[column] = title[i];
    row->attrs[column++] = kBoxAttrTitle;
  }
  row->chars[column] = ' ';
  row->attrs[column] = kBoxAttrTitle;
}

void BoxSeparatorRow(DisplayTextRow* row, uint8_t columns) {
  row->chars[0] = (char)kBoxSeparatorLeft;
  for (uint8_t i = 1; i + 1 < columns; ++i) {
    row->chars[i] = (char)kBoxSeparator;
  }
  row->chars[columns - 1] = (char)kBoxSeparatorRight;
  for (uint8_t i = 0; i < columns; ++i) {
    row->attrs[i] = kBoxAttrBorder;
  }
}

void BoxPutText(
    DisplayTextRow* row, uint8_t columns, uint8_t column, const char* text,
    uint8_t attr) {
  while (*text != '\0' && column + 1 < columns) {
    row->chars[column] = *text++;
    row->attrs[column++] = attr;
  }
}

void BoxItemTextRow(
    DisplayTextRow* row, uint8_t columns, const char* text, bool is_selected) {
  BoxBlankRow(row, columns);
  if (is_selected) {
    for (uint8_t i = 1; i + 1 < columns; ++i) {
      row->attrs[i] = kBoxAttrSelected;
    }
  }
  BoxPutText(
      row, columns, 3, text, is_selected ? kBoxAttrSelected : kBoxAttrText);
}

void BoxItemRow(
    DisplayTextRow* row, uint8_t columns, const BoxItem* item,
    bool is_selected) {
  BoxItemTextRow(row, columns, item->text, is_selected);
  if (item->opens_box) {
    row->chars[columns - 3] = '>';
  }
}

void BoxPlace(DisplayBox* box, uint8_t columns, uint8_t rows) {
  box->columns = columns;
  box->rows = rows;
  box->column = (uint8_t)((kTextColumns - columns) / 2);
  box->row = (uint8_t)((kGuestScreenRows - rows) / 2);
}
