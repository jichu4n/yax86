#include "settings_screen.h"

#include <stdbool.h>
#include <stdint.h>

#include "box.h"
#include "display.h"
#include "display_timeout_screen.h"
#include "font_screen.h"
#include "list_box.h"

typedef enum SettingsItem {
  kSettingsItemFont = 0,
  kSettingsItemDisplayTimeout,
  kNumSettingsItems,
} SettingsItem;

static const BoxItem kSettingsItems[kNumSettingsItems] = {
    [kSettingsItemFont] = {"Font", true},
    [kSettingsItemDisplayTimeout] = {"Display timeout", true},
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
      if (g_list_box.selected == kSettingsItemDisplayTimeout) {
        return DisplayTimeoutScreenOpen(g_settings);
      }
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
