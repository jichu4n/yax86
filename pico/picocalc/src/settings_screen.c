#include "settings_screen.h"

#include <stdbool.h>
#include <stdint.h>

#include "display.h"
#include "font_screen.h"
#include "ui_box.h"
#include "ui_page.h"

static const UIBoxItem kSettingsItems[] = {{"Font", true}};
enum {
  kSettingsItemCount = sizeof(kSettingsItems) / sizeof(kSettingsItems[0]),
};

extern const Screen kSettingsScreen;

static Settings* g_settings;
// The item chosen, and whether the page is out of date.
static UIPage g_page;

ScreenResult SettingsScreenOpen(Settings* settings) {
  g_settings = settings;
  g_page.chosen = 0;
  g_page.dirty = true;
  return ScreenOpen(&kSettingsScreen);
}

static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  switch (UIPageTakeEvent(&g_page, event, kSettingsItemCount)) {
    case kUIPageKeyChoose:
      return FontScreenOpen(g_settings);
    case kUIPageKeyBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

static void Draw(Y_UNUSED uint64_t now_us) {
  DisplayMenu* menu = UIPageBeginDraw(&g_page);
  if (menu == NULL) {
    return;
  }
  UIPageDrawItems(
      menu, "Settings", NULL, 0, kSettingsItems, kSettingsItemCount,
      g_page.chosen, true);
  UIPageEndDraw(&g_page);
}

static ScreenResult Resume(void) {
  g_page.dirty = true;
  return ScreenStays();
}

static const char* Hint(void) { return kUIPageHint; }

static const char* PageName(void) { return "settings"; }

const Screen kSettingsScreen = {
    .handle_key = HandleKey,
    .draw = Draw,
    .resume = Resume,
    .hint = Hint,
    .page_name = PageName,
};
