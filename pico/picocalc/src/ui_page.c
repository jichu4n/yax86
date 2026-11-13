#include "ui_page.h"

#include <stddef.h>

#include "southbridge.h"
#include "status_bar.h"

bool UIPageIsMoveKey(uint8_t key) {
  return key == kKeyCodeUp || key == kKeyCodeDown || key == kKeyCodePageUp ||
         key == kKeyCodePageDown;
}

bool UIPageMove(uint8_t key, uint8_t count, uint8_t* chosen) {
  if (count == 0) {
    return false;
  }
  switch (key) {
    case kKeyCodeUp:
      *chosen = (uint8_t)((*chosen + count - 1) % count);
      break;
    case kKeyCodeDown:
      *chosen = (uint8_t)((*chosen + 1) % count);
      break;
    case kKeyCodePageUp:
      *chosen =
          *chosen > kUIPageListRows ? (uint8_t)(*chosen - kUIPageListRows) : 0;
      break;
    case kKeyCodePageDown:
      *chosen = *chosen + kUIPageListRows < count
                    ? (uint8_t)(*chosen + kUIPageListRows)
                    : (uint8_t)(count - 1);
      break;
    default:
      break;
  }
  return true;
}

void UIPageScrollTo(uint8_t row, uint8_t* top) {
  if (row < *top) {
    *top = row;
  } else if (row >= *top + kUIPageListRows) {
    *top = (uint8_t)(row - kUIPageListRows + 1);
  }
}

void UIPageDrawScrollArrows(DisplayMenu* menu, uint8_t top, uint8_t total) {
  if (top > 0) {
    menu->text[1].chars[menu->columns - 1] = (char)kUIBoxArrowUp;
  }
  if (top + kUIPageListRows < total) {
    menu->text[kUIPageListRows].chars[menu->columns - 1] =
        (char)kUIBoxArrowDown;
  }
}

void UIPageDrawItems(
    DisplayMenu* menu, const char* title, const char* const* lines,
    uint8_t line_count, const UIBoxItem* items, uint8_t count, uint8_t chosen,
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
  if (list_sized && rows < kUIPageListRows + 2) {
    rows = kUIPageListRows + 2;
  }
  UIBoxPlace(menu, kUIPageNarrowColumns, rows);
  uint8_t row = 0;
  UIBoxEdgeRow(&menu->text[row++], menu->columns, true, title);
  if (!list_sized || line_count > 0) {
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
    UIBoxMenuItemRow(&menu->text[row++], menu->columns, &items[i], i == chosen);
  }
  while (row + 1 < rows) {
    UIBoxBlankRow(&menu->text[row++], menu->columns);
  }
  UIBoxEdgeRow(&menu->text[row], menu->columns, false, NULL);
}

bool UIPageBoxResized(const UIPageBox* box, const DisplayMenu* menu) {
  return box->drawn &&
         (menu->columns != box->columns || menu->rows != box->rows);
}

void UIPageBoxDrawn(UIPageBox* box, const DisplayMenu* menu) {
  box->columns = menu->columns;
  box->rows = menu->rows;
  box->drawn = true;
}

const char kUIPageHint[] = "\x18\x19=Move  Enter=Select  Esc=Back";

bool UIPageKeyPressed(SouthbridgeKeyEvent event) {
  // A key held down repeats, which moves through the items as a press does.
  if (event.state == kSouthbridgeKeyReleased) {
    return false;
  }
  StatusBarClearProblem();
  return true;
}

UIPageKey UIPageTakeKey(UIPage* page, uint8_t key, uint8_t count) {
  if (UIPageIsMoveKey(key)) {
    if (UIPageMove(key, count, &page->chosen)) {
      page->dirty = true;
    }
    return kUIPageKeyMoved;
  }
  switch (key) {
    case kKeyCodeEnter:
    case kKeyCodeSpace:
      return kUIPageKeyChoose;
    case kKeyCodeBackspace:
    case kKeyCodeEsc:
      return kUIPageKeyBack;
    default:
      return kUIPageKeyNone;
  }
}

UIPageKey UIPageTakeEvent(
    UIPage* page, SouthbridgeKeyEvent event, uint8_t count) {
  if (!UIPageKeyPressed(event)) {
    return kUIPageKeyNone;
  }
  return UIPageTakeKey(page, event.key, count);
}

DisplayMenu* UIPageBeginDraw(UIPage* page) {
  if (!page->dirty) {
    return NULL;
  }
  return DisplayBeginMenu();
}

void UIPageEndDraw(UIPage* page) {
  DisplayEndMenu();
  page->dirty = false;
}
