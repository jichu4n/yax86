#include "../util/common.h"
#include "public.h"

enum {
  // Tick frequency of the PIT in Hz.
  kPITTickFrequencyHz = 1193182,
  // Fallback reload value when 0 is written to the counter. The hardware
  // treats a reload value of 0 as 0x10000.
  kPITFallbackReloadValue = 0x10000,
};

// Specifies the behavior of a timer channel in a specific mode (0-5).
typedef struct PITModeMetadata {
  // Initial output state when a timer channel is programmed in this mode.
  bool initial_output_state;
  // Callback to handle a tick for this mode.
  void (*handle_tick)(
      PITState* pit, PITChannelState* channel, int channel_index);
} PITModeMetadata;

// Metadata for unsupported modes (1, 4, 5).
static const PITModeMetadata kPITUnsupportedMode = {0};

// Handles a channel reaching terminal count.
static inline void PITChannelSetOutputState(
    PITState* pit, PITChannelState* channel, int channel_index,
    bool new_output_state) {
  // No-op if the output state is unchanged.
  if (channel->output_state == new_output_state) {
    return;
  }

  // Set the new output state.
  channel->output_state = new_output_state;

  // On rising edge of channel 0 output state, raise IRQ 0.
  if (channel_index == 0 && new_output_state && pit->config &&
      pit->config->raise_irq_0) {
    pit->config->raise_irq_0(pit->config->context);
  }
}

// Tick handler for Mode 0: Interrupt on Terminal Count.
static void PITMode0HandleTick(
    PITState* pit, PITChannelState* channel, int channel_index) {
  // Since this is a one-shot timer, do nothing if the counter is already 0.
  if (channel->counter == 0) {
    return;
  }

  // Decrement the counter by 1.
  --channel->counter;

  // If at terminal count, set output high and trigger terminal count.
  if (channel->counter == 0) {
    PITChannelSetOutputState(pit, channel, channel_index, true);
  }
}

// Metadata for Mode 0: Interrupt on Terminal Count.
static const PITModeMetadata kPITMode0Metadata = {
    .initial_output_state = false,
    .handle_tick = PITMode0HandleTick,
};

// Tick handler for Mode 2: Rate Generator.
static void PITMode2HandleTick(
    PITState* pit, PITChannelState* channel, int channel_index) {
  // Decrement the counter by 1.
  --channel->counter;

  switch (channel->counter) {
    case 1:
      // When the counter reaches 1, set output low for one tick.
      PITChannelSetOutputState(pit, channel, channel_index, false);
      break;
    case 0:
      // When the counter reaches 0, reload, set output high again.
      channel->counter = channel->reload_value;
      PITChannelSetOutputState(pit, channel, channel_index, true);
      break;
    default:
      break;
  }
}

// Metadata for Mode 2: Rate Generator.
static const PITModeMetadata kPITMode2Metadata = {
    .initial_output_state = true,
    .handle_tick = PITMode2HandleTick,
};

// Tick handler for Mode 3: Square Wave Generator.
static void PITMode3HandleTick(
    PITState* pit, PITChannelState* channel, int channel_index) {
  // In Mode 3, the counter decrements by 2 each tick. We reach terminal count
  // when we reach either 0 or wrap around to 0xFFFF.
  channel->counter -= 2;

  switch (channel->counter) {
    case 0:
    case 0xFFFF:
      // When the counter reaches terminal count, reload and toggle output.
      channel->counter = channel->reload_value;
      PITChannelSetOutputState(
          pit, channel, channel_index, !channel->output_state);
      break;
    default:
      break;
  }
}

// Metadata for Mode 3: Square Wave Generator.
static const PITModeMetadata kPITMode3Metadata = {
    .initial_output_state = true,
    .handle_tick = PITMode3HandleTick,
};

// Array of mode metadata indexed by mode number.
static const PITModeMetadata* kPITModeMetadata[kPITNumModes] = {
    &kPITMode0Metadata,    // Mode 0
    &kPITUnsupportedMode,  // Mode 1 (unsupported)
    &kPITMode2Metadata,    // Mode 2
    &kPITMode3Metadata,    // Mode 3
    &kPITUnsupportedMode,  // Mode 4 (unsupported)
    &kPITUnsupportedMode,  // Mode 5 (unsupported)
};

void PITInit(PITState* pit, PITConfig* config) {
  static const PITState zero_pit_state = {0};
  *pit = zero_pit_state;
  pit->config = config;

  // On the IBM PC, the output pins of all three channels are initially pulled
  // high.
  for (int i = 0; i < kPITNumChannels; ++i) {
    pit->channels[i].output_state = true;
  }
}

// Helper function to load the counter and handle side effects.
static inline void PITChannelLoadCounter(
    PITState* pit, PITChannelState* channel, int channel_index) {
  // A reload value of 0 is treated as 0x10000 by the hardware.
  // This will wrap to 0 when assigned to the 16-bit counter.
  channel->counter = channel->reload_value;

  // If this is channel 2, notify the platform of the new PC speaker frequency.
  if (channel_index == 2 && pit->config &&
      pit->config->set_pc_speaker_frequency) {
    uint32_t frequency =
        kPITTickFrequencyHz / (channel->reload_value ? channel->reload_value
                                                     : kPITFallbackReloadValue);
    pit->config->set_pc_speaker_frequency(pit->config->context, frequency);
  }
}

// Helper function to handle a write to a channel's data port.
static inline void PITChannelWritePort(
    PITState* pit, PITChannelState* channel, int channel_index, uint8_t value) {
  switch (channel->access_mode) {
    case kPITAccessLatch:
      // If latch command, ignore data writes.
      break;
    case kPITAccessLSBOnly:
      channel->reload_value = (channel->reload_value & 0xFF00) | value;
      PITChannelLoadCounter(pit, channel, channel_index);
      break;
    case kPITAccessMSBOnly:
      channel->reload_value =
          (channel->reload_value & 0x00FF) | ((uint16_t)value << 8);
      PITChannelLoadCounter(pit, channel, channel_index);
      break;
    case kPITAccessLSBThenMSB:
      switch (channel->rw_byte) {
        case kPITByteLSB:
          // LSB
          channel->reload_value = (channel->reload_value & 0xFF00) | value;
          channel->rw_byte = kPITByteMSB;
          break;
        case kPITByteMSB:
          // MSB
          channel->reload_value =
              (channel->reload_value & 0x00FF) | ((uint16_t)value << 8);
          channel->rw_byte = kPITByteLSB;
          PITChannelLoadCounter(pit, channel, channel_index);
          break;
        default:
          // Should not happen - ignore.
          break;
      }
      break;
    default:
      // Invalid access mode - ignore.
      break;
  }
}

void PITWritePort(PITState* pit, uint16_t port, uint8_t value) {
  switch (port) {
    case kPITPortControl: {
      // Control word.
      int channel_index = (value >> 6) & 0x03;
      if (channel_index >= kPITNumChannels) {
        // Invalid channel, or read-back command (not supported).
        return;
      }
      PITChannelState* channel = &pit->channels[channel_index];

      PITAccessMode access_mode = (PITAccessMode)((value >> 4) & 0x03);
      if (access_mode == kPITAccessLatch) {
        // Latch command.
        channel->latch = channel->counter;
        channel->latch_active = true;
      } else {
        // Programming command.
        channel->access_mode = access_mode;
        channel->mode = (value >> 1) & 0x07;
        if (channel->mode >= kPITNumModes) {
          // Modes 6 and 7 are equivalent to modes 2 and 3.
          channel->mode -= 4;
        }
        channel->rw_byte = kPITByteLSB;
        PITChannelSetOutputState(
            pit, channel, channel_index,
            kPITModeMetadata[channel->mode]->initial_output_state);
      }
      break;
    }
    case kPITPortChannel0:
    case kPITPortChannel1:
    case kPITPortChannel2: {
      // Data port for a channel.
      int channel_index = port - kPITPortChannel0;
      PITChannelState* channel = &pit->channels[channel_index];
      PITChannelWritePort(pit, channel, channel_index, value);
      break;
    }
    default:
      // Invalid port - ignore.
      break;
  }
}

// Helper function to handle a read from a channel's data port.
static inline uint8_t PITChannelReadPort(
    YAX86_UNUSED PITState* pit, PITChannelState* channel,
    YAX86_UNUSED int channel_index) {
  uint16_t value = channel->latch_active ? channel->latch : channel->counter;
  uint8_t result = 0;

  switch (channel->access_mode) {
    case kPITAccessLatch:
      // This is a command, not a persistent access mode. Ignore.
      break;
    case kPITAccessLSBOnly:
      result = value & 0xFF;
      channel->latch_active = false;
      break;
    case kPITAccessMSBOnly:
      result = (value >> 8) & 0xFF;
      channel->latch_active = false;
      break;
    case kPITAccessLSBThenMSB:
      switch (channel->rw_byte) {
        case kPITByteLSB:
          result = value & 0xFF;
          channel->rw_byte = kPITByteMSB;
          break;
        case kPITByteMSB:
          result = (value >> 8) & 0xFF;
          channel->rw_byte = kPITByteLSB;
          // The full value has been read, so deactivate the latch.
          channel->latch_active = false;
          break;
        default:
          // Should not happen.
          break;
      }
      break;
    default:
      // Invalid access mode.
      break;
  }
  return result;
}

uint8_t PITReadPort(PITState* pit, uint16_t port) {
  switch (port) {
    case kPITPortChannel0:
    case kPITPortChannel1:
    case kPITPortChannel2: {
      // Data port for a channel.
      int channel_index = port - kPITPortChannel0;
      PITChannelState* channel = &pit->channels[channel_index];
      return PITChannelReadPort(pit, channel, channel_index);
    }
    default:
      // Invalid port - return 0xFF as is common for reads from unused ports.
      return 0xFF;
  }
}

void PITTick(PITState* pit) {
  PITChannelState* channel = &pit->channels[0];
  for (int i = 0; i < kPITNumChannels; ++i, ++channel) {
    if (channel->mode >= kPITNumModes) {
      // Invalid mode - ignore.
      continue;
    }
    const PITModeMetadata* mode_metadata = kPITModeMetadata[channel->mode];
    if (mode_metadata->handle_tick) {
      mode_metadata->handle_tick(pit, channel, i);
    }
  }
}
