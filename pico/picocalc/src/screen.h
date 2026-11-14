// Screens: the pages of a menu, each a screen of its own, kept as a stack
// with the page first shown at the bottom and above it each page opened from
// the one below. A mode's loop shows them until they close. Through the
// functions here, it hands each key to the screen on top, lets it count
// time, and has it draw into a box, which the loop sends to core 1. The loop
// composes the status rows from the screen's keys. A screen does the rest
// itself, and calls only down, into the modules it shares with the others.
//
// A screen is opened through its own open function, which takes what the
// screen works on as parameters, and returns the result that opens it. A
// parent keeps the state and passes pointers to it down. What a screen
// produces for the one that opened it, or for the loop, it writes through
// those pointers: there is no value in a result. A screen that is done
// returns ScreenBack(), and the one under it shows again, told so by its
// on_resume function, and finds there whatever the screen above changed. A
// screen that ends the whole menu returns ScreenClose(), and the loop, seeing
// no screen open, finds what was selected through the pointer it passed.
#ifndef YAX86_PICOCALC_SCREEN_H
#define YAX86_PICOCALC_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "display.h"
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

// What a screen does. The router calls each function only while the screen
// is on top. The on_ functions are called when something happens to the
// screen, and say what it leads to. The others answer questions about it.
struct Screen {
  // Called with each key event: a press, a held key's repeat or a release.
  ScreenResult (*on_key)(SouthbridgeKeyEvent key);
  // Called every pass of the loop, for what happens with time, such as a
  // countdown. NULL for a screen that nothing happens to with time.
  ScreenResult (*on_tick)(void);
  // Called to draw the screen into box, on the loop's first pass after its
  // dirty flag was set.
  void (*on_draw)(DisplayBox* box);
  // Called when the screen above was taken off, so that this one shows
  // again. What the screen above left can lead this one on, as a key does:
  // back, say, once what it was opened for is done. NULL for a screen that
  // only shows again.
  ScreenResult (*on_resume)(void);
  // The keys to show on the status rows' message row, as Key=Action pairs.
  const char* (*hint)(void);
  // The page showing, as the report gives it.
  const char* (*page_name)(void);
  // The screen's dirty flag, which is set while what it shows is out of
  // date, and cleared once it is drawn. The router sets it when the screen
  // opens and when it shows again. The screen sets it when a key or time
  // passing changes what it shows.
  bool* is_dirty;
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

// Take every screen off now, as a screen returning ScreenClose() does, for a
// loop that closes the screens itself, such as on a key that closes a menu
// whatever shows.
void ScreenHide(void);

// Whether a screen shows: false once they have all closed.
bool ScreenIsOpen(void);

// Hand a key event to the screen on top, and open or take off screens as it
// says.
void ScreenHandleKey(SouthbridgeKeyEvent key);

// Let the screen on top act on time passing, if it does anything with time,
// and open or take off screens as it says. Call every pass of the loop while
// a screen shows, as a screen that draws again with time sets its dirty
// flag here.
void ScreenTick(void);

// Draw the screen on top into box if its dirty flag is set, clear the flag,
// and return whether it drew. The loop takes box from DisplayBeginBox(), and
// sends it to core 1 with DisplayEndBox() if the screen drew. A mode's loop
// does the sending so that it can do what it needs first, such as drawing the
// guest's screen under a box of a new size.
bool ScreenDraw(DisplayBox* box);

// The keys of the screen on top, for the loop to compose the status rows
// from, with what else its rows show.
const char* ScreenHint(void);

// The page showing, as the report gives it.
const char* ScreenPageName(void);

#endif  // YAX86_PICOCALC_SCREEN_H
