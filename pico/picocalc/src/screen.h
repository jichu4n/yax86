// The screens shown before a VM starts, as a stack: the list at the bottom,
// and above it each page opened from the one below. main.c's loop hands each
// key to the screen on top, lets it count time, composes the status rows
// from its keys and draws it, through the functions here. A screen does the
// rest itself, and calls only down, into the modules it shares with the
// others.
//
// A screen opens another through that screen's own open function, which
// takes what the new screen works on as parameters: the parent keeps the
// state, and passes pointers to it down. The open function returns a result
// naming the new screen, which the router puts on top. A screen that is done
// returns kScreenBack, and the router takes it off. The one under it then
// shows again, told so by its resume function, and finds whatever the screen
// above changed in the state it passed.
#ifndef YAX86_PICOCALC_SCREEN_H
#define YAX86_PICOCALC_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "southbridge.h"

typedef struct Screen Screen;

// What a key, or time passing, led a screen to.
typedef enum ScreenAction {
  // Nothing for the router or the loop to do.
  kScreenStays = 0,
  // A VM was chosen to start, which the loop takes.
  kScreenChoseVM,
  // Show the screen the result names, on top.
  kScreenOpen,
  // Take the screen on top off, and show the one under it again.
  kScreenBack,
} ScreenAction;

typedef struct ScreenResult {
  ScreenAction action;
  // The screen to show, for kScreenOpen.
  const Screen* screen;
} ScreenResult;

// What a screen does, called only while it is on top.
struct Screen {
  // Carry out what a key event leads to.
  ScreenResult (*handle_key)(SouthbridgeKeyEvent key);
  // Called every pass of the loop, for what happens with time: the list's
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

static inline ScreenResult ScreenStays(void) {
  return (ScreenResult){kScreenStays, 0};
}

static inline ScreenResult ScreenChoseVM(void) {
  return (ScreenResult){kScreenChoseVM, 0};
}

// For a screen's open function to return, once it has taken its state.
static inline ScreenResult ScreenOpen(const Screen* screen) {
  return (ScreenResult){kScreenOpen, screen};
}

static inline ScreenResult ScreenBack(void) {
  return (ScreenResult){kScreenBack, 0};
}

// Show screen alone, until ScreenClose().
void ScreenShow(const Screen* screen);
void ScreenClose(void);
bool ScreenIsOpen(void);

// Hand a key event, or time passing, to the screen on top, and open or take
// off a screen as it says. Returns what it led to.
ScreenResult ScreenHandleKey(SouthbridgeKeyEvent key);
ScreenResult ScreenTick(void);

// Draw the screen on top, if it has changed.
void ScreenDraw(uint64_t now_us);

// The keys of the screen on top, for the loop to compose the status rows
// from: the list's loop with ui_status.c, and the VM's with its drives and
// locks.
const char* ScreenHint(void);

// The page showing, as the report gives it.
const char* ScreenPageName(void);

#endif  // YAX86_PICOCALC_SCREEN_H
