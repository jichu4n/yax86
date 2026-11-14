#include "display_power.h"

#include <stdint.h>

#include "disk_image.h"
#include "display.h"
#include "pico/time.h"
#include "runtime_log.h"

enum {
  kMillisecondsPerSecond = 1000,
  // How long after the last brightness change to wait before writing yax86.cfg,
  // so stepping through several Alt+, or Alt+. presses writes once.
  kBrightnessSaveDelayMs = 1500,
  // How long to wait before retrying a failed backlight restore. From v1.6 the
  // controller restarts its I2C peripheral after 2.5 quiet seconds.
  kBacklightRetryMs = 3000,
};

static Settings* g_settings;
static bool g_is_on = true;
static bool g_are_backlights_synced = true;
static absolute_time_t g_retry_backlights_at;
static bool g_is_swallowing_wake_key;
static uint8_t g_wake_key;
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

static bool SyncActiveBacklights(void) {
  if (g_settings == NULL || !SouthbridgeHasBacklights()) {
    g_are_backlights_synced = true;
    return true;
  }
  if (SouthbridgeSetBacklights(
          g_settings->display_brightness, g_settings->keyboard_brightness)) {
    g_are_backlights_synced = true;
    return true;
  }
  g_are_backlights_synced = false;
  g_retry_backlights_at = make_timeout_time_ms(kBacklightRetryMs);
  return false;
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

// Kept out of line, so that the problem buffer stays off core 0's stack while
// the loops run.
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
  if (!SyncActiveBacklights()) {
    DISPLAY_WARN("backlights_not_set");
  }
  g_is_on = true;
  if (SouthbridgeHasBacklights()) {
    const SouthbridgeBacklights backlights = SouthbridgeGetBacklights();
    DISPLAY_INFO(
        "resume", Y_LOG_UINT("display_backlight", backlights.display),
        Y_LOG_UINT("keyboard_backlight", backlights.keyboard));
  } else {
    DISPLAY_INFO("resume");
  }
}

// Kept out of line, as Wake() is, so that log fields stay off core 0's stack.
static __noinline void TurnOff(void) {
  g_is_swallowing_wake_key = false;
  uint8_t display = 0;
  uint8_t keyboard = 0;
  if (g_are_backlights_synced &&
      SouthbridgeReadBacklights(&display, &keyboard)) {
    ObserveBrightness(display, keyboard);
  }
  DisplayTurnOff();
  if (SouthbridgeHasBacklights() && !SouthbridgeSetBacklights(0, 0)) {
    DISPLAY_WARN("backlights_not_set");
  }
  g_are_backlights_synced = false;
  g_is_on = false;
  if (SouthbridgeHasBacklights()) {
    const SouthbridgeBacklights backlights = SouthbridgeGetBacklights();
    DISPLAY_INFO(
        "timeout", Y_LOG_UINT("timeout_s", g_settings->display_timeout),
        Y_LOG_UINT("display_backlight", backlights.display),
        Y_LOG_UINT("keyboard_backlight", backlights.keyboard));
  } else {
    DISPLAY_INFO(
        "timeout", Y_LOG_UINT("timeout_s", g_settings->display_timeout));
  }
}

void DisplayPowerApplySettings(Settings* settings) {
  g_settings = settings;
  g_is_swallowing_wake_key = false;
  if (SouthbridgeHasBacklights() && g_is_on && g_are_backlights_synced) {
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
  } else if (!SyncActiveBacklights()) {
    DISPLAY_WARN("backlights_not_set");
  }
  if (SouthbridgeHasBacklights() && g_are_backlights_synced &&
      !g_is_brightness_dirty) {
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

bool DisplayPowerConsumeWakeKey(
    SouthbridgeKeyEvent event, bool should_arm_timer) {
  if (!g_is_on) {
    Wake();
    g_off_at = should_arm_timer ? NextDeadline() : at_the_end_of_time;
    // Only a press starts something nobody saw. A repeat or a release that
    // wakes the display belongs to a key pressed while it was on, which the
    // guest has seen go down: the repeat is dropped, and the release is
    // passed on so the guest sees it come up.
    g_is_swallowing_wake_key = event.state == kSouthbridgeKeyPressed;
    g_wake_key = event.key;
    return event.state != kSouthbridgeKeyReleased;
  }
  if (!g_is_swallowing_wake_key || event.key != g_wake_key) {
    return false;
  }
  if (event.state == kSouthbridgeKeyPressed) {
    g_is_swallowing_wake_key = false;
    return false;
  }
  if (event.state == kSouthbridgeKeyReleased) {
    g_is_swallowing_wake_key = false;
  }
  g_off_at = should_arm_timer ? NextDeadline() : at_the_end_of_time;
  return true;
}

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
    if (!g_are_backlights_synced && time_reached(g_retry_backlights_at)) {
      (void)SyncActiveBacklights();
    }
    if (g_are_backlights_synced) {
      const SouthbridgeBacklights backlights = SouthbridgeGetBacklights();
      if (backlights.is_known) {
        ObserveBrightness(backlights.display, backlights.keyboard);
      }
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
  if (g_is_on && g_are_backlights_synced) {
    const SouthbridgeBacklights backlights = SouthbridgeGetBacklights();
    if (backlights.is_known) {
      ObserveBrightness(backlights.display, backlights.keyboard);
    }
  }
  if (g_is_brightness_dirty) {
    SaveBrightnessNow();
  }
}
