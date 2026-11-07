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

#ifndef YAX86_PIT_BUNDLE_H
#include "../util/log.h"
#endif  // YAX86_PIT_BUNDLE_H

enum {
  // Log module ID for the PIT.
  Y_PIT_LOG_MODULE_ID = 3,
};

// Log module for the PIT.
Y_PUBLIC_HEADER const YLogModule YPITLogModule = {
    .id = Y_PIT_LOG_MODULE_ID,
    .name = "PIT",
};

enum {
  // Number of PIT channels.
  Y_PIT_NUM_CHANNELS = 3,

  // Channel 0 drives IRQ 0. Channel 1 was DRAM refresh on a real machine and
  // channel 2 the PC speaker; neither has an effect here beyond its recorded
  // output state.
  Y_PIT_CHANNEL_TIMER = 0,
  // Total number of operating modes (0-5).
  // We only implement modes 0, 2, and 3.
  Y_PIT_NUM_MODES = 6,
};

// I/O ports exposed by the PIT.
typedef enum YPITPort {
  // Data port for PIT channel 0
  Y_PIT_PORT_CHANNEL0 = 0x40,
  // Data port for PIT channel 1
  Y_PIT_PORT_CHANNEL1 = 0x41,
  // Data port for PIT channel 2
  Y_PIT_PORT_CHANNEL2 = 0x42,
  // Control word port
  Y_PIT_PORT_CONTROL = 0x43,
} YPITPort;

// Channel read/write access modes. This corresponds to bits 4-5 of the control
// word written to port 0x43.
typedef enum YPITAccessMode {
  // Latch count value command
  Y_PIT_ACCESS_LATCH = 0,
  // Read/write lower byte only
  Y_PIT_ACCESS_LSB_ONLY = 1,
  // Read/write upper byte only
  Y_PIT_ACCESS_MSB_ONLY = 2,
  // Read/write lower byte then upper byte
  Y_PIT_ACCESS_LSB_THEN_MSB = 3,
} YPITAccessMode;

// Which byte to read/write next when in mode Y_PIT_ACCESS_LSB_THEN_MSB.
typedef enum YPITByte {
  Y_PIT_BYTE_LSB = 0,
  Y_PIT_BYTE_MSB = 1,
} YPITByte;

struct YPITState;

// Caller-provided runtime configuration for the PIT.
typedef struct YPITConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger for this module. May be NULL.
  YLogger* logger;

  // Callback to raise IRQ 0.
  void (*raise_irq_0)(void* context);

  // Callback to set PC speaker frequency in Hz, or 0 when channel 2 is not
  // producing a tone.
  //
  // This reports current state, not a stream of events. Programming a channel
  // takes several port writes, so an intermediate 0 can be reported
  // microseconds before the new frequency - a host that keeps only the latest
  // value hears one continuous tone, while a host that queues every value
  // hears a gap.
  void (*set_pc_speaker_frequency)(void* context, uint32_t frequency_hz);
} YPITConfig;

// State of a single PIT timer channel.
typedef struct YPITChannelState {
  // The 16-bit counter value.
  uint16_t counter;
  // The 16-bit latched value for reading.
  uint16_t latch;
  // The 16-bit reload value.
  uint16_t reload_value;
  // The operating mode (0-5).
  uint8_t mode;
  // The read/write access mode.
  YPITAccessMode access_mode;
  // The current output state of the channel.
  bool output_state;
  // Which byte to read/write next when in mode Y_PIT_ACCESS_LSB_THEN_MSB.
  YPITByte rw_byte;
  // Whether a latch command is active.
  bool latch_active;
} YPITChannelState;

// State of the PIT.
typedef struct YPITState {
  // Pointer to the PIT configuration.
  YPITConfig config;

  // The three timer channels.
  YPITChannelState channels[Y_PIT_NUM_CHANNELS];
} YPITState;

// Initializes the PIT to its power-on state.
Y_PUBLIC void YPITInit(YPITState* pit);

// Handles reads from the PIT's I/O ports (0x40-0x42).
Y_PUBLIC uint8_t YPITReadPort(YPITState* pit, uint16_t port);

// Handles writes to the PIT's I/O ports (0x40-0x43).
Y_PUBLIC void YPITWritePort(YPITState* pit, uint16_t port, uint8_t value);

// Simulates a single tick of the PIT's input clock. This method should be
// invoked at a frequency of 1.193182 MHz for accurate timing.
Y_PUBLIC void YPITTick(YPITState* pit);

enum {
  // Returned by YPITTicksUntilNextEvent() when no channel is counting towards
  // anything, so there is nothing to schedule.
  Y_PIT_NO_EVENT = 0x7FFFFFFF,
};

// Advances the PIT by num_ticks of its input clock. Equivalent to calling
// YPITTick() num_ticks times, but does not take time proportional to num_ticks.
//
// A counter spends nearly all of its life counting down through values that
// change nothing observable, so those stretches are skipped arithmetically and
// only the ticks that can change a channel's output are simulated one at a
// time.
Y_PUBLIC void YPITAdvance(YPITState* pit, uint32_t num_ticks);

// Returns the number of input clock ticks until the earliest tick that could
// change any channel's output state, or Y_PIT_NO_EVENT if no channel is
// counting towards one.
//
// This is a lower bound rather than an exact answer. A caller that advances the
// PIT by this much and finds nothing happened has only done unnecessary work,
// whereas one that waited longer would miss an edge.
Y_PUBLIC uint32_t YPITTicksUntilNextEvent(const YPITState* pit);

#endif  // YAX86_PIT_PUBLIC_H
