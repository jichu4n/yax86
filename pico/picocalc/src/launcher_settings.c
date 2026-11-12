#include "launcher_settings.h"

#include <stdbool.h>
#include <stdio.h>

#include "display.h"
#include "launcher.h"
#include "runtime_log.h"
#include "ui_box.h"
#include "ui_page.h"
#include "vm_config.h"

typedef enum SettingsPage {
  // Settings, from the list, and Font, from Settings: the 4x10 fonts.
  kSettingsPageSettings = 0,
  kSettingsPageFont,
} SettingsPage;

static const char* const kPageNames[] = {
    [kSettingsPageSettings] = "settings",
    [kSettingsPageFont] = "font",
};

static const UIBoxItem kSettingsItems[] = {{"Font", true}};
enum {
  kSettingsItemCount = sizeof(kSettingsItems) / sizeof(kSettingsItems[0]),
};

static SettingsPage g_page;
static uint8_t g_chosen;
// Whether what is on the panel is out of date.
static bool g_dirty;
// The font in use, which the Font page shows as chosen.
static Font4x10 g_font = kFont4x10Default;

static void ShowPage(SettingsPage page, uint8_t chosen) {
  g_page = page;
  g_chosen = chosen;
  g_dirty = true;
}

void LauncherSettingsOpen(Font4x10 font) {
  g_font = font;
  ShowPage(kSettingsPageSettings, 0);
}

Font4x10 LauncherSettingsFont(void) { return g_font; }

const char* LauncherSettingsPageName(void) { return kPageNames[g_page]; }

const char* LauncherSettingsHint(void) {
  return "\x18\x19=Move  Enter=Select  Esc=Back";
}

// Use the font chosen at once, and save it if it is another than before.
// Returns whether it is.
static __noinline bool ChooseFont(Font4x10 font) {
  DisplaySetFont(font);
  if (font == g_font) {
    return false;
  }
  g_font = font;
  char problem[kVMProblemLength + 1];
  if (VMSettingsSaveFont(font, problem)) {
    SYSTEM_INFO("font_saved", Y_LOG_STRING("font", Font4x10Name(font)));
    return true;
  }
  char message[kTextColumns + 1];
  snprintf(message, sizeof(message), "Could not save font: %.50s", problem);
  LauncherSetProblem(message);
  SYSTEM_WARN("font_not_saved", Y_LOG_STRING("problem", problem));
  return true;
}

// Go back a page, or to the list from the first.
static LauncherSettingsResult Back(void) {
  if (g_page == kSettingsPageSettings) {
    return kLauncherSettingsLeft;
  }
  ShowPage(kSettingsPageSettings, 0);
  return kLauncherSettingsStays;
}

static LauncherSettingsResult Choose(void) {
  if (g_page == kSettingsPageSettings) {
    ShowPage(kSettingsPageFont, g_font);
    return kLauncherSettingsStays;
  }
  const Font4x10 font = (Font4x10)g_chosen;
  ShowPage(kSettingsPageSettings, 0);
  return ChooseFont(font) ? kLauncherSettingsFontChanged
                          : kLauncherSettingsStays;
}

LauncherSettingsResult LauncherSettingsHandleKey(SouthbridgeKeyEvent event) {
  if (event.state != kSouthbridgeKeyReleased && LauncherClearProblem()) {
    g_dirty = true;
  }
  // A key held down repeats, which moves through the items as a press does.
  if (event.state == kSouthbridgeKeyReleased) {
    return kLauncherSettingsStays;
  }
  if (UIPageIsMoveKey(event.key)) {
    const uint8_t count =
        g_page == kSettingsPageSettings ? kSettingsItemCount : kFont4x10Count;
    if (UIPageMove(event.key, count, &g_chosen)) {
      g_dirty = true;
    }
    return kLauncherSettingsStays;
  }
  switch (event.key) {
    case kKeyCodeEnter:
    case kKeyCodeSpace:
      return Choose();
    case kKeyCodeBackspace:
    case kKeyCodeEsc:
      return Back();
    default:
      return kLauncherSettingsStays;
  }
}

static void DrawFontPage(DisplayMenu* menu) {
  UIBoxItem items[kFont4x10Count];
  for (uint8_t i = 0; i < kFont4x10Count; ++i) {
    items[i] = (UIBoxItem){Font4x10Label((Font4x10)i), false};
  }
  UIPageDrawItems(menu, "Font", NULL, 0, items, kFont4x10Count, g_chosen, true);
}

void LauncherSettingsDraw(void) {
  if (!g_dirty) {
    return;
  }
  DisplayMenu* menu = DisplayBeginMenu();
  if (menu == NULL) {
    return;
  }
  if (g_page == kSettingsPageSettings) {
    UIPageDrawItems(
        menu, "Settings", NULL, 0, kSettingsItems, kSettingsItemCount, g_chosen,
        true);
  } else {
    DrawFontPage(menu);
  }
  // Drawn the list's size, as every page from the list is, so the box never
  // changes size.
  DisplayEndMenu();
  g_dirty = false;
}
