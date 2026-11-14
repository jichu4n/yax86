// Display timeout, from Settings: how long to wait with no key pressed before
// turning the display and backlights off, saved to the card.
#ifndef YAX86_PICOCALC_DISPLAY_TIMEOUT_SCREEN_H
#define YAX86_PICOCALC_DISPLAY_TIMEOUT_SCREEN_H

#include "screen.h"
#include "settings.h"

// Show the display timeout presets, with the one matching settings selected.
// settings must outlive the screen: choosing a preset sets it there and saves
// it.
ScreenResult DisplayTimeoutScreenOpen(Settings* settings);

#endif  // YAX86_PICOCALC_DISPLAY_TIMEOUT_SCREEN_H
