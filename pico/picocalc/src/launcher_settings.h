// Settings, a page from the list at power on, and its Font page, which
// chooses the 4x10 font: used at once, and saved to the card's YAX86.CFG for
// the next start.
//
// This holds the pages and does what is chosen on them; launcher.c opens it
// from the list and goes back to the list when it says so.
#ifndef YAX86_PICOCALC_LAUNCHER_SETTINGS_H
#define YAX86_PICOCALC_LAUNCHER_SETTINGS_H

#include <stdint.h>

#include "font_4x10.h"
#include "southbridge.h"

// What a key did in Settings.
typedef enum LauncherSettingsResult {
  // It stays open.
  kLauncherSettingsStays = 0,
  // Esc on its first page: back to the list.
  kLauncherSettingsLeft,
  // Another font was chosen, used and saved: see LauncherSettingsFont().
  kLauncherSettingsFontChanged,
} LauncherSettingsResult;

// Show Settings, with font the one in use.
void LauncherSettingsOpen(Font4x10 font);

// The font in use, as last chosen.
Font4x10 LauncherSettingsFont(void);

// Hand Settings a key event, while it shows.
LauncherSettingsResult LauncherSettingsHandleKey(SouthbridgeKeyEvent event);

// Draw Settings if it has changed. Call every pass of the loop while it
// shows.
void LauncherSettingsDraw(void);

// The keys to show on the status bar's message row, as Key=Action pairs.
const char* LauncherSettingsHint(void);

// The page showing, as the report gives it.
const char* LauncherSettingsPageName(void);

#endif  // YAX86_PICOCALC_LAUNCHER_SETTINGS_H
