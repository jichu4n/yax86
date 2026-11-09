// ==============================================================================
// YAX86 VIDEO MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_VIDEO_BUNDLE_H
#define YAX86_VIDEO_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/util/common.h start
// ==============================================================================

#line 1 "./src/util/common.h"
#ifndef YAX86_UTIL_COMMON_H
#define YAX86_UTIL_COMMON_H

// Part of a module's public interface. Declared in the module's public.h and
// defined in one of its source files.
#define Y_PUBLIC

// Part of a module's public interface, defined in a header rather than in a
// source file: one copy per translation unit.
#define Y_PUBLIC_HEADER static

// Visible to other files within the same module, but not publicly to users of
// the bundled library.
//
// This enables better IDE integration as it allows each source file to be
// compiled independently in unbundled form, but still keeps the symbols private
// when bundled.
#ifdef YAX86_IMPLEMENTATION
// When bundled, static linkage so that the symbol is only visible within the
// implementation file.
#define Y_MODULE_PRIVATE static
#else
// When unbundled, use default linkage.
#define Y_MODULE_PRIVATE
#endif  // YAX86_IMPLEMENTATION

// Shared between a module's source files, but defined in a header rather than
// in a source file: one copy per translation unit. Also used for internal
// utilities (like snprintf) that are included into multiple module bundles.
#define Y_MODULE_PRIVATE_HEADER static

// Visible only within the source file that defines it.
#define Y_FILE_PRIVATE static

// Macro to mark a function or parameter as unused.
#if defined(__GNUC__) || defined(__clang__)
#define Y_UNUSED __attribute__((unused))
#else
#define Y_UNUSED
#endif  // defined(__GNUC__) || defined(__clang__)

// Macro to keep a function out of line.
//
// Only worth reaching for where inlining has been measured to hurt. On a core
// with few registers, folding a large callee into an already register-hungry
// caller makes both spill.
#if defined(__GNUC__) || defined(__clang__)
#define Y_NOINLINE __attribute__((noinline))
#else
#define Y_NOINLINE
#endif  // defined(__GNUC__) || defined(__clang__)

// Macro to keep a function inlined.
//
// Like Y_NOINLINE, only worth reaching for against a measurement. The case
// it exists for is a small helper on the hot path that the compiler inlines
// while it has a single caller and emits out of line once it has several - a
// change to the hot path that nothing in the source shows.
#if defined(__GNUC__) || defined(__clang__)
#if !defined(YAX86_IMPLEMENTATION)
#define Y_ALWAYS_INLINE __attribute__((always_inline))
#else
#define Y_ALWAYS_INLINE __attribute__((always_inline)) inline
#endif
#else
#define Y_ALWAYS_INLINE inline
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
#ifndef Y_HOT
#define Y_HOT
#endif  // Y_HOT

// The same, for data rather than code.
//
// Separate from Y_HOT because a compiler will not put executable code and
// read-only data in one section, and because a target may well want to place
// them differently even where it could.
#ifndef Y_HOT_DATA
#define Y_HOT_DATA
#endif  // Y_HOT_DATA

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

Y_MODULE_PRIVATE_HEADER int YVSNPrintF(
    char* buffer, size_t size, const char* format, va_list args) Y_UNUSED;

Y_MODULE_PRIVATE_HEADER int YSNPrintF(
    char* buffer, size_t size, const char* format, ...) Y_UNUSED;

// Helper to put a character into the buffer safely.
// Returns 1 (always counts the character, even if not written).
Y_MODULE_PRIVATE_HEADER size_t
YSNPrintFPutC(char* buffer, size_t size, size_t* pos, char c) {
  if (*pos < size) {
    buffer[*pos] = c;
  }
  (*pos)++;
  return 1;
}

Y_MODULE_PRIVATE_HEADER size_t YSNPrintFPutS(
    char* buffer, size_t size, size_t* pos, const char* s, int width) {
  size_t count = 0;
  size_t len = 0;
  const char* tmp = s;
  while (*tmp++) len++;

  int pad = width - (int)len;
  if (pad < 0) pad = 0;

  // Strings always use space padding (zero flag is ignored per standard)
  while (pad-- > 0) {
    count += YSNPrintFPutC(buffer, size, pos, ' ');
  }

  while (*s) {
    count += YSNPrintFPutC(buffer, size, pos, *s++);
  }
  return count;
}

Y_MODULE_PRIVATE_HEADER size_t YSNPrintFPutUI(
    char* buffer, size_t size, size_t* pos, unsigned long long value, int base,
    int uppercase, int width, int pad_zero, int negative) {
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
      count += YSNPrintFPutC(buffer, size, pos, '-');
      negative = 0; // Sign already handled
    }
    while (pad-- > 0) {
      count += YSNPrintFPutC(buffer, size, pos, '0');
    }
  } else {
    while (pad-- > 0) {
      count += YSNPrintFPutC(buffer, size, pos, ' ');
    }
  }

  if (negative) {
    count += YSNPrintFPutC(buffer, size, pos, '-');
  }

  while (i > 0) {
    count += YSNPrintFPutC(buffer, size, pos, temp[--i]);
  }
  return count;
}

Y_MODULE_PRIVATE_HEADER int YVSNPrintF(
    char* buffer, size_t size, const char* format, va_list args) {
  size_t pos = 0;

  while (*format) {
    if (*format != '%') {
      YSNPrintFPutC(buffer, size, &pos, *format++);
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
          YSNPrintFPutC(buffer, size, &pos, ' ');
        }
        YSNPrintFPutC(buffer, size, &pos, c);
        break;
      }
      case 's': {
        const char* s = va_arg(args, const char*);
        if (!s) s = "(null)";
        YSNPrintFPutS(buffer, size, &pos, s, width);
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
          // Negated as unsigned: the magnitude of the most negative value
          // does not fit in a long long.
          uval = 0ULL - (unsigned long long)val;
        } else {
          uval = (unsigned long long)val;
        }
        YSNPrintFPutUI(
            buffer, size, &pos, uval, 10, 0, width, pad_zero, negative);
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

        YSNPrintFPutUI(
            buffer, size, &pos, val, base, uppercase, width, pad_zero, 0);
        break;
      }
      case 'p': {
        unsigned long long val =
            (unsigned long long)(uintptr_t)va_arg(args, void*);
        // Print 0x prefix
        YSNPrintFPutC(buffer, size, &pos, '0');
        YSNPrintFPutC(buffer, size, &pos, 'x');
        // Adjust width to account for "0x" prefix
        int adjusted_width = width > 2 ? width - 2 : 0;
        YSNPrintFPutUI(
            buffer, size, &pos, val, 16, 0, adjusted_width, pad_zero, 0);
        break;
      }
      case '%': {
        YSNPrintFPutC(buffer, size, &pos, '%');
        break;
      }
      default: {
        // Unknown specifier, print % and the specifier literally
        YSNPrintFPutC(buffer, size, &pos, '%');
        YSNPrintFPutC(buffer, size, &pos, *format);
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

Y_MODULE_PRIVATE_HEADER int YSNPrintF(
    char* buffer, size_t size, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int ret = YVSNPrintF(buffer, size, format, args);
  va_end(args);
  return ret;
}

#endif  // YAX86_UTIL_SNPRINTF_H


// ==============================================================================
// src/util/snprintf.h end
// ==============================================================================

// ==============================================================================
// src/util/logfmt.h start
// ==============================================================================

#line 1 "./src/util/logfmt.h"
// logfmt lines: space separated key=value pairs, one record to a line.
//
// The format everything the core and its hosts print is written in, and that
// a host's tools read back. A value is written bare when it can be: a number,
// true or false, or a string of printable ASCII with no space, quote, equals
// sign or backslash in it. Any other string is quoted, with a quote or a
// backslash in it escaped by a backslash, and a byte outside printable ASCII
// written as \xNN - CP437 text from the guest included - so that a line is
// always printable ASCII from end to end. Keys are written as given, and are
// expected to be snake_case.
//
// A line is built into a buffer the caller owns. A pair that does not fit is
// dropped whole, along with every pair after it, and the line ends with
// truncated=true instead, so that a truncated line is still a valid one.
#ifndef YAX86_UTIL_LOGFMT_H
#define YAX86_UTIL_LOGFMT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef YAX86_UTIL_COMMON_H
#include "common.h"
#endif  // YAX86_UTIL_COMMON_H

#ifndef YAX86_UTIL_SNPRINTF_H
#include "snprintf.h"
#endif  // YAX86_UTIL_SNPRINTF_H

// A line being built.
typedef struct YLogfmtLine {
  char* buffer;
  size_t size;
  // Characters in the line so far, not counting the terminating null byte.
  size_t length;
  // Whether a pair has been dropped for want of room.
  bool truncated;
} YLogfmtLine;

// What a truncated line ends with, in place of the pairs that did not fit.
Y_MODULE_PRIVATE_HEADER const char YLogfmtTruncatedPair[] Y_UNUSED =
    " truncated=true";

enum {
  // Room kept free at the end of the buffer for the truncation marker and the
  // terminating null byte.
  Y_LOGFMT_RESERVED_LENGTH = sizeof(YLogfmtTruncatedPair),
};

// Start an empty line in buffer, which has room for size characters including
// the terminating null byte. A buffer too small to hold even the truncation
// marker takes no pairs at all.
Y_PUBLIC_HEADER inline void YLogfmtLineInit(
    YLogfmtLine* line, char* buffer, size_t size) {
  line->buffer = buffer;
  line->size = size;
  line->length = 0;
  line->truncated = false;
  if (size > 0) {
    buffer[0] = '\0';
  }
}

// How far a pair may run: the buffer, less what is kept for the truncation
// marker and the terminating null byte.
Y_MODULE_PRIVATE_HEADER inline size_t YLogfmtLineLimit(
    const YLogfmtLine* line) {
  return line->size > Y_LOGFMT_RESERVED_LENGTH
             ? line->size - Y_LOGFMT_RESERVED_LENGTH
             : 0;
}

// Start a pair: the separator if the line already has one, then the key and
// the equals sign. Returns where the value goes, which may be past the limit -
// YLogfmtLineEndPair() is what decides whether the pair stays.
Y_MODULE_PRIVATE_HEADER inline size_t YLogfmtLineBeginPair(
    YLogfmtLine* line, const char* key) {
  const size_t limit = YLogfmtLineLimit(line);
  size_t position = line->length;
  if (position > 0) {
    YSNPrintFPutC(line->buffer, limit, &position, ' ');
  }
  YSNPrintFPutS(line->buffer, limit, &position, key, 0);
  YSNPrintFPutC(line->buffer, limit, &position, '=');
  return position;
}

// Keep the pair that ends at position if it fit, or drop it and mark the line
// truncated if it did not. Nothing is written beyond the limit either way,
// since every write is bounded by it.
Y_MODULE_PRIVATE_HEADER inline void YLogfmtLineEndPair(
    YLogfmtLine* line, size_t position) {
  if (position <= YLogfmtLineLimit(line)) {
    line->length = position;
  } else {
    line->truncated = true;
  }
  if (line->size > 0) {
    line->buffer[line->length] = '\0';
  }
}

Y_PUBLIC_HEADER inline void YLogfmtLineAddUInt(
    YLogfmtLine* line, const char* key, uint64_t value) {
  if (line->truncated) {
    return;
  }
  size_t position = YLogfmtLineBeginPair(line, key);
  YSNPrintFPutUI(
      line->buffer, YLogfmtLineLimit(line), &position, value, 10, 0, 0, 0, 0);
  YLogfmtLineEndPair(line, position);
}

Y_PUBLIC_HEADER inline void YLogfmtLineAddInt(
    YLogfmtLine* line, const char* key, int64_t value) {
  if (line->truncated) {
    return;
  }
  size_t position = YLogfmtLineBeginPair(line, key);
  // The magnitude of the most negative value does not fit in an int64_t, but
  // it does in a uint64_t, which is where the negation happens.
  const uint64_t magnitude =
      value < 0 ? (uint64_t)0 - (uint64_t)value : (uint64_t)value;
  YSNPrintFPutUI(
      line->buffer, YLogfmtLineLimit(line), &position, magnitude, 10, 0, 0, 0,
      value < 0);
  YLogfmtLineEndPair(line, position);
}

// A number in hexadecimal, with a 0x prefix, upper case digits, and at least
// digits of them - 0x0201 for a port, 0x9A for a byte.
Y_PUBLIC_HEADER inline void YLogfmtLineAddHex(
    YLogfmtLine* line, const char* key, uint64_t value, uint8_t digits) {
  if (line->truncated) {
    return;
  }
  const size_t limit = YLogfmtLineLimit(line);
  size_t position = YLogfmtLineBeginPair(line, key);
  YSNPrintFPutC(line->buffer, limit, &position, '0');
  YSNPrintFPutC(line->buffer, limit, &position, 'x');
  YSNPrintFPutUI(line->buffer, limit, &position, value, 16, 1, digits, 1, 0);
  YLogfmtLineEndPair(line, position);
}

Y_PUBLIC_HEADER inline void YLogfmtLineAddBool(
    YLogfmtLine* line, const char* key, bool value) {
  if (line->truncated) {
    return;
  }
  size_t position = YLogfmtLineBeginPair(line, key);
  YSNPrintFPutS(
      line->buffer, YLogfmtLineLimit(line), &position, value ? "true" : "false",
      0);
  YLogfmtLineEndPair(line, position);
}

// Whether a byte can appear in a value written bare.
Y_MODULE_PRIVATE_HEADER inline bool YLogfmtIsBareByte(uint8_t byte) {
  return byte > ' ' && byte < 0x7F && byte != '"' && byte != '=' &&
         byte != '\\';
}

// Whether a string can be written without quotes.
Y_MODULE_PRIVATE_HEADER inline bool YLogfmtIsBare(const char* value) {
  if (*value == '\0') {
    return false;
  }
  for (; *value != '\0'; ++value) {
    if (!YLogfmtIsBareByte((uint8_t)*value)) {
      return false;
    }
  }
  return true;
}

// A string, quoted and escaped if it has to be. A NULL string is written as an
// empty one.
Y_PUBLIC_HEADER inline void YLogfmtLineAddString(
    YLogfmtLine* line, const char* key, const char* value) {
  if (line->truncated) {
    return;
  }
  if (value == NULL) {
    value = "";
  }
  const size_t limit = YLogfmtLineLimit(line);
  size_t position = YLogfmtLineBeginPair(line, key);
  if (YLogfmtIsBare(value)) {
    YSNPrintFPutS(line->buffer, limit, &position, value, 0);
    YLogfmtLineEndPair(line, position);
    return;
  }
  YSNPrintFPutC(line->buffer, limit, &position, '"');
  for (; *value != '\0'; ++value) {
    const uint8_t byte = (uint8_t)*value;
    if (byte == '"' || byte == '\\') {
      YSNPrintFPutC(line->buffer, limit, &position, '\\');
      YSNPrintFPutC(line->buffer, limit, &position, (char)byte);
    } else if (byte < ' ' || byte >= 0x7F) {
      YSNPrintFPutC(line->buffer, limit, &position, '\\');
      YSNPrintFPutC(line->buffer, limit, &position, 'x');
      YSNPrintFPutUI(line->buffer, limit, &position, byte, 16, 1, 2, 1, 0);
    } else {
      YSNPrintFPutC(line->buffer, limit, &position, (char)byte);
    }
  }
  YSNPrintFPutC(line->buffer, limit, &position, '"');
  YLogfmtLineEndPair(line, position);
}

// Finish the line, marking it if anything was dropped, and return its length.
// The line is null terminated whether or not this is called.
Y_PUBLIC_HEADER inline size_t YLogfmtLineFinish(YLogfmtLine* line) {
  if (line->truncated && line->size >= Y_LOGFMT_RESERVED_LENGTH) {
    // A line with no pairs in it does not start with the separator.
    const char* marker =
        line->length == 0 ? YLogfmtTruncatedPair + 1 : YLogfmtTruncatedPair;
    size_t position = line->length;
    YSNPrintFPutS(line->buffer, line->size, &position, marker, 0);
    line->length = position;
    line->buffer[line->length] = '\0';
  }
  return line->length;
}

#endif  // YAX86_UTIL_LOGFMT_H


// ==============================================================================
// src/util/logfmt.h end
// ==============================================================================

// ==============================================================================
// src/util/log.h start
// ==============================================================================

#line 1 "./src/util/log.h"
// Logging library.
//
// Provides a YLogger that writes records as logfmt lines - see logfmt.h - and
// hands each line to a caller-provided sink. The core and its hosts write
// through the same logger, so that what any of them prints has one format
// whichever host it runs in.
//
// A line is one of two kinds, named by its first pair:
//
//   kind=log level=warn module=platform tick=1234 cs=0xF000 ip=0xE0A3
//       event=unmapped_port_read port=0x0201
//   kind=reply id=7 cmd=screen status=ok
//
// A log is something that happened, said unprompted: an event name, then
// fields. Logs are filtered at runtime, by module and by severity level,
// before anything is formatted, so a log that is filtered out costs a few
// comparisons. Logging is always compiled in - even on MCU targets the sink
// can write to a debugging serial port.
//
// A reply answers a command a host was sent, carrying the command's id. It is
// never filtered: whoever sent the command is waiting for it.
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
#ifndef YAX86_UTIL_LOGFMT_H
#include "logfmt.h"
#endif  // YAX86_UTIL_LOGFMT_H

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
  // Worth knowing in the normal course of things, e.g. how far a boot has
  // got.
  Y_LOG_LEVEL_INFO,
  // Diagnostic detail.
  Y_LOG_LEVEL_DEBUG,
} YLogLevel;

// The level's name, as a line gives it.
Y_PUBLIC_HEADER inline const char* YLogLevelName(YLogLevel level) {
  switch (level) {
    case Y_LOG_LEVEL_ERROR:
      return "error";
    case Y_LOG_LEVEL_WARN:
      return "warn";
    case Y_LOG_LEVEL_INFO:
      return "info";
    case Y_LOG_LEVEL_DEBUG:
      return "debug";
  }
  return "unknown";
}

enum {
  // Maximum length of a line, including the terminating null byte. A longer
  // one is truncated - see logfmt.h.
  Y_LOG_MAX_LINE_LENGTH = 256,
  // Maximum number of distinct log modules, bounded by the width of
  // YLoggerConfig.enabled_modules.
  Y_LOG_MAX_MODULES = 32,
  // Module IDs below this are the core's, and the rest are left for hosts to
  // give modules of their own.
  Y_LOG_FIRST_HOST_MODULE_ID = 16,
};

// Identifies the module a log line originated from.
//
// Each module declares its own YLogModule in its own public header, so that
// modules do not need to know about one another. IDs must be unique across
// modules - see the module ID test in core/tests/util.
typedef struct YLogModule {
  // Bit index used for mask-based filtering. Must be less than
  // Y_LOG_MAX_MODULES.
  uint8_t id;
  // The module's name, as a line gives it: snake_case, e.g. "fdc".
  const char* name;
} YLogModule;

// Returns the filter mask bit for a module.
Y_PUBLIC_HEADER inline uint32_t YLogModuleMask(const YLogModule* module) {
  return (uint32_t)1 << module->id;
}

// ============================================================================
// Fields
// ============================================================================

typedef enum YLogFieldType {
  // Marks the end of a list of fields; see Y_LOG().
  Y_LOG_FIELD_END = 0,
  Y_LOG_FIELD_UINT,
  Y_LOG_FIELD_INT,
  Y_LOG_FIELD_HEX,
  Y_LOG_FIELD_BOOL,
  Y_LOG_FIELD_STRING,
} YLogFieldType;

// A key and a typed value, which a line writes as key=value.
typedef struct YLogField {
  // snake_case, e.g. "port".
  const char* key;
  YLogFieldType type;
  // How many hex digits to write at least, for Y_LOG_FIELD_HEX.
  uint8_t digits;
  union {
    uint64_t unsigned_value;
    int64_t signed_value;
    bool bool_value;
    const char* string_value;
  } value;
} YLogField;

Y_PUBLIC_HEADER inline YLogField YLogUInt(const char* key, uint64_t value) {
  YLogField field;
  field.key = key;
  field.type = Y_LOG_FIELD_UINT;
  field.digits = 0;
  field.value.unsigned_value = value;
  return field;
}

Y_PUBLIC_HEADER inline YLogField YLogInt(const char* key, int64_t value) {
  YLogField field;
  field.key = key;
  field.type = Y_LOG_FIELD_INT;
  field.digits = 0;
  field.value.signed_value = value;
  return field;
}

// A number in hexadecimal, with at least digits digits: written 0x0201.
Y_PUBLIC_HEADER inline YLogField YLogHex(
    const char* key, uint64_t value, uint8_t digits) {
  YLogField field;
  field.key = key;
  field.type = Y_LOG_FIELD_HEX;
  field.digits = digits;
  field.value.unsigned_value = value;
  return field;
}

Y_PUBLIC_HEADER inline YLogField YLogBool(const char* key, bool value) {
  YLogField field;
  field.key = key;
  field.type = Y_LOG_FIELD_BOOL;
  field.digits = 0;
  field.value.bool_value = value;
  return field;
}

Y_PUBLIC_HEADER inline YLogField YLogString(
    const char* key, const char* value) {
  YLogField field;
  field.key = key;
  field.type = Y_LOG_FIELD_STRING;
  field.digits = 0;
  field.value.string_value = value;
  return field;
}

Y_PUBLIC_HEADER inline YLogField YLogEnd(void) {
  YLogField field;
  field.key = NULL;
  field.type = Y_LOG_FIELD_END;
  field.digits = 0;
  field.value.unsigned_value = 0;
  return field;
}

// The fields a call site writes, in the widths the 8086's values come in.
#define Y_LOG_UINT(key, value) YLogUInt((key), (uint64_t)(value))
#define Y_LOG_INT(key, value) YLogInt((key), (int64_t)(value))
#define Y_LOG_HEX8(key, value) YLogHex((key), (uint64_t)(value), 2)
#define Y_LOG_HEX16(key, value) YLogHex((key), (uint64_t)(value), 4)
// A 20-bit physical address.
#define Y_LOG_HEX20(key, value) YLogHex((key), (uint64_t)(value), 5)
#define Y_LOG_HEX32(key, value) YLogHex((key), (uint64_t)(value), 8)
#define Y_LOG_BOOL(key, value) YLogBool((key), (bool)(value))
#define Y_LOG_STRING(key, value) YLogString((key), (value))

// Add a field to a line.
Y_PUBLIC_HEADER inline void YLogAppendField(
    YLogfmtLine* line, const YLogField* field) {
  switch (field->type) {
    case Y_LOG_FIELD_UINT:
      YLogfmtLineAddUInt(line, field->key, field->value.unsigned_value);
      break;
    case Y_LOG_FIELD_INT:
      YLogfmtLineAddInt(line, field->key, field->value.signed_value);
      break;
    case Y_LOG_FIELD_HEX:
      YLogfmtLineAddHex(
          line, field->key, field->value.unsigned_value, field->digits);
      break;
    case Y_LOG_FIELD_BOOL:
      YLogfmtLineAddBool(line, field->key, field->value.bool_value);
      break;
    case Y_LOG_FIELD_STRING:
      YLogfmtLineAddString(line, field->key, field->value.string_value);
      break;
    case Y_LOG_FIELD_END:
      break;
  }
}

// Add each field in args to a line, up to the YLogEnd() that ends them.
Y_PUBLIC_HEADER inline void YLogAppendFields(YLogfmtLine* line, va_list args) {
  for (;;) {
    const YLogField field = va_arg(args, YLogField);
    if (field.type == Y_LOG_FIELD_END) {
      return;
    }
    YLogAppendField(line, &field);
  }
}

// ============================================================================
// Logger
// ============================================================================

typedef enum YLogKind {
  Y_LOG_KIND_LOG = 0,
  Y_LOG_KIND_REPLY,
} YLogKind;

// Caller-provided runtime configuration for a logger.
typedef struct YLoggerConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Callback to write a finished line. The line is null terminated and has
  // no trailing newline. The module and level are those of a log, for a sink
  // that wants to route or colour by them; a reply has neither, and is passed
  // NULL and Y_LOG_LEVEL_INFO.
  void (*write_line)(
      void* context, YLogKind kind, const YLogModule* module, YLogLevel level,
      const char* line, size_t length);

  // Bit mask of enabled modules, indexed by YLogModule.id.
  uint32_t enabled_modules;
  // Maximum severity level to emit. Logs with a level greater than this are
  // suppressed.
  YLogLevel min_level;
} YLoggerConfig;

// Where a logger finds out when and where in the guest a log happened: the
// machine's tick count, and the instruction the CPU is running. Set by
// whoever owns the logger - the platform, for the logger it shares with its
// modules - rather than by the host, which would only wire it to the same
// two things.
typedef struct YLoggerSource {
  void* context;
  // The current tick count, or NULL to leave tick out of each line.
  uint64_t (*get_tick)(void* context);
  // The CS and IP of the instruction running, or NULL to leave them out.
  // Returns false to leave them out of one line.
  bool (*get_location)(void* context, uint16_t* cs, uint16_t* ip);
} YLoggerSource;

// State of a logger.
typedef struct YLogger {
  // Pointer to caller-provided runtime configuration.
  YLoggerConfig* config;
  YLoggerSource source;
  // Scratch buffer a line is built in. A logger is therefore not reentrant:
  // a write_line callback must never log.
  char buffer[Y_LOG_MAX_LINE_LENGTH];
} YLogger;

// Initialize a logger with the provided configuration, and no source.
Y_PUBLIC_HEADER inline void YLoggerInit(
    YLogger* logger, YLoggerConfig* config) {
  logger->config = config;
  logger->source.context = NULL;
  logger->source.get_tick = NULL;
  logger->source.get_location = NULL;
  logger->buffer[0] = '\0';
}

// Set where the logger finds the tick and the location for each log.
Y_PUBLIC_HEADER inline void YLoggerSetSource(
    YLogger* logger, const YLoggerSource* source) {
  logger->source = *source;
}

// Whether a log with the given module and level would be emitted. This is
// checked before anything is formatted, so that disabled log statements cost
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

// Write a log: an event, then the fields that follow it, ended by YLogEnd().
// Prefer the Y_LOG macro, which skips all of it when the log would be
// suppressed, and ends the fields itself.
Y_PUBLIC_HEADER void YLoggerWrite(
    YLogger* logger, const YLogModule* module, YLogLevel level,
    const char* event, ...) Y_UNUSED;
Y_PUBLIC_HEADER void YLoggerWrite(
    YLogger* logger, const YLogModule* module, YLogLevel level,
    const char* event, ...) {
  // Callers normally go through Y_LOG, which has already checked this, but
  // YLoggerWrite is also callable directly.
  if (!YLoggerIsEnabled(logger, module, level)) {
    return;
  }
  YLogfmtLine line;
  YLogfmtLineInit(&line, logger->buffer, Y_LOG_MAX_LINE_LENGTH);
  YLogfmtLineAddString(&line, "kind", "log");
  YLogfmtLineAddString(&line, "level", YLogLevelName(level));
  YLogfmtLineAddString(&line, "module", module->name);
  const YLoggerSource* source = &logger->source;
  if (source->get_tick != NULL) {
    YLogfmtLineAddUInt(&line, "tick", source->get_tick(source->context));
  }
  uint16_t cs = 0;
  uint16_t ip = 0;
  if (source->get_location != NULL &&
      source->get_location(source->context, &cs, &ip)) {
    YLogfmtLineAddHex(&line, "cs", cs, 4);
    YLogfmtLineAddHex(&line, "ip", ip, 4);
  }
  YLogfmtLineAddString(&line, "event", event);
  va_list args;
  va_start(args, event);
  YLogAppendFields(&line, args);
  va_end(args);
  const size_t length = YLogfmtLineFinish(&line);
  logger->config->write_line(
      logger->config->context, Y_LOG_KIND_LOG, module, level, logger->buffer,
      length);
}

// Write a reply to the command with the given id: the command's name, then
// the fields that follow it, ended by YLogEnd(). Never filtered. Prefer the
// Y_LOG_REPLY macro, which ends the fields itself.
Y_PUBLIC_HEADER void YLoggerWriteReply(
    YLogger* logger, uint32_t id, const char* command, ...) Y_UNUSED;
Y_PUBLIC_HEADER void YLoggerWriteReply(
    YLogger* logger, uint32_t id, const char* command, ...) {
  if (logger == NULL || logger->config == NULL ||
      logger->config->write_line == NULL) {
    return;
  }
  YLogfmtLine line;
  YLogfmtLineInit(&line, logger->buffer, Y_LOG_MAX_LINE_LENGTH);
  YLogfmtLineAddString(&line, "kind", "reply");
  YLogfmtLineAddUInt(&line, "id", id);
  YLogfmtLineAddString(&line, "cmd", command);
  va_list args;
  va_start(args, command);
  YLogAppendFields(&line, args);
  va_end(args);
  const size_t length = YLogfmtLineFinish(&line);
  logger->config->write_line(
      logger->config->context, Y_LOG_KIND_REPLY, NULL, Y_LOG_LEVEL_INFO,
      logger->buffer, length);
}

// Write a log, skipping everything - the fields included - if it would be
// suppressed: Y_LOG(logger, module, level, "event_name", fields...), where
// each field is one of the Y_LOG_UINT() family above.
//
// The event is the first of the variadic arguments rather than a parameter of
// its own, so that a log with no fields is still a valid C99 macro call.
#define Y_LOG(logger, module, level, ...)                                \
  do {                                                                   \
    if (YLoggerIsEnabled((logger), (module), (level))) {                 \
      YLoggerWrite((logger), (module), (level), __VA_ARGS__, YLogEnd()); \
    }                                                                    \
  } while (0)

// Write a reply: Y_LOG_REPLY(logger, id, "command", fields...).
#define Y_LOG_REPLY(logger, id, ...) \
  YLoggerWriteReply((logger), (id), __VA_ARGS__, YLogEnd())

#endif  // YAX86_UTIL_LOG_H


// ==============================================================================
// src/util/log.h end
// ==============================================================================

// ==============================================================================
// src/video/public.h start
// ==============================================================================

#line 1 "./src/video/public.h"
// Public interface for the Video module.
#ifndef YAX86_VIDEO_PUBLIC_H
#define YAX86_VIDEO_PUBLIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef YAX86_VIDEO_BUNDLE_H
#include "../util/log.h"
#include "../util/static_vector.h"
#endif  // YAX86_VIDEO_BUNDLE_H

enum {
  // Log module ID for the Video.
  Y_VIDEO_LOG_MODULE_ID = 8,
};

// Log module for the Video.
Y_PUBLIC_HEADER const YLogModule YVideoLogModule = {
    .id = Y_VIDEO_LOG_MODULE_ID,
    .name = "video",
};

// ============================================================================
// General
// ============================================================================

// RGB pixel value.
typedef struct YVideoRGB {
  // Red component (0-255).
  uint8_t r;
  // Green component (0-255).
  uint8_t g;
  // Blue component (0-255).
  uint8_t b;
} YVideoRGB;

// Position in 2D space.
typedef struct YVideoPosition {
  // X coordinate.
  uint16_t x;
  // Y coordinate.
  uint16_t y;
} YVideoPosition;

// A rectangular portion of the host frame buffer, in pixels.
typedef struct YVideoRegion {
  YVideoPosition origin;
  uint16_t width;
  uint16_t height;
} YVideoRegion;

// The size of a frame, in pixels.
typedef struct YVideoSize {
  uint16_t width;
  uint16_t height;
} YVideoSize;

// Text mode character position. We use a different structure to avoid confusion
// with YVideoPosition, which is used for pixel coordinates.
typedef struct YVideoTextPosition {
  // Column (0-based).
  uint8_t col;
  // Row (0-based).
  uint8_t row;
} YVideoTextPosition;

// Video adapters supported by this module. Only one adapter is present in a
// machine at a time, selected before initialization.
typedef enum YVideoAdapter {
  // Monochrome Display and Printer Adapter.
  Y_VIDEO_ADAPTER_MDA = 0,
  // Color Graphics Adapter.
  Y_VIDEO_ADAPTER_CGA = 1,

  // Number of adapters supported.
  Y_VIDEO_NUM_ADAPTERS = 2,
} YVideoAdapter;

// Video modes, numbered as in the BIOS INT 10h mode numbers.
typedef enum YVideoMode {
  // CGA text mode 0x00: Text, 40x25, color burst off, 8x8
  Y_VIDEO_MODE_CGA_TEXT_40X25_MONO = 0x00,
  // CGA text mode 0x01: Text, 40x25, 16 colors, 8x8
  Y_VIDEO_MODE_CGA_TEXT_40X25_COLOR = 0x01,
  // CGA text mode 0x02: Text, 80x25, color burst off, 8x8
  Y_VIDEO_MODE_CGA_TEXT_80X25_MONO = 0x02,
  // CGA text mode 0x03: Text, 80x25, 16 colors, 8x8
  Y_VIDEO_MODE_CGA_TEXT_80X25_COLOR = 0x03,
  // CGA graphics mode 0x04: 320x200, 4 colors
  Y_VIDEO_MODE_CGA_GRAPHICS_320X200 = 0x04,
  // CGA graphics mode 0x05: 320x200, 4 colors, alternate palette
  Y_VIDEO_MODE_CGA_GRAPHICS_320X200_ALT = 0x05,
  // CGA graphics mode 0x06: 640x200, 2 colors
  Y_VIDEO_MODE_CGA_GRAPHICS_640X200 = 0x06,
  // MDA text mode 0x07: Text, 80×25, monochrome, 720x350, 9x14
  Y_VIDEO_MODE_MDA_TEXT_80X25 = 0x07,

  // Number of video modes supported.
  Y_VIDEO_NUM_MODES = 8,
} YVideoMode;

// Text vs graphics modes.
typedef enum YVideoModeType {
  // Invalid video mode. This is needed due to gap in the list of video mode
  // values.
  Y_VIDEO_MODE_UNSUPPORTED = 0,
  // Text mode.
  Y_VIDEO_MODE_TEXT,
  // Graphics mode.
  Y_VIDEO_MODE_GRAPHICS,
} YVideoModeType;

// Metadata for video modes.
typedef struct YVideoModeMetadata {
  // The video mode.
  YVideoMode mode;
  // Type of the video mode (text or graphics).
  YVideoModeType type;
  // Mapped memory address of video RAM.
  uint32_t vram_address;
  // Video RAM size in bytes.
  uint32_t vram_size;
  // Resolution width in pixels.
  uint16_t width;
  // Resolution height in pixels.
  uint16_t height;
  // Number of pages in the video mode.
  uint8_t num_pages;

  // Text mode - number of columns.
  uint8_t columns;
  // Text mode - number of rows.
  uint8_t rows;
  // Text mode - character width in pixels.
  uint8_t char_width;
  // Text mode - character height in pixels.
  uint8_t char_height;
} YVideoModeMetadata;

// ============================================================================
// Monochrome Display and Printer Adapter (MDA)
// ============================================================================

// MDA I/O ports
// ========================================
// I/O Register |
// Address      |  Function
// -------------|--------------------------
// 3B0          | Not Used
// 3B1          | Not Used
// 3B2          | Not Used
// 3B3          | Not Used
// 3B4          | 6845 Index Register
// 3B5          | 6845 Data Register
// 3B6          | Not Used
// 3B7          | Not Used
// 3B8          | CRT Control Port 1
// 3B9          | Reserved
// 3BA          | CRT Status Port
// 3BB          | Reserved
// 3BC          | Parallel Data Port
// 3BD          | Printer Status Port
// 3BE          | Printer Control Port
// 3BF          | Not Used
// ========================================

// CRT Control Port 1 (I/O port 3B8) - write only
// ========================================
// Bit Number | Function
//------------|-------------------------
// 0          | + High Resolution Mode
// 1          | Not Used
// 2          | Not Used
// 3          | + Video Enable
// 4          | Not Used
// 5          | + Enable Blink
// 6,7        | Not Used
// ========================================

// CRT Status Port (I/O port 3BA) - read only
// ========================================
// Bit Number | Function
//------------|-------------------------
// 0          | + Horizontal Drive
// 1          | Reserved
// 2          | Reserved
// 3          | + Black/White Video
// ========================================

// MDA I/O ports.
enum {
  Y_VIDEO_MDA_PORT_REGISTER_INDEX = 0x3B4,
  Y_VIDEO_MDA_PORT_REGISTER_DATA = 0x3B5,
  Y_VIDEO_MDA_PORT_CONTROL = 0x3B8,
  Y_VIDEO_MDA_PORT_STATUS = 0x3BA,
  Y_VIDEO_MDA_PORT_PRINTER_DATA = 0x3BC,
  Y_VIDEO_MDA_PORT_PRINTER_STATUS = 0x3BD,
  Y_VIDEO_MDA_PORT_PRINTER_CONTROL = 0x3BE,

  // Start of the MDA I/O port range.
  Y_VIDEO_MDA_PORT_START = 0x3B0,
  // Inclusive end of the MDA I/O port range.
  Y_VIDEO_MDA_PORT_END = 0x3BF,
};

enum {
  // MDA VRAM address.
  Y_VIDEO_MDA_VRAM_ADDRESS = 0xB0000,
  // MDA VRAM size.
  Y_VIDEO_MDA_VRAM_SIZE = 4 * 1024,  // 4K
};

// ============================================================================
// Color Graphics Adapter (CGA)
// ============================================================================

// CGA I/O ports
// ========================================
// I/O Register |
// Address      |  Function
// -------------|--------------------------
// 3D0          | 6845 Index Register (alias of 3D4)
// 3D1          | 6845 Data Register (alias of 3D5)
// 3D2          | 6845 Index Register (alias of 3D4)
// 3D3          | 6845 Data Register (alias of 3D5)
// 3D4          | 6845 Index Register
// 3D5          | 6845 Data Register
// 3D6          | 6845 Index Register (alias of 3D4)
// 3D7          | 6845 Data Register (alias of 3D5)
// 3D8          | Mode Control Register
// 3D9          | Color Select Register
// 3DA          | CRT Status Register
// 3DB          | Clear Light Pen Latch
// 3DC          | Preset Light Pen Latch
// 3DD - 3DF    | Not Used
// ========================================

// Mode Control Register (I/O port 3D8) - write only
// ========================================
// Bit Number | Function
//------------|-------------------------
// 0          | + 80x25 Text Mode (0 = 40x25)
// 1          | + Graphics Mode
// 2          | + Black/White Mode (disables the color burst)
// 3          | + Video Enable
// 4          | + 640x200 High Resolution Graphics Mode
// 5          | + Enable Blink
// 6,7        | Not Used
// ========================================

// Color Select Register (I/O port 3D9) - write only
// ========================================
// Bit Number | Function
//------------|-------------------------
// 0-2        | Background / border RGB. In 640x200 graphics, this is the
//            | foreground color instead.
// 3          | + Intensity of the above
// 4          | + Intensity of the 320x200 graphics palette
// 5          | 320x200 graphics palette (0 = green/red/brown,
//            | 1 = cyan/magenta/white)
// 6,7        | Not Used
// ========================================

// CRT Status Register (I/O port 3DA) - read only
// ========================================
// Bit Number | Function
//------------|-------------------------
// 0          | + Display disabled - VRAM access is safe
// 1          | + Light pen trigger set
// 2          | - Light pen switch is on
// 3          | + Vertical retrace in progress
// ========================================

// CGA I/O ports.
enum {
  Y_VIDEO_CGA_PORT_REGISTER_INDEX = 0x3D4,
  Y_VIDEO_CGA_PORT_REGISTER_DATA = 0x3D5,
  Y_VIDEO_CGA_PORT_CONTROL = 0x3D8,
  Y_VIDEO_CGA_PORT_COLOR_SELECT = 0x3D9,
  Y_VIDEO_CGA_PORT_STATUS = 0x3DA,
  Y_VIDEO_CGA_PORT_CLEAR_LIGHT_PEN = 0x3DB,
  Y_VIDEO_CGA_PORT_PRESET_LIGHT_PEN = 0x3DC,

  // Start of the CGA I/O port range.
  Y_VIDEO_CGA_PORT_START = 0x3D0,
  // Inclusive end of the CGA I/O port range.
  Y_VIDEO_CGA_PORT_END = 0x3DF,
};

enum {
  // CGA VRAM address.
  Y_VIDEO_CGA_VRAM_ADDRESS = 0xB8000,
  // CGA VRAM size.
  Y_VIDEO_CGA_VRAM_SIZE = 16 * 1024,  // 16K
  // Byte offset of the odd scan lines in CGA graphics modes. Graphics VRAM is
  // interleaved - even scan lines live in the first half and odd scan lines in
  // the second.
  Y_VIDEO_CGA_GRAPHICS_ODD_SCAN_LINE_OFFSET = 0x2000,
  // Number of bytes per scan line in CGA graphics modes.
  Y_VIDEO_CGA_GRAPHICS_BYTES_PER_SCAN_LINE = 80,
  // Number of colors in the CGA palette.
  Y_VIDEO_NUM_CGA_COLORS = 16,
};

// ============================================================================
// Motorola 6845 CRT controller
// ============================================================================

// Both the MDA and the CGA are built around a Motorola 6845 CRT controller. The
// index port (3B4 on MDA, 3D4 on CGA) selects a register, and the data port
// (3B5 / 3D5) is used to read or write the data for that register. Below are
// the registers and their default values for the IBM Monochrome Display and for
// the CGA in 80x25 text mode, as programmed by GLaBIOS.
// =============================================================================
// Register | Register File              | Program Unit     | MDA  | CGA 80x25
// ---------|----------------------------|------------------|------|-----------
// R0       | Horizontal Total           | Characters       | 0x61 | 0x71
// R1       | Horizontal Displayed       | Characters       | 0x50 | 0x50
// R2       | Horizontal Sync Position   | Characters       | 0x52 | 0x5A
// R3       | Horizontal Sync Width      | Characters       | 0x0F | 0x0A
// R4       | Vertical Total             | Character Rows   | 0x19 | 0x1F
// R5       | Vertical Total Adjust      | Scan Line        | 0x06 | 0x06
// R6       | Vertical Displayed         | Character Row    | 0x19 | 0x19
// R7       | Vertical Sync Position     | Character Row    | 0x19 | 0x1C
// R8       | Interlace Mode             | --------         | 0x02 | 0x02
// R9       | Maximum Scan Line          | Scan Line        | 0x0D | 0x07
// R10      | Cursor Start               | Scan Line        | 0x0B | 0x06
// R11      | Cursor End                 | Scan Line        | 0x0C | 0x07
// R12      | Start Address (H)          | --------         | 0x00 | 0x00
// R13      | Start Address (L)          | --------         | 0x00 | 0x00
// R14      | Cursor (H)                 | --------         | 0x00 | 0x00
// R15      | Cursor (L)                 | --------         | 0x00 | 0x00
// R16      | Reserved                   | --------         | --   | --
// R17      | Reserved                   | --------         | --   | --
// =============================================================================

// 6845 CRT controller registers.
enum {
  Y_VIDEO_CRTC_REGISTER_HORIZONTAL_TOTAL = 0,
  Y_VIDEO_CRTC_REGISTER_HORIZONTAL_DISPLAYED,
  Y_VIDEO_CRTC_REGISTER_HORIZONTAL_SYNC_POSITION,
  Y_VIDEO_CRTC_REGISTER_HORIZONTAL_SYNC_WIDTH,
  Y_VIDEO_CRTC_REGISTER_VERTICAL_TOTAL,
  Y_VIDEO_CRTC_REGISTER_VERTICAL_TOTAL_ADJUST,
  Y_VIDEO_CRTC_REGISTER_VERTICAL_DISPLAYED,
  Y_VIDEO_CRTC_REGISTER_VERTICAL_SYNC_POSITION,
  Y_VIDEO_CRTC_REGISTER_INTERLACE_MODE,
  Y_VIDEO_CRTC_REGISTER_MAXIMUM_SCAN_LINE,
  Y_VIDEO_CRTC_REGISTER_CURSOR_START,
  Y_VIDEO_CRTC_REGISTER_CURSOR_END,
  Y_VIDEO_CRTC_REGISTER_START_ADDRESS_H,
  Y_VIDEO_CRTC_REGISTER_START_ADDRESS_L,
  Y_VIDEO_CRTC_REGISTER_CURSOR_H,
  Y_VIDEO_CRTC_REGISTER_CURSOR_L,
  Y_VIDEO_CRTC_REGISTER_RESERVED16,
  Y_VIDEO_CRTC_REGISTER_RESERVED17,

  // Total number of 6845 CRT controller registers.
  Y_VIDEO_NUM_CRTC_REGISTERS,
};

// ============================================================================
// Register bits
// ============================================================================

// Bits in the mode control register - I/O port 3B8 on MDA, 3D8 on CGA.
enum {
  // CGA only - 80x25 text mode. MDA uses the same bit for high resolution mode,
  // which it always is.
  Y_VIDEO_CONTROL_HIGH_RESOLUTION = 1 << 0,
  // CGA only - graphics mode.
  Y_VIDEO_CONTROL_GRAPHICS = 1 << 1,
  // CGA only - black and white mode, i.e. the color burst is disabled. This has
  // no effect on an RGB monitor in text modes, but selects the third palette in
  // 320x200 graphics mode.
  Y_VIDEO_CONTROL_BLACK_AND_WHITE = 1 << 2,
  // Video signal enabled. When clear, the display is blank.
  Y_VIDEO_CONTROL_VIDEO_ENABLE = 1 << 3,
  // CGA only - 640x200 high resolution graphics mode.
  Y_VIDEO_CONTROL_HIGH_RESOLUTION_GRAPHICS = 1 << 4,
  // Attribute bit 7 means blinking rather than intense background.
  Y_VIDEO_CONTROL_ENABLE_BLINK = 1 << 5,
};

// Bits in the CGA color select register - I/O port 3D9.
enum {
  // Background / border color, or the foreground color in 640x200 graphics.
  Y_VIDEO_CGA_COLOR_SELECT_COLOR_MASK = 0x07,
  // Intensity of the background / border color.
  Y_VIDEO_CGA_COLOR_SELECT_INTENSITY = 1 << 3,
  // Intensity of the 320x200 graphics palette.
  Y_VIDEO_CGA_COLOR_SELECT_PALETTE_INTENSITY = 1 << 4,
  // 320x200 graphics palette selection.
  Y_VIDEO_CGA_COLOR_SELECT_PALETTE = 1 << 5,
};

// Bits in the status register - I/O port 3BA on MDA, 3DA on CGA.
enum {
  // Display is disabled, i.e. a horizontal or vertical retrace is in progress,
  // so VRAM can be accessed without causing snow.
  Y_VIDEO_STATUS_DISPLAY_DISABLED = 1 << 0,
  // Light pen trigger set. Always clear, as no light pen is emulated.
  Y_VIDEO_STATUS_LIGHT_PEN_TRIGGER = 1 << 1,
  // Light pen switch is off. Always set, as no light pen is emulated.
  Y_VIDEO_STATUS_LIGHT_PEN_SWITCH_OFF = 1 << 2,
  // Vertical retrace in progress.
  Y_VIDEO_STATUS_VERTICAL_RETRACE = 1 << 3,
};

// Bits in a text mode attribute byte.
enum {
  // Foreground color.
  Y_VIDEO_ATTRIBUTE_FOREGROUND_MASK = 0x07,
  // Intense foreground.
  Y_VIDEO_ATTRIBUTE_INTENSE_FOREGROUND = 1 << 3,
  // Background color.
  Y_VIDEO_ATTRIBUTE_BACKGROUND_MASK = 0x70,
  // Number of bits to shift right to get the background color.
  Y_VIDEO_ATTRIBUTE_BACKGROUND_SHIFT = 4,
  // Blinking foreground, or intense background if blinking is disabled in the
  // mode control register.
  Y_VIDEO_ATTRIBUTE_BLINK = 1 << 7,
};

// ============================================================================
// Video mode and adapter metadata
// ============================================================================

// Metadata for each video mode, indexed by mode number.
Y_PUBLIC_HEADER const YVideoModeMetadata
    YVideoModeMetadataTable[Y_VIDEO_NUM_MODES] = {
        // 0x00: CGA text, 40x25, color burst off
        {
            .mode = Y_VIDEO_MODE_CGA_TEXT_40X25_MONO,
            .type = Y_VIDEO_MODE_TEXT,
            .vram_address = Y_VIDEO_CGA_VRAM_ADDRESS,
            .vram_size = Y_VIDEO_CGA_VRAM_SIZE,
            .width = 320,
            .height = 200,
            .num_pages = 8,
            .columns = 40,
            .rows = 25,
            .char_width = 8,
            .char_height = 8,
        },
        // 0x01: CGA text, 40x25, 16 colors
        {
            .mode = Y_VIDEO_MODE_CGA_TEXT_40X25_COLOR,
            .type = Y_VIDEO_MODE_TEXT,
            .vram_address = Y_VIDEO_CGA_VRAM_ADDRESS,
            .vram_size = Y_VIDEO_CGA_VRAM_SIZE,
            .width = 320,
            .height = 200,
            .num_pages = 8,
            .columns = 40,
            .rows = 25,
            .char_width = 8,
            .char_height = 8,
        },
        // 0x02: CGA text, 80x25, color burst off
        {
            .mode = Y_VIDEO_MODE_CGA_TEXT_80X25_MONO,
            .type = Y_VIDEO_MODE_TEXT,
            .vram_address = Y_VIDEO_CGA_VRAM_ADDRESS,
            .vram_size = Y_VIDEO_CGA_VRAM_SIZE,
            .width = 640,
            .height = 200,
            .num_pages = 4,
            .columns = 80,
            .rows = 25,
            .char_width = 8,
            .char_height = 8,
        },
        // 0x03: CGA text, 80x25, 16 colors
        {
            .mode = Y_VIDEO_MODE_CGA_TEXT_80X25_COLOR,
            .type = Y_VIDEO_MODE_TEXT,
            .vram_address = Y_VIDEO_CGA_VRAM_ADDRESS,
            .vram_size = Y_VIDEO_CGA_VRAM_SIZE,
            .width = 640,
            .height = 200,
            .num_pages = 4,
            .columns = 80,
            .rows = 25,
            .char_width = 8,
            .char_height = 8,
        },
        // 0x04: CGA graphics, 320x200, 4 colors
        {
            .mode = Y_VIDEO_MODE_CGA_GRAPHICS_320X200,
            .type = Y_VIDEO_MODE_GRAPHICS,
            .vram_address = Y_VIDEO_CGA_VRAM_ADDRESS,
            .vram_size = Y_VIDEO_CGA_VRAM_SIZE,
            .width = 320,
            .height = 200,
            .num_pages = 1,
        },
        // 0x05: CGA graphics, 320x200, 4 colors, alternate palette
        {
            .mode = Y_VIDEO_MODE_CGA_GRAPHICS_320X200_ALT,
            .type = Y_VIDEO_MODE_GRAPHICS,
            .vram_address = Y_VIDEO_CGA_VRAM_ADDRESS,
            .vram_size = Y_VIDEO_CGA_VRAM_SIZE,
            .width = 320,
            .height = 200,
            .num_pages = 1,
        },
        // 0x06: CGA graphics, 640x200, 2 colors
        {
            .mode = Y_VIDEO_MODE_CGA_GRAPHICS_640X200,
            .type = Y_VIDEO_MODE_GRAPHICS,
            .vram_address = Y_VIDEO_CGA_VRAM_ADDRESS,
            .vram_size = Y_VIDEO_CGA_VRAM_SIZE,
            .width = 640,
            .height = 200,
            .num_pages = 1,
        },
        // 0x07: MDA text, 80x25, monochrome
        {
            .mode = Y_VIDEO_MODE_MDA_TEXT_80X25,
            .type = Y_VIDEO_MODE_TEXT,
            .vram_address = Y_VIDEO_MDA_VRAM_ADDRESS,
            .vram_size = Y_VIDEO_MDA_VRAM_SIZE,
            .width = 720,
            .height = 350,
            .num_pages = 1,
            .columns = 80,
            .rows = 25,
            .char_width = 9,
            .char_height = 14,
        },
};

// Metadata for a video adapter - the facts about an adapter that do not depend
// on the current video mode.
typedef struct YVideoAdapterMetadata {
  // The adapter.
  YVideoAdapter adapter;

  // Width of the frame buffer the host must provide, in pixels. This does not
  // change with the video mode: the CGA always scans the same number of dots
  // across the screen, and modes with a lower horizontal resolution are drawn
  // with each pixel doubled horizontally, just as on real hardware.
  uint16_t frame_buffer_width;
  // Height of the frame buffer the host must provide, in pixels.
  uint16_t frame_buffer_height;

  // Mapped memory address of video RAM.
  uint32_t vram_address;
  // Video RAM size in bytes.
  uint32_t vram_size;

  // Start of the adapter's I/O port range.
  uint16_t port_start;
  // Inclusive end of the adapter's I/O port range.
  uint16_t port_end;

  // Mode control register value at power-on. The power-on video mode follows
  // from it, so it is not stored separately.
  uint8_t default_control_register;

  // Number of CPU cycles per scan line.
  uint16_t cycles_per_scan_line;
  // Number of CPU cycles at the start of a scan line during which the display
  // is active. The remainder of the scan line is the horizontal retrace.
  uint16_t display_cycles_per_scan_line;
  // Number of scan lines per frame, including the vertical retrace.
  uint16_t scan_lines_per_frame;
  // Number of scan lines at the start of a frame that are displayed. The
  // remainder of the frame is the vertical retrace.
  uint16_t displayed_scan_lines;
} YVideoAdapterMetadata;

// Metadata for each video adapter, indexed by YVideoAdapter.
//
// The timing values are derived from the same 14.318MHz master crystal the CPU
// clock comes from, so they are exact in CPU cycles. The MDA scans 882 dots per
// line at 16.257MHz over 370 lines, which is 259 cycles per line and just under
// 50Hz. The CGA scans 912 dots per line at 14.318MHz over 262 lines, which is
// 304 cycles per line and just under 60Hz.
Y_PUBLIC_HEADER const YVideoAdapterMetadata
    YVideoAdapterMetadataTable[Y_VIDEO_NUM_ADAPTERS] = {
        // MDA
        {
            .adapter = Y_VIDEO_ADAPTER_MDA,
            .frame_buffer_width = 720,
            .frame_buffer_height = 350,
            .vram_address = Y_VIDEO_MDA_VRAM_ADDRESS,
            .vram_size = Y_VIDEO_MDA_VRAM_SIZE,
            .port_start = Y_VIDEO_MDA_PORT_START,
            .port_end = Y_VIDEO_MDA_PORT_END,
            // High resolution mode, video enable, blink enable.
            .default_control_register = 0x29,
            .cycles_per_scan_line = 259,
            .display_cycles_per_scan_line = 211,
            .scan_lines_per_frame = 370,
            .displayed_scan_lines = 350,
        },
        // CGA
        {
            .adapter = Y_VIDEO_ADAPTER_CGA,
            .frame_buffer_width = 640,
            .frame_buffer_height = 200,
            .vram_address = Y_VIDEO_CGA_VRAM_ADDRESS,
            .vram_size = Y_VIDEO_CGA_VRAM_SIZE,
            .port_start = Y_VIDEO_CGA_PORT_START,
            .port_end = Y_VIDEO_CGA_PORT_END,
            // 80x25 text mode, video enable, blink enable.
            .default_control_register = 0x29,
            .cycles_per_scan_line = 304,
            .display_cycles_per_scan_line = 213,
            .scan_lines_per_frame = 262,
            .displayed_scan_lines = 200,
        },
};

enum {
  // Number of frames between cursor blink phase changes. At roughly 50Hz this
  // gives a blink rate of about 3.1Hz, close to what the 6845 produces when it
  // is configured to blink at a sixteenth of the field rate.
  Y_VIDEO_FRAMES_PER_CURSOR_BLINK_PHASE = 8,
  // Number of frames between character blink phase changes. Characters blink
  // half as fast as the cursor: the 6845 generates the cursor blink itself,
  // while the blink attribute is decoded by the adapter from a separate
  // divider running at a thirty-secondth of the field rate.
  Y_VIDEO_FRAMES_PER_TEXT_BLINK_PHASE = 16,
};

enum {
  // Every supported mode divides horizontally into 80 natural rendering
  // units: text columns on the MDA and 80-column CGA, half-character cells in
  // 40-column CGA text modes, and VRAM bytes in CGA graphics modes.
  Y_VIDEO_DIRTY_COLUMNS = 80,
  // Vertically, one horizontal range is tracked per dirty row. A text mode's
  // dirty row is one character row, which covers exactly the scan lines a
  // changed cell occupies. Graphics modes have no character rows, so
  // neighboring scan lines share a range instead.
  Y_VIDEO_DIRTY_SCAN_LINES_PER_GROUP = 4,
  // The tallest graphics mode is 200 lines.
  Y_VIDEO_MAX_GRAPHICS_HEIGHT = 200,
  // The most character rows in any text mode.
  Y_VIDEO_MAX_TEXT_ROWS = 25,
  // Dirty rows needed to cover the tallest graphics mode.
  Y_VIDEO_DIRTY_GRAPHICS_ROW_COUNT = (
      Y_VIDEO_MAX_GRAPHICS_HEIGHT + Y_VIDEO_DIRTY_SCAN_LINES_PER_GROUP - 1) /
      Y_VIDEO_DIRTY_SCAN_LINES_PER_GROUP,
  // The tracker is statically sized for whichever mode needs the most rows.
  // Graphics modes do, at 50 against a text mode's 25.
  Y_VIDEO_DIRTY_ROW_COUNT =
              Y_VIDEO_DIRTY_GRAPHICS_ROW_COUNT > Y_VIDEO_MAX_TEXT_ROWS
      ? Y_VIDEO_DIRTY_GRAPHICS_ROW_COUNT
      : Y_VIDEO_MAX_TEXT_ROWS,
  // Pixels are transferred to the host in small horizontal spans. This removes
  // most callback overhead without requiring a frame or scan-line buffer.
  Y_VIDEO_PIXEL_BATCH_SIZE = 32,
};

// ============================================================================
// Host text font
// ============================================================================

enum {
  // Widest cell a host text font may have. A glyph row is one byte.
  Y_VIDEO_TEXT_FONT_MAX_WIDTH = 8,
  // Characters a text font covers: every value of a character byte.
  Y_VIDEO_TEXT_FONT_GLYPHS = 256,
};

// A font for text modes, in place of the adapter's own.
//
// The adapters draw text in 8x8 (CGA) or 9x14 (MDA) cells, 640 or 720 pixels
// across. A display narrower than that cannot show 80 columns by shrinking
// those glyphs, which makes them unreadable; it needs glyphs drawn for a
// narrower cell, such as 4x10 for 80 columns across 320 pixels. With a host
// font, text modes render in the font's cells, so an 80-column text frame is
// 80 * width by rows * height pixels - see YVideoGetFrameSize(). Graphics
// modes and everything the guest can observe are unaffected: the font changes
// what is drawn, not the CRTC or its timing.
//
// The cursor and the MDA underline, which the adapter places by scan line of
// its own cell, are scaled to the font's cell height.
typedef struct YVideoTextFont {
  // Glyph rows for all Y_VIDEO_TEXT_FONT_GLYPHS characters, height rows a
  // character, top to bottom. Bit (width - 1) of a row is its leftmost pixel,
  // the same layout as the adapters' own fonts. NULL for no host font.
  const uint8_t* glyphs;
  // Cell width in pixels, 1 to Y_VIDEO_TEXT_FONT_MAX_WIDTH.
  uint8_t width;
  // Cell height in pixels, at least 1.
  uint8_t height;
} YVideoTextFont;

// Half-open horizontal dirty range [start_column, end_column). Both fields are
// zero when the range is empty.
typedef struct YVideoDirtyRange {
  uint8_t start_column;
  uint8_t end_column;
} YVideoDirtyRange;

// What the next render has to do. The three states are mutually exclusive, so
// they are one value rather than a pair of flags that could disagree.
typedef enum YVideoDirtyStatus {
  // The host display already matches video state, so a render does nothing.
  // This is the zero value, which makes clearing the tracker a zero
  // initialization.
  Y_VIDEO_CLEAN = 0,
  // Some rows carry a dirty range, and only those need to be redrawn.
  Y_VIDEO_DIRTY,
  // The next render must replace the complete frame buffer. Used when a change
  // cannot be represented as local VRAM damage, such as a mode or palette
  // change.
  Y_VIDEO_FULL_REDRAW,
} YVideoDirtyStatus;

typedef struct YVideoDirtyState {
  // One horizontal range per dirty row, indexed by dirty row. Only meaningful
  // when status is Y_VIDEO_DIRTY.
  YVideoDirtyRange ranges[Y_VIDEO_DIRTY_ROW_COUNT];
  // What the next render has to do.
  YVideoDirtyStatus status;
} YVideoDirtyState;

// ============================================================================
// Video state
// ============================================================================

struct YVideoState;

// Caller-provided configuration for video rendering.
typedef struct YVideoConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger for this module. May be NULL.
  YLogger* logger;

  // The video adapter to emulate.
  YVideoAdapter adapter;

  // MDA - foreground color.
  YVideoRGB foreground;
  // MDA - intense foreground color.
  YVideoRGB intense_foreground;
  // MDA - background color.
  YVideoRGB background;

  // CGA - the 16 color RGBI palette.
  YVideoRGB cga_palette[Y_VIDEO_NUM_CGA_COLORS];

  // The font text modes are drawn in. All zeros, the default, draws them in
  // the adapter's own font. Checked by YVideoInit(); a font whose size the
  // renderer cannot draw is logged and the adapter's font used instead.
  YVideoTextFont text_font;

  // The emulated video RAM, at least the adapter's vram_size bytes of it.
  // Required - the adapter reads and writes it directly.
  //
  // Rendering is the heaviest reader of video RAM, so a callback per byte
  // would cost an indirect call in its inner loops. Writes still go through
  // YVideoWriteVRAM(), which marks the corresponding display region dirty.
  //
  // The caller owns the buffer and it must outlive the video state.
  uint8_t* vram;

  // Callback to write a horizontal span of RGB pixels to the real display,
  // invoked from YVideoRender(). The pixels begin at origin, remain on one scan
  // line, and are valid only for the duration of the synchronous callback.
  // count is between 1 and Y_VIDEO_PIXEL_BATCH_SIZE, inclusive.
  void (*write_pixels)(
      struct YVideoState* video, YVideoPosition origin, const YVideoRGB* pixels,
      uint8_t count);
  // Optional callbacks surrounding each dirty rectangular region emitted by
  // YVideoRender(). A retained display can use them to set a transfer window
  // before the row-major pixel stream begins.
  void (*begin_render_region)(struct YVideoState* video, YVideoRegion region);
  void (*end_render_region)(struct YVideoState* video);
} YVideoConfig;

// Default video config.
Y_PUBLIC_HEADER const YVideoConfig YVideoDefaultConfig = {
    .context = NULL,

    .adapter = Y_VIDEO_ADAPTER_MDA,

    .foreground = {.r = 0xAA, .g = 0xAA, .b = 0xAA},
    .intense_foreground = {.r = 0xFF, .g = 0xFF, .b = 0xFF},
    .background = {.r = 0x00, .g = 0x00, .b = 0x00},

    .cga_palette =
        {
            {.r = 0x00, .g = 0x00, .b = 0x00},  // 0  black
            {.r = 0x00, .g = 0x00, .b = 0xAA},  // 1  blue
            {.r = 0x00, .g = 0xAA, .b = 0x00},  // 2  green
            {.r = 0x00, .g = 0xAA, .b = 0xAA},  // 3  cyan
            {.r = 0xAA, .g = 0x00, .b = 0x00},  // 4  red
            {.r = 0xAA, .g = 0x00, .b = 0xAA},  // 5  magenta
            {.r = 0xAA, .g = 0x55, .b = 0x00},  // 6  brown
            {.r = 0xAA, .g = 0xAA, .b = 0xAA},  // 7  light gray
            {.r = 0x55, .g = 0x55, .b = 0x55},  // 8  dark gray
            {.r = 0x55, .g = 0x55, .b = 0xFF},  // 9  light blue
            {.r = 0x55, .g = 0xFF, .b = 0x55},  // 10 light green
            {.r = 0x55, .g = 0xFF, .b = 0xFF},  // 11 light cyan
            {.r = 0xFF, .g = 0x55, .b = 0x55},  // 12 light red
            {.r = 0xFF, .g = 0x55, .b = 0xFF},  // 13 light magenta
            {.r = 0xFF, .g = 0xFF, .b = 0x55},  // 14 yellow
            {.r = 0xFF, .g = 0xFF, .b = 0xFF},  // 15 white
        },

    .text_font = {.glyphs = NULL, .width = 0, .height = 0},

    .vram = NULL,
    .write_pixels = NULL,
    .begin_render_region = NULL,
    .end_render_region = NULL,
};

// Video state.
typedef struct YVideoState {
  // Caller-provided runtime configuration.
  YVideoConfig config;

  // The video adapter being emulated, copied from the config at init time.
  YVideoAdapter adapter;
  // Whether text modes are drawn in config.text_font, decided by YVideoInit().
  // A flag rather than a pointer to the font, so that a copy of the state does
  // not point back into the original.
  bool has_text_font;

  // Motorola 6845 CRT controller registers.
  uint8_t registers[Y_VIDEO_NUM_CRTC_REGISTERS];
  // Currently selected 6845 CRT controller register index (I/O port 3B4/3D4).
  uint8_t selected_register;
  // Mode control register value (I/O port 3B8/3D8).
  uint8_t control_register;
  // CGA color select register value (I/O port 3D9). Unused on MDA.
  uint8_t color_select_register;

  // Current scan line within the frame, including the vertical retrace.
  uint16_t scan_line;
  // CPU cycles elapsed within the current scan line.
  uint32_t scan_line_cycles;
  // Number of frames since initialization. Drives blinking.
  uint32_t frames;

  // Portions of the retained host display that no longer match video state.
  YVideoDirtyState dirty_state;

  // Pixels emitted so far for the region currently being rendered. A windowed
  // display places pixels by count rather than by coordinate, so a region that
  // emits a different number of pixels than it declared would corrupt it; this
  // is what the check at the end of each region compares.
  uint32_t num_pixels_emitted_for_region;
} YVideoState;

// Initialize video state with the provided configuration.
Y_PUBLIC void YVideoInit(YVideoState* video);

// Metadata for the adapter being emulated.
Y_PUBLIC const YVideoAdapterMetadata* YVideoGetAdapterMetadata(
    const YVideoState* video);

// The current video mode, derived from the mode control register.
Y_PUBLIC YVideoMode YVideoGetMode(const YVideoState* video);

// Metadata for the current video mode.
Y_PUBLIC const YVideoModeMetadata* YVideoGetModeMetadata(
    const YVideoState* video);

// The size of the frame the current mode renders. Without a host text font
// this is the adapter's frame buffer in every mode. With one, text modes
// render at the font's cell size instead, so the size changes when the guest
// switches between a text and a graphics mode; a mode change always renders
// the whole frame afresh.
Y_PUBLIC YVideoSize YVideoGetFrameSize(const YVideoState* video);

// Read a byte from a video I/O port.
Y_PUBLIC uint8_t YVideoReadPort(YVideoState* video, uint16_t port);
// Write a byte to a video I/O port.
Y_PUBLIC void YVideoWritePort(YVideoState* video, uint16_t port, uint8_t value);

// Read a byte from video RAM.
Y_PUBLIC uint8_t YVideoReadVRAM(YVideoState* video, uint32_t address);
// Write a byte to video RAM.
Y_PUBLIC void YVideoWriteVRAM(
    YVideoState* video, uint32_t address, uint8_t value);

// Advance the CRT beam by the given number of CPU cycles. This drives the
// retrace bits in the status register and the blink phase.
//
// The cost does not depend on how many cycles are passed, so a caller that
// advances the beam only when something is about to look at it may pass a
// whole frame's worth at once.
Y_PUBLIC void YVideoTick(YVideoState* video, uint32_t cycles);

// Bring dirty portions of the retained host display up to date. Each region is
// bracketed by the optional region callbacks and its pixels are passed to
// write_pixels in row-major horizontal spans. Does nothing when the display is
// unchanged.
Y_PUBLIC void YVideoRender(YVideoState* video);

// Hand what has changed on the screen to a copy of the video state, for a host
// that renders somewhere the machine is not running - on another core, say -
// and so cannot render from the live state while the machine changes it.
//
// Copies video into frame and the adapter's video memory into frame_vram,
// which must hold at least the adapter's vram_size bytes, and points frame at
// frame_vram. The record of what has changed moves with the copy: video is left
// marked up to date, so YVideoRender(frame) draws exactly what
// YVideoRender(video) would have, and a change made after the copy is drawn
// from the next one rather than lost or drawn twice. frame keeps video's
// callbacks.
//
// Returns false, copying nothing, when nothing has changed since the last
// render or copy.
Y_PUBLIC bool YVideoCopyForRender(
    YVideoState* video, YVideoState* frame, uint8_t* frame_vram);

#endif  // YAX86_VIDEO_PUBLIC_H


// ==============================================================================
// src/video/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/video/fonts.h start
// ==============================================================================

#line 1 "./src/video/fonts.h"
#ifndef YAX86_VIDEO_FONTS_H
#define YAX86_VIDEO_FONTS_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"

// MDA 9x14 font bitmaps.
extern const uint16_t YVideoFontMDA9x14Bitmap[256][14];
// CGA 8x8 font bitmaps.
extern const uint8_t YVideoFontCGA8x8Bitmap[256][8];

#endif  // YAX86_IMPLEMENTATION

#endif  // YAX86_VIDEO_FONTS_H


// ==============================================================================
// src/video/fonts.h end
// ==============================================================================

// ==============================================================================
// src/video/internal.h start
// ==============================================================================

#line 1 "./src/video/internal.h"
// Internal interface shared between the video module's source files.
#ifndef YAX86_VIDEO_INTERNAL_H
#define YAX86_VIDEO_INTERNAL_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Read a byte from the emulated video RAM, or 0xFF if no callback is installed.
// VRAM is aliased throughout the adapter's window, so an address past the end
// wraps around.
Y_MODULE_PRIVATE uint8_t
YVideoReadVRAMByte(YVideoState* video, uint32_t address);

// Write a byte to the emulated video RAM, ignored if no callback is installed.
// The address wraps in the same way as for reads.
Y_MODULE_PRIVATE void YVideoWriteVRAMByte(
    YVideoState* video, uint32_t address, uint8_t value);

// A horizontal run of pixels on its way to the host, buffered so that the
// callback is crossed once per batch rather than once per pixel.
//
// Every renderer walks a region's rows left to right, so a run is opened at
// the start of a row and pixels are pushed in order. That makes the position
// of each pixel implicit - it is wherever the run has got to - so nothing
// here compares coordinates, and a renderer has no way to name a position at
// all, let alone the wrong one.
typedef struct YVideoPixelRun {
  YVideoState* video;
  // Where the pixels currently buffered begin. Advances past each batch.
  YVideoPosition origin;
  YVideoRGB pixels[Y_VIDEO_PIXEL_BATCH_SIZE];
  uint8_t count;
} YVideoPixelRun;

// Hand over what has been buffered and continue the run after it.
Y_MODULE_PRIVATE_HEADER inline void YVideoPixelRunFlush(YVideoPixelRun* run) {
  if (run->count == 0) {
    return;
  }
  // Batching lets the region's pixel count be accumulated once per batch
  // rather than once per pixel.
  run->video->num_pixels_emitted_for_region += run->count;
  run->video->config.write_pixels(
      run->video, run->origin, run->pixels, run->count);
  run->origin.x += run->count;
  run->count = 0;
}

// Open a run at the leftmost pixel of a row. Any partial batch is handed over
// first, so one call both ends the previous row and begins the next.
Y_MODULE_PRIVATE_HEADER inline void YVideoPixelRunBegin(
    YVideoPixelRun* run, uint16_t x, uint16_t y) {
  YVideoPixelRunFlush(run);
  run->origin.x = x;
  run->origin.y = y;
}

Y_MODULE_PRIVATE_HEADER inline void YVideoPixelRunPush(
    YVideoPixelRun* run, YVideoRGB rgb) {
  run->pixels[run->count] = rgb;
  ++run->count;
  if (run->count == Y_VIDEO_PIXEL_BATCH_SIZE) {
    YVideoPixelRunFlush(run);
  }
}

// The width of the cell a text mode draws a character in: the host text font's
// if there is one, otherwise the mode's own.
Y_MODULE_PRIVATE_HEADER inline uint8_t YVideoGetTextCellWidth(
    const YVideoState* video, const YVideoModeMetadata* metadata) {
  return video->has_text_font ? video->config.text_font.width
                              : metadata->char_width;
}

// The height of the cell a text mode draws a character in.
Y_MODULE_PRIVATE_HEADER inline uint8_t YVideoGetTextCellHeight(
    const YVideoState* video, const YVideoModeMetadata* metadata) {
  return video->has_text_font ? video->config.text_font.height
                              : metadata->char_height;
}

// A row of a character in the host text font.
Y_MODULE_PRIVATE_HEADER inline uint8_t YVideoGetTextFontRow(
    const YVideoState* video, uint8_t character, uint8_t row) {
  const YVideoTextFont* font = &video->config.text_font;
  return font->glyphs[(uint16_t)character * font->height + row];
}

// The adapter places the cursor and the MDA underline by scan line of its own
// character cell, so with a host text font of a different height they have to
// move with it. A scan line of the adapter's cell covers a span of the drawn
// cell's rows, which these two take the ends of: a range of scan lines maps to
// the first row its first line covers through the last row its last line
// covers, so a range that is not empty in the adapter's cell is not empty in
// the drawn one. A scan line past the adapter's cell maps past the drawn one,
// so an out of range cursor register still draws nothing.

// The first row of the drawn cell that a scan line of the adapter's covers.
Y_MODULE_PRIVATE_HEADER inline uint16_t YVideoScaleTextScanLineStart(
    const YVideoState* video, const YVideoModeMetadata* metadata,
    uint8_t scan_line) {
  if (!video->has_text_font) {
    return scan_line;
  }
  return (uint16_t)((uint16_t)scan_line * video->config.text_font.height /
                    metadata->char_height);
}

// The last row of the drawn cell that a scan line of the adapter's covers.
Y_MODULE_PRIVATE_HEADER inline uint16_t YVideoScaleTextScanLineEnd(
    const YVideoState* video, const YVideoModeMetadata* metadata,
    uint8_t scan_line) {
  if (!video->has_text_font) {
    return scan_line;
  }
  return (
      uint16_t)(((uint16_t)(scan_line + 1) * video->config.text_font.height +
                 metadata->char_height - 1) /
                    metadata->char_height -
                1);
}

// Mark a half-open rectangle dirty. Horizontal coordinates use the mode's 80
// natural columns; vertical coordinates are dirty rows - a character row in
// text modes, a group of Y_VIDEO_DIRTY_SCAN_LINES_PER_GROUP scan lines in
// graphics modes.
Y_MODULE_PRIVATE void YVideoInvalidateRows(
    YVideoState* video, uint8_t start_column, uint8_t end_column,
    uint8_t first_row, uint8_t end_row);

// Whether the text mode cursor is currently in the visible half of its blink
// cycle.
Y_MODULE_PRIVATE bool YVideoIsCursorBlinkOn(const YVideoState* video);

// Whether characters carrying the blink attribute are currently visible. This
// runs at half the cursor's rate, so the two drift in and out of phase.
Y_MODULE_PRIVATE bool YVideoIsTextBlinkOn(const YVideoState* video);

// Whether the text mode cursor is enabled in the 6845 registers.
Y_MODULE_PRIVATE bool YVideoIsCursorEnabled(const YVideoState* video);

// The first scan line of the character cell covered by the text mode cursor,
// from the 6845 cursor start register. May be out of range for the current
// character height.
Y_MODULE_PRIVATE uint8_t YVideoGetCursorStartScanLine(const YVideoState* video);

// The last scan line of the character cell covered by the text mode cursor,
// from the 6845 cursor end register. May be out of range for the current
// character height.
Y_MODULE_PRIVATE uint8_t YVideoGetCursorEndScanLine(const YVideoState* video);

// The address of the first displayed character, in character units, from the
// 6845 start address registers.
Y_MODULE_PRIVATE uint16_t YVideoGetStartAddress(const YVideoState* video);

// The address of the text mode cursor, in character units, from the 6845 cursor
// address registers.
Y_MODULE_PRIVATE uint16_t YVideoGetCursorAddress(const YVideoState* video);

// Whether the text mode cursor is currently drawn, and if so where. The offset
// is relative to start_address in character cells, and is only written when
// this returns true. The cursor is not drawn when it is disabled, when its
// blink phase is off, or when it addresses a cell outside the display.
Y_MODULE_PRIVATE bool YVideoGetVisibleCursorOffset(
    const YVideoState* video, const YVideoModeMetadata* metadata,
    uint16_t start_address, uint16_t* cursor_offset);

// Render a dirty region of the current MDA text display. Pixels are emitted in
// row-major order.
Y_MODULE_PRIVATE void YVideoMDARenderRegion(
    YVideoState* video, YVideoPixelRun* run, uint8_t start_column,
    uint8_t end_column, uint16_t first_y, uint16_t end_y);

// Render a dirty region of the current CGA display. Pixels are emitted in
// row-major order.
Y_MODULE_PRIVATE void YVideoCGARenderRegion(
    YVideoState* video, YVideoPixelRun* run, uint8_t start_column,
    uint8_t end_column, uint16_t first_y, uint16_t end_y);

#endif  // YAX86_VIDEO_INTERNAL_H


// ==============================================================================
// src/video/internal.h end
// ==============================================================================

// ==============================================================================
// src/video/fonts.c start
// ==============================================================================

#line 1 "./src/video/fonts.c"
#ifndef YAX86_IMPLEMENTATION
#include "fonts.h"

#include "../util/common.h"
#endif  // YAX86_IMPLEMENTATION

// MDA 9x14 font bitmaps. Each character is actually 8x14, with the rightmost
// column left blank except for block characters.
//
// From pcface project:
// https://github.com/susam/pcface/blob/main/out/oldschool-mda-9x14/fontlist.js
Y_MODULE_PRIVATE const uint16_t YVideoFontMDA9x14Bitmap[256][14] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [] (0)
    {0x00, 0x00, 0xfc, 0x102, 0x14a, 0x102, 0x102, 0x17a, 0x132, 0x102, 0xfc,
     0x00, 0x00, 0x00},  // [☺] (1)
    {0x00, 0x00, 0xfc, 0x1fe, 0x1b6, 0x1fe, 0x1fe, 0x186, 0x1ce, 0x1fe, 0xfc,
     0x00, 0x00, 0x00},  // [☻] (2)
    {0x00, 0x00, 0x00, 0x6c, 0xfe, 0xfe, 0xfe, 0xfe, 0x7c, 0x38, 0x10, 0x00,
     0x00, 0x00},  // [♥] (3)
    {0x00, 0x00, 0x00, 0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x38, 0x10, 0x00, 0x00,
     0x00, 0x00},  // [♦] (4)
    {0x00, 0x00, 0x30, 0x78, 0x78, 0x1ce, 0x1ce, 0x1ce, 0x30, 0x30, 0x78, 0x00,
     0x00, 0x00},  // [♣] (5)
    {0x00, 0x00, 0x30, 0x78, 0xfc, 0x1fe, 0x1fe, 0xfc, 0x30, 0x30, 0x78, 0x00,
     0x00, 0x00},  // [♠] (6)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x78, 0x78, 0x30, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [•] (7)
    {0x1fe, 0x1fe, 0x1fe, 0x1fe, 0x1fe, 0x1ce, 0x186, 0x186, 0x1ce, 0x1fe,
     0x1fe, 0x1fe, 0x1fe, 0x1fe},  // [◘] (8)
    {0x00, 0x00, 0x00, 0x00, 0x78, 0xcc, 0x84, 0x84, 0xcc, 0x78, 0x00, 0x00,
     0x00, 0x00},  // [○] (9)
    {0x1fe, 0x1fe, 0x1fe, 0x1fe, 0x186, 0x132, 0x17a, 0x17a, 0x132, 0x186,
     0x1fe, 0x1fe, 0x1fe, 0x1fe},  // [◙] (10)
    {0x00, 0x00, 0x1e, 0x0e, 0x1a, 0x32, 0x78, 0xcc, 0xcc, 0xcc, 0x78, 0x00,
     0x00, 0x00},  // [♂] (11)
    {0x00, 0x00, 0x78, 0xcc, 0xcc, 0xcc, 0x78, 0x30, 0xfc, 0x30, 0x30, 0x00,
     0x00, 0x00},  // [♀] (12)
    {0x00, 0x00, 0x7e, 0x66, 0x7e, 0x60, 0x60, 0x60, 0xe0, 0x1e0, 0x1c0, 0x00,
     0x00, 0x00},  // [♪] (13)
    {0x00, 0x00, 0xfe, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0xce, 0x1ce, 0x1cc, 0x180,
     0x00, 0x00},  // [♫] (14)
    {0x00, 0x00, 0x30, 0x30, 0x1b6, 0x78, 0x1ce, 0x78, 0x1b6, 0x30, 0x30, 0x00,
     0x00, 0x00},  // [☼] (15)
    {0x00, 0x00, 0x80, 0xc0, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0xc0, 0x80, 0x00,
     0x00, 0x00},  // [►] (16)
    {0x00, 0x00, 0x02, 0x06, 0x0e, 0x3e, 0xfe, 0x3e, 0x0e, 0x06, 0x02, 0x00,
     0x00, 0x00},  // [◄] (17)
    {0x00, 0x00, 0x30, 0x78, 0xfc, 0x30, 0x30, 0x30, 0xfc, 0x78, 0x30, 0x00,
     0x00, 0x00},  // [↕] (18)
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x66, 0x66, 0x00,
     0x00, 0x00},  // [‼] (19)
    {0x00, 0x00, 0xfe, 0x1b6, 0x1b6, 0x1b6, 0xf6, 0x36, 0x36, 0x36, 0x36, 0x00,
     0x00, 0x00},  // [¶] (20)
    {0x00, 0x7c, 0xc6, 0x60, 0x38, 0x6c, 0xc6, 0xc6, 0x6c, 0x38, 0x0c, 0xc6,
     0x7c, 0x00},  // [§] (21)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xfe, 0xfe, 0x00,
     0x00, 0x00},  // [▬] (22)
    {0x00, 0x00, 0x30, 0x78, 0xfc, 0x30, 0x30, 0x30, 0xfc, 0x78, 0x30, 0xfc,
     0x00, 0x00},  // [↨] (23)
    {0x00, 0x00, 0x30, 0x78, 0xfc, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00,
     0x00, 0x00},  // [↑] (24)
    {0x00, 0x00, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0xfc, 0x78, 0x30, 0x00,
     0x00, 0x00},  // [↓] (25)
    {0x00, 0x00, 0x00, 0x00, 0x18, 0x0c, 0xfe, 0x0c, 0x18, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [→] (26)
    {0x00, 0x00, 0x00, 0x00, 0x30, 0x60, 0xfe, 0x60, 0x30, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [←] (27)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xc0, 0xc0, 0xfe, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [∟] (28)
    {0x00, 0x00, 0x00, 0x00, 0x48, 0xcc, 0x1fe, 0xcc, 0x48, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [↔] (29)
    {0x00, 0x00, 0x00, 0x10, 0x38, 0x38, 0x7c, 0x7c, 0xfe, 0xfe, 0x00, 0x00,
     0x00, 0x00},  // [▲] (30)
    {0x00, 0x00, 0x00, 0xfe, 0xfe, 0x7c, 0x7c, 0x38, 0x38, 0x10, 0x00, 0x00,
     0x00, 0x00},  // [▼] (31)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [ ] (32)
    {0x00, 0x00, 0x30, 0x78, 0x78, 0x78, 0x30, 0x30, 0x00, 0x30, 0x30, 0x00,
     0x00, 0x00},  // [!] (33)
    {0x00, 0xc6, 0xc6, 0xc6, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // ["] (34)
    {0x00, 0x00, 0x6c, 0x6c, 0xfe, 0x6c, 0x6c, 0x6c, 0xfe, 0x6c, 0x6c, 0x00,
     0x00, 0x00},  // [#] (35)
    {0x18, 0x18, 0x7c, 0xc6, 0xc2, 0xc0, 0x7c, 0x06, 0x86, 0xc6, 0x7c, 0x18,
     0x18, 0x00},  // [$] (36)
    {0x00, 0x00, 0x00, 0x00, 0xc2, 0xc6, 0x0c, 0x18, 0x30, 0x66, 0xc6, 0x00,
     0x00, 0x00},  // [%] (37)
    {0x00, 0x00, 0x38, 0x6c, 0x6c, 0x38, 0x76, 0xdc, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [&] (38)
    {0x00, 0x60, 0x60, 0x60, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // ['] (39)
    {0x00, 0x00, 0x18, 0x30, 0x60, 0x60, 0x60, 0x60, 0x60, 0x30, 0x18, 0x00,
     0x00, 0x00},  // [(] (40)
    {0x00, 0x00, 0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x18, 0x30, 0x00,
     0x00, 0x00},  // [)] (41)
    {0x00, 0x00, 0x00, 0x00, 0xcc, 0x78, 0x1fe, 0x78, 0xcc, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [*] (42)
    {0x00, 0x00, 0x00, 0x30, 0x30, 0x30, 0x1fe, 0x30, 0x30, 0x30, 0x00, 0x00,
     0x00, 0x00},  // [+] (43)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x30, 0x60,
     0x00, 0x00},  // [,] (44)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1fe, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [-] (45)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00,
     0x00, 0x00},  // [.] (46)
    {0x00, 0x00, 0x02, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x80, 0x00, 0x00,
     0x00, 0x00},  // [/] (47)
    {0x00, 0x00, 0x7c, 0xc6, 0xce, 0xde, 0xf6, 0xe6, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [0] (48)
    {0x00, 0x00, 0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00,
     0x00, 0x00},  // [1] (49)
    {0x00, 0x00, 0x7c, 0xc6, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc6, 0xfe, 0x00,
     0x00, 0x00},  // [2] (50)
    {0x00, 0x00, 0x7c, 0xc6, 0x06, 0x06, 0x3c, 0x06, 0x06, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [3] (51)
    {0x00, 0x00, 0x0c, 0x1c, 0x3c, 0x6c, 0xcc, 0xfe, 0x0c, 0x0c, 0x1e, 0x00,
     0x00, 0x00},  // [4] (52)
    {0x00, 0x00, 0xfe, 0xc0, 0xc0, 0xc0, 0xfc, 0x06, 0x06, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [5] (53)
    {0x00, 0x00, 0x38, 0x60, 0xc0, 0xc0, 0xfc, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [6] (54)
    {0x00, 0x00, 0xfe, 0xc6, 0x06, 0x0c, 0x18, 0x30, 0x30, 0x30, 0x30, 0x00,
     0x00, 0x00},  // [7] (55)
    {0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0x7c, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [8] (56)
    {0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x06, 0x0c, 0x78, 0x00,
     0x00, 0x00},  // [9] (57)
    {0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00,
     0x00, 0x00},  // [:] (58)
    {0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x30, 0x30, 0x60, 0x00,
     0x00, 0x00},  // [;] (59)
    {0x00, 0x00, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x60, 0x30, 0x18, 0x0c, 0x00,
     0x00, 0x00},  // [<] (60)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [=] (61)
    {0x00, 0x00, 0xc0, 0x60, 0x30, 0x18, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x00,
     0x00, 0x00},  // [>] (62)
    {0x00, 0x00, 0x7c, 0xc6, 0xc6, 0x0c, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00,
     0x00, 0x00},  // [?] (63)
    {0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xde, 0xde, 0xde, 0xdc, 0xc0, 0x7c, 0x00,
     0x00, 0x00},  // [@] (64)
    {0x00, 0x00, 0x10, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0x00,
     0x00, 0x00},  // [A] (65)
    {0x00, 0x00, 0xfc, 0x66, 0x66, 0x66, 0x7c, 0x66, 0x66, 0x66, 0xfc, 0x00,
     0x00, 0x00},  // [B] (66)
    {0x00, 0x00, 0x3c, 0x66, 0xc2, 0xc0, 0xc0, 0xc0, 0xc2, 0x66, 0x3c, 0x00,
     0x00, 0x00},  // [C] (67)
    {0x00, 0x00, 0xf8, 0x6c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x6c, 0xf8, 0x00,
     0x00, 0x00},  // [D] (68)
    {0x00, 0x00, 0xfe, 0x66, 0x62, 0x68, 0x78, 0x68, 0x62, 0x66, 0xfe, 0x00,
     0x00, 0x00},  // [E] (69)
    {0x00, 0x00, 0xfe, 0x66, 0x62, 0x68, 0x78, 0x68, 0x60, 0x60, 0xf0, 0x00,
     0x00, 0x00},  // [F] (70)
    {0x00, 0x00, 0x3c, 0x66, 0xc2, 0xc0, 0xc0, 0xde, 0xc6, 0x66, 0x3a, 0x00,
     0x00, 0x00},  // [G] (71)
    {0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0xc6, 0x00,
     0x00, 0x00},  // [H] (72)
    {0x00, 0x00, 0x78, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00,
     0x00, 0x00},  // [I] (73)
    {0x00, 0x00, 0x1e, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0xcc, 0xcc, 0x78, 0x00,
     0x00, 0x00},  // [J] (74)
    {0x00, 0x00, 0xe6, 0x66, 0x6c, 0x6c, 0x78, 0x6c, 0x6c, 0x66, 0xe6, 0x00,
     0x00, 0x00},  // [K] (75)
    {0x00, 0x00, 0xf0, 0x60, 0x60, 0x60, 0x60, 0x60, 0x62, 0x66, 0xfe, 0x00,
     0x00, 0x00},  // [L] (76)
    {0x00, 0x00, 0x186, 0x1ce, 0x1fe, 0x1b6, 0x186, 0x186, 0x186, 0x186, 0x186,
     0x00, 0x00, 0x00},  // [M] (77)
    {0x00, 0x00, 0xc6, 0xe6, 0xf6, 0xfe, 0xde, 0xce, 0xc6, 0xc6, 0xc6, 0x00,
     0x00, 0x00},  // [N] (78)
    {0x00, 0x00, 0x38, 0x6c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x00,
     0x00, 0x00},  // [O] (79)
    {0x00, 0x00, 0xfc, 0x66, 0x66, 0x66, 0x7c, 0x60, 0x60, 0x60, 0xf0, 0x00,
     0x00, 0x00},  // [P] (80)
    {0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xd6, 0xde, 0x7c, 0x0c, 0x0e,
     0x00, 0x00},  // [Q] (81)
    {0x00, 0x00, 0xfc, 0x66, 0x66, 0x66, 0x7c, 0x6c, 0x66, 0x66, 0xe6, 0x00,
     0x00, 0x00},  // [R] (82)
    {0x00, 0x00, 0x7c, 0xc6, 0xc6, 0x60, 0x38, 0x0c, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [S] (83)
    {0x00, 0x00, 0x1fe, 0x1b6, 0x132, 0x30, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00,
     0x00, 0x00},  // [T] (84)
    {0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [U] (85)
    {0x00, 0x00, 0x186, 0x186, 0x186, 0x186, 0x186, 0x186, 0xcc, 0x78, 0x30,
     0x00, 0x00, 0x00},  // [V] (86)
    {0x00, 0x00, 0x186, 0x186, 0x186, 0x186, 0x1b6, 0x1b6, 0x1fe, 0xcc, 0xcc,
     0x00, 0x00, 0x00},  // [W] (87)
    {0x00, 0x00, 0x186, 0x186, 0xcc, 0x78, 0x30, 0x78, 0xcc, 0x186, 0x186, 0x00,
     0x00, 0x00},  // [X] (88)
    {0x00, 0x00, 0x186, 0x186, 0x186, 0xcc, 0x78, 0x30, 0x30, 0x30, 0x78, 0x00,
     0x00, 0x00},  // [Y] (89)
    {0x00, 0x00, 0x1fe, 0x186, 0x10c, 0x18, 0x30, 0x60, 0xc2, 0x186, 0x1fe,
     0x00, 0x00, 0x00},  // [Z] (90)
    {0x00, 0x00, 0x78, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x78, 0x00,
     0x00, 0x00},  // [[] (91)
    {0x00, 0x00, 0x80, 0xc0, 0xe0, 0x70, 0x38, 0x1c, 0x0e, 0x06, 0x02, 0x00,
     0x00, 0x00},  // [\] (92)
    {0x00, 0x00, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x78, 0x00,
     0x00, 0x00},  // []] (93)
    {0x10, 0x38, 0x6c, 0xc6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [^] (94)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x1fe, 0x00},  // [_] (95)
    {0x30, 0x30, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [`] (96)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [a] (97)
    {0x00, 0x00, 0xe0, 0x60, 0x60, 0x78, 0x6c, 0x66, 0x66, 0x66, 0xdc, 0x00,
     0x00, 0x00},  // [b] (98)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc0, 0xc0, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [c] (99)
    {0x00, 0x00, 0x1c, 0x0c, 0x0c, 0x3c, 0x6c, 0xcc, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [d] (100)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [e] (101)
    {0x00, 0x00, 0x38, 0x6c, 0x64, 0x60, 0xf8, 0x60, 0x60, 0x60, 0xf0, 0x00,
     0x00, 0x00},  // [f] (102)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xcc, 0xcc, 0xcc, 0x7c, 0x0c, 0xcc,
     0x78, 0x00},  // [g] (103)
    {0x00, 0x00, 0xe0, 0x60, 0x60, 0x6c, 0x76, 0x66, 0x66, 0x66, 0xe6, 0x00,
     0x00, 0x00},  // [h] (104)
    {0x00, 0x00, 0x18, 0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00,
     0x00, 0x00},  // [i] (105)
    {0x00, 0x00, 0x0c, 0x0c, 0x00, 0x1c, 0x0c, 0x0c, 0x0c, 0x0c, 0xcc, 0xcc,
     0x78, 0x00},  // [j] (106)
    {0x00, 0x00, 0xe0, 0x60, 0x60, 0x66, 0x6c, 0x78, 0x6c, 0x66, 0xe6, 0x00,
     0x00, 0x00},  // [k] (107)
    {0x00, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00,
     0x00, 0x00},  // [l] (108)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1cc, 0x1fe, 0x1b6, 0x1b6, 0x1b6, 0x1b6,
     0x00, 0x00, 0x00},  // [m] (109)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xdc, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00,
     0x00, 0x00},  // [n] (110)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [o] (111)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xdc, 0x66, 0x66, 0x66, 0x7c, 0x60, 0x60,
     0xf0, 0x00},  // [p] (112)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xcc, 0xcc, 0xcc, 0x7c, 0x0c, 0x0c,
     0x1e, 0x00},  // [q] (113)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xdc, 0x76, 0x66, 0x60, 0x60, 0xf0, 0x00,
     0x00, 0x00},  // [r] (114)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0x70, 0x1c, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [s] (115)
    {0x00, 0x00, 0x10, 0x30, 0x30, 0xfc, 0x30, 0x30, 0x30, 0x36, 0x1c, 0x00,
     0x00, 0x00},  // [t] (116)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [u] (117)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x186, 0x186, 0x186, 0xcc, 0x78, 0x30, 0x00,
     0x00, 0x00},  // [v] (118)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x186, 0x186, 0x1b6, 0x1b6, 0x1fe, 0xcc,
     0x00, 0x00, 0x00},  // [w] (119)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xc6, 0x6c, 0x38, 0x38, 0x6c, 0xc6, 0x00,
     0x00, 0x00},  // [x] (120)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x0c,
     0x78, 0x00},  // [y] (121)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xcc, 0x18, 0x30, 0x66, 0xfe, 0x00,
     0x00, 0x00},  // [z] (122)
    {0x00, 0x00, 0x1c, 0x30, 0x30, 0x30, 0xe0, 0x30, 0x30, 0x30, 0x1c, 0x00,
     0x00, 0x00},  // [{] (123)
    {0x00, 0x00, 0x30, 0x30, 0x30, 0x30, 0x00, 0x30, 0x30, 0x30, 0x30, 0x00,
     0x00, 0x00},  // [|] (124)
    {0x00, 0x00, 0xe0, 0x30, 0x30, 0x30, 0x1c, 0x30, 0x30, 0x30, 0xe0, 0x00,
     0x00, 0x00},  // [}] (125)
    {0x00, 0x00, 0x76, 0xdc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [~] (126)
    {0x00, 0x00, 0x00, 0x00, 0x10, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0x00, 0x00,
     0x00, 0x00},  // [⌂] (127)
    {0x00, 0x00, 0x3c, 0x66, 0xc2, 0xc0, 0xc0, 0xc2, 0x66, 0x3c, 0x0c, 0x06,
     0x7c, 0x00},  // [Ç] (128)
    {0x00, 0x00, 0xcc, 0xcc, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [ü] (129)
    {0x00, 0x0c, 0x18, 0x30, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [é] (130)
    {0x00, 0x10, 0x38, 0x6c, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [â] (131)
    {0x00, 0x00, 0xcc, 0xcc, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [ä] (132)
    {0x00, 0x60, 0x30, 0x18, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [à] (133)
    {0x00, 0x38, 0x6c, 0x38, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [å] (134)
    {0x00, 0x00, 0x00, 0x00, 0x78, 0xcc, 0xc0, 0xcc, 0x78, 0x18, 0x0c, 0x78,
     0x00, 0x00},  // [ç] (135)
    {0x00, 0x10, 0x38, 0x6c, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [ê] (136)
    {0x00, 0x00, 0xcc, 0xcc, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [ë] (137)
    {0x00, 0x60, 0x30, 0x18, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [è] (138)
    {0x00, 0x00, 0xcc, 0xcc, 0x00, 0x70, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00,
     0x00, 0x00},  // [ï] (139)
    {0x00, 0x30, 0x78, 0xcc, 0x00, 0x70, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00,
     0x00, 0x00},  // [î] (140)
    {0x00, 0xc0, 0x60, 0x30, 0x00, 0x70, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00,
     0x00, 0x00},  // [ì] (141)
    {0x00, 0xc6, 0xc6, 0x10, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0x00,
     0x00, 0x00},  // [Ä] (142)
    {0x38, 0x6c, 0x38, 0x00, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0x00,
     0x00, 0x00},  // [Å] (143)
    {0x18, 0x30, 0x60, 0x00, 0xfe, 0x66, 0x60, 0x7c, 0x60, 0x66, 0xfe, 0x00,
     0x00, 0x00},  // [É] (144)
    {0x00, 0x00, 0x00, 0x00, 0xdc, 0x76, 0x36, 0xfc, 0x1b0, 0x1b8, 0xee, 0x00,
     0x00, 0x00},  // [æ] (145)
    {0x00, 0x00, 0x3e, 0x6c, 0xcc, 0xcc, 0xfe, 0xcc, 0xcc, 0xcc, 0xce, 0x00,
     0x00, 0x00},  // [Æ] (146)
    {0x00, 0x10, 0x38, 0x6c, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [ô] (147)
    {0x00, 0x00, 0xc6, 0xc6, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [ö] (148)
    {0x00, 0x60, 0x30, 0x18, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [ò] (149)
    {0x00, 0x30, 0x78, 0xcc, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [û] (150)
    {0x00, 0x60, 0x30, 0x18, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [ù] (151)
    {0x00, 0x00, 0xc6, 0xc6, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x0c,
     0x78, 0x00},  // [ÿ] (152)
    {0x00, 0xc6, 0xc6, 0x38, 0x6c, 0xc6, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x00,
     0x00, 0x00},  // [Ö] (153)
    {0x00, 0xc6, 0xc6, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [Ü] (154)
    {0x00, 0x30, 0x30, 0xfc, 0x186, 0x180, 0x180, 0x186, 0xfc, 0x30, 0x30, 0x00,
     0x00, 0x00},  // [¢] (155)
    {0x00, 0x38, 0x6c, 0x64, 0x60, 0xf0, 0x60, 0x60, 0x60, 0xe6, 0xfc, 0x00,
     0x00, 0x00},  // [£] (156)
    {0x00, 0x00, 0x186, 0xcc, 0x78, 0x30, 0x1fe, 0x30, 0x1fe, 0x30, 0x30, 0x00,
     0x00, 0x00},  // [¥] (157)
    {0x00, 0x1f8, 0xcc, 0xcc, 0xf8, 0xc4, 0xcc, 0xde, 0xcc, 0xcc, 0x1e6, 0x00,
     0x00, 0x00},  // [₧] (158)
    {0x00, 0x1c, 0x36, 0x30, 0x30, 0x30, 0xfc, 0x30, 0x30, 0x30, 0x30, 0x1b0,
     0xe0, 0x00},  // [ƒ] (159)
    {0x00, 0x18, 0x30, 0x60, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [á] (160)
    {0x00, 0x18, 0x30, 0x60, 0x00, 0x70, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00,
     0x00, 0x00},  // [í] (161)
    {0x00, 0x18, 0x30, 0x60, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [ó] (162)
    {0x00, 0x18, 0x30, 0x60, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [ú] (163)
    {0x00, 0x00, 0x76, 0xdc, 0x00, 0xdc, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00,
     0x00, 0x00},  // [ñ] (164)
    {0x76, 0xdc, 0x00, 0xc6, 0xe6, 0xf6, 0xfe, 0xde, 0xce, 0xc6, 0xc6, 0x00,
     0x00, 0x00},  // [Ñ] (165)
    {0x00, 0x78, 0xd8, 0xd8, 0x7c, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [ª] (166)
    {0x00, 0x70, 0xd8, 0xd8, 0x70, 0x00, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [º] (167)
    {0x00, 0x00, 0x30, 0x30, 0x00, 0x30, 0x30, 0x60, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [¿] (168)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xc0, 0xc0, 0xc0, 0x00, 0x00,
     0x00, 0x00},  // [⌐] (169)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x06, 0x06, 0x06, 0x00, 0x00,
     0x00, 0x00},  // [¬] (170)
    {0x00, 0xc0, 0x1c0, 0xc6, 0xcc, 0xd8, 0x30, 0x60, 0xdc, 0x186, 0x0c, 0x18,
     0x3e, 0x00},  // [½] (171)
    {0x00, 0xc0, 0x1c0, 0xc6, 0xcc, 0xd8, 0x30, 0x66, 0xce, 0x19e, 0x3e, 0x06,
     0x06, 0x00},  // [¼] (172)
    {0x00, 0x00, 0x30, 0x30, 0x00, 0x30, 0x30, 0x78, 0x78, 0x78, 0x30, 0x00,
     0x00, 0x00},  // [¡] (173)
    {0x00, 0x00, 0x00, 0x00, 0x36, 0x6c, 0xd8, 0x6c, 0x36, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [«] (174)
    {0x00, 0x00, 0x00, 0x00, 0xd8, 0x6c, 0x36, 0x6c, 0xd8, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [»] (175)
    {0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88,
     0x22, 0x88},  // [░] (176)
    {0xaa, 0x154, 0xaa, 0x154, 0xaa, 0x154, 0xaa, 0x154, 0xaa, 0x154, 0xaa,
     0x154, 0xaa, 0x154},  // [▒] (177)
    {0x1ba, 0xee, 0x1ba, 0xee, 0x1ba, 0xee, 0x1ba, 0xee, 0x1ba, 0xee, 0x1ba,
     0xee, 0x1ba, 0xee},  // [▓] (178)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [│] (179)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x1f0, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [┤] (180)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x1f0, 0x30, 0x1f0, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [╡] (181)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x1ec, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╢] (182)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1fc, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╖] (183)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1f0, 0x30, 0x1f0, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [╕] (184)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x1ec, 0x0c, 0x1ec, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╣] (185)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [║] (186)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1fc, 0x0c, 0x1ec, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╗] (187)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x1ec, 0x0c, 0x1fc, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [╝] (188)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x1fc, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [╜] (189)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x1f0, 0x30, 0x1f0, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [╛] (190)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f0, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [┐] (191)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3f, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [└] (192)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x1ff, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [┴] (193)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1ff, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [┬] (194)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3f, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [├] (195)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1ff, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [─] (196)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x1ff, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [┼] (197)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x3f, 0x30, 0x3f, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [╞] (198)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6f, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╟] (199)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6f, 0x60, 0x7f, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [╚] (200)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x60, 0x6f, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╔] (201)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x1ef, 0x00, 0x1ff, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [╩] (202)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1ff, 0x00, 0x1ef, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╦] (203)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6f, 0x60, 0x6f, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╠] (204)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1ff, 0x00, 0x1ff, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [═] (205)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x1ef, 0x00, 0x1ef, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╬] (206)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x1ff, 0x00, 0x1ff, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [╧] (207)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x1ff, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [╨] (208)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1ff, 0x00, 0x1ff, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [╤] (209)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1ff, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╥] (210)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x7f, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [╙] (211)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x3f, 0x30, 0x3f, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [╘] (212)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x30, 0x3f, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [╒] (213)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╓] (214)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x1ff, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╫] (215)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x1ff, 0x30, 0x1ff, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [╪] (216)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x1f0, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [┘] (217)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [┌] (218)
    {0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x1ff,
     0x1ff, 0x1ff, 0x1ff, 0x1ff},  // [█] (219)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1ff, 0x1ff, 0x1ff, 0x1ff,
     0x1ff, 0x1ff, 0x1ff},  // [▄] (220)
    {0x1e0, 0x1e0, 0x1e0, 0x1e0, 0x1e0, 0x1e0, 0x1e0, 0x1e0, 0x1e0, 0x1e0,
     0x1e0, 0x1e0, 0x1e0, 0x1e0},  // [▌] (221)
    {0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
     0x1f, 0x1f},  // [▐] (222)
    {0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00},  // [▀] (223)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xdc, 0xd8, 0xd8, 0xdc, 0x76, 0x00,
     0x00, 0x00},  // [α] (224)
    {0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xfc, 0xc6, 0xc6, 0xfc, 0xc0, 0xc0,
     0x40, 0x00},  // [ß] (225)
    {0x00, 0x00, 0xfe, 0xc6, 0xc6, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0x00,
     0x00, 0x00},  // [Γ] (226)
    {0x00, 0x00, 0x00, 0x00, 0xfe, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x00,
     0x00, 0x00},  // [π] (227)
    {0x00, 0x00, 0xfe, 0xc6, 0x60, 0x30, 0x18, 0x30, 0x60, 0xc6, 0xfe, 0x00,
     0x00, 0x00},  // [Σ] (228)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0xd8, 0xd8, 0xd8, 0xd8, 0x70, 0x00,
     0x00, 0x00},  // [σ] (229)
    {0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x60, 0x60, 0xc0,
     0x00, 0x00},  // [µ] (230)
    {0x00, 0x00, 0x00, 0x00, 0x76, 0xdc, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00,
     0x00, 0x00},  // [τ] (231)
    {0x00, 0x00, 0xfc, 0x30, 0x78, 0xcc, 0xcc, 0xcc, 0x78, 0x30, 0xfc, 0x00,
     0x00, 0x00},  // [Φ] (232)
    {0x00, 0x00, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0x6c, 0x38, 0x00,
     0x00, 0x00},  // [Θ] (233)
    {0x00, 0x00, 0x38, 0x6c, 0xc6, 0xc6, 0xc6, 0x6c, 0x6c, 0x6c, 0xee, 0x00,
     0x00, 0x00},  // [Ω] (234)
    {0x00, 0x00, 0x3c, 0x60, 0x30, 0x18, 0x7c, 0xcc, 0xcc, 0xcc, 0x78, 0x00,
     0x00, 0x00},  // [δ] (235)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x1b6, 0x1b6, 0xfc, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [∞] (236)
    {0x00, 0x00, 0x06, 0x0c, 0xfc, 0x1b6, 0x1b6, 0x1e6, 0xfc, 0xc0, 0x180, 0x00,
     0x00, 0x00},  // [φ] (237)
    {0x00, 0x00, 0x38, 0x60, 0xc0, 0xc0, 0xf8, 0xc0, 0xc0, 0x60, 0x38, 0x00,
     0x00, 0x00},  // [ε] (238)
    {0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x00,
     0x00, 0x00},  // [∩] (239)
    {0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0xfe, 0x00, 0x00, 0xfe, 0x00, 0x00,
     0x00, 0x00},  // [≡] (240)
    {0x00, 0x00, 0x30, 0x30, 0x30, 0x1fe, 0x30, 0x30, 0x30, 0x00, 0x1fe, 0x00,
     0x00, 0x00},  // [±] (241)
    {0x00, 0x00, 0x60, 0x30, 0x18, 0x0c, 0x18, 0x30, 0x60, 0x00, 0xfc, 0x00,
     0x00, 0x00},  // [≥] (242)
    {0x00, 0x00, 0x18, 0x30, 0x60, 0xc0, 0x60, 0x30, 0x18, 0x00, 0xfc, 0x00,
     0x00, 0x00},  // [≤] (243)
    {0x00, 0x00, 0x1c, 0x36, 0x36, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [⌠] (244)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x1b0, 0x1b0, 0xe0, 0x00,
     0x00, 0x00},  // [⌡] (245)
    {0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x1fe, 0x00, 0x00, 0x30, 0x30, 0x00,
     0x00, 0x00},  // [÷] (246)
    {0x00, 0x00, 0x00, 0x00, 0x76, 0xdc, 0x00, 0x76, 0xdc, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [≈] (247)
    {0x00, 0x70, 0xd8, 0xd8, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [°] (248)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [∙] (249)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [·] (250)
    {0x00, 0x1e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1d8, 0xd8, 0x78, 0x38, 0x00,
     0x00, 0x00},  // [√] (251)
    {0x00, 0x1b0, 0xd8, 0xd8, 0xd8, 0xd8, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [ⁿ] (252)
    {0x00, 0xe0, 0x1b0, 0x60, 0xc0, 0x190, 0x1f0, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [²] (253)
    {0x00, 0x00, 0x00, 0x00, 0x7c, 0x7c, 0x7c, 0x7c, 0x7c, 0x7c, 0x00, 0x00,
     0x00, 0x00},  // [■] (254)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [ ] (255)
};

// CGA 8x8 font bitmaps.
//
// From pcface project:
// https://github.com/susam/pcface/blob/main/out/oldschool-cga-8x8/fontlist.js
Y_MODULE_PRIVATE const uint8_t YVideoFontCGA8x8Bitmap[256][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // [] (0)
    {0x7e, 0x81, 0xa5, 0x81, 0xbd, 0x99, 0x81, 0x7e},  // [☺] (1)
    {0x7e, 0xff, 0xdb, 0xff, 0xc3, 0xe7, 0xff, 0x7e},  // [☻] (2)
    {0x6c, 0xfe, 0xfe, 0xfe, 0x7c, 0x38, 0x10, 0x00},  // [♥] (3)
    {0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x38, 0x10, 0x00},  // [♦] (4)
    {0x38, 0x7c, 0x38, 0xfe, 0xfe, 0xd6, 0x10, 0x38},  // [♣] (5)
    {0x10, 0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x10, 0x38},  // [♠] (6)
    {0x00, 0x00, 0x18, 0x3c, 0x3c, 0x18, 0x00, 0x00},  // [•] (7)
    {0xff, 0xff, 0xe7, 0xc3, 0xc3, 0xe7, 0xff, 0xff},  // [◘] (8)
    {0x00, 0x3c, 0x66, 0x42, 0x42, 0x66, 0x3c, 0x00},  // [○] (9)
    {0xff, 0xc3, 0x99, 0xbd, 0xbd, 0x99, 0xc3, 0xff},  // [◙] (10)
    {0x0f, 0x07, 0x0f, 0x7d, 0xcc, 0xcc, 0xcc, 0x78},  // [♂] (11)
    {0x3c, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x7e, 0x18},  // [♀] (12)
    {0x3f, 0x33, 0x3f, 0x30, 0x30, 0x70, 0xf0, 0xe0},  // [♪] (13)
    {0x7f, 0x63, 0x7f, 0x63, 0x63, 0x67, 0xe6, 0xc0},  // [♫] (14)
    {0x18, 0xdb, 0x3c, 0xe7, 0xe7, 0x3c, 0xdb, 0x18},  // [☼] (15)
    {0x80, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0x80, 0x00},  // [►] (16)
    {0x02, 0x0e, 0x3e, 0xfe, 0x3e, 0x0e, 0x02, 0x00},  // [◄] (17)
    {0x18, 0x3c, 0x7e, 0x18, 0x18, 0x7e, 0x3c, 0x18},  // [↕] (18)
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x66, 0x00},  // [‼] (19)
    {0x7f, 0xdb, 0xdb, 0x7b, 0x1b, 0x1b, 0x1b, 0x00},  // [¶] (20)
    {0x3e, 0x63, 0x38, 0x6c, 0x6c, 0x38, 0xcc, 0x78},  // [§] (21)
    {0x00, 0x00, 0x00, 0x00, 0x7e, 0x7e, 0x7e, 0x00},  // [▬] (22)
    {0x18, 0x3c, 0x7e, 0x18, 0x7e, 0x3c, 0x18, 0xff},  // [↨] (23)
    {0x18, 0x3c, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x00},  // [↑] (24)
    {0x18, 0x18, 0x18, 0x18, 0x7e, 0x3c, 0x18, 0x00},  // [↓] (25)
    {0x00, 0x18, 0x0c, 0xfe, 0x0c, 0x18, 0x00, 0x00},  // [→] (26)
    {0x00, 0x30, 0x60, 0xfe, 0x60, 0x30, 0x00, 0x00},  // [←] (27)
    {0x00, 0x00, 0xc0, 0xc0, 0xc0, 0xfe, 0x00, 0x00},  // [∟] (28)
    {0x00, 0x24, 0x66, 0xff, 0x66, 0x24, 0x00, 0x00},  // [↔] (29)
    {0x00, 0x18, 0x3c, 0x7e, 0xff, 0xff, 0x00, 0x00},  // [▲] (30)
    {0x00, 0xff, 0xff, 0x7e, 0x3c, 0x18, 0x00, 0x00},  // [▼] (31)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // [ ] (32)
    {0x30, 0x78, 0x78, 0x30, 0x30, 0x00, 0x30, 0x00},  // [!] (33)
    {0x6c, 0x6c, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00},  // ["] (34)
    {0x6c, 0x6c, 0xfe, 0x6c, 0xfe, 0x6c, 0x6c, 0x00},  // [#] (35)
    {0x30, 0x7c, 0xc0, 0x78, 0x0c, 0xf8, 0x30, 0x00},  // [$] (36)
    {0x00, 0xc6, 0xcc, 0x18, 0x30, 0x66, 0xc6, 0x00},  // [%] (37)
    {0x38, 0x6c, 0x38, 0x76, 0xdc, 0xcc, 0x76, 0x00},  // [&] (38)
    {0x60, 0x60, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00},  // ['] (39)
    {0x18, 0x30, 0x60, 0x60, 0x60, 0x30, 0x18, 0x00},  // [(] (40)
    {0x60, 0x30, 0x18, 0x18, 0x18, 0x30, 0x60, 0x00},  // [)] (41)
    {0x00, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x00, 0x00},  // [*] (42)
    {0x00, 0x30, 0x30, 0xfc, 0x30, 0x30, 0x00, 0x00},  // [+] (43)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x60},  // [,] (44)
    {0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00},  // [-] (45)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00},  // [.] (46)
    {0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x80, 0x00},  // [/] (47)
    {0x7c, 0xc6, 0xce, 0xde, 0xf6, 0xe6, 0x7c, 0x00},  // [0] (48)
    {0x30, 0x70, 0x30, 0x30, 0x30, 0x30, 0xfc, 0x00},  // [1] (49)
    {0x78, 0xcc, 0x0c, 0x38, 0x60, 0xcc, 0xfc, 0x00},  // [2] (50)
    {0x78, 0xcc, 0x0c, 0x38, 0x0c, 0xcc, 0x78, 0x00},  // [3] (51)
    {0x1c, 0x3c, 0x6c, 0xcc, 0xfe, 0x0c, 0x1e, 0x00},  // [4] (52)
    {0xfc, 0xc0, 0xf8, 0x0c, 0x0c, 0xcc, 0x78, 0x00},  // [5] (53)
    {0x38, 0x60, 0xc0, 0xf8, 0xcc, 0xcc, 0x78, 0x00},  // [6] (54)
    {0xfc, 0xcc, 0x0c, 0x18, 0x30, 0x30, 0x30, 0x00},  // [7] (55)
    {0x78, 0xcc, 0xcc, 0x78, 0xcc, 0xcc, 0x78, 0x00},  // [8] (56)
    {0x78, 0xcc, 0xcc, 0x7c, 0x0c, 0x18, 0x70, 0x00},  // [9] (57)
    {0x00, 0x30, 0x30, 0x00, 0x00, 0x30, 0x30, 0x00},  // [:] (58)
    {0x00, 0x30, 0x30, 0x00, 0x00, 0x30, 0x30, 0x60},  // [;] (59)
    {0x18, 0x30, 0x60, 0xc0, 0x60, 0x30, 0x18, 0x00},  // [<] (60)
    {0x00, 0x00, 0xfc, 0x00, 0x00, 0xfc, 0x00, 0x00},  // [=] (61)
    {0x60, 0x30, 0x18, 0x0c, 0x18, 0x30, 0x60, 0x00},  // [>] (62)
    {0x78, 0xcc, 0x0c, 0x18, 0x30, 0x00, 0x30, 0x00},  // [?] (63)
    {0x7c, 0xc6, 0xde, 0xde, 0xde, 0xc0, 0x78, 0x00},  // [@] (64)
    {0x30, 0x78, 0xcc, 0xcc, 0xfc, 0xcc, 0xcc, 0x00},  // [A] (65)
    {0xfc, 0x66, 0x66, 0x7c, 0x66, 0x66, 0xfc, 0x00},  // [B] (66)
    {0x3c, 0x66, 0xc0, 0xc0, 0xc0, 0x66, 0x3c, 0x00},  // [C] (67)
    {0xf8, 0x6c, 0x66, 0x66, 0x66, 0x6c, 0xf8, 0x00},  // [D] (68)
    {0xfe, 0x62, 0x68, 0x78, 0x68, 0x62, 0xfe, 0x00},  // [E] (69)
    {0xfe, 0x62, 0x68, 0x78, 0x68, 0x60, 0xf0, 0x00},  // [F] (70)
    {0x3c, 0x66, 0xc0, 0xc0, 0xce, 0x66, 0x3e, 0x00},  // [G] (71)
    {0xcc, 0xcc, 0xcc, 0xfc, 0xcc, 0xcc, 0xcc, 0x00},  // [H] (72)
    {0x78, 0x30, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00},  // [I] (73)
    {0x1e, 0x0c, 0x0c, 0x0c, 0xcc, 0xcc, 0x78, 0x00},  // [J] (74)
    {0xe6, 0x66, 0x6c, 0x78, 0x6c, 0x66, 0xe6, 0x00},  // [K] (75)
    {0xf0, 0x60, 0x60, 0x60, 0x62, 0x66, 0xfe, 0x00},  // [L] (76)
    {0xc6, 0xee, 0xfe, 0xfe, 0xd6, 0xc6, 0xc6, 0x00},  // [M] (77)
    {0xc6, 0xe6, 0xf6, 0xde, 0xce, 0xc6, 0xc6, 0x00},  // [N] (78)
    {0x38, 0x6c, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x00},  // [O] (79)
    {0xfc, 0x66, 0x66, 0x7c, 0x60, 0x60, 0xf0, 0x00},  // [P] (80)
    {0x78, 0xcc, 0xcc, 0xcc, 0xdc, 0x78, 0x1c, 0x00},  // [Q] (81)
    {0xfc, 0x66, 0x66, 0x7c, 0x6c, 0x66, 0xe6, 0x00},  // [R] (82)
    {0x78, 0xcc, 0x60, 0x30, 0x18, 0xcc, 0x78, 0x00},  // [S] (83)
    {0xfc, 0xb4, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00},  // [T] (84)
    {0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xfc, 0x00},  // [U] (85)
    {0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x78, 0x30, 0x00},  // [V] (86)
    {0xc6, 0xc6, 0xc6, 0xd6, 0xfe, 0xee, 0xc6, 0x00},  // [W] (87)
    {0xc6, 0xc6, 0x6c, 0x38, 0x38, 0x6c, 0xc6, 0x00},  // [X] (88)
    {0xcc, 0xcc, 0xcc, 0x78, 0x30, 0x30, 0x78, 0x00},  // [Y] (89)
    {0xfe, 0xc6, 0x8c, 0x18, 0x32, 0x66, 0xfe, 0x00},  // [Z] (90)
    {0x78, 0x60, 0x60, 0x60, 0x60, 0x60, 0x78, 0x00},  // [[] (91)
    {0xc0, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x02, 0x00},  // [\] (92)
    {0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x78, 0x00},  // []] (93)
    {0x10, 0x38, 0x6c, 0xc6, 0x00, 0x00, 0x00, 0x00},  // [^] (94)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff},  // [_] (95)
    {0x30, 0x30, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00},  // [`] (96)
    {0x00, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0x76, 0x00},  // [a] (97)
    {0xe0, 0x60, 0x60, 0x7c, 0x66, 0x66, 0xdc, 0x00},  // [b] (98)
    {0x00, 0x00, 0x78, 0xcc, 0xc0, 0xcc, 0x78, 0x00},  // [c] (99)
    {0x1c, 0x0c, 0x0c, 0x7c, 0xcc, 0xcc, 0x76, 0x00},  // [d] (100)
    {0x00, 0x00, 0x78, 0xcc, 0xfc, 0xc0, 0x78, 0x00},  // [e] (101)
    {0x38, 0x6c, 0x60, 0xf0, 0x60, 0x60, 0xf0, 0x00},  // [f] (102)
    {0x00, 0x00, 0x76, 0xcc, 0xcc, 0x7c, 0x0c, 0xf8},  // [g] (103)
    {0xe0, 0x60, 0x6c, 0x76, 0x66, 0x66, 0xe6, 0x00},  // [h] (104)
    {0x30, 0x00, 0x70, 0x30, 0x30, 0x30, 0x78, 0x00},  // [i] (105)
    {0x0c, 0x00, 0x0c, 0x0c, 0x0c, 0xcc, 0xcc, 0x78},  // [j] (106)
    {0xe0, 0x60, 0x66, 0x6c, 0x78, 0x6c, 0xe6, 0x00},  // [k] (107)
    {0x70, 0x30, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00},  // [l] (108)
    {0x00, 0x00, 0xcc, 0xfe, 0xfe, 0xd6, 0xc6, 0x00},  // [m] (109)
    {0x00, 0x00, 0xf8, 0xcc, 0xcc, 0xcc, 0xcc, 0x00},  // [n] (110)
    {0x00, 0x00, 0x78, 0xcc, 0xcc, 0xcc, 0x78, 0x00},  // [o] (111)
    {0x00, 0x00, 0xdc, 0x66, 0x66, 0x7c, 0x60, 0xf0},  // [p] (112)
    {0x00, 0x00, 0x76, 0xcc, 0xcc, 0x7c, 0x0c, 0x1e},  // [q] (113)
    {0x00, 0x00, 0xdc, 0x76, 0x66, 0x60, 0xf0, 0x00},  // [r] (114)
    {0x00, 0x00, 0x7c, 0xc0, 0x78, 0x0c, 0xf8, 0x00},  // [s] (115)
    {0x10, 0x30, 0x7c, 0x30, 0x30, 0x34, 0x18, 0x00},  // [t] (116)
    {0x00, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00},  // [u] (117)
    {0x00, 0x00, 0xcc, 0xcc, 0xcc, 0x78, 0x30, 0x00},  // [v] (118)
    {0x00, 0x00, 0xc6, 0xd6, 0xfe, 0xfe, 0x6c, 0x00},  // [w] (119)
    {0x00, 0x00, 0xc6, 0x6c, 0x38, 0x6c, 0xc6, 0x00},  // [x] (120)
    {0x00, 0x00, 0xcc, 0xcc, 0xcc, 0x7c, 0x0c, 0xf8},  // [y] (121)
    {0x00, 0x00, 0xfc, 0x98, 0x30, 0x64, 0xfc, 0x00},  // [z] (122)
    {0x1c, 0x30, 0x30, 0xe0, 0x30, 0x30, 0x1c, 0x00},  // [{] (123)
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00},  // [|] (124)
    {0xe0, 0x30, 0x30, 0x1c, 0x30, 0x30, 0xe0, 0x00},  // [}] (125)
    {0x76, 0xdc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // [~] (126)
    {0x00, 0x10, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0x00},  // [⌂] (127)
    {0x78, 0xcc, 0xc0, 0xcc, 0x78, 0x18, 0x0c, 0x78},  // [Ç] (128)
    {0x00, 0xcc, 0x00, 0xcc, 0xcc, 0xcc, 0x7e, 0x00},  // [ü] (129)
    {0x1c, 0x00, 0x78, 0xcc, 0xfc, 0xc0, 0x78, 0x00},  // [é] (130)
    {0x7e, 0xc3, 0x3c, 0x06, 0x3e, 0x66, 0x3f, 0x00},  // [â] (131)
    {0xcc, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0x7e, 0x00},  // [ä] (132)
    {0xe0, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0x7e, 0x00},  // [à] (133)
    {0x30, 0x30, 0x78, 0x0c, 0x7c, 0xcc, 0x7e, 0x00},  // [å] (134)
    {0x00, 0x00, 0x78, 0xc0, 0xc0, 0x78, 0x0c, 0x38},  // [ç] (135)
    {0x7e, 0xc3, 0x3c, 0x66, 0x7e, 0x60, 0x3c, 0x00},  // [ê] (136)
    {0xcc, 0x00, 0x78, 0xcc, 0xfc, 0xc0, 0x78, 0x00},  // [ë] (137)
    {0xe0, 0x00, 0x78, 0xcc, 0xfc, 0xc0, 0x78, 0x00},  // [è] (138)
    {0xcc, 0x00, 0x70, 0x30, 0x30, 0x30, 0x78, 0x00},  // [ï] (139)
    {0x7c, 0xc6, 0x38, 0x18, 0x18, 0x18, 0x3c, 0x00},  // [î] (140)
    {0xe0, 0x00, 0x70, 0x30, 0x30, 0x30, 0x78, 0x00},  // [ì] (141)
    {0xc6, 0x38, 0x6c, 0xc6, 0xfe, 0xc6, 0xc6, 0x00},  // [Ä] (142)
    {0x30, 0x30, 0x00, 0x78, 0xcc, 0xfc, 0xcc, 0x00},  // [Å] (143)
    {0x1c, 0x00, 0xfc, 0x60, 0x78, 0x60, 0xfc, 0x00},  // [É] (144)
    {0x00, 0x00, 0x7f, 0x0c, 0x7f, 0xcc, 0x7f, 0x00},  // [æ] (145)
    {0x3e, 0x6c, 0xcc, 0xfe, 0xcc, 0xcc, 0xce, 0x00},  // [Æ] (146)
    {0x78, 0xcc, 0x00, 0x78, 0xcc, 0xcc, 0x78, 0x00},  // [ô] (147)
    {0x00, 0xcc, 0x00, 0x78, 0xcc, 0xcc, 0x78, 0x00},  // [ö] (148)
    {0x00, 0xe0, 0x00, 0x78, 0xcc, 0xcc, 0x78, 0x00},  // [ò] (149)
    {0x78, 0xcc, 0x00, 0xcc, 0xcc, 0xcc, 0x7e, 0x00},  // [û] (150)
    {0x00, 0xe0, 0x00, 0xcc, 0xcc, 0xcc, 0x7e, 0x00},  // [ù] (151)
    {0x00, 0xcc, 0x00, 0xcc, 0xcc, 0x7c, 0x0c, 0xf8},  // [ÿ] (152)
    {0xc3, 0x18, 0x3c, 0x66, 0x66, 0x3c, 0x18, 0x00},  // [Ö] (153)
    {0xcc, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0x78, 0x00},  // [Ü] (154)
    {0x18, 0x18, 0x7e, 0xc0, 0xc0, 0x7e, 0x18, 0x18},  // [¢] (155)
    {0x38, 0x6c, 0x64, 0xf0, 0x60, 0xe6, 0xfc, 0x00},  // [£] (156)
    {0xcc, 0xcc, 0x78, 0xfc, 0x30, 0xfc, 0x30, 0x30},  // [¥] (157)
    {0xf8, 0xcc, 0xcc, 0xfa, 0xc6, 0xcf, 0xc6, 0xc7},  // [₧] (158)
    {0x0e, 0x1b, 0x18, 0x3c, 0x18, 0x18, 0xd8, 0x70},  // [ƒ] (159)
    {0x1c, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0x7e, 0x00},  // [á] (160)
    {0x38, 0x00, 0x70, 0x30, 0x30, 0x30, 0x78, 0x00},  // [í] (161)
    {0x00, 0x1c, 0x00, 0x78, 0xcc, 0xcc, 0x78, 0x00},  // [ó] (162)
    {0x00, 0x1c, 0x00, 0xcc, 0xcc, 0xcc, 0x7e, 0x00},  // [ú] (163)
    {0x00, 0xf8, 0x00, 0xf8, 0xcc, 0xcc, 0xcc, 0x00},  // [ñ] (164)
    {0xfc, 0x00, 0xcc, 0xec, 0xfc, 0xdc, 0xcc, 0x00},  // [Ñ] (165)
    {0x3c, 0x6c, 0x6c, 0x3e, 0x00, 0x7e, 0x00, 0x00},  // [ª] (166)
    {0x38, 0x6c, 0x6c, 0x38, 0x00, 0x7c, 0x00, 0x00},  // [º] (167)
    {0x30, 0x00, 0x30, 0x60, 0xc0, 0xcc, 0x78, 0x00},  // [¿] (168)
    {0x00, 0x00, 0x00, 0xfc, 0xc0, 0xc0, 0x00, 0x00},  // [⌐] (169)
    {0x00, 0x00, 0x00, 0xfc, 0x0c, 0x0c, 0x00, 0x00},  // [¬] (170)
    {0xc3, 0xc6, 0xcc, 0xde, 0x33, 0x66, 0xcc, 0x0f},  // [½] (171)
    {0xc3, 0xc6, 0xcc, 0xdb, 0x37, 0x6f, 0xcf, 0x03},  // [¼] (172)
    {0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00},  // [¡] (173)
    {0x00, 0x33, 0x66, 0xcc, 0x66, 0x33, 0x00, 0x00},  // [«] (174)
    {0x00, 0xcc, 0x66, 0x33, 0x66, 0xcc, 0x00, 0x00},  // [»] (175)
    {0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88},  // [░] (176)
    {0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa},  // [▒] (177)
    {0xdb, 0x77, 0xdb, 0xee, 0xdb, 0x77, 0xdb, 0xee},  // [▓] (178)
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18},  // [│] (179)
    {0x18, 0x18, 0x18, 0x18, 0xf8, 0x18, 0x18, 0x18},  // [┤] (180)
    {0x18, 0x18, 0xf8, 0x18, 0xf8, 0x18, 0x18, 0x18},  // [╡] (181)
    {0x36, 0x36, 0x36, 0x36, 0xf6, 0x36, 0x36, 0x36},  // [╢] (182)
    {0x00, 0x00, 0x00, 0x00, 0xfe, 0x36, 0x36, 0x36},  // [╖] (183)
    {0x00, 0x00, 0xf8, 0x18, 0xf8, 0x18, 0x18, 0x18},  // [╕] (184)
    {0x36, 0x36, 0xf6, 0x06, 0xf6, 0x36, 0x36, 0x36},  // [╣] (185)
    {0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36},  // [║] (186)
    {0x00, 0x00, 0xfe, 0x06, 0xf6, 0x36, 0x36, 0x36},  // [╗] (187)
    {0x36, 0x36, 0xf6, 0x06, 0xfe, 0x00, 0x00, 0x00},  // [╝] (188)
    {0x36, 0x36, 0x36, 0x36, 0xfe, 0x00, 0x00, 0x00},  // [╜] (189)
    {0x18, 0x18, 0xf8, 0x18, 0xf8, 0x00, 0x00, 0x00},  // [╛] (190)
    {0x00, 0x00, 0x00, 0x00, 0xf8, 0x18, 0x18, 0x18},  // [┐] (191)
    {0x18, 0x18, 0x18, 0x18, 0x1f, 0x00, 0x00, 0x00},  // [└] (192)
    {0x18, 0x18, 0x18, 0x18, 0xff, 0x00, 0x00, 0x00},  // [┴] (193)
    {0x00, 0x00, 0x00, 0x00, 0xff, 0x18, 0x18, 0x18},  // [┬] (194)
    {0x18, 0x18, 0x18, 0x18, 0x1f, 0x18, 0x18, 0x18},  // [├] (195)
    {0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00},  // [─] (196)
    {0x18, 0x18, 0x18, 0x18, 0xff, 0x18, 0x18, 0x18},  // [┼] (197)
    {0x18, 0x18, 0x1f, 0x18, 0x1f, 0x18, 0x18, 0x18},  // [╞] (198)
    {0x36, 0x36, 0x36, 0x36, 0x37, 0x36, 0x36, 0x36},  // [╟] (199)
    {0x36, 0x36, 0x37, 0x30, 0x3f, 0x00, 0x00, 0x00},  // [╚] (200)
    {0x00, 0x00, 0x3f, 0x30, 0x37, 0x36, 0x36, 0x36},  // [╔] (201)
    {0x36, 0x36, 0xf7, 0x00, 0xff, 0x00, 0x00, 0x00},  // [╩] (202)
    {0x00, 0x00, 0xff, 0x00, 0xf7, 0x36, 0x36, 0x36},  // [╦] (203)
    {0x36, 0x36, 0x37, 0x30, 0x37, 0x36, 0x36, 0x36},  // [╠] (204)
    {0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00},  // [═] (205)
    {0x36, 0x36, 0xf7, 0x00, 0xf7, 0x36, 0x36, 0x36},  // [╬] (206)
    {0x18, 0x18, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00},  // [╧] (207)
    {0x36, 0x36, 0x36, 0x36, 0xff, 0x00, 0x00, 0x00},  // [╨] (208)
    {0x00, 0x00, 0xff, 0x00, 0xff, 0x18, 0x18, 0x18},  // [╤] (209)
    {0x00, 0x00, 0x00, 0x00, 0xff, 0x36, 0x36, 0x36},  // [╥] (210)
    {0x36, 0x36, 0x36, 0x36, 0x3f, 0x00, 0x00, 0x00},  // [╙] (211)
    {0x18, 0x18, 0x1f, 0x18, 0x1f, 0x00, 0x00, 0x00},  // [╘] (212)
    {0x00, 0x00, 0x1f, 0x18, 0x1f, 0x18, 0x18, 0x18},  // [╒] (213)
    {0x00, 0x00, 0x00, 0x00, 0x3f, 0x36, 0x36, 0x36},  // [╓] (214)
    {0x36, 0x36, 0x36, 0x36, 0xff, 0x36, 0x36, 0x36},  // [╫] (215)
    {0x18, 0x18, 0xff, 0x18, 0xff, 0x18, 0x18, 0x18},  // [╪] (216)
    {0x18, 0x18, 0x18, 0x18, 0xf8, 0x00, 0x00, 0x00},  // [┘] (217)
    {0x00, 0x00, 0x00, 0x00, 0x1f, 0x18, 0x18, 0x18},  // [┌] (218)
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},  // [█] (219)
    {0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff},  // [▄] (220)
    {0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0},  // [▌] (221)
    {0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f},  // [▐] (222)
    {0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00},  // [▀] (223)
    {0x00, 0x00, 0x76, 0xdc, 0xc8, 0xdc, 0x76, 0x00},  // [α] (224)
    {0x00, 0x78, 0xcc, 0xf8, 0xcc, 0xf8, 0xc0, 0xc0},  // [ß] (225)
    {0x00, 0xfc, 0xcc, 0xc0, 0xc0, 0xc0, 0xc0, 0x00},  // [Γ] (226)
    {0x00, 0xfe, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x00},  // [π] (227)
    {0xfc, 0xcc, 0x60, 0x30, 0x60, 0xcc, 0xfc, 0x00},  // [Σ] (228)
    {0x00, 0x00, 0x7e, 0xd8, 0xd8, 0xd8, 0x70, 0x00},  // [σ] (229)
    {0x00, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x60, 0xc0},  // [µ] (230)
    {0x00, 0x76, 0xdc, 0x18, 0x18, 0x18, 0x18, 0x00},  // [τ] (231)
    {0xfc, 0x30, 0x78, 0xcc, 0xcc, 0x78, 0x30, 0xfc},  // [Φ] (232)
    {0x38, 0x6c, 0xc6, 0xfe, 0xc6, 0x6c, 0x38, 0x00},  // [Θ] (233)
    {0x38, 0x6c, 0xc6, 0xc6, 0x6c, 0x6c, 0xee, 0x00},  // [Ω] (234)
    {0x1c, 0x30, 0x18, 0x7c, 0xcc, 0xcc, 0x78, 0x00},  // [δ] (235)
    {0x00, 0x00, 0x7e, 0xdb, 0xdb, 0x7e, 0x00, 0x00},  // [∞] (236)
    {0x06, 0x0c, 0x7e, 0xdb, 0xdb, 0x7e, 0x60, 0xc0},  // [φ] (237)
    {0x38, 0x60, 0xc0, 0xf8, 0xc0, 0x60, 0x38, 0x00},  // [ε] (238)
    {0x78, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x00},  // [∩] (239)
    {0x00, 0xfc, 0x00, 0xfc, 0x00, 0xfc, 0x00, 0x00},  // [≡] (240)
    {0x30, 0x30, 0xfc, 0x30, 0x30, 0x00, 0xfc, 0x00},  // [±] (241)
    {0x60, 0x30, 0x18, 0x30, 0x60, 0x00, 0xfc, 0x00},  // [≥] (242)
    {0x18, 0x30, 0x60, 0x30, 0x18, 0x00, 0xfc, 0x00},  // [≤] (243)
    {0x0e, 0x1b, 0x1b, 0x18, 0x18, 0x18, 0x18, 0x18},  // [⌠] (244)
    {0x18, 0x18, 0x18, 0x18, 0x18, 0xd8, 0xd8, 0x70},  // [⌡] (245)
    {0x30, 0x30, 0x00, 0xfc, 0x00, 0x30, 0x30, 0x00},  // [÷] (246)
    {0x00, 0x76, 0xdc, 0x00, 0x76, 0xdc, 0x00, 0x00},  // [≈] (247)
    {0x38, 0x6c, 0x6c, 0x38, 0x00, 0x00, 0x00, 0x00},  // [°] (248)
    {0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00},  // [∙] (249)
    {0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00},  // [·] (250)
    {0x0f, 0x0c, 0x0c, 0x0c, 0xec, 0x6c, 0x3c, 0x1c},  // [√] (251)
    {0x78, 0x6c, 0x6c, 0x6c, 0x6c, 0x00, 0x00, 0x00},  // [ⁿ] (252)
    {0x70, 0x18, 0x30, 0x60, 0x78, 0x00, 0x00, 0x00},  // [²] (253)
    {0x00, 0x00, 0x3c, 0x3c, 0x3c, 0x3c, 0x00, 0x00},  // [■] (254)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // [ ] (255)
};


// ==============================================================================
// src/video/fonts.c end
// ==============================================================================

// ==============================================================================
// src/video/mda.c start
// ==============================================================================

#line 1 "./src/video/mda.c"
#ifndef YAX86_IMPLEMENTATION
#include "fonts.h"
#include "internal.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

enum {
  // Scan line of the underline within an MDA character cell.
  Y_VIDEO_MDA_UNDERLINE_POSITION = 12,
  // Documented three-bit foreground and background combinations.
  Y_VIDEO_MDA_ATTRIBUTE_NORMAL = 0x07,
  Y_VIDEO_MDA_ATTRIBUTE_INVERSE = 0x00,
  Y_VIDEO_MDA_ATTRIBUTE_UNDERLINE = 0x01,
};

typedef struct YVideoMDACellColors {
  const YVideoRGB* foreground;
  const YVideoRGB* background;
  bool underline;
} YVideoMDACellColors;

// Decode the documented normal, inverse, invisible, underline, intensity and
// blink combinations. Undefined combinations are rendered as normal text.
Y_FILE_PRIVATE YVideoMDACellColors
YVideoMDADecodeAttribute(YVideoState* video, uint8_t attr_value) {
  const YVideoConfig* config = &video->config;
  YVideoMDACellColors colors = {
      .foreground = &config->foreground,
      .background = &config->background,
      .underline = false,
  };

  bool intense = (attr_value & Y_VIDEO_ATTRIBUTE_INTENSE_FOREGROUND) != 0;
  const YVideoRGB* intense_aware_foreground =
      intense ? &config->intense_foreground : &config->foreground;
  uint8_t background_attr = (attr_value & Y_VIDEO_ATTRIBUTE_BACKGROUND_MASK) >>
                            Y_VIDEO_ATTRIBUTE_BACKGROUND_SHIFT;
  uint8_t foreground_attr = attr_value & Y_VIDEO_ATTRIBUTE_FOREGROUND_MASK;

  if (background_attr == Y_VIDEO_MDA_ATTRIBUTE_INVERSE &&
      foreground_attr == Y_VIDEO_MDA_ATTRIBUTE_NORMAL) {
    // Normal video mode.
    colors.foreground = intense_aware_foreground;
  } else if (
      background_attr == Y_VIDEO_MDA_ATTRIBUTE_NORMAL &&
      foreground_attr == Y_VIDEO_MDA_ATTRIBUTE_INVERSE) {
    // Inverse video mode.
    colors.foreground = &config->background;
    colors.background = &config->foreground;
  } else if (
      background_attr == Y_VIDEO_MDA_ATTRIBUTE_INVERSE &&
      foreground_attr == Y_VIDEO_MDA_ATTRIBUTE_INVERSE) {
    // Invisible mode.
    colors.foreground = &config->background;
  } else if (
      background_attr == Y_VIDEO_MDA_ATTRIBUTE_INVERSE &&
      foreground_attr == Y_VIDEO_MDA_ATTRIBUTE_UNDERLINE) {
    // Underline mode.
    colors.underline = true;
    colors.foreground = intense_aware_foreground;
  } else {
    // Other combinations are treated as normal.
    colors.foreground = intense_aware_foreground;
  }

  // Blinking characters alternate between the character and blank. The blink
  // attribute only applies if blinking is enabled in the mode control register.
  if ((attr_value & Y_VIDEO_ATTRIBUTE_BLINK) &&
      (video->control_register & Y_VIDEO_CONTROL_ENABLE_BLINK) &&
      !YVideoIsTextBlinkOn(video)) {
    colors.foreground = colors.background;
    colors.underline = false;
  }
  return colors;
}

// Render a rectangular slice directly from text VRAM. Iterating scan lines
// first makes write_pixels a row-major stream suitable for an SPI window.
Y_MODULE_PRIVATE void YVideoMDARenderRegion(
    YVideoState* video, YVideoPixelRun* run, uint8_t start_column,
    uint8_t end_column, uint16_t first_y, uint16_t end_y) {
  // The MDA has exactly one mode, so its metadata is looked up directly
  // rather than derived from the mode control register.
  const YVideoModeMetadata* metadata =
      &YVideoModeMetadataTable[Y_VIDEO_MODE_MDA_TEXT_80X25];

  // The cursor is resolved once for the region rather than per cell: it can
  // only be in one place, so the loop just tests each cell against it.
  uint16_t start_address = YVideoGetStartAddress(video);
  uint16_t cursor_offset = 0;
  bool cursor_visible = YVideoGetVisibleCursorOffset(
      video, metadata, start_address, &cursor_offset);
  uint16_t cursor_start = YVideoScaleTextScanLineStart(
      video, metadata, YVideoGetCursorStartScanLine(video));
  uint16_t cursor_end = YVideoScaleTextScanLineEnd(
      video, metadata, YVideoGetCursorEndScanLine(video));

  // The adapter's own 9x14 cell, or a host text font's, with the underline
  // moved to match.
  const uint8_t cell_width = YVideoGetTextCellWidth(video, metadata);
  const uint8_t cell_height = YVideoGetTextCellHeight(video, metadata);
  const uint16_t underline_scan_line = YVideoScaleTextScanLineStart(
      video, metadata, Y_VIDEO_MDA_UNDERLINE_POSITION);

  // Scan lines are the outer loop so that pixels leave in row-major order,
  // which is what a display addressed by transfer window expects. The cost is
  // re-reading a cell's character and attribute once per scan line it covers.
  for (uint16_t y = first_y; y < end_y; ++y) {
    YVideoPixelRunBegin(run, (uint16_t)start_column * cell_width, y);
    uint8_t row = (uint8_t)(y / cell_height);
    uint8_t char_scan_line = (uint8_t)(y % cell_height);
    for (uint8_t col = start_column; col < end_column; ++col) {
      // Character and attribute occupy consecutive bytes of a cell. The 6845
      // start address can push a cell past the end of VRAM, which wraps.
      uint16_t cell_offset = (uint16_t)row * metadata->columns + col;
      uint32_t char_address = ((uint32_t)start_address + cell_offset) * 2;
      char_address &= metadata->vram_size - 1;
      uint8_t char_value = YVideoReadVRAMByte(video, char_address);
      uint8_t attr_value = YVideoReadVRAMByte(video, char_address + 1);
      YVideoMDACellColors colors = YVideoMDADecodeAttribute(video, attr_value);

      // The underline is a solid rule across the cell, so it replaces the
      // glyph row outright on the one scan line it occupies. The glyph row is
      // a bitmap with the leftmost pixel in the high bit, and is 16 bits wide
      // because an MDA cell is 9 pixels across.
      uint16_t row_bitmap;
      if (char_scan_line == underline_scan_line && colors.underline) {
        row_bitmap = 0xFFFF;
      } else if (video->has_text_font) {
        row_bitmap = YVideoGetTextFontRow(video, char_value, char_scan_line);
      } else {
        row_bitmap = YVideoFontMDA9x14Bitmap[char_value][char_scan_line];
      }
      bool cursor_scan_line = cursor_visible && cursor_offset == cell_offset &&
                              char_scan_line >= cursor_start &&
                              char_scan_line <= cursor_end;
      for (uint8_t x = 0; x < cell_width; ++x) {
        bool is_foreground = (row_bitmap & (1 << (cell_width - 1 - x))) != 0;
        // The cursor overrides the cell entirely, including a blinking
        // character that is currently hidden.
        const YVideoRGB* rgb =
            cursor_scan_line
                ? &video->config.foreground
                : (is_foreground ? colors.foreground : colors.background);
        YVideoPixelRunPush(run, *rgb);
      }
    }
  }
}


// ==============================================================================
// src/video/mda.c end
// ==============================================================================

// ==============================================================================
// src/video/cga.c start
// ==============================================================================

#line 1 "./src/video/cga.c"
#ifndef YAX86_IMPLEMENTATION
#include "fonts.h"
#include "internal.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

enum {
  Y_VIDEO_CGA40_COLUMN_COUNT = 40,
  Y_VIDEO_CGA40_COLUMN_SCALE = 2,
  Y_VIDEO_CGA_320X200_HORIZONTAL_SCALE = 2,
  // Number of pixels per byte in 320x200 graphics mode.
  Y_VIDEO_CGA_PIXELS_PER_BYTE_320X200 = 4,
  // Number of bits per pixel in 320x200 graphics mode.
  Y_VIDEO_CGA_BITS_PER_PIXEL_320X200 = 2,
  // Mask of a single pixel in 320x200 graphics mode.
  Y_VIDEO_CGA_PIXEL_MASK_320X200 = 0x03,
  // Number of frame buffer pixels per byte in 320x200 graphics mode, after
  // each pixel is doubled horizontally.
  Y_VIDEO_CGA_FRAME_BUFFER_PIXELS_PER_BYTE_320X200 =
          Y_VIDEO_CGA_PIXELS_PER_BYTE_320X200 *
      Y_VIDEO_CGA_320X200_HORIZONTAL_SCALE,
  // Number of pixels per byte in 640x200 graphics mode.
  Y_VIDEO_CGA_PIXELS_PER_BYTE_640X200 = 8,
  // Mask of a graphics mode address within the half of VRAM holding either the
  // even or the odd scan lines.
  Y_VIDEO_CGA_GRAPHICS_SCAN_LINE_ADDRESS_MASK =
          Y_VIDEO_CGA_GRAPHICS_ODD_SCAN_LINE_OFFSET -
      1,
};

// The three 320x200 graphics palettes, each holding colors 1 to 3. Color 0
// comes from the color select register instead. The palette-select bit chooses
// the first two, unless black-and-white mode selects the third.
Y_FILE_PRIVATE const uint8_t YVideoCGAGraphicsPalettes[3][3] = {
    // Palette 0: green, red, brown.
    {2, 4, 6},
    // Palette 1: cyan, magenta, light gray.
    {3, 5, 7},
    // Black and white: cyan, red, light gray.
    {3, 4, 7},
};

Y_FILE_PRIVATE inline YVideoRGB YVideoCGAGetColor(
    const YVideoState* video, uint8_t color) {
  return video->config.cga_palette[color & (Y_VIDEO_NUM_CGA_COLORS - 1)];
}

// Address of the first byte of a graphics mode scan line. Even and odd scan
// lines occupy separate halves of CGA VRAM.
Y_FILE_PRIVATE inline uint32_t YVideoCGAGetScanLineAddress(
    const YVideoState* video, uint16_t y) {
  uint32_t address =
      ((uint32_t)YVideoGetStartAddress(video) * 2 +
       (uint32_t)(y / 2) * Y_VIDEO_CGA_GRAPHICS_BYTES_PER_SCAN_LINE) &
      Y_VIDEO_CGA_GRAPHICS_SCAN_LINE_ADDRESS_MASK;
  return address + (y & 1 ? Y_VIDEO_CGA_GRAPHICS_ODD_SCAN_LINE_OFFSET : 0);
}

Y_FILE_PRIVATE void YVideoCGARenderTextRegion(
    YVideoState* video, YVideoPixelRun* run, const YVideoModeMetadata* metadata,
    uint8_t start_column, uint8_t end_column, uint16_t first_y,
    uint16_t end_y) {
  // A 40-column cell is drawn twice as wide, so it spans two dirty columns
  // and the range converts back to character cells by halving. Invalidation
  // always scales a character column by the same factor, so the range is
  // aligned to whole cells and neither end truncates.
  uint8_t scale = metadata->columns == Y_VIDEO_CGA40_COLUMN_COUNT
                      ? Y_VIDEO_CGA40_COLUMN_SCALE
                      : 1;
  uint8_t first_char = scale == Y_VIDEO_CGA40_COLUMN_SCALE
                           ? start_column / Y_VIDEO_CGA40_COLUMN_SCALE
                           : start_column;
  uint8_t end_char = scale == Y_VIDEO_CGA40_COLUMN_SCALE
                         ? end_column / Y_VIDEO_CGA40_COLUMN_SCALE
                         : end_column;

  // The cursor is resolved once for the region rather than per cell: it can
  // only be in one place, so the loop just tests each cell against it.
  uint16_t start_address = YVideoGetStartAddress(video);
  uint16_t cursor_offset = 0;
  bool cursor_visible = YVideoGetVisibleCursorOffset(
      video, metadata, start_address, &cursor_offset);
  uint16_t cursor_start = YVideoScaleTextScanLineStart(
      video, metadata, YVideoGetCursorStartScanLine(video));
  uint16_t cursor_end = YVideoScaleTextScanLineEnd(
      video, metadata, YVideoGetCursorEndScanLine(video));

  // The adapter's own 8x8 cell, or a host text font's.
  const uint8_t cell_width = YVideoGetTextCellWidth(video, metadata);
  const uint8_t cell_height = YVideoGetTextCellHeight(video, metadata);

  // Scan lines are the outer loop so that pixels leave in row-major order,
  // which is what a display addressed by transfer window expects. The cost is
  // re-reading a cell's character and attribute once per scan line it covers.
  for (uint16_t y = first_y; y < end_y; ++y) {
    // A 40-column cell is twice as wide but there are half as many, so a row
    // starts at the same pixel either way.
    YVideoPixelRunBegin(run, (uint16_t)start_column * cell_width, y);
    uint8_t row = (uint8_t)(y / cell_height);
    uint8_t char_scan_line = (uint8_t)(y % cell_height);
    for (uint8_t col = first_char; col < end_char; ++col) {
      // Character and attribute occupy consecutive bytes of a cell.
      uint16_t cell_offset = (uint16_t)row * metadata->columns + col;
      uint32_t char_address = ((uint32_t)start_address + cell_offset) * 2;
      uint8_t char_value = YVideoReadVRAMByte(video, char_address);
      uint8_t attr_value = YVideoReadVRAMByte(video, char_address + 1);

      uint8_t foreground_color =
          attr_value & (Y_VIDEO_ATTRIBUTE_FOREGROUND_MASK |
                        Y_VIDEO_ATTRIBUTE_INTENSE_FOREGROUND);
      // The cursor uses the cell's foreground even when that cell's blinking
      // character is currently hidden.
      YVideoRGB cursor_foreground = YVideoCGAGetColor(video, foreground_color);
      uint8_t background_color =
          (attr_value & Y_VIDEO_ATTRIBUTE_BACKGROUND_MASK) >>
          Y_VIDEO_ATTRIBUTE_BACKGROUND_SHIFT;
      // Bit 7 is the blink attribute only while blinking is enabled. With it
      // disabled the same bit is the background's intensity, which is how the
      // CGA reaches all 16 background colors.
      if (video->control_register & Y_VIDEO_CONTROL_ENABLE_BLINK) {
        if ((attr_value & Y_VIDEO_ATTRIBUTE_BLINK) &&
            !YVideoIsTextBlinkOn(video)) {
          foreground_color = background_color;
        }
      } else if (attr_value & Y_VIDEO_ATTRIBUTE_BLINK) {
        background_color |= Y_VIDEO_NUM_CGA_COLORS / 2;
      }

      YVideoRGB foreground = YVideoCGAGetColor(video, foreground_color);
      YVideoRGB background = YVideoCGAGetColor(video, background_color);
      bool cursor_scan_line = cursor_visible && cursor_offset == cell_offset &&
                              char_scan_line >= cursor_start &&
                              char_scan_line <= cursor_end;
      // The glyph row is a bitmap with the leftmost pixel in the high bit.
      uint8_t row_bitmap =
          video->has_text_font
              ? YVideoGetTextFontRow(video, char_value, char_scan_line)
              : YVideoFontCGA8x8Bitmap[char_value][char_scan_line];
      for (uint8_t x = 0; x < cell_width; ++x) {
        bool is_foreground = (row_bitmap & (1 << (cell_width - 1 - x))) != 0;
        YVideoRGB rgb = cursor_scan_line
                            ? cursor_foreground
                            : (is_foreground ? foreground : background);
        // In 40-column mode each glyph pixel is emitted twice, which is how
        // the adapter fills the same 640 dot line with half the characters.
        for (uint8_t i = 0; i < scale; ++i) {
          YVideoPixelRunPush(run, rgb);
        }
      }
    }
  }
}

Y_FILE_PRIVATE void YVideoCGAResolve320x200Palette(
    YVideoState* video, YVideoRGB palette[4]) {
  const uint8_t* palette_colors;
  if (video->control_register & Y_VIDEO_CONTROL_BLACK_AND_WHITE) {
    palette_colors = YVideoCGAGraphicsPalettes[2];
  } else if (video->color_select_register & Y_VIDEO_CGA_COLOR_SELECT_PALETTE) {
    palette_colors = YVideoCGAGraphicsPalettes[1];
  } else {
    palette_colors = YVideoCGAGraphicsPalettes[0];
  }
  uint8_t intensity = (video->color_select_register &
                       Y_VIDEO_CGA_COLOR_SELECT_PALETTE_INTENSITY)
                          ? Y_VIDEO_NUM_CGA_COLORS / 2
                          : 0;
  palette[0] = YVideoCGAGetColor(
      video,
      video->color_select_register & (Y_VIDEO_CGA_COLOR_SELECT_COLOR_MASK |
                                      Y_VIDEO_CGA_COLOR_SELECT_INTENSITY));
  for (uint8_t i = 0; i < 3; ++i) {
    palette[i + 1] = YVideoCGAGetColor(video, palette_colors[i] | intensity);
  }
}

Y_FILE_PRIVATE void YVideoCGARenderGraphics320x200Region(
    YVideoState* video, YVideoPixelRun* run, uint8_t start_column,
    uint8_t end_column, uint16_t first_y, uint16_t end_y) {
  YVideoRGB palette[4];
  YVideoCGAResolve320x200Palette(video, palette);

  for (uint16_t y = first_y; y < end_y; ++y) {
    YVideoPixelRunBegin(
        run,
        (uint16_t)start_column *
            Y_VIDEO_CGA_FRAME_BUFFER_PIXELS_PER_BYTE_320X200,
        y);
    uint32_t scan_line_address = YVideoCGAGetScanLineAddress(video, y);
    for (uint8_t byte_column = start_column; byte_column < end_column;
         ++byte_column) {
      uint8_t byte_value =
          YVideoReadVRAMByte(video, scan_line_address + byte_column);
      for (uint8_t i = 0; i < Y_VIDEO_CGA_PIXELS_PER_BYTE_320X200; ++i) {
        uint8_t shift =
            (uint8_t)((Y_VIDEO_CGA_PIXELS_PER_BYTE_320X200 - 1 - i) *
                      Y_VIDEO_CGA_BITS_PER_PIXEL_320X200);
        uint8_t color = (byte_value >> shift) & Y_VIDEO_CGA_PIXEL_MASK_320X200;
        for (uint8_t scale_x = 0;
             scale_x < Y_VIDEO_CGA_320X200_HORIZONTAL_SCALE; ++scale_x) {
          YVideoPixelRunPush(run, palette[color]);
        }
      }
    }
  }
}

Y_FILE_PRIVATE void YVideoCGARenderGraphics640x200Region(
    YVideoState* video, YVideoPixelRun* run, uint8_t start_column,
    uint8_t end_column, uint16_t first_y, uint16_t end_y) {
  YVideoRGB foreground = YVideoCGAGetColor(
      video,
      video->color_select_register & (Y_VIDEO_CGA_COLOR_SELECT_COLOR_MASK |
                                      Y_VIDEO_CGA_COLOR_SELECT_INTENSITY));
  YVideoRGB background = YVideoCGAGetColor(video, 0);

  for (uint16_t y = first_y; y < end_y; ++y) {
    YVideoPixelRunBegin(
        run, (uint16_t)start_column * Y_VIDEO_CGA_PIXELS_PER_BYTE_640X200, y);
    uint32_t scan_line_address = YVideoCGAGetScanLineAddress(video, y);
    for (uint8_t byte_column = start_column; byte_column < end_column;
         ++byte_column) {
      uint8_t byte_value =
          YVideoReadVRAMByte(video, scan_line_address + byte_column);
      for (uint8_t i = 0; i < Y_VIDEO_CGA_PIXELS_PER_BYTE_640X200; ++i) {
        uint8_t shift = (uint8_t)(Y_VIDEO_CGA_PIXELS_PER_BYTE_640X200 - 1 - i);
        bool is_foreground = (byte_value >> shift) & 1;
        YVideoPixelRunPush(run, is_foreground ? foreground : background);
      }
    }
  }
}

Y_MODULE_PRIVATE void YVideoCGARenderRegion(
    YVideoState* video, YVideoPixelRun* run, uint8_t start_column,
    uint8_t end_column, uint16_t first_y, uint16_t end_y) {
  const YVideoModeMetadata* metadata = YVideoGetModeMetadata(video);
  switch (metadata->mode) {
    case Y_VIDEO_MODE_CGA_TEXT_40X25_MONO:
    case Y_VIDEO_MODE_CGA_TEXT_40X25_COLOR:
    case Y_VIDEO_MODE_CGA_TEXT_80X25_MONO:
    case Y_VIDEO_MODE_CGA_TEXT_80X25_COLOR:
      YVideoCGARenderTextRegion(
          video, run, metadata, start_column, end_column, first_y, end_y);
      break;
    case Y_VIDEO_MODE_CGA_GRAPHICS_320X200:
    case Y_VIDEO_MODE_CGA_GRAPHICS_320X200_ALT:
      YVideoCGARenderGraphics320x200Region(
          video, run, start_column, end_column, first_y, end_y);
      break;
    case Y_VIDEO_MODE_CGA_GRAPHICS_640X200:
      YVideoCGARenderGraphics640x200Region(
          video, run, start_column, end_column, first_y, end_y);
      break;
    default:
      break;
  }
}


// ==============================================================================
// src/video/cga.c end
// ==============================================================================

// ==============================================================================
// src/video/video.c start
// ==============================================================================

#line 1 "./src/video/video.c"
#ifndef YAX86_IMPLEMENTATION
#include "internal.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

#define Y_VIDEO_ERROR(...) \
  Y_LOG(video->config.logger, &YVideoLogModule, Y_LOG_LEVEL_ERROR, __VA_ARGS__)
#define Y_VIDEO_WARN(...) \
  Y_LOG(video->config.logger, &YVideoLogModule, Y_LOG_LEVEL_WARN, __VA_ARGS__)
#define Y_VIDEO_INFO(...) \
  Y_LOG(video->config.logger, &YVideoLogModule, Y_LOG_LEVEL_INFO, __VA_ARGS__)
#define Y_VIDEO_DEBUG(...) \
  Y_LOG(video->config.logger, &YVideoLogModule, Y_LOG_LEVEL_DEBUG, __VA_ARGS__)

// Power-on 6845 register values for the IBM Monochrome Display.
Y_FILE_PRIVATE const uint8_t
    YVideoDefaultMDARegisters[Y_VIDEO_NUM_CRTC_REGISTERS] = {
        0x61, 0x50, 0x52, 0x0F, 0x19, 0x06, 0x19, 0x19, 0x02,
        0x0D, 0x0B, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// Power-on 6845 register values for the CGA in 80x25 text mode, matching the
// values GLaBIOS programs for that mode.
Y_FILE_PRIVATE const uint8_t
    YVideoDefaultCGARegisters[Y_VIDEO_NUM_CRTC_REGISTERS] = {
        0x71, 0x50, 0x5A, 0x0A, 0x1F, 0x06, 0x19, 0x1C, 0x02,
        0x07, 0x06, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

enum {
  // Value of bits 6-5 of the cursor start register that disables the cursor.
  Y_VIDEO_CRTC_CURSOR_DISABLED = 0x20,
  // Mask of bits 6-5 of the cursor start register.
  Y_VIDEO_CRTC_CURSOR_MODE_MASK = 0x60,
  // Mask of the scan line select bits in the cursor start and end registers.
  Y_VIDEO_CRTC_CURSOR_SCAN_LINE_MASK = 0x1F,

  // The 6845 latches only the low five bits of the register index.
  Y_VIDEO_CRTC_REGISTER_INDEX_MASK = 0x1F,

  // Value returned for reads that decode to nothing.
  Y_VIDEO_UNMAPPED_PORT_VALUE = 0xFF,

  Y_VIDEO_TEXT_COLUMNS40 = 40,
  Y_VIDEO_TEXT_COLUMNS80 = 80,
  Y_VIDEO_TEXT40_COLUMN_SCALE = 2,
  Y_VIDEO_TEXT80_COLUMN_SCALE = 1,
};

Y_FILE_PRIVATE void YVideoInvalidateVRAMAddress(
    YVideoState* video, uint32_t address);
Y_FILE_PRIVATE void YVideoInvalidateCursor(YVideoState* video);
Y_FILE_PRIVATE void YVideoInvalidateBlinkingText(YVideoState* video);
Y_FILE_PRIVATE void YVideoInvalidateAll(YVideoState* video);

Y_PUBLIC const YVideoAdapterMetadata* YVideoGetAdapterMetadata(
    const YVideoState* video) {
  return &YVideoAdapterMetadataTable[video->adapter];
}

// ============================================================================
// Video RAM
// ============================================================================

Y_MODULE_PRIVATE uint8_t
YVideoReadVRAMByte(YVideoState* video, uint32_t address) {
  if (!video->config.vram) {
    return Y_VIDEO_UNMAPPED_PORT_VALUE;
  }
  // VRAM is aliased throughout the adapter's window, so an address past the end
  // wraps around rather than reading nothing. The renderer relies on this when
  // the 6845 start address pushes it past the end of VRAM.
  //
  // Every adapter's VRAM size is a power of two, so the wrap is a mask rather
  // than a remainder. The size is only known at run time, so a remainder would
  // compile to a hardware divide in the middle of the render loop - and the
  // Cortex-M0+ this targets has no divide instruction at all.
  uint32_t vram_size = YVideoGetAdapterMetadata(video)->vram_size;
  return video->config.vram[address & (vram_size - 1)];
}

Y_MODULE_PRIVATE void YVideoWriteVRAMByte(
    YVideoState* video, uint32_t address, uint8_t value) {
  if (!video->config.vram) {
    return;
  }
  uint32_t vram_size = YVideoGetAdapterMetadata(video)->vram_size;
  address &= vram_size - 1;
  if (video->config.vram[address] == value) {
    return;
  }
  video->config.vram[address] = value;
  if (video->dirty_state.status == Y_VIDEO_FULL_REDRAW) {
    return;
  }
  YVideoInvalidateVRAMAddress(video, address);
}

Y_PUBLIC uint8_t YVideoReadVRAM(YVideoState* video, uint32_t address) {
  if (address >= YVideoGetAdapterMetadata(video)->vram_size) {
    return Y_VIDEO_UNMAPPED_PORT_VALUE;
  }
  return YVideoReadVRAMByte(video, address);
}

Y_PUBLIC void YVideoWriteVRAM(
    YVideoState* video, uint32_t address, uint8_t value) {
  if (address >= YVideoGetAdapterMetadata(video)->vram_size) {
    return;
  }
  YVideoWriteVRAMByte(video, address, value);
}

// ============================================================================
// Initialization
// ============================================================================

// Whether a host text font is one the renderer can draw. A glyph row is a
// byte, which bounds the width; the height only has to be non-zero.
Y_FILE_PRIVATE bool YVideoIsTextFontDrawable(const YVideoTextFont* font) {
  return font->width >= 1 && font->width <= Y_VIDEO_TEXT_FONT_MAX_WIDTH &&
         font->height >= 1;
}

Y_PUBLIC void YVideoInit(YVideoState* video) {
  video->adapter = video->config.adapter == Y_VIDEO_ADAPTER_CGA
                       ? Y_VIDEO_ADAPTER_CGA
                       : Y_VIDEO_ADAPTER_MDA;

  video->has_text_font = false;
  if (video->config.text_font.glyphs) {
    if (YVideoIsTextFontDrawable(&video->config.text_font)) {
      video->has_text_font = true;
    } else {
      // The adapter's own font is drawn instead.
      Y_VIDEO_ERROR(
          "text_font_not_drawable",
          Y_LOG_UINT("width", video->config.text_font.width),
          Y_LOG_UINT("height", video->config.text_font.height));
    }
  }

  const YVideoAdapterMetadata* adapter = YVideoGetAdapterMetadata(video);
  const uint8_t* default_registers = video->adapter == Y_VIDEO_ADAPTER_CGA
                                         ? YVideoDefaultCGARegisters
                                         : YVideoDefaultMDARegisters;
  for (uint8_t i = 0; i < Y_VIDEO_NUM_CRTC_REGISTERS; ++i) {
    video->registers[i] = default_registers[i];
  }
  video->control_register = adapter->default_control_register;
  YVideoInvalidateAll(video);

  for (uint32_t i = 0; i < adapter->vram_size; i += 2) {
    YVideoWriteVRAMByte(video, i, ' ');
    YVideoWriteVRAMByte(video, i + 1, 0x07 /* default attr */);
  }
}

// ============================================================================
// Video mode
// ============================================================================

Y_PUBLIC Y_HOT YVideoMode YVideoGetMode(const YVideoState* video) {
  if (video->adapter == Y_VIDEO_ADAPTER_MDA) {
    // The MDA has only one mode.
    return Y_VIDEO_MODE_MDA_TEXT_80X25;
  }
  // Derive the CGA mode from the mode control register, matching the values
  // the BIOS writes for each of its INT 10h modes.
  //
  // The bits are tested in the order a real card resolves them: the high
  // resolution bit wins over the graphics bits, and the graphics bit over the
  // high resolution graphics bit. The BIOS never writes a combination where
  // the order matters, but software that ORs a bit into a saved mode byte can,
  // and this is the order 86Box's renderer decodes them in.
  if (video->control_register & Y_VIDEO_CONTROL_HIGH_RESOLUTION) {
    return video->control_register & Y_VIDEO_CONTROL_BLACK_AND_WHITE
               ? Y_VIDEO_MODE_CGA_TEXT_80X25_MONO
               : Y_VIDEO_MODE_CGA_TEXT_80X25_COLOR;
  }
  if (!(video->control_register & Y_VIDEO_CONTROL_GRAPHICS)) {
    return video->control_register & Y_VIDEO_CONTROL_BLACK_AND_WHITE
               ? Y_VIDEO_MODE_CGA_TEXT_40X25_MONO
               : Y_VIDEO_MODE_CGA_TEXT_40X25_COLOR;
  }
  if (video->control_register & Y_VIDEO_CONTROL_HIGH_RESOLUTION_GRAPHICS) {
    return Y_VIDEO_MODE_CGA_GRAPHICS_640X200;
  }
  return video->control_register & Y_VIDEO_CONTROL_BLACK_AND_WHITE
             ? Y_VIDEO_MODE_CGA_GRAPHICS_320X200_ALT
             : Y_VIDEO_MODE_CGA_GRAPHICS_320X200;
}

Y_PUBLIC const YVideoModeMetadata* YVideoGetModeMetadata(
    const YVideoState* video) {
  return &YVideoModeMetadataTable[YVideoGetMode(video)];
}

Y_PUBLIC YVideoSize YVideoGetFrameSize(const YVideoState* video) {
  const YVideoModeMetadata* metadata = YVideoGetModeMetadata(video);
  YVideoSize size;
  if (video->has_text_font && metadata->type == Y_VIDEO_MODE_TEXT) {
    // A 40-column cell is drawn twice as wide, so both text widths come to
    // the same 80 font cells across.
    size.width =
        (uint16_t)(Y_VIDEO_DIRTY_COLUMNS * video->config.text_font.width);
    size.height = (uint16_t)(metadata->rows * video->config.text_font.height);
    return size;
  }
  const YVideoAdapterMetadata* adapter = YVideoGetAdapterMetadata(video);
  size.width = adapter->frame_buffer_width;
  size.height = adapter->frame_buffer_height;
  return size;
}

// ============================================================================
// 6845 CRT controller
// ============================================================================

Y_MODULE_PRIVATE uint16_t YVideoGetStartAddress(const YVideoState* video) {
  return (uint16_t)(video->registers[Y_VIDEO_CRTC_REGISTER_START_ADDRESS_H]
                    << 8) |
         video->registers[Y_VIDEO_CRTC_REGISTER_START_ADDRESS_L];
}

Y_MODULE_PRIVATE uint16_t YVideoGetCursorAddress(const YVideoState* video) {
  return (uint16_t)(video->registers[Y_VIDEO_CRTC_REGISTER_CURSOR_H] << 8) |
         video->registers[Y_VIDEO_CRTC_REGISTER_CURSOR_L];
}

Y_MODULE_PRIVATE bool YVideoIsCursorEnabled(const YVideoState* video) {
  return (video->registers[Y_VIDEO_CRTC_REGISTER_CURSOR_START] &
          Y_VIDEO_CRTC_CURSOR_MODE_MASK) != Y_VIDEO_CRTC_CURSOR_DISABLED;
}

Y_MODULE_PRIVATE uint8_t
YVideoGetCursorStartScanLine(const YVideoState* video) {
  return video->registers[Y_VIDEO_CRTC_REGISTER_CURSOR_START] &
         Y_VIDEO_CRTC_CURSOR_SCAN_LINE_MASK;
}

Y_MODULE_PRIVATE uint8_t YVideoGetCursorEndScanLine(const YVideoState* video) {
  return video->registers[Y_VIDEO_CRTC_REGISTER_CURSOR_END] &
         Y_VIDEO_CRTC_CURSOR_SCAN_LINE_MASK;
}

Y_MODULE_PRIVATE bool YVideoIsCursorBlinkOn(const YVideoState* video) {
  return (video->frames / Y_VIDEO_FRAMES_PER_CURSOR_BLINK_PHASE) % 2 == 0;
}

Y_MODULE_PRIVATE bool YVideoIsTextBlinkOn(const YVideoState* video) {
  return (video->frames / Y_VIDEO_FRAMES_PER_TEXT_BLINK_PHASE) % 2 == 0;
}

Y_MODULE_PRIVATE bool YVideoGetVisibleCursorOffset(
    const YVideoState* video, const YVideoModeMetadata* metadata,
    uint16_t start_address, uint16_t* cursor_offset) {
  if (!YVideoIsCursorEnabled(video) || !YVideoIsCursorBlinkOn(video)) {
    return false;
  }
  *cursor_offset = (YVideoGetCursorAddress(video) - start_address) &
                   (metadata->vram_size / 2 - 1);
  return *cursor_offset < (uint16_t)metadata->columns * metadata->rows;
}

// ============================================================================
// Dirty regions
// ============================================================================

Y_FILE_PRIVATE void YVideoDirtyRangeAdd(
    YVideoDirtyRange* range, uint8_t start_column, uint8_t end_column) {
  if (range->end_column == 0) {
    range->start_column = start_column;
    range->end_column = end_column;
    return;
  }
  if (start_column < range->start_column) {
    range->start_column = start_column;
  }
  if (end_column > range->end_column) {
    range->end_column = end_column;
  }
}

// The vertical unit of dirty tracking for the current mode.
typedef struct YVideoDirtyGeometry {
  // Number of dirty rows covering the display.
  uint8_t rows;
  // Physical scan lines per dirty row.
  uint8_t scan_lines_per_row;
} YVideoDirtyGeometry;

Y_FILE_PRIVATE YVideoDirtyGeometry
YVideoGetDirtyGeometry(const YVideoState* video) {
  const YVideoModeMetadata* metadata = YVideoGetModeMetadata(video);
  YVideoDirtyGeometry geometry;
  if (metadata->type == Y_VIDEO_MODE_TEXT) {
    geometry.rows = metadata->rows;
    geometry.scan_lines_per_row = YVideoGetTextCellHeight(video, metadata);
  } else {
    geometry.rows =
        (uint8_t)((metadata->height + Y_VIDEO_DIRTY_SCAN_LINES_PER_GROUP - 1) /
                  Y_VIDEO_DIRTY_SCAN_LINES_PER_GROUP);
    geometry.scan_lines_per_row = Y_VIDEO_DIRTY_SCAN_LINES_PER_GROUP;
  }
  if (geometry.rows > Y_VIDEO_DIRTY_ROW_COUNT) {
    geometry.rows = Y_VIDEO_DIRTY_ROW_COUNT;
  }
  return geometry;
}

Y_MODULE_PRIVATE void YVideoInvalidateRows(
    YVideoState* video, uint8_t start_column, uint8_t end_column,
    uint8_t first_row, uint8_t end_row) {
  if (start_column >= end_column || end_column > Y_VIDEO_DIRTY_COLUMNS ||
      first_row >= end_row || first_row >= Y_VIDEO_DIRTY_ROW_COUNT) {
    return;
  }
  if (end_row > Y_VIDEO_DIRTY_ROW_COUNT) {
    end_row = Y_VIDEO_DIRTY_ROW_COUNT;
  }

  for (uint8_t row = first_row; row < end_row; ++row) {
    YVideoDirtyRangeAdd(
        &video->dirty_state.ranges[row], start_column, end_column);
  }
  // A pending full redraw already covers these rows, and downgrading it would
  // drop the rest of the frame.
  if (video->dirty_state.status == Y_VIDEO_CLEAN) {
    video->dirty_state.status = Y_VIDEO_DIRTY;
  }
}

Y_FILE_PRIVATE void YVideoInvalidateAll(YVideoState* video) {
  video->dirty_state.status = Y_VIDEO_FULL_REDRAW;
}

Y_FILE_PRIVATE void YVideoInvalidateTextCell(
    YVideoState* video, const YVideoModeMetadata* metadata,
    uint16_t cell_offset) {
  if (cell_offset >= (uint16_t)metadata->columns * metadata->rows) {
    return;
  }
  uint8_t row;
  uint8_t col;
  uint8_t column_scale;
  if (metadata->columns == Y_VIDEO_TEXT_COLUMNS40) {
    row = (uint8_t)(cell_offset / Y_VIDEO_TEXT_COLUMNS40);
    col = (uint8_t)(cell_offset % Y_VIDEO_TEXT_COLUMNS40);
    column_scale = Y_VIDEO_TEXT40_COLUMN_SCALE;
  } else {
    row = (uint8_t)(cell_offset / Y_VIDEO_TEXT_COLUMNS80);
    col = (uint8_t)(cell_offset % Y_VIDEO_TEXT_COLUMNS80);
    column_scale = Y_VIDEO_TEXT80_COLUMN_SCALE;
  }
  // The character row is the dirty row, so a changed cell marks exactly the
  // scan lines it covers - no conversion to scan lines and back.
  YVideoInvalidateRows(
      video, col * column_scale, (col + 1) * column_scale, row,
      (uint8_t)(row + 1));
}

Y_FILE_PRIVATE void YVideoInvalidateVRAMAddress(
    YVideoState* video, uint32_t address) {
  const YVideoModeMetadata* metadata = YVideoGetModeMetadata(video);
  if (metadata->type == Y_VIDEO_MODE_TEXT) {
    uint16_t character_mask = (uint16_t)(metadata->vram_size / 2 - 1);
    uint16_t character_address = (uint16_t)(address / 2);
    uint16_t cell_offset =
        (character_address - YVideoGetStartAddress(video)) & character_mask;
    YVideoInvalidateTextCell(video, metadata, cell_offset);
    return;
  }

  // Each half of CGA graphics VRAM holds either the even or odd scan lines.
  uint32_t half_mask = Y_VIDEO_CGA_GRAPHICS_ODD_SCAN_LINE_OFFSET - 1;
  uint32_t start = (uint32_t)YVideoGetStartAddress(video) * 2 & half_mask;
  uint32_t offset = ((address & half_mask) - start) & half_mask;
  uint32_t visible_bytes =
      (metadata->height / 2) * Y_VIDEO_CGA_GRAPHICS_BYTES_PER_SCAN_LINE;
  if (offset >= visible_bytes) {
    return;
  }
  uint16_t row_in_half =
      (uint16_t)(offset / Y_VIDEO_CGA_GRAPHICS_BYTES_PER_SCAN_LINE);
  uint8_t byte_column = offset % Y_VIDEO_CGA_GRAPHICS_BYTES_PER_SCAN_LINE;
  uint16_t y = row_in_half * 2 +
               (address >= Y_VIDEO_CGA_GRAPHICS_ODD_SCAN_LINE_OFFSET ? 1 : 0);
  // Graphics modes have no character rows, so neighboring scan lines share a
  // dirty row.
  uint8_t dirty_row = (uint8_t)(y / Y_VIDEO_DIRTY_SCAN_LINES_PER_GROUP);
  YVideoInvalidateRows(
      video, byte_column, byte_column + 1, dirty_row, (uint8_t)(dirty_row + 1));
}

Y_FILE_PRIVATE void YVideoInvalidateCursor(YVideoState* video) {
  const YVideoModeMetadata* metadata = YVideoGetModeMetadata(video);
  if (metadata->type != Y_VIDEO_MODE_TEXT) {
    return;
  }
  uint16_t character_mask = (uint16_t)(metadata->vram_size / 2 - 1);
  uint16_t cursor_offset =
      (YVideoGetCursorAddress(video) - YVideoGetStartAddress(video)) &
      character_mask;
  YVideoInvalidateTextCell(video, metadata, cursor_offset);
}

Y_FILE_PRIVATE void YVideoInvalidateBlinkingText(YVideoState* video) {
  const YVideoModeMetadata* metadata = YVideoGetModeMetadata(video);
  if (metadata->type != Y_VIDEO_MODE_TEXT ||
      !(video->control_register & Y_VIDEO_CONTROL_ENABLE_BLINK)) {
    return;
  }

  uint16_t start_address = YVideoGetStartAddress(video);
  uint8_t column_scale = metadata->columns == Y_VIDEO_TEXT_COLUMNS40
                             ? Y_VIDEO_TEXT40_COLUMN_SCALE
                             : Y_VIDEO_TEXT80_COLUMN_SCALE;
  for (uint8_t row = 0; row < metadata->rows; ++row) {
    uint8_t start_column = Y_VIDEO_DIRTY_COLUMNS;
    uint8_t end_column = 0;
    for (uint8_t col = 0; col < metadata->columns; ++col) {
      uint16_t cell_offset = (uint16_t)row * metadata->columns + col;
      uint32_t attr_address = ((uint32_t)start_address + cell_offset) * 2 + 1;
      if (YVideoReadVRAMByte(video, attr_address) & Y_VIDEO_ATTRIBUTE_BLINK) {
        uint8_t dirty_column = col * column_scale;
        if (dirty_column < start_column) {
          start_column = dirty_column;
        }
        end_column = (col + 1) * column_scale;
      }
    }
    if (end_column != 0) {
      YVideoInvalidateRows(
          video, start_column, end_column, row, (uint8_t)(row + 1));
    }
  }
}

// ============================================================================
// Retrace timing
// ============================================================================

// Advances a software model of the CRT beam position by the given number of
// CPU cycles. This is what status port reads (below) derive retrace timing
// from.
//
// The beam advances one scan line every kMDACyclesPerScanLine cycles, but
// callers report cycles in whatever amount the CPU just consumed, which
// rarely divides evenly. scan_line_cycles banks the remainder as credit
// toward the next scan line; the loop pays it off a scan line at a time,
// looping rather than branching once since a single call can be worth more
// than one scan line. scan_line wraps at the adapter's scan_lines_per_frame,
// incrementing frames, which YVideoIsCursorBlinkOn() and YVideoIsTextBlinkOn()
// use to derive their blink phases.
Y_PUBLIC Y_HOT void YVideoTick(YVideoState* video, uint32_t cycles) {
  const YVideoAdapterMetadata* adapter = YVideoGetAdapterMetadata(video);
  if (adapter->cycles_per_scan_line == 0 ||
      adapter->scan_lines_per_frame == 0) {
    // No adapter geometry to advance through.
    return;
  }

  bool cursor_was_on = YVideoIsCursorBlinkOn(video);
  bool text_was_on = YVideoIsTextBlinkOn(video);

  // Computed rather than stepped, because the platform advances the beam in
  // arrears - only when something actually looks at it - so cycles here can be
  // a whole frame's worth rather than a single instruction's.
  const uint32_t total = video->scan_line_cycles + cycles;
  video->scan_line_cycles = total % adapter->cycles_per_scan_line;

  const uint32_t elapsed_scan_lines = total / adapter->cycles_per_scan_line;
  if (elapsed_scan_lines == 0) {
    return;
  }
  const uint32_t scan_line = (uint32_t)video->scan_line + elapsed_scan_lines;
  video->frames += scan_line / adapter->scan_lines_per_frame;
  video->scan_line = (uint16_t)(scan_line % adapter->scan_lines_per_frame);

  if (cursor_was_on != YVideoIsCursorBlinkOn(video) &&
      YVideoIsCursorEnabled(video)) {
    YVideoInvalidateCursor(video);
  }
  if (text_was_on != YVideoIsTextBlinkOn(video)) {
    YVideoInvalidateBlinkingText(video);
  }
}

// The value the status port reads back, computed from where the CRT beam
// currently is.
Y_FILE_PRIVATE uint8_t YVideoGetStatus(const YVideoState* video) {
  const YVideoAdapterMetadata* adapter = YVideoGetAdapterMetadata(video);
  bool in_vertical_retrace = video->scan_line >= adapter->displayed_scan_lines;
  bool in_horizontal_retrace =
      video->scan_line_cycles >= adapter->display_cycles_per_scan_line;

  // No light pen is emulated, so its switch always reads as off.
  uint8_t status = Y_VIDEO_STATUS_LIGHT_PEN_SWITCH_OFF;
  if (in_vertical_retrace || in_horizontal_retrace) {
    status |= Y_VIDEO_STATUS_DISPLAY_DISABLED;
  }
  if (in_vertical_retrace) {
    status |= Y_VIDEO_STATUS_VERTICAL_RETRACE;
  }
  // Note that the unused high bits are deliberately left clear rather than
  // floating high as they do on a real card: GLaBIOS probes bit 7 of the MDA
  // status port to detect a Hercules adapter.
  return status;
}

// ============================================================================
// I/O ports
// ============================================================================

// What an I/O port in the adapter's range does.
typedef enum YVideoPortFunction {
  // Port is not decoded by the adapter.
  Y_VIDEO_PORT_NONE = 0,
  // 6845 register index.
  Y_VIDEO_PORT_REGISTER_INDEX,
  // 6845 register data.
  Y_VIDEO_PORT_REGISTER_DATA,
  // Mode control register.
  Y_VIDEO_PORT_CONTROL,
  // CGA color select register.
  Y_VIDEO_PORT_COLOR_SELECT,
  // Status register.
  Y_VIDEO_PORT_STATUS,
} YVideoPortFunction;

// Decode an I/O port within the adapter's range.
Y_FILE_PRIVATE YVideoPortFunction
YVideoDecodePort(const YVideoState* video, uint16_t port) {
  if (video->adapter == Y_VIDEO_ADAPTER_MDA) {
    switch (port) {
      case Y_VIDEO_MDA_PORT_REGISTER_INDEX:
        return Y_VIDEO_PORT_REGISTER_INDEX;
      case Y_VIDEO_MDA_PORT_REGISTER_DATA:
        return Y_VIDEO_PORT_REGISTER_DATA;
      case Y_VIDEO_MDA_PORT_CONTROL:
        return Y_VIDEO_PORT_CONTROL;
      case Y_VIDEO_MDA_PORT_STATUS:
        return Y_VIDEO_PORT_STATUS;
      default:
        return Y_VIDEO_PORT_NONE;
    }
  }

  // The CGA only decodes the low four address bits, so the 6845 index and data
  // registers are aliased across 3D0 to 3D7.
  uint16_t offset = port - Y_VIDEO_CGA_PORT_START;
  if (offset < 8) {
    return offset & 1 ? Y_VIDEO_PORT_REGISTER_DATA
                      : Y_VIDEO_PORT_REGISTER_INDEX;
  }
  switch (port) {
    case Y_VIDEO_CGA_PORT_CONTROL:
      return Y_VIDEO_PORT_CONTROL;
    case Y_VIDEO_CGA_PORT_COLOR_SELECT:
      return Y_VIDEO_PORT_COLOR_SELECT;
    case Y_VIDEO_CGA_PORT_STATUS:
      return Y_VIDEO_PORT_STATUS;
    default:
      // The light pen strobe ports respond but do nothing, as no light pen is
      // emulated.
      return Y_VIDEO_PORT_NONE;
  }
}

Y_PUBLIC uint8_t YVideoReadPort(YVideoState* video, uint16_t port) {
  switch (YVideoDecodePort(video, port)) {
    case Y_VIDEO_PORT_REGISTER_INDEX:
      return video->selected_register;
    case Y_VIDEO_PORT_REGISTER_DATA:
      if (video->selected_register < Y_VIDEO_NUM_CRTC_REGISTERS) {
        return video->registers[video->selected_register];
      }
      return Y_VIDEO_UNMAPPED_PORT_VALUE;
    case Y_VIDEO_PORT_CONTROL:
      return video->control_register;
    case Y_VIDEO_PORT_COLOR_SELECT:
      return video->color_select_register;
    case Y_VIDEO_PORT_STATUS:
      return YVideoGetStatus(video);
    default:
      return Y_VIDEO_UNMAPPED_PORT_VALUE;
  }
}

Y_PUBLIC void YVideoWritePort(
    YVideoState* video, uint16_t port, uint8_t value) {
  switch (YVideoDecodePort(video, port)) {
    case Y_VIDEO_PORT_REGISTER_INDEX:
      video->selected_register = value & Y_VIDEO_CRTC_REGISTER_INDEX_MASK;
      break;
    case Y_VIDEO_PORT_REGISTER_DATA:
      if (video->selected_register < Y_VIDEO_NUM_CRTC_REGISTERS) {
        uint8_t* selected = &video->registers[video->selected_register];
        if (*selected == value) {
          break;
        }
        if (video->selected_register == Y_VIDEO_CRTC_REGISTER_CURSOR_H ||
            video->selected_register == Y_VIDEO_CRTC_REGISTER_CURSOR_L) {
          YVideoInvalidateCursor(video);
          *selected = value;
          YVideoInvalidateCursor(video);
        } else {
          *selected = value;
          if (video->selected_register ==
                  Y_VIDEO_CRTC_REGISTER_START_ADDRESS_H ||
              video->selected_register ==
                  Y_VIDEO_CRTC_REGISTER_START_ADDRESS_L) {
            YVideoInvalidateAll(video);
          } else if (
              video->selected_register == Y_VIDEO_CRTC_REGISTER_CURSOR_START ||
              video->selected_register == Y_VIDEO_CRTC_REGISTER_CURSOR_END) {
            YVideoInvalidateCursor(video);
          }
        }
      }
      break;
    case Y_VIDEO_PORT_CONTROL:
      if (video->control_register != value) {
        video->control_register = value;
        YVideoInvalidateAll(video);
      }
      break;
    case Y_VIDEO_PORT_COLOR_SELECT:
      if (video->color_select_register != value) {
        video->color_select_register = value;
        YVideoInvalidateAll(video);
      }
      break;
    default:
      // The status port is read only.
      break;
  }
}

// ============================================================================
// Rendering
// ============================================================================

Y_FILE_PRIVATE void YVideoRenderBlankRegion(
    YVideoState* video, YVideoPixelRun* run, YVideoRegion region) {
  YVideoRGB blank = video->adapter == Y_VIDEO_ADAPTER_CGA
                        ? video->config.cga_palette[0]
                        : video->config.background;
  uint16_t end_x = region.origin.x + region.width;
  uint16_t end_y = region.origin.y + region.height;
  for (uint16_t y = region.origin.y; y < end_y; ++y) {
    YVideoPixelRunBegin(run, region.origin.x, y);
    for (uint16_t x = region.origin.x; x < end_x; ++x) {
      YVideoPixelRunPush(run, blank);
    }
  }
}

Y_FILE_PRIVATE void YVideoRenderDirtyRegion(
    YVideoState* video, uint8_t start_column, uint8_t end_column,
    uint16_t first_y, uint16_t end_y) {
  // Every mode divides the frame into Y_VIDEO_DIRTY_COLUMNS equal columns, so
  // a dirty column is 9 pixels wide on the MDA, 8 on the CGA, and a host text
  // font's width in a text mode drawn in one.
  uint16_t column_width =
      YVideoGetFrameSize(video).width / Y_VIDEO_DIRTY_COLUMNS;
  YVideoRegion region = {
      .origin = {.x = start_column * column_width, .y = first_y},
      .width = (end_column - start_column) * column_width,
      .height = end_y - first_y,
  };
  if (video->config.begin_render_region) {
    video->config.begin_render_region(video, region);
  }

  video->num_pixels_emitted_for_region = 0;
  // Only these two members need a value on the way in: origin is set whenever
  // a span starts, and a pixel is stored before the count that exposes it to
  // the flush. Initializing the whole struct would clear the 96-byte batch on
  // every region, which a graphics frame can split into fifty of them.
  YVideoPixelRun run;
  run.video = video;
  run.count = 0;

  if (!(video->control_register & Y_VIDEO_CONTROL_VIDEO_ENABLE)) {
    YVideoRenderBlankRegion(video, &run, region);
  } else if (video->adapter == Y_VIDEO_ADAPTER_CGA) {
    YVideoCGARenderRegion(
        video, &run, start_column, end_column, first_y, end_y);
  } else {
    YVideoMDARenderRegion(
        video, &run, start_column, end_column, first_y, end_y);
  }
  YVideoPixelRunFlush(&run);

  // A retained display addressed by transfer window advances its own write
  // pointer once per pixel, so it is the pixel count that positions them, not
  // the coordinates passed alongside. Emitting a different number than the
  // region declared leaves that pointer mid-window and displaces every pixel
  // after it, including those of later regions - so report the region that
  // caused it rather than let the corruption surface somewhere else.
  uint32_t declared_pixels = (uint32_t)region.width * region.height;
  if (video->num_pixels_emitted_for_region != declared_pixels) {
    Y_VIDEO_ERROR(
        "render_region_pixel_count_mismatch", Y_LOG_UINT("width", region.width),
        Y_LOG_UINT("height", region.height), Y_LOG_UINT("x", region.origin.x),
        Y_LOG_UINT("y", region.origin.y),
        Y_LOG_UINT("emitted", video->num_pixels_emitted_for_region),
        Y_LOG_UINT("expected", declared_pixels));
  }

  if (video->config.end_render_region) {
    video->config.end_render_region(video);
  }
}

Y_PUBLIC void YVideoRender(YVideoState* video) {
  if (!video->config.write_pixels ||
      video->dirty_state.status == Y_VIDEO_CLEAN) {
    return;
  }

  const YVideoSize frame = YVideoGetFrameSize(video);
  if (video->dirty_state.status == Y_VIDEO_FULL_REDRAW) {
    YVideoRenderDirtyRegion(video, 0, Y_VIDEO_DIRTY_COLUMNS, 0, frame.height);
  } else {
    YVideoDirtyGeometry geometry = YVideoGetDirtyGeometry(video);
    for (uint8_t row = 0; row < geometry.rows;) {
      YVideoDirtyRange range = video->dirty_state.ranges[row];
      if (range.end_column == 0) {
        ++row;
        continue;
      }

      uint8_t end_row = row + 1;
      while (end_row < geometry.rows &&
             video->dirty_state.ranges[end_row].start_column ==
                 range.start_column &&
             video->dirty_state.ranges[end_row].end_column ==
                 range.end_column) {
        ++end_row;
      }
      uint16_t first_y = (uint16_t)row * geometry.scan_lines_per_row;
      uint16_t end_y = (uint16_t)end_row * geometry.scan_lines_per_row;
      if (end_y > frame.height) {
        end_y = frame.height;
      }
      YVideoRenderDirtyRegion(
          video, range.start_column, range.end_column, first_y, end_y);
      row = end_row;
    }
  }

  static const YVideoDirtyState kEmptyDirtyState = {0};
  video->dirty_state = kEmptyDirtyState;
}

Y_PUBLIC bool YVideoCopyForRender(
    YVideoState* video, YVideoState* frame, uint8_t* frame_vram) {
  if (video->dirty_state.status == Y_VIDEO_CLEAN) {
    return false;
  }
  *frame = *video;
  if (video->config.vram) {
    const uint8_t* source = video->config.vram;
    const uint8_t* const end =
        source + YVideoGetAdapterMetadata(video)->vram_size;
    uint8_t* destination = frame_vram;
    while (source < end) {
      *destination++ = *source++;
    }
    frame->config.vram = frame_vram;
  }
  static const YVideoDirtyState kEmptyDirtyState = {0};
  video->dirty_state = kEmptyDirtyState;
  return true;
}


// ==============================================================================
// src/video/video.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_VIDEO_BUNDLE_H

