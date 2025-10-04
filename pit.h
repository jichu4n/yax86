// ==============================================================================
// YAX86 PIT MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_PIT_BUNDLE_H
#define YAX86_PIT_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/pit/public.h start
// ==============================================================================

#line 1 "./src/pit/public.h"
// Public interface for the PIT module.
#ifndef YAX86_PIT_PUBLIC_H
#define YAX86_PIT_PUBLIC_H

// This module emulates the Intel 8253/8254 PIT on the IBM PC series.
//
// Note that we do not support all features of the 8253/8254 PIT, notably:
// - Only supports binary mode (not BCD).
// - Only supports modes 0, 2, and 3 (not 1, 4, and 5)
//
// Channel 0 is used for the system timer (IRQ 0).
// Channel 1 is used for DRAM refresh on real hardware but not relevant here.
// Channel 2 is used for the PC speaker.

#include <stdbool.h>
#include <stdint.h>

enum {
  // Number of PIT channels.
  kPITNumChannels = 3,
};

// I/O ports exposed by the PIT.
typedef enum PITPort {
  // Data port for PIT channel 0
  kPITPortChannel0 = 0x40,
  // Data port for PIT channel 1
  kPITPortChannel1 = 0x41,
  // Data port for PIT channel 2
  kPITPortChannel2 = 0x42,
  // Control word port
  kPITPortControl = 0x43,
} PITPort;

struct PITState;

// Caller-provided runtime configuration for the PIT.
typedef struct PITConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Callback to raise IRQ 0.
  void (*raise_irq_0)(void* context);
  // Callback to set PC speaker frequency in Hz.
  void (*set_pc_speaker_frequency)(void* context, uint32_t frequency_hz);
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
  // Whether a latch command is active.
  bool latch_active;
} PITTimer;

// State of the PIT.
typedef struct PITState {
  // Pointer to the PIT configuration.
  PITConfig* config;

  // The three timer channels.
  PITTimer timers[kPITNumChannels];
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



// ==============================================================================
// src/pit/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/pit/pit.c start
// ==============================================================================

#line 1 "./src/pit/pit.c"
#include "public.h"

// Tick frequency of the PIT in Hz.
enum {
  kPITTickFrequencyHz = 1193182,
};

void PITInit(PITState* pit, PITConfig* config) {
  static const PITState zero_pit_state = {0};
  *pit = zero_pit_state;
  pit->config = config;
}

// Helper function to load the counter and handle side effects.
static inline void LoadCounter(PITState* pit, int i) {
  PITTimer* timer = &pit->timers[i];

  // A reload value of 0 is treated as 0x10000 by the hardware.
  // This will wrap to 0 when assigned to the 16-bit counter, which is correct.
  timer->counter = timer->reload_value;

  // If this is channel 2, notify the platform of the new frequency.
  if (i == 2 && pit->config && pit->config->set_pc_speaker_frequency) {
    uint32_t frequency = (timer->reload_value == 0)
                             ? 0
                             : kPITTickFrequencyHz / timer->reload_value;
    pit->config->set_pc_speaker_frequency(pit->config->context, frequency);
  }
}

// Helper function to handle a data write to a channel.
static inline void WriteData(PITState* pit, int i, uint8_t value) {
  PITTimer* timer = &pit->timers[i];

  switch (timer->access_mode) {
    case 1:  // LSB-only
      timer->reload_value = value;
      LoadCounter(pit, i);
      break;
    case 2:  // MSB-only
      timer->reload_value = (uint16_t)value << 8;
      LoadCounter(pit, i);
      break;
    case 3:  // LSB then MSB
      if (!timer->rw_byte_toggle) {
        // LSB
        timer->reload_value = (timer->reload_value & 0xFF00) | value;
        timer->rw_byte_toggle = true;
      } else {
        // MSB
        timer->reload_value =
            (timer->reload_value & 0x00FF) | ((uint16_t)value << 8);
        timer->rw_byte_toggle = false;
        LoadCounter(pit, i);
      }
      break;
  }
}

void PITWritePort(PITState* pit, uint16_t port, uint8_t value) {
  switch (port) {
    case kPITPortControl: {
      uint8_t channel = (value >> 6) & 0x03;
      if (channel > 2) {
        // Invalid channel, or read-back command (not supported).
        return;
      }

      PITTimer* timer = &pit->timers[channel];
      uint8_t access_mode = (value >> 4) & 0x03;

      if (access_mode == 0) {
        // Latch command.
        timer->latch = timer->counter;
        timer->latch_active = true;
      } else {
        // Programming command.
        timer->access_mode = access_mode;
        timer->mode = (value >> 1) & 0x07;
        timer->bcd_mode = (value & 0x01);
        timer->rw_byte_toggle = false;

        // Set initial output state based on mode.
        switch (timer->mode) {
          case 0:
            timer->output_state = false;
            break;
          case 2:
          case 3:
            timer->output_state = true;
            break;
        }
      }
      break;
    }

    case kPITPortChannel0:
    case kPITPortChannel1:
    case kPITPortChannel2:
      WriteData(pit, port - kPITPortChannel0, value);
      break;
    default:
      // Invalid port - ignore.
      break;
  }
}

uint8_t PITReadPort(PITState* pit, uint16_t port) {
  // TODO: Implement PIT read logic.
  return 0;
}

// Helper function to handle common logic on terminal count.
static inline void HandleTerminalCount(PITState* pit, int i) {
  PITTimer* timer = &pit->timers[i];

  // Perform mode-specific actions for output and reloading.
  switch (timer->mode) {
    case 0:  // Mode 0: Interrupt on Terminal Count
      timer->output_state = true;
      // Do not reload.
      break;
    case 2:  // Mode 2: Rate Generator
      timer->output_state = true;
      timer->counter = timer->reload_value;
      break;
    case 3:  // Mode 3: Square Wave Generator
      timer->output_state = !timer->output_state;
      timer->counter = timer->reload_value;
      break;
  }

  if (i == 0 && pit->config && pit->config->raise_irq_0) {
    pit->config->raise_irq_0(pit->config->context);
  }
}

void PITTick(PITState* pit) {
  // Iterate through all three timer channels.
  for (int i = 0; i < kPITNumChannels; ++i) {
    PITTimer* timer = &pit->timers[i];

    // Don't tick a one-shot timer that has already fired.
    if (timer->mode == 0 && timer->counter == 0) {
      continue;
    }

    // Handle the 1-tick low pulse for Mode 2 before decrementing.
    if (timer->mode == 2 && timer->counter == 1) {
      timer->output_state = false;
    }

    // In Mode 3, the counter decrements by 2, otherwise by 1.
    if (timer->mode == 3 && timer->counter >= 2) {
      timer->counter -= 2;
    } else {
      timer->counter--;
    }

    if (timer->counter == 0) {
      HandleTerminalCount(pit, i);
    }
  }
}


// ==============================================================================
// src/pit/pit.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_PIT_BUNDLE_H

