#include "settings_screen.h"

#include <stdbool.h>
#include <stdint.h>

#include "display.h"
#include "font_screen.h"
#include "ui_box.h"
#include "ui_page.h"
#include "ui_status.h"

static const UIBoxItem kSettingsItems[] = {{"Font", true}};
enum {
  kSettingsItemCount = sizeof(kSettingsItems) / sizeof(kSettingsItems[0]),
};

extern const Screen kSettingsScreen;

static Settings* g_settings;
static uint8_t g_chosen;
// Whether what is on the panel is out of date.
static bool g_dirty;

ScreenResult SettingsScreenOpen(Settings* settings) {
  g_settings = settings;
  g_chosen = 0;
  g_dirty = true;
  return ScreenOpen(&kSettingsScreen);
}

static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  // A key held down repeats, which moves through the items as a press does.
  if (event.state == kSouthbridgeKeyReleased) {
    return ScreenStays();
  }
  UIStatusClearProblem();
  if (UIPageIsMoveKey(event.key)) {
    if (UIPageMove(event.key, kSettingsItemCount, &g_chosen)) {
      g_dirty = true;
    }
    return ScreenStays();
  }
  switch (event.key) {
    case kKeyCodeEnter:
    case kKeyCodeSpace:
      return FontScreenOpen(g_settings);
    case kKeyCodeBackspace:
    case kKeyCodeEsc:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

static void Draw(Y_UNUSED uint64_t now_us) {
  if (!g_dirty) {
    return;
  }
  DisplayMenu* menu = DisplayBeginMenu();
  if (menu == NULL) {
    return;
  }
  UIPageDrawItems(
      menu, "Settings", NULL, 0, kSettingsItems, kSettingsItemCount, g_chosen,
      true);
  DisplayEndMenu();
  g_dirty = false;
}

static void Resume(void) { g_dirty = true; }

static const char* Hint(void) {
  return "\x18\x19=Move  Enter=Select  Esc=Back";
}

static const char* PageName(void) { return "settings"; }

const Screen kSettingsScreen = {
    .handle_key = HandleKey,
    .draw = Draw,
    .resume = Resume,
    .hint = Hint,
    .page_name = PageName,
};
