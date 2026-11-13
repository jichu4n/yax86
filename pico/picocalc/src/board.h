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
} BoardStatus;

// What became of the overclock, as the report names it.
const char* BoardOverclockName(OverclockResult overclock);

// Write the keyboard controller's firmware version into buffer, size bytes
// long: such as v1.6, or why it is not known.
void BoardFormatSouthbridgeVersion(
    const BoardStatus* board, char* buffer, size_t size);

#endif  // YAX86_PICOCALC_BOARD_H
