#include "public.h"

void PITInit(PITState* pit, PITConfig* config) {
  static const PITState zero_pit_state = {0};
  *pit = zero_pit_state;
  pit->config = config;
}

void PITWritePort(PITState* pit, uint16_t port, uint8_t value) {
  // TODO: Implement PIT write logic.
}

uint8_t PITReadPort(PITState* pit, uint16_t port) {
  // TODO: Implement PIT read logic.
  return 0;
}

void PITTick(PITState* pit) {
  // TODO: Implement PIT tick logic.
}
