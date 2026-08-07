// Public interface for the logging module.
#ifndef YAX86_LOG_PUBLIC_H
#define YAX86_LOG_PUBLIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ============================================================================
// Levels and categories
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
  // Maximum number of distinct log categories, bounded by the width of
  // LoggerConfig.enabled_categories.
  kLogMaxCategories = 32,
};

// Identifies the module a log message originated from.
//
// Each module declares its own category in its own public header, so that
// modules do not need to know about one another. IDs must be unique across
// modules - see the category ID test in core/tests/log.
typedef struct LogCategory {
  // Bit index used for mask-based filtering. Must be less than
  // kLogMaxCategories.
  uint8_t id;
  // Human-readable module name, e.g. "FDC".
  const char* name;
} LogCategory;

// Returns the filter mask bit for a category.
static inline uint32_t LogCategoryMask(const LogCategory* category) {
  return (uint32_t)1 << category->id;
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
      void* context, const LogCategory* category, LogLevel level, uint64_t tick,
      const char* message, size_t length);

  // Callback returning the current tick count. The platform wires this to its
  // own tick counter. May be NULL, in which case the tick passed to write_line
  // is 0.
  uint64_t (*get_tick)(void* context);

  // Bit mask of enabled categories, indexed by LogCategory.id.
  uint32_t enabled_categories;

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
void LoggerInit(Logger* logger, LoggerConfig* config);

// Whether a message with the given category and level would be emitted. This
// is checked before a message is formatted, so that disabled log statements
// cost only a few comparisons.
static inline bool LoggerIsEnabled(
    const Logger* logger, const LogCategory* category, LogLevel level) {
  return logger != NULL && logger->config != NULL &&
         logger->config->write_line != NULL &&
         level <= logger->config->min_level &&
         (logger->config->enabled_categories & LogCategoryMask(category)) != 0;
}

// Format and emit a log message. Prefer the YAX86_LOG macro, which skips
// formatting when the message would be suppressed.
void LoggerWrite(
    Logger* logger, const LogCategory* category, LogLevel level,
    const char* format, ...);

// Enable a category on a logger.
static inline void LoggerEnableCategory(
    Logger* logger, const LogCategory* category) {
  if (logger != NULL && logger->config != NULL) {
    logger->config->enabled_categories |= LogCategoryMask(category);
  }
}

// Disable a category on a logger.
static inline void LoggerDisableCategory(
    Logger* logger, const LogCategory* category) {
  if (logger != NULL && logger->config != NULL) {
    logger->config->enabled_categories &= ~LogCategoryMask(category);
  }
}

// Emit a log message, skipping formatting if it would be suppressed.
//
// This is a macro rather than a function because it takes a variable number of
// arguments and must avoid the cost of formatting a message that will be
// discarded.
#define YAX86_LOG(logger, category, level, ...)                \
  do {                                                         \
    if (LoggerIsEnabled((logger), (category), (level))) {      \
      LoggerWrite((logger), (category), (level), __VA_ARGS__); \
    }                                                          \
  } while (0)

#endif  // YAX86_LOG_PUBLIC_H
