// Keys a host types, through the commands in command.h: a queue of key events
// the main loop takes one at a time, through the same path as the keyboard's.
//
// An event is the keyboard controller's, so the keymap turns it into
// scancodes exactly as it does a key pressed on the PicoCalc. The controller
// reports a shifted character as the character - 'A', or '!' - and Shift as a
// key of its own, so typing text adds Shift around the characters that need
// it, as a hand on the keyboard would.
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

// Queue the events that type text: printable ASCII, with \r or \n for Enter,
// \t for Tab, \b for Backspace and \x1B for Esc. Queues nothing and returns
// false if any character cannot be typed, or if there is not room for all of
// it.
bool TypingQueueText(const char* text);

// Queue a key by the controller's code for it.
bool TypingQueueKey(uint8_t code, TypingAction action);

// The controller's code for a key named in a command: f1 to f10, esc, enter,
// tab, backspace, delete, insert, home, end, pageup, pagedown, up, down, left,
// right, break, shift, rshift, ctrl, alt, capslock and space, or a single
// printable character. Returns whether there is one.
bool TypingFindKey(const char* name, uint8_t* code);

// Take the next event to send, if there is one.
bool TypingNext(SouthbridgeKeyEvent* event);

// Events still to send.
uint16_t TypingPending(void);

#endif  // YAX86_PICOCALC_TYPING_H
