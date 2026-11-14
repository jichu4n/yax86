#include "settings_screen.h"

#include <stdbool.h>
#include <stdint.h>

#include "box.h"
#include "display.h"
#include "font_screen.h"
#include "list_box.h"

static const BoxItem kSettingsItems[] = {{"Font", true}};
enum {
  kNumSettingsItems = sizeof(kSettingsItems) / sizeof(kSettingsItems[0]),
};

extern const Screen kSettingsScreen;

static Settings* g_settings;
// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;

ScreenResult SettingsScreenOpen(Settings* settings) {
  g_settings = settings;
  g_list_box.selected = 0;
  return ScreenOpen(&kSettingsScreen);
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxHandleKeyEvent(&g_list_box, event, kNumSettingsItems)) {
    case kListBoxActionSelect:
      return FontScreenOpen(g_settings);
    case kListBoxActionBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

static void OnDraw(DisplayBox* box) {
  ListBoxDrawItems(
      box, "Settings", NULL, 0, kSettingsItems, kNumSettingsItems,
      g_list_box.selected, true);
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return "settings"; }

const Screen kSettingsScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .hint = Hint,
    .page_name = PageName,
    .is_dirty = &g_list_box.is_dirty,
};
