// Public interface for the PIT module.
#ifndef YAX86_PIT_PUBLIC_H
#define YAX86_PIT_PUBLIC_H

#include <stdbool.h>
#include <stdint.h>

// State of a single PIT timer channel.
typedef struct PITTimer {
  // The 16-bit counter value.
  uint16_t counter;
  // The 16-bit latched value for reading.
  uint16_t latch;
  // The 16-bit reload value.
  uint16_t reload_value;
  // The operating mode (0-5).
  uint8_t mode;
  // The read/write access mode.
  uint8_t access_mode;
  // BCD mode flag.
  bool bcd_mode;
  // The output state of the timer.
  bool output_state;
  // Read/write byte toggle for 16-bit access.
  bool rw_byte_toggle;
} PITTimer;

// State of the PIT.
typedef struct PITState {
  // The three timer channels.
  PITTimer timers[3];
} PITState;

// Initializes the PIT to its power-on state.
void PITInit(PITState* pit);

// Handles writes to the PIT's I/O ports (0x40-0x43).
void PITWritePort(PITState* pit, uint16_t port, uint8_t value);

// Handles reads from the PIT's I/O ports (0x40-0x42).
uint8_t PITReadPort(PITState* pit, uint16_t port);

// Simulates a single tick of the PIT's input clock.
void PITTick(PITState* pit);

#endif  // YAX86_PIT_PUBLIC_H

