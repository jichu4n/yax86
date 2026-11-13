#include "font_screen.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "display.h"
#include "font_4x10.h"
#include "pico/stdlib.h"
#include "runtime_log.h"
#include "status_bar.h"
#include "text.h"
#include "ui_box.h"
#include "ui_page.h"
#include "vm_config.h"

extern const Screen kFontScreen;

static Settings* g_settings;
// The item chosen, and whether the page is out of date.
static UIPage g_page;

ScreenResult FontScreenOpen(Settings* settings) {
  g_settings = settings;
  g_page.chosen = settings->font;
  g_page.dirty = true;
  return ScreenOpen(&kFontScreen);
}

// Use the font chosen, and save it if it changed. Kept out of line, so that
// its message is on core 0's stack only while it runs.
static __noinline void ChooseFont(void) {
  const Font4x10 font = (Font4x10)g_page.chosen;
  DisplaySetFont(font);
  if (font == g_settings->font) {
    return;
  }
  char problem[kVMProblemLength + 1];
  if (SettingsSaveFont(g_settings, font, problem)) {
    SYSTEM_INFO("font_saved", Y_LOG_STRING("font", Font4x10Name(font)));
    return;
  }
  char message[kTextColumns + 1];
  snprintf(message, sizeof(message), "Could not save font: %.50s", problem);
  StatusBarSetProblem(message);
  SYSTEM_WARN("font_not_saved", Y_LOG_STRING("problem", problem));
}

static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  switch (UIPageTakeEvent(&g_page, event, kFont4x10Count)) {
    case kUIPageKeyChoose:
      ChooseFont();
      return ScreenBack();
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
  UIBoxItem items[kFont4x10Count];
  for (uint8_t i = 0; i < kFont4x10Count; ++i) {
    items[i] = (UIBoxItem){Font4x10Label((Font4x10)i), false};
  }
  UIPageDrawItems(
      menu, "Font", NULL, 0, items, kFont4x10Count, g_page.chosen, true);
  UIPageEndDraw(&g_page);
}

// Nothing opens over Font.
static ScreenResult Resume(void) {
  g_page.dirty = true;
  return ScreenStays();
}

static const char* Hint(void) { return kUIPageHint; }

static const char* PageName(void) { return "font"; }

const Screen kFontScreen = {
    .handle_key = HandleKey,
    .draw = Draw,
    .resume = Resume,
    .hint = Hint,
    .page_name = PageName,
};
