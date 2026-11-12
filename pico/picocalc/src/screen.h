// The screens shown before a VM starts, and the one showing. main.c's loop
// hands each key to the screen showing, lets it count time, composes the
// status rows from its keys and draws it, through the functions here; a
// screen does the rest itself, and calls only down, into the modules it
// shares with the others.
#ifndef YAX86_PICOCALC_SCREEN_H
#define YAX86_PICOCALC_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "southbridge.h"

// What a key, or time passing, led a screen to.
typedef enum ScreenResult {
  // Nothing for the loop to do.
  kScreenStays = 0,
  // A VM was chosen to start, which the loop takes.
  kScreenChoseVM,
} ScreenResult;

// What a screen does, called only while it shows.
typedef struct Screen {
  // Carry out what a key event leads to.
  ScreenResult (*handle_key)(SouthbridgeKeyEvent key);
  // Called every pass of the loop, for what happens with time: the list's
  // countdown.
  ScreenResult (*tick)(void);
  // Draw the screen if it has changed. Called every pass of the loop.
  void (*draw)(uint64_t now_us);
  // The keys to show on the status rows' message row, as Key=Action pairs.
  const char* (*hint)(void);
  // The page showing, as the report gives it.
  const char* (*page_name)(void);
} Screen;

// Show screen, until ScreenClose().
void ScreenShow(const Screen* screen);
void ScreenClose(void);
bool ScreenIsOpen(void);

// What the screen showing makes of a key event, and of time passing.
ScreenResult ScreenHandleKey(SouthbridgeKeyEvent key);
ScreenResult ScreenTick(void);

// Draw the screen showing, if it has changed.
void ScreenDraw(uint64_t now_us);

// The keys of the screen showing, for the loop to compose the status rows
// from: the list's loop with ui_status.c, and the VM's with its drives and
// locks.
const char* ScreenHint(void);

// The page showing, as the report gives it.
const char* ScreenPageName(void);

#endif  // YAX86_PICOCALC_SCREEN_H
