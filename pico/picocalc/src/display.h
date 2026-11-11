// Everything on the LCD - the guest's screen, the menu over it and the status
// rows below it - drawn by core 1, which owns the LCD and does nothing else.
//
// Core 0 runs the VM and never touches the LCD. It hands core 1 what to
// draw: a copy of the guest's screen once a frame, the status rows and the
// menu as text, and the requests to turn the panel off and on, and to read
// it back. Each is
// handed over only while core 1 is not using the last one, so the two cores
// never share anything either of them is changing.
//
// Every function here is called from core 0.
#ifndef YAX86_PICOCALC_DISPLAY_H
#define YAX86_PICOCALC_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "core/video.h"
#include "text.h"

enum {
  // The status rows: the three text rows at the bottom of the panel.
  kDisplayStatusRows = 3,
  // The most rows the menu's box can take: its tallest page, the status
  // page, needs 10. A row is 160 bytes of SRAM.
  kDisplayMenuMaxRows = 12,
};

// A row of the runtime's own text: code page 437 characters, and a CGA
// attribute for each, the foreground in the low nibble and the background in
// the high, as the guest's text modes have them.
typedef struct DisplayTextRow {
  char chars[kTextColumns];
  uint8_t attrs[kTextColumns];
} DisplayTextRow;

// The status rows, top to bottom: messages, the drives, and the indicators.
typedef struct DisplayStatus {
  DisplayTextRow rows[kDisplayStatusRows];
} DisplayStatus;

// The menu, a box of text drawn over the guest's screen: its top left cell,
// in the panel's text cells, its size, and its rows.
typedef struct DisplayMenu {
  uint8_t column;
  uint8_t row;
  uint8_t columns;
  uint8_t rows;
  DisplayTextRow text[kDisplayMenuMaxRows];
} DisplayMenu;

// What drawing has cost, for the status page and the report.
typedef struct DisplayStats {
  // Core 1: frames that drew something, the pixels they drew, and the time
  // spent in YVideoRender().
  uint32_t frames_drawn;
  uint64_t pixels_drawn;
  uint64_t render_us;
  // Core 0: frames left for the next because core 1 was still drawing, and
  // the time spent copying the guest's screen for core 1.
  uint32_t frames_late;
  uint64_t copy_us;
} DisplayStats;

// Start core 1, which brings up the LCD, draws the banner and owns the LCD
// from then on. Returns the LCD's serial clock in Hz, once it is up.
uint32_t DisplayStart(void);

// Hand core 1 a copy of the guest's screen, if it has changed since the last
// one. If core 1 is still drawing the last one, nothing is copied, and what
// changed is left recorded in video for the next.
//
// Returns whether core 1 has what changed: true if it took the copy, or if
// nothing had changed.
bool DisplayOfferFrame(YVideoState* video);

// The status rows to fill in, or NULL while core 1 is still drawing the last
// ones. Filling them in and calling DisplayEndStatus() hands them over. Only
// the rows that differ from the ones last drawn are drawn.
DisplayStatus* DisplayBeginStatus(void);
void DisplayEndStatus(void);

// The menu to fill in, or NULL while core 1 is still drawing the last one.
// Filling it in and calling DisplayEndMenu() hands it over, to be drawn over
// the guest's screen. What it covers is drawn again by the next frame, once
// the caller marks the guest's screen to be drawn in full.
DisplayMenu* DisplayBeginMenu(void);
void DisplayEndMenu(void);

// Set the font the runtime's own text - the menu and the status rows - is
// drawn in, redrawing the status rows if they are already showing.
void DisplaySetFont(Font4x10 font);

// Turn the panel off, after whatever core 1 has already been handed, and on
// again. The panel keeps what it showed while it is off, so only what changed
// meanwhile needs drawing once it is back on. Turning it on takes core 1
// 120ms, which core 0 does not wait for.
void DisplayTurnOff(void);
void DisplayTurnOn(void);

// Have core 1 read the panel back and send it to the host as the reply to the
// screenshot command with this id, which it ends with status=ok. Returns
// false, doing nothing, while it is still sending the last one. Core 1 draws
// nothing meanwhile, for about two seconds, and what changes on the guest's
// screen is drawn once it is done.
bool DisplaySendScreenshot(uint32_t id);

DisplayStats DisplayGetStats(void);

#endif  // YAX86_PICOCALC_DISPLAY_H
