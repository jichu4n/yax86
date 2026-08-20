#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

enum {
  // Tick frequency of the PIT in Hz.
  kPITTickFrequencyHz = 1193182,
  // Fallback reload value when 0 is written to the counter. The hardware
  // treats a reload value of 0 as 0x10000.
  kPITFallbackReloadValue = 0x10000,
  // The channel wired to the PC speaker.
  kPITSpeakerChannel = 2,
};

// Specifies the behavior of a timer channel in a specific mode (0-5).
typedef struct PITModeMetadata {
  // Initial output state when a timer channel is programmed in this mode.
  bool initial_output_state;
  // Callback to handle a tick for this mode.
  void (*handle_tick)(
      PITState* pit, PITChannelState* channel, int channel_index);
  // How much the counter moves per tick. Modes 0 and 2 count down by one;
  // mode 3 counts down by two so that its output is a square wave.
  uint16_t counter_step;
  // How many ticks the counter can be advanced by arithmetic before the next
  // tick could change the channel's output, and how many ticks until that
  // happens. Both may be NULL for a mode that never changes its output.
  //
  // skip_ticks returns how many ticks may be applied to the counter with no
  // observable effect other than the counter's own value; ticks_until_event
  // returns a lower bound on the ticks until the output could change.
  uint32_t (*skip_ticks)(const PITChannelState* channel);
  uint32_t (*ticks_until_event)(const PITChannelState* channel);
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
  //
  // This is the only effect any channel's output has outside the PIT, which is
  // why PITTicksUntilNextEvent() schedules deadlines for channel 0 alone. Give
  // another channel's output an effect here and that has to change with it.
  if (channel_index == kPITChannelTimer && new_output_state && pit->config &&
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

// A counter above 1 cannot reach terminal count on the next tick, so every
// tick down to 1 is uneventful. A counter already at 0 has finished and never
// changes again, which the caller detects as no event rather than a skip.
static uint32_t PITMode0SkipTicks(const PITChannelState* channel) {
  return channel->counter > 1 ? (uint32_t)(channel->counter - 1) : 0;
}

static uint32_t PITMode0TicksUntilEvent(const PITChannelState* channel) {
  // A one-shot that has already fired is not counting towards anything.
  return channel->counter == 0 ? kPITNoEvent : (uint32_t)channel->counter;
}

// Metadata for Mode 0: Interrupt on Terminal Count.
static const PITModeMetadata kPITMode0Metadata = {
    .initial_output_state = false,
    .handle_tick = PITMode0HandleTick,
    .counter_step = 1,
    .skip_ticks = PITMode0SkipTicks,
    .ticks_until_event = PITMode0TicksUntilEvent,
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

// Mode 2 acts when the counter reaches 1 and again when it reaches 0, so every
// tick down to 2 is uneventful.
static uint32_t PITMode2SkipTicks(const PITChannelState* channel) {
  return channel->counter > 2 ? (uint32_t)(channel->counter - 2) : 0;
}

static uint32_t PITMode2TicksUntilEvent(const PITChannelState* channel) {
  // A counter of 0 wraps to 0xFFFF on the next tick without changing the
  // output, but reporting 1 only costs a wasted wakeup.
  return channel->counter > 1 ? (uint32_t)(channel->counter - 1) : 1;
}

// Metadata for Mode 2: Rate Generator.
static const PITModeMetadata kPITMode2Metadata = {
    .initial_output_state = true,
    .handle_tick = PITMode2HandleTick,
    .counter_step = 1,
    .skip_ticks = PITMode2SkipTicks,
    .ticks_until_event = PITMode2TicksUntilEvent,
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

// Mode 3 steps by two, so terminal count is reached at 0 from an even counter
// and at 0xFFFF from an odd one. Either way a counter of 4 or more has at
// least one uneventful step left, and stopping at 2 or 3 leaves the next step
// to the tick handler.
static uint32_t PITMode3SkipTicks(const PITChannelState* channel) {
  return channel->counter >= 4 ? (uint32_t)((channel->counter - 2) / 2) : 0;
}

static uint32_t PITMode3TicksUntilEvent(const PITChannelState* channel) {
  // Even counters reach 0 after counter/2 steps, odd ones reach 0xFFFF after
  // (counter+1)/2; the rounding covers both.
  const uint32_t ticks = ((uint32_t)channel->counter + 1) / 2;
  return ticks > 0 ? ticks : 1;
}

// Metadata for Mode 3: Square Wave Generator.
static const PITModeMetadata kPITMode3Metadata = {
    .initial_output_state = true,
    .handle_tick = PITMode3HandleTick,
    .counter_step = 2,
    .skip_ticks = PITMode3SkipTicks,
    .ticks_until_event = PITMode3TicksUntilEvent,
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

// Whether a channel in this mode drives its output at the reload frequency.
// Only modes 2 and 3 oscillate; the others produce a single edge, which a
// frequency has no way to express.
static inline bool PITModeOscillates(uint8_t mode) {
  return mode == 2 || mode == 3;
}

// Reports channel 2's tone frequency to the host, or 0 if it is not producing
// one. Modes other than 2 and 3 are silent, as is a channel that has been
// reprogrammed but not yet given a count - the hardware does not start
// counting until the count is written.
//
// The frequency describes only how often the output oscillates. Mode 2's
// narrow output pulse sounds thinner than mode 3's square wave, which a
// frequency cannot express.
static inline void PITNotifySpeakerFrequency(
    PITState* pit, const PITChannelState* channel, int channel_index,
    bool has_count) {
  if (channel_index != kPITSpeakerChannel || !pit->config ||
      !pit->config->set_pc_speaker_frequency) {
    return;
  }
  uint32_t frequency = 0;
  if (has_count && PITModeOscillates(channel->mode)) {
    frequency =
        kPITTickFrequencyHz / (channel->reload_value ? channel->reload_value
                                                     : kPITFallbackReloadValue);
  }
  pit->config->set_pc_speaker_frequency(pit->config->context, frequency);
}

// Helper function to load the counter and handle side effects.
static inline void PITChannelLoadCounter(
    PITState* pit, PITChannelState* channel, int channel_index) {
  // A reload value of 0 is treated as 0x10000 by the hardware.
  // This will wrap to 0 when assigned to the 16-bit counter.
  channel->counter = channel->reload_value;

  PITNotifySpeakerFrequency(pit, channel, channel_index, true);
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
        // A reprogrammed channel is silent until its count arrives, so a tone
        // playing on channel 2 stops here and resumes on the next counter
        // load.
        PITNotifySpeakerFrequency(pit, channel, channel_index, false);
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

// Advances a single channel by num_ticks.
//
// The result is identical to running the channel's tick handler num_ticks
// times. Stretches of the count where no tick can change the output are
// applied to the counter arithmetically; every tick that could change it still
// goes through the handler, so there is only one description of what a tick
// does.
static void PITAdvanceChannel(
    PITState* pit, PITChannelState* channel, int channel_index,
    uint32_t num_ticks) {
  if (channel->mode >= kPITNumModes) {
    // Invalid mode - ignore.
    return;
  }
  const PITModeMetadata* mode_metadata = kPITModeMetadata[channel->mode];
  if (!mode_metadata->handle_tick) {
    // A mode that does nothing on a tick cannot be moved on by one.
    return;
  }

  while (num_ticks > 0) {
    if (mode_metadata->skip_ticks) {
      uint32_t skip = mode_metadata->skip_ticks(channel);
      if (skip > num_ticks) {
        skip = num_ticks;
      }
      if (skip > 0) {
        channel->counter -= (uint16_t)(skip * mode_metadata->counter_step);
        num_ticks -= skip;
        continue;
      }
    }
    // The next tick could change the output, so run it properly. A mode that
    // has stopped counting reports no event and no skip, which would spin here
    // for the rest of num_ticks, so stop once nothing further can happen.
    if (mode_metadata->ticks_until_event &&
        mode_metadata->ticks_until_event(channel) == kPITNoEvent) {
      return;
    }
    mode_metadata->handle_tick(pit, channel, channel_index);
    --num_ticks;
  }
}

void PITAdvance(PITState* pit, uint32_t num_ticks) {
  if (num_ticks == 0) {
    return;
  }
  PITChannelState* channel = &pit->channels[0];
  for (int i = 0; i < kPITNumChannels; ++i, ++channel) {
    PITAdvanceChannel(pit, channel, i, num_ticks);
  }
}

uint32_t PITTicksUntilNextEvent(const PITState* pit) {
  uint32_t earliest = kPITNoEvent;
  const PITChannelState* channel = &pit->channels[0];
  for (int i = 0; i < kPITNumChannels; ++i, ++channel) {
    // Only channel 0 is worth waking up for. Its output is the one thing that
    // leaves the chip - PITChannelSetOutputState() raises IRQ 0 from it - while
    // channels 1 and 2 do nothing on a transition but record it, and that
    // record is recomputed from scratch whenever the PIT is advanced. Every
    // path that reads it advances the PIT first, so there is no state to miss.
    //
    // This is not hypothetical tidiness: the BIOS leaves channel 2 programmed
    // after the POST beep with the speaker gated off, asking to be woken every
    // 678 ticks for an output nobody is listening to.
    if (i != kPITChannelTimer) {
      continue;
    }
    if (channel->mode >= kPITNumModes) {
      continue;
    }
    const PITModeMetadata* mode_metadata = kPITModeMetadata[channel->mode];
    if (!mode_metadata->ticks_until_event) {
      continue;
    }
    const uint32_t ticks = mode_metadata->ticks_until_event(channel);
    if (ticks < earliest) {
      earliest = ticks;
    }
  }
  return earliest;
}
