// Settings, from the list: a page of what can be set, of which there is one,
// the font.
#ifndef YAX86_PICOCALC_SETTINGS_SCREEN_H
#define YAX86_PICOCALC_SETTINGS_SCREEN_H

#include "screen.h"
#include "settings.h"

// Show Settings, for the settings read from the card. settings must outlive
// the screen, and Font changes and saves it.
ScreenResult SettingsScreenOpen(Settings* settings);

#endif  // YAX86_PICOCALC_SETTINGS_SCREEN_H
