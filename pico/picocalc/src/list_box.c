#include "list_box.h"

#include <stddef.h>

#include "southbridge.h"
#include "status_bar.h"

bool ListBoxIsMoveKey(uint8_t key) {
  return key == kKeyCodeUp || key == kKeyCodeDown || key == kKeyCodePageUp ||
         key == kKeyCodePageDown;
}

bool ListBoxMove(uint8_t key, uint8_t num_items, uint8_t* selected) {
  if (num_items == 0) {
    return false;
  }
  switch (key) {
    case kKeyCodeUp:
      *selected = (uint8_t)((*selected + num_items - 1) % num_items);
      break;
    case kKeyCodeDown:
      *selected = (uint8_t)((*selected + 1) % num_items);
      break;
    case kKeyCodePageUp:
      *selected =
          *selected > kListBoxRows ? (uint8_t)(*selected - kListBoxRows) : 0;
      break;
    case kKeyCodePageDown:
      *selected = *selected + kListBoxRows < num_items
                      ? (uint8_t)(*selected + kListBoxRows)
                      : (uint8_t)(num_items - 1);
      break;
    default:
      break;
  }
  return true;
}

void ListBoxScrollTo(uint8_t row, uint8_t* top) {
  if (row < *top) {
    *top = row;
  } else if (row >= *top + kListBoxRows) {
    *top = (uint8_t)(row - kListBoxRows + 1);
  }
}

void ListBoxDrawScrollArrows(DisplayBox* box, uint8_t top, uint8_t num_rows) {
  if (top > 0) {
    box->text[1].chars[box->columns - 1] = (char)kBoxArrowUp;
  }
  if (top + kListBoxRows < num_rows) {
    box->text[kListBoxRows].chars[box->columns - 1] = (char)kBoxArrowDown;
  }
}

void ListBoxDrawItems(
    DisplayBox* box, const char* title, const char* const* lines,
    uint8_t num_lines, const BoxItem* items, uint8_t num_items,
    uint8_t selected, bool is_list_sized) {
  // The box holds kDisplayBoxMaxRows: lines past what fits are left out,
  // rather than drawn past its rows.
  if (num_items + 4 + num_lines + 1 > kDisplayBoxMaxRows) {
    num_lines = num_items + 5 < kDisplayBoxMaxRows
                    ? (uint8_t)(kDisplayBoxMaxRows - num_items - 5)
                    : 0;
  }
  const uint8_t text_rows = num_lines > 0 ? (uint8_t)(num_lines + 1) : 0;
  // The edges, a blank row either side of the items, and the lines with a
  // blank row under them - or at power on, the list's size, as there is no
  // guest's screen to draw under a smaller box to hide the list's.
  uint8_t rows = (uint8_t)(num_items + 4 + text_rows);
  if (is_list_sized && rows < kListBoxRows + 2) {
    rows = kListBoxRows + 2;
  }
  BoxPlace(box, kListBoxNarrowColumns, rows);
  uint8_t row = 0;
  BoxEdgeRow(&box->text[row++], box->columns, true, title);
  if (!is_list_sized || num_lines > 0) {
    BoxBlankRow(&box->text[row++], box->columns);
  }
  for (uint8_t i = 0; i < num_lines; ++i) {
    BoxBlankRow(&box->text[row], box->columns);
    BoxPutText(&box->text[row++], box->columns, 3, lines[i], kBoxAttrText);
  }
  if (num_lines > 0) {
    BoxBlankRow(&box->text[row++], box->columns);
  }
  for (uint8_t i = 0; i < num_items; ++i) {
    BoxItemRow(&box->text[row++], box->columns, &items[i], i == selected);
  }
  while (row + 1 < rows) {
    BoxBlankRow(&box->text[row++], box->columns);
  }
  BoxEdgeRow(&box->text[row], box->columns, false, NULL);
}

const char kListBoxHint[] = "\x18\x19=Move  Enter=Select  Esc=Back";

bool ListBoxIsKeyPress(SouthbridgeKeyEvent event) {
  // A key held down repeats, which moves through the items as a press does.
  if (event.state == kSouthbridgeKeyReleased) {
    return false;
  }
  StatusBarClearProblem();
  return true;
}

ListBoxAction ListBoxHandleKeyCode(
    ListBox* list_box, uint8_t key, uint8_t num_items) {
  if (ListBoxIsMoveKey(key)) {
    if (ListBoxMove(key, num_items, &list_box->selected)) {
      list_box->is_dirty = true;
    }
    return kListBoxActionMoved;
  }
  switch (key) {
    case kKeyCodeEnter:
    case kKeyCodeSpace:
      return kListBoxActionSelect;
    case kKeyCodeBackspace:
    case kKeyCodeEsc:
      return kListBoxActionBack;
    default:
      return kListBoxActionNone;
  }
}

ListBoxAction ListBoxHandleKeyEvent(
    ListBox* list_box, SouthbridgeKeyEvent event, uint8_t num_items) {
  if (!ListBoxIsKeyPress(event)) {
    return kListBoxActionNone;
  }
  return ListBoxHandleKeyCode(list_box, event.key, num_items);
}
