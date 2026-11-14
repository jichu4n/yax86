// Settings, from the list: a page of card-wide settings, Font and Display
// timeout.
#ifndef YAX86_PICOCALC_SETTINGS_SCREEN_H
#define YAX86_PICOCALC_SETTINGS_SCREEN_H

#include "screen.h"
#include "settings.h"

// Show Settings, for the settings read from the card. settings must outlive
// the screen, and its pages change and save it.
ScreenResult SettingsScreenOpen(Settings* settings);

#endif  // YAX86_PICOCALC_SETTINGS_SCREEN_H
