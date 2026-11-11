#include "ui_box.h"

#include <stddef.h>
#include <string.h>

void UIBoxBlankRow(DisplayTextRow* row, uint8_t columns) {
  row->chars[0] = (char)kUIBoxVertical;
  row->attrs[0] = kUIBoxAttrFrame;
  for (uint8_t i = 1; i + 1 < columns; ++i) {
    row->chars[i] = ' ';
    row->attrs[i] = kUIBoxAttrText;
  }
  row->chars[columns - 1] = (char)kUIBoxVertical;
  row->attrs[columns - 1] = kUIBoxAttrFrame;
}

void UIBoxEdgeRow(
    DisplayTextRow* row, uint8_t columns, bool top, const char* title) {
  row->chars[0] = (char)(top ? kUIBoxTopLeft : kUIBoxBottomLeft);
  for (uint8_t i = 1; i + 1 < columns; ++i) {
    row->chars[i] = (char)kUIBoxHorizontal;
  }
  row->chars[columns - 1] = (char)(top ? kUIBoxTopRight : kUIBoxBottomRight);
  for (uint8_t i = 0; i < columns; ++i) {
    row->attrs[i] = kUIBoxAttrFrame;
  }
  if (title == NULL) {
    return;
  }
  const uint8_t length = (uint8_t)strlen(title);
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
  row->chars[0] = (char)kUIBoxSeparatorLeft;
  for (uint8_t i = 1; i + 1 < columns; ++i) {
    row->chars[i] = (char)kUIBoxSeparator;
  }
  row->chars[columns - 1] = (char)kUIBoxSeparatorRight;
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
  UIBoxPutText(row, columns, 3, text, chosen ? kUIBoxAttrChosen : kUIBoxAttrText);
}

void UIBoxMenuItemRow(
    DisplayTextRow* row, uint8_t columns, const MenuItem* item, bool chosen) {
  UIBoxItemRow(row, columns, item->text, chosen);
  if (item->opens_menu) {
    row->chars[columns - 3] = '>';
  }
}

void UIBoxPlace(DisplayMenu* menu, uint8_t columns, uint8_t rows) {
  menu->columns = columns;
  menu->rows = rows;
  menu->column = (uint8_t)((kTextColumns - columns) / 2);
  menu->row = (uint8_t)((kUIBoxGuestScreenRows - rows) / 2);
}

void UIBoxDrawItems(
    DisplayMenu* menu, const char* title, const char* const* lines,
    uint8_t line_count, const MenuItem* items, uint8_t count, uint8_t chosen,
    bool choosing) {
  if (count + 4 + line_count + 1 > kDisplayMenuMaxRows) {
    line_count = count + 5 < kDisplayMenuMaxRows
                     ? (uint8_t)(kDisplayMenuMaxRows - count - 5)
                     : 0;
  }
  const uint8_t text_rows = line_count > 0 ? (uint8_t)(line_count + 1) : 0;
  uint8_t rows = (uint8_t)(count + 4 + text_rows);
  if (choosing && rows < kDisplayMenuMaxRows) {
    rows = kDisplayMenuMaxRows;
  }
  UIBoxPlace(menu, kUIBoxNarrowColumns, rows);
  uint8_t row = 0;
  UIBoxEdgeRow(&menu->text[row++], menu->columns, true, title);
  if (!choosing || line_count > 0) {
    UIBoxBlankRow(&menu->text[row++], menu->columns);
  }
  for (uint8_t i = 0; i < line_count; ++i) {
    UIBoxBlankRow(&menu->text[row], menu->columns);
    UIBoxPutText(
        &menu->text[row++], menu->columns, 3, lines[i], kUIBoxAttrText);
  }
  if (line_count > 0) {
    UIBoxBlankRow(&menu->text[row++], menu->columns);
  }
  for (uint8_t i = 0; i < count; ++i) {
    UIBoxMenuItemRow(
        &menu->text[row++], menu->columns, &items[i], i == chosen);
  }
  while (row + 1 < rows) {
    UIBoxBlankRow(&menu->text[row++], menu->columns);
  }
  UIBoxEdgeRow(&menu->text[row], menu->columns, false, NULL);
}

void UIBoxScrollTo(uint8_t row, uint8_t* top, uint8_t visible_rows) {
  if (row < *top) {
    *top = row;
  } else if (row >= *top + visible_rows) {
    *top = (uint8_t)(row - visible_rows + 1);
  }
}

void UIBoxDrawScrollArrows(
    DisplayMenu* menu, uint8_t top, uint8_t visible_rows, uint8_t total_rows) {
  if (top > 0) {
    menu->text[1].chars[menu->columns - 1] = (char)kUIBoxArrowUp;
  }
  if (top + visible_rows < total_rows) {
    menu->text[visible_rows].chars[menu->columns - 1] = (char)kUIBoxArrowDown;
  }
}
