// ==============================================================================
// YAX86 PPI MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_PPI_BUNDLE_H
#define YAX86_PPI_BUNDLE_H

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
// Written by YLogfmtLine, and read back by YLogfmtParse().
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

// ============================================================================
// Reading
// ============================================================================

// A pair read from a line: pointers into the line, which the parse has cut up
// and unescaped in place.
typedef struct YLogfmtPair {
  const char* key;
  const char* value;
} YLogfmtPair;

// Read a hex digit, returning whether it was one.
Y_MODULE_PRIVATE_HEADER inline bool YLogfmtHexDigit(char c, uint8_t* digit) {
  if (c >= '0' && c <= '9') {
    *digit = (uint8_t)(c - '0');
  } else if (c >= 'A' && c <= 'F') {
    *digit = (uint8_t)(c - 'A' + 10);
  } else if (c >= 'a' && c <= 'f') {
    *digit = (uint8_t)(c - 'a' + 10);
  } else {
    return false;
  }
  return true;
}

// Read the value starting at *cursor into the line at *write, unescaping a
// quoted one, and leave *cursor past it. Returns whether it was well formed.
// The write never overtakes the read, which is what lets this work in place.
Y_MODULE_PRIVATE_HEADER inline bool YLogfmtParseValue(
    char** cursor, char** write) {
  char* read = *cursor;
  char* out = *write;
  if (*read != '"') {
    while (*read != '\0' && *read != ' ') {
      *out++ = *read++;
    }
    *cursor = read;
    *write = out;
    return true;
  }
  ++read;
  for (;;) {
    const char c = *read;
    if (c == '\0') {
      return false;
    }
    if (c == '"') {
      ++read;
      break;
    }
    if (c != '\\') {
      *out++ = c;
      ++read;
      continue;
    }
    const char escaped = read[1];
    if (escaped == '"' || escaped == '\\') {
      *out++ = escaped;
      read += 2;
    } else if (escaped == 'x') {
      uint8_t high = 0;
      uint8_t low = 0;
      if (!YLogfmtHexDigit(read[2], &high) || !YLogfmtHexDigit(read[3], &low)) {
        return false;
      }
      *out++ = (char)((high << 4) | low);
      read += 4;
    } else {
      return false;
    }
  }
  // A quoted value ends at a space or at the end of the line.
  if (*read != '\0' && *read != ' ') {
    return false;
  }
  *cursor = read;
  *write = out;
  return true;
}

// Read a line into pairs, cutting it up and unescaping it in place, so that
// the pairs point into the line. Returns whether the line is well formed: a
// token that is not a key=value pair, an empty key, a quoted value left open
// or with an escape this does not write, and more pairs than max_pairs all
// make it not. An empty line is well formed and has no pairs.
Y_PUBLIC_HEADER inline bool YLogfmtParse(
    char* line, YLogfmtPair* pairs, uint8_t max_pairs, uint8_t* num_pairs) {
  *num_pairs = 0;
  char* read = line;
  for (;;) {
    while (*read == ' ') {
      ++read;
    }
    if (*read == '\0') {
      return true;
    }
    if (*num_pairs >= max_pairs) {
      return false;
    }
    char* const key = read;
    while (*read != '\0' && *read != '=' && *read != ' ' && *read != '"') {
      ++read;
    }
    if (*read != '=' || read == key) {
      return false;
    }
    *read++ = '\0';
    // The value is written over itself, from where it starts.
    char* value = read;
    char* write = read;
    if (!YLogfmtParseValue(&read, &write)) {
      return false;
    }
    // What ends the value - a space, or the end of the line - is either past
    // the write or is the write, so terminating there loses nothing unread.
    const bool at_end = *read == '\0';
    *write = '\0';
    if (!at_end) {
      ++read;
    }
    pairs[*num_pairs].key = key;
    pairs[*num_pairs].value = value;
    ++*num_pairs;
    if (at_end) {
      return true;
    }
  }
}

// The value of the first pair with the given key, or NULL if there is none.
Y_PUBLIC_HEADER inline const char* YLogfmtFind(
    const YLogfmtPair* pairs, uint8_t num_pairs, const char* key) {
  for (uint8_t i = 0; i < num_pairs; ++i) {
    const char* a = pairs[i].key;
    const char* b = key;
    while (*a != '\0' && *a == *b) {
      ++a;
      ++b;
    }
    if (*a == *b) {
      return pairs[i].value;
    }
  }
  return NULL;
}

// Read a number, decimal or hex with a 0x prefix, as the writer writes them.
// Returns whether the whole value was one that fits.
Y_PUBLIC_HEADER inline bool YLogfmtParseUInt(
    const char* value, uint64_t* number) {
  uint64_t result = 0;
  const bool hex = value[0] == '0' && (value[1] == 'x' || value[1] == 'X');
  const char* digits = hex ? value + 2 : value;
  if (*digits == '\0') {
    return false;
  }
  for (const char* c = digits; *c != '\0'; ++c) {
    uint8_t digit = 0;
    if (hex) {
      if (!YLogfmtHexDigit(*c, &digit) || result > (UINT64_MAX >> 4)) {
        return false;
      }
      result = (result << 4) | digit;
    } else {
      if (*c < '0' || *c > '9') {
        return false;
      }
      digit = (uint8_t)(*c - '0');
      if (result > (UINT64_MAX - digit) / 10) {
        return false;
      }
      result = result * 10 + digit;
    }
  }
  *number = result;
  return true;
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
// src/ppi/public.h start
// ==============================================================================

#line 1 "./src/ppi/public.h"
// Public interface for the PPI (Programmable Peripheral Interface) module.
#ifndef YAX86_PPI_PUBLIC_H
#define YAX86_PPI_PUBLIC_H

// This module emulates the Intel 8255 PPI chip as used in the IBM PC and
// PC/XT. Note that we don't implement all features of the 8255, only those
// needed to support GLaBIOS in ARCH_TYPE_EMU mode. Specifically:
//
// - Simplified PC speaker control
//     - Only on/off and frequency from PIT channel 2
//     - No real-time mirroring of PIT channel 2 on port C pin 5
// - No memory or I/O parity checking
// - No cassette support
//
// Reference tables from GLaBIOS source code:
//
// ----------------------------------------------------------------------------
//  5160/Standard: 8255 PPI Channel B (Port 61h) Flags
// ----------------------------------------------------------------------------
//  84218421
//  7 	    |	PBKB	0=enable keyboard read, 1=clear
//   6      |	PBKC	0=hold keyboard clock low, 1=enable clock
//    5     |	PBIO	0=enable i/o check, 1=disable
//     4    |	PBPC	0=enable memory parity check, 1=disable
//      3   |	PBSW	0=read SW1-4, 1=read SW-5-8
//       2  |	PBTB	0=turbo, 1=normal
//        1 |	PBSP	0=turn off speaker, 1=turn on
//         0|	PBST	0=turn off timer 2, 1=turn on
// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------
//  5160: 8255 PPI Channel C (Port 62h) Flags When PPI B PBSW = 0
// ----------------------------------------------------------------------------
//  84218421
//  7 	    |	PCPE	0=no parity error, 1=memory parity error
//   6      |	PCIE	0=no i/o channel error, 1=i/o channel error
//    5     |	PCT2	timer 2 output / cassette data output
//     4    |	PCCI	cassette data input
//      32  |	PCMB	SW 3,4: MB RAM (00=64K, 01=128K, 10=192K, 11=256K)
//        1 |	PCFP	SW 2: 0=no FPU, 1=FPU installed
//         0|	PCFD	SW 1: Floppy drive (IPL) installed
// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------
//  8255 PPI Channel C (Port 62h) Flags When PPI B PBSW = 1
// ----------------------------------------------------------------------------
//  84218421
//  7 	    |	PC2PE	0=no parity error, 1 r/w memory parity check error
//   6      |	PC2IE	0=no i/o channel error, 1 i/o channel check error
//    5     |	PC2T2	timer 2 output
//     4    |	PC2CI	cassette data input
//      32  |	PCDRV	SW 7,8: # of drives (00=1, 01=2, 10=3, 11=4)
//        10|	PCVID	SW 5,6: video Mode (00=ROM, 01=CG40, 10=CG80, 11=MDA)
// ----------------------------------------------------------------------------

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_PPI_BUNDLE_H
#include "../util/log.h"
#endif  // YAX86_PPI_BUNDLE_H

enum {
  // Log module ID for the PPI.
  Y_PPI_LOG_MODULE_ID = 4,
};

// Log module for the PPI.
Y_PUBLIC_HEADER const YLogModule YPPILogModule = {
    .id = Y_PPI_LOG_MODULE_ID,
    .name = "ppi",
};

// I/O ports exposed by the PPI.
typedef enum YPPIPort {
  // Keyboard scancode
  Y_PPI_PORT_A = 0x60,
  // System control
  Y_PPI_PORT_B = 0x61,
  // DIP switches
  Y_PPI_PORT_C = 0x62,
  // Control word
  Y_PPI_PORT_CONTROL = 0x63,
} YPPIPort;

// Bit definitions for PPI Port B (Y_PPI_PORT_B).
enum {
  // Bit 0: Timer 2 signal gate (0 = disable, 1 = enable).
  Y_PPI_PORT_B_TIMER2_GATE = (1 << 0),
  // Bit 1: PC speaker enable/disable.
  Y_PPI_PORT_B_SPEAKER_DATA = (1 << 1),
  // Bit 2: Turbo mode (0 = turbo, 1 = normal). Not supported.
  Y_PPI_PORT_B_TURBO_MODE = (1 << 2),
  // Bit 3: DIP switch select (0 = SW1-4, 1 = SW5-8).
  Y_PPI_PORT_B_DIP_SWITCH_SELECT = (1 << 3),
  // Bit 4: Memory parity check enable/disable. Not supported.
  Y_PPI_PORT_B_MEMORY_PARITY_CHECK = (1 << 4),
  // Bit 5: I/O channel check enable/disable. Not supported.
  Y_PPI_PORT_B_IO_CHANNEL_CHECK = (1 << 5),
  // Bit 6: Keyboard clock control (0 = hold low, 1 = enable).
  Y_PPI_PORT_B_KEYBOARD_CLOCK_LOW = (1 << 6),
  // Bit 7: Keyboard enable/clear (0 = enable read, 1 = clear).
  Y_PPI_PORT_B_KEYBOARD_ENABLE_CLEAR = (1 << 7),
};

// Memory sizes defined by DIP switches, corresponding to Port A bits 2-3.
// Note that GLaBIOS in ARCH_TYPE_EMU mode does not actually make use of these
// and instead performs its own memory detection based on the video card type.
typedef enum YPPIMemorySize {
  // 00 = 64KB
  Y_PPI_MEMORY_SIZE_64KB = 0,
  // 01 = 128KB
  Y_PPI_MEMORY_SIZE_128KB = 1,
  // 10 = 192KB
  Y_PPI_MEMORY_SIZE_192KB = 2,
  // 11 = 256KB
  Y_PPI_MEMORY_SIZE_256KB = 3,
} YPPIMemorySize;

// Display mode at boot time, corresponding to Port A bits 4-5.
typedef enum YPPIDisplayMode {
  // 00 = EGA/VGA (ROM)
  Y_PPI_DISPLAY_EGA = 0,
  // 01 = CGA 40x25
  Y_PPI_DISPLAY_CGA_40X25 = 1,
  // 10 = CGA 80x25
  Y_PPI_DISPLAY_CGA_80X25 = 2,
  // 11 = MDA 80x25
  Y_PPI_DISPLAY_MDA = 3,
} YPPIDisplayMode;

struct YPPIState;

// Caller-provided runtime configuration for the PPI.
typedef struct YPPIConfig {
  // Opaque context pointer, passed to all callbacks.
  void* context;

  // Logger for this module. May be NULL.
  YLogger* logger;

  // Number of floppy drives (1-4).
  uint8_t num_floppy_drives;

  // Memory size setting from DIP switches.
  YPPIMemorySize memory_size;

  // Display mode setting from DIP switches.
  YPPIDisplayMode display_mode;

  // Whether FPU is installed.
  bool fpu_installed;

  // Callback to control PC speaker.If frequency_hz is 0, the speaker should
  // be turned off. Otherwise, it should be set to the specified frequency.
  void (*set_pc_speaker_frequency)(void* context, uint32_t frequency_hz);

  // Callback when keyboard control bits are modified (bits 6 and 7 of Port B).
  void (*set_keyboard_control)(
      void* context,
      // Port B bit 7
      bool keyboard_enable_clear,
      // Port B bit 6
      bool keyboard_clock_low);
} YPPIConfig;

// State of the PPI.
typedef struct YPPIState {
  // Pointer to the PPI configuration.
  YPPIConfig config;

  // Port A: Keyboard scancode latch.
  uint8_t port_a_latch;

  // Port B: System control register.
  uint8_t port_b;

  // Current frequency of the PC speaker generated by the PIT, in Hz.
  uint32_t pc_speaker_frequency_from_pit;
} YPPIState;

// Initializes the PPI to its power-on state.
Y_PUBLIC void YPPIInit(YPPIState* ppi);

// Handles reads from the PPI's I/O ports (0x60-0x62).
Y_PUBLIC uint8_t YPPIReadPort(YPPIState* ppi, uint16_t port);

// Handles writes to the PPI's I/O ports (0x61, 0x63).
Y_PUBLIC void YPPIWritePort(YPPIState* ppi, uint16_t port, uint8_t value);

// Returns whether the PC speaker is currently enabled. This is determined by
// bit 0 and 1 of Port B.
Y_PUBLIC bool YPPIIsPCSpeakerEnabled(YPPIState* ppi);

// Sets the PC speaker frequency from the 8253 timer channel 2 output. This
// should be wired up to the callback from the PIT emulation module.
Y_PUBLIC void YPPISetPCSpeakerFrequencyFromPIT(
    YPPIState* ppi, uint32_t frequency_hz);

// Sets the scancode byte that will be returned when the CPU reads from Port A.
// This function should be called by the keyboard emulation module.
Y_PUBLIC void YPPISetScancode(YPPIState* ppi, uint8_t scancode);

#endif  // YAX86_PPI_PUBLIC_H


// ==============================================================================
// src/ppi/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/ppi/ppi.c start
// ==============================================================================

#line 1 "./src/ppi/ppi.c"
#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

Y_PUBLIC void YPPIInit(YPPIState* ppi) {
  // Initially, keyboard clock is enabled (bit 6 = 1) and keyboard read is
  // enabled (bit 7 = 0).
  ppi->port_b = Y_PPI_PORT_B_KEYBOARD_CLOCK_LOW;
}

// Gets the number of floppy drives from the config, clamped to 1-4.
Y_FILE_PRIVATE inline uint8_t YPPIGetNumFloppyDrives(const YPPIConfig* config) {
  if (config->num_floppy_drives < 1) return 1;
  if (config->num_floppy_drives > 4) return 4;
  return config->num_floppy_drives;
}

Y_PUBLIC uint8_t YPPIReadPort(YPPIState* ppi, uint16_t port) {
  switch (port) {
    case Y_PPI_PORT_A:
      // Reading Port A gets the keyboard scancode.
      return ppi->port_a_latch;
    case Y_PPI_PORT_B:
      // Reading Port B returns its last written value.
      return ppi->port_b;
    case Y_PPI_PORT_C:
      if ((ppi->port_b & Y_PPI_PORT_B_DIP_SWITCH_SELECT) == 0) {
        // Read from SW1-4.
        uint8_t port_c = 0;
        // Bit 0: Floppy drive (IPL) installed
        port_c |= (ppi->config.num_floppy_drives > 0) & 0x01;
        // Bit 1: FPU installed
        port_c |= (ppi->config.fpu_installed << 1);
        // Bits 2-3: Memory size
        port_c |= ((ppi->config.memory_size & 0x03) << 2);
        // Bits 4-7 are for unsupported features (cassette, parity, etc.).
        return port_c;
      } else {
        // Read from SW5-8.
        uint8_t port_c = 0;
        // Bits 0-1: Video mode.
        port_c |= ppi->config.display_mode & 0x03;
        // Bits 2-3: Number of drives. Slightly confusingly, the encoding is
        // 1-based, i.e. 00=1 drive, 01=2 drives, etc.
        port_c |= (((YPPIGetNumFloppyDrives(&ppi->config) - 1) & 0x03) << 2);
        // Bits 4-7 are for unsupported features.
        return port_c;
      }
    default:
      // Invalid port.
      return 0xFF;
  }
}

Y_PUBLIC bool YPPIIsPCSpeakerEnabled(YPPIState* ppi) {
  return (ppi->port_b & Y_PPI_PORT_B_TIMER2_GATE) &&
         (ppi->port_b & Y_PPI_PORT_B_SPEAKER_DATA);
}

Y_FILE_PRIVATE inline uint8_t YPPIGetKeyboardControl(const YPPIState* ppi) {
  return (
      ppi->port_b &
      (Y_PPI_PORT_B_KEYBOARD_ENABLE_CLEAR | Y_PPI_PORT_B_KEYBOARD_CLOCK_LOW));
}

Y_PUBLIC void YPPIWritePort(YPPIState* ppi, uint16_t port, uint8_t value) {
  switch (port) {
    case Y_PPI_PORT_B: {
      // Save old states in order to check for changes after the write.
      bool old_speaker_enabled = YPPIIsPCSpeakerEnabled(ppi);
      uint8_t old_keyboard_control = YPPIGetKeyboardControl(ppi);

      ppi->port_b = value;

      // Bit 7: Keyboard enable/clear (0 = enable read, 1 = clear).
      if (value & Y_PPI_PORT_B_KEYBOARD_ENABLE_CLEAR) {
        ppi->port_a_latch = 0;
      }

      // Check for changes in PC speaker control bits and fire callback.
      bool speaker_enabled = YPPIIsPCSpeakerEnabled(ppi);
      if (old_speaker_enabled != speaker_enabled &&
          ppi->config.set_pc_speaker_frequency) {
        const uint32_t frequency = YPPIIsPCSpeakerEnabled(ppi)
                                       ? ppi->pc_speaker_frequency_from_pit
                                       : 0;
        ppi->config.set_pc_speaker_frequency(ppi->config.context, frequency);
      }

      // Check for changes in keyboard control bits and fire callback.
      uint8_t keyboard_control = YPPIGetKeyboardControl(ppi);
      if (old_keyboard_control != keyboard_control &&
          ppi->config.set_keyboard_control) {
        ppi->config.set_keyboard_control(
            ppi->config.context,
            (ppi->port_b & Y_PPI_PORT_B_KEYBOARD_ENABLE_CLEAR) != 0,
            (ppi->port_b & Y_PPI_PORT_B_KEYBOARD_CLOCK_LOW) != 0);
      }
      break;
    }
    case Y_PPI_PORT_CONTROL:
      // The BIOS always writes 0x99 (0b10011001) to set up the PPI. We can
      // ignore it since our emulation is hardcoded to behave accordingly.
      break;

    default:
      // Writes to Port A or C are ignored as they are inputs.
      break;
  }
}

Y_PUBLIC void YPPISetPCSpeakerFrequencyFromPIT(
    YPPIState* ppi, uint32_t frequency_hz) {
  uint32_t old_frequency = ppi->pc_speaker_frequency_from_pit;
  ppi->pc_speaker_frequency_from_pit = frequency_hz;
  // Invoke the callback only if the speaker is currently enabled and the
  // frequency has changed.
  if (YPPIIsPCSpeakerEnabled(ppi) && (frequency_hz != old_frequency) &&
      ppi->config.set_pc_speaker_frequency) {
    ppi->config.set_pc_speaker_frequency(ppi->config.context, frequency_hz);
  }
}

Y_PUBLIC void YPPISetScancode(YPPIState* ppi, uint8_t scancode) {
  ppi->port_a_latch = scancode;
}


// ==============================================================================
// src/ppi/ppi.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_PPI_BUNDLE_H

