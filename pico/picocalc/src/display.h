// Everything on the LCD - the guest's screen, the banner and the status lines
// below it - drawn by core 1, which owns the LCD and does nothing else.
//
// Core 0 runs the machine and never touches the LCD. It hands core 1 what to
// draw: a copy of the guest's screen once a frame, the status line values,
// and the requests to turn the panel off and on. Each is handed over only
// while core 1 is not using the last one, so the two cores never share
// anything either of them is changing.
//
// Every function here is called from core 0.
#ifndef YAX86_PICOCALC_DISPLAY_H
#define YAX86_PICOCALC_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "core/video.h"
#include "text.h"

// The status lines, in the order they appear below the guest's screen.
typedef enum DisplayStatusLine {
  kDisplayStatusSystem = 0,
  kDisplayStatusBoot,
  kDisplayStatusSpeed,
  kDisplayStatusClock,
  kDisplayStatusDisplay,
  kDisplayStatusLineCount,
} DisplayStatusLine;

// The value shown on each status line.
typedef struct DisplayStatus {
  char values[kDisplayStatusLineCount][kTextColumns + 1];
} DisplayStatus;

// What drawing has cost, for the status lines and the report.
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
void DisplayOfferFrame(YVideoState* video);

// The status values to fill in, or NULL while core 1 is still drawing the last
// ones. Filling them in and calling DisplayEndStatus() hands them over.
DisplayStatus* DisplayBeginStatus(void);
void DisplayEndStatus(void);

// Turn the panel off, after whatever core 1 has already been handed, and on
// again. The panel keeps what it showed while it is off, so only what changed
// meanwhile needs drawing once it is back on. Turning it on takes core 1
// 120ms, which core 0 does not wait for.
void DisplayTurnOff(void);
void DisplayTurnOn(void);

DisplayStats DisplayGetStats(void);

#endif  // YAX86_PICOCALC_DISPLAY_H
