#include "boot_delay_screen.h"

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

typedef struct BootDelayPreset {
  const char* label;
  // Seconds to count down (at most kSettingsMaxBootDelay), or
  // kSettingsBootDelayNever.
  uint8_t boot_delay;
} BootDelayPreset;

// Every preset other than Never is at most kSettingsMaxBootDelay (60).
static const BootDelayPreset kPresets[] = {
    {"Start at once", 0}, {"1 second", 1},
    {"3 seconds", 3},     {"5 seconds", 5},
    {"10 seconds", 10},   {"30 seconds", 30},
    {"1 minute", 60},     {"Never", kSettingsBootDelayNever},
};

enum {
  kNumPresets = sizeof(kPresets) / sizeof(kPresets[0]),
};

// sizeof rather than kNumPresets: kListBoxRows is in another anonymous enum
// (-Wenum-compare).
_Static_assert(
    sizeof(kPresets) / sizeof(kPresets[0]) <= kListBoxRows,
    "boot delay presets must fit in one list");

extern const Screen kBootDelayScreen;

static Settings* g_settings;
// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;

// Index of the preset matching boot_delay, or the closest numbered preset for
// a hand-edited value in yax86.cfg.
static uint8_t PresetForBootDelay(uint8_t boot_delay) {
  if (boot_delay == kSettingsBootDelayNever) {
    return kNumPresets - 1;
  }
  uint8_t best = 0;
  uint32_t best_distance = UINT32_MAX;
  for (uint8_t i = 0; i < kNumPresets; ++i) {
    if (kPresets[i].boot_delay == kSettingsBootDelayNever) {
      continue;
    }
    const uint32_t distance =
        kPresets[i].boot_delay > boot_delay
            ? (uint32_t)(kPresets[i].boot_delay - boot_delay)
            : (uint32_t)(boot_delay - kPresets[i].boot_delay);
    if (distance < best_distance) {
      best_distance = distance;
      best = i;
    }
  }
  return best;
}

ScreenResult BootDelayScreenOpen(Settings* settings) {
  g_settings = settings;
  g_list_box.selected = PresetForBootDelay(settings->boot_delay);
  return ScreenOpen(&kBootDelayScreen);
}

// Save the boot delay chosen, for the next power on, if it changed. Kept out of
// line, so that its message is on core 0's stack only while it runs.
static __noinline void ChooseBootDelay(void) {
  const uint8_t boot_delay = kPresets[g_list_box.selected].boot_delay;
  if (boot_delay == g_settings->boot_delay) {
    return;
  }
  char problem[kVMProblemLength + 1];
  if (SettingsSaveBootDelay(g_settings, boot_delay, problem)) {
    SYSTEM_INFO(
        "boot_delay_saved", boot_delay == kSettingsBootDelayNever
                                ? Y_LOG_STRING("value", "never")
                                : Y_LOG_UINT("value", boot_delay));
    return;
  }
  char message[kTextColumns + 1];
  snprintf(
      message, sizeof(message), "Could not save boot_delay: %.50s", problem);
  StatusBarSetProblem(message);
  SYSTEM_WARN("boot_delay_not_saved", Y_LOG_STRING("problem", problem));
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxHandleKeyEvent(&g_list_box, event, kNumPresets)) {
    case kListBoxActionSelect:
      ChooseBootDelay();
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
      box, "Boot delay", NULL, 0, items, kNumPresets, g_list_box.selected,
      true);
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return "boot_delay"; }

const Screen kBootDelayScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .hint = Hint,
    .page_name = PageName,
    .is_dirty = &g_list_box.is_dirty,
};
