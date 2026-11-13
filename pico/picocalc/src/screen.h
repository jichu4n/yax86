// Screens: what a menu shows, one at a time. A loop shows a screen until it
// closes, handing it each key, letting it count time, composing the status
// rows from its keys and drawing it, through the functions here. The screen
// does the rest itself, and calls only down, into the modules it shares with
// the others.
//
// A result says only what the router does, never what was chosen: a screen
// writes that through a pointer its caller passed when setting it up, and
// the loop finds it there once the screen closes.
#ifndef YAX86_PICOCALC_SCREEN_H
#define YAX86_PICOCALC_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "southbridge.h"

// What a key, or time passing, led a screen to.
typedef enum ScreenResult {
  // Nothing for the router to do.
  kScreenStays = 0,
  // The screen is done, which ends the loop showing it.
  kScreenClose,
} ScreenResult;

// What a screen does, called only while it shows.
typedef struct Screen {
  // Carry out what a key event leads to.
  ScreenResult (*handle_key)(SouthbridgeKeyEvent key);
  // Called every pass of the loop, for what happens with time, such as a
  // countdown.
  ScreenResult (*tick)(void);
  // Draw the screen if it has changed. Called every pass of the loop.
  void (*draw)(uint64_t now_us);
  // The keys to show on the status rows' message row, as Key=Action pairs.
  const char* (*hint)(void);
  // The page showing, as the report gives it.
  const char* (*page_name)(void);
} Screen;

// Show screen, until it closes.
void ScreenShow(const Screen* screen);

// Whether a screen shows: false once it has closed.
bool ScreenIsOpen(void);

// Hand a key event to the screen showing, and close it if it says.
void ScreenHandleKey(SouthbridgeKeyEvent key);

// Let the screen showing act on time passing, and close it if it says. Call
// every pass of the loop while a screen shows.
void ScreenTick(void);

// Draw the screen showing, if it has changed.
void ScreenDraw(uint64_t now_us);

// The keys of the screen showing, for the loop to compose the status rows
// from, with whatever else its rows show.
const char* ScreenHint(void);

// The page showing, as the report gives it.
const char* ScreenPageName(void);

#endif  // YAX86_PICOCALC_SCREEN_H
