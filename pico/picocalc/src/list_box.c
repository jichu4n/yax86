#include "list_box.h"

#include <stddef.h>

#include "southbridge.h"
#include "status_bar.h"

bool ListBoxIsMoveKey(uint8_t key) {
  return key == kKeyCodeUp || key == kKeyCodeDown || key == kKeyCodePageUp ||
         key == kKeyCodePageDown;
}

bool ListBoxMove(uint8_t key, uint8_t count, uint8_t* selected) {
  if (count == 0) {
    return false;
  }
  switch (key) {
    case kKeyCodeUp:
      *selected = (uint8_t)((*selected + count - 1) % count);
      break;
    case kKeyCodeDown:
      *selected = (uint8_t)((*selected + 1) % count);
      break;
    case kKeyCodePageUp:
      *selected =
          *selected > kListBoxRows ? (uint8_t)(*selected - kListBoxRows) : 0;
      break;
    case kKeyCodePageDown:
      *selected = *selected + kListBoxRows < count
                      ? (uint8_t)(*selected + kListBoxRows)
                      : (uint8_t)(count - 1);
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

void ListBoxDrawScrollArrows(DisplayMenu* menu, uint8_t top, uint8_t total) {
  if (top > 0) {
    menu->text[1].chars[menu->columns - 1] = (char)kBoxArrowUp;
  }
  if (top + kListBoxRows < total) {
    menu->text[kListBoxRows].chars[menu->columns - 1] = (char)kBoxArrowDown;
  }
}

void ListBoxDrawItems(
    DisplayMenu* menu, const char* title, const char* const* lines,
    uint8_t line_count, const BoxItem* items, uint8_t count, uint8_t selected,
    bool list_sized) {
  // The box holds kDisplayMenuMaxRows: lines past what fits are left out,
  // rather than drawn past its rows.
  if (count + 4 + line_count + 1 > kDisplayMenuMaxRows) {
    line_count = count + 5 < kDisplayMenuMaxRows
                     ? (uint8_t)(kDisplayMenuMaxRows - count - 5)
                     : 0;
  }
  const uint8_t text_rows = line_count > 0 ? (uint8_t)(line_count + 1) : 0;
  // The edges, a blank row either side of the items, and the lines with a
  // blank row under them - or at power on, the list's size, as there is no
  // guest's screen to draw under a smaller box to hide the list's.
  uint8_t rows = (uint8_t)(count + 4 + text_rows);
  if (list_sized && rows < kListBoxRows + 2) {
    rows = kListBoxRows + 2;
  }
  BoxPlace(menu, kListBoxNarrowColumns, rows);
  uint8_t row = 0;
  BoxEdgeRow(&menu->text[row++], menu->columns, true, title);
  if (!list_sized || line_count > 0) {
    BoxBlankRow(&menu->text[row++], menu->columns);
  }
  for (uint8_t i = 0; i < line_count; ++i) {
    BoxBlankRow(&menu->text[row], menu->columns);
    BoxPutText(&menu->text[row++], menu->columns, 3, lines[i], kBoxAttrText);
  }
  if (line_count > 0) {
    BoxBlankRow(&menu->text[row++], menu->columns);
  }
  for (uint8_t i = 0; i < count; ++i) {
    BoxItemRow(&menu->text[row++], menu->columns, &items[i], i == selected);
  }
  while (row + 1 < rows) {
    BoxBlankRow(&menu->text[row++], menu->columns);
  }
  BoxEdgeRow(&menu->text[row], menu->columns, false, NULL);
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
    ListBox* list_box, uint8_t key, uint8_t count) {
  if (ListBoxIsMoveKey(key)) {
    if (ListBoxMove(key, count, &list_box->selected)) {
      list_box->dirty = true;
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
    ListBox* list_box, SouthbridgeKeyEvent event, uint8_t count) {
  if (!ListBoxIsKeyPress(event)) {
    return kListBoxActionNone;
  }
  return ListBoxHandleKeyCode(list_box, event.key, count);
}

DisplayMenu* ListBoxBeginDraw(ListBox* list_box) {
  if (!list_box->dirty) {
    return NULL;
  }
  return DisplayBeginMenu();
}

void ListBoxEndDraw(ListBox* list_box) {
  DisplayEndMenu();
  list_box->dirty = false;
}
