// ==============================================================================
// YAX86 PIT MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_PIT_BUNDLE_H
#define YAX86_PIT_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/util/common.h start
// ==============================================================================

#line 1 "./src/util/common.h"
#ifndef YAX86_UTIL_COMMON_H
#define YAX86_UTIL_COMMON_H

// Macro that expands to `static` when bundled. Use for variables and functions
// that need to be visible to other files within the same module, but not
// publicly to users of the bundled library.
//
// This enables better IDE integration as it allows each source file to be
// compiled independently in unbundled form, but still keeps the symbols private
// when bundled.
#ifdef YAX86_IMPLEMENTATION
// When bundled, static linkage so that the symbol is only visible within the
// implementation file.
#define YAX86_PRIVATE static
#else
// When unbundled, use default linkage.
#define YAX86_PRIVATE
#endif  // YAX86_IMPLEMENTATION

// Macro to mark a function or parameter as unused.
#if defined(__GNUC__) || defined(__clang__)
#define YAX86_UNUSED __attribute__((unused))
#else
#define YAX86_UNUSED
#endif  // defined(__GNUC__) || defined(__clang__)

// Macro to keep a function out of line.
//
// Only worth reaching for where inlining has been measured to hurt. On a core
// with few registers, folding a large callee into an already register-hungry
// caller makes both spill.
#if defined(__GNUC__) || defined(__clang__)
#define YAX86_NOINLINE __attribute__((noinline))
#else
#define YAX86_NOINLINE
#endif  // defined(__GNUC__) || defined(__clang__)

// Macro to keep a function inlined.
//
// Like YAX86_NOINLINE, only worth reaching for against a measurement. The case
// it exists for is a small helper on the hot path that the compiler inlines
// while it has a single caller and emits out of line once it has several - a
// change to the hot path that nothing in the source shows.
#if defined(__GNUC__) || defined(__clang__)
#define YAX86_ALWAYS_INLINE __attribute__((always_inline)) inline
#else
#define YAX86_ALWAYS_INLINE inline
#endif  // defined(__GNUC__) || defined(__clang__)

// Marks a function as being on the per-instruction hot path.
//
// Empty by default, because on a machine with a normal cache hierarchy there is
// nothing useful to say. A target whose fastest memory has to be chosen
// explicitly can define it to whatever placement attribute it needs - on an
// RP2040, code runs from QSPI flash through a 16KB cache, and putting the hot
// path in SRAM instead takes that cache out of the picture.
//
// Define it before including this header, or on the command line.
#ifndef YAX86_HOT
#define YAX86_HOT
#endif  // YAX86_HOT

// The same, for data rather than code.
//
// Separate from YAX86_HOT because a compiler will not put executable code and
// read-only data in one section, and because a target may well want to place
// them differently even where it could.
#ifndef YAX86_HOT_DATA
#define YAX86_HOT_DATA
#endif  // YAX86_HOT_DATA

#endif  // YAX86_UTIL_COMMON_H


// ==============================================================================
// src/util/common.h end
// ==============================================================================

// ==============================================================================
// src/util/snprintf.h start
// ==============================================================================

#line 1 "./src/util/snprintf.h"
#ifndef YAX86_UTIL_SNPRINTF_H
#define YAX86_UTIL_SNPRINTF_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef YAX86_UTIL_COMMON_H
#include "common.h"
#endif  // YAX86_UTIL_COMMON_H

// A minimal implementation of snprintf/vsnprintf for freestanding environments.
// Supports:
// - %c: Character
// - %s: String
// - %d, %i: Signed integer
// - %u: Unsigned integer
// - %x, %X: Hexadecimal integer
// - %p: Pointer
// - %%: Percent sign
// - Width specifier (e.g., %5d)
// - Zero padding (e.g., %05d)
// - Length modifiers: 'l' (long), 'll' (long long), 'z' (size_t)

static int VSNPrintF(char* buffer, size_t size, const char* format,
                     va_list args) YAX86_UNUSED;

static int SNPrintF(char* buffer, size_t size, const char* format, ...)
    YAX86_UNUSED;

// Helper to put a character into the buffer safely.
// Returns 1 (always counts the character, even if not written).
static size_t SNPrintFPutC(char* buffer, size_t size, size_t* pos, char c) {
  if (*pos < size) {
    buffer[*pos] = c;
  }
  (*pos)++;
  return 1;
}

static size_t SNPrintFPutS(char* buffer, size_t size, size_t* pos,
                           const char* s, int width) {
  size_t count = 0;
  size_t len = 0;
  const char* tmp = s;
  while (*tmp++) len++;

  int pad = width - (int)len;
  if (pad < 0) pad = 0;

  // Strings always use space padding (zero flag is ignored per standard)
  while (pad-- > 0) {
    count += SNPrintFPutC(buffer, size, pos, ' ');
  }

  while (*s) {
    count += SNPrintFPutC(buffer, size, pos, *s++);
  }
  return count;
}

static size_t SNPrintFPutUI(char* buffer, size_t size, size_t* pos,
                            unsigned long long value, int base, int uppercase,
                            int width, int pad_zero, int negative) {
  char temp[64];
  int i = 0;
  size_t count = 0;

  if (value == 0) {
    temp[i++] = '0';
  } else {
    while (value != 0) {
      int digit = value % base;
      if (digit < 10) {
        temp[i++] = digit + '0';
      } else {
        temp[i++] = digit - 10 + (uppercase ? 'A' : 'a');
      }
      value /= base;
    }
  }

  int len = i;
  if (negative) len++;

  int pad = width - len;
  if (pad < 0) pad = 0;

  // If zero padding is requested, sign should be printed before padding
  if (pad_zero) {
    if (negative) {
      count += SNPrintFPutC(buffer, size, pos, '-');
      negative = 0; // Sign already handled
    }
    while (pad-- > 0) {
      count += SNPrintFPutC(buffer, size, pos, '0');
    }
  } else {
    while (pad-- > 0) {
      count += SNPrintFPutC(buffer, size, pos, ' ');
    }
  }

  if (negative) {
    count += SNPrintFPutC(buffer, size, pos, '-');
  }

  while (i > 0) {
    count += SNPrintFPutC(buffer, size, pos, temp[--i]);
  }
  return count;
}

static int VSNPrintF(char* buffer, size_t size, const char* format,
                     va_list args) {
  size_t pos = 0;

  while (*format) {
    if (*format != '%') {
      SNPrintFPutC(buffer, size, &pos, *format++);
      continue;
    }

    format++;  // Skip '%'

    // Flags
    int pad_zero = 0;
    if (*format == '0') {
      pad_zero = 1;
      format++;
    }

    // Width
    int width = 0;
    while (*format >= '0' && *format <= '9') {
      width = width * 10 + (*format - '0');
      format++;
    }

    // Length modifiers
    int length_l = 0;  // 0: int, 1: long, 2: long long
    int length_z = 0;  // size_t
    if (*format == 'l') {
      length_l++;
      format++;
      if (*format == 'l') {
        length_l++;
        format++;
      }
    } else if (*format == 'z') {
      length_z = 1;
      format++;
    }

    // Specifier
    switch (*format) {
      case 'c': {
        char c = (char)va_arg(args, int);
        // Pad with spaces (zero flag is ignored for %c per standard)
        int pad = width - 1;
        while (pad-- > 0) {
          SNPrintFPutC(buffer, size, &pos, ' ');
        }
        SNPrintFPutC(buffer, size, &pos, c);
        break;
      }
      case 's': {
        const char* s = va_arg(args, const char*);
        if (!s) s = "(null)";
        SNPrintFPutS(buffer, size, &pos, s, width);
        break;
      }
      case 'd':
      case 'i': {
        long long val;
        if (length_z)
          // Treat size_t as signed (ssize_t) for %d, or just cast to compatible signed type.
          // Since we don't have ssize_t here explicitly, we assume the user passes a signed type
          // compatible with size_t or we cast.
          val = (long long)va_arg(args, size_t);
        else if (length_l == 2)
          val = va_arg(args, long long);
        else if (length_l == 1)
          val = va_arg(args, long);
        else
          val = va_arg(args, int);

        int negative = 0;
        unsigned long long uval;
        if (val < 0) {
          negative = 1;
          uval = (unsigned long long)-val;
        } else {
          uval = (unsigned long long)val;
        }
        SNPrintFPutUI(buffer, size, &pos, uval, 10, 0, width, pad_zero,
                        negative);
        break;
      }
      case 'u':
      case 'x':
      case 'X': {
        unsigned long long val;
        int base = 10;
        int uppercase = 0;

        if (*format == 'x') {
          base = 16;
        } else if (*format == 'X') {
          base = 16;
          uppercase = 1;
        }

        if (length_z)
          val = va_arg(args, size_t);
        else if (length_l == 2)
          val = va_arg(args, unsigned long long);
        else if (length_l == 1)
          val = va_arg(args, unsigned long);
        else
          val = va_arg(args, unsigned int);

        SNPrintFPutUI(buffer, size, &pos, val, base, uppercase, width,
                        pad_zero, 0);
        break;
      }
      case 'p': {
        unsigned long long val =
            (unsigned long long)(uintptr_t)va_arg(args, void*);
        // Print 0x prefix
        SNPrintFPutC(buffer, size, &pos, '0');
        SNPrintFPutC(buffer, size, &pos, 'x');
        // Adjust width to account for "0x" prefix
        int adjusted_width = width > 2 ? width - 2 : 0;
        SNPrintFPutUI(buffer, size, &pos, val, 16, 0, adjusted_width, pad_zero,
                        0);
        break;
      }
      case '%': {
        SNPrintFPutC(buffer, size, &pos, '%');
        break;
      }
      default: {
        // Unknown specifier, print % and the specifier literally
        SNPrintFPutC(buffer, size, &pos, '%');
        SNPrintFPutC(buffer, size, &pos, *format);
        break;
      }
    }
    format++;
  }

  // Null terminate if possible
  if (size > 0) {
    if (pos < size) {
      buffer[pos] = '\0';
    } else {
      buffer[size - 1] = '\0';
    }
  }

  return (int)pos;
}

static int SNPrintF(char* buffer, size_t size, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int ret = VSNPrintF(buffer, size, format, args);
  va_end(args);
  return ret;
}

#endif  // YAX86_UTIL_SNPRINTF_H


// ==============================================================================
// src/util/snprintf.h end
// ==============================================================================

// ==============================================================================
// src/util/log.h start
// ==============================================================================

#line 1 "./src/util/log.h"
// Logging library.
//
// Provides a Logger that formats messages and hands them to a caller-provided
// sink. Logging is always compiled in - even on MCU targets the sink can write
// to a debugging serial port - and is filtered entirely at runtime, by module
// and by severity level, before a message is formatted.

#ifndef YAX86_UTIL_LOG_H
#define YAX86_UTIL_LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Sibling includes are guarded by the target's own include guard rather than
// by YAX86_IMPLEMENTATION, so that this header works both on its own and when
// bundled into a module, in either declaration-only or implementation mode.
#ifndef YAX86_UTIL_COMMON_H
#include "common.h"
#endif  // YAX86_UTIL_COMMON_H
#ifndef YAX86_UTIL_SNPRINTF_H
#include "snprintf.h"
#endif  // YAX86_UTIL_SNPRINTF_H

// ============================================================================
// Levels and modules
// ============================================================================

// Log severity levels, in decreasing order of severity.
typedef enum LogLevel {
  // Emulation is likely incorrect, e.g. an invalid opcode or an unmapped
  // memory access.
  kLogLevelError = 0,
  // Suspicious but handled, e.g. a read from an unmapped I/O port.
  kLogLevelWarn,
  // Diagnostic detail.
  kLogLevelDebug,
} LogLevel;

enum {
  // Maximum length of a formatted log message, including the terminating null
  // byte. Longer messages are truncated.
  kLogMaxLineLength = 256,
  // Maximum number of distinct log modules, bounded by the width of
  // LoggerConfig.enabled_modules.
  kLogMaxModules = 32,
};

// Identifies the module a log message originated from.
//
// Each module declares its own LogModule in its own public header, so that
// modules do not need to know about one another. IDs must be unique across
// modules - see the module ID test in core/tests/util.
typedef struct LogModule {
  // Bit index used for mask-based filtering. Must be less than kLogMaxModules.
  uint8_t id;
  // Human-readable module name, e.g. "FDC".
  const char* name;
} LogModule;

// Returns the filter mask bit for a module.
static inline uint32_t LogModuleMask(const LogModule* module) {
  return (uint32_t)1 << module->id;
}

// ============================================================================
// Logger
// ============================================================================

// Caller-provided runtime configuration for a logger.
typedef struct LoggerConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Callback to write a formatted log message. The message is null-terminated
  // and carries no prefix or trailing newline - the host composes the final
  // output line.
  void (*write_line)(
      void* context, const LogModule* module, LogLevel level, uint64_t tick,
      const char* message, size_t length);

  // Callback returning the current tick count. The platform wires this to its
  // own tick counter. May be NULL, in which case the tick passed to write_line
  // is 0.
  uint64_t (*get_tick)(void* context);

  // Bit mask of enabled modules, indexed by LogModule.id.
  uint32_t enabled_modules;

  // Maximum severity level to emit. Messages with a level greater than this
  // are suppressed.
  LogLevel min_level;
} LoggerConfig;

// State of a logger.
typedef struct Logger {
  // Pointer to caller-provided runtime configuration.
  LoggerConfig* config;

  // Scratch buffer used to format a single message. A logger is therefore not
  // reentrant: a write_line callback must never log.
  char buffer[kLogMaxLineLength];
} Logger;

// Initialize a logger with the provided configuration.
static inline void LoggerInit(Logger* logger, LoggerConfig* config) {
  logger->config = config;
  logger->buffer[0] = '\0';
}

// Whether a message with the given module and level would be emitted. This is
// checked before a message is formatted, so that disabled log statements cost
// only a few comparisons.
static inline bool LoggerIsEnabled(
    const Logger* logger, const LogModule* module, LogLevel level) {
  return logger != NULL && logger->config != NULL &&
         logger->config->write_line != NULL &&
         level <= logger->config->min_level &&
         (logger->config->enabled_modules & LogModuleMask(module)) != 0;
}

// Enable a module on a logger.
static inline void LoggerEnableModule(Logger* logger, const LogModule* module) {
  if (logger != NULL && logger->config != NULL) {
    logger->config->enabled_modules |= LogModuleMask(module);
  }
}

// Disable a module on a logger.
static inline void LoggerDisableModule(
    Logger* logger, const LogModule* module) {
  if (logger != NULL && logger->config != NULL) {
    logger->config->enabled_modules &= ~LogModuleMask(module);
  }
}

// Format and emit a log message. Prefer the YAX86_LOG macro, which skips
// formatting when the message would be suppressed.
static void LoggerWrite(
    Logger* logger, const LogModule* module, LogLevel level, const char* format,
    ...) YAX86_UNUSED;

static void LoggerWrite(
    Logger* logger, const LogModule* module, LogLevel level, const char* format,
    ...) {
  // Callers normally go through YAX86_LOG, which has already checked this, but
  // LoggerWrite is also callable directly.
  if (!LoggerIsEnabled(logger, module, level)) {
    return;
  }

  va_list args;
  va_start(args, format);
  int formatted_length =
      VSNPrintF(logger->buffer, kLogMaxLineLength, format, args);
  va_end(args);

  // VSNPrintF returns the length the message would have had, which may exceed
  // the buffer. Report the length actually written instead.
  size_t length = 0;
  if (formatted_length > 0) {
    length = (size_t)formatted_length < kLogMaxLineLength
                 ? (size_t)formatted_length
                 : kLogMaxLineLength - 1;
  }

  uint64_t tick = logger->config->get_tick != NULL
                      ? logger->config->get_tick(logger->config->context)
                      : 0;
  logger->config->write_line(
      logger->config->context, module, level, tick, logger->buffer, length);
}

// Emit a log message, skipping formatting if it would be suppressed.
//
// This is a macro rather than a function because it takes a variable number of
// arguments and must avoid the cost of formatting a message that will be
// discarded.
#define YAX86_LOG(logger, module, level, ...)                \
  do {                                                       \
    if (LoggerIsEnabled((logger), (module), (level))) {      \
      LoggerWrite((logger), (module), (level), __VA_ARGS__); \
    }                                                        \
  } while (0)

#endif  // YAX86_UTIL_LOG_H


// ==============================================================================
// src/util/log.h end
// ==============================================================================

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

#ifndef YAX86_PIT_BUNDLE_H
#include "../util/log.h"
#endif  // YAX86_PIT_BUNDLE_H

enum {
  // Log module ID for the PIT.
  kLogModuleIDPIT = 3,
};

// Log module for the PIT.
static const LogModule kLogModulePIT = {
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

// Channel read/write access modes. This corresponds to bits 4-5 of the control
// word written to port 0x43.
typedef enum PITAccessMode {
  // Latch count value command
  kPITAccessLatch = 0,
  // Read/write lower byte only
  kPITAccessLSBOnly = 1,
  // Read/write upper byte only
  kPITAccessMSBOnly = 2,
  // Read/write lower byte then upper byte
  kPITAccessLSBThenMSB = 3,
} PITAccessMode;

// Which byte to read/write next when in mode kPITAccessLSBThenMSB.
typedef enum PITByte {
  kPITByteLSB = 0,
  kPITByteMSB = 1,
} PITByte;

struct PITState;

// Caller-provided runtime configuration for the PIT.
typedef struct PITConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger for this module. May be NULL.
  Logger* logger;

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
} PITConfig;

// State of a single PIT timer channel.
typedef struct PITChannelState {
  // The 16-bit counter value.
  uint16_t counter;
  // The 16-bit latched value for reading.
  uint16_t latch;
  // The 16-bit reload value.
  uint16_t reload_value;
  // The operating mode (0-5).
  uint8_t mode;
  // The read/write access mode.
  PITAccessMode access_mode;
  // The current output state of the channel.
  bool output_state;
  // Which byte to read/write next when in mode kPITAccessLSBThenMSB.
  PITByte rw_byte;
  // Whether a latch command is active.
  bool latch_active;
} PITChannelState;

// State of the PIT.
typedef struct PITState {
  // Pointer to the PIT configuration.
  PITConfig* config;

  // The three timer channels.
  PITChannelState channels[kPITNumChannels];
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

enum {
  // Returned by PITTicksUntilNextEvent() when no channel is counting towards
  // anything, so there is nothing to schedule.
  kPITNoEvent = 0x7FFFFFFF,
};

// Advances the PIT by num_ticks of its input clock. Equivalent to calling
// PITTick() num_ticks times, but does not take time proportional to num_ticks.
//
// A counter spends nearly all of its life counting down through values that
// change nothing observable, so those stretches are skipped arithmetically and
// only the ticks that can change a channel's output are simulated one at a
// time.
void PITAdvance(PITState* pit, uint32_t num_ticks);

// Returns the number of input clock ticks until the earliest tick that could
// change any channel's output state, or kPITNoEvent if no channel is counting
// towards one.
//
// This is a lower bound rather than an exact answer. A caller that advances the
// PIT by this much and finds nothing happened has only done unnecessary work,
// whereas one that waited longer would miss an edge.
uint32_t PITTicksUntilNextEvent(const PITState* pit);

#endif  // YAX86_PIT_PUBLIC_H


// ==============================================================================
// src/pit/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/pit/pit.c start
// ==============================================================================

#line 1 "./src/pit/pit.c"
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

YAX86_HOT static uint32_t PITMode2TicksUntilEvent(
    const PITChannelState* channel) {
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
YAX86_HOT static uint32_t PITMode3SkipTicks(const PITChannelState* channel) {
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

YAX86_HOT void PITTick(PITState* pit) {
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

YAX86_HOT void PITAdvance(PITState* pit, uint32_t num_ticks) {
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


// ==============================================================================
// src/pit/pit.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_PIT_BUNDLE_H

