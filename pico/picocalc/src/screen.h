// Screens: the pages of a menu, each a screen of its own, kept as a stack
// with the page first shown at the bottom and above it each page opened from
// the one below. A loop shows them until they close, handing each key to the
// screen on top, letting it count time, composing the status rows from its
// keys and drawing it, through the functions here. A screen does the rest
// itself, and calls only down, into the modules it shares with the others.
//
// A screen is opened through its own open function, which takes what the
// screen works on as parameters, and returns the result that opens it. A
// parent keeps the state and passes pointers to it down. What a screen
// produces for the one that opened it, or for the loop, it writes through
// those pointers: there is no value in a result. A screen that is done
// returns ScreenBack(), and the one under it shows again, told so by its
// resume function, and finds there whatever the screen above changed. A
// screen that ends the whole menu returns ScreenClose(), and the loop, seeing
// no screen open, finds what was chosen through the pointer it passed.
#ifndef YAX86_PICOCALC_SCREEN_H
#define YAX86_PICOCALC_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "southbridge.h"

typedef struct Screen Screen;

// What a key, or time passing, led a screen to: nothing, a screen to show on
// top, the screen on top done, or every screen done. Make one with the
// functions below. It is one pointer, the screen to open or one of the
// markers in screen.c for the rest, so that it comes back in a register: a
// larger struct comes back through memory that every caller on the way from
// the loop reserves on core 0's stack.
typedef struct ScreenResult {
  const Screen* screen;
} ScreenResult;

// What a screen does, called only while it is on top.
struct Screen {
  // Carry out what a key event leads to.
  ScreenResult (*handle_key)(SouthbridgeKeyEvent key);
  // Called every pass of the loop, for what happens with time, such as a
  // countdown. NULL for a screen that nothing happens to with time.
  ScreenResult (*tick)(void);
  // Draw the screen if it has changed. Called every pass of the loop.
  void (*draw)(uint64_t now_us);
  // The screen above was taken off, so this one shows again and has to be
  // drawn.
  void (*resume)(void);
  // The keys to show on the status rows' message row, as Key=Action pairs.
  const char* (*hint)(void);
  // The page showing, as the report gives it.
  const char* (*page_name)(void);
};

extern const Screen kScreenBackMarker;
extern const Screen kScreenCloseMarker;

// Nothing for the router to do.
static inline ScreenResult ScreenStays(void) { return (ScreenResult){0}; }

// Show screen on top. For a screen's open function to return, once it has
// taken its state.
static inline ScreenResult ScreenOpen(const Screen* screen) {
  return (ScreenResult){screen};
}

// Take the screen on top off, and show the one under it again. From the
// screen at the bottom, the same as ScreenClose().
static inline ScreenResult ScreenBack(void) {
  return (ScreenResult){&kScreenBackMarker};
}

// Take every screen off, which ends the loop showing them.
static inline ScreenResult ScreenClose(void) {
  return (ScreenResult){&kScreenCloseMarker};
}

// Show the screen an open function opened, alone, until it closes.
void ScreenShow(ScreenResult opened);

// Whether a screen shows: false once they have all closed.
bool ScreenIsOpen(void);

// Hand a key event, or time passing, to the screen on top, and open or take
// off screens as it says.
void ScreenHandleKey(SouthbridgeKeyEvent key);
void ScreenTick(void);

// Draw the screen on top, if it has changed.
void ScreenDraw(uint64_t now_us);

// The keys of the screen on top, for the loop to compose the status rows
// from, with what else its rows show.
const char* ScreenHint(void);

// The page showing, as the report gives it.
const char* ScreenPageName(void);

#endif  // YAX86_PICOCALC_SCREEN_H
