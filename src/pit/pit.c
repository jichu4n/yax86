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
