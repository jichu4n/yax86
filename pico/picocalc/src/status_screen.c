#include "status_screen.h"

#include <stdbool.h>
#include <stddef.h>

#include "box.h"
#include "display.h"
#include "list_box.h"
#include "pico/time.h"

enum {
  // A label's width, in columns, before its value.
  kLabelColumns = 9,
  // How often the page is drawn again while it shows.
  kRefreshUs = 1000 * 1000,
};

extern const Screen kStatusScreen;

// The lines to draw, the emulator's, which it keeps current.
static const StatusScreenLine* g_lines;
// The list box, for whether it is out of date. It has no items to select.
static ListBox g_list_box;
// When the page was last marked out of date, in microseconds since boot. It
// is marked again kRefreshUs later, so that its figures stay current.
static uint64_t g_refreshed_us;

ScreenResult StatusScreenOpen(
    const StatusScreenLine lines[kStatusScreenLines]) {
  g_lines = lines;
  g_list_box.selected = 0;
  g_refreshed_us = time_us_64();
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

// Mark the page out of date once a second, so that it is drawn again with
// the figures the emulator has filled in since.
static ScreenResult OnTick(void) {
  const uint64_t now_us = time_us_64();
  if (now_us - g_refreshed_us >= kRefreshUs) {
    g_refreshed_us = now_us;
    g_list_box.dirty = true;
  }
  return ScreenStays();
}

static void OnDraw(DisplayBox* box) {
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
}

static const char* Hint(void) { return "Esc=Back"; }

static const char* PageName(void) { return "status"; }

const Screen kStatusScreen = {
    .on_key = OnKey,
    .on_tick = OnTick,
    .on_draw = OnDraw,
    .hint = Hint,
    .page_name = PageName,
    .dirty = &g_list_box.dirty,
};
