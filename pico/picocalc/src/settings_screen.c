#include "settings_screen.h"

#include <stdbool.h>
#include <stdint.h>

#include "box.h"
#include "display.h"
#include "font_screen.h"
#include "list_box.h"

static const BoxItem kSettingsItems[] = {{"Font", true}};
enum {
  kSettingsItemCount = sizeof(kSettingsItems) / sizeof(kSettingsItems[0]),
};

extern const Screen kSettingsScreen;

static Settings* g_settings;
// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;

ScreenResult SettingsScreenOpen(Settings* settings) {
  g_settings = settings;
  g_list_box.selected = 0;
  g_list_box.dirty = true;
  return ScreenOpen(&kSettingsScreen);
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxHandleKeyEvent(&g_list_box, event, kSettingsItemCount)) {
    case kListBoxActionSelect:
      return FontScreenOpen(g_settings);
    case kListBoxActionBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

static void OnDraw(Y_UNUSED uint64_t now_us) {
  DisplayMenu* menu = ListBoxBeginDraw(&g_list_box);
  if (menu == NULL) {
    return;
  }
  ListBoxDrawItems(
      menu, "Settings", NULL, 0, kSettingsItems, kSettingsItemCount,
      g_list_box.selected, true);
  ListBoxEndDraw(&g_list_box);
}

static ScreenResult OnResume(void) {
  g_list_box.dirty = true;
  return ScreenStays();
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return "settings"; }

const Screen kSettingsScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .on_resume = OnResume,
    .hint = Hint,
    .page_name = PageName,
};
