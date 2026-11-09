#include "menu.h"

#include <stddef.h>

#include "display.h"

enum {
  // The controller's codes for the keys the menu listens to.
  kKeyLeftShift = 0xA2,
  kKeyRightShift = 0xA3,
  kKeyUp = 0xB5,
  kKeyDown = 0xB6,
  kKeyEnter = 0x0A,
  kKeySpace = ' ',
  kKeyEsc = 0xB1,
  kKeyBackspace = 0x08,

  // A page's box: the main page and the question are narrow, centered on the
  // guest's screen, and the status page takes its whole width.
  kGuestScreenRows = 25,
  kNarrowColumns = 40,
  kWideColumns = kTextColumns,
  // A label's column on the status page.
  kStatusLabelColumns = 9,

  // CGA attributes: white on blue for the box, and black on light gray for
  // the item chosen.
  kAttrFrame = 0x1F,
  kAttrText = 0x17,
  kAttrChosen = 0x70,
  kAttrTitle = 0x1E,

  // Code page 437's double lined box.
  kBoxTopLeft = 0xC9,
  kBoxTopRight = 0xBB,
  kBoxBottomLeft = 0xC8,
  kBoxBottomRight = 0xBC,
  kBoxHorizontal = 0xCD,
  kBoxVertical = 0xBA,

  kStatusRefreshUs = 1000 * 1000,
};

typedef enum MenuPage {
  kMenuPageMain = 0,
  kMenuPageStatus,
  kMenuPageReset,
} MenuPage;

static const char* const kPageNames[] = {
    [kMenuPageMain] = "main",
    [kMenuPageStatus] = "status",
    [kMenuPageReset] = "reset",
};

typedef enum MainItem {
  kMainResume = 0,
  kMainStatus,
  kMainReset,
  kMainItemCount,
} MainItem;

static const char* const kMainItems[kMainItemCount] = {
    [kMainResume] = "Resume",
    [kMainStatus] = "Status",
    [kMainReset] = "Reset the machine",
};

typedef enum ResetItem {
  kResetCancel = 0,
  kResetConfirm,
  kResetItemCount,
} ResetItem;

static const char* const kResetItems[kResetItemCount] = {
    [kResetCancel] = "Cancel",
    [kResetConfirm] = "Reset",
};

static MenuConfig g_config;
static bool g_open;
static MenuPage g_page;
static uint8_t g_chosen;
// Whether what is on the panel is out of date, and when the status page was
// last drawn.
static bool g_dirty;
static uint64_t g_status_drawn_us;
// The box last drawn, whose size a new one has to match for nothing of it to
// be left showing, and whether there is one.
static uint8_t g_drawn_columns;
static uint8_t g_drawn_rows;
static bool g_box_drawn;
// Which Shift keys are held, from the controller's events.
static bool g_left_shift_held;
static bool g_right_shift_held;

void MenuInit(const MenuConfig* config) { g_config = *config; }

bool MenuIsOpen(void) { return g_open; }

const char* MenuPageName(void) { return kPageNames[g_page]; }

const char* MenuHint(void) {
  if (g_page == kMenuPageStatus) {
    return "Esc back";
  }
  return "\x18\x19 move   Enter choose   Esc back";
}

static void ShowPage(MenuPage page, uint8_t chosen) {
  g_page = page;
  g_chosen = chosen;
  g_dirty = true;
}

static uint8_t PageItemCount(void) {
  switch (g_page) {
    case kMenuPageMain:
      return kMainItemCount;
    case kMenuPageReset:
      return kResetItemCount;
    case kMenuPageStatus:
      return 0;
  }
  return 0;
}

// Go back a page, or close the menu from the main one.
static MenuAction Back(void) {
  if (g_page == kMenuPageMain) {
    g_open = false;
    return kMenuActionClosed;
  }
  ShowPage(kMenuPageMain, g_page == kMenuPageReset ? kMainReset : kMainStatus);
  return kMenuActionNone;
}

static MenuAction Choose(void) {
  switch (g_page) {
    case kMenuPageMain:
      switch ((MainItem)g_chosen) {
        case kMainResume:
          g_open = false;
          return kMenuActionClosed;
        case kMainStatus:
          ShowPage(kMenuPageStatus, 0);
          return kMenuActionNone;
        case kMainReset:
          ShowPage(kMenuPageReset, kResetCancel);
          return kMenuActionNone;
        case kMainItemCount:
          break;
      }
      break;
    case kMenuPageReset:
      if (g_chosen == kResetConfirm) {
        g_config.reset();
      }
      return Back();
    case kMenuPageStatus:
      return Back();
  }
  return kMenuActionNone;
}

// Track the Shift keys, and return whether this event completes the hotkey:
// both held, the second just pressed.
static bool TrackHotkey(SouthbridgeKeyEvent event) {
  bool* held = NULL;
  if (event.key == kKeyLeftShift) {
    held = &g_left_shift_held;
  } else if (event.key == kKeyRightShift) {
    held = &g_right_shift_held;
  } else {
    return false;
  }
  const bool was_held = *held;
  *held = event.state != kSouthbridgeKeyReleased;
  return *held && !was_held && g_left_shift_held && g_right_shift_held;
}

MenuAction MenuHandleKey(SouthbridgeKeyEvent event) {
  if (TrackHotkey(event)) {
    g_open = !g_open;
    if (g_open) {
      g_box_drawn = false;
      ShowPage(kMenuPageMain, kMainResume);
      return kMenuActionOpened;
    }
    return kMenuActionClosed;
  }
  // A key held down repeats, which moves through the items as a press does.
  if (!g_open || event.state == kSouthbridgeKeyReleased) {
    return kMenuActionNone;
  }
  const uint8_t count = PageItemCount();
  switch (event.key) {
    case kKeyUp:
      if (count > 0) {
        g_chosen = (uint8_t)((g_chosen + count - 1) % count);
        g_dirty = true;
      }
      return kMenuActionNone;
    case kKeyDown:
      if (count > 0) {
        g_chosen = (uint8_t)((g_chosen + 1) % count);
        g_dirty = true;
      }
      return kMenuActionNone;
    case kKeyEnter:
    case kKeySpace:
      return Choose();
    case kKeyEsc:
    case kKeyBackspace:
      return Back();
    default:
      return kMenuActionNone;
  }
}

// ============================================================================
// Drawing
// ============================================================================

// Fill a row of the box: its border, and its inside blank.
static void BlankRow(DisplayTextRow* row, uint8_t columns) {
  row->chars[0] = (char)kBoxVertical;
  row->attrs[0] = kAttrFrame;
  for (uint8_t i = 1; i + 1 < columns; ++i) {
    row->chars[i] = ' ';
    row->attrs[i] = kAttrText;
  }
  row->chars[columns - 1] = (char)kBoxVertical;
  row->attrs[columns - 1] = kAttrFrame;
}

// The top or bottom edge of the box, with a title centered in the top one.
static void EdgeRow(
    DisplayTextRow* row, uint8_t columns, bool top, const char* title) {
  row->chars[0] = (char)(top ? kBoxTopLeft : kBoxBottomLeft);
  for (uint8_t i = 1; i + 1 < columns; ++i) {
    row->chars[i] = (char)kBoxHorizontal;
  }
  row->chars[columns - 1] = (char)(top ? kBoxTopRight : kBoxBottomRight);
  for (uint8_t i = 0; i < columns; ++i) {
    row->attrs[i] = kAttrFrame;
  }
  if (title == NULL) {
    return;
  }
  uint8_t length = 0;
  while (title[length] != '\0') {
    ++length;
  }
  // The title, with a space either side, over the middle of the edge.
  uint8_t column = (uint8_t)((columns - length - 2) / 2);
  row->chars[column] = ' ';
  row->attrs[column++] = kAttrTitle;
  for (uint8_t i = 0; i < length; ++i) {
    row->chars[column] = title[i];
    row->attrs[column++] = kAttrTitle;
  }
  row->chars[column] = ' ';
  row->attrs[column] = kAttrTitle;
}

// Put text inside a row of the box from column, clipped at its border.
static void PutText(
    DisplayTextRow* row, uint8_t columns, uint8_t column, const char* text,
    uint8_t attr) {
  while (*text != '\0' && column + 1 < columns) {
    row->chars[column] = *text++;
    row->attrs[column++] = attr;
  }
}

// An item's row: its text, and the whole width inside the border marked if
// it is the item chosen.
static void ItemRow(
    DisplayTextRow* row, uint8_t columns, const char* text, bool chosen) {
  BlankRow(row, columns);
  if (chosen) {
    for (uint8_t i = 1; i + 1 < columns; ++i) {
      row->attrs[i] = kAttrChosen;
    }
  }
  PutText(row, columns, 3, text, chosen ? kAttrChosen : kAttrText);
}

// Lay out a box of rows, centered on the guest's screen.
static void PlaceBox(DisplayMenu* menu, uint8_t columns, uint8_t rows) {
  menu->columns = columns;
  menu->rows = rows;
  menu->column = (uint8_t)((kTextColumns - columns) / 2);
  menu->row = (uint8_t)((kGuestScreenRows - rows) / 2);
}

static void DrawItems(
    DisplayMenu* menu, const char* title, const char* question,
    const char* const* items, uint8_t count) {
  const uint8_t question_rows = question != NULL ? 2 : 0;
  // The edges, a blank row either side of the items, and the question.
  PlaceBox(menu, kNarrowColumns, (uint8_t)(count + 4 + question_rows));
  uint8_t row = 0;
  EdgeRow(&menu->text[row++], menu->columns, true, title);
  BlankRow(&menu->text[row++], menu->columns);
  if (question != NULL) {
    BlankRow(&menu->text[row], menu->columns);
    PutText(&menu->text[row++], menu->columns, 3, question, kAttrText);
    BlankRow(&menu->text[row++], menu->columns);
  }
  for (uint8_t i = 0; i < count; ++i) {
    ItemRow(&menu->text[row++], menu->columns, items[i], i == g_chosen);
  }
  BlankRow(&menu->text[row++], menu->columns);
  EdgeRow(&menu->text[row], menu->columns, false, NULL);
}

static void DrawStatusPage(DisplayMenu* menu) {
  MenuStatusLine lines[kMenuStatusLines];
  g_config.describe_status(lines);
  PlaceBox(menu, kWideColumns, kMenuStatusLines + 4);
  uint8_t row = 0;
  EdgeRow(&menu->text[row++], menu->columns, true, "Status");
  BlankRow(&menu->text[row++], menu->columns);
  for (uint8_t i = 0; i < kMenuStatusLines; ++i) {
    DisplayTextRow* text = &menu->text[row++];
    BlankRow(text, menu->columns);
    PutText(text, menu->columns, 2, lines[i].label, kAttrTitle);
    PutText(
        text, menu->columns, 2 + kStatusLabelColumns, lines[i].value,
        kAttrText);
  }
  BlankRow(&menu->text[row++], menu->columns);
  EdgeRow(&menu->text[row], menu->columns, false, NULL);
}

void MenuDraw(uint64_t now_us) {
  if (!g_open) {
    return;
  }
  if (g_page == kMenuPageStatus &&
      now_us - g_status_drawn_us >= kStatusRefreshUs) {
    g_dirty = true;
  }
  if (!g_dirty) {
    return;
  }
  DisplayMenu* menu = DisplayBeginMenu();
  if (menu == NULL) {
    return;
  }
  switch (g_page) {
    case kMenuPageMain:
      DrawItems(menu, "yax86", NULL, kMainItems, kMainItemCount);
      break;
    case kMenuPageReset:
      DrawItems(
          menu, "Reset", "Restart the machine from power on?", kResetItems,
          kResetItemCount);
      break;
    case kMenuPageStatus:
      DrawStatusPage(menu);
      g_status_drawn_us = now_us;
      break;
  }
  // A box of another size than the last leaves part of the last showing, so
  // the guest's screen goes under it first. If core 1 cannot take that yet,
  // the box is laid out again next time.
  if (g_box_drawn &&
      (menu->columns != g_drawn_columns || menu->rows != g_drawn_rows) &&
      !g_config.redraw_guest()) {
    return;
  }
  DisplayEndMenu();
  g_drawn_columns = menu->columns;
  g_drawn_rows = menu->rows;
  g_box_drawn = true;
  g_dirty = false;
}
