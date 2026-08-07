// ==============================================================================
// YAX86 LOG MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_LOG_BUNDLE_H
#define YAX86_LOG_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/log/public.h start
// ==============================================================================

#line 1 "./src/log/public.h"
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


// ==============================================================================
// src/log/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

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

#ifndef YAX86_IMPLEMENTATION
#include "common.h"
#endif

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
// src/log/log.c start
// ==============================================================================

#line 1 "./src/log/log.c"
#ifndef YAX86_IMPLEMENTATION
#include <stdarg.h>

#include "../util/snprintf.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

void LoggerInit(Logger* logger, LoggerConfig* config) {
  logger->config = config;
  logger->buffer[0] = '\0';
}

void LoggerWrite(
    Logger* logger, const LogCategory* category, LogLevel level,
    const char* format, ...) {
  // Callers normally go through YAX86_LOG, which has already checked this, but
  // LoggerWrite is also callable directly.
  if (!LoggerIsEnabled(logger, category, level)) {
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
      logger->config->context, category, level, tick, logger->buffer, length);
}


// ==============================================================================
// src/log/log.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_LOG_BUNDLE_H

