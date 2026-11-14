// Turns the PicoCalc keyboard's key events into the scancodes a PC/XT keyboard
// sends.
//
// The keyboard controller reports characters rather than keys: shifted keys
// arrive already shifted, as '!' rather than Shift and 1, and the keys a PC
// has but the PicoCalc does not - F6 to F10, Home, End, Page Up and Down,
// Insert and Break - arrive as the shifted forms of other keys. It reports
// Shift, Ctrl and Alt as keys of their own as well, so what is held is known.
//
// So a character is sent as the key that types it on a PC, and the modifiers
// are passed through as they are. A key the PicoCalc only has as a shifted
// form is sent as a press and release of the PC key with the modifiers that
// made it let go around it, since on a PC Shift would turn Page Up into a 9.
#ifndef YAX86_PICOCALC_KEYMAP_H
#define YAX86_PICOCALC_KEYMAP_H

#include <stdint.h>

#include "southbridge.h"

enum {
  // The most scancodes one key event turns into: a key sent with Left Shift,
  // Right Shift and Alt let go around it, its make and break codes, and the
  // three modifiers pressed again.
  kKeymapMaxNumScancodes = 8,
};

// Translate one key event into the scancodes to send, in order, and return how
// many there are. Keys a PC has no scancode for are dropped.
uint8_t KeymapTranslate(
    SouthbridgeKeyEvent event, uint8_t scancodes[kKeymapMaxNumScancodes]);

#endif  // YAX86_PICOCALC_KEYMAP_H
