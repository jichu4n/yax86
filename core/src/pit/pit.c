#ifndef Y_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // Y_IMPLEMENTATION

enum {
  // Tick frequency of the PIT in Hz.
  Y_PIT_TICK_FREQUENCY_HZ = 1193182,
  // Fallback reload value when 0 is written to the counter. The hardware
  // treats a reload value of 0 as 0x10000.
  Y_PIT_FALLBACK_RELOAD_VALUE = 0x10000,
  // The channel wired to the PC speaker.
  Y_PIT_SPEAKER_CHANNEL = 2,
};

// Specifies the behavior of a timer channel in a specific mode (0-5).
typedef struct YPITModeMetadata {
  // Initial output state when a timer channel is programmed in this mode.
  bool initial_output_state;
  // Callback to handle a tick for this mode.
  void (*handle_tick)(
      YPITState* pit, YPITChannelState* channel, int channel_index);
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
  uint32_t (*skip_ticks)(const YPITChannelState* channel);
  uint32_t (*ticks_until_event)(const YPITChannelState* channel);
} YPITModeMetadata;

// Metadata for unsupported modes (1, 4, 5).
Y_FILE_PRIVATE const YPITModeMetadata YPITUnsupportedMode = {0};

// Handles a channel reaching terminal count.
Y_FILE_PRIVATE inline void YPITChannelSetOutputState(
    YPITState* pit, YPITChannelState* channel, int channel_index,
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
  // why YPITTicksUntilNextEvent() schedules deadlines for channel 0 alone. Give
  // another channel's output an effect here and that has to change with it.
  if (channel_index == Y_PIT_CHANNEL_TIMER && new_output_state &&
      pit->config.raise_irq_0) {
    pit->config.raise_irq_0(pit->config.context);
  }
}

// Tick handler for Mode 0: Interrupt on Terminal Count.
Y_FILE_PRIVATE void YPITMode0HandleTick(
    YPITState* pit, YPITChannelState* channel, int channel_index) {
  // Since this is a one-shot timer, do nothing if the counter is already 0.
  if (channel->counter == 0) {
    return;
  }

  // Decrement the counter by 1.
  --channel->counter;

  // If at terminal count, set output high and trigger terminal count.
  if (channel->counter == 0) {
    YPITChannelSetOutputState(pit, channel, channel_index, true);
  }
}

// A counter above 1 cannot reach terminal count on the next tick, so every
// tick down to 1 is uneventful. A counter already at 0 has finished and never
// changes again, which the caller detects as no event rather than a skip.
Y_FILE_PRIVATE uint32_t YPITMode0SkipTicks(const YPITChannelState* channel) {
  return channel->counter > 1 ? (uint32_t)(channel->counter - 1) : 0;
}

Y_FILE_PRIVATE uint32_t
YPITMode0TicksUntilEvent(const YPITChannelState* channel) {
  // A one-shot that has already fired is not counting towards anything.
  return channel->counter == 0 ? Y_PIT_NO_EVENT : (uint32_t)channel->counter;
}

// Metadata for Mode 0: Interrupt on Terminal Count.
Y_FILE_PRIVATE const YPITModeMetadata YPITMode0Metadata = {
    .initial_output_state = false,
    .handle_tick = YPITMode0HandleTick,
    .counter_step = 1,
    .skip_ticks = YPITMode0SkipTicks,
    .ticks_until_event = YPITMode0TicksUntilEvent,
};

// Tick handler for Mode 2: Rate Generator.
Y_FILE_PRIVATE void YPITMode2HandleTick(
    YPITState* pit, YPITChannelState* channel, int channel_index) {
  // Decrement the counter by 1.
  --channel->counter;

  switch (channel->counter) {
    case 1:
      // When the counter reaches 1, set output low for one tick.
      YPITChannelSetOutputState(pit, channel, channel_index, false);
      break;
    case 0:
      // When the counter reaches 0, reload, set output high again.
      channel->counter = channel->reload_value;
      YPITChannelSetOutputState(pit, channel, channel_index, true);
      break;
    default:
      break;
  }
}

// Mode 2 acts when the counter reaches 1 and again when it reaches 0, so every
// tick down to 2 is uneventful.
Y_FILE_PRIVATE uint32_t YPITMode2SkipTicks(const YPITChannelState* channel) {
  return channel->counter > 2 ? (uint32_t)(channel->counter - 2) : 0;
}

Y_FILE_PRIVATE Y_HOT uint32_t
YPITMode2TicksUntilEvent(const YPITChannelState* channel) {
  // A counter of 0 wraps to 0xFFFF on the next tick without changing the
  // output, but reporting 1 only costs a wasted wakeup.
  return channel->counter > 1 ? (uint32_t)(channel->counter - 1) : 1;
}

// Metadata for Mode 2: Rate Generator.
Y_FILE_PRIVATE const YPITModeMetadata YPITMode2Metadata = {
    .initial_output_state = true,
    .handle_tick = YPITMode2HandleTick,
    .counter_step = 1,
    .skip_ticks = YPITMode2SkipTicks,
    .ticks_until_event = YPITMode2TicksUntilEvent,
};

// Tick handler for Mode 3: Square Wave Generator.
Y_FILE_PRIVATE void YPITMode3HandleTick(
    YPITState* pit, YPITChannelState* channel, int channel_index) {
  // In Mode 3, the counter decrements by 2 each tick. We reach terminal count
  // when we reach either 0 or wrap around to 0xFFFF.
  channel->counter -= 2;

  switch (channel->counter) {
    case 0:
    case 0xFFFF:
      // When the counter reaches terminal count, reload and toggle output.
      channel->counter = channel->reload_value;
      YPITChannelSetOutputState(
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
Y_FILE_PRIVATE Y_HOT uint32_t
YPITMode3SkipTicks(const YPITChannelState* channel) {
  return channel->counter >= 4 ? (uint32_t)((channel->counter - 2) / 2) : 0;
}

Y_FILE_PRIVATE uint32_t
YPITMode3TicksUntilEvent(const YPITChannelState* channel) {
  // Even counters reach 0 after counter/2 steps, odd ones reach 0xFFFF after
  // (counter+1)/2; the rounding covers both.
  const uint32_t ticks = ((uint32_t)channel->counter + 1) / 2;
  return ticks > 0 ? ticks : 1;
}

// Metadata for Mode 3: Square Wave Generator.
Y_FILE_PRIVATE const YPITModeMetadata YPITMode3Metadata = {
    .initial_output_state = true,
    .handle_tick = YPITMode3HandleTick,
    .counter_step = 2,
    .skip_ticks = YPITMode3SkipTicks,
    .ticks_until_event = YPITMode3TicksUntilEvent,
};

// Array of mode metadata indexed by mode number.
Y_FILE_PRIVATE const YPITModeMetadata* YPITModeMetadataTable[Y_PIT_NUM_MODES] =
    {
        &YPITMode0Metadata,    // Mode 0
        &YPITUnsupportedMode,  // Mode 1 (unsupported)
        &YPITMode2Metadata,    // Mode 2
        &YPITMode3Metadata,    // Mode 3
        &YPITUnsupportedMode,  // Mode 4 (unsupported)
        &YPITUnsupportedMode,  // Mode 5 (unsupported)
};

Y_PUBLIC void YPITInit(YPITState* pit) {
  // On the IBM PC, the output pins of all three channels are initially pulled
  // high.
  for (int i = 0; i < Y_PIT_NUM_CHANNELS; ++i) {
    pit->channels[i].output_state = true;
  }
}

// Whether a channel in this mode drives its output at the reload frequency.
// Only modes 2 and 3 oscillate; the others produce a single edge, which a
// frequency has no way to express.
Y_FILE_PRIVATE inline bool YPITModeOscillates(uint8_t mode) {
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
Y_FILE_PRIVATE inline void YPITNotifySpeakerFrequency(
    YPITState* pit, const YPITChannelState* channel, int channel_index,
    bool has_count) {
  if (channel_index != Y_PIT_SPEAKER_CHANNEL ||
      !pit->config.set_pc_speaker_frequency) {
    return;
  }
  uint32_t frequency = 0;
  if (has_count && YPITModeOscillates(channel->mode)) {
    frequency = Y_PIT_TICK_FREQUENCY_HZ / (channel->reload_value
                                               ? channel->reload_value
                                               : Y_PIT_FALLBACK_RELOAD_VALUE);
  }
  pit->config.set_pc_speaker_frequency(pit->config.context, frequency);
}

// Helper function to load the counter and handle side effects.
Y_FILE_PRIVATE inline void YPITChannelLoadCounter(
    YPITState* pit, YPITChannelState* channel, int channel_index) {
  // A reload value of 0 is treated as 0x10000 by the hardware.
  // This will wrap to 0 when assigned to the 16-bit counter.
  channel->counter = channel->reload_value;

  YPITNotifySpeakerFrequency(pit, channel, channel_index, true);
}

// Helper function to handle a write to a channel's data port.
Y_FILE_PRIVATE inline void YPITChannelWritePort(
    YPITState* pit, YPITChannelState* channel, int channel_index,
    uint8_t value) {
  switch (channel->access_mode) {
    case Y_PIT_ACCESS_LATCH:
      // If latch command, ignore data writes.
      break;
    case Y_PIT_ACCESS_LSB_ONLY:
      channel->reload_value = (channel->reload_value & 0xFF00) | value;
      YPITChannelLoadCounter(pit, channel, channel_index);
      break;
    case Y_PIT_ACCESS_MSB_ONLY:
      channel->reload_value =
          (channel->reload_value & 0x00FF) | ((uint16_t)value << 8);
      YPITChannelLoadCounter(pit, channel, channel_index);
      break;
    case Y_PIT_ACCESS_LSB_THEN_MSB:
      switch (channel->rw_byte) {
        case Y_PIT_BYTE_LSB:
          // LSB
          channel->reload_value = (channel->reload_value & 0xFF00) | value;
          channel->rw_byte = Y_PIT_BYTE_MSB;
          break;
        case Y_PIT_BYTE_MSB:
          // MSB
          channel->reload_value =
              (channel->reload_value & 0x00FF) | ((uint16_t)value << 8);
          channel->rw_byte = Y_PIT_BYTE_LSB;
          YPITChannelLoadCounter(pit, channel, channel_index);
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

Y_PUBLIC void YPITWritePort(YPITState* pit, uint16_t port, uint8_t value) {
  switch (port) {
    case Y_PIT_PORT_CONTROL: {
      // Control word.
      int channel_index = (value >> 6) & 0x03;
      if (channel_index >= Y_PIT_NUM_CHANNELS) {
        // Invalid channel, or read-back command (not supported).
        return;
      }
      YPITChannelState* channel = &pit->channels[channel_index];

      YPITAccessMode access_mode = (YPITAccessMode)((value >> 4) & 0x03);
      if (access_mode == Y_PIT_ACCESS_LATCH) {
        // Latch command.
        channel->latch = channel->counter;
        channel->latch_active = true;
      } else {
        // Programming command.
        channel->access_mode = access_mode;
        channel->mode = (value >> 1) & 0x07;
        if (channel->mode >= Y_PIT_NUM_MODES) {
          // Modes 6 and 7 are equivalent to modes 2 and 3.
          channel->mode -= 4;
        }
        channel->rw_byte = Y_PIT_BYTE_LSB;
        YPITChannelSetOutputState(
            pit, channel, channel_index,
            YPITModeMetadataTable[channel->mode]->initial_output_state);
        // A reprogrammed channel is silent until its count arrives, so a tone
        // playing on channel 2 stops here and resumes on the next counter
        // load.
        YPITNotifySpeakerFrequency(pit, channel, channel_index, false);
      }
      break;
    }
    case Y_PIT_PORT_CHANNEL0:
    case Y_PIT_PORT_CHANNEL1:
    case Y_PIT_PORT_CHANNEL2: {
      // Data port for a channel.
      int channel_index = port - Y_PIT_PORT_CHANNEL0;
      YPITChannelState* channel = &pit->channels[channel_index];
      YPITChannelWritePort(pit, channel, channel_index, value);
      break;
    }
    default:
      // Invalid port - ignore.
      break;
  }
}

// Helper function to handle a read from a channel's data port.
Y_FILE_PRIVATE inline uint8_t YPITChannelReadPort(
    Y_UNUSED YPITState* pit, YPITChannelState* channel,
    Y_UNUSED int channel_index) {
  uint16_t value = channel->latch_active ? channel->latch : channel->counter;
  uint8_t result = 0;

  switch (channel->access_mode) {
    case Y_PIT_ACCESS_LATCH:
      // This is a command, not a persistent access mode. Ignore.
      break;
    case Y_PIT_ACCESS_LSB_ONLY:
      result = value & 0xFF;
      channel->latch_active = false;
      break;
    case Y_PIT_ACCESS_MSB_ONLY:
      result = (value >> 8) & 0xFF;
      channel->latch_active = false;
      break;
    case Y_PIT_ACCESS_LSB_THEN_MSB:
      switch (channel->rw_byte) {
        case Y_PIT_BYTE_LSB:
          result = value & 0xFF;
          channel->rw_byte = Y_PIT_BYTE_MSB;
          break;
        case Y_PIT_BYTE_MSB:
          result = (value >> 8) & 0xFF;
          channel->rw_byte = Y_PIT_BYTE_LSB;
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

Y_PUBLIC uint8_t YPITReadPort(YPITState* pit, uint16_t port) {
  switch (port) {
    case Y_PIT_PORT_CHANNEL0:
    case Y_PIT_PORT_CHANNEL1:
    case Y_PIT_PORT_CHANNEL2: {
      // Data port for a channel.
      int channel_index = port - Y_PIT_PORT_CHANNEL0;
      YPITChannelState* channel = &pit->channels[channel_index];
      return YPITChannelReadPort(pit, channel, channel_index);
    }
    default:
      // Invalid port - return 0xFF as is common for reads from unused ports.
      return 0xFF;
  }
}

Y_PUBLIC Y_HOT void YPITTick(YPITState* pit) {
  YPITChannelState* channel = &pit->channels[0];
  for (int i = 0; i < Y_PIT_NUM_CHANNELS; ++i, ++channel) {
    if (channel->mode >= Y_PIT_NUM_MODES) {
      // Invalid mode - ignore.
      continue;
    }
    const YPITModeMetadata* mode_metadata =
        YPITModeMetadataTable[channel->mode];
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
Y_FILE_PRIVATE void YPITAdvanceChannel(
    YPITState* pit, YPITChannelState* channel, int channel_index,
    uint32_t num_ticks) {
  if (channel->mode >= Y_PIT_NUM_MODES) {
    // Invalid mode - ignore.
    return;
  }
  const YPITModeMetadata* mode_metadata = YPITModeMetadataTable[channel->mode];
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
        mode_metadata->ticks_until_event(channel) == Y_PIT_NO_EVENT) {
      return;
    }
    mode_metadata->handle_tick(pit, channel, channel_index);
    --num_ticks;
  }
}

Y_PUBLIC Y_HOT void YPITAdvance(YPITState* pit, uint32_t num_ticks) {
  if (num_ticks == 0) {
    return;
  }
  YPITChannelState* channel = &pit->channels[0];
  for (int i = 0; i < Y_PIT_NUM_CHANNELS; ++i, ++channel) {
    YPITAdvanceChannel(pit, channel, i, num_ticks);
  }
}

Y_PUBLIC uint32_t YPITTicksUntilNextEvent(const YPITState* pit) {
  uint32_t earliest = Y_PIT_NO_EVENT;
  const YPITChannelState* channel = &pit->channels[0];
  for (int i = 0; i < Y_PIT_NUM_CHANNELS; ++i, ++channel) {
    // Only channel 0 is worth waking up for. Its output is the one thing that
    // leaves the chip - YPITChannelSetOutputState() raises IRQ 0 from it -
    // while channels 1 and 2 do nothing on a transition but record it, and that
    // record is recomputed from scratch whenever the PIT is advanced. Every
    // path that reads it advances the PIT first, so there is no state to miss.
    //
    // This is not hypothetical tidiness: the BIOS leaves channel 2 programmed
    // after the POST beep with the speaker gated off, asking to be woken every
    // 678 ticks for an output nobody is listening to.
    if (i != Y_PIT_CHANNEL_TIMER) {
      continue;
    }
    if (channel->mode >= Y_PIT_NUM_MODES) {
      continue;
    }
    const YPITModeMetadata* mode_metadata =
        YPITModeMetadataTable[channel->mode];
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
