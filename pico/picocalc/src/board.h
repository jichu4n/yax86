// What bringing the board up found, which main.c fills in for the report and
// the status page: what became of raising the system clock, the LCD's clock,
// the keyboard controller's firmware, and whether the last start ended in a
// hard fault.
#ifndef YAX86_PICOCALC_BOARD_H
#define YAX86_PICOCALC_BOARD_H

#include <stdbool.h>
#include <stddef.h>
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

// What became of the overclock, as the report names it.
const char* BoardOverclockName(OverclockResult overclock);

// The keyboard controller's firmware version, as v1.6, or why there is
// none, into buffer, size bytes long.
void BoardFormatSouthbridgeVersion(
    const BoardStatus* board, char* buffer, size_t size);

#endif  // YAX86_PICOCALC_BOARD_H
