// Boot delay, from Settings: how long the list at power on waits for a key
// before starting the default VM by itself, or Never to wait for a key with no
// countdown, saved to the card.
#ifndef YAX86_PICOCALC_BOOT_DELAY_SCREEN_H
#define YAX86_PICOCALC_BOOT_DELAY_SCREEN_H

#include "screen.h"
#include "settings.h"

// Show the boot delay presets, with the one matching settings selected.
// settings must outlive the screen: choosing a preset sets it there and saves
// it.
ScreenResult BootDelayScreenOpen(Settings* settings);

#endif  // YAX86_PICOCALC_BOOT_DELAY_SCREEN_H
