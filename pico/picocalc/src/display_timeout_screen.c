#include "display_timeout_screen.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "box.h"
#include "display.h"
#include "list_box.h"
#include "pico/stdlib.h"
#include "runtime_log.h"
#include "status_bar.h"
#include "text.h"
#include "vm_config.h"

typedef struct TimeoutPreset {
  const char* label;
  uint16_t seconds;
} TimeoutPreset;

static const TimeoutPreset kPresets[] = {
    {"30 seconds", 30}, {"1 minute", 60},    {"2 minutes", 120},
    {"5 minutes", 300}, {"10 minutes", 600}, {"30 minutes", 1800},
    {"Never", 0},
};

enum {
  kNumPresets = sizeof(kPresets) / sizeof(kPresets[0]),
};

extern const Screen kDisplayTimeoutScreen;

static Settings* g_settings;
// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;

// Index of the preset matching seconds, or the closest non-zero preset for a
// hand-edited value in yax86.cfg.
static uint8_t PresetForSeconds(uint16_t seconds) {
  if (seconds == 0) {
    return kNumPresets - 1;
  }
  uint8_t best = 0;
  uint32_t best_distance = UINT32_MAX;
  for (uint8_t i = 0; i < kNumPresets; ++i) {
    if (kPresets[i].seconds == 0) {
      continue;
    }
    const uint32_t distance = kPresets[i].seconds > seconds
                                  ? (uint32_t)(kPresets[i].seconds - seconds)
                                  : (uint32_t)(seconds - kPresets[i].seconds);
    if (distance < best_distance) {
      best_distance = distance;
      best = i;
    }
  }
  return best;
}

ScreenResult DisplayTimeoutScreenOpen(Settings* settings) {
  g_settings = settings;
  g_list_box.selected = PresetForSeconds(settings->display_timeout);
  return ScreenOpen(&kDisplayTimeoutScreen);
}

// Use the timeout chosen, and save it if it changed. Kept out of line, so that
// its message is on core 0's stack only while it runs.
static __noinline void ChooseDisplayTimeout(void) {
  const uint16_t seconds = kPresets[g_list_box.selected].seconds;
  if (seconds == g_settings->display_timeout) {
    return;
  }
  char problem[kVMProblemLength + 1];
  if (SettingsSaveDisplayTimeout(g_settings, seconds, problem)) {
    SYSTEM_INFO("display_timeout_saved", Y_LOG_UINT("seconds", seconds));
    return;
  }
  char message[kTextColumns + 1];
  snprintf(
      message, sizeof(message), "Could not save display_timeout: %.48s",
      problem);
  StatusBarSetProblem(message);
  SYSTEM_WARN("display_timeout_not_saved", Y_LOG_STRING("problem", problem));
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxHandleKeyEvent(&g_list_box, event, kNumPresets)) {
    case kListBoxActionSelect:
      ChooseDisplayTimeout();
      return ScreenBack();
    case kListBoxActionBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

static void OnDraw(DisplayBox* box) {
  BoxItem items[kNumPresets];
  for (uint8_t i = 0; i < kNumPresets; ++i) {
    items[i] = (BoxItem){kPresets[i].label, false};
  }
  ListBoxDrawItems(
      box, "Display timeout", NULL, 0, items, kNumPresets, g_list_box.selected,
      true);
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return "display_timeout"; }

const Screen kDisplayTimeoutScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .hint = Hint,
    .page_name = PageName,
    .is_dirty = &g_list_box.is_dirty,
};
