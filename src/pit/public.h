// Public interface for the PIT module.
#ifndef YAX86_PIT_PUBLIC_H
#define YAX86_PIT_PUBLIC_H

// This module emulates the Intel 8253/8254 PIT on the IBM PC series.
//
// Note that we do not support all features of the 8253/8254 PIT, notably:
// - BCD mode
// - Modes 1, 4, and 5
//
// Channel 0 is the system timer (IRQ 0).
// Channel 1 is for DRAM refresh (not connected).
// Channel 2 is for the PC speaker.

#include <stdbool.h>
#include <stdint.h>

struct PITState;

// Caller-provided runtime configuration for the PIT.
typedef struct PITConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Callback to raise an IRQ.
  void (*raise_irq)(void* context, uint8_t irq);
} PITConfig;

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
  // Pointer to the PIT configuration.
  PITConfig* config;

  // The three timer channels.
  PITTimer timers[3];
} PITState;

// Initializes the PIT to its power-on state.
void PITInit(PITState* pit, PITConfig* config);

// Handles reads from the PIT's I/O ports (0x40-0x42).
uint8_t PITReadPort(PITState* pit, uint16_t port);

// Handles writes to the PIT's I/O ports (0x40-0x43).
void PITWritePort(PITState* pit, uint16_t port, uint8_t value);

// Simulates a single tick of the PIT's input clock. This method should be
// invoked at a frequency of 1.193182 MHz for accurate timing.
void PITTick(PITState* pit);

#endif  // YAX86_PIT_PUBLIC_H

