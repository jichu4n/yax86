#include "board.h"

#include <stdio.h>

static const char* const kOverclockNames[] = {
    [kOverclockNotRequested] = "not_requested",
    [kOverclockApplied] = "applied",
    [kOverclockPLLCannot] = "pll_cannot_produce_clock",
    [kOverclockSkippedAfterHang] = "skipped_after_hang",
};

const char* BoardOverclockName(OverclockResult overclock) {
  return kOverclockNames[overclock];
}

void BoardFormatSouthbridgeVersion(
    const BoardStatus* board, char* buffer, size_t size) {
  if (!board->southbridge_answered) {
    snprintf(buffer, size, "not answering");
  } else if (board->southbridge_version == 0) {
    snprintf(buffer, size, "older than v1.6");
  } else {
    snprintf(
        buffer, size, "v%X.%X", board->southbridge_version >> 4,
        board->southbridge_version & 0xF);
  }
}
