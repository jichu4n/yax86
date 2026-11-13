// The status rows at the bottom of the panel, composed on core 0 and drawn by
// core 1 - two, with no more on them than is worth a glance:
//
//   row 30  messages: the keys of the screen showing, or a problem in their
//           place, and in emulator mode a few seconds each of what happened
//   row 31  in emulator mode the VM's name, its drives and the guest's locks
//           that are on; and the battery
//
// What every mode's rows have in common is here, knowing neither mode: the
// rows' layout, the battery, the problem a screen shows until the next key,
// and handing the rows to core 1. What each mode puts on them is
// launcher_status.c's and emulator_status.c's.
#ifndef YAX86_PICOCALC_STATUS_BAR_H
#define YAX86_PICOCALC_STATUS_BAR_H

#include <stdbool.h>
#include <stdint.h>

#include "display.h"
#include "southbridge.h"

enum {
  // The rows.
  kStatusBarRowMessages = 0,
  kStatusBarRowMachine,

  // CGA attributes: the foreground in the low nibble, on black.
  kStatusBarAttrNormal = 0x07,
  kStatusBarAttrBright = 0x0F,
  kStatusBarAttrDim = 0x08,
  kStatusBarAttrLit = 0x0A,
};

// Show problem in place of the keys until StatusBarClearProblem(); NULL for
// nothing. The text is copied. Any screen in either mode can show one.
void StatusBarSetProblem(const char* problem);

// Stop showing the problem, as a key pressed does. Returns whether there was
// one.
bool StatusBarClearProblem(void);

// The problem showing, or NULL. A new problem is a new pointer, so that a
// change can be told by the pointer alone.
const char* StatusBarProblem(void);

// Blank a row.
void StatusBarClearRow(DisplayTextRow* row);

// Put text on a row from column, clipped at its end, and return the column
// after it.
uint8_t StatusBarPut(
    DisplayTextRow* row, uint8_t column, const char* text, uint8_t attr);

// Put Key=Action pairs, two spaces apart, the way MS-DOS's own programs
// label theirs - "\x18\x19=Move  Enter=Select" - on a row from column: each
// key and its = in key_attr, each action in action_attr. Returns the column
// after.
uint8_t StatusBarPutKeys(
    DisplayTextRow* row, uint8_t column, const char* text, uint8_t key_attr,
    uint8_t action_attr);

// Put the battery at the end of a row: a cell for each fifth of a charge,
// and the percent, with + while it charges. Nothing while it is not known.
void StatusBarPutBattery(
    const SouthbridgeBattery* battery, DisplayTextRow* row);

// The rows to compose into, and hand them to core 1 if composed says they
// changed, or they are still waiting from the last time it was busy.
DisplayStatus* StatusBarRows(void);
void StatusBarHandOver(bool composed);

#endif  // YAX86_PICOCALC_STATUS_BAR_H
