// What bringing the board up found, which main.c fills in and the emulator
// reports: the system clock, the LCD's and the keyboard controller's, and
// whether the last start ended in a hard fault.
#ifndef YAX86_PICOCALC_BOARD_H
#define YAX86_PICOCALC_BOARD_H

#include <stdbool.h>
#include <stdint.h>

// What became of the request to raise the system clock.
typedef enum OverclockResult {
  kOverclockNotRequested = 0,
  kOverclockApplied,
  kOverclockPLLCannot,
  kOverclockSkippedAfterHang,
} OverclockResult;

typedef struct BoardStatus {
  OverclockResult overclock;
  uint32_t lcd_spi_hz;
  bool southbridge_answered;
  uint8_t southbridge_version;
  // Whether the last start ended in a hard fault, which leaves paging off for
  // this one.
  bool after_fault;
} BoardStatus;

#endif  // YAX86_PICOCALC_BOARD_H
