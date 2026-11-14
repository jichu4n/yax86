#include "settings.h"

#include <stdio.h>
#include <string.h>

#include "core/platform.h"
#include "fs.h"
#include "runtime_log.h"

static const char kSettingsPath[] = "yax86/yax86.cfg";
static const char kBootDefaultKey[] = "boot_default";
static const char kFontKey[] = "font";
static const char kDisplayTimeoutKey[] = "display_timeout";
static const char kDisplayBrightnessKey[] = "display_brightness";
static const char kKeyboardBrightnessKey[] = "keyboard_brightness";

static bool TakeSettingsPair(
    const char* key, const char* value, void* context,
    char problem[kConfigFileProblemLength + 1]) {
  Settings* settings = context;
  // Every pair as the file has it, before it is checked, so that the log
  // shows what the settings were read from.
  SYSTEM_INFO(
      "settings_key", Y_LOG_STRING("key", key), Y_LOG_STRING("value", value));
  if (strcmp(key, kBootDefaultKey) == 0) {
    VMEntry entry;
    settings->boot_default[0] = '\0';
    if (value[0] != '\0' && !VMEntryMake(value, &entry)) {
      snprintf(
          problem, kConfigFileProblemLength + 1,
          "boot_default is not a VM's name");
      return false;
    }
    snprintf(
        settings->boot_default, sizeof(settings->boot_default), "%s", value);
    return true;
  }
  if (strcmp(key, "boot_delay") == 0) {
    uint64_t seconds = 0;
    if (!YLogfmtParseUInt(value, &seconds) || seconds > kSettingsMaxBootDelay) {
      snprintf(
          problem, kConfigFileProblemLength + 1, "boot_delay is not 0 to %u",
          (unsigned)kSettingsMaxBootDelay);
      return false;
    }
    settings->boot_delay = (uint8_t)seconds;
    return true;
  }
  if (strcmp(key, kFontKey) == 0) {
    if (!Font4x10FromName(value, &settings->font)) {
      snprintf(problem, kConfigFileProblemLength + 1, "no font %s", value);
      return false;
    }
    return true;
  }
  if (strcmp(key, kDisplayTimeoutKey) == 0) {
    uint64_t seconds = 0;
    if (!YLogfmtParseUInt(value, &seconds) ||
        seconds > kSettingsMaxDisplayTimeout) {
      snprintf(
          problem, kConfigFileProblemLength + 1,
          "display_timeout is not 0 to %u",
          (unsigned)kSettingsMaxDisplayTimeout);
      return false;
    }
    settings->display_timeout = (uint16_t)seconds;
    return true;
  }
  if (strcmp(key, kDisplayBrightnessKey) == 0) {
    uint64_t brightness = 0;
    if (!YLogfmtParseUInt(value, &brightness) ||
        brightness < kSettingsMinDisplayBrightness ||
        brightness > kSettingsMaxBrightness) {
      snprintf(
          problem, kConfigFileProblemLength + 1,
          "display_brightness is not %u to %u",
          (unsigned)kSettingsMinDisplayBrightness,
          (unsigned)kSettingsMaxBrightness);
      return false;
    }
    settings->display_brightness = (uint8_t)brightness;
    settings->has_display_brightness = true;
    return true;
  }
  if (strcmp(key, kKeyboardBrightnessKey) == 0) {
    uint64_t brightness = 0;
    if (!YLogfmtParseUInt(value, &brightness) ||
        brightness > kSettingsMaxBrightness) {
      snprintf(
          problem, kConfigFileProblemLength + 1,
          "keyboard_brightness is not 0 to %u",
          (unsigned)kSettingsMaxBrightness);
      return false;
    }
    settings->keyboard_brightness = (uint8_t)brightness;
    settings->has_keyboard_brightness = true;
    return true;
  }
  // A key this firmware does not know, perhaps one a later firmware wrote,
  // is left for whatever reads it, rather than stopping the reading.
  SYSTEM_WARN("settings_key_ignored", Y_LOG_STRING("key", key));
  return true;
}

// SettingsRead(), but for logging its problem.
static bool ReadFile(
    Settings* settings, char problem[kConfigFileProblemLength + 1]) {
  settings->boot_default[0] = '\0';
  settings->boot_delay = kSettingsDefaultBootDelay;
  settings->font = kFont4x10Default;
  settings->display_timeout = kSettingsDefaultDisplayTimeout;
  settings->display_brightness = kSettingsDefaultDisplayBrightness;
  settings->keyboard_brightness = kSettingsDefaultKeyboardBrightness;
  settings->has_display_brightness = false;
  settings->has_keyboard_brightness = false;
  const ConfigFileResult result =
      ConfigFileRead(kSettingsPath, TakeSettingsPair, settings, problem);
  if (result == kConfigFileMissing) {
    problem[0] = '\0';
  }
  if (result == kConfigFileProblem) {
    return false;
  }
  VMEntry entry;
  if (settings->boot_default[0] != '\0' &&
      (!VMEntryMake(settings->boot_default, &entry) || !FSExists(entry.path))) {
    snprintf(
        problem, kConfigFileProblemLength + 1, "no VM named %s",
        settings->boot_default);
    settings->boot_default[0] = '\0';
    return false;
  }
  return true;
}

bool SettingsRead(
    Settings* settings, char problem[kConfigFileProblemLength + 1]) {
  if (ReadFile(settings, problem)) {
    return true;
  }
  SYSTEM_WARN("settings_problem", Y_LOG_STRING("problem", problem));
  return false;
}

bool SettingsSaveBootDefault(
    Settings* settings, const char* name,
    char problem[kConfigFileProblemLength + 1]) {
  const ConfigFileSetting setting = {.key = kBootDefaultKey, .value = name};
  if (!ConfigFileUpdate(kSettingsPath, &setting, 1, problem)) {
    return false;
  }
  snprintf(settings->boot_default, sizeof(settings->boot_default), "%s", name);
  return true;
}

bool SettingsSaveFont(
    Settings* settings, Font4x10 font,
    char problem[kConfigFileProblemLength + 1]) {
  settings->font = font;
  const ConfigFileSetting setting = {
      .key = kFontKey, .value = Font4x10Name(font)};
  return ConfigFileUpdate(kSettingsPath, &setting, 1, problem);
}

bool SettingsSaveDisplayTimeout(
    Settings* settings, uint16_t seconds,
    char problem[kConfigFileProblemLength + 1]) {
  settings->display_timeout = seconds;
  char value[8];
  snprintf(value, sizeof(value), "%u", (unsigned)seconds);
  const ConfigFileSetting setting = {.key = kDisplayTimeoutKey, .value = value};
  return ConfigFileUpdate(kSettingsPath, &setting, 1, problem);
}

bool SettingsSaveBrightness(
    Settings* settings, uint8_t display_brightness, uint8_t keyboard_brightness,
    char problem[kConfigFileProblemLength + 1]) {
  settings->display_brightness = display_brightness;
  settings->keyboard_brightness = keyboard_brightness;
  settings->has_display_brightness = true;
  settings->has_keyboard_brightness = true;
  char display_value[4];
  char keyboard_value[4];
  snprintf(
      display_value, sizeof(display_value), "%u", (unsigned)display_brightness);
  snprintf(
      keyboard_value, sizeof(keyboard_value), "%u",
      (unsigned)keyboard_brightness);
  const ConfigFileSetting pairs[2] = {
      {.key = kDisplayBrightnessKey, .value = display_value},
      {.key = kKeyboardBrightnessKey, .value = keyboard_value},
  };
  return ConfigFileUpdate(
      kSettingsPath, pairs, sizeof(pairs) / sizeof(pairs[0]), problem);
}
