// Logging library.
//
// Provides a YLogger that formats messages and hands them to a caller-provided
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
typedef enum YLogLevel {
  // Emulation is likely incorrect, e.g. an invalid opcode or an unmapped
  // memory access.
  Y_LOG_LEVEL_ERROR = 0,
  // Suspicious but handled, e.g. a read from an unmapped I/O port.
  Y_LOG_LEVEL_WARN,
  // Diagnostic detail.
  Y_LOG_LEVEL_DEBUG,
} YLogLevel;

enum {
  // Maximum length of a formatted log message, including the terminating null
  // byte. Longer messages are truncated.
  Y_LOG_MAX_LINE_LENGTH = 256,
  // Maximum number of distinct log modules, bounded by the width of
  // YLoggerConfig.enabled_modules.
  Y_LOG_MAX_MODULES = 32,
};

// Identifies the module a log message originated from.
//
// Each module declares its own YLogModule in its own public header, so that
// modules do not need to know about one another. IDs must be unique across
// modules - see the module ID test in core/tests/util.
typedef struct YLogModule {
  // Bit index used for mask-based filtering. Must be less than
  // Y_LOG_MAX_MODULES.
  uint8_t id;
  // Human-readable module name, e.g. "FDC".
  const char* name;
} YLogModule;

// Returns the filter mask bit for a module.
Y_PUBLIC_HEADER inline uint32_t YLogModuleMask(const YLogModule* module) {
  return (uint32_t)1 << module->id;
}

// ============================================================================
// Logger
// ============================================================================

// Caller-provided runtime configuration for a logger.
typedef struct YLoggerConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Callback to write a formatted log message. The message is null-terminated
  // and carries no prefix or trailing newline - the host composes the final
  // output line.
  void (*write_line)(
      void* context, const YLogModule* module, YLogLevel level, uint64_t tick,
      const char* message, size_t length);

  // Callback returning the current tick count. The platform wires this to its
  // own tick counter. May be NULL, in which case the tick passed to write_line
  // is 0.
  uint64_t (*get_tick)(void* context);

  // Bit mask of enabled modules, indexed by YLogModule.id.
  uint32_t enabled_modules;

  // Maximum severity level to emit. Messages with a level greater than this
  // are suppressed.
  YLogLevel min_level;
} YLoggerConfig;

// State of a logger.
typedef struct YLogger {
  // Pointer to caller-provided runtime configuration.
  YLoggerConfig* config;

  // Scratch buffer used to format a single message. A logger is therefore not
  // reentrant: a write_line callback must never log.
  char buffer[Y_LOG_MAX_LINE_LENGTH];
} YLogger;

// Initialize a logger with the provided configuration.
Y_PUBLIC_HEADER inline void YLoggerInit(
    YLogger* logger, YLoggerConfig* config) {
  logger->config = config;
  logger->buffer[0] = '\0';
}

// Whether a message with the given module and level would be emitted. This is
// checked before a message is formatted, so that disabled log statements cost
// only a few comparisons.
Y_PUBLIC_HEADER inline bool YLoggerIsEnabled(
    const YLogger* logger, const YLogModule* module, YLogLevel level) {
  return logger != NULL && logger->config != NULL &&
         logger->config->write_line != NULL &&
         level <= logger->config->min_level &&
         (logger->config->enabled_modules & YLogModuleMask(module)) != 0;
}

// Enable a module on a logger.
Y_PUBLIC_HEADER inline void YLoggerEnableModule(
    YLogger* logger, const YLogModule* module) {
  if (logger != NULL && logger->config != NULL) {
    logger->config->enabled_modules |= YLogModuleMask(module);
  }
}

// Disable a module on a logger.
Y_PUBLIC_HEADER inline void YLoggerDisableModule(
    YLogger* logger, const YLogModule* module) {
  if (logger != NULL && logger->config != NULL) {
    logger->config->enabled_modules &= ~YLogModuleMask(module);
  }
}

// Format and emit a log message. Prefer the Y_LOG macro, which skips
// formatting when the message would be suppressed.
Y_PUBLIC_HEADER void YLoggerWrite(
    YLogger* logger, const YLogModule* module, YLogLevel level,
    const char* format, ...) Y_UNUSED;

Y_PUBLIC_HEADER void YLoggerWrite(
    YLogger* logger, const YLogModule* module, YLogLevel level,
    const char* format, ...) {
  // Callers normally go through Y_LOG, which has already checked this, but
  // YLoggerWrite is also callable directly.
  if (!YLoggerIsEnabled(logger, module, level)) {
    return;
  }

  va_list args;
  va_start(args, format);
  int formatted_length =
      YVSNPrintF(logger->buffer, Y_LOG_MAX_LINE_LENGTH, format, args);
  va_end(args);

  // YVSNPrintF returns the length the message would have had, which may exceed
  // the buffer. Report the length actually written instead.
  size_t length = 0;
  if (formatted_length > 0) {
    length = (size_t)formatted_length < Y_LOG_MAX_LINE_LENGTH
                 ? (size_t)formatted_length
                 : Y_LOG_MAX_LINE_LENGTH - 1;
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
#define Y_LOG(logger, module, level, ...)                     \
  do {                                                        \
    if (YLoggerIsEnabled((logger), (module), (level))) {      \
      YLoggerWrite((logger), (module), (level), __VA_ARGS__); \
    }                                                         \
  } while (0)

#endif  // YAX86_UTIL_LOG_H
