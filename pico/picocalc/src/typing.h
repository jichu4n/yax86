// Keys a host presses, through the key command in command.h: a queue of key
// events the main loop takes one at a time, through the same path as the
// keyboard's.
//
// An event is the keyboard controller's, so the keymap turns it into
// scancodes exactly as it does a key pressed on the PicoCalc, and a host can
// do exactly what a hand on the keyboard can. A character names the key that
// types it, and Shift is a key of its own: the keymap sends 'a' and 'A' as the
// same key, so 'A' is Shift held around a tap of 'a', as it is on the device.
#ifndef YAX86_PICOCALC_TYPING_H
#define YAX86_PICOCALC_TYPING_H

#include <stdbool.h>
#include <stdint.h>

#include "southbridge.h"

typedef enum TypingAction {
  // Press and release.
  kTypingTap = 0,
  kTypingPress,
  kTypingRelease,
} TypingAction;

// Queue a key by the controller's code for it. Queues nothing and returns
// false if there is not room for it.
bool TypingQueueKey(uint8_t code, TypingAction action);

// The controller's code for a key named in a command: f1 to f10, esc, enter,
// tab, backspace, delete, insert, home, end, pageup, pagedown, up, down, left,
// right, break, shift, rshift, ctrl, alt, capslock and space, or a single
// printable character. Returns whether there is one.
bool TypingFindKey(const char* name, uint8_t* code);

// Take the next event to send, if there is one.
bool TypingNext(SouthbridgeKeyEvent* event);

#endif  // YAX86_PICOCALC_TYPING_H
