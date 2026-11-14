#include "display_power.h"

#include <stdint.h>

#include "disk_image.h"
#include "display.h"
#include "pico/time.h"
#include "runtime_log.h"
#include "southbridge.h"

enum {
  kMillisecondsPerSecond = 1000,
  // How long after the last brightness change to wait before writing yax86.cfg,
  // so stepping through several Alt+, or Alt+. presses writes once.
  kBrightnessSaveDelayMs = 1500,
};

static Settings* g_settings;
static bool g_is_on = true;
static absolute_time_t g_off_at;
static bool g_is_brightness_dirty;
static absolute_time_t g_save_brightness_at;

static absolute_time_t NextDeadline(void) {
  if (g_settings == NULL || g_settings->display_timeout == 0) {
    return at_the_end_of_time;
  }
  return make_timeout_time_ms(
      (uint32_t)g_settings->display_timeout * kMillisecondsPerSecond);
}

static void ObserveBrightness(uint8_t display, uint8_t keyboard) {
  bool did_change = false;
  if (display >= kSettingsMinDisplayBrightness &&
      display != g_settings->display_brightness) {
    g_settings->display_brightness = display;
    did_change = true;
  }
  if (keyboard != g_settings->keyboard_brightness) {
    g_settings->keyboard_brightness = keyboard;
    did_change = true;
  }
  if (did_change) {
    g_is_brightness_dirty = true;
    g_save_brightness_at = make_timeout_time_ms(kBrightnessSaveDelayMs);
  }
}

// Kept out of line, so that problem stays off core 0's stack while the loops
// run.
static __noinline void SaveBrightnessNow(void) {
  g_is_brightness_dirty = false;
  if (DiskImageMountCard() != kDiskImageOpen) {
    return;
  }
  char problem[kConfigFileProblemLength + 1];
  if (SettingsSaveBrightness(
          g_settings, g_settings->display_brightness,
          g_settings->keyboard_brightness, problem)) {
    SYSTEM_INFO(
        "brightness_saved",
        Y_LOG_UINT("display", g_settings->display_brightness),
        Y_LOG_UINT("keyboard", g_settings->keyboard_brightness));
    return;
  }
  SYSTEM_WARN("brightness_not_saved", Y_LOG_STRING("problem", problem));
}

// Kept out of line, so that log fields stay off core 0's stack while the loops
// run.
static __noinline void Wake(void) {
  DisplayTurnOn();
  if (g_settings != NULL && SouthbridgeHasBacklights() &&
      !SouthbridgeSetBacklights(
          g_settings->display_brightness, g_settings->keyboard_brightness)) {
    DISPLAY_WARN("backlights_not_set");
  }
  g_is_on = true;
  const SouthbridgeBacklights backlights = SouthbridgeGetBacklights();
  DISPLAY_INFO(
      "resume", Y_LOG_UINT("display_backlight", backlights.display),
      Y_LOG_UINT("keyboard_backlight", backlights.keyboard));
}

static __noinline void TurnOff(void) {
  uint8_t display = 0;
  uint8_t keyboard = 0;
  if (SouthbridgeReadBacklights(&display, &keyboard)) {
    ObserveBrightness(display, keyboard);
  }
  DisplayTurnOff();
  if (SouthbridgeHasBacklights() && !SouthbridgeSetBacklights(0, 0)) {
    DISPLAY_WARN("backlights_not_set");
  }
  g_is_on = false;
  const SouthbridgeBacklights backlights = SouthbridgeGetBacklights();
  DISPLAY_INFO(
      "timeout", Y_LOG_UINT("timeout_s", g_settings->display_timeout),
      Y_LOG_UINT("display_backlight", backlights.display),
      Y_LOG_UINT("keyboard_backlight", backlights.keyboard));
}

void DisplayPowerApplySettings(Settings* settings) {
  g_settings = settings;
  if (SouthbridgeHasBacklights() && g_is_on) {
    uint8_t live_display = 0;
    uint8_t live_keyboard = 0;
    if (g_is_brightness_dirty &&
        SouthbridgeReadBacklights(&live_display, &live_keyboard)) {
      // A brightness change made while USB mass storage owned the card was not
      // in the file just read back; keep it so the next tick writes it.
      if (live_display >= kSettingsMinDisplayBrightness) {
        settings->display_brightness = live_display;
      }
      settings->keyboard_brightness = live_keyboard;
    } else if (
        (!settings->has_display_brightness ||
         !settings->has_keyboard_brightness) &&
        SouthbridgeReadBacklights(&live_display, &live_keyboard)) {
      if (!settings->has_display_brightness) {
        settings->display_brightness =
            live_display >= kSettingsMinDisplayBrightness
                ? live_display
                : kSettingsDefaultDisplayBrightness;
      }
      if (!settings->has_keyboard_brightness) {
        settings->keyboard_brightness = live_keyboard;
      }
    }
  }
  if (!g_is_on) {
    Wake();
  } else if (
      SouthbridgeHasBacklights() &&
      !SouthbridgeSetBacklights(
          settings->display_brightness, settings->keyboard_brightness)) {
    DISPLAY_WARN("backlights_not_set");
  }
  if (SouthbridgeHasBacklights() && !g_is_brightness_dirty) {
    // Match settings to any step rounding or clamping the controller applied,
    // so the next poll does not treat that rounding as a shortcut press.
    const SouthbridgeBacklights applied = SouthbridgeGetBacklights();
    if (applied.is_known) {
      if (applied.display >= kSettingsMinDisplayBrightness) {
        settings->display_brightness = applied.display;
      }
      settings->keyboard_brightness = applied.keyboard;
    }
  }
  g_off_at = NextDeadline();
}

bool DisplayPowerIsOn(void) { return g_is_on; }

void DisplayPowerResetTimer(void) {
  if (!g_is_on) {
    Wake();
  }
  g_off_at = NextDeadline();
}

void DisplayPowerDisarm(void) {
  if (!g_is_on) {
    Wake();
  }
  g_off_at = at_the_end_of_time;
}

void DisplayPowerTick(bool can_save_settings) {
  if (g_settings == NULL) {
    return;
  }
  if (g_is_on) {
    const SouthbridgeBacklights backlights = SouthbridgeGetBacklights();
    if (backlights.is_known) {
      ObserveBrightness(backlights.display, backlights.keyboard);
    }
    if (time_reached(g_off_at)) {
      TurnOff();
    }
  }
  if (g_is_brightness_dirty && can_save_settings &&
      time_reached(g_save_brightness_at)) {
    SaveBrightnessNow();
  }
}

void DisplayPowerFlush(bool can_save_settings) {
  if (g_settings == NULL || !can_save_settings) {
    return;
  }
  if (g_is_on) {
    const SouthbridgeBacklights backlights = SouthbridgeGetBacklights();
    if (backlights.is_known) {
      ObserveBrightness(backlights.display, backlights.keyboard);
    }
  }
  if (g_is_brightness_dirty) {
    SaveBrightnessNow();
  }
}
