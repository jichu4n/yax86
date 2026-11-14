// The status page of the in-VM menu: a wide box of labeled lines on what the
// runtime found and what running the VM has cost so far, such as the clock,
// the boot and the speed. It is drawn again every second while it shows, so
// that its figures stay current. Any of Enter, Space, Esc and Backspace goes
// back.
//
// The lines are the emulator's, which fills them in. This only draws them.
#ifndef YAX86_PICOCALC_STATUS_SCREEN_H
#define YAX86_PICOCALC_STATUS_SCREEN_H

#include "screen.h"

enum {
  // The lines on the page, and the longest value one can have.
  kStatusScreenNumLines = 6,
  kStatusScreenValueColumns = 68,
};

// One line of the status page.
typedef struct StatusScreenLine {
  // What the line is about, such as "Clock", drawn as a label at its start.
  const char* label;
  // The line's figures, as text.
  char value[kStatusScreenValueColumns + 1];
} StatusScreenLine;

// Show the status page with lines, which the caller keeps current while it
// shows. lines must outlive the screen.
ScreenResult StatusScreenOpen(
    const StatusScreenLine lines[kStatusScreenNumLines]);

#endif  // YAX86_PICOCALC_STATUS_SCREEN_H
