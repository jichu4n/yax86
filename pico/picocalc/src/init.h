// Initializing the system at startup, before either mode runs: the fault
// marker the last start may have left, USB serial, the system clock, the
// PSRAM, the logs, the LCD on core 1, the keyboard controller and the
// speaker. What it found is an InitStatus, which the report and the status
// page give.
//
// Raising the clock and starting the PSRAM are each done with the watchdog
// armed, and a marker in RAM that startup does not clear, so that a hang in
// either reboots into a start that skips it. A hard fault reboots too, with a
// marker that leaves paging off for the start after.
#ifndef YAX86_PICOCALC_INIT_H
#define YAX86_PICOCALC_INIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// What became of the request to raise the system clock.
typedef enum OverclockResult {
  // Built for the stock clock, so none was asked for.
  kOverclockNotRequested = 0,
  kOverclockApplied,
  // The PLL cannot make the clock asked for, so the stock one stays.
  kOverclockPLLCannot,
  // The last start hung raising it, so this one runs at the stock clock.
  kOverclockSkippedAfterHang,
} OverclockResult;

// What initializing the system found.
typedef struct InitStatus {
  // What became of raising the system clock.
  OverclockResult overclock;
  // The clock the LCD's SPI bus runs at, in Hz.
  uint32_t lcd_spi_hz;
  // Whether the keyboard controller answered when asked its firmware's
  // version, and the version it gave, as 0x16 for v1.6, or 0 for one older
  // than v1.6, which cannot say.
  bool southbridge_answered;
  uint8_t southbridge_version;
  // Whether the last start ended in a hard fault, which leaves paging off for
  // this one.
  bool after_fault;
} InitStatus;

// Initialize the system, recording what was found in *status. Call once, at
// startup, after painting the stacks.
void InitSystem(InitStatus* status);

// What became of the overclock, as the report names it.
const char* InitOverclockName(OverclockResult overclock);

// Write the keyboard controller's firmware version into buffer, size bytes
// long: such as v1.6, or why it is not known.
void InitFormatKeyboardVersion(
    const InitStatus* status, char* buffer, size_t size);

#endif  // YAX86_PICOCALC_INIT_H
