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
  kLogModuleIDPIT = 3,
};

// Log module for the PIT.
YAX86_PUBLIC_HEADER const YLogModule kLogModulePIT = {
    .id = kLogModuleIDPIT,
    .name = "PIT",
};

enum {
  // Number of PIT channels.
  kPITNumChannels = 3,

  // Channel 0 drives IRQ 0. Channel 1 was DRAM refresh on a real machine and
  // channel 2 the PC speaker; neither has an effect here beyond its recorded
  // output state.
  kPITChannelTimer = 0,
  // Total number of operating modes (0-5).
  // We only implement modes 0, 2, and 3.
  kPITNumModes = 6,
};

// I/O ports exposed by the PIT.
typedef enum YPITPort {
  // Data port for PIT channel 0
  kPITPortChannel0 = 0x40,
  // Data port for PIT channel 1
  kPITPortChannel1 = 0x41,
  // Data port for PIT channel 2
  kPITPortChannel2 = 0x42,
  // Control word port
  kPITPortControl = 0x43,
} YPITPort;

// Channel read/write access modes. This corresponds to bits 4-5 of the control
// word written to port 0x43.
typedef enum YPITAccessMode {
  // Latch count value command
  kPITAccessLatch = 0,
  // Read/write lower byte only
  kPITAccessLSBOnly = 1,
  // Read/write upper byte only
  kPITAccessMSBOnly = 2,
  // Read/write lower byte then upper byte
  kPITAccessLSBThenMSB = 3,
} YPITAccessMode;

// Which byte to read/write next when in mode kPITAccessLSBThenMSB.
typedef enum YPITByte {
  kPITByteLSB = 0,
  kPITByteMSB = 1,
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
  // Which byte to read/write next when in mode kPITAccessLSBThenMSB.
  YPITByte rw_byte;
  // Whether a latch command is active.
  bool latch_active;
} YPITChannelState;

// State of the PIT.
typedef struct YPITState {
  // Pointer to the PIT configuration.
  YPITConfig config;

  // The three timer channels.
  YPITChannelState channels[kPITNumChannels];
} YPITState;

// Initializes the PIT to its power-on state.
YAX86_PUBLIC void YPITInit(YPITState* pit);

// Handles reads from the PIT's I/O ports (0x40-0x42).
YAX86_PUBLIC uint8_t YPITReadPort(YPITState* pit, uint16_t port);

// Handles writes to the PIT's I/O ports (0x40-0x43).
YAX86_PUBLIC void YPITWritePort(YPITState* pit, uint16_t port, uint8_t value);

// Simulates a single tick of the PIT's input clock. This method should be
// invoked at a frequency of 1.193182 MHz for accurate timing.
YAX86_PUBLIC void YPITTick(YPITState* pit);

enum {
  // Returned by YPITTicksUntilNextEvent() when no channel is counting towards
  // anything, so there is nothing to schedule.
  kPITNoEvent = 0x7FFFFFFF,
};

// Advances the PIT by num_ticks of its input clock. Equivalent to calling
// YPITTick() num_ticks times, but does not take time proportional to num_ticks.
//
// A counter spends nearly all of its life counting down through values that
// change nothing observable, so those stretches are skipped arithmetically and
// only the ticks that can change a channel's output are simulated one at a
// time.
YAX86_PUBLIC void YPITAdvance(YPITState* pit, uint32_t num_ticks);

// Returns the number of input clock ticks until the earliest tick that could
// change any channel's output state, or kPITNoEvent if no channel is counting
// towards one.
//
// This is a lower bound rather than an exact answer. A caller that advances the
// PIT by this much and finds nothing happened has only done unnecessary work,
// whereas one that waited longer would miss an edge.
YAX86_PUBLIC uint32_t YPITTicksUntilNextEvent(const YPITState* pit);

#endif  // YAX86_PIT_PUBLIC_H
