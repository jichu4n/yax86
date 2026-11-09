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
  Y_PIT_LOG_MODULE_ID = 3,
};

// Log module for the PIT.
Y_PUBLIC_HEADER const YLogModule YPITLogModule = {
    .id = Y_PIT_LOG_MODULE_ID,
    .name = "pit",
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


// ==============================================================================
// src/pit/pit.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_PIT_BUNDLE_H

