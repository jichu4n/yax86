#include "status_screen.h"

#include <stdbool.h>
#include <stddef.h>

#include "box.h"
#include "display.h"
#include "list_box.h"

enum {
  // A label's width, in columns, before its value.
  kLabelColumns = 9,
  // How often the page is drawn again while it shows.
  kRefreshUs = 1000 * 1000,
};

extern const Screen kStatusScreen;

static const StatusScreenLine* g_lines;
// The list box, for whether it is out of date. It has no items to select.
static ListBox g_list_box;
// When the page was last drawn.
static uint64_t g_drawn_us;

ScreenResult StatusScreenOpen(
    const StatusScreenLine lines[kStatusScreenLines]) {
  g_lines = lines;
  g_list_box.selected = 0;
  g_list_box.dirty = true;
  return ScreenOpen(&kStatusScreen);
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxHandleKeyEvent(&g_list_box, event, 0)) {
    case kListBoxActionSelect:
    case kListBoxActionBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

static bool OnDraw(DisplayBox* box, uint64_t now_us) {
  if (now_us - g_drawn_us >= kRefreshUs) {
    g_list_box.dirty = true;
  }
  if (!ListBoxTakeDirty(&g_list_box)) {
    return false;
  }
  BoxPlace(box, kListBoxWideColumns, kStatusScreenLines + 4);
  uint8_t row = 0;
  BoxEdgeRow(&box->text[row++], box->columns, true, "Status");
  BoxBlankRow(&box->text[row++], box->columns);
  for (uint8_t i = 0; i < kStatusScreenLines; ++i) {
    DisplayTextRow* text = &box->text[row++];
    BoxBlankRow(text, box->columns);
    BoxPutText(text, box->columns, 2, g_lines[i].label, kBoxAttrTitle);
    BoxPutText(
        text, box->columns, 2 + kLabelColumns, g_lines[i].value, kBoxAttrText);
  }
  BoxBlankRow(&box->text[row++], box->columns);
  BoxEdgeRow(&box->text[row], box->columns, false, NULL);
  g_drawn_us = now_us;
  return true;
}

// Nothing opens over the page.
static ScreenResult OnResume(void) {
  g_list_box.dirty = true;
  return ScreenStays();
}

static const char* Hint(void) { return "Esc=Back"; }

static const char* PageName(void) { return "status"; }

const Screen kStatusScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .on_resume = OnResume,
    .hint = Hint,
    .page_name = PageName,
};
