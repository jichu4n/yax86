#include "vm_store.h"

#include <stdio.h>
#include <strings.h>

#include "runtime_log.h"

static VMList g_vms;
static VMSettings g_settings;

const VMList* VMStoreList(void) { return &g_vms; }

const VMSettings* VMStoreSettings(void) { return &g_settings; }

bool VMStoreRead(uint8_t* default_index, char problem[kVMProblemLength + 1]) {
  VMListRead(&g_vms);
  const bool read = VMSettingsRead(&g_settings, problem);
  *default_index = 0;
  if (read && g_settings.boot_default[0] != '\0') {
    // Past the list's end, the first is the default instead.
    const uint8_t index = VMListFind(&g_vms, g_settings.boot_default);
    if (index < kVMListMax) {
      *default_index = index;
    }
  }
  if (!read) {
    SYSTEM_WARN("settings_problem", Y_LOG_STRING("problem", problem));
  }
  return read;
}

// Writing the card only when the default changes keeps a board that starts
// its default every time from writing at all.
bool VMStoreSaveBootDefault(
    const char* name, char problem[kVMProblemLength + 1]) {
  if (strcasecmp(g_settings.boot_default, name) == 0) {
    return true;
  }
  if (!VMSettingsSaveBootDefault(name, problem)) {
    SYSTEM_WARN("boot_default_not_saved", Y_LOG_STRING("problem", problem));
    return false;
  }
  snprintf(
      g_settings.boot_default, sizeof(g_settings.boot_default), "%s", name);
  SYSTEM_INFO("boot_default_saved", Y_LOG_STRING("name", name));
  return true;
}

bool VMStoreSaveFont(Font4x10 font, char problem[kVMProblemLength + 1]) {
  if (font == g_settings.font) {
    return true;
  }
  g_settings.font = font;
  if (!VMSettingsSaveFont(font, problem)) {
    SYSTEM_WARN("font_not_saved", Y_LOG_STRING("problem", problem));
    return false;
  }
  SYSTEM_INFO("font_saved", Y_LOG_STRING("font", Font4x10Name(font)));
  return true;
}
