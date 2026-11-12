#include "settings.h"

#include <stdio.h>
#include <string.h>

#include "core/platform.h"
#include "fs.h"
#include "runtime_log.h"

static const char kSettingsPath[] = "yax86/yax86.cfg";
static const char kBootDefaultKey[] = "boot_default";
static const char kFontKey[] = "font";

static bool TakeSettingsPair(
    const char* key, const char* value, void* context,
    char problem[kConfigFileProblemLength + 1]) {
  Settings* settings = context;
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
  // A key this firmware does not know, perhaps one a later firmware wrote,
  // is left for whatever reads it, rather than stopping the reading.
  SYSTEM_WARN("settings_key_ignored", Y_LOG_STRING("key", key));
  return true;
}

bool SettingsRead(
    Settings* settings, char problem[kConfigFileProblemLength + 1]) {
  settings->boot_default[0] = '\0';
  settings->boot_delay = kSettingsDefaultBootDelay;
  settings->font = kFont4x10Default;
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
