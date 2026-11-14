// Turning the display and backlights off on keyboard inactivity and back on at
// a key, and saving display_brightness and keyboard_brightness to yax86.cfg as
// the keyboard controller's Alt shortcuts change them.
//
// Shared by launcher mode and emulator mode. Turning off puts the ST7365P panel
// to sleep through DisplayTurnOff() and, on keyboard controller firmware v1.6
// and later, sets both backlight registers to 0 through
// SouthbridgeSetBacklights().
#ifndef YAX86_PICOCALC_DISPLAY_POWER_H
#define YAX86_PICOCALC_DISPLAY_POWER_H

#include <stdbool.h>

#include "settings.h"

// Sync the display and keyboard backlights with *settings after reading
// yax86.cfg (or taking defaults without a card), turn the display on if it was
// off, and arm the inactivity timeout from settings->display_timeout. settings
// must outlive the mode loops.
void DisplayPowerApplySettings(Settings* settings);

// Whether the display and backlights are currently on.
bool DisplayPowerIsOn(void);

// Wake the display and restore the saved backlights if they are off, and start
// the inactivity timeout afresh from settings->display_timeout.
void DisplayPowerResetTimer(void);

// Wake the display if it is off, and disarm the inactivity timeout until
// DisplayPowerResetTimer() arms it again.
void DisplayPowerDisarm(void);

// Check for backlight changes from the keyboard controller, turn the display
// and backlights off when the timeout expires, and save debounced brightness
// changes to yax86.cfg when can_save_settings allows touching the card.
void DisplayPowerTick(bool can_save_settings);

// Save any pending brightness change to yax86.cfg at once, if can_save_settings
// allows touching the card.
void DisplayPowerFlush(bool can_save_settings);

#endif  // YAX86_PICOCALC_DISPLAY_POWER_H
