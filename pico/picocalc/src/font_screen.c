#include "font_screen.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "display.h"
#include "font_4x10.h"
#include "pico/stdlib.h"
#include "runtime_log.h"
#include "text.h"
#include "ui_box.h"
#include "ui_page.h"
#include "ui_status.h"
#include "vm_config.h"

extern const Screen kFontScreen;

static Settings* g_settings;
static uint8_t g_chosen;
// Whether what is on the panel is out of date.
static bool g_dirty;

ScreenResult FontScreenOpen(Settings* settings) {
  g_settings = settings;
  g_chosen = settings->font;
  g_dirty = true;
  return ScreenOpen(&kFontScreen);
}

// Use the font chosen, and save it if it changed. Kept out of line, so that
// its message is on core 0's stack only while it runs.
static __noinline void ChooseFont(void) {
  const Font4x10 font = (Font4x10)g_chosen;
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
  UIStatusSetProblem(message);
  SYSTEM_WARN("font_not_saved", Y_LOG_STRING("problem", problem));
}

static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  // A key held down repeats, which moves through the items as a press does.
  if (event.state == kSouthbridgeKeyReleased) {
    return ScreenStays();
  }
  UIStatusClearProblem();
  if (UIPageIsMoveKey(event.key)) {
    if (UIPageMove(event.key, kFont4x10Count, &g_chosen)) {
      g_dirty = true;
    }
    return ScreenStays();
  }
  switch (event.key) {
    case kKeyCodeEnter:
    case kKeyCodeSpace:
      ChooseFont();
      return ScreenBack();
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
  UIBoxItem items[kFont4x10Count];
  for (uint8_t i = 0; i < kFont4x10Count; ++i) {
    items[i] = (UIBoxItem){Font4x10Label((Font4x10)i), false};
  }
  UIPageDrawItems(menu, "Font", NULL, 0, items, kFont4x10Count, g_chosen, true);
  DisplayEndMenu();
  g_dirty = false;
}

// Nothing opens over Font.
static void Resume(void) { g_dirty = true; }

static const char* Hint(void) {
  return "\x18\x19=Move  Enter=Select  Esc=Back";
}

static const char* PageName(void) { return "font"; }

const Screen kFontScreen = {
    .handle_key = HandleKey,
    .draw = Draw,
    .resume = Resume,
    .hint = Hint,
    .page_name = PageName,
};
