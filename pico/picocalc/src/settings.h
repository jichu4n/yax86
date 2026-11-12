// The card's settings, `yax86/yax86.cfg`, in the same form as a VM's
// vm.cfg:
//
//   boot_default=DOS330
//   boot_delay=5
//   font=ymini
//
// boot_default is a VM's name. It is the VM the list highlights, and starts
// by itself after boot_delay seconds with no key pressed; 0 starts it at
// once. Without settings, or with a boot_default naming no VM, the first VM
// listed is the default, after 5 seconds. Whichever VM starts becomes
// boot_default for the next power on. font names the 4x10 font.
#ifndef YAX86_PICOCALC_SETTINGS_H
#define YAX86_PICOCALC_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#include "config_file.h"
#include "font_4x10.h"
#include "vm_config.h"

enum {
  // How long the list at power on waits for a key, in seconds, without
  // settings, and at most.
  kSettingsDefaultBootDelay = 5,
  kSettingsMaxBootDelay = 60,
};

typedef struct Settings {
  // The name of the VM to highlight and start, or empty for the first
  // listed.
  char boot_default[kVMNameLength + 1];
  uint8_t boot_delay;
  Font4x10 font;
} Settings;

// Read `yax86/yax86.cfg` into settings.
//
// Settings the file leaves out keep their defaults, as does every setting if
// there is no card or no file. Each key and value is logged as it is read,
// and a key this firmware does not know is skipped with a warning.
//
// Returns false, with the problem described and logged as settings_problem,
// in two cases:
//   - The file cannot be read, a line cannot be parsed, or a known key has a
//     bad value. Reading stops there, so settings has the values read before
//     it and the defaults for the rest.
//   - boot_default names a VM that does not exist. boot_default is then left
//     empty, so the first VM listed is the default.
bool SettingsRead(
    Settings* settings, char problem[kConfigFileProblemLength + 1]);

// Make the VM named name the boot_default of settings and of the card's
// file, through ConfigFileUpdate(); or with name empty, clear it, so that
// the first VM listed is the default. Returns false, with the problem
// described and settings left as they were, if the file could not be read
// or written.
bool SettingsSaveBootDefault(
    Settings* settings, const char* name,
    char problem[kConfigFileProblemLength + 1]);

// Make font the font of settings, and save it to the card's file through
// ConfigFileUpdate(). Returns false, with the problem described, if the file
// could not be read or written; settings has the font either way, as it is
// in use whether or not it was saved.
bool SettingsSaveFont(
    Settings* settings, Font4x10 font,
    char problem[kConfigFileProblemLength + 1]);

#endif  // YAX86_PICOCALC_SETTINGS_H
