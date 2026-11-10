// ==============================================================================
// YAX86 UTIL MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_UTIL_BUNDLE_H
#define YAX86_UTIL_BUNDLE_H


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
// src/util/static_vector.h start
// ==============================================================================

#line 1 "./src/util/static_vector.h"
// Static vector library.
//
// A static vector is a vector backed by a fixed-size array. It's essentially
// a vector, but whose underlying storage is statically allocated and does not
// rely on dynamic memory allocation.

#ifndef YAX86_UTIL_STATIC_VECTOR_H
#define YAX86_UTIL_STATIC_VECTOR_H

#include <stddef.h>
#include <stdint.h>

// Header structure at the beginning of a static vector.
typedef struct YStaticVectorHeader {
  // Element size in bytes.
  size_t element_size;
  // Maximum number of elements the vector can hold.
  size_t max_length;
  // Number of elements currently in the vector.
  size_t length;
} YStaticVectorHeader;

// Define a static vector type with an element type.
//
// The generated helper functions use plain `static` rather than a visibility
// macro because this macro is instantiated across different headers and tiers
// (both public headers and internal source files); its output's tier depends
// on the instantiation site.
#define Y_STATIC_VECTOR_TYPE(name, element_type, max_length_value)        \
  typedef struct name {                                                   \
    YStaticVectorHeader header;                                           \
    element_type elements[max_length_value];                              \
  } name;                                                                 \
  static void name##Init(name* vector) __attribute__((unused));           \
  static void name##Init(name* vector) {                                  \
    static const YStaticVectorHeader header = {                           \
        .element_size = sizeof(element_type),                             \
        .max_length = (max_length_value),                                 \
        .length = 0,                                                      \
    };                                                                    \
    vector->header = header;                                              \
  }                                                                       \
  static size_t name##Length(const name* vector) __attribute__((unused)); \
  static size_t name##Length(const name* vector) {                        \
    return vector->header.length;                                         \
  }                                                                       \
  static element_type* name##Get(name* vector, size_t index)              \
      __attribute__((unused));                                            \
  static element_type* name##Get(name* vector, size_t index) {            \
    if (index >= (max_length_value)) {                                    \
      return NULL;                                                        \
    }                                                                     \
    return &(vector->elements[index]);                                    \
  }                                                                       \
  static bool name##Append(name* vector, const element_type* element)     \
      __attribute__((unused));                                            \
  static bool name##Append(name* vector, const element_type* element) {   \
    if (vector->header.length >= (max_length_value)) {                    \
      return false;                                                       \
    }                                                                     \
    vector->elements[vector->header.length++] = *element;                 \
    return true;                                                          \
  }                                                                       \
  static bool name##Insert(                                               \
      name* vector, size_t index, const element_type* element)            \
      __attribute__((unused));                                            \
  static bool name##Insert(                                               \
      name* vector, size_t index, const element_type* element) {          \
    if (index > vector->header.length ||                                  \
        vector->header.length >= (max_length_value)) {                    \
      return false;                                                       \
    }                                                                     \
    for (size_t i = vector->header.length; i > index; --i) {              \
      vector->elements[i] = vector->elements[i - 1];                      \
    }                                                                     \
    vector->elements[index] = *element;                                   \
    ++vector->header.length;                                              \
    return true;                                                          \
  }                                                                       \
  static bool name##Remove(name* vector, size_t index)                    \
      __attribute__((unused));                                            \
  static bool name##Remove(name* vector, size_t index) {                  \
    if (index >= vector->header.length) {                                 \
      return false;                                                       \
    }                                                                     \
    for (size_t i = index; i < vector->header.length - 1; ++i) {          \
      vector->elements[i] = vector->elements[i + 1];                      \
    }                                                                     \
    --vector->header.length;                                              \
    return true;                                                          \
  }                                                                       \
  static void name##Clear(name* vector) __attribute__((unused));          \
  static void name##Clear(name* vector) { vector->header.length = 0; }

#endif  // YAX86_UTIL_STATIC_VECTOR_H


// ==============================================================================
// src/util/static_vector.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION



#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_UTIL_BUNDLE_H

// ==============================================================================
// YAX86 BIOS MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_BIOS_BUNDLE_H
#define YAX86_BIOS_BUNDLE_H

#ifndef YAX86_UTIL_BUNDLE_H
#include "util.h"
#endif  // YAX86_UTIL_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/bios/public.h start
// ==============================================================================

#line 1 "./src/bios/public.h"
// Public interface for the BIOS module.
#ifndef YAX86_BIOS_PUBLIC_H
#define YAX86_BIOS_PUBLIC_H

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_BIOS_BUNDLE_H
#include "../util/common.h"
#endif  // YAX86_BIOS_BUNDLE_H

enum {
  // Start address of the BIOS ROM.
  Y_BIOS_ROM_START_ADDRESS = 0xFE000,
};

// Get size of BIOS ROM data.
Y_PUBLIC uint32_t YBIOSGetROMSize(void);

// Get a pointer to the BIOS ROM image, YBIOSGetROMSize() bytes of it. The image
// is a constant array compiled into the library, so the platform maps it
// directly rather than reading it a byte at a time through a callback.
Y_PUBLIC const uint8_t* YBIOSGetROMData(void);

#endif  // YAX86_BIOS_PUBLIC_H


// ==============================================================================
// src/bios/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/bios/bios_rom_data.h start
// ==============================================================================

#line 1 "./src/bios/bios_rom_data.h"
// ROM data automatically generated by generate-rom-data-files.js
// Source: GLABIOS_0.2.5_8E.rom

#ifndef YAX86_BIOS_ROM_DATA_H
#define YAX86_BIOS_ROM_DATA_H

#include <stdint.h>

enum {
  Y_BIOS_ROM_DATA_SIZE = 8192,
};

// In unbundled builds, external callers within the module need an extern
// declaration. In bundled builds, the array is static and defined earlier in
// the same translation unit (via bundle.json), so an extern declaration
// would conflict and is omitted.
#ifndef YAX86_BIOS_BUNDLE_H
extern const uint8_t YBIOSROMData[Y_BIOS_ROM_DATA_SIZE];
#endif // YAX86_BIOS_BUNDLE_H

#endif // YAX86_BIOS_ROM_DATA_H


// ==============================================================================
// src/bios/bios_rom_data.h end
// ==============================================================================

// ==============================================================================
// src/bios/bios_rom_data.c start
// ==============================================================================

#line 1 "./src/bios/bios_rom_data.c"
// ROM data automatically generated by generate-rom-data-files.js
// Source: GLABIOS_0.2.5_8E.rom

#include <stdint.h>

#ifndef YAX86_BIOS_BUNDLE_H
#include "../util/common.h"
#endif // YAX86_BIOS_BUNDLE_H

Y_MODULE_PRIVATE const uint8_t YBIOSROMData[] = {
0x0a,0x47,0x4c,0x61,0x42,0x49,0x4f,0x53,0x20,0x5b,0x03,0x5d,0x20,0x00,0x54,0x68,
0x65,0x20,0x68,0x65,0x72,0x6f,0x20,0x77,0x65,0x20,0x6e,0x65,0x65,0x64,0x20,0x62,
0x75,0x74,0x20,0x64,0x6f,0x6e,0x27,0x74,0x20,0x64,0x65,0x73,0x65,0x72,0x76,0x65,
0x00,0x0d,0x0a,0x28,0x43,0x29,0x20,0x32,0x30,0x32,0x32,0x2d,0x32,0x33,0x20,0x36,
0x34,0x30,0x4b,0x42,0x20,0x52,0x65,0x6c,0x65,0x61,0x73,0x65,0x64,0x20,0x75,0x6e,
0x64,0x65,0x72,0x20,0x47,0x50,0x4c,0x76,0x33,0x0a,0x00,0xfa,0xfc,0xb8,0xa5,0x95,
0x8b,0xd8,0x8e,0xdb,0x8c,0xd9,0x8e,0xc1,0x8c,0xc6,0x8e,0xd6,0x8c,0xd7,0x8b,0xef,
0x8b,0xe5,0x8b,0xd4,0xf7,0xc2,0x5a,0x6a,0xb3,0x21,0x75,0x3a,0x33,0xc0,0x85,0xd2,
0x75,0xde,0xe6,0xa0,0xe6,0x83,0xba,0xd8,0x03,0xee,0x40,0xb2,0xb8,0xee,0x81,0x3e,
0x72,0x04,0x34,0x12,0x75,0x03,0xbd,0x00,0x80,0x48,0xe6,0x43,0xe4,0x40,0xd0,0xe8,
0x24,0x03,0x0b,0xe8,0x8c,0xca,0x8e,0xd2,0xb0,0x99,0xe6,0x63,0xb0,0xad,0xe6,0x61,
0xb0,0x04,0xe6,0x08,0xeb,0x02,0xeb,0x43,0xb0,0x54,0xe6,0x43,0xe6,0x0d,0xb0,0x00,
0xe6,0x08,0xb1,0x03,0xe6,0x0a,0xb0,0x41,0xe6,0x0b,0x40,0xe2,0xfb,0xe4,0x61,0x0c,
0x30,0xe6,0x61,0x91,0x8e,0xd8,0xbe,0x72,0x04,0x8b,0x14,0xbc,0xe1,0xe0,0xe9,0x4e,
0x05,0xe3,0xe0,0x75,0x14,0xe4,0x61,0x0c,0x10,0xe6,0x61,0x24,0xef,0xe6,0x61,0xbc,
0xf5,0xe0,0xe9,0x09,0x05,0xf7,0xe0,0x74,0x05,0xb3,0x13,0xe9,0x76,0x07,0xb8,0xa5,
0x95,0xbc,0x07,0xe1,0xe9,0x28,0x05,0x09,0xe1,0x75,0xee,0xbc,0x11,0xe1,0xe9,0x1e,
0x05,0x13,0xe1,0x75,0xe4,0x87,0x14,0x40,0xa2,0x40,0x04,0xe4,0x62,0x24,0x80,0x74,
0x04,0xb3,0x14,0xeb,0xd6,0xe4,0x61,0x0c,0x30,0xe6,0x61,0xb8,0x30,0x00,0x8e,0xd0,
0xbc,0x00,0x01,0xb2,0x20,0xb0,0x13,0xee,0x42,0xb0,0x08,0xee,0xb7,0x01,0xb0,0x09,
0xee,0x90,0xb0,0xff,0xee,0xb0,0xfe,0xee,0xb0,0x36,0xe6,0x43,0x0e,0x32,0xc0,0xe6,
0x40,0x1f,0xe6,0x40,0x8c,0xc8,0xb1,0x1f,0x33,0xff,0xbe,0xe3,0xfe,0xa5,0xab,0xe2,
0xfc,0xbb,0x00,0xf6,0x8b,0xf3,0xbf,0x55,0xaa,0xb1,0x04,0x8e,0xdb,0xa1,0x00,0x00,
0x3d,0x55,0xaa,0x74,0x18,0x97,0x3b,0xc7,0x74,0x13,0xb0,0x10,0x80,0xc7,0x02,0xe8,
0xcf,0x03,0xe1,0xe7,0x75,0x07,0xbf,0x60,0x00,0x91,0xab,0x96,0xab,0xb8,0x40,0x00,
0x8e,0xd8,0x8e,0xc0,0xb8,0x01,0x01,0xd5,0x10,0x3c,0x0b,0x75,0x05,0x80,0x0e,0x12,
0x00,0x04,0xe4,0x61,0x50,0x24,0xf7,0x50,0xe6,0x61,0xe4,0x62,0x24,0x0f,0x8a,0xe8,
0x58,0x0c,0x08,0xe6,0x61,0xe4,0x62,0xb1,0x04,0xd2,0xe0,0x0a,0xc5,0x50,0xba,0x80,
0x02,0x24,0x30,0x74,0x08,0xb2,0xc0,0x3c,0x30,0x74,0x02,0xb2,0xe0,0x89,0x16,0x15,
0x00,0x58,0xa8,0x02,0x74,0x07,0xe8,0xea,0x1d,0x74,0x02,0x24,0xfd,0xa2,0x10,0x00,
0x92,0x58,0xe6,0x61,0xb8,0x00,0xc0,0xbf,0x00,0xc8,0x52,0xe8,0x15,0x03,0x5a,0xa0,
0x10,0x00,0x83,0xe0,0x30,0xb1,0x04,0xd2,0xc8,0x92,0x75,0x05,0xb3,0x33,0xe9,0x73,
0x06,0xa8,0x30,0x74,0x0f,0x4a,0xb8,0x01,0x00,0x74,0x07,0xb0,0x03,0x4a,0x74,0x02,
0xb0,0x07,0xcd,0x10,0xe8,0xb0,0x16,0x33,0xdb,0xb4,0x03,0xcd,0x10,0x89,0x0e,0xe8,
0x00,0x52,0xba,0x00,0x18,0xb4,0x02,0xcd,0x10,0xbe,0xe0,0xff,0xe8,0xbe,0x14,0xbe,
0xf5,0xff,0xe8,0xb8,0x14,0x5a,0xb4,0x02,0xcd,0x10,0xe9,0xa9,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x50,0xe4,0x62,0xa8,0xc0,0x75,0x02,0x58,0xcf,0xbe,0xb2,0xf7,0xa8,
0x80,0x75,0x03,0xbe,0xb0,0xf7,0x33,0xc0,0xe6,0xa0,0xcd,0x10,0xe8,0x0e,0x14,0xe8,
0xc3,0x14,0xfa,0xf4,0xeb,0xfc,0xbf,0x78,0x00,0xb8,0x14,0x14,0xab,0xab,0xb8,0x01,
0x01,0xab,0xab,0xbb,0x11,0x00,0xba,0x01,0x02,0xec,0xa8,0x0f,0x75,0x03,0x80,0x0f,
0x10,0xb9,0x04,0x00,0x33,0xff,0xba,0xfa,0x03,0x32,0xc0,0xee,0xec,0xec,0x4a,0x4a,
0xa8,0xf0,0x75,0x06,0x92,0xab,0x92,0x80,0x07,0x02,0x81,0xea,0xfe,0x00,0x80,0xf9,
0x03,0x75,0x04,0x81,0xc2,0xf0,0x01,0xe2,0xe0,0xbf,0x08,0x00,0xba,0xbc,0x03,0xb1,
0x03,0xb0,0x0b,0xee,0x42,0x42,0x40,0xee,0x4a,0x4a,0xec,0x34,0x0b,0x75,0x06,0x92,
0xab,0x92,0x80,0x07,0x40,0xfe,0xce,0x80,0xf9,0x03,0x75,0x04,0x81,0xc2,0xbc,0x00,
0xe2,0xdf,0xba,0x61,0x00,0xec,0x24,0x3f,0xee,0x96,0xb8,0x1e,0x00,0xbf,0x1a,0x00,
0xab,0xab,0xbf,0x80,0x00,0xab,0xb0,0x3e,0xab,0xb0,0x1e,0xe8,0x06,0x14,0x96,0x0c,
0xc0,0xee,0x24,0x7f,0xee,0xe4,0x21,0x96,0xb0,0xfd,0xe6,0x21,0xfb,0x90,0xf7,0xc5,
0x00,0x40,0xe1,0xf9,0xfa,0x81,0xf5,0x00,0x40,0xec,0x0c,0xc0,0xee,0x24,0x7f,0xee,
0xe2,0xfe,0xe4,0x60,0x84,0xc0,0x74,0x04,0x81,0xcd,0x00,0x20,0xec,0x0c,0xc0,0xee,
0x96,0xe6,0x21,0x8e,0xc1,0xbf,0x24,0x00,0xb8,0x87,0xe9,0xab,0xe8,0x8c,0x0c,0xe8,
0x8c,0x02,0xe8,0xb0,0x01,0xe8,0x5f,0x12,0xe8,0x77,0x0c,0xe4,0x21,0x24,0xbc,0xe6,
0x21,0xb8,0x00,0xc8,0xbf,0x00,0xfe,0xe8,0x39,0x01,0xfb,0x33,0xc0,0x99,0xcd,0x13,
0x72,0x0d,0xa0,0x10,0x00,0x25,0xc0,0x00,0xb1,0x02,0xd2,0xc0,0x40,0xeb,0x04,0x81,
0xcd,0x00,0x10,0xb3,0x0e,0xbe,0xf4,0xe4,0xe8,0xa9,0x12,0xe8,0x70,0x13,0xe8,0xe1,
0x12,0x98,0xb2,0x80,0xcd,0x13,0x72,0x24,0xb2,0x80,0xe8,0x21,0x03,0x72,0x1d,0xb3,
0x0e,0xbe,0xf8,0xe4,0xe8,0xb4,0x12,0x52,0x92,0xe8,0x52,0x13,0xe8,0xbe,0x12,0x59,
0xe3,0x0d,0xb2,0x80,0xe8,0x4c,0x14,0x42,0xe2,0xfa,0xeb,0x03,0xe8,0xf7,0x12,0xe8,
0x83,0x13,0xb0,0x80,0xe6,0xa0,0xe8,0x26,0x05,0xe4,0x61,0x24,0x7f,0xe6,0x61,0x33,
0xc0,0xa3,0x72,0x00,0x81,0xe5,0xff,0x7f,0x74,0x20,0xbe,0xf5,0xe8,0xe8,0xad,0x12,
0x8b,0xc5,0xe8,0x05,0x13,0xe8,0xce,0x12,0xe8,0xd3,0x04,0xe8,0x74,0x04,0xbe,0xab,
0xe4,0xe8,0x99,0x12,0x91,0xcd,0x16,0xe8,0xbc,0x12,0xb7,0x07,0xb8,0x02,0x07,0xb9,
0x00,0x17,0xba,0x50,0x19,0xcd,0x10,0xe8,0x44,0x04,0xe8,0xa9,0x12,0x99,0xb9,0x03,
0x00,0x51,0xcd,0x19,0x59,0xe2,0xfa,0xcd,0x18,0xbe,0x8e,0xe4,0xe8,0x6e,0x12,0x33,
0xc0,0x8e,0xd8,0xa3,0x72,0x04,0xcd,0x16,0xe9,0xd0,0xfb,0x0d,0x0a,0x00,0x44,0x69,
0x73,0x6b,0x20,0x42,0x6f,0x6f,0x74,0x20,0x46,0x61,0x69,0x6c,0x2e,0x20,0x59,0x6f,
0x75,0x20,0x6d,0x6f,0x6e,0x73,0x74,0x65,0x72,0x2e,0x0a,0x0d,0x0a,0x50,0x72,0x65,
0x73,0x73,0x20,0x74,0x68,0x65,0x20,0x41,0x6e,0x79,0x20,0x4b,0x65,0x79,0x2e,0x2e,
0x2e,0x00,0x43,0x50,0x55,0x00,0x38,0x30,0x38,0x38,0x00,0x46,0x50,0x55,0x00,0x38,
0x30,0x38,0x37,0x00,0x3a,0x00,0x20,0x4d,0x42,0x00,0x52,0x41,0x4d,0x00,0x30,0x30,
0x30,0x20,0x4b,0x42,0x20,0x4f,0x4b,0x00,0x56,0x32,0x30,0x00,0x4c,0x50,0x54,0x00,
0x43,0x4f,0x4d,0x00,0x46,0x44,0x44,0x00,0x48,0x44,0x44,0x00,0x20,0x5b,0x20,0x00,
0x20,0x5d,0x00,0x55,0x1e,0x06,0x8e,0xd8,0xfc,0xbb,0x40,0x00,0x8e,0xc3,0x1e,0xb3,
0x80,0x53,0x33,0xf6,0xad,0x3d,0x55,0xaa,0x75,0x29,0xac,0x3c,0x00,0x7e,0x24,0xe8,
0x2f,0x00,0xe8,0x12,0x14,0x75,0x1c,0x5b,0xb1,0x04,0xd3,0xe8,0x50,0xe4,0x21,0x50,
0x57,0xbf,0x67,0x00,0x26,0x89,0x35,0x26,0x8c,0x5d,0x02,0x26,0xff,0x1d,0xfa,0x5f,
0x58,0xe6,0x21,0x5b,0x58,0x03,0xc3,0x8e,0xd8,0x3b,0xc7,0x72,0xbb,0x07,0x1f,0x5d,
0xc3,0x51,0x56,0x98,0x99,0x8b,0xf2,0x86,0xc4,0x91,0xad,0x02,0xd0,0x02,0xd4,0xe2,
0xf9,0x96,0x5e,0x59,0xc3,0x06,0x1e,0xb8,0x40,0x00,0x8e,0xd8,0xa1,0x15,0x00,0xb1,
0x04,0xd3,0xe8,0x91,0x49,0xba,0x00,0x04,0x8e,0xc2,0xe8,0x81,0x00,0x75,0x07,0x80,
0xc6,0x04,0x8e,0xc2,0xe2,0xf4,0xb1,0x06,0xd3,0xea,0x89,0x16,0x13,0x00,0x8c,0xc2,
0xb3,0x0a,0xbe,0xda,0xe4,0xe8,0xfc,0x10,0xe8,0x52,0x11,0xe8,0x34,0x11,0x33,0xdb,
0x8e,0xc3,0x33,0xc0,0x83,0xc3,0x10,0x83,0xfb,0x64,0x14,0x09,0xe8,0x4d,0x11,0x8b,
0xc3,0x53,0xe8,0x72,0x11,0x5b,0x8c,0xc0,0x05,0x00,0x04,0x3b,0xc2,0x73,0x0d,0x8e,
0xc0,0x33,0xc0,0xb9,0x00,0x20,0x8b,0xf8,0xf3,0xab,0xeb,0xd6,0x1f,0x07,0xc3,0x81,
0xcd,0x00,0x02,0xe8,0x40,0x11,0xb3,0x0c,0xbe,0xfc,0xe8,0xe8,0xb6,0x10,0x4f,0x4f,
0x26,0x33,0x05,0x97,0x8c,0xc3,0xe8,0x55,0x11,0x84,0xd2,0x74,0x08,0xbe,0xb2,0xf7,
0xe8,0xfa,0x10,0xeb,0x04,0x97,0xe8,0x51,0x11,0xe8,0xd6,0x10,0xeb,0xce,0xb0,0x01,
0x33,0xdb,0xbf,0x00,0x20,0x26,0x88,0x07,0xf6,0xd0,0x26,0x88,0x05,0xf6,0xd0,0x26,
0x38,0x07,0x75,0x0f,0xf6,0xd0,0x26,0x38,0x05,0x75,0x08,0xf6,0xd0,0xd0,0xc0,0xd1,
0xef,0x75,0xe2,0xc3,0xb8,0xa5,0x95,0xe8,0x05,0x00,0x75,0xf7,0xb8,0x5a,0x6a,0xb9,
0x00,0x20,0x33,0xff,0xf3,0xab,0xb5,0x20,0x33,0xff,0xf3,0xaf,0x91,0xc3,0xbe,0x55,
0xe6,0xb3,0x04,0xe8,0x4e,0x10,0x85,0xed,0x78,0x05,0xb3,0x09,0xbe,0x5f,0xe6,0xe8,
0x89,0x10,0xe9,0x78,0x10,0x42,0x6f,0x6f,0x74,0x00,0x57,0x41,0x52,0x4d,0x00,0x43,
0x4f,0x4c,0x44,0x00,0xb3,0x0a,0xbe,0x47,0xff,0xe8,0x4f,0x10,0x1e,0x33,0xc0,0x8e,
0xd8,0xc5,0x06,0x40,0x00,0x3d,0x65,0xf0,0x8c,0xd8,0x1f,0x75,0x13,0x8c,0xcb,0x3b,
0xc3,0x75,0x0d,0xe8,0xff,0x09,0xbe,0x27,0xe8,0x72,0x21,0xbe,0x23,0xe8,0xeb,0x1c,
0xb8,0x00,0x1a,0xcd,0x10,0x3c,0x1a,0x75,0x05,0xbe,0x1b,0xe8,0xeb,0x0e,0xb4,0x12,
0xb3,0x10,0xcd,0x10,0x80,0xfb,0x10,0x74,0x03,0xbe,0x1f,0xe8,0xe8,0x3e,0x10,0xe9,
0x1b,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0xfb,0xfc,0x33,0xc9,0x8e,0xd9,0x8e,0xc1,0xbf,0x78,0x00,0xb8,0xc7,0xef,
0xab,0x8c,0xc8,0xab,0x91,0xcd,0x13,0x72,0x14,0xb8,0x01,0x02,0xbb,0x00,0x7c,0xb9,
0x01,0x00,0x8a,0xf5,0xcd,0x13,0x72,0x05,0xea,0x00,0x7c,0x00,0x00,0xcf,0xb4,0x08,
0xcd,0x13,0x72,0x14,0x8b,0xd9,0x86,0xfb,0xd0,0xc7,0xd0,0xc7,0x80,0xe7,0x03,0x83,
0xe1,0x3f,0x8a,0xc6,0x40,0x43,0x32,0xf6,0xc3,0xfb,0x51,0xb9,0x03,0x00,0x3b,0xd1,
0x77,0x56,0x57,0x53,0x52,0x1e,0xbf,0x40,0x00,0x8e,0xdf,0x8b,0xfa,0xd1,0xe7,0x8b,
0x15,0xd1,0xef,0x85,0xd2,0x74,0x3d,0x38,0xcc,0x77,0x39,0x74,0x2d,0x80,0xfc,0x01,
0x77,0x56,0x74,0x36,0x8b,0xfa,0x03,0xd1,0x93,0xb0,0x80,0xee,0xd3,0xe3,0xd2,0xeb,
0xb8,0x17,0x04,0x8a,0xcf,0xe3,0x05,0xb8,0x00,0x06,0xd3,0xe8,0x87,0xd7,0xef,0x93,
0x87,0xd7,0xee,0x87,0xd7,0x33,0xc0,0x42,0xee,0x4a,0x83,0xc2,0x05,0x53,0xec,0x86,
0xe0,0x42,0x5b,0xec,0x1f,0x5a,0x5b,0x5f,0x59,0xcf,0x52,0x50,0x83,0xc2,0x04,0xb0,
0x03,0xee,0x42,0x42,0xbb,0x20,0x30,0xe8,0x24,0x00,0x5b,0x8a,0xc3,0x5a,0x75,0x03,
0xee,0xeb,0xe1,0x80,0xcc,0x80,0xeb,0xdc,0x52,0x83,0xc2,0x04,0xb0,0x01,0xee,0x42,
0x42,0xbb,0x01,0x20,0xe8,0x07,0x00,0x5a,0x80,0xe4,0x1e,0xec,0xeb,0xc6,0xe8,0x05,
0x00,0x75,0x1a,0x86,0xfb,0x4a,0x56,0x33,0xc9,0x8a,0x45,0x7c,0x98,0x96,0xec,0x8a,
0xe0,0x32,0xc7,0x84,0xc7,0x74,0x05,0xe2,0xf5,0x4e,0x75,0xf2,0x5e,0xc3,0xb4,0x01,
0x33,0xc9,0x8b,0xfa,0x8a,0xcf,0x8b,0xd7,0x8a,0xc4,0xee,0xee,0x42,0xd0,0xc0,0xe2,
0xf9,0x8a,0xcf,0x8b,0xd7,0x8a,0xdc,0xec,0x38,0xd8,0x75,0x0e,0xec,0x38,0xd8,0x75,
0x09,0x42,0xd0,0xc3,0xe2,0xf1,0xd0,0xe4,0x75,0xda,0xc3,0x56,0x47,0x41,0x00,0x45,
0x47,0x41,0x00,0x43,0x47,0x41,0x00,0x4d,0x6f,0x6e,0x6f,0x00,0x00,0x00,0xfb,0xfc,
0x1e,0x56,0xbe,0x40,0x00,0x8e,0xde,0x80,0xfc,0x02,0x74,0x33,0x77,0x2e,0x7b,0x15,
0xbe,0x1a,0x00,0xfa,0xad,0x3b,0x04,0x75,0x06,0xfb,0x5e,0x1f,0xca,0x02,0x00,0x96,
0xad,0xeb,0xf6,0xfb,0xf4,0xbe,0x1a,0x00,0xfa,0xad,0x3b,0x04,0x74,0xf5,0x96,0xad,
0x83,0xfe,0x3e,0x72,0x03,0xbe,0x1e,0x00,0x89,0x36,0x1a,0x00,0x5e,0x1f,0xcf,0xa0,
0x17,0x00,0xeb,0xf8,0x8c,0xca,0x8e,0xd2,0x93,0xd4,0x10,0x95,0x8b,0xd5,0xbe,0xf1,
0x08,0x33,0xc9,0x8b,0xf9,0x8b,0xc6,0xbc,0x8c,0xe8,0xeb,0x4f,0x8e,0xe8,0x8b,0xcf,
0xe2,0xfe,0xbc,0x97,0xe8,0xeb,0x57,0x99,0xe8,0x2b,0xcf,0xe2,0xfe,0xfe,0xca,0x75,
0xe4,0x86,0xd6,0x85,0xd2,0x74,0xd5,0xb5,0x55,0xbe,0xa5,0x06,0xeb,0xd5,0x50,0xb8,
0x4c,0x05,0x53,0xe8,0x25,0x00,0x5b,0xb8,0xfa,0x00,0xe8,0xb7,0x0e,0xe8,0x2e,0x00,
0x58,0xc3,0xe8,0x00,0x00,0x50,0xb8,0x4c,0x05,0x53,0xe8,0x0e,0x00,0x5b,0xb5,0x20,
0xe2,0xfe,0xe8,0x19,0x00,0xb5,0x20,0xe2,0xfe,0x58,0xc3,0x93,0xb0,0xb6,0xe6,0x43,
0x93,0xe6,0x42,0x8a,0xc4,0xe6,0x42,0xe4,0x61,0x0c,0x03,0xe6,0x61,0xc3,0xe4,0x61,
0x24,0xfc,0xe6,0x61,0xc3,0x0d,0x0a,0x50,0x4f,0x53,0x54,0x20,0x45,0x72,0x72,0x6f,
0x72,0x20,0x00,0x4b,0x42,0x00,0x4b,0x65,0x79,0x00,0x46,0x44,0x43,0x00,0x44,0x4d,
0x41,0x00,0x03,0xe9,0x06,0xe9,0x0a,0xe9,0xf4,0xe4,0x0e,0xe9,0xda,0xe4,0xb9,0x07,
0x00,0xbe,0x10,0xe9,0x2e,0xad,0xd1,0xe5,0x73,0x07,0x9c,0x96,0xe8,0xe4,0x0d,0x96,
0x9d,0xe0,0xf1,0xc3,0x50,0x81,0xe5,0xff,0xbf,0xe4,0x60,0x3c,0xaa,0x75,0x04,0x81,
0xcd,0x00,0x40,0xe4,0x61,0x0c,0xc0,0xe6,0x61,0xb0,0x20,0xe6,0x20,0x58,0xcf,0x06,
0xb8,0x40,0x00,0x8e,0xc0,0x26,0xa1,0x80,0x00,0xbf,0x1a,0x00,0xab,0xab,0x07,0xc3,
0x00,0x03,0x1e,0x07,0x1f,0x0c,0x7f,0x0e,0x0a,0x1c,0x77,0x47,0x84,0x49,0x73,0x4b,
0x74,0x4d,0x75,0x4f,0x76,0x51,0x72,0x37,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x50,0xe4,0x60,0x8a,0xe0,0xe4,0x61,0x50,0x0c,
0x80,0xe6,0x61,0x58,0xe6,0x61,0xb0,0x20,0xe6,0x20,0xfb,0xfc,0x53,0x51,0x52,0x57,
0x56,0x1e,0x06,0xba,0x40,0x00,0x8a,0xc4,0x3c,0xff,0x74,0x6d,0x8e,0xda,0x8c,0xca,
0x8e,0xc2,0x8b,0x16,0x17,0x00,0x3c,0x44,0x77,0x21,0x3c,0x3b,0x72,0x1d,0xb0,0x00,
0xf6,0xc2,0x0f,0x74,0x16,0x80,0xc4,0x2d,0xf6,0xc2,0x08,0x75,0x0b,0x80,0xec,0x0a,
0xf6,0xc2,0x04,0x75,0x03,0x80,0xec,0x0a,0xe9,0x96,0x00,0xf6,0xc2,0x04,0x74,0x14,
0x3c,0x46,0x75,0x0c,0xe8,0x68,0xff,0xc6,0x06,0x71,0x00,0x80,0xcd,0x1b,0xeb,0x27,
0x3c,0x45,0x74,0x16,0xf6,0xc2,0x20,0x74,0x25,0x3c,0x53,0x77,0x21,0x3c,0x47,0x72,
0x1d,0xd0,0xe0,0xf6,0xc2,0x03,0x74,0x1e,0xeb,0x1b,0xbb,0x18,0x00,0x80,0x0f,0x08,
0xf4,0x90,0xf6,0x07,0x08,0x75,0xf9,0xeb,0x73,0xe8,0xa9,0xfe,0xeb,0x6e,0xd0,0xe0,
0xf6,0xc2,0x0b,0x75,0x01,0x40,0xbb,0x8e,0xeb,0x2e,0xd7,0x84,0xc0,0x79,0x03,0xe9,
0xf9,0x00,0x84,0xe4,0x78,0x56,0xf6,0xc2,0x40,0x74,0x0f,0x50,0x0c,0x20,0x3c,0x61,
0x72,0x03,0x3c,0x7b,0xf5,0x58,0x72,0x02,0x34,0x20,0xf6,0xc2,0x08,0x75,0x46,0x3d,
0x00,0x37,0x74,0x0d,0x3d,0x00,0x4c,0x74,0x33,0xf6,0xc2,0x04,0x74,0x07,0xe9,0xb4,
0x00,0xcd,0x05,0xeb,0x27,0xf6,0xc6,0x08,0x74,0x07,0x80,0x26,0x18,0x00,0xf7,0xeb,
0x1b,0x8b,0x3e,0x1c,0x00,0x8d,0x75,0x02,0x83,0xfe,0x3e,0x72,0x03,0xbe,0x1e,0x00,
0x3b,0x36,0x1a,0x00,0x74,0x93,0x89,0x05,0x89,0x36,0x1c,0x00,0x07,0x1f,0x5e,0x5f,
0x5a,0x59,0x5b,0x58,0xcf,0x80,0xfc,0x39,0x74,0xbf,0x8b,0xd8,0xb0,0x00,0x80,0xfc,
0x0d,0x77,0x0a,0x80,0xfc,0x02,0x72,0x05,0x80,0xc4,0x76,0xeb,0xc4,0x80,0xeb,0x30,
0x72,0x12,0x80,0xfb,0x09,0x77,0x0d,0x93,0x8a,0x26,0x19,0x00,0xd5,0x0a,0xa2,0x19,
0x00,0x93,0xeb,0xc8,0xbf,0x0c,0xec,0xb9,0x0b,0x00,0x86,0xe0,0xf2,0xae,0x86,0xe0,
0x75,0x87,0xeb,0xb8,0x93,0xb9,0x0c,0x00,0xbe,0x60,0xe9,0x2e,0xad,0x38,0xfc,0xe0,
0xfa,0x75,0x0c,0x80,0xf9,0x06,0x7f,0x04,0xb4,0x00,0x86,0xc4,0xe9,0x76,0xff,0x93,
0x80,0xfc,0x27,0x72,0x05,0x80,0xfc,0x29,0x76,0x92,0x80,0xfc,0x10,0x72,0x09,0x80,
0xfc,0x32,0x77,0x04,0x24,0x1f,0xeb,0xe4,0x80,0xfc,0x02,0x72,0xdf,0x80,0xfc,0x35,
0x77,0xda,0xe9,0x77,0xff,0xf6,0xc2,0x08,0x74,0xba,0x80,0xfc,0x53,0x75,0xcd,0xe8,
0x8c,0xfd,0xc7,0x06,0x72,0x00,0x34,0x12,0xe9,0x30,0xf5,0xbb,0x18,0x00,0xbf,0x04,
0xec,0x8a,0xc4,0x24,0x7f,0xb1,0x01,0xae,0x74,0x07,0xd0,0xe1,0x75,0xf9,0xe9,0xf1,
0xfe,0xfa,0x8a,0xc1,0xa8,0xf0,0x74,0x18,0x84,0xe4,0x79,0x06,0xf6,0xd0,0x20,0x07,
0xeb,0x0b,0x08,0x07,0x30,0x47,0xff,0xa8,0x80,0xb0,0x00,0x75,0x2e,0xe9,0x2c,0xff,
0xa8,0x0c,0x74,0x08,0xd0,0xe8,0xd0,0xe8,0x30,0x07,0x8a,0xc1,0x4b,0x84,0xe4,0x78,
0x04,0x08,0x07,0xeb,0xe8,0xf6,0xd0,0x20,0x07,0x80,0xf9,0x08,0x75,0xdf,0xa0,0x19,
0x00,0x84,0xc0,0x74,0xd8,0xb4,0x00,0x88,0x26,0x19,0x00,0xe9,0xe3,0xfe,0x00,0x00,
0x1b,0x1b,0x21,0x31,0x40,0x32,0x23,0x33,0x24,0x34,0x25,0x35,0x5e,0x36,0x26,0x37,
0x2a,0x38,0x28,0x39,0x29,0x30,0x5f,0x2d,0x2b,0x3d,0x08,0x08,0x00,0x09,0x51,0x71,
0x57,0x77,0x45,0x65,0x52,0x72,0x54,0x74,0x59,0x79,0x55,0x75,0x49,0x69,0x4f,0x6f,
0x50,0x70,0x7b,0x5b,0x7d,0x5d,0x0d,0x0d,0x9d,0x9d,0x41,0x61,0x53,0x73,0x44,0x64,
0x46,0x66,0x47,0x67,0x48,0x68,0x4a,0x6a,0x4b,0x6b,0x4c,0x6c,0x3a,0x3b,0x22,0x27,
0x7e,0x60,0xaa,0xaa,0x7c,0x5c,0x5a,0x7a,0x58,0x78,0x43,0x63,0x56,0x76,0x42,0x62,
0x4e,0x6e,0x4d,0x6d,0x3c,0x2c,0x3e,0x2e,0x3f,0x2f,0xb6,0xb6,0x00,0x2a,0xb8,0xb8,
0x20,0x20,0xba,0xba,0x36,0x2a,0x1d,0x38,0x46,0x45,0x3a,0x52,0x0f,0x1c,0x27,0x28,
0x29,0x2b,0x33,0x34,0x35,0x37,0x00,0x00,0xc5,0xc5,0xc6,0xc6,0x37,0x00,0x38,0x00,
0x39,0x00,0x2d,0x2d,0x34,0x00,0x35,0x00,0x36,0x00,0x2b,0x2b,0x31,0x00,0x32,0x00,
0x33,0x00,0x30,0xd2,0x2e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xfb,0xfc,0x1e,0x57,0x52,0x51,0x53,
0x50,0xbf,0x40,0x00,0x8e,0xdf,0xbf,0x41,0x00,0x80,0xfc,0x01,0x72,0x29,0x74,0x74,
0x80,0xfa,0x03,0x77,0x08,0xc6,0x05,0x00,0x80,0xfc,0x06,0x72,0x74,0xc6,0x05,0x01,
0xb0,0x02,0xe8,0x0b,0x03,0xa2,0x40,0x00,0x58,0x8a,0x25,0x5b,0x59,0x5a,0x5f,0x1f,
0x80,0xfc,0x01,0xf5,0xca,0x02,0x00,0xba,0xf2,0x03,0xfa,0xa0,0x3f,0x00,0x8a,0xe8,
0x24,0x0f,0x74,0x0e,0xd0,0xe8,0xa8,0x04,0x74,0x02,0xb0,0x03,0xb1,0x04,0xd2,0xe5,
0x0a,0xc5,0x0c,0x08,0xee,0x33,0xc9,0x88,0x4d,0xfd,0x88,0x0d,0x0c,0x04,0xee,0xfb,
0xe8,0x1e,0x02,0x72,0x04,0x3c,0xc0,0x74,0x02,0xb4,0x20,0x88,0x25,0x75,0xb1,0xb0,
0x03,0xe8,0xcc,0x01,0x72,0xaa,0xb0,0x00,0xe8,0xc2,0x01,0x72,0xa3,0xb0,0x01,0xe8,
0xbb,0x01,0xeb,0x9c,0x58,0x8a,0x05,0x8a,0xe0,0xeb,0xa0,0x80,0x0d,0x09,0xf9,0xeb,
0x64,0xe8,0x0b,0x01,0x72,0x8a,0x50,0x52,0x92,0x8a,0xc6,0xb6,0x4a,0x80,0x4d,0xfe,
0x80,0xa8,0x01,0x75,0x0c,0x80,0x65,0xfe,0x7f,0xb6,0x42,0x3c,0x04,0x74,0x02,0xb6,
0x46,0x92,0x51,0xe6,0x0c,0x8c,0xc2,0xb1,0x04,0xd3,0xc2,0x86,0xc4,0xe6,0x0b,0x32,
0xc0,0x91,0xb0,0x03,0xe8,0x69,0x02,0x91,0xd1,0xe8,0xd3,0xe0,0x48,0x91,0x8a,0xc2,
0x24,0x0f,0x80,0xe2,0xf0,0x03,0xd3,0x14,0x00,0x8b,0xda,0x03,0xd9,0x72,0xac,0xfa,
0xe6,0x81,0x92,0xe6,0x04,0x86,0xc4,0xe6,0x04,0x91,0xe6,0x05,0x86,0xc4,0xe6,0x05,
0xfb,0xb0,0x02,0xe6,0x0a,0x59,0x5a,0x58,0x72,0x88,0xb0,0xc5,0x80,0xfc,0x03,0x74,
0x09,0xb0,0x4d,0x80,0xfc,0x05,0x74,0x02,0xb0,0xe6,0x50,0xe8,0x32,0x01,0x58,0x72,
0x6f,0x8a,0xc6,0x92,0x25,0x03,0x01,0xd0,0xe4,0xd0,0xe4,0x0a,0xc4,0xe8,0x20,0x01,
0x72,0x5e,0x80,0xfe,0x05,0x74,0x5d,0x8a,0xc5,0xe8,0x14,0x01,0x72,0x52,0x8a,0xc2,
0xe8,0x0d,0x01,0x72,0x4b,0x8a,0xc1,0xe8,0x06,0x01,0x72,0x44,0xb0,0x03,0xe8,0xfc,
0x00,0x72,0x3d,0xb0,0x04,0xe8,0xf5,0x00,0x72,0x36,0xb0,0x05,0xe8,0xee,0x00,0x72,
0x2f,0xb0,0x06,0xe8,0xe7,0x00,0x72,0x28,0xe8,0x5e,0x01,0x72,0x23,0x8a,0x65,0x06,
0x8a,0xf2,0x8a,0xd5,0x3b,0x55,0x04,0x74,0x0e,0xb0,0x04,0xe8,0xc2,0x01,0x3a,0x75,
0x05,0x75,0x02,0xd0,0xe0,0x02,0xe0,0x8a,0xc4,0x2a,0xc1,0x59,0x50,0xe9,0xa0,0xfe,
0xb0,0x00,0xeb,0xf7,0xb0,0x03,0xe8,0xb4,0x00,0x72,0xf5,0xb0,0x04,0xe8,0xad,0x00,
0x72,0xee,0xb0,0x07,0xe8,0xa6,0x00,0x72,0xe7,0xb0,0x08,0xb1,0x01,0xeb,0xb4,0x50,
0xe8,0x2b,0x00,0x72,0x26,0xb0,0x0f,0xe8,0x96,0x00,0x72,0x1c,0x8a,0xc2,0xe8,0x8f,
0x00,0x72,0x15,0x8a,0xc5,0xe8,0x88,0x00,0x72,0x0e,0xe8,0xc4,0x00,0x72,0x09,0x34,
0x60,0x24,0x60,0x74,0x03,0xf8,0x58,0xc3,0x80,0x0d,0x40,0xf9,0xeb,0xf8,0x51,0x52,
0x8a,0xca,0x80,0xe1,0x03,0xb8,0x10,0x01,0xd3,0xe0,0xfa,0x8a,0x6d,0xfe,0x84,0xec,
0x75,0x20,0x0a,0xc1,0x0c,0x0c,0xba,0xf2,0x03,0xee,0x8a,0xc5,0x24,0xf0,0x0a,0xc4,
0xb4,0xff,0xa3,0x3f,0x00,0xfb,0xa8,0x80,0x74,0x11,0xb0,0x0a,0xe8,0x31,0x01,0xe8,
0x0e,0x09,0xb0,0x02,0xe8,0x29,0x01,0xa2,0x40,0x00,0xfb,0x5a,0xb0,0x01,0xd2,0xe0,
0x84,0x45,0xfd,0x75,0x1f,0x8a,0xe8,0xb0,0x07,0xe8,0x24,0x00,0x72,0x16,0x8a,0xc1,
0xe8,0x1d,0x00,0x72,0x0f,0xe8,0x59,0x00,0x72,0x0a,0x34,0x60,0x24,0x60,0xf9,0x74,
0x03,0x08,0x6d,0xfd,0x59,0xc3,0x53,0x33,0xdb,0x8a,0xf8,0xeb,0x08,0xe8,0xf0,0x00,
0x53,0x8a,0xf8,0xb3,0x01,0x51,0x52,0xba,0xf4,0x03,0xb9,0x05,0x00,0x8a,0x26,0x6c,
0x00,0xec,0x84,0xc0,0x79,0x19,0x42,0x84,0xdb,0x74,0x09,0xa8,0x40,0x75,0x10,0x8a,
0xc7,0xee,0xeb,0x05,0xa8,0x40,0x74,0x07,0xec,0x32,0xe4,0x5a,0x59,0x5b,0xc3,0xa0,
0x6c,0x00,0x38,0xe0,0x74,0xdb,0x8a,0xe0,0xe0,0xd7,0xb4,0x80,0x08,0x25,0xf9,0xeb,
0xea,0xe8,0x87,0x00,0x72,0x32,0xb0,0x08,0xe8,0xb5,0xff,0x72,0x2b,0x06,0x51,0x57,
0x52,0xba,0xf4,0x03,0xb8,0x40,0x00,0x8e,0xc0,0x47,0xb9,0x07,0x00,0xe8,0x96,0xff,
0x72,0x12,0xaa,0xb0,0x0a,0x48,0x75,0xfd,0xec,0x24,0x10,0xe0,0xf0,0xf3,0xaa,0xa0,
0x42,0x00,0xa8,0xc0,0x5a,0x5f,0x59,0x07,0xc3,0xe8,0x4f,0x00,0xe8,0xcc,0xff,0x76,
0x1d,0x56,0xa8,0x40,0xb0,0x20,0x74,0x10,0x8a,0x65,0x02,0x84,0xe4,0x74,0x09,0xbe,
0x3f,0xef,0x2e,0xac,0xd0,0xe4,0x77,0xfa,0x8a,0xe0,0x5e,0x08,0x25,0xf9,0xc3,0x04,
0x20,0x10,0x08,0x20,0x04,0x03,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xfb,0x50,0x1e,0xb8,0x40,0x00,0x8e,0xd8,0x80,
0x0e,0x3e,0x00,0x80,0xb0,0x20,0xe6,0x20,0x1f,0x58,0xcf,0xfb,0x50,0x51,0xb9,0x25,
0x00,0xa1,0x6c,0x00,0xf6,0x45,0xfd,0x80,0x75,0x0f,0xf4,0x3b,0x06,0x6c,0x00,0x74,
0xf3,0xe2,0xee,0x80,0x0d,0x80,0xf9,0xeb,0x04,0x80,0x65,0xfd,0x7f,0x59,0x58,0xc3,
0x1e,0x53,0x33,0xdb,0x8e,0xdb,0xc5,0x1e,0x78,0x00,0xd7,0x5b,0x1f,0xc3,0x51,0xbf,
0x41,0x00,0xe8,0x89,0xfe,0x72,0x1a,0x85,0xed,0x78,0x12,0xe2,0xfe,0xbe,0xc3,0xef,
0x2e,0xac,0x8a,0xe8,0x51,0xe8,0x47,0xfe,0x59,0xe3,0x02,0x73,0xf3,0xb5,0x40,0xe2,
0xfe,0x59,0xc3,0x26,0x02,0x13,0x00,0xcf,0x02,0x25,0x02,0x08,0x2a,0xff,0x50,0xf6,
0x19,0x04,0xfb,0x83,0xfa,0x02,0x77,0x4a,0x52,0x57,0x51,0x50,0x1e,0xb9,0x40,0x00,
0x8e,0xd9,0x8b,0xfa,0x8a,0x4d,0x78,0xd1,0xe7,0x8b,0x55,0x08,0x1f,0x85,0xd2,0x74,
0x2b,0xfe,0xcc,0x74,0x2e,0x7f,0x1d,0xee,0x42,0x33,0xff,0xec,0xa8,0x80,0x75,0x09,
0x4f,0x75,0xf8,0xe2,0xf4,0x0c,0x01,0xeb,0x0f,0xb0,0x0d,0x42,0x52,0xee,0x5a,0xb0,
0x0c,0xee,0x4a,0x4a,0x42,0xec,0x24,0xf8,0x34,0x48,0x8a,0xe8,0x58,0x8a,0xe5,0x59,
0x5f,0x5a,0xcf,0xb0,0x08,0x42,0x42,0xee,0xb5,0x08,0xe2,0xfe,0x0c,0x04,0xee,0x4a,
0xeb,0xe3,0xe8,0xe1,0x06,0x8b,0x0e,0xe8,0x00,0xeb,0x03,0xb9,0x00,0x20,0x50,0xb4,
0x01,0xcd,0x10,0x58,0xc3,0xe4,0xf0,0x9f,0xf1,0xb5,0xf1,0xfb,0xf1,0x9e,0xf1,0x0c,
0xf2,0x34,0xf2,0x33,0xf2,0x47,0xf3,0xe8,0xf3,0xe8,0xf3,0xd2,0xf4,0xfd,0xf4,0x24,
0xf5,0x76,0xf5,0xe5,0xf5,0xfb,0x80,0xfc,0x0f,0x77,0x19,0x06,0x1e,0x57,0xfc,0xbf,
0x40,0x00,0x8e,0xdf,0x8b,0xf8,0x86,0xe0,0xd0,0xe0,0x98,0x97,0x2e,0xff,0x95,0x45,
0xf0,0x5f,0x1f,0x07,0xcf,0xa0,0x49,0x00,0x3c,0x07,0xf5,0x74,0x02,0xa8,0x04,0xc3,
0x50,0x1e,0xb8,0x40,0x00,0x8e,0xd8,0xa0,0x49,0x00,0x3c,0x02,0x74,0x02,0x3c,0x03,
0x1f,0x58,0xc3,0x00,0x38,0x28,0x2d,0x0a,0x1f,0x06,0x19,0x1c,0x02,0x07,0x06,0x07,
0x00,0x00,0x00,0x00,0x71,0x50,0x5a,0x0a,0x1f,0x06,0x19,0x1c,0x02,0x07,0x06,0x07,
0x00,0x00,0x00,0x00,0x38,0x28,0x2d,0x0a,0x7f,0x06,0x64,0x70,0x02,0x01,0x06,0x07,
0x00,0x00,0x00,0x00,0x61,0x50,0x52,0x0f,0x19,0x06,0x19,0x19,0x02,0x0d,0x0b,0x0c,
0x00,0x00,0x00,0x00,0x53,0x52,0x55,0x51,0x56,0xbf,0x49,0x00,0x3c,0x07,0x72,0x02,
0x8a,0x05,0x93,0x33,0xc0,0xb9,0x0f,0x00,0x1e,0x07,0xf3,0xab,0xa0,0x10,0x00,0x34,
0x30,0x24,0x30,0xb0,0x00,0xbe,0x10,0x00,0xb7,0x50,0xb5,0x10,0x8a,0xe5,0xbf,0x20,
0x07,0x75,0x0e,0x40,0xbd,0x00,0xb0,0xba,0xb8,0x03,0xb3,0x07,0xbe,0x30,0x00,0xeb,
0x26,0xb5,0x40,0xba,0xd8,0x03,0xbd,0x00,0xb8,0xf6,0xc3,0x04,0x74,0x0e,0xbe,0x20,
0x00,0xb4,0x40,0x33,0xff,0xf6,0xc3,0x02,0x75,0x0d,0xeb,0x09,0xf6,0xc3,0x02,0x75,
0x06,0xb4,0x08,0x33,0xf6,0xd0,0xef,0x89,0x1e,0x49,0x00,0x8e,0xc5,0x89,0x2e,0xea,
0x00,0xee,0x88,0x26,0x4d,0x00,0x80,0xea,0x04,0x89,0x16,0x63,0x00,0x8b,0xeb,0x97,
0x33,0xff,0xd1,0xe9,0xf3,0xab,0x1e,0x8b,0xc1,0xa2,0x62,0x00,0x8e,0xd8,0xc5,0x1e,
0x74,0x00,0x8b,0x78,0x0a,0xb1,0x10,0x8a,0x20,0xef,0x40,0x46,0xe2,0xf9,0x95,0x3c,
0x06,0xb4,0x3f,0x74,0x02,0xb4,0x30,0xbb,0x0f,0xf6,0xd7,0x1f,0x80,0xc2,0x04,0xa3,
0x65,0x00,0xef,0x97,0x86,0xe0,0xa3,0x60,0x00,0x5e,0x59,0x5d,0x5a,0x5b,0xc3,0x52,
0x97,0x89,0x0e,0x60,0x00,0xb0,0x0a,0x8a,0xe5,0x8b,0x16,0x63,0x00,0xef,0x40,0x8a,
0xe1,0xef,0x97,0x5a,0xc3,0x50,0x8a,0xc7,0x3c,0x07,0x77,0x30,0x98,0x97,0xd1,0xe7,
0x89,0x95,0x50,0x00,0x38,0x3e,0x62,0x00,0x75,0x22,0x53,0x52,0xa0,0x4a,0x00,0xf6,
0xe6,0x32,0xf6,0x03,0xc2,0x8b,0x1e,0x4e,0x00,0xd1,0xeb,0x03,0xd8,0xb0,0x0e,0x8a,
0xe7,0x8b,0x16,0x63,0x00,0xef,0x40,0x8a,0xe3,0xef,0x5a,0x5b,0x58,0xc3,0x32,0xff,
0xf6,0x06,0x49,0x00,0x04,0x75,0x04,0x8a,0x3e,0x62,0x00,0x50,0x8a,0xc7,0x98,0x97,
0xd1,0xe7,0x8b,0x95,0x50,0x00,0x8b,0x0e,0x60,0x00,0x58,0xc3,0x50,0x53,0x52,0xa2,
0x62,0x00,0x98,0x97,0xa1,0x4c,0x00,0xf7,0xe7,0xa3,0x4e,0x00,0xd1,0xe8,0x8a,0xd8,
0xb0,0x0c,0x8b,0x16,0x63,0x00,0xef,0x40,0x8a,0xe3,0xef,0xd1,0xe7,0x8b,0x95,0x50,
0x00,0xeb,0x99,0xfd,0x50,0x53,0x56,0x55,0x8b,0x36,0xea,0x00,0x8e,0xc6,0x80,0xfc,
0x06,0x9f,0x8b,0xe8,0x8b,0xfa,0x75,0x02,0x8b,0xd1,0xa0,0x4a,0x00,0x38,0xc2,0x72,
0x03,0x8a,0xd0,0x4a,0xf6,0xe6,0x8a,0xf0,0xe8,0x2a,0xfe,0x8a,0xc6,0xb6,0x00,0x1e,
0x75,0x5e,0x03,0xd0,0xd1,0xe2,0x03,0x16,0x4e,0x00,0x8b,0xf2,0x87,0xfa,0x2b,0xd1,
0x8b,0x0e,0x4a,0x00,0x8c,0xc0,0x8e,0xd8,0x8b,0xc5,0xd0,0xe1,0xf6,0xe1,0x95,0x81,
0xc2,0x01,0x01,0x9e,0x8a,0xe7,0x9c,0x8b,0xd9,0x8a,0xca,0x2b,0xd9,0x2b,0xd9,0x9d,
0x74,0x04,0xf7,0xdb,0xf7,0xdd,0x84,0xc0,0x74,0x14,0x2a,0xf0,0x74,0x0e,0x03,0xf5,
0x8a,0xca,0xf3,0xa5,0x03,0xfb,0x03,0xf3,0xfe,0xce,0x75,0xf4,0x8a,0xf0,0xb0,0x20,
0x8a,0xca,0xf3,0xab,0x03,0xfb,0xfe,0xce,0x75,0xf6,0x1f,0x5d,0x5e,0x5b,0x58,0xc3,
0xd1,0xe0,0xd1,0xe0,0x03,0xd0,0x87,0xd7,0x81,0xc2,0x01,0x01,0x2b,0xd1,0x95,0x8a,
0xd8,0xbd,0x50,0x00,0xb9,0x02,0x00,0xd2,0xe6,0xd2,0xe3,0x80,0x3e,0x49,0x00,0x06,
0x74,0x08,0xd1,0xe7,0xd0,0xe2,0x9e,0x74,0x0a,0x47,0x9e,0x74,0x06,0x81,0xc7,0xf0,
0x00,0xf7,0xdd,0x84,0xc0,0x74,0x33,0x8b,0xf7,0x8b,0xc5,0xf6,0xeb,0x03,0xf0,0x8c,
0xc0,0x8e,0xd8,0x53,0x2a,0xf3,0x8b,0xc6,0x8b,0xdf,0x8a,0xca,0xf3,0xa4,0xbe,0x00,
0x20,0x8b,0xfe,0x03,0xf0,0x03,0xfb,0x8a,0xca,0xf3,0xa4,0x03,0xc5,0x03,0xdd,0x8b,
0xf0,0x8b,0xfb,0xfe,0xce,0x75,0xe3,0x5b,0x8a,0xf3,0x8a,0xc7,0x8b,0xf7,0x8a,0xca,
0xf3,0xaa,0xbf,0x00,0x20,0x03,0xfe,0x8a,0xca,0xf3,0xaa,0x03,0xf5,0x8b,0xfe,0xfe,
0xce,0x75,0xeb,0x1f,0xe9,0x74,0xff,0x8b,0x3e,0xea,0x00,0x8e,0xc7,0xe8,0x9d,0x02,
0xa0,0x49,0x00,0x3c,0x07,0x75,0x04,0x26,0x8b,0x05,0xc3,0xd0,0xe8,0x3c,0x01,0x76,
0xf6,0x53,0x51,0x52,0x56,0x83,0xec,0x08,0xe8,0x52,0x01,0x8b,0xf7,0x8b,0xfc,0x06,
0x1f,0x16,0x07,0xb9,0x04,0x00,0xba,0x00,0x20,0x3c,0x03,0x74,0x5d,0xd1,0xe6,0x8b,
0x04,0x86,0xc4,0x8b,0xd8,0xd1,0xe0,0x0b,0xd8,0xb4,0x08,0xd1,0xe3,0x12,0xc0,0xd1,
0xe3,0xfe,0xcc,0x75,0xf6,0xaa,0x33,0xf2,0x85,0xf2,0x75,0xe3,0x83,0xc6,0x50,0xe2,
0xde,0x8b,0xfc,0xbe,0x6e,0xfa,0x8c,0xcb,0x8e,0xdb,0x33,0xc0,0xbb,0x80,0x00,0x56,
0x57,0xb1,0x04,0xf3,0xa7,0x5f,0x5e,0x74,0x19,0x83,0xc6,0x08,0xfe,0xc0,0x74,0x12,
0x4b,0x75,0xec,0x99,0x8e,0xda,0xc5,0x36,0x7c,0x00,0x8c,0xda,0x0b,0xd6,0x75,0xdc,
0x33,0xc0,0x83,0xc4,0x08,0x5e,0x5a,0x59,0x5b,0xc3,0xa4,0x4e,0x33,0xf2,0xa4,0x33,
0xf2,0x83,0xc6,0x4f,0xe2,0xf4,0xeb,0xb9,0x51,0x50,0x8b,0x3e,0xea,0x00,0x8e,0xc7,
0xe8,0xfa,0x01,0xa0,0x49,0x00,0x3c,0x07,0x75,0x12,0x58,0x80,0xfc,0x09,0x74,0x06,
0xaa,0x47,0xe2,0xfc,0x59,0xc3,0x8a,0xe3,0xf3,0xab,0x59,0xc3,0xd0,0xe8,0x3c,0x01,
0x76,0xe8,0x58,0x50,0x53,0x52,0x56,0x1e,0x8a,0x3e,0x49,0x00,0xe8,0x9e,0x00,0xbe,
0x6e,0xfa,0x8c,0xca,0x8e,0xda,0x84,0xc0,0x79,0x0a,0x24,0x7f,0x33,0xd2,0x8e,0xda,
0xc5,0x36,0x7c,0x00,0x98,0xd1,0xe0,0xd1,0xe0,0xd1,0xe0,0x03,0xf0,0x80,0xff,0x06,
0x74,0x56,0x8a,0xd3,0x83,0xe2,0x03,0x0a,0xf2,0xd0,0xe2,0xd0,0xe2,0x75,0xf8,0x8a,
0xd6,0xd1,0xe7,0x56,0x57,0x51,0xb9,0x08,0x00,0xac,0x53,0x51,0x33,0xdb,0xb1,0x08,
0xd0,0xe0,0x9f,0xd1,0xd3,0x9e,0xd1,0xd3,0xe2,0xf6,0x93,0x86,0xc4,0x59,0x5b,0x23,
0xc2,0x84,0xdb,0x79,0x03,0x26,0x33,0x05,0x26,0x89,0x05,0x81,0xf7,0x00,0x20,0xf7,
0xc7,0x00,0x20,0x75,0x03,0x83,0xc7,0x50,0xe2,0xcf,0x59,0x5f,0x5e,0x47,0x47,0xe2,
0xc2,0x1f,0x5e,0x5a,0x5b,0x58,0x59,0xc3,0x56,0x57,0xb7,0x04,0xad,0x84,0xdb,0x79,
0x08,0x26,0x32,0x05,0x26,0x32,0xa5,0x00,0x20,0xaa,0x26,0x88,0xa5,0xff,0x1f,0x83,
0xc7,0x4f,0xfe,0xcf,0x75,0xe6,0x5f,0x5e,0x47,0xe2,0xdd,0xeb,0xd4,0x8b,0xf8,0xa0,
0x4a,0x00,0x8b,0x16,0x50,0x00,0xf6,0xe6,0xd1,0xe0,0xd1,0xe0,0x92,0x98,0x03,0xc2,
0x97,0xc3,0x50,0xa0,0x66,0x00,0x84,0xff,0x75,0x07,0x24,0xe0,0x80,0xe3,0x1f,0xeb,
0x0b,0x24,0xdf,0x80,0xe3,0x01,0xd0,0xcb,0xd0,0xcb,0xd0,0xcb,0x0a,0xc3,0xa2,0x66,
0x00,0x52,0x8b,0x16,0x63,0x00,0x80,0xc2,0x05,0xee,0x5a,0x58,0xc3,0x50,0x51,0x8b,
0x3e,0xea,0x00,0x8e,0xc7,0xe8,0x2f,0x00,0x8a,0xe8,0x22,0xc4,0xd2,0xe0,0x84,0xed,
0x78,0x0d,0xd2,0xe4,0xf6,0xd4,0x26,0x22,0x25,0x0a,0xc4,0xaa,0x59,0x58,0xc3,0x26,
0x30,0x05,0xeb,0xf8,0x51,0x8b,0x3e,0xea,0x00,0x8e,0xc7,0xe8,0x09,0x00,0x26,0x8a,
0x05,0xd2,0xe8,0x22,0xc4,0x59,0xc3,0x53,0x52,0x8b,0xf9,0xd1,0xef,0xd1,0xef,0xb4,
0x03,0x8a,0xdc,0x8a,0xe9,0xb1,0x01,0x80,0x3e,0x49,0x00,0x06,0x72,0x07,0xd0,0xec,
0xd0,0xd3,0x49,0xd1,0xef,0x22,0xeb,0x2a,0xdd,0xd2,0xe3,0x8a,0xcb,0xd1,0xea,0x86,
0xd6,0x73,0x04,0x81,0xc7,0x00,0x20,0xd1,0xea,0xd1,0xea,0x03,0xfa,0xd1,0xea,0xd1,
0xea,0x03,0xfa,0x5a,0x5b,0xc3,0x50,0x53,0x51,0x52,0xe8,0x71,0xfc,0x3c,0x0d,0x76,
0x40,0xb9,0x01,0x00,0xe8,0x61,0xfe,0x8a,0x0e,0x4a,0x00,0x49,0x38,0xca,0x72,0x02,
0xb2,0xff,0x42,0x80,0xfe,0x18,0x76,0x21,0x32,0xe4,0xfe,0xce,0xe8,0xe6,0xfa,0x75,
0x03,0xe8,0xa3,0xfd,0x53,0x93,0xb8,0x01,0x06,0x33,0xc9,0x52,0x8a,0x16,0x4a,0x00,
0xb6,0x18,0x4a,0xfc,0xe8,0x7d,0xfc,0x5a,0x5b,0xe8,0xf9,0xfb,0x5a,0x59,0x5b,0x58,
0xc3,0x74,0x13,0x3c,0x0a,0x74,0x13,0x3c,0x08,0x74,0x13,0x3c,0x07,0x75,0xb2,0x5a,
0x59,0x5b,0x58,0xe9,0xd8,0xf2,0x32,0xd2,0xeb,0xdf,0xfe,0xc6,0xeb,0xb5,0x84,0xd2,
0x74,0xda,0x4a,0xeb,0xae,0xa1,0x49,0x00,0x8a,0x3e,0x62,0x00,0xc3,0x50,0x52,0x8a,
0xc7,0x98,0x97,0xa1,0x4c,0x00,0xf7,0xe7,0xd1,0xe7,0x8b,0x95,0x50,0x00,0x97,0xa0,
0x4a,0x00,0xf6,0xe6,0x92,0x98,0x03,0xc2,0xd1,0xe0,0x03,0xf8,0x5a,0x58,0xc3,0x2c,
0x28,0x2d,0x29,0x2a,0x2e,0x1e,0x29,0xe8,0x4a,0xf0,0xb3,0x0a,0xbe,0xc2,0xe4,0xe8,
0x72,0x00,0xf6,0x06,0x12,0x00,0x04,0x74,0x03,0xbe,0xe8,0xe4,0xe8,0xbe,0x00,0xe8,
0xa0,0x00,0xb3,0x0a,0xbe,0xcb,0xe4,0xe8,0x81,0x00,0xf6,0x06,0x10,0x00,0x02,0x75,
0x03,0xbe,0x4d,0xff,0xe8,0xa6,0x00,0xe8,0x83,0x00,0xb3,0x0e,0xbe,0xec,0xe4,0xe8,
0x42,0x00,0xa0,0x11,0x00,0x25,0xc0,0x00,0xb1,0x02,0xd2,0xc0,0x91,0xbe,0x08,0x00,
0xe8,0x15,0x00,0xb3,0x0e,0xbe,0xf0,0xe4,0xe8,0x50,0x00,0xa0,0x11,0x00,0x83,0xe0,
0x0e,0xb1,0x01,0xd2,0xc8,0x91,0x33,0xf6,0xe3,0x0e,0xad,0xe8,0xcc,0x00,0xe8,0x9e,
0x00,0xe2,0xf7,0xbe,0x01,0xe5,0xeb,0x4d,0xbe,0x4d,0xff,0xe8,0x5f,0x00,0xeb,0x42,
0xe8,0x83,0x00,0x58,0x50,0xb0,0x06,0x53,0x8a,0xe3,0xb9,0x09,0x00,0xbb,0x03,0x00,
0xe8,0x38,0x00,0xe8,0x56,0x00,0x56,0xbe,0xfc,0xe4,0xe8,0x40,0x00,0x8a,0xdc,0x4e,
0xb1,0x13,0x51,0xe8,0x25,0x00,0x59,0x5e,0x5b,0x58,0xc3,0x50,0xb4,0x0f,0xcd,0x10,
0x3c,0x01,0x7e,0xcc,0xb0,0x20,0xe8,0x33,0x00,0xb0,0x26,0xeb,0xca,0xe8,0x02,0x00,
0xeb,0x44,0xbe,0x00,0xe5,0xb9,0x02,0x00,0xbb,0x03,0x00,0x50,0xe8,0xa6,0xf9,0x73,
0x06,0x80,0xe3,0x0f,0x80,0xcb,0x07,0xb8,0x20,0x09,0xcd,0x10,0x58,0x50,0xb4,0x0e,
0x2e,0xac,0x84,0xc0,0x74,0x04,0xcd,0x10,0xeb,0xf6,0x58,0xc3,0x50,0x53,0x51,0x52,
0x50,0x32,0xff,0xb4,0x03,0xcd,0x10,0x58,0x8a,0xd0,0xb4,0x02,0xcd,0x10,0x5a,0x59,
0x5b,0x58,0xc3,0xe8,0xd7,0xff,0x56,0xbe,0x8b,0xe4,0xe8,0xd0,0xff,0x5e,0xc3,0x50,
0xb8,0x20,0x0e,0xcd,0x10,0x58,0xc3,0xbb,0x0a,0x00,0x52,0x33,0xd2,0xf7,0xf3,0x85,
0xc0,0x74,0x03,0xe8,0xf4,0xff,0x92,0x35,0x30,0x0e,0xcd,0x10,0x5a,0xc3,0x50,0x8b,
0xc3,0xe8,0x06,0x00,0xb0,0x3a,0xe8,0x1c,0x00,0x58,0x50,0x8a,0xc4,0xe8,0x01,0x00,
0x58,0x50,0xd0,0xe8,0xd0,0xe8,0xd0,0xe8,0xd0,0xe8,0xe8,0x01,0x00,0x58,0x24,0x0f,
0x3c,0x0a,0x1c,0x69,0x2f,0x50,0x53,0x33,0xdb,0xb4,0x0e,0xcd,0x10,0x5b,0x58,0xc3,
0xb4,0x7d,0xf6,0xe4,0x53,0xbb,0x52,0x09,0x93,0x51,0x52,0xf7,0xe3,0x93,0xe8,0x14,
0x00,0x8b,0xc8,0xe8,0x0f,0x00,0x2b,0xc8,0x2b,0xd9,0x8b,0xc8,0x83,0xda,0x00,0x73,
0xf2,0x5a,0x59,0x5b,0xc3,0xb0,0x00,0x9c,0xfa,0xe6,0x43,0xe4,0x40,0x8a,0xe0,0xe4,
0x40,0x9d,0x86,0xc4,0xc3,0xe4,0x61,0x0c,0x30,0xe6,0x61,0x34,0x30,0xe6,0x61,0xc3,
0x49,0x4f,0x20,0x50,0x41,0x52,0x49,0x54,0x59,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0xfb,0x1e,0xb8,0x40,0x00,0x8e,0xd8,0xa1,0x13,0x00,0x1f,0xcf,0xfb,0x1e,0xb8,
0x40,0x00,0x8e,0xd8,0xa1,0x10,0x00,0x1f,0xcf,0xfb,0xb4,0x86,0x80,0xfc,0x01,0xf5,
0xca,0x02,0x00,0x51,0x52,0x52,0xe8,0xb5,0xee,0x5a,0x72,0x54,0x51,0x50,0x53,0x50,
0x53,0x51,0xb8,0x20,0x09,0xbb,0x03,0x00,0xb9,0x01,0x00,0xcd,0x10,0x59,0x5b,0x58,
0x92,0x24,0x03,0x04,0x43,0xe8,0xdd,0xfe,0x92,0xf6,0xe1,0xf7,0xe3,0xb9,0x00,0x08,
0xf7,0xf1,0xb3,0x0a,0xbe,0xd4,0xe4,0xe8,0xfa,0xfd,0xe8,0x8a,0xfe,0xe8,0x4d,0xfe,
0x58,0xe8,0x2e,0xfe,0xb3,0x0a,0xbe,0xc3,0xf8,0xe8,0x0f,0xfe,0xe8,0x78,0xfe,0xe8,
0x6d,0xfe,0x58,0xe8,0x74,0xfe,0xe8,0x66,0xfe,0x58,0xe8,0x6d,0xfe,0xe8,0x0d,0xfe,
0x5a,0x59,0xc3,0x43,0x48,0x53,0x00,0xbe,0x00,0xe0,0xe8,0x20,0xfe,0x8b,0xc5,0x83,
0xe5,0xfc,0x83,0xe0,0x03,0x96,0xd1,0xe6,0x2e,0x8b,0xb4,0x2f,0xf9,0xe8,0x0d,0xfe,
0xbe,0x31,0xe0,0xe8,0x2d,0xfe,0xc3,0x52,0x65,0x61,0x64,0x79,0x20,0x74,0x6f,0x20,
0x62,0x65,0x6c,0x69,0x65,0x76,0x65,0x20,0x79,0x6f,0x75,0x00,0x41,0x62,0x6f,0x72,
0x74,0x2c,0x20,0x52,0x65,0x74,0x72,0x79,0x2c,0x20,0x53,0x75,0x63,0x63,0x65,0x65,
0x64,0x00,0x57,0x68,0x65,0x6e,0x20,0x6c,0x69,0x66,0x65,0x20,0x67,0x69,0x76,0x65,
0x73,0x20,0x79,0x6f,0x75,0x20,0x6c,0x65,0x6d,0x6f,0x6e,0x61,0x64,0x65,0x00,0x0e,
0xe0,0xe7,0xf8,0xfc,0xf8,0x12,0xf9,0x50,0x56,0x9c,0x81,0xff,0x00,0xc8,0x76,0x32,
0x50,0xb3,0x0e,0xbe,0x8b,0xf9,0xe8,0x4b,0xfd,0x8c,0xd8,0xe8,0xfc,0xfd,0xe8,0x81,
0xfd,0x58,0x9d,0x9c,0x75,0x20,0x50,0xb3,0x0e,0xbe,0x8f,0xf9,0xe8,0x5c,0xfd,0x58,
0x86,0xe0,0xd0,0xe8,0xd0,0xe8,0xe8,0xbe,0xfd,0xbe,0x9c,0xf9,0xe8,0x7e,0xfd,0xe8,
0x5b,0xfd,0x9d,0x5e,0x58,0xc3,0xb3,0x0c,0xbe,0xfc,0xe8,0xe8,0x3d,0xfd,0xbe,0x94,
0xf9,0xe8,0x69,0xfd,0x8a,0xc2,0xe8,0xc8,0xfd,0xeb,0xe4,0x52,0x4f,0x4d,0x00,0x53,
0x69,0x7a,0x65,0x00,0x43,0x68,0x6b,0x73,0x75,0x6d,0x20,0x00,0x20,0x4b,0x42,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x7e,0x81,0xa5,0x81,0xbd,0x99,0x81,0x7e,0x7e,0xdb,
0xff,0xff,0xc3,0xe7,0x7e,0x00,0x36,0x7f,0x7f,0x7f,0x3e,0x1c,0x08,0x00,0x08,0x1c,
0x3e,0x7f,0x3e,0x1c,0x08,0x00,0x18,0x3c,0x18,0x66,0xff,0x66,0x18,0x3c,0x08,0x1c,
0x3e,0x7f,0x7f,0x3e,0x08,0x1c,0x00,0x00,0x18,0x3c,0x3c,0x18,0x00,0x00,0xff,0xff,
0xe7,0xc3,0xc3,0xe7,0xff,0xff,0x00,0x3c,0x66,0x42,0x42,0x66,0x3c,0x00,0xff,0xc3,
0x99,0xbd,0xbd,0x99,0xc3,0xff,0x3f,0x0d,0x1c,0x3e,0x63,0x63,0x3e,0x00,0x3e,0x63,
0x63,0x3e,0x1c,0x7f,0x1c,0x00,0x0e,0x0f,0x0d,0x0d,0x1c,0x7f,0x1c,0x00,0x0f,0x3b,
0x37,0x3b,0x33,0x37,0x77,0x70,0x18,0xdb,0x3c,0xe7,0x3c,0xdb,0x18,0x00,0x60,0x78,
0x7e,0x7f,0x7e,0x78,0x60,0x00,0x03,0x0f,0x3f,0x7f,0x3f,0x0f,0x03,0x00,0x18,0x3c,
0x7e,0x18,0x18,0x7e,0x3c,0x18,0x66,0x66,0x66,0x66,0x66,0x00,0x66,0x00,0x3f,0x6d,
0x6d,0x3d,0x0d,0x0d,0x0d,0x00,0x3f,0x70,0x3e,0x63,0x63,0x3e,0x07,0x7e,0x00,0x00,
0x00,0x00,0xff,0xff,0xff,0x00,0x3c,0x7e,0x18,0x18,0x7e,0x3c,0x18,0x7e,0x18,0x3c,
0x7e,0x18,0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x18,0x7e,0x3c,0x18,0x00,0x18,0x0c,
0x06,0x7f,0x06,0x0c,0x18,0x00,0x0c,0x18,0x30,0x7f,0x30,0x18,0x0c,0x00,0x00,0x00,
0x00,0x60,0x60,0x7f,0x00,0x00,0x00,0x24,0x66,0xff,0x66,0x24,0x00,0x00,0x08,0x1c,
0x1c,0x3e,0x3e,0x7f,0x7f,0x00,0x7f,0x7f,0x3e,0x3e,0x1c,0x1c,0x08,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00,0x33,0x66,
0xcc,0x00,0x00,0x00,0x00,0x00,0x36,0x36,0x7f,0x36,0x36,0x7f,0x36,0x36,0x18,0x7e,
0x58,0x38,0x1c,0x1a,0x7e,0x18,0xe3,0xa6,0xec,0x18,0x37,0x65,0xc7,0x00,0x3c,0x66,
0x3c,0x38,0x6d,0x66,0x3d,0x00,0x06,0x0c,0x18,0x00,0x00,0x00,0x00,0x00,0x1c,0x30,
0x60,0x60,0x60,0x30,0x1c,0x00,0x38,0x0c,0x06,0x06,0x06,0x0c,0x38,0x00,0x63,0x36,
0x1c,0x7f,0x1c,0x36,0x63,0x00,0x18,0x18,0x18,0x7e,0x18,0x18,0x18,0x00,0x00,0x00,
0x00,0x00,0x00,0x18,0x18,0x30,0x00,0x00,0x00,0x7e,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x18,0x18,0x00,0x02,0x06,0x0c,0x18,0x30,0x60,0x40,0x00,0x3e,0x63,
0x63,0x6b,0x63,0x63,0x3e,0x00,0x1c,0x3c,0x0c,0x0c,0x0c,0x0c,0x0c,0x00,0x3e,0x63,
0x03,0x0e,0x38,0x60,0x7f,0x00,0x7c,0x06,0x06,0x3c,0x06,0x06,0x7c,0x00,0x0e,0x1e,
0x36,0x66,0x7f,0x06,0x06,0x00,0x7e,0x60,0x7c,0x06,0x06,0x06,0x7c,0x00,0x1e,0x30,
0x60,0x7e,0x63,0x63,0x3e,0x00,0x7e,0x66,0x0c,0x18,0x30,0x30,0x30,0x00,0x3e,0x63,
0x36,0x1c,0x36,0x63,0x3e,0x00,0x3e,0x63,0x63,0x3f,0x03,0x06,0x7c,0x00,0x00,0x00,
0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x18,0x18,0x00,0x18,0x18,0x30,0x0c,0x18,
0x30,0x60,0x30,0x18,0x0c,0x00,0x00,0x00,0x7e,0x00,0x7e,0x00,0x00,0x00,0x30,0x18,
0x0c,0x06,0x0c,0x18,0x30,0x00,0x3e,0x63,0x06,0x0c,0x0c,0x00,0x0c,0x00,0x3e,0x63,
0x6f,0x69,0x6f,0x60,0x3f,0x00,0x3c,0x66,0x66,0x7e,0x66,0x66,0x66,0x00,0x7e,0x63,
0x63,0x7e,0x63,0x63,0x7e,0x00,0x1e,0x33,0x60,0x60,0x60,0x33,0x1e,0x00,0x7c,0x66,
0x63,0x63,0x63,0x66,0x7c,0x00,0x7e,0x60,0x60,0x7c,0x60,0x60,0x7e,0x00,0x7e,0x60,
0x60,0x7c,0x60,0x60,0x60,0x00,0x1e,0x33,0x63,0x60,0x67,0x33,0x1e,0x00,0x63,0x63,
0x63,0x7f,0x63,0x63,0x63,0x00,0x3c,0x18,0x18,0x18,0x18,0x18,0x3c,0x00,0x06,0x06,
0x06,0x06,0x66,0x66,0x3c,0x00,0x63,0x66,0x6c,0x78,0x6c,0x66,0x63,0x00,0x30,0x30,
0x30,0x30,0x30,0x30,0x3f,0x00,0x63,0x77,0x7f,0x6b,0x63,0x63,0x63,0x00,0x63,0x73,
0x7b,0x6f,0x67,0x63,0x63,0x00,0x3e,0x63,0x63,0x63,0x63,0x63,0x3e,0x00,0x7e,0x63,
0x63,0x7e,0x60,0x60,0x60,0x00,0x3e,0x63,0x63,0x63,0x7b,0x6e,0x3c,0x07,0x7e,0x63,
0x63,0x7e,0x6c,0x66,0x63,0x00,0x3e,0x63,0x30,0x1c,0x06,0x63,0x3e,0x00,0x7e,0x18,
0x18,0x18,0x18,0x18,0x18,0x00,0x63,0x63,0x63,0x63,0x63,0x63,0x3e,0x00,0x63,0x63,
0x63,0x63,0x36,0x1c,0x08,0x00,0x63,0x63,0x63,0x6b,0x6b,0x7f,0x36,0x00,0x63,0x63,
0x36,0x1c,0x36,0x63,0x63,0x00,0x63,0x63,0x63,0x3e,0x0c,0x0c,0x0c,0x00,0x7f,0x06,
0x0c,0x18,0x30,0x60,0x7f,0x00,0x3e,0x30,0x30,0x30,0x30,0x30,0x3e,0x00,0x40,0x60,
0x30,0x18,0x0c,0x06,0x02,0x00,0x3e,0x06,0x06,0x06,0x06,0x06,0x3e,0x00,0x1c,0x36,
0x63,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x30,0x18,
0x0c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3c,0x06,0x3e,0x66,0x3f,0x00,0x60,0x60,
0x7c,0x66,0x66,0x66,0x7c,0x00,0x00,0x00,0x3e,0x63,0x60,0x60,0x3f,0x00,0x06,0x06,
0x3e,0x66,0x66,0x66,0x3e,0x00,0x00,0x00,0x3c,0x66,0x7c,0x60,0x3e,0x00,0x1e,0x30,
0x30,0x7c,0x30,0x30,0x30,0x00,0x00,0x00,0x3f,0x63,0x63,0x3f,0x03,0x7e,0x60,0x60,
0x6c,0x76,0x66,0x66,0x66,0x00,0x18,0x00,0x38,0x18,0x18,0x18,0x18,0x00,0x06,0x00,
0x06,0x06,0x06,0x06,0x66,0x3c,0x60,0x60,0x66,0x6c,0x78,0x6c,0x66,0x00,0x18,0x18,
0x18,0x18,0x18,0x18,0x0c,0x00,0x00,0x00,0x76,0x7f,0x6b,0x6b,0x63,0x00,0x00,0x00,
0x6c,0x76,0x66,0x66,0x66,0x00,0x00,0x00,0x3e,0x63,0x63,0x63,0x3e,0x00,0x00,0x00,
0x7c,0x66,0x66,0x7c,0x60,0x60,0x00,0x00,0x3e,0x66,0x66,0x3e,0x06,0x06,0x00,0x00,
0x36,0x3b,0x30,0x30,0x30,0x00,0x00,0x00,0x3e,0x70,0x3c,0x0e,0x7c,0x00,0x18,0x18,
0x7e,0x18,0x18,0x18,0x0e,0x00,0x00,0x00,0x66,0x66,0x66,0x66,0x3b,0x00,0x00,0x00,
0x66,0x66,0x66,0x3c,0x18,0x00,0x00,0x00,0x63,0x63,0x6b,0x7f,0x36,0x00,0x00,0x00,
0x63,0x36,0x1c,0x36,0x63,0x00,0x00,0x00,0x66,0x66,0x66,0x3e,0x06,0x7c,0x00,0x00,
0x7e,0x0c,0x18,0x30,0x7e,0x00,0x0e,0x18,0x18,0x78,0x18,0x18,0x0e,0x00,0x18,0x18,
0x18,0x00,0x18,0x18,0x18,0x00,0x70,0x18,0x18,0x0e,0x18,0x18,0x70,0x00,0x3b,0x6e,
0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x3c,0x66,0xc3,0xc3,0xff,0x00,0x00,0xfb,0x80,
0xec,0x01,0x77,0x18,0x1e,0x56,0xbe,0x40,0x00,0x8e,0xde,0xbe,0x6c,0x00,0xfa,0x74,
0x0c,0xad,0x92,0xad,0x91,0x33,0xc0,0x86,0x04,0xfb,0x5e,0x1f,0xcf,0x89,0x14,0x89,
0x4c,0x02,0x88,0x64,0x04,0xeb,0xf2,0xb0,0xf0,0x20,0x06,0x3f,0x00,0x34,0xfc,0xba,
0xf2,0x03,0xee,0xeb,0x20,0x50,0x52,0x1e,0x57,0xb8,0x40,0x00,0x8e,0xd8,0x99,0xbf,
0x6c,0x00,0x83,0x05,0x01,0x11,0x55,0x02,0x83,0x7d,0x02,0x18,0x73,0x13,0xfb,0xfe,
0x0e,0x40,0x00,0x74,0xd2,0xcd,0x1c,0xfa,0xb0,0x20,0xe6,0x20,0x5f,0x1f,0x5a,0x58,
0xcf,0x80,0x3d,0xb0,0x72,0xe8,0x89,0x15,0x89,0x55,0x02,0xfe,0x45,0x04,0xeb,0xde,
0x00,0x00,0x00,0x23,0xff,0x23,0xff,0xc3,0xe2,0x23,0xff,0x23,0xff,0x54,0xff,0x23,
0xff,0x23,0xff,0xa5,0xfe,0x34,0xe9,0x23,0xff,0x23,0xff,0x23,0xff,0x23,0xff,0x57,
0xef,0x23,0xff,0x65,0xf0,0x4d,0xf8,0x41,0xf8,0x59,0xec,0x39,0xe7,0x59,0xf8,0x2e,
0xe8,0xd2,0xef,0x79,0xe4,0xf2,0xe6,0x6e,0xfe,0x53,0xff,0x53,0xff,0xa4,0xf0,0xc7,
0xef,0x00,0x00,0x50,0x1e,0xb0,0x0b,0xe6,0x20,0xb8,0x40,0x00,0x8e,0xd8,0xe4,0x20,
0x48,0x7c,0x0d,0x40,0x8a,0xe0,0xe4,0x21,0x0a,0xc4,0xe6,0x21,0xb0,0x20,0xe6,0x20,
0x88,0x26,0x6b,0x00,0x1f,0x58,0xcf,0x56,0x69,0x64,0x65,0x6f,0x00,0x4e,0x6f,0x6e,
0x65,0x00,0x00,0xcf,0xfc,0x50,0x57,0x06,0xbf,0x50,0x00,0x8e,0xc7,0x33,0xff,0xb0,
0x01,0xae,0x74,0x42,0xfb,0x4f,0xaa,0x53,0x51,0x52,0xb4,0x0f,0xcd,0x10,0x8a,0xdc,
0x4b,0xb4,0x03,0xcd,0x10,0x52,0xe8,0x31,0x00,0x72,0x1f,0x99,0xb4,0x02,0xcd,0x10,
0xb4,0x08,0xcd,0x10,0xe8,0x2c,0x00,0x72,0x11,0x38,0xd3,0x75,0x07,0xb2,0xff,0xe8,
0x18,0x00,0x72,0x06,0x42,0x80,0xfe,0x19,0x75,0xe2,0x1a,0xc0,0x4f,0xaa,0x5a,0xb4,
0x02,0xcd,0x10,0x5a,0x59,0x5b,0x07,0x5f,0x58,0xcf,0xb0,0x0d,0xe8,0x04,0x00,0x72,
0x11,0xb0,0x0a,0x52,0xb4,0x00,0x99,0x84,0xc0,0x75,0x02,0xb0,0x20,0xcd,0x17,0xd0,
0xec,0x5a,0xc3,0x97,0xdb,0xe3,0x50,0x8b,0xdc,0x36,0xdd,0x3f,0x90,0x58,0x84,0xc0,
0x75,0x0c,0x50,0x36,0xd9,0x3f,0x90,0x58,0x35,0x3f,0x10,0x83,0xe0,0x3f,0x97,0xc3,
0x56,0x65,0x72,0x3a,0x20,0x30,0x2e,0x32,0x2e,0x35,0x2d,0x38,0x45,0x20,0x00,0x00,
0xea,0x5b,0xe0,0x00,0xf0,0x30,0x37,0x2f,0x33,0x31,0x2f,0x32,0x33,0x00,0xfe,0xed,
};


// ==============================================================================
// src/bios/bios_rom_data.c end
// ==============================================================================

// ==============================================================================
// src/bios/bios.c start
// ==============================================================================

#line 1 "./src/bios/bios.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "bios_rom_data.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

Y_PUBLIC uint32_t YBIOSGetROMSize(void) { return Y_BIOS_ROM_DATA_SIZE; }

Y_PUBLIC const uint8_t* YBIOSGetROMData(void) { return YBIOSROMData; }


// ==============================================================================
// src/bios/bios.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_BIOS_BUNDLE_H

// ==============================================================================
// YAX86 CPU MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_CPU_BUNDLE_H
#define YAX86_CPU_BUNDLE_H

#ifndef YAX86_UTIL_BUNDLE_H
#include "util.h"
#endif  // YAX86_UTIL_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/cpu/public.h start
// ==============================================================================

#line 1 "./src/cpu/public.h"
// Public interface for the CPU emulator module.
#ifndef YAX86_CPU_PUBLIC_H
#define YAX86_CPU_PUBLIC_H

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_CPU_BUNDLE_H
#include "../util/log.h"
#endif  // YAX86_CPU_BUNDLE_H

enum {
  // Log module ID for the CPU.
  Y_CPU_LOG_MODULE_ID = 1,
};

// Log module for the CPU.
Y_PUBLIC_HEADER const YLogModule YCPULogModule = {
    .id = Y_CPU_LOG_MODULE_ID,
    .name = "cpu",
};

// ============================================================================
// CPU state
// ============================================================================

// CPU registers.
// Note that the order / numeric values of these constants are important here as
// they must match how the registers are encoded in the ModR/M byte.
typedef enum YCPURegisterIndex {
  // General-purpose and index registers.

  // Accumulator Register
  Y_CPU_AX = 0,
  // Counter Register
  Y_CPU_CX = 1,
  // Data Register
  Y_CPU_DX = 2,
  // Base Register
  Y_CPU_BX = 3,
  // Stack Pointer Register
  Y_CPU_SP = 4,
  // Base Pointer Register
  Y_CPU_BP = 5,
  // Source Index Register
  Y_CPU_SI = 6,
  // Destination Index Register
  Y_CPU_DI = 7,

  // Segment registers.

  // Extra Segment Register
  Y_CPU_ES = 8,
  // Code Segment Register
  Y_CPU_CS = 9,
  // Stack Segment Register
  Y_CPU_SS = 10,
  // Data Segment Register
  Y_CPU_DS = 11,

  // Instruction Pointer Register
  Y_CPU_IP,
} YCPURegisterIndex;

enum {
  // Number of registers.
  Y_CPU_NUM_REGISTERS = Y_CPU_IP + 1,
};

// CPU flag masks.
typedef enum YCPUFlag {
  // Carry Flag
  Y_CPU_CF = (1 << 0),
  // Parity Flag
  Y_CPU_PF = (1 << 2),
  // Auxiliary Carry Flag
  Y_CPU_AF = (1 << 4),
  // Zero Flag
  Y_CPU_ZF = (1 << 6),
  // Sign Flag
  Y_CPU_SF = (1 << 7),
  // Trap Flag
  Y_CPU_TF = (1 << 8),
  // Interrupt Enable Flag
  Y_CPU_IF = (1 << 9),
  // Direction Flag
  Y_CPU_DF = (1 << 10),
  // Overflow Flag
  Y_CPU_OF = (1 << 11),
} YCPUFlag;

enum {
  // Bits of the flags register that are not flags. The 8086/8088 does not
  // store them: bit 1 and bits 12 through 15 always read as one, and bits 3
  // and 5 always read as zero, whatever gets written over them.
  Y_CPU_FLAGS_ALWAYS_SET = 0xF002,
  Y_CPU_FLAGS_ALWAYS_CLEAR = 0x0028,
  // CPU flags value on reset - no flags set, and the bits that are not flags
  // reading the only way they can.
  Y_CPU_INITIAL_FLAGS = Y_CPU_FLAGS_ALWAYS_SET,
};

// Standard interrupts.
typedef enum YCPUInterruptNumber {
  Y_CPU_INTERRUPT_DIVIDE_ERROR = 0,
  Y_CPU_INTERRUPT_SINGLE_STEP = 1,
  Y_CPU_INTERRUPT_NMI = 2,
  Y_CPU_INTERRUPT_BREAKPOINT = 3,
  Y_CPU_INTERRUPT_OVERFLOW = 4,
} YCPUInterruptNumber;

// Result of executing a single instruction.
typedef enum YCPUInstructionResult {
  // The instruction was executed.
  Y_CPU_INSTRUCTION_EXECUTED = 0,
  // The instruction could not be executed, because its opcode has no handler or
  // because its encoding does not match the expected format for its opcode.
  Y_CPU_INSTRUCTION_INVALID,
} YCPUInstructionResult;

// Result of a single CPU tick.
typedef enum YCPUTickResult {
  // An instruction was executed. This includes HLT: the tick that halts the
  // CPU still ran an instruction.
  Y_CPU_TICK_EXECUTED = 0,
  // The instruction at CS:IP could not be fetched or executed. The CPU is left
  // pointing past the offending instruction; it is up to the caller to decide
  // whether to continue.
  Y_CPU_TICK_INVALID,
  // The CPU was already halted and executed no instruction this tick. It stays
  // halted until an interrupt wakes it, so the caller must keep ticking the
  // rest of the machine.
  Y_CPU_TICK_HALTED,
} YCPUTickResult;

// Result of the handle_interrupt callback, directing how the CPU should
// proceed with an interrupt.
typedef enum YCPUInterruptHandlerResult {
  // The callback serviced the interrupt itself. The CPU restores the state
  // saved on entry and resumes at the interrupted instruction.
  Y_CPU_INTERRUPT_HANDLER_HANDLED = 0,
  // The callback did not service the interrupt. The CPU dispatches it through
  // the Interrupt Vector Table instead.
  Y_CPU_INTERRUPT_HANDLER_UNHANDLED,
} YCPUInterruptHandlerResult;

struct YCPUState;
struct YCPUInstruction;
struct YCPUDecodeCacheEntry;

// Caller-provided runtime configuration.
typedef struct YCPUConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger for this module. May be NULL.
  YLogger* logger;

  // Callback to read a byte from memory.
  //
  // On the 8086, accessing an invalid memory address will yield garbage data
  // rather than causing a page fault. This callback interface mirrors that
  // behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  uint8_t (*read_memory_byte)(struct YCPUState* cpu, uint32_t address);

  // Callback handing the CPU a run of bytes it may read directly, for
  // instruction fetch. Optional - when NULL, every instruction byte is read
  // through read_memory_byte, which is what happens anyway wherever this
  // declines.
  //
  // Given a linear address the CPU wants to fetch from, fills in
  // YCPUState.instruction_fetch_window with a range covering it, or sets its
  // data to NULL to decline. A host must decline wherever a read has to be
  // observed or computed rather than loaded: a device region, unmapped memory,
  // or an address covered by anything the host has to be told about.
  //
  // The range should be the whole of whatever region the address falls in
  // rather than the tail of it starting at the address, so that a jump
  // backwards within the same region still lands inside it.
  //
  // The window is kept across instructions. Writes through the same buffer
  // need no invalidation, and self-modifying code keeps working, because the
  // window is a pointer into the host's own storage rather than a copy of it.
  // A host that wants it dropped clears data itself.
  void (*get_instruction_fetch_window)(struct YCPUState* cpu, uint32_t address);

  // Callback to write a byte to memory.
  //
  // On the 8086, accessing an invalid memory address will yield garbage data
  // rather than causing a page fault. This callback interface mirrors that
  // behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  void (*write_memory_byte)(
      struct YCPUState* cpu, uint32_t address, uint8_t value);

  // Callback modeling the interrupt acknowledge cycle the CPU runs in response
  // to a request on its INTR pin. Returns false if no external interrupt is
  // requested; otherwise stores the vector number and marks the interrupt in
  // service in the controller, exactly as the two INTA pulses do on real
  // hardware.
  //
  // The CPU only calls this at an instruction boundary with interrupts
  // enabled, so acknowledging and taking the interrupt are a single step and
  // no request can be latched, stranded, or overwritten in between. If NULL,
  // the CPU takes no external interrupts.
  bool (*acknowledge_interrupt)(struct YCPUState* cpu, uint8_t* vector);

  // Optional flag saying whether acknowledge_interrupt could possibly have
  // anything to report. The CPU reads it at every instruction boundary with
  // interrupts enabled, which is nearly all of them, and skips the
  // acknowledge cycle entirely while it reads false.
  //
  // This points into the interrupt controller's own state rather than being a
  // value the host hands over, so there is nothing to keep in step and nothing
  // to invalidate - a change the controller makes is visible to the next
  // instruction.
  //
  // The host may make it conservative: true where nothing turns out to be
  // takeable costs only a call that reports nothing. It may never be falsely
  // false, because the CPU takes it as permission not to ask at all. A host
  // that cannot promise that leaves it NULL and is asked every time.
  const bool* interrupt_request_hint;

  // Optional storage for the decode cache, which lets an instruction the guest
  // runs more than once be decoded once. NULL means every instruction is
  // decoded.
  //
  // Read only by YCPUInit(), which checks the count and clears its own copy of
  // the pointer if it does not pass.
  struct YCPUDecodeCacheEntry* decode_cache;
  // Must be a power of two of at least two. See decode_cache_index_mask.
  uint32_t decode_cache_num_entries;

  // Callback to handle an interrupt. If NULL, every interrupt is dispatched
  // through the Interrupt Vector Table.
  YCPUInterruptHandlerResult (*handle_interrupt)(
      struct YCPUState* cpu, uint8_t interrupt_number);

  // Callback to read a byte from an I/O port.
  //
  // On the 8086, accessing an invalid I/O port will most likely yield garbage
  // data. This callback interface mirrors that behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  uint8_t (*read_port)(struct YCPUState* cpu, uint16_t port);

  // Callback to write a byte to an I/O port.
  //
  // On the 8086, accessing an invalid I/O port will most likely yield garbage
  // data. This callback interface mirrors that behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  void (*write_port)(struct YCPUState* cpu, uint16_t port, uint8_t value);
} YCPUConfig;

// A run of bytes instruction fetch may read directly, covering the linear
// addresses [start, end), with data pointing at the byte at start.
//
// A NULL data means there is no window and every byte takes the ordinary path
// through YCPUConfig.read_memory_byte.
typedef struct YCPUInstructionFetchWindow {
  const uint8_t* data;
  uint32_t start;
  uint32_t end;
} YCPUInstructionFetchWindow;

// Guest memory the CPU may read and write by indexing, covering the half-open
// range of linear addresses [0, end), with data pointing at the byte at 0.
//
// This is not the same thing as the whole of guest RAM. It is the part of the
// address space that is plain host storage reachable with a bounds check and
// an index, so a host whose memory is not all directly addressable - some of
// it behind a bus, or reached through a driver - hands over the part that is
// and serves the rest through the callbacks.
//
// There is no start to go with end because the window always begins at 0,
// which is where guest RAM begins. That is what makes the bounds test a single
// unsigned compare.
//
// end is 0 exactly when data is NULL, which is what lets that compare stand
// alone rather than needing a null check beside it.
// YCPUSetDirectDataWindow() is what keeps the two in step.
typedef struct YCPUDirectDataWindow {
  uint8_t* data;
  uint32_t end;
} YCPUDirectDataWindow;

enum {
  // Granularity at which a write to guest memory discards decoded
  // instructions: 4KB pages over the 8086's 1MB, which is 256 counters. It is
  // the coarsest page that still separates the code a program is running from
  // the data it is writing, and a finer one would only cost more memory.
  Y_CPU_CODE_PAGE_SHIFT = 12,
  Y_CPU_CODE_PAGE_SIZE = 1 << Y_CPU_CODE_PAGE_SHIFT,
  Y_CPU_CODE_PAGE_OFFSET_MASK = Y_CPU_CODE_PAGE_SIZE - 1,
  Y_CPU_NUM_CODE_PAGES = 0x100000 >> Y_CPU_CODE_PAGE_SHIFT,
};

// State of the emulated CPU.
//
// A caller zero-initializes one of these, fills in the fields of config, and
// calls YCPUInit(). Everything outside config is the CPU's own. A register or
// flag may be written directly, since nothing is derived from one; a field
// another field depends on has a setter, which is its sole writer.
typedef struct YCPUState {
  // Register values
  uint16_t registers[Y_CPU_NUM_REGISTERS];
  // Flag values
  uint16_t flags;

  // An internal interrupt, raised by the CPU itself as a result of the
  // instruction it just executed: INT n, INT 3, INTO, a divide error, or a
  // single-step trap. IF does not gate these - INT 21h works with interrupts
  // disabled, which is how DOS calls inside critical sections behave.
  bool has_pending_internal_interrupt;
  // Interrupt number of the pending internal interrupt.
  uint8_t pending_internal_interrupt_number;

  // Whether the CPU is in halted state. When true, YCPUTick() will not fetch
  // or execute any instructions until an external event (e.g., an interrupt)
  // clears this state.
  bool is_halted;

  // Instructions retired since YCPUInit(), as the two halves of a 64-bit count.
  // A halted tick retires none. Read them as one number with
  // YCPUInstructionsRetired().
  //
  // Counted here rather than left to the caller because YCPUTick() already
  // knows whether it ran an instruction, while a caller can only find out by
  // sampling is_halted before every tick - which is what every benchmark
  // harness did, at the cost of giving up batching.
  //
  // Split rather than held as a uint64_t because the increment sits in the
  // hottest basic block the emulator has. A 64-bit one is eight instructions
  // on a Cortex-M0+ - two loads, two constants, an add, an add with carry and
  // two stores - where the low half alone is three and the carry above it is a
  // branch not taken until 2^32 instructions have run. Worth 1.19% at -O3.
  // The count still has to reach 64 bits: a machine left running retires 2^32
  // in a couple of hours.
  uint32_t instructions_retired_low;
  uint32_t instructions_retired_high;

  // Cycles charged by the instruction currently executing on top of its base
  // cost: its time on the data bus, and whatever it adds for itself when its
  // cost depends on its operands. YCPUTick clears this before each instruction
  // and folds it into cycles_this_tick afterwards.
  uint16_t pending_cycles;

  // Cycles the last call to YCPUTick consumed, at the 4.77MHz CPU clock. The
  // caller drives the rest of the machine from this, so that everything timed
  // against the CPU keeps the ratio real hardware has.
  //
  // A tick that runs several instructions charges the whole run here, so this
  // is what the run cost rather than what one instruction cost.
  uint16_t cycles_this_tick;

  // The run of bytes instruction fetch is currently reading from, as handed
  // over by YCPUConfig.get_instruction_fetch_window.
  //
  // Kept across instructions rather than re-derived per instruction: execution
  // is sequential and a window spans a whole memory region, so the next
  // instruction is almost always inside the one already open.
  YCPUInstructionFetchWindow instruction_fetch_window;

  // Guest memory the CPU reads and writes by indexing, as handed over by
  // YCPUSetDirectDataWindow(). Unlike the fetch window this is not asked for
  // per access: data accesses are scattered rather than sequential, so there
  // is no locality for a per-access callback to exploit and the host sets it
  // once instead.
  YCPUDirectDataWindow direct_data_window;

  // One less than YCPUConfig.decode_cache_num_entries, so that an address
  // becomes an index with a mask rather than a remainder - a remainder by a
  // runtime value is a division, which this target has no instruction for.
  // Derived by YCPUInit().
  uint32_t decode_cache_index_mask;

  // How many times each 4KB page has been written, as a wrapping byte. A
  // cached decode records what its page stood at and is discarded once the two
  // disagree, which is what makes caching safe against code that writes over
  // itself or is loaded over an earlier program.
  //
  // Maintained whether or not a decode cache exists: a write cannot cheaply
  // tell whether anything holds a decode of the page it lands on, and the test
  // to find out would cost about what the counter does.
  uint8_t code_page_generation[Y_CPU_NUM_CODE_PAGES];

  // Caller-provided runtime configuration. Filled in before YCPUInit(), which
  // is where it is checked, and read-only afterwards.
  //
  // Held by value rather than by pointer: a pointer cost 3.37% at -O2 on a
  // path that reads a config field every instruction, and made the config an
  // object the caller had to keep alive for as long as the CPU.
  YCPUConfig config;
} YCPUState;

// Initialize CPU state. The caller zero-initializes the YCPUState and fills in
// the fields of its config first.
//
// This zeroes nothing - a struct the caller has already had to zero in order
// to fill in its config does not want zeroing twice - and it is the only place
// the config is checked, so a host is told about a mistake here or not at
// all.
Y_PUBLIC void YCPUInit(YCPUState* cpu);

// Instructions retired since YCPUInit(), as one number.
Y_PUBLIC_HEADER inline uint64_t YCPUInstructionsRetired(const YCPUState* cpu) {
  return ((uint64_t)cpu->instructions_retired_high << 32) |
         cpu->instructions_retired_low;
}

// Get the value of a CPU flag.
Y_PUBLIC_HEADER inline bool YCPUGetFlag(const YCPUState* cpu, YCPUFlag flag) {
  return (cpu->flags & flag) != 0;
}
// Set a CPU flag.
Y_PUBLIC_HEADER inline void YCPUSetFlag(
    YCPUState* cpu, YCPUFlag flag, bool value) {
  if (value) {
    cpu->flags |= flag;
  } else {
    cpu->flags &= ~flag;
  }
}

// Raise an interrupt from within the CPU, to be taken at the end of the
// instruction currently executing. This is for the sources the 8086 calls
// internal - INT n, INT 3, INTO, a divide error, a single-step trap - which
// are not maskable by IF. External requests arrive on the INTR pin instead,
// via the acknowledge_interrupt callback.
Y_PUBLIC_HEADER inline void YCPURaiseInternalInterrupt(
    YCPUState* cpu, uint8_t interrupt_number) {
  cpu->has_pending_internal_interrupt = true;
  cpu->pending_internal_interrupt_number = interrupt_number;
}

// Discard a pending internal interrupt without taking it.
Y_PUBLIC_HEADER inline void YCPUClearInternalInterrupt(YCPUState* cpu) {
  cpu->has_pending_internal_interrupt = false;
  cpu->pending_internal_interrupt_number = 0;
}

// Charge the instruction currently executing for cycles beyond its base cost.
// Used by the instructions whose cost is not a property of the opcode alone -
// a conditional jump that is taken, a shift by a count in CL, a multiply or a
// divide.
Y_PUBLIC void YCPUAddCycles(YCPUState* cpu, uint16_t cycles);

// Hands the CPU guest memory it may read and write by indexing, covering the
// half-open range of linear addresses [0, end). Optional - a host that
// supplies none has every access go through YCPUConfig.read_memory_byte and
// YCPUConfig.write_memory_byte, which is also what happens for every address at
// or above end.
//
// The window must be plain storage whose reads and writes are the caller's
// buffer and nothing else. A host must not hand over a region where a read has
// to be observed or computed - a device, or anything else the host has to be
// told about - because an access through the window is a load or a store and
// the host never learns of it.
//
// Writes through the same buffer need no further call, so a host writing guest
// memory itself, or by DMA, stays coherent with the CPU for free. What does
// need a call is a change to what an address means: remapping memory, or
// enabling something that has to observe accesses, calls
// YCPUInvalidateDirectDataWindow().
Y_PUBLIC_HEADER inline void YCPUSetDirectDataWindow(
    YCPUState* cpu, uint8_t* data, uint32_t end) {
  cpu->direct_data_window.data = data;
  cpu->direct_data_window.end = data ? end : 0;
}

// Discards the direct data window, so that every access goes back through
// YCPUConfig.read_memory_byte and YCPUConfig.write_memory_byte.
Y_PUBLIC_HEADER inline void YCPUInvalidateDirectDataWindow(YCPUState* cpu) {
  cpu->direct_data_window.data = NULL;
  cpu->direct_data_window.end = 0;
}

// Discards every cached decode.
//
// A host calls this when it changes what an address means rather than what is
// stored at it - remapping memory is the case that matters. Ordinary writes
// are covered by YCPUNotifyMemoryWrite() instead.
Y_PUBLIC void YCPUInvalidateDecodeCache(YCPUState* cpu);

// Tells the CPU that the byte at a linear address has been written, so that
// any decode taken from that page stops being used.
//
// The CPU calls this for every write it makes itself, so what a host has to
// report is a write it makes some other way - by DMA, or through the memory
// map from outside a tick.
//
// The counter is a byte, so it comes back round every 256 writes to a page, at
// which point a decode taken exactly that long ago would look current again -
// hence the flush. The address is masked rather than range checked: aliasing
// onto a page costs a spurious invalidation, where indexing past the array
// would corrupt whatever follows it.
Y_PUBLIC_HEADER inline void YCPUNotifyMemoryWrite(
    YCPUState* cpu, uint32_t address) {
  const uint32_t page =
      (address >> Y_CPU_CODE_PAGE_SHIFT) & (Y_CPU_NUM_CODE_PAGES - 1);
  if (++cpu->code_page_generation[page] == 0) {
    YCPUInvalidateDecodeCache(cpu);
  }
}

// ============================================================================
// Instructions
// ============================================================================

enum {
  // Maximum number of displacement bytes in an 8086 instruction.
  Y_CPU_MAX_DISPLACEMENT_BYTES = 2,
  // Maximum number of immediate data bytes in an 8086 instruction.
  Y_CPU_MAX_IMMEDIATE_BYTES = 4,
  // Bytes an instruction carries besides its prefixes: the opcode, a ModR/M
  // byte, displacement, and immediate data.
  Y_CPU_MAX_NON_PREFIX_BYTES = 2 + Y_CPU_MAX_DISPLACEMENT_BYTES +
      Y_CPU_MAX_IMMEDIATE_BYTES,
  // Maximum number of prefix bytes accepted.
  //
  // The 8086 puts no limit on this. A real one fetches prefixes for as long as
  // they keep coming, and a stream of nothing but prefixes hangs it outright -
  // a prefix is not an instruction boundary, so no interrupt is ever
  // recognized. Two things here cannot follow it that far: the fetch loop has
  // to hand control back to its caller, and YCPUInstruction.size is a uint8_t
  // that
  // YCPUTick adds to IP. Accepting as many prefixes as still leave the whole
  // encoding addressable by size satisfies both, and is far past anything an
  // assembler emits.
  Y_CPU_MAX_PREFIX_BYTES = 0xFF - Y_CPU_MAX_NON_PREFIX_BYTES,
};

// Instruction prefixes.
typedef enum {
  Y_CPU_PREFIX_ES = 0x26,    // ES segment override
  Y_CPU_PREFIX_CS = 0x2E,    // CS segment override
  Y_CPU_PREFIX_SS = 0x36,    // SS segment override
  Y_CPU_PREFIX_DS = 0x3E,    // DS segment override
  Y_CPU_PREFIX_LOCK = 0xF0,  // LOCK
  // Undocumented alias of LOCK on the 8086/8088, which does not decode bit 0
  // of this opcode.
  Y_CPU_PREFIX_LOCK_ALT = 0xF1,
  Y_CPU_PREFIX_REPNZ = 0xF2,  // REPNE/REPNZ
  Y_CPU_PREFIX_REP = 0xF3,    // REP/REPE/REPZ
} YCPUInstructionPrefix;

enum {
  // Value of YCPUInstruction.segment_override when an instruction carries no
  // segment override prefix. Zero is Y_CPU_AX, which is never a segment
  // register,
  // so a zero-initialized YCPUInstruction correctly has no override.
  Y_CPU_NO_SEGMENT_OVERRIDE = 0,
};

// The Mod R/M byte.
// The fields are stored a byte each rather than packed back into the bit
// positions they were decoded from. Every read of one would otherwise be a
// load, a shift and a mask, and mod and rm are read on the hottest path in the
// emulator - YCPUGetMemoryOperandAddress() takes them for every memory operand
// and YCPUGetEffectiveAddressCycles() for every instruction - while reg selects
// the handler for all five instruction groups.
typedef struct YCPUModRM {
  // Mod field - bits 6 and 7 of the ModR/M byte.
  uint8_t mod;
  // REG field - bits 3 to 5 of the ModR/M byte.
  uint8_t reg;
  // R/M field - bits 0 to 2 of the ModR/M byte.
  uint8_t rm;
} YCPUModRM;

// An encoded instruction.
//
// Prefixes are resolved into fields as they are fetched rather than kept as
// raw bytes. Every consumer wants to know what a prefix selected, not which
// byte encoded it, and a field spares each of them a walk over the bytes.
//
// How many prefixes there were is not among them - nothing needs it, and
// size already accounts for the bytes they occupied. LOCK and its
// undocumented 0xF1 alias are consumed but not recorded at all, because
// nothing acts on them: the bus is not shared on a PC/XT.
//
// No field here is a bitfield, for the reason given at YCPUModRM above. That
// puts the struct at 16 bytes, which is what a decode cache entry is sized
// around - so the size is worth watching again, though for how many decodes fit
// in a given amount of memory rather than for what a fetch costs.
typedef struct YCPUInstruction {
  // The segment register selected by a segment override prefix, as a
  // YCPURegisterIndex, or Y_CPU_NO_SEGMENT_OVERRIDE if the instruction carries
  // none. A later override wins over an earlier one, as on hardware.
  uint8_t segment_override;

  // The repetition prefix present - Y_CPU_PREFIX_REP or Y_CPU_PREFIX_REPNZ - or
  // 0 if the instruction carries neither. A later one wins over an earlier one.
  uint8_t repetition_prefix;

  // The primary opcode byte.
  uint8_t opcode;

  // The ModR/M byte, which specifies addressing modes. For some instructions,
  // the REG field within this byte acts as an opcode extension.
  YCPUModRM mod_rm;

  // Raw displacement bytes. If displacement_size is 1, only disp_bytes[0] is
  // valid (value is typically sign-extended). If displacement_size is 2,
  // disp_bytes[0] is the low byte, disp_bytes[1] is the high byte.
  uint8_t displacement[Y_CPU_MAX_DISPLACEMENT_BYTES];

  // Raw immediate data bytes.
  uint8_t immediate[Y_CPU_MAX_IMMEDIATE_BYTES];

  // Flags

  // Flag indicating if a ModR/M byte is part of this instruction.
  bool has_mod_rm;
  // Number of displacement bytes present: 0, 1, or 2. A whole byte can express
  // more than displacement[] holds, so a consumer bounds itself by the array
  // rather than trusting this - see the fetch, which clamps its own writes.
  uint8_t displacement_size;
  // Number of immediate data bytes present: 0, 1, 2, or 4. Same caveat as
  // displacement_size.
  uint8_t immediate_size;

  // Total length of the original encoded instruction in bytes.
  uint8_t size;

  // What the instruction costs before it runs: its base cost from the opcode
  // table plus the effective address computation. Both are settled by the
  // encoding, so the decode works them out once and every later run of the
  // same decode is charged from here.
  uint16_t base_cycles;
} YCPUInstruction;

// One cached decode.
//
// A hit is used in place - the executor is handed a pointer to the instruction
// inside the entry and nothing is copied. That is the whole of why caching
// pays: copying the struct out measures 3.25% slower at -O3, because it costs
// about what decoding a short instruction from an open fetch window costs. No
// instruction handler writes through the YCPUInstruction it is given, so
// lending out the entry is safe.
typedef struct YCPUDecodeCacheEntry {
  // The linear address the instruction starts at, which is the key. First so
  // that the entry packs into 24 bytes: YCPUInstruction is 18 and needs 2-byte
  // alignment, where the key needs 4.
  uint32_t address;
  YCPUInstruction instruction;
  // What code_page_generation said for that address's page when the decode was
  // taken. A hit requires it to still say the same.
  uint8_t generation;
  bool valid;
} YCPUDecodeCacheEntry;

// ============================================================================
// Execution
// ============================================================================

// Result status from fetching the next instruction.
typedef enum YCPUFetchNextInstructionStatus {
  Y_CPU_FETCH_SUCCESS = 0,
  // Prefix exceeds maximum allowed size.
  Y_CPU_FETCH_PREFIX_TOO_LONG = -1,
} YCPUFetchNextInstructionStatus;

// Fetch the next instruction from CS:IP.
//
// The instruction is decoded directly into dest_instruction, so on failure
// dest_instruction holds however much had been decoded when the fetch failed.
// Since this function is part of the core CPU execution loop, assembling and
// copying a whole instruction struct would have a measurable impact on
// performance.
Y_PUBLIC YCPUFetchNextInstructionStatus
YCPUFetchNextInstruction(YCPUState* cpu, YCPUInstruction* instruction);

// Execute a single fetched instruction.
Y_PUBLIC YCPUInstructionResult
YCPUExecuteInstruction(YCPUState* cpu, YCPUInstruction* instruction);

enum {
  // The most instructions one call to YCPUTick() will run back to back.
  //
  // This is a bound on how coarse a tick can be, not on how long a run should
  // be - what governs that is max_run_cycles, which is what keeps a device
  // from being serviced late. This only caps the case where the next deadline
  // is far away, and its cost is that YPlatformRun() overshoots the budget it
  // was given by up to one run.
  //
  // On dos-boot, raising it from 4 to 16 is worth 2.9% and 32 only 0.5% beyond
  // that, while doubling the overshoot - so this is where the curve stops
  // paying rather than where it flattens.
  Y_CPU_MAX_INSTRUCTIONS_PER_TICK = 16,
};

// Run a single instruction cycle, including fetching and executing the next
// instruction at CS:IP, and handling interrupts.
//
// max_run_cycles is how long the tick may keep running before the host needs
// control back, which for a host driving a machine is how far away the nearest
// device deadline is. A tick runs several instructions back to back where it
// can, and this is what stops it running past the point at which a device
// should have been serviced - so the guest sees an interrupt exactly where it
// would have without the batching.
//
// Zero runs exactly one instruction. That is what a host stepping the machine
// passes, and what a host that has to see every instruction boundary itself
// passes, and it is the behaviour this had before there were runs at all.
Y_PUBLIC YCPUTickResult YCPUTick(YCPUState* cpu, uint16_t max_run_cycles);

#endif  // YAX86_CPU_PUBLIC_H


// ==============================================================================
// src/cpu/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/cpu/types.h start
// ==============================================================================

#line 1 "./src/cpu/types.h"
#ifndef YAX86_CPU_TYPES_H
#define YAX86_CPU_TYPES_H

#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Data width helpers.

// Data widths supported by the 8086 CPU.
typedef enum YCPUWidth {
  Y_CPU_BYTE = 0,
  Y_CPU_WORD,
} YCPUWidth;

enum {
  // Number of data width types.
  Y_CPU_NUM_WIDTHS = Y_CPU_WORD + 1,
};

enum {
  // How many bytes a segment addresses. An offset is 16 bits and wraps within
  // its segment where a linear address does not, so this is where anything
  // walking memory through an offset has to stop.
  Y_CPU_SEGMENT_SIZE = 0x10000,
};

// Bitmask to extract the sign bit of a value.
Y_MODULE_PRIVATE_HEADER const uint32_t YCPUSignBit[Y_CPU_NUM_WIDTHS] = {
    1 << 7,   // Y_CPU_BYTE
    1 << 15,  // Y_CPU_WORD
};

// Maximum unsigned value for each data width.
Y_MODULE_PRIVATE_HEADER const uint32_t YCPUMaxValue[Y_CPU_NUM_WIDTHS] = {
    0xFF,   // Y_CPU_BYTE
    0xFFFF  // Y_CPU_WORD
};

// Maximum signed value for each data width.
Y_MODULE_PRIVATE_HEADER const int32_t YCPUMaxSignedValue[Y_CPU_NUM_WIDTHS] = {
    0x7F,   // Y_CPU_BYTE
    0x7FFF  // Y_CPU_WORD
};

// Minimum signed value for each data width.
Y_MODULE_PRIVATE_HEADER const int32_t YCPUMinSignedValue[Y_CPU_NUM_WIDTHS] = {
    -0x80,   // Y_CPU_BYTE
    -0x8000  // Y_CPU_WORD
};

// Number of bytes in each data width.
Y_MODULE_PRIVATE_HEADER const uint8_t YCPUNumBytes[Y_CPU_NUM_WIDTHS] = {
    1,  // Y_CPU_BYTE
    2,  // Y_CPU_WORD
};

// Number of bits in each data width.
Y_MODULE_PRIVATE_HEADER const uint8_t YCPUNumBits[Y_CPU_NUM_WIDTHS] = {
    8,   // Y_CPU_BYTE
    16,  // Y_CPU_WORD
};

// Operand types.

// The address of a register operand.
typedef struct YCPURegisterAddress {
  // Register index. A uint8_t so the layout does not depend on
  // -fshort-enums.
  uint8_t register_index;
  // Byte offset within the register; only relevant for byte-sized operands.
  // 0 for low byte (AL, CL, DL, BL), 8 for high byte (AH, CH, DH, BH).
  uint8_t byte_offset;
} YCPURegisterAddress;

// The address of a memory operand.
typedef struct YCPUMemoryAddress {
  // Segment register. A uint8_t because YCPUApplySegmentOverride() writes it
  // through a pointer, and an enum's width is the target's business - one byte
  // under -fshort-enums and four otherwise.
  uint8_t segment_register_index;
  // Effective address offset.
  uint16_t offset;
} YCPUMemoryAddress;

// Whether the operand is a register or memory operand.
typedef enum YCPUOperandAddressType {
  Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER = 0,
  Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
} YCPUOperandAddressType;

enum {
  // Number of operand address types.
  Y_CPU_NUM_OPERAND_ADDRESS_TYPES = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY + 1,
};

// Where an operand lives.
//
// Four bytes, which is the largest a composite AAPCS returns in a register
// rather than through memory.
typedef struct YCPUOperandAddress {
  // Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER or Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY. A
  // uint8_t rather than the enum to ensure the four byte layout.
  uint8_t type;
  // The register the operand is in, or the segment register it is addressed
  // through.
  uint8_t register_index;
  // The byte within that register - 0 for AL, 8 for AH - or the effective
  // address offset within that segment.
  uint16_t offset;
} YCPUOperandAddress;

// The value of an operand.
//
// A number rather than a struct, because a struct here is a struct on the
// stack: an operand value is produced and consumed on the hottest path in the
// emulator, and one that does not fit in a register travels through memory
// every time it is passed, returned or assigned.
//
// A byte-wide value is held in the low byte with the high byte zero. Every
// path that produces one truncates to a byte, which is what lets a consumer
// widen it by doing nothing. What the type no longer carries is the width, and
// the width is what sign extension needs - so that is passed explicitly, from
// the opcode table entry the caller already has.
typedef uint16_t YCPUOperandValue;

// An operand.
typedef struct YCPUOperand {
  // Address of the operand.
  YCPUOperandAddress address;
  // Value of the operand.
  YCPUOperandValue value;
} YCPUOperand;

// Instruction types.

struct YCPUOpcodeMetadata;

// Context during instruction execution.
typedef struct {
  YCPUState* cpu;
  const YCPUInstruction* instruction;
  const struct YCPUOpcodeMetadata* metadata;
} YCPUInstructionContext;

// Handler function for an opcode.
typedef YCPUInstructionResult (*YCPUOpcodeHandler)(
    const YCPUInstructionContext* context);

// An entry in the opcode lookup table.
typedef struct YCPUOpcodeMetadata {
  // What the instruction costs before the effective address computation and
  // its time on the data bus - see cycles.c for how the three fit together.
  uint8_t base_cycles;

  // Instruction has ModR/M byte.
  bool has_modrm;

  // Number of immediate data bytes: 0, 1, 2, or 4.
  uint8_t immediate_size;

  // Width of the instruction's operands.
  YCPUWidth width : 8;

  // Handler function.
  YCPUOpcodeHandler handler;
} YCPUOpcodeMetadata;

#endif  // YAX86_CPU_TYPES_H


// ==============================================================================
// src/cpu/types.h end
// ==============================================================================

// ==============================================================================
// src/cpu/cycles.h start
// ==============================================================================

#line 1 "./src/cpu/cycles.h"
#ifndef YAX86_CPU_CYCLES_H
#define YAX86_CPU_CYCLES_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

enum {
  // What a tick costs while the CPU is halted. It is waiting for an interrupt
  // rather than stopped, so time still passes - and the timer that will wake
  // it is driven from that time.
  Y_CPU_HALTED_CYCLES = 4,
  // Extra cycles a taken jump costs, for the prefetch queue it throws away.
  Y_CPU_JUMP_TAKEN_CYCLES = 12,
  // Extra cycles a shift or rotate costs for each bit it moves, when the count
  // comes from CL rather than being 1.
  Y_CPU_SHIFT_CYCLES_PER_BIT = 4,
  // Cycles per byte transferred over the data bus. The 8088's bus is 8 bits
  // wide, so a word costs twice a byte.
  Y_CPU_BUS_CYCLES_PER_BYTE = 4,
};

// Cycles to compute the effective address of a ModR/M memory operand.
Y_MODULE_PRIVATE uint8_t
YCPUGetEffectiveAddressCycles(const YCPUInstruction* instruction);

// Charge the instruction currently executing for time on the data bus.
Y_MODULE_PRIVATE void YCPUAddBusCycles(YCPUState* cpu, uint8_t num_bytes);

#endif  // YAX86_CPU_CYCLES_H


// ==============================================================================
// src/cpu/cycles.h end
// ==============================================================================

// ==============================================================================
// src/cpu/operands.h start
// ==============================================================================

#line 1 "./src/cpu/operands.h"
#ifndef YAX86_CPU_OPERANDS_H
#define YAX86_CPU_OPERANDS_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// Narrow a computed result to what an operand of the given width holds.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUToOperandValue(YCPUWidth width, uint32_t raw_value);

// Helper function to zero-extend YCPUOperandValue to a 32-bit value. This makes
// it simpler to do direct arithmetic without worrying about overflow.
Y_MODULE_PRIVATE uint32_t YCPUFromOperandValue(YCPUOperandValue value);

// Helper function to sign-extend YCPUOperandValue to a 32-bit value. This makes
// it simpler to do direct arithmetic without worrying about overflow.
Y_MODULE_PRIVATE int32_t
YCPUFromSignedOperandValue(YCPUWidth width, YCPUOperandValue value);

// Helper function to extract a zero-extended value from an operand.
Y_MODULE_PRIVATE uint32_t YCPUFromOperand(const YCPUOperand* operand);

// Helper function to extract a sign-extended value from an operand.
Y_MODULE_PRIVATE int32_t
YCPUFromSignedOperand(YCPUWidth width, const YCPUOperand* operand);

// Computes the raw address a segment register and an offset address.
Y_MODULE_PRIVATE uint32_t YCPUToRawAddress(
    const YCPUState* cpu, uint8_t segment_register_index, uint16_t offset);

// Read a byte from memory as a uint8_t.
Y_MODULE_PRIVATE uint8_t
YCPUReadRawMemoryByte(YCPUState* cpu, uint32_t raw_address);

// Read a word from memory as a uint16_t.
Y_MODULE_PRIVATE uint16_t
YCPUReadRawMemoryWord(YCPUState* cpu, uint32_t raw_address);

// Read a byte from memory as a YCPUOperandValue.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadMemoryOperandByte(YCPUState* cpu, const YCPUOperandAddress* address);

// Read a word from memory as a YCPUOperandValue.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadMemoryOperandWord(YCPUState* cpu, const YCPUOperandAddress* address);

// Read a byte from a register as a YCPUOperandValue.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadRegisterOperandByte(YCPUState* cpu, const YCPUOperandAddress* address);

// Read a word from a register as a YCPUOperandValue.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadRegisterOperandWord(YCPUState* cpu, const YCPUOperandAddress* address);

// Write a byte as uint8_t to memory.
Y_MODULE_PRIVATE void YCPUWriteRawMemoryByte(
    YCPUState* cpu, uint32_t address, uint8_t value);

// Write a byte to memory.
Y_MODULE_PRIVATE void YCPUWriteMemoryOperandByte(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value);

// Write a word to memory.
Y_MODULE_PRIVATE void YCPUWriteMemoryOperandWord(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value);

// Write a byte to a register.
Y_MODULE_PRIVATE void YCPUWriteRegisterOperandByte(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value);

// Write a word to a register.
Y_MODULE_PRIVATE void YCPUWriteRegisterOperandWord(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value);

// Add an 8-bit signed relative offset to a 16-bit unsigned base address.
Y_MODULE_PRIVATE uint16_t
YCPUAddSignedOffsetByte(uint16_t base, uint8_t raw_offset);

// Add a 16-bit signed relative offset to a 16-bit unsigned base address.
Y_MODULE_PRIVATE uint16_t
YCPUAddSignedOffsetWord(uint16_t base, uint16_t raw_offset);

// Get the register operand for a byte instruction based on the ModR/M byte's
// reg or R/M field.
Y_MODULE_PRIVATE YCPURegisterAddress
YCPUGetRegisterAddressByte(YCPUState* cpu, uint8_t reg_or_rm);

// Get the register operand for a word instruction based on the ModR/M byte's
// reg or R/M field.
Y_MODULE_PRIVATE YCPURegisterAddress
YCPUGetRegisterAddressWord(YCPUState* cpu, uint8_t reg_or_rm);

// Replace a segment register index with whatever the instruction's segment
// override prefix names, if it carries one.
Y_MODULE_PRIVATE void YCPUApplySegmentOverride(
    const YCPUInstruction* instruction, uint8_t* segment_register_index);

// Compute the memory address for an instruction.
Y_MODULE_PRIVATE YCPUMemoryAddress
YCPUGetMemoryOperandAddress(YCPUState* cpu, const YCPUInstruction* instruction);

// Get a register or memory operand address based on the ModR/M byte and
// displacement, without reading the value currently there.
Y_MODULE_PRIVATE YCPUOperandAddress
YCPUGetRegisterOrMemoryOperandAddress(const YCPUInstructionContext* ctx);

// Read an 8-bit immediate value.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadImmediateOperandByte(const YCPUInstruction* instruction);

// Read a 16-bit immediate value.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadImmediateOperandWord(const YCPUInstruction* instruction);

// Read a value from an operand address.
Y_MODULE_PRIVATE YCPUOperandValue YCPUReadOperandValue(
    const YCPUInstructionContext* ctx, const YCPUOperandAddress* address);

// Get a register or memory operand for an instruction based on the ModR/M
// byte and displacement.
Y_MODULE_PRIVATE void YCPUReadRegisterOrMemoryOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand);

// Get a register operand for an instruction.
Y_MODULE_PRIVATE void YCPUReadRegisterOperandForRegisterIndex(
    const YCPUInstructionContext* ctx, YCPURegisterIndex register_index,
    YCPUOperand* operand);

// Get a register operand for an instruction from the REG field of the Mod/RM
// byte.
Y_MODULE_PRIVATE void YCPUReadRegisterOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand);

// Get a segment register operand for an instruction from the REG field of the
// Mod/RM byte.
Y_MODULE_PRIVATE void YCPUReadSegmentRegisterOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand);

// Write a value to a register or memory operand address.
Y_MODULE_PRIVATE void YCPUWriteOperandAddress(
    const YCPUInstructionContext* ctx, const YCPUOperandAddress* address,
    uint32_t raw_value);

// Write a value to a register or memory operand.
Y_MODULE_PRIVATE void YCPUWriteOperand(
    const YCPUInstructionContext* ctx, const YCPUOperand* operand,
    uint32_t raw_value);

// Read an immediate value from the instruction.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadImmediate(const YCPUInstructionContext* ctx);

#endif  // YAX86_CPU_OPERANDS_H


// ==============================================================================
// src/cpu/operands.h end
// ==============================================================================

// ==============================================================================
// src/cpu/cycles.c start
// ==============================================================================

#line 1 "./src/cpu/cycles.c"
#ifndef YAX86_IMPLEMENTATION
#include "cycles.h"

#include "../util/common.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Instruction timing
// ============================================================================
//
// How long each instruction takes, in 4.77MHz CPU clock cycles. This is not a
// cycle-accurate model - it does not track the prefetch queue, and it charges
// a whole instruction at its boundary rather than spreading it over the bus
// cycles it really occupies. What it does give is a clock that means
// something: the ratio between time spent executing and time measured by the
// PIT comes out right, so a guest that calibrates a delay loop against the
// timer arrives at roughly the figure real hardware would.
//
// The cost of an instruction is built from three parts.
//
// 1. A base cost per opcode, which is YCPUOpcodeMetadata.base_cycles in
//    YCPUOpcodeTable.c. These are the published 8086 figures for the register
//    form, less the time the figure already accounts for on the bus, which is
//    charged separately in part 3. Where the published figure covers an
//    instruction that necessarily touches memory - the stack instructions, the
//    string instructions, the software interrupts - that bus time has been
//    taken back out, so that charging the traffic separately does not count it
//    twice. PUSH ES is 2 rather than 10 for this reason, and POP r16 is 0.
//
// 2. The effective address calculation, for instructions that address memory
//    through a ModR/M byte.
//
// 3. Four cycles for every byte the instruction moves over the data bus. The
//    8088 has an 8-bit bus, so a word costs twice a byte - this is the main
//    reason it is slower than the 8086 it shares timings with, and it is the
//    dominant term for most instructions. Charging it from the actual
//    accesses rather than from a table means the string instructions, the
//    stack and the interrupt sequence all cost what their traffic costs,
//    including when REP runs them many times over.
//
// Instructions whose cost depends on more than their operands - a conditional
// jump that is taken, a shift by a count in CL, a multiply or divide - add the
// difference themselves through YCPUAddCycles().

// Cycles to compute an effective address, by addressing mode. The 8086 pays
// for each component it has to add together.
enum {
  // A displacement on its own.
  Y_CPU_EA_CYCLES_DISPLACEMENT_ONLY = 6,
  // A single base or index register.
  Y_CPU_EA_CYCLES_BASE_OR_INDEX = 5,
  // A base or index register plus a displacement.
  Y_CPU_EA_CYCLES_BASE_OR_INDEX_AND_DISPLACEMENT = 9,
  // Base plus index. BP+DI and BX+SI cost one cycle less than the other two
  // pairings, which this does not distinguish.
  Y_CPU_EA_CYCLES_BASE_AND_INDEX = 8,
  // Base plus index plus a displacement.
  Y_CPU_EA_CYCLES_BASE_AND_INDEX_AND_DISPLACEMENT = 12,
  // A segment override prefix costs two more, since the address has to be
  // formed against a different segment base.
  Y_CPU_EA_CYCLES_SEGMENT_OVERRIDE = 2,
};

// Cycles to compute the effective address of a ModR/M memory operand.
Y_MODULE_PRIVATE uint8_t
YCPUGetEffectiveAddressCycles(const YCPUInstruction* instruction) {
  if (!instruction->has_mod_rm || instruction->mod_rm.mod == 0x03) {
    // A register operand needs no address computed.
    return 0;
  }

  const uint8_t mod = instruction->mod_rm.mod;
  const uint8_t rm = instruction->mod_rm.rm;
  const bool has_displacement =
      mod == 0x01 || mod == 0x02 || (mod == 0x00 && rm == 0x06);
  // R/M values 0 through 3 pair a base register with an index register. The
  // rest name a single register, except for the direct address at mod 0, rm 6.
  const bool has_base_and_index = rm <= 0x03;
  const bool is_direct_address = mod == 0x00 && rm == 0x06;

  uint8_t cycles;
  if (is_direct_address) {
    cycles = Y_CPU_EA_CYCLES_DISPLACEMENT_ONLY;
  } else if (has_base_and_index) {
    cycles = has_displacement ? Y_CPU_EA_CYCLES_BASE_AND_INDEX_AND_DISPLACEMENT
                              : Y_CPU_EA_CYCLES_BASE_AND_INDEX;
  } else {
    cycles = has_displacement ? Y_CPU_EA_CYCLES_BASE_OR_INDEX_AND_DISPLACEMENT
                              : Y_CPU_EA_CYCLES_BASE_OR_INDEX;
  }

  // Charged once for an instruction that carries a segment override, rather
  // than per override prefix. Only one can take effect, and real code never
  // emits more than one.
  if (instruction->segment_override != Y_CPU_NO_SEGMENT_OVERRIDE) {
    cycles += Y_CPU_EA_CYCLES_SEGMENT_OVERRIDE;
  }
  return cycles;
}

Y_MODULE_PRIVATE void YCPUAddBusCycles(YCPUState* cpu, uint8_t num_bytes) {
  cpu->pending_cycles += (uint16_t)num_bytes * Y_CPU_BUS_CYCLES_PER_BYTE;
}

Y_PUBLIC void YCPUAddCycles(YCPUState* cpu, uint16_t cycles) {
  cpu->pending_cycles += cycles;
}


// ==============================================================================
// src/cpu/cycles.c end
// ==============================================================================

// ==============================================================================
// src/cpu/operands.c start
// ==============================================================================

#line 1 "./src/cpu/operands.c"
#ifndef YAX86_IMPLEMENTATION
#include "operands.h"

#include "../util/common.h"
#include "cycles.h"
#endif  // YAX86_IMPLEMENTATION

// What the unreachable arm of a width dispatch returns.
//
// YCPUWidth names two values and every switch below handles both, so nothing
// reaches these arms. They exist because C does not promise an enum object
// holds only the values its enumerators name, and a function with a return
// type has to return something.
//
// All ones in the low byte is what an 8086 reads from an address nothing
// drives, and it is a legal value at either width. 0xFFFF is not: at Y_CPU_BYTE
// it would break the zero high byte a byte-wide value is promised to have,
// which is the invariant YCPUFromOperandValue() widens by doing nothing.
enum { Y_CPU_UNREACHABLE_OPERAND_VALUE = 0xFF };

// Narrow a computed result to what an operand of the given width holds. A
// byte keeps a zero high byte, which is what makes widening it again free.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUToOperandValue(YCPUWidth width, uint32_t raw_value) {
  return (YCPUOperandValue)(raw_value & YCPUMaxValue[width]);
}

// Widen an operand value for arithmetic, which is done in 32 bits so that
// nothing overflows on the way. A byte value already has a zero high byte, so
// this is where that invariant is spent rather than a width being consulted.
Y_MODULE_PRIVATE uint32_t YCPUFromOperandValue(YCPUOperandValue value) {
  return value;
}

// The same, sign-extended. This one does need the width, since which bit is
// the sign bit is exactly what the value no longer says.
//
// One test, byte or not, rather than a switch with a return after it for the
// values outside the enum that a width can still hold. That return is a third
// path, and once this is inlined GCC carries it on to the caller's next
// lookup in a table indexed by width - YCPUMaxSignedValue in IMUL and
// YCPUMulDivResultHighHalfAddress in IDIV - and warns that the index can be 2.
Y_MODULE_PRIVATE int32_t
YCPUFromSignedOperandValue(YCPUWidth width, YCPUOperandValue value) {
  if (width == Y_CPU_BYTE) {
    return (int32_t)((int8_t)value);
  }
  return (int32_t)((int16_t)value);
}

// Helper function to extract a zero-extended value from an operand.
Y_MODULE_PRIVATE uint32_t YCPUFromOperand(const YCPUOperand* operand) {
  return YCPUFromOperandValue(operand->value);
}

// Helper function to extract a sign-extended value from an operand.
Y_MODULE_PRIVATE int32_t
YCPUFromSignedOperand(YCPUWidth width, const YCPUOperand* operand) {
  return YCPUFromSignedOperandValue(width, operand->value);
}

enum {
  // The address bus is 20 bits wide. A segment base plus an offset can sum to
  // as much as 0x10FFEF, and the carry out of bit 19 goes nowhere, so an
  // address past the top of memory wraps around to the bottom.
  Y_CPU_PHYSICAL_ADDRESS_MASK = 0xFFFFF,
};

// Computes the raw address a segment register and an offset address.
Y_MODULE_PRIVATE uint32_t YCPUToRawAddress(
    const YCPUState* cpu, uint8_t segment_register_index, uint16_t offset) {
  uint16_t segment = cpu->registers[segment_register_index];
  return ((((uint32_t)segment) << 4) + (uint32_t)offset) &
         Y_CPU_PHYSICAL_ADDRESS_MASK;
}

// Read a byte from memory as a uint8_t.
Y_MODULE_PRIVATE uint8_t
YCPUReadRawMemoryByte(YCPUState* cpu, uint32_t raw_address) {
  // Guest RAM, where nearly every operand lands, is reached by indexing. The
  // call below ends up indexing an array too, having gone through a function
  // pointer, the host's context and a memory map lookup to arrive at it.
  if (raw_address < cpu->direct_data_window.end) {
    return cpu->direct_data_window.data[raw_address];
  }
  return cpu->config.read_memory_byte
             ? cpu->config.read_memory_byte(cpu, raw_address)
             : 0xFF;
}

// Read a word from memory as a uint16_t.
Y_MODULE_PRIVATE uint16_t
YCPUReadRawMemoryWord(YCPUState* cpu, uint32_t raw_address) {
  uint8_t low_byte_value = YCPUReadRawMemoryByte(cpu, raw_address);
  uint8_t high_byte_value = YCPUReadRawMemoryByte(cpu, raw_address + 1);
  return (((uint16_t)high_byte_value) << 8) | (uint16_t)low_byte_value;
}

// Read a byte from memory to a YCPUOperandValue.
Y_MODULE_PRIVATE Y_HOT YCPUOperandValue
YCPUReadMemoryOperandByte(YCPUState* cpu, const YCPUOperandAddress* address) {
  YCPUAddBusCycles(cpu, 1);
  return YCPUReadRawMemoryByte(
      cpu, YCPUToRawAddress(cpu, address->register_index, address->offset));
}

// Read a word from memory to a YCPUOperandValue.
Y_MODULE_PRIVATE Y_HOT YCPUOperandValue
YCPUReadMemoryOperandWord(YCPUState* cpu, const YCPUOperandAddress* address) {
  YCPUAddBusCycles(cpu, 2);
  // The offset is 16 bits wide and wraps within the segment, so the high byte
  // of a word at offset 0xFFFF comes from offset 0 of the same segment rather
  // than from the paragraph above it.
  const uint8_t segment = address->register_index;
  uint8_t low_byte_value = YCPUReadRawMemoryByte(
      cpu, YCPUToRawAddress(cpu, segment, address->offset));
  uint8_t high_byte_value = YCPUReadRawMemoryByte(
      cpu, YCPUToRawAddress(cpu, segment, (uint16_t)(address->offset + 1)));
  return (YCPUOperandValue)((((uint16_t)high_byte_value) << 8) |
                            (uint16_t)low_byte_value);
}

// Read a memory operand of the given width to a YCPUOperandValue.
Y_FILE_PRIVATE YCPUOperandValue YCPUReadMemoryOperandValue(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUWidth width) {
  switch (width) {
    case Y_CPU_BYTE:
      return YCPUReadMemoryOperandByte(cpu, address);
    case Y_CPU_WORD:
      return YCPUReadMemoryOperandWord(cpu, address);
  }
  // Should never reach here, but return a default value to avoid warnings.
  return Y_CPU_UNREACHABLE_OPERAND_VALUE;
}

// Read a byte from a register to a YCPUOperandValue.
Y_MODULE_PRIVATE Y_HOT YCPUOperandValue
YCPUReadRegisterOperandByte(YCPUState* cpu, const YCPUOperandAddress* address) {
  // Truncated to a byte, which is the invariant every consumer widens by
  // doing nothing: AH and the high half of a word register live in the bits
  // this drops.
  return (uint8_t)(cpu->registers[address->register_index] >> address->offset);
}

// Read a word from a register to a YCPUOperandValue.
Y_MODULE_PRIVATE Y_HOT YCPUOperandValue
YCPUReadRegisterOperandWord(YCPUState* cpu, const YCPUOperandAddress* address) {
  return cpu->registers[address->register_index];
}

// Read a register operand of the given width to a YCPUOperandValue.
Y_FILE_PRIVATE YCPUOperandValue YCPUReadRegisterOperandValue(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUWidth width) {
  switch (width) {
    case Y_CPU_BYTE:
      return YCPUReadRegisterOperandByte(cpu, address);
    case Y_CPU_WORD:
      return YCPUReadRegisterOperandWord(cpu, address);
  }
  // Should never reach here, but return a default value to avoid warnings.
  return Y_CPU_UNREACHABLE_OPERAND_VALUE;
}

// Write a byte as uint8_t to memory.
Y_MODULE_PRIVATE void YCPUWriteRawMemoryByte(
    YCPUState* cpu, uint32_t address, uint8_t value) {
  // Every write the CPU makes comes through here - operands, stack pushes and
  // the interrupt vector alike - which is what lets a host report only the
  // writes it makes some other way.
  YCPUNotifyMemoryWrite(cpu, address);
  if (address < cpu->direct_data_window.end) {
    cpu->direct_data_window.data[address] = value;
    return;
  }
  if (!cpu->config.write_memory_byte) {
    return;
  }
  cpu->config.write_memory_byte(cpu, address, value);
}

// Write a byte to memory.
Y_MODULE_PRIVATE Y_HOT void YCPUWriteMemoryOperandByte(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value) {
  YCPUAddBusCycles(cpu, 1);
  YCPUWriteRawMemoryByte(
      cpu, YCPUToRawAddress(cpu, address->register_index, address->offset),
      (uint8_t)value);
}

// Write a word to memory.
Y_MODULE_PRIVATE Y_HOT void YCPUWriteMemoryOperandWord(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value) {
  YCPUAddBusCycles(cpu, 2);
  // See YCPUReadMemoryOperandWord() for why the high byte's offset wraps within
  // the segment.
  const uint8_t segment = address->register_index;
  YCPUWriteRawMemoryByte(
      cpu, YCPUToRawAddress(cpu, segment, address->offset), (uint8_t)value);
  YCPUWriteRawMemoryByte(
      cpu, YCPUToRawAddress(cpu, segment, (uint16_t)(address->offset + 1)),
      (uint8_t)(value >> 8));
}

// Write a memory operand of the given width.
Y_FILE_PRIVATE void YCPUWriteMemoryOperand(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value,
    YCPUWidth width) {
  switch (width) {
    case Y_CPU_BYTE:
      YCPUWriteMemoryOperandByte(cpu, address, value);
      return;
    case Y_CPU_WORD:
      YCPUWriteMemoryOperandWord(cpu, address, value);
      return;
  }
  // Should never reach here. Writing nothing is the safe default.
}

// Write a byte to a register.
Y_MODULE_PRIVATE Y_HOT void YCPUWriteRegisterOperandByte(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value) {
  const uint16_t updated_byte = ((uint16_t)(uint8_t)value) << address->offset;
  const uint16_t other_byte = cpu->registers[address->register_index] &
                              (((uint16_t)0xFF) << (8 - address->offset));
  cpu->registers[address->register_index] = other_byte | updated_byte;
}

// Write a word to a register.
Y_MODULE_PRIVATE Y_HOT void YCPUWriteRegisterOperandWord(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value) {
  cpu->registers[address->register_index] = value;
}

// Write a register operand of the given width.
Y_FILE_PRIVATE void YCPUWriteRegisterOperand(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value,
    YCPUWidth width) {
  switch (width) {
    case Y_CPU_BYTE:
      YCPUWriteRegisterOperandByte(cpu, address, value);
      return;
    case Y_CPU_WORD:
      YCPUWriteRegisterOperandWord(cpu, address, value);
      return;
  }
  // Should never reach here. Writing nothing is the safe default.
}

// Add an 8-bit signed relative offset to a 16-bit unsigned base address.
Y_MODULE_PRIVATE uint16_t
YCPUAddSignedOffsetByte(uint16_t base, uint8_t raw_offset) {
  // Sign-extend the offset to 32 bits
  int32_t signed_offset = (int32_t)((int8_t)raw_offset);
  // Zero-extend base to 32 bits
  int32_t signed_base = (int32_t)base;
  // Add the two 32-bit signed values then truncate back down to 16-bit unsigned
  return (uint16_t)(signed_base + signed_offset);
}

// Add a 16-bit signed relative offset to a 16-bit unsigned base address.
Y_MODULE_PRIVATE uint16_t
YCPUAddSignedOffsetWord(uint16_t base, uint16_t raw_offset) {
  // Sign-extend the offset to 32 bits
  int32_t signed_offset = (int32_t)((int16_t)raw_offset);
  // Zero-extend base to 32 bits
  int32_t signed_base = (int32_t)base;
  // Add the two 32-bit signed values then truncate back down to 16-bit unsigned
  return (uint16_t)(signed_base + signed_offset);
}

// Get the register operand for a byte instruction based on the ModR/M byte's
// reg or R/M field.
Y_MODULE_PRIVATE Y_HOT YCPURegisterAddress
YCPUGetRegisterAddressByte(Y_UNUSED YCPUState* cpu, uint8_t reg_or_rm) {
  YCPURegisterAddress address;
  if (reg_or_rm < 4) {
    // AL, CL, DL, BL
    address.register_index = reg_or_rm;
    address.byte_offset = 0;
  } else {
    // AH, CH, DH, BH
    address.register_index = reg_or_rm - 4;
    address.byte_offset = 8;
  }
  return address;
}

// Get the register operand for a word instruction based on the ModR/M byte's
// reg or R/M field.
Y_MODULE_PRIVATE YCPURegisterAddress
YCPUGetRegisterAddressWord(Y_UNUSED YCPUState* cpu, uint8_t reg_or_rm) {
  const YCPURegisterAddress address = {
      .register_index = reg_or_rm, .byte_offset = 0};
  return address;
}

// Get the register operand of the given width from the ModR/M byte's reg or
// R/M field.
Y_FILE_PRIVATE YCPURegisterAddress
YCPUGetRegisterAddress(YCPUState* cpu, uint8_t reg_or_rm, YCPUWidth width) {
  switch (width) {
    case Y_CPU_BYTE:
      return YCPUGetRegisterAddressByte(cpu, reg_or_rm);
    case Y_CPU_WORD:
      return YCPUGetRegisterAddressWord(cpu, reg_or_rm);
  }
  // Should never reach here. AL is in range for both widths, so a caller that
  // somehow got here names a real register rather than running off the array.
  const YCPURegisterAddress fallback = {
      .register_index = Y_CPU_AX, .byte_offset = 0};
  return fallback;
}

// Replace a segment register index with whatever the instruction's segment
// override prefix names, if it carries one.
Y_MODULE_PRIVATE void YCPUApplySegmentOverride(
    const YCPUInstruction* instruction, uint8_t* segment_register_index) {
  if (instruction->segment_override != Y_CPU_NO_SEGMENT_OVERRIDE) {
    *segment_register_index = instruction->segment_override;
  }
}

// Compute the memory address for an instruction.
Y_MODULE_PRIVATE Y_HOT YCPUMemoryAddress YCPUGetMemoryOperandAddress(
    YCPUState* cpu, const YCPUInstruction* instruction) {
  YCPUMemoryAddress address;
  uint8_t mod = instruction->mod_rm.mod;
  uint8_t rm = instruction->mod_rm.rm;
  switch (rm) {
    case 0:  // [BX + SI]
      address.offset = cpu->registers[Y_CPU_BX] + cpu->registers[Y_CPU_SI];
      address.segment_register_index = Y_CPU_DS;
      break;
    case 1:  // [BX + DI]
      address.offset = cpu->registers[Y_CPU_BX] + cpu->registers[Y_CPU_DI];
      address.segment_register_index = Y_CPU_DS;
      break;
    case 2:  // [BP + SI]
      address.offset = cpu->registers[Y_CPU_BP] + cpu->registers[Y_CPU_SI];
      address.segment_register_index = Y_CPU_SS;
      break;
    case 3:  // [BP + DI]
      address.offset = cpu->registers[Y_CPU_BP] + cpu->registers[Y_CPU_DI];
      address.segment_register_index = Y_CPU_SS;
      break;
    case 4:  // [SI]
      address.offset = cpu->registers[Y_CPU_SI];
      address.segment_register_index = Y_CPU_DS;
      break;
    case 5:  // [DI]
      address.offset = cpu->registers[Y_CPU_DI];
      address.segment_register_index = Y_CPU_DS;
      break;
    case 6:
      if (mod == 0) {
        // Direct memory address with 16-bit displacement
        address.offset = 0;
        address.segment_register_index = Y_CPU_DS;
      } else {
        // [BP]
        address.offset = cpu->registers[Y_CPU_BP];
        address.segment_register_index = Y_CPU_SS;
      }
      break;
    case 7:  // [BX]
      address.offset = cpu->registers[Y_CPU_BX];
      address.segment_register_index = Y_CPU_DS;
      break;
    default:
      // Not possible as RM field is 3 bits (0-7).
      address.offset = 0xFFFF;
      address.segment_register_index = Y_CPU_DS;  // Invalid RM field
      break;
  }

  // Apply segment override prefixes if present
  YCPUApplySegmentOverride(instruction, &address.segment_register_index);

  // Add displacement if present
  switch (instruction->displacement_size) {
    case 1: {
      uint8_t raw_displacement = instruction->displacement[0];
      address.offset =
          YCPUAddSignedOffsetByte(address.offset, raw_displacement);
      break;
    }
    case 2: {
      // Concatenate the two displacement bytes as an unsigned 16-bit integer
      uint16_t raw_displacement =
          ((uint16_t)instruction->displacement[0]) |
          (((uint16_t)instruction->displacement[1]) << 8);
      address.offset =
          YCPUAddSignedOffsetWord(address.offset, raw_displacement);
      break;
    }
    default:
      // No displacement
      break;
  }

  return address;
}

// Get a register or memory operand address based on the ModR/M byte and
// displacement, without reading the value currently there.
//
// An instruction that overwrites its destination completely - as opposed to a
// read-modify-write - resolves the address with this and stores through
// YCPUWriteOperandAddress(). Reading the destination on the way is not free: it
// charges the bus cycles of a memory access the 8088 never performs.
//
// Always inlined. With more than one caller, -Os and -O2 emit it out of line,
// which puts a call and its register shuffling on the hottest path in the
// emulator - 3.6% at -O2.
Y_MODULE_PRIVATE Y_ALWAYS_INLINE YCPUOperandAddress
YCPUGetRegisterOrMemoryOperandAddress(const YCPUInstructionContext* ctx) {
  YCPUState* cpu = ctx->cpu;
  const YCPUInstruction* instruction = ctx->instruction;
  YCPUOperandAddress address;
  uint8_t mod = instruction->mod_rm.mod;
  uint8_t rm = instruction->mod_rm.rm;
  if (mod == 3) {
    // Register operand
    const YCPURegisterAddress register_address =
        YCPUGetRegisterAddress(cpu, rm, ctx->metadata->width);
    address.type = Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER;
    address.register_index = register_address.register_index;
    address.offset = register_address.byte_offset;
  } else {
    // Memory operand
    const YCPUMemoryAddress memory_address =
        YCPUGetMemoryOperandAddress(cpu, instruction);
    address.type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY;
    address.register_index = (uint8_t)memory_address.segment_register_index;
    address.offset = memory_address.offset;
  }
  return address;
}

// Read an 8-bit immediate value.
Y_MODULE_PRIVATE Y_HOT YCPUOperandValue
YCPUReadImmediateOperandByte(const YCPUInstruction* instruction) {
  return instruction->immediate[0];
}

// Read a 16-bit immediate value.
Y_MODULE_PRIVATE Y_HOT YCPUOperandValue
YCPUReadImmediateOperandWord(const YCPUInstruction* instruction) {
  return (YCPUOperandValue)(((uint16_t)instruction->immediate[0]) |
                            (((uint16_t)instruction->immediate[1]) << 8));
}

// Read an immediate value of the given width.
Y_FILE_PRIVATE YCPUOperandValue
YCPUReadImmediateOperand(const YCPUInstruction* instruction, YCPUWidth width) {
  switch (width) {
    case Y_CPU_BYTE:
      return YCPUReadImmediateOperandByte(instruction);
    case Y_CPU_WORD:
      return YCPUReadImmediateOperandWord(instruction);
  }
  // Should never reach here, but return a default value to avoid warnings.
  return Y_CPU_UNREACHABLE_OPERAND_VALUE;
}

// Read a value from an operand address.
//
// Always inlined. Left to itself GCC emits this out of line, in flash, and
// puts a veneer and an XIP fetch on every operand read - 5.4% at -O3.
Y_MODULE_PRIVATE Y_ALWAYS_INLINE YCPUOperandValue YCPUReadOperandValue(
    const YCPUInstructionContext* ctx, const YCPUOperandAddress* address) {
  // Not a switch, unlike the width dispatch it calls into.
  // YCPUOperandAddressType is a plain enum field rather than a bitfield, so
  // most of the values it can hold are ones the enum does not name and the
  // compiler cannot prove a switch's default arm unreachable: the memory path,
  // which is the common one, ends up paying an extra compare and branch for a
  // case that cannot happen. YCPUWidth is a one-bit bitfield, which has no
  // unnamed value to represent, so there the default arm disappears from the
  // output entirely. Switching this one too costs 1.06% at -O3.
  //
  // Narrowing type to a one-bit bitfield to make the switch free was measured
  // and is worse still - YCPUGetRegisterOrMemoryOperandAddress() writes this
  // field for every operand, and a write to a bitfield is a read-modify-write.
  // Widening YCPUOpcodeMetadata.width to a byte would likewise put the width
  // switches in this same position; see core/src/cpu/AGENTS.md before doing
  // either.
  const YCPUWidth width = ctx->metadata->width;
  if (address->type == Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER) {
    return YCPUReadRegisterOperandValue(ctx->cpu, address, width);
  }
  return YCPUReadMemoryOperandValue(ctx->cpu, address, width);
}

// Get a register or memory operand for an instruction based on the ModR/M
// byte and displacement.
Y_MODULE_PRIVATE Y_HOT void YCPUReadRegisterOrMemoryOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand) {
  operand->address = YCPUGetRegisterOrMemoryOperandAddress(ctx);
  operand->value = YCPUReadOperandValue(ctx, &operand->address);
}

// Get a register operand for an instruction.
Y_MODULE_PRIVATE Y_HOT void YCPUReadRegisterOperandForRegisterIndex(
    const YCPUInstructionContext* ctx, YCPURegisterIndex register_index,
    YCPUOperand* operand) {
  const YCPUWidth width = ctx->metadata->width;
  // Do not fold these into an initializer: C would zero the whole struct
  // before a single field of it is written.
  operand->address.type = Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER;
  const YCPURegisterAddress register_address =
      YCPUGetRegisterAddress(ctx->cpu, register_index, width);
  operand->address.register_index = register_address.register_index;
  operand->address.offset = register_address.byte_offset;
  operand->value = YCPUReadOperandValue(ctx, &operand->address);
}

// Get a register operand for an instruction from the REG field of the Mod/RM
// byte.
Y_MODULE_PRIVATE void YCPUReadRegisterOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand) {
  YCPUReadRegisterOperandForRegisterIndex(
      ctx, (YCPURegisterIndex)ctx->instruction->mod_rm.reg, operand);
}

// Get a segment register operand for an instruction from the REG field of the
// Mod/RM byte.
Y_MODULE_PRIVATE void YCPUReadSegmentRegisterOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand) {
  // The segment register field is only two bits wide. The 8086/8088 does not
  // decode the third bit at all, so REG 4 through 7 name the same four
  // registers over again - which is what makes 0x8C and 0x8E accept every REG
  // value. Masking it also keeps the index inside the register array, which
  // REG 4 and above would otherwise run past the end of.
  YCPUReadRegisterOperandForRegisterIndex(
      ctx,
      (YCPURegisterIndex)(Y_CPU_ES + (ctx->instruction->mod_rm.reg & 0x03)),
      operand);
}

// Write a value to a register or memory operand address.
Y_MODULE_PRIVATE Y_HOT void YCPUWriteOperandAddress(
    const YCPUInstructionContext* ctx, const YCPUOperandAddress* address,
    uint32_t raw_value) {
  const YCPUWidth width = ctx->metadata->width;
  const YCPUOperandValue value = YCPUToOperandValue(width, raw_value);
  // See YCPUReadOperandValue() for why this one is not a switch.
  if (address->type == Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER) {
    YCPUWriteRegisterOperand(ctx->cpu, address, value, width);
  } else {
    YCPUWriteMemoryOperand(ctx->cpu, address, value, width);
  }
}

// Write a value to a register or memory operand.
Y_MODULE_PRIVATE void YCPUWriteOperand(
    const YCPUInstructionContext* ctx, const YCPUOperand* operand,
    uint32_t raw_value) {
  YCPUWriteOperandAddress(ctx, &operand->address, raw_value);
}

// Read an immediate value from the instruction.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadImmediate(const YCPUInstructionContext* ctx) {
  return YCPUReadImmediateOperand(ctx->instruction, ctx->metadata->width);
}


// ==============================================================================
// src/cpu/operands.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions.h start
// ==============================================================================

#line 1 "./src/cpu/instructions.h"
#ifndef YAX86_CPU_INSTRUCTIONS_H
#define YAX86_CPU_INSTRUCTIONS_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Helpers - instructions_helpers.h
// ============================================================================

// Set common CPU flags after an instruction. This includes:
// - Zero flag (ZF)
// - Sign flag (SF)
// - Parity Flag (PF)
Y_MODULE_PRIVATE void YCPUSetCommonFlagsAfterInstruction(
    const YCPUInstructionContext* ctx, uint32_t result);

// Apply the bits that are not flags to a value on its way into the flags
// register. POPF, IRET and SAHF all load flags from somewhere the guest
// controls, and none of them can change these bits.
Y_MODULE_PRIVATE uint16_t YCPUToFlagsRegisterValue(uint16_t value);

// Push a value the caller has already worked out onto the stack - a return
// address, the flags, a segment register. Use this wherever the value does not
// depend on the stack pointer.
Y_MODULE_PRIVATE void YCPUPushValue(YCPUState* cpu, YCPUOperandValue value);
// Push a PUSH instruction's source operand onto the stack. The operand is
// taken as of after the stack pointer has moved, rather than as of the start
// of the instruction, which is what makes PUSH SP store the decremented value.
Y_MODULE_PRIVATE void YCPUPushSourceOperand(
    YCPUState* cpu, const YCPUOperand* src);
// Pop a value from the stack.
Y_MODULE_PRIVATE YCPUOperandValue YCPUPop(YCPUState* cpu);

// Dummy instruction for unsupported opcodes.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteNoOp(const YCPUInstructionContext* ctx);
// Handler for the opcode bytes that are prefixes rather than instructions.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInvalidOpcode(const YCPUInstructionContext* ctx);

// ============================================================================
// Opcode table - YCPUOpcodeTable.h
// ============================================================================

// Global opcode metadata lookup table.
#ifndef YAX86_IMPLEMENTATION
extern const YCPUOpcodeMetadata YCPUOpcodeTable[256];
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Move instructions - instructions_mov.h
// ============================================================================

// MOV r/m8, r8
// MOV r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx);
// MOV r8, r/m8
// MOV r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx);
// MOV r/m16, sreg
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveSegmentRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx);
// MOV sreg, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveRegisterOrMemoryToSegmentRegister(
    const YCPUInstructionContext* ctx);
// MOV AX/CX/DX/BX/SP/BP/SI/DI, imm16
// MOV AH/AL/CH/CL/DH/DL/BH/BL, imm8
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveImmediateToRegister(const YCPUInstructionContext* ctx);
// MOV AL, moffs16
// MOV AX, moffs16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveMemoryOffsetToALOrAX(const YCPUInstructionContext* ctx);
// MOV moffs16, AL
// MOV moffs16, AX
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveALOrAXToMemoryOffset(const YCPUInstructionContext* ctx);
// MOV r/m8, imm8
// MOV r/m16, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveImmediateToRegisterOrMemory(const YCPUInstructionContext* ctx);
// XCHG AX, AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteExchangeRegister(const YCPUInstructionContext* ctx);
// XCHG r/m8, r8
// XCHG r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteExchangeRegisterOrMemory(const YCPUInstructionContext* ctx);
// XLAT
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteTranslateByte(const YCPUInstructionContext* ctx);

// ============================================================================
// LEA instructions - instructions_lea.h
// ============================================================================

// LEA r16, m
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoadEffectiveAddress(const YCPUInstructionContext* ctx);
// LES r16, m
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoadESWithPointer(const YCPUInstructionContext* ctx);
// LDS r16, m
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoadDSWithPointer(const YCPUInstructionContext* ctx);

// ============================================================================
// Addition instructions - instructions_add.h
// ============================================================================

// Common logic for ADD instructions
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteAdd(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);
// Common logic for INC instructions
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInc(const YCPUInstructionContext* ctx, YCPUOperand* dest);
// Common logic for ADC instructions
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteAddWithCarry(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);

// ADD r/m8, r8
// ADD r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx);
// ADD r8, r/m8
// ADD r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx);
// ADD AL, imm8
// ADD AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddImmediateToALOrAX(const YCPUInstructionContext* ctx);
// ADC r/m8, r8
// ADC r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddRegisterToRegisterOrMemoryWithCarry(
    const YCPUInstructionContext* ctx);
// ADC r8, r/m8
// ADC r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddRegisterOrMemoryToRegisterWithCarry(
    const YCPUInstructionContext* ctx);
// ADC AL, imm8
// ADC AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddImmediateToALOrAXWithCarry(const YCPUInstructionContext* ctx);
// INC AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteIncRegister(const YCPUInstructionContext* ctx);

// ============================================================================
// Subtraction instructions - instructions_sub.c
// ============================================================================

// Set CPU flags after a SUB, SBB, CMP, or NEG instruction.
Y_MODULE_PRIVATE void YCPUSetFlagsAfterSub(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_borrow);

// Common logic for SUB instructions
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteSub(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);
// Common logic for SBB instructions
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteSubWithBorrow(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);
// Common logic for DEC instructions
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDec(const YCPUInstructionContext* ctx, YCPUOperand* dest);

// SUB r/m8, r8
// SUB r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubRegisterFromRegisterOrMemory(const YCPUInstructionContext* ctx);
// SUB r8, r/m8
// SUB r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubRegisterOrMemoryFromRegister(const YCPUInstructionContext* ctx);
// SUB AL, imm8
// SUB AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubImmediateFromALOrAX(const YCPUInstructionContext* ctx);
// SBB r/m8, r8
// SBB r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubRegisterFromRegisterOrMemoryWithBorrow(
    const YCPUInstructionContext* ctx);
// SBB r8, r/m8
// SBB r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubRegisterOrMemoryFromRegisterWithBorrow(
    const YCPUInstructionContext* ctx);
// SBB AL, imm8
// SBB AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubImmediateFromALOrAXWithBorrow(const YCPUInstructionContext* ctx);
// DEC AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDecRegister(const YCPUInstructionContext* ctx);

// ============================================================================
// Sign extension instructions - instructions_sign_ext.c
// ============================================================================

// CBW
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCbw(const YCPUInstructionContext* ctx);
// CWD
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCwd(const YCPUInstructionContext* ctx);

// ============================================================================
// CMP instructions - instructions_cmp.c
// ============================================================================

// Common logic for CMP instructions. Computes dest - src and sets flags.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteCmp(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);

// CMP r/m8, r8
// CMP r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCmpRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx);
// CMP r8, r/m8
// CMP r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCmpRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx);
// CMP AL, imm8
// CMP AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCmpImmediateToALOrAX(const YCPUInstructionContext* ctx);

// ============================================================================
// Boolean instructions - instructions_bool.c
// ============================================================================

Y_MODULE_PRIVATE void YCPUSetFlagsAfterBooleanInstruction(
    const YCPUInstructionContext* ctx, uint32_t result);
// Common logic for AND instructions.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteBooleanAnd(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);
// Common logic for OR instructions.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteBooleanOr(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);
// Common logic for XOR instructions.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteBooleanXor(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);
// Common logic for TEST instructions.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteTest(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);

// AND r/m8, r8
// AND r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanAndRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx);
// AND r8, r/m8
// AND r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanAndRegisterOrMemoryToRegister(
    const YCPUInstructionContext* ctx);
// AND AL, imm8
// AND AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanAndImmediateToALOrAX(const YCPUInstructionContext* ctx);
// OR r/m8, r8
// OR r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanOrRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx);
// OR r8, r/m8
// OR r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanOrRegisterOrMemoryToRegister(
    const YCPUInstructionContext* ctx);
// OR AL, imm8
// OR AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanOrImmediateToALOrAX(const YCPUInstructionContext* ctx);
// XOR r/m8, r8
// XOR r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanXorRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx);
// XOR r8, r/m8
// XOR r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanXorRegisterOrMemoryToRegister(
    const YCPUInstructionContext* ctx);
// XOR AL, imm8
// XOR AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanXorImmediateToALOrAX(const YCPUInstructionContext* ctx);
// TEST r/m8, r8
// TEST r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteTestRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx);
// TEST AL, imm8
// TEST AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteTestImmediateToALOrAX(const YCPUInstructionContext* ctx);

// ============================================================================
// Control flow instructions - instructions_ctrl_flow.c
// ============================================================================

// Common logic for far jumps.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteFarJump(
    const YCPUInstructionContext* ctx, YCPUOperandValue segment,
    YCPUOperandValue offset);
// Common logic for far calls.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteFarCall(
    const YCPUInstructionContext* ctx, YCPUOperandValue segment,
    YCPUOperandValue offset);
// Common logic for returning from an interrupt.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteReturnFromInterrupt(YCPUState* cpu);

// JMP rel8
// JMP rel16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteShortOrNearJump(const YCPUInstructionContext* ctx);
// JMP ptr16:16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDirectFarJump(const YCPUInstructionContext* ctx);
// Unsigned conditional jumps.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteUnsignedConditionalJump(const YCPUInstructionContext* ctx);
// JL/JGNE and JNL/JGE
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSignedConditionalJumpJLOrJNL(const YCPUInstructionContext* ctx);
// JLE/JG and JNLE/JG
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSignedConditionalJumpJLEOrJNLE(const YCPUInstructionContext* ctx);
// LOOP rel8
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoop(const YCPUInstructionContext* ctx);
// LOOPZ rel8
// LOOPNZ rel8
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoopZOrNZ(const YCPUInstructionContext* ctx);
// JCXZ rel8
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteJumpIfCXIsZero(const YCPUInstructionContext* ctx);
// CALL rel16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDirectNearCall(const YCPUInstructionContext* ctx);
// CALL ptr16:16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDirectFarCall(const YCPUInstructionContext* ctx);
// RET
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteNearReturn(const YCPUInstructionContext* ctx);
// RET imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteNearReturnAndPop(const YCPUInstructionContext* ctx);
// RETF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteFarReturn(const YCPUInstructionContext* ctx);
// RETF imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteFarReturnAndPop(const YCPUInstructionContext* ctx);
// IRET
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteIret(const YCPUInstructionContext* ctx);
// INT 3
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInt3(const YCPUInstructionContext* ctx);
// INTO
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInto(const YCPUInstructionContext* ctx);
// INT n
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteIntN(const YCPUInstructionContext* ctx);
// HLT
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteHlt(const YCPUInstructionContext* ctx);

// ============================================================================
// Stack instructions - instructions_stack.c
// ============================================================================

// PUSH AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePushRegister(const YCPUInstructionContext* ctx);
// POP AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePopRegister(const YCPUInstructionContext* ctx);
// PUSH ES/CS/SS/DS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePushSegmentRegister(const YCPUInstructionContext* ctx);
// POP ES/CS/SS/DS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePopSegmentRegister(const YCPUInstructionContext* ctx);
// PUSHF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePushFlags(const YCPUInstructionContext* ctx);
// POPF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePopFlags(const YCPUInstructionContext* ctx);
// POP r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePopRegisterOrMemory(const YCPUInstructionContext* ctx);
// LAHF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoadAHFromFlags(const YCPUInstructionContext* ctx);
// SAHF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteStoreAHToFlags(const YCPUInstructionContext* ctx);

// ============================================================================
// Flag manipulation instructions - instructions_flags.c
// ============================================================================

// CLC, STC, CLI, STI, CLD, STD
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteClearOrSetFlag(const YCPUInstructionContext* ctx);
// CMC
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteComplementCarryFlag(const YCPUInstructionContext* ctx);
// SALC
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSetALFromCarry(const YCPUInstructionContext* ctx);

// ============================================================================
// IN and OUT instructions - instructions_io.c
// ============================================================================

// IN AL, imm8
// IN AX, imm8
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInImmediate(const YCPUInstructionContext* ctx);
// IN AL, DX
// IN AX, DX
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInDX(const YCPUInstructionContext* ctx);
// OUT imm8, AL
// OUT imm8, AX
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteOutImmediate(const YCPUInstructionContext* ctx);
// OUT DX, AL
// OUT DX, AX
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteOutDX(const YCPUInstructionContext* ctx);

// ============================================================================
// String instructions - instructions_string.c
// ============================================================================

// MOVS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMovs(const YCPUInstructionContext* ctx);
// STOS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteStos(const YCPUInstructionContext* ctx);
// LODS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLods(const YCPUInstructionContext* ctx);
// SCAS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteScas(const YCPUInstructionContext* ctx);
// CMPS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCmps(const YCPUInstructionContext* ctx);

// ============================================================================
// BCD and ASCII arithmetic instructions - instructions_bcd_ascii.c
// ============================================================================

// AAA
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAaa(const YCPUInstructionContext* ctx);
// AAS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAas(const YCPUInstructionContext* ctx);
// AAM
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAam(const YCPUInstructionContext* ctx);
// AAD
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAad(const YCPUInstructionContext* ctx);
// DAA
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDaa(const YCPUInstructionContext* ctx);
// DAS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDas(const YCPUInstructionContext* ctx);

// ============================================================================
// Group 1 instructions - instructions_group_1.c
// ============================================================================

// Group 1 instruction handler.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup1Instruction(const YCPUInstructionContext* ctx);

// Group 1 instruction handler, but sign-extends the 8-bit immediate value.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup1InstructionWithSignExtension(
    const YCPUInstructionContext* ctx);

// ============================================================================
// Group 2 instructions - instructions_group_2.c
// ============================================================================

// Group 2 shift / rotate by 1.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup2ShiftOrRotateBy1Instruction(const YCPUInstructionContext* ctx);
// Group 2 shift / rotate by CL.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup2ShiftOrRotateByCLInstruction(
    const YCPUInstructionContext* ctx);

// ============================================================================
// Group 3 instructions - instructions_group_3.c
// ============================================================================

// Group 3 instruction handler.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup3Instruction(const YCPUInstructionContext* ctx);

// ============================================================================
// Group 4 instructions - instructions_group_4.c
// ============================================================================

// Group 4 instruction handler.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup4Instruction(const YCPUInstructionContext* ctx);

// ============================================================================
// Group 5 instructions - instructions_group_5.c
// ============================================================================

// Group 5 instruction handler.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup5Instruction(const YCPUInstructionContext* ctx);

#endif  // YAX86_CPU_INSTRUCTIONS_H


// ==============================================================================
// src/cpu/instructions.h end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_helpers.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_helpers.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// Set common CPU flags after an instruction. This includes:
// - Zero flag (ZF)
// - Sign flag (SF)
// - Parity Flag (PF)
Y_MODULE_PRIVATE Y_HOT void YCPUSetCommonFlagsAfterInstruction(
    const YCPUInstructionContext* ctx, uint32_t result) {
  YCPUWidth width = ctx->metadata->width;
  result &= YCPUMaxValue[width];
  // Zero flag (ZF)
  YCPUSetFlag(ctx->cpu, Y_CPU_ZF, result == 0);
  // Sign flag (SF)
  YCPUSetFlag(ctx->cpu, Y_CPU_SF, result & YCPUSignBit[width]);
  // Parity flag (PF)
  // Set if the number of set bits in the least significant byte is even
  uint8_t parity = result & 0xFF;  // Check only the low byte for parity
  parity ^= parity >> 4;
  parity ^= parity >> 2;
  parity ^= parity >> 1;
  YCPUSetFlag(ctx->cpu, Y_CPU_PF, (parity & 1) == 0);
}

Y_MODULE_PRIVATE uint16_t YCPUToFlagsRegisterValue(uint16_t value) {
  return (value | (uint16_t)Y_CPU_FLAGS_ALWAYS_SET) &
         ~(uint16_t)Y_CPU_FLAGS_ALWAYS_CLEAR;
}

// Write a word where the stack pointer already points. The caller is
// responsible for having made room for it.
Y_FILE_PRIVATE void YCPUWriteToStackTop(
    YCPUState* cpu, YCPUOperandValue value) {
  YCPUOperandAddress address = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
      .register_index = Y_CPU_SS,
      .offset = cpu->registers[Y_CPU_SP],
  };
  YCPUWriteMemoryOperandWord(cpu, &address, value);
}

Y_MODULE_PRIVATE void YCPUPushValue(YCPUState* cpu, YCPUOperandValue value) {
  cpu->registers[Y_CPU_SP] -= 2;
  YCPUWriteToStackTop(cpu, value);
}

Y_MODULE_PRIVATE void YCPUPushSourceOperand(
    YCPUState* cpu, const YCPUOperand* src) {
  cpu->registers[Y_CPU_SP] -= 2;
  // The 8086/8088 moves the stack pointer before it reads the source, so
  // PUSH SP stores the value SP has after the decrement rather than the one it
  // had on entry. SP is the only source that can tell the difference. The
  // 80286 and later store the entry value instead.
  const bool source_is_stack_pointer =
      src->address.type == Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER &&
      src->address.register_index == Y_CPU_SP;
  const YCPUOperandValue value =
      source_is_stack_pointer ? cpu->registers[Y_CPU_SP] : src->value;
  YCPUWriteToStackTop(cpu, value);
}

Y_MODULE_PRIVATE YCPUOperandValue YCPUPop(YCPUState* cpu) {
  YCPUOperandAddress address = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
      .register_index = Y_CPU_SS,
      .offset = cpu->registers[Y_CPU_SP],
  };
  YCPUOperandValue value = YCPUReadMemoryOperandWord(cpu, &address);
  cpu->registers[Y_CPU_SP] += 2;
  return value;
}

// Dummy instruction for unsupported opcodes.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteNoOp(Y_UNUSED const YCPUInstructionContext* ctx) {
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Handler for the eight opcode bytes that are prefixes rather than
// instructions. The prefix decoder consumes those bytes before an opcode is
// looked up, so a decoded instruction never names one and this is unreachable
// from YCPUTick(). It exists so that every entry in the opcode table is
// callable, which is what lets the tick path dispatch without first checking
// for a null handler.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInvalidOpcode(Y_UNUSED const YCPUInstructionContext* ctx) {
  return Y_CPU_INSTRUCTION_INVALID;
}


// ==============================================================================
// src/cpu/instructions_helpers.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_mov.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_mov.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// MOV instructions
// ============================================================================

// MOV r/m8, r8
// MOV r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperandAddress dest = YCPUGetRegisterOrMemoryOperandAddress(ctx);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  YCPUWriteOperandAddress(ctx, &dest, YCPUFromOperand(&src));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV r8, r/m8
// MOV r16, r/m16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteMoveRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperand(&src));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV r/m16, sreg
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveSegmentRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx) {
  YCPUOperandAddress dest = YCPUGetRegisterOrMemoryOperandAddress(ctx);
  YCPUOperand src;
  YCPUReadSegmentRegisterOperand(ctx, &src);
  YCPUWriteOperandAddress(ctx, &dest, YCPUFromOperand(&src));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV sreg, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveRegisterOrMemoryToSegmentRegister(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadSegmentRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperand(&src));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV AX/CX/DX/BX/SP/BP/SI/DI, imm16
// MOV AH/AL/CH/CL/DH/DL/BH/BL, imm8
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteMoveImmediateToRegister(const YCPUInstructionContext* ctx) {
  static const uint8_t register_index_opcode_base[Y_CPU_NUM_WIDTHS] = {
      0xB0,  // Y_CPU_BYTE
      0xB8,  // Y_CPU_WORD
  };
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode -
                          register_index_opcode_base[ctx->metadata->width]);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperandValue(src_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV AL, moffs16
// MOV AX, moffs16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteMoveMemoryOffsetToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  // Offset is always 16 bits, even though the data width of the operation may
  // be 8 bits.
  YCPUOperandValue src_offset_value =
      YCPUReadImmediateOperandWord(ctx->instruction);
  // The source is DS:offset by default, but can be overridden by a segment
  // override prefix.
  YCPUOperandAddress src_address = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
      .register_index = Y_CPU_DS,
      .offset = (uint16_t)YCPUFromOperandValue(src_offset_value),
  };
  YCPUApplySegmentOverride(ctx->instruction, &src_address.register_index);
  YCPUOperandValue src_value = YCPUReadOperandValue(ctx, &src_address);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperandValue(src_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV moffs16, AL
// MOV moffs16, AX
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteMoveALOrAXToMemoryOffset(const YCPUInstructionContext* ctx) {
  YCPUOperand src;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &src);
  // Offset is always 16 bits, even though the data width of the operation may
  // be 8 bits.
  YCPUOperandValue dest_offset_value =
      YCPUReadImmediateOperandWord(ctx->instruction);
  // The destination is DS:offset by default, but can be overridden by a segment
  // override prefix.
  YCPUOperandAddress dest_address = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
      .register_index = Y_CPU_DS,
      .offset = (uint16_t)YCPUFromOperandValue(dest_offset_value),
  };
  YCPUApplySegmentOverride(ctx->instruction, &dest_address.register_index);
  YCPUWriteOperandAddress(ctx, &dest_address, YCPUFromOperand(&src));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV r/m8, imm8
// MOV r/m16, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveImmediateToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperandAddress dest = YCPUGetRegisterOrMemoryOperandAddress(ctx);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  YCPUWriteOperandAddress(ctx, &dest, YCPUFromOperandValue(src_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ============================================================================
// XCHG instructions
// ============================================================================

// XCHG AX, AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteExchangeRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode - 0x90);
  if (register_index == Y_CPU_AX) {
    // No-op
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  YCPUOperand src;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &src);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  uint32_t temp = YCPUFromOperand(&dest);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperand(&src));
  YCPUWriteOperand(ctx, &src, temp);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// XCHG r/m8, r8
// XCHG r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteExchangeRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  uint32_t temp = YCPUFromOperand(&dest);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperand(&src));
  YCPUWriteOperand(ctx, &src, temp);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ============================================================================
// XLAT
// ============================================================================

// XLAT
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteTranslateByte(const YCPUInstructionContext* ctx) {
  // Read the AL register
  YCPUOperand al;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &al);
  // The translation table is at DS:BX by default, but can be overridden by a
  // segment override prefix.
  YCPUOperandAddress src_address = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
      .register_index = Y_CPU_DS,
      .offset =
          (uint16_t)(ctx->cpu->registers[Y_CPU_BX] + YCPUFromOperand(&al)),
  };
  YCPUApplySegmentOverride(ctx->instruction, &src_address.register_index);
  YCPUOperandValue src_value =
      YCPUReadMemoryOperandByte(ctx->cpu, &src_address);
  YCPUWriteOperandAddress(ctx, &al.address, YCPUFromOperandValue(src_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}


// ==============================================================================
// src/cpu/instructions_mov.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_lea.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_lea.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// LEA instruction
// ============================================================================

// LEA r16, m
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoadEffectiveAddress(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUMemoryAddress memory_address =
      YCPUGetMemoryOperandAddress(ctx->cpu, ctx->instruction);
  // LEA yields the effective address - the offset within the segment - and
  // never resolves it against a segment or touches memory. Converting to a
  // linear address here would fold the segment base into the result, which is
  // invisible only while the segment register happens to be zero.
  YCPUWriteOperandAddress(ctx, &dest.address, memory_address.offset);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ============================================================================
// LES and LDS instructions
// ============================================================================

// Common logic for LES and LDS instructions.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteLoadSegmentWithPointer(
    const YCPUInstructionContext* ctx,
    YCPURegisterIndex segment_register_index) {
  YCPUOperand destRegister;
  YCPUReadRegisterOperand(ctx, &destRegister);
  YCPUOperand destSegmentRegister;
  YCPUReadRegisterOperandForRegisterIndex(
      ctx, segment_register_index, &destSegmentRegister);

  const YCPUMemoryAddress src_memory_address =
      YCPUGetMemoryOperandAddress(ctx->cpu, ctx->instruction);
  YCPUOperandAddress src_address = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
      .register_index = (uint8_t)src_memory_address.segment_register_index,
      .offset = src_memory_address.offset,
  };
  YCPUOperandValue src_offset_value =
      YCPUReadMemoryOperandWord(ctx->cpu, &src_address);
  src_address.offset += 2;
  YCPUOperandValue src_segment_value =
      YCPUReadMemoryOperandWord(ctx->cpu, &src_address);

  YCPUWriteOperand(ctx, &destRegister, YCPUFromOperandValue(src_offset_value));
  YCPUWriteOperand(
      ctx, &destSegmentRegister, YCPUFromOperandValue(src_segment_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// LES r16, m
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoadESWithPointer(const YCPUInstructionContext* ctx) {
  return YCPUExecuteLoadSegmentWithPointer(ctx, Y_CPU_ES);
}

// LDS r16, m
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoadDSWithPointer(const YCPUInstructionContext* ctx) {
  return YCPUExecuteLoadSegmentWithPointer(ctx, Y_CPU_DS);
}


// ==============================================================================
// src/cpu/instructions_lea.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_add.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_add.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// ADD, ADC, and INC instructions
// ============================================================================

// Set CPU flags after an INC instruction.
// Other than common flags, the INC instruction sets the following flags:
// - Overflow Flag (OF) - Set when result has wrong sign
// - Auxiliary Carry Flag (AF) - carry from bit 3 to bit 4
Y_FILE_PRIVATE void YCPUSetFlagsAfterInc(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_carry) {
  YCPUSetCommonFlagsAfterInstruction(ctx, result);

  // Overflow Flag (OF) Set when result has wrong sign (both operands have same
  // sign but result has different sign)
  uint32_t sign_bit = YCPUSignBit[ctx->metadata->width];
  bool op1_sign = (op1 & sign_bit) != 0;
  bool op2_sign = (op2 & sign_bit) != 0;
  bool result_sign = (result & sign_bit) != 0;
  YCPUSetFlag(
      ctx->cpu, Y_CPU_OF, (op1_sign == op2_sign) && (result_sign != op1_sign));

  // Auxiliary Carry Flag (AF) - carry from bit 3 to bit 4
  YCPUSetFlag(
      ctx->cpu, Y_CPU_AF,
      ((op1 & 0xF) + (op2 & 0xF) + (did_carry ? 1 : 0)) > 0xF);
}

// Set CPU flags after an ADD or ADC instruction.
// Other than the flags set by the INC instruction, the ADD instruction sets the
// following flags:
// - Carry Flag (CF) - Set when result overflows the maximum width
Y_FILE_PRIVATE void YCPUSetFlagsAfterAdd(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_carry) {
  YCPUSetFlagsAfterInc(ctx, op1, op2, result, did_carry);
  // Carry Flag (CF)
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, result > YCPUMaxValue[ctx->metadata->width]);
}

// Common signature of YCPUSetFlagsAfterAdd and YCPUSetFlagsAfterInc.
typedef void (*YCPUSetFlagsAfterAddFn)(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_carry);

// Common logic for ADD, ADC, and INC instructions.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteAddCommon(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value, bool carry,
    YCPUSetFlagsAfterAddFn set_flags_after_fn) {
  uint32_t raw_dest_value = YCPUFromOperand(dest);
  uint32_t raw_src_value = YCPUFromOperandValue(src_value);
  bool should_carry = carry && YCPUGetFlag(ctx->cpu, Y_CPU_CF);
  uint32_t result = raw_dest_value + raw_src_value + (should_carry ? 1 : 0);
  YCPUWriteOperand(ctx, dest, result);
  (*set_flags_after_fn)(
      ctx, raw_dest_value, raw_src_value, result, should_carry);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Common logic for ADD instructions
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteAdd(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  return YCPUExecuteAddCommon(
      ctx, dest, src_value, /* carry */ false, YCPUSetFlagsAfterAdd);
}

// ADD r/m8, r8
// ADD r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteAdd(ctx, &dest, src.value);
}

// ADD r8, r/m8
// ADD r16, r/m16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteAddRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteAdd(ctx, &dest, src.value);
}

// ADD AL, imm8
// ADD AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteAdd(ctx, &dest, src_value);
}

// Common logic for ADC instructions
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult YCPUExecuteAddWithCarry(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  return YCPUExecuteAddCommon(
      ctx, dest, src_value, /* carry */ true, YCPUSetFlagsAfterAdd);
}

// ADC r/m8, r8
// ADC r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddRegisterToRegisterOrMemoryWithCarry(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteAddWithCarry(ctx, &dest, src.value);
}
// ADC r8, r/m8
// ADC r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddRegisterOrMemoryToRegisterWithCarry(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteAddWithCarry(ctx, &dest, src.value);
}

// ADC AL, imm8
// ADC AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddImmediateToALOrAXWithCarry(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteAddWithCarry(ctx, &dest, src_value);
}

// Common logic for INC instructions
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInc(const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUOperandValue src_value = 1;
  return YCPUExecuteAddCommon(
      ctx, dest, src_value, /* carry */ false, YCPUSetFlagsAfterInc);
}

// INC AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteIncRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode - 0x40);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &dest);
  return YCPUExecuteInc(ctx, &dest);
}


// ==============================================================================
// src/cpu/instructions_add.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_sub.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_sub.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// SUB, SBB, and DEC instructions
// ============================================================================

// Set CPU flags after a DEC or SUB/SBB operation (base function).
// This function sets ZF, SF, PF, OF, AF. It does NOT affect CF.
// - OF is for the full operation op1 - (op2 + did_borrow).
// - AF is for the full operation op1 - (op2 + did_borrow).
Y_FILE_PRIVATE void YCPUSetFlagsAfterDec(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_borrow) {
  YCPUSetCommonFlagsAfterInstruction(ctx, result);

  uint32_t sign_bit = YCPUSignBit[ctx->metadata->width];
  uint32_t max_val = YCPUMaxValue[ctx->metadata->width];

  // Overflow Flag (OF)
  // A subtraction overflows when the operands have different signs and the
  // result takes the sign of the subtrahend rather than the minuend. The
  // borrow does not enter into it: it shifts the result by one, and the result
  // is already what the test looks at. Folding the borrow into the subtrahend
  // instead would change that operand's sign whenever it is 0x7F or 0x7FFF,
  // and give the wrong answer for exactly those values.
  bool op1_sign = (op1 & sign_bit) != 0;
  bool op2_sign = ((op2 & max_val) & sign_bit) != 0;
  bool result_sign = (result & sign_bit) != 0;
  YCPUSetFlag(
      ctx->cpu, Y_CPU_OF, (op1_sign != op2_sign) && (result_sign != op1_sign));

  // Auxiliary Carry Flag (AF) - borrow from bit 3 to bit 4
  YCPUSetFlag(
      ctx->cpu, Y_CPU_AF, (op1 & 0xF) < ((op2 & 0xF) + (did_borrow ? 1 : 0)));
}

// Set CPU flags after a SUB, SBB, CMP or NEG instruction.
// This calls YCPUSetFlagsAfterDec and then sets the Carry Flag (CF).
Y_MODULE_PRIVATE void YCPUSetFlagsAfterSub(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_borrow) {
  YCPUSetFlagsAfterDec(ctx, op1, op2, result, did_borrow);

  // Carry Flag (CF) - Set when a borrow is generated
  // CF is set if op1 < (op2 + did_borrow) (unsigned comparison)
  uint32_t val_being_subtracted =
      (op2 & YCPUMaxValue[ctx->metadata->width]) + (did_borrow ? 1 : 0);
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, op1 < val_being_subtracted);
}

// Common signature of YCPUSetFlagsAfterSub and YCPUSetFlagsAfterDec.
typedef void (*YCPUSetFlagsAfterSubFn)(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_borrow);

// Common logic for SUB, SBB, and DEC instructions.
Y_FILE_PRIVATE Y_HOT YCPUInstructionResult YCPUExecuteSubCommon(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value, bool borrow,
    YCPUSetFlagsAfterSubFn set_flags_after_fn) {
  uint32_t raw_dest_value = YCPUFromOperand(dest);
  uint32_t raw_src_value = YCPUFromOperandValue(src_value);
  bool should_borrow = borrow && YCPUGetFlag(ctx->cpu, Y_CPU_CF);
  uint32_t result = raw_dest_value - raw_src_value - (should_borrow ? 1 : 0);
  YCPUWriteOperand(ctx, dest, result);
  (*set_flags_after_fn)(
      ctx, raw_dest_value, raw_src_value, result, should_borrow);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Common logic for SUB instructions
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteSub(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  return YCPUExecuteSubCommon(
      ctx, dest, src_value, /* borrow */ false, YCPUSetFlagsAfterSub);
}

// SUB r/m8, r8
// SUB r/m16, r16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteSubRegisterFromRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteSub(ctx, &dest, src.value);
}

// SUB r8, r/m8
// SUB r16, r/m16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteSubRegisterOrMemoryFromRegister(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteSub(ctx, &dest, src.value);
}

// SUB AL, imm8
// SUB AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubImmediateFromALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteSub(ctx, &dest, src_value);
}

// Common logic for SBB instructions
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteSubWithBorrow(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  return YCPUExecuteSubCommon(
      ctx, dest, src_value, /* borrow */ true, YCPUSetFlagsAfterSub);
}

// SBB r/m8, r8
// SBB r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubRegisterFromRegisterOrMemoryWithBorrow(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteSubWithBorrow(ctx, &dest, src.value);
}

// SBB r8, r/m8
// SBB r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubRegisterOrMemoryFromRegisterWithBorrow(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteSubWithBorrow(ctx, &dest, src.value);
}

// SBB AL, imm8
// SBB AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubImmediateFromALOrAXWithBorrow(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteSubWithBorrow(ctx, &dest, src_value);
}

// Common logic for DEC instructions
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDec(const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUOperandValue src_value = 1;
  return YCPUExecuteSubCommon(
      ctx, dest, src_value, /* borrow */ false, YCPUSetFlagsAfterDec);
}

// DEC AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDecRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode - 0x48);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &dest);
  return YCPUExecuteDec(ctx, &dest);
}


// ==============================================================================
// src/cpu/instructions_sub.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_sign_ext.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_sign_ext.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Sign extension instructions
// ============================================================================

// CBW
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCbw(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[Y_CPU_AX] & 0xFF;
  uint8_t ah = (al & YCPUSignBit[Y_CPU_BYTE]) ? 0xFF : 0x00;
  ctx->cpu->registers[Y_CPU_AX] = (ah << 8) | al;
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// CWD
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteCwd(const YCPUInstructionContext* ctx) {
  ctx->cpu->registers[Y_CPU_DX] =
      (ctx->cpu->registers[Y_CPU_AX] & YCPUSignBit[Y_CPU_WORD]) ? 0xFFFF
                                                                : 0x0000;
  return Y_CPU_INSTRUCTION_EXECUTED;
}


// ==============================================================================
// src/cpu/instructions_sign_ext.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_cmp.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_cmp.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// CMP instructions
// ============================================================================

// Common logic for CMP instructions. Computes dest - src and sets flags.
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult YCPUExecuteCmp(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  uint32_t raw_dest_value = YCPUFromOperand(dest);
  uint32_t raw_src_value = YCPUFromOperandValue(src_value);
  uint32_t result = raw_dest_value - raw_src_value;
  YCPUSetFlagsAfterSub(ctx, raw_dest_value, raw_src_value, result, false);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// CMP r/m8, r8
// CMP r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCmpRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteCmp(ctx, &dest, src.value);
}

// CMP r8, r/m8
// CMP r16, r/m16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteCmpRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteCmp(ctx, &dest, src.value);
}

// CMP AL, imm8
// CMP AX, imm16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteCmpImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteCmp(ctx, &dest, src_value);
}


// ==============================================================================
// src/cpu/instructions_cmp.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_bool.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_bool.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Boolean AND, OR and XOR instructions
// ============================================================================

Y_MODULE_PRIVATE void YCPUSetFlagsAfterBooleanInstruction(
    const YCPUInstructionContext* ctx, uint32_t result) {
  YCPUSetCommonFlagsAfterInstruction(ctx, result);
  // Carry Flag (CF) should be cleared
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, false);
  // Overflow Flag (OF) should be cleared
  YCPUSetFlag(ctx->cpu, Y_CPU_OF, false);
}

// Common logic for AND instructions.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteBooleanAnd(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  uint32_t result = YCPUFromOperand(dest) & YCPUFromOperandValue(src_value);
  YCPUWriteOperand(ctx, dest, result);
  YCPUSetFlagsAfterBooleanInstruction(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// AND r/m8, r8
// AND r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanAndRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteBooleanAnd(ctx, &dest, src.value);
}

// AND r8, r/m8
// AND r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanAndRegisterOrMemoryToRegister(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteBooleanAnd(ctx, &dest, src.value);
}

// AND AL, imm8
// AND AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanAndImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteBooleanAnd(ctx, &dest, src_value);
}

// Common logic for OR instructions.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteBooleanOr(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  uint32_t result = YCPUFromOperand(dest) | YCPUFromOperandValue(src_value);
  YCPUWriteOperand(ctx, dest, result);
  YCPUSetFlagsAfterBooleanInstruction(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// OR r/m8, r8
// OR r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanOrRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteBooleanOr(ctx, &dest, src.value);
}

// OR r8, r/m8
// OR r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanOrRegisterOrMemoryToRegister(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteBooleanOr(ctx, &dest, src.value);
}

// OR AL, imm8
// OR AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanOrImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteBooleanOr(ctx, &dest, src_value);
}

// Common logic for XOR instructions.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteBooleanXor(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  uint32_t result = YCPUFromOperand(dest) ^ YCPUFromOperandValue(src_value);
  YCPUWriteOperand(ctx, dest, result);
  YCPUSetFlagsAfterBooleanInstruction(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// XOR r/m8, r8
// XOR r/m16, r16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteBooleanXorRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteBooleanXor(ctx, &dest, src.value);
}

// XOR r8, r/m8
// XOR r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanXorRegisterOrMemoryToRegister(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteBooleanXor(ctx, &dest, src.value);
}

// XOR AL, imm8
// XOR AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanXorImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteBooleanXor(ctx, &dest, src_value);
}

// ============================================================================
// TEST instructions
// ============================================================================

// Common logic for TEST instructions.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteTest(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  uint32_t result = YCPUFromOperand(dest) & YCPUFromOperandValue(src_value);
  YCPUSetFlagsAfterBooleanInstruction(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// TEST r/m8, r8
// TEST r/m16, r16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteTestRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteTest(ctx, &dest, src.value);
}

// TEST AL, imm8
// TEST AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteTestImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteTest(ctx, &dest, src_value);
}


// ==============================================================================
// src/cpu/instructions_bool.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_ctrl_flow.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_ctrl_flow.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "cycles.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// JMP instructions
// ============================================================================

// Jump to a relative signed byte offset.
Y_FILE_PRIVATE Y_HOT YCPUInstructionResult YCPUExecuteRelativeJumpByte(
    const YCPUInstructionContext* ctx, YCPUOperandValue offset_value) {
  ctx->cpu->registers[Y_CPU_IP] = YCPUAddSignedOffsetByte(
      ctx->cpu->registers[Y_CPU_IP], YCPUFromOperandValue(offset_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Jump to a relative signed word offset.
Y_FILE_PRIVATE Y_HOT YCPUInstructionResult YCPUExecuteRelativeJumpWord(
    const YCPUInstructionContext* ctx, YCPUOperandValue offset_value) {
  ctx->cpu->registers[Y_CPU_IP] = YCPUAddSignedOffsetWord(
      ctx->cpu->registers[Y_CPU_IP], YCPUFromOperandValue(offset_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Table of relative jump instructions, indexed by width.
Y_FILE_PRIVATE
YCPUInstructionResult (*const YCPURelativeJumpFn[Y_CPU_NUM_WIDTHS])(
    const YCPUInstructionContext* ctx, YCPUOperandValue offset_value) = {
    YCPUExecuteRelativeJumpByte,  // Y_CPU_BYTE
    YCPUExecuteRelativeJumpWord,  // Y_CPU_WORD
};

// Common logic for JMP instructions.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteRelativeJump(
    const YCPUInstructionContext* ctx, YCPUOperandValue offset_value) {
  return YCPURelativeJumpFn[ctx->metadata->width](ctx, offset_value);
}

// JMP rel8
// JMP rel16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteShortOrNearJump(const YCPUInstructionContext* ctx) {
  YCPUOperandValue offset_value = YCPUReadImmediate(ctx);
  return YCPUExecuteRelativeJump(ctx, offset_value);
}

// Common logic for far jumps.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteFarJump(
    const YCPUInstructionContext* ctx, YCPUOperandValue segment,
    YCPUOperandValue offset) {
  ctx->cpu->registers[Y_CPU_CS] = YCPUFromOperandValue(segment);
  ctx->cpu->registers[Y_CPU_IP] = YCPUFromOperandValue(offset);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// JMP ptr16:16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDirectFarJump(const YCPUInstructionContext* ctx) {
  YCPUOperandValue new_cs =
      (YCPUOperandValue)(((uint16_t)ctx->instruction->immediate[2]) |
                         (((uint16_t)ctx->instruction->immediate[3]) << 8));
  YCPUOperandValue new_ip =
      (YCPUOperandValue)(((uint16_t)ctx->instruction->immediate[0]) |
                         (((uint16_t)ctx->instruction->immediate[1]) << 8));
  return YCPUExecuteFarJump(ctx, new_cs, new_ip);
}

// ============================================================================
// Conditional jumps
// ============================================================================

// Common logic for conditional jumps.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteConditionalJump(
    const YCPUInstructionContext* ctx, bool value, bool success_value) {
  if (value == success_value) {
    // A taken jump throws away the prefetch queue and has to fill it again.
    // The base cost in the cycle table is for the branch not being taken.
    YCPUAddCycles(ctx->cpu, Y_CPU_JUMP_TAKEN_CYCLES);
    YCPUOperandValue offset_value = YCPUReadImmediate(ctx);
    return YCPUExecuteRelativeJump(ctx, offset_value);
  }
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Table of flag register bitmasks for conditional jumps. The index corresponds
// to (opcode & 0x0F) / 2, so that the undocumented 0x60-0x6F aliases share the
// entries of their 0x70-0x7F counterparts.
Y_FILE_PRIVATE const uint16_t YCPUUnsignedConditionalJumpFlagBitmasks[] = {
    Y_CPU_OF,             // 0x70 - JO, 0x71 - JNO
    Y_CPU_CF,             // 0x72 - JC, 0x73 - JNC
    Y_CPU_ZF,             // 0x74 - JE, 0x75 - JNE
    Y_CPU_CF | Y_CPU_ZF,  // 0x76 - JBE, 0x77 - JNBE
    Y_CPU_SF,             // 0x78 - JS, 0x79 - JNS
    Y_CPU_PF,             // 0x7A - JP, 0x7B - JNP
};

// Unsigned conditional jumps.
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteUnsignedConditionalJump(const YCPUInstructionContext* ctx) {
  // Masking off the high nibble handles both 0x70-0x7F and their undocumented
  // 0x60-0x6F aliases, which the 8086/8088 decodes identically.
  uint16_t flag_mask = YCPUUnsignedConditionalJumpFlagBitmasks
      [(ctx->instruction->opcode & 0x0F) / 2];
  bool flag_value = (ctx->cpu->flags & flag_mask) != 0;
  // Even opcode => jump if the flag is set
  // Odd opcode => jump if the flag is not set
  bool success_value = ((ctx->instruction->opcode & 0x1) == 0);
  return YCPUExecuteConditionalJump(ctx, flag_value, success_value);
}

// JL/JGNE and JNL/JGE
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteSignedConditionalJumpJLOrJNL(const YCPUInstructionContext* ctx) {
  const bool is_greater_or_equal =
      YCPUGetFlag(ctx->cpu, Y_CPU_SF) == YCPUGetFlag(ctx->cpu, Y_CPU_OF);
  const bool success_value = (ctx->instruction->opcode & 0x1);
  return YCPUExecuteConditionalJump(ctx, is_greater_or_equal, success_value);
}

// JLE/JG and JNLE/JG
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSignedConditionalJumpJLEOrJNLE(const YCPUInstructionContext* ctx) {
  const bool is_greater =
      !YCPUGetFlag(ctx->cpu, Y_CPU_ZF) &&
      (YCPUGetFlag(ctx->cpu, Y_CPU_SF) == YCPUGetFlag(ctx->cpu, Y_CPU_OF));
  const bool success_value = (ctx->instruction->opcode & 0x1);
  return YCPUExecuteConditionalJump(ctx, is_greater, success_value);
}

// ============================================================================
// Loop instructions
// ============================================================================

// LOOP rel8
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteLoop(const YCPUInstructionContext* ctx) {
  return YCPUExecuteConditionalJump(
      ctx, --(ctx->cpu->registers[Y_CPU_CX]) != 0, true);
}

// LOOPZ rel8
// LOOPNZ rel8
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoopZOrNZ(const YCPUInstructionContext* ctx) {
  bool condition1 = --(ctx->cpu->registers[Y_CPU_CX]) != 0;
  bool condition2 = YCPUGetFlag(ctx->cpu, Y_CPU_ZF) ==
                    (bool)(ctx->instruction->opcode - 0xE0);
  return YCPUExecuteConditionalJump(ctx, condition1 && condition2, true);
}

// JCXZ rel8
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteJumpIfCXIsZero(const YCPUInstructionContext* ctx) {
  return YCPUExecuteConditionalJump(
      ctx, ctx->cpu->registers[Y_CPU_CX] == 0, true);
}

// ============================================================================
// CALL and RET instructions
// ============================================================================

// Common logic for near calls.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteNearCall(
    const YCPUInstructionContext* ctx, YCPUOperandValue offset) {
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[Y_CPU_IP]);
  return YCPUExecuteRelativeJump(ctx, offset);
}

// CALL rel16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteDirectNearCall(const YCPUInstructionContext* ctx) {
  YCPUOperandValue offset = YCPUReadImmediate(ctx);
  return YCPUExecuteNearCall(ctx, offset);
}

// Common logic for far calls.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteFarCall(
    const YCPUInstructionContext* ctx, YCPUOperandValue segment,
    YCPUOperandValue offset) {
  // Push the current CS and IP onto the stack.
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[Y_CPU_CS]);
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[Y_CPU_IP]);
  return YCPUExecuteFarJump(ctx, segment, offset);
}

// CALL ptr16:16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDirectFarCall(const YCPUInstructionContext* ctx) {
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[Y_CPU_CS]);
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[Y_CPU_IP]);
  return YCPUExecuteDirectFarJump(ctx);
}

// Common logic for RET instructions.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteNearReturnCommon(
    const YCPUInstructionContext* ctx, uint16_t arg_size) {
  YCPUOperandValue new_ip = YCPUPop(ctx->cpu);
  ctx->cpu->registers[Y_CPU_IP] = YCPUFromOperandValue(new_ip);
  ctx->cpu->registers[Y_CPU_SP] += arg_size;
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// RET
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteNearReturn(const YCPUInstructionContext* ctx) {
  return YCPUExecuteNearReturnCommon(ctx, 0);
}

// RET imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteNearReturnAndPop(const YCPUInstructionContext* ctx) {
  YCPUOperandValue arg_size_value = YCPUReadImmediate(ctx);
  return YCPUExecuteNearReturnCommon(ctx, YCPUFromOperandValue(arg_size_value));
}

// Common logic for RETF instructions.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteFarReturnCommon(
    const YCPUInstructionContext* ctx, uint16_t arg_size) {
  YCPUOperandValue new_ip = YCPUPop(ctx->cpu);
  YCPUOperandValue new_cs = YCPUPop(ctx->cpu);
  ctx->cpu->registers[Y_CPU_IP] = YCPUFromOperandValue(new_ip);
  ctx->cpu->registers[Y_CPU_CS] = YCPUFromOperandValue(new_cs);
  ctx->cpu->registers[Y_CPU_SP] += arg_size;
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// RETF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteFarReturn(const YCPUInstructionContext* ctx) {
  return YCPUExecuteFarReturnCommon(ctx, 0);
}

// RETF imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteFarReturnAndPop(const YCPUInstructionContext* ctx) {
  YCPUOperandValue arg_size_value = YCPUReadImmediate(ctx);
  return YCPUExecuteFarReturnCommon(ctx, YCPUFromOperandValue(arg_size_value));
}

// ============================================================================
// Interrupt instructions
// ============================================================================

// Common logic for returning from an interrupt.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteReturnFromInterrupt(YCPUState* cpu) {
  YCPUOperandValue ip_value = YCPUPop(cpu);
  cpu->registers[Y_CPU_IP] = YCPUFromOperandValue(ip_value);
  YCPUOperandValue cs_value = YCPUPop(cpu);
  cpu->registers[Y_CPU_CS] = YCPUFromOperandValue(cs_value);
  YCPUOperandValue flags_value = YCPUPop(cpu);
  cpu->flags = YCPUToFlagsRegisterValue(YCPUFromOperandValue(flags_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// IRET
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteIret(const YCPUInstructionContext* ctx) {
  return YCPUExecuteReturnFromInterrupt(ctx->cpu);
}

// INT 3
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInt3(const YCPUInstructionContext* ctx) {
  YCPURaiseInternalInterrupt(ctx->cpu, Y_CPU_INTERRUPT_BREAKPOINT);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// INTO
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInto(const YCPUInstructionContext* ctx) {
  if (YCPUGetFlag(ctx->cpu, Y_CPU_OF)) {
    YCPURaiseInternalInterrupt(ctx->cpu, Y_CPU_INTERRUPT_OVERFLOW);
  }
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// INT n
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteIntN(const YCPUInstructionContext* ctx) {
  YCPUOperandValue interrupt_number_value = YCPUReadImmediate(ctx);
  YCPURaiseInternalInterrupt(
      ctx->cpu, YCPUFromOperandValue(interrupt_number_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// HLT
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteHlt(Y_UNUSED const YCPUInstructionContext* ctx) {
  // HLT executes successfully - it just leaves the CPU halted until an
  // interrupt wakes it. YCPUTick() reports the halted state to its caller.
  ctx->cpu->is_halted = true;
  return Y_CPU_INSTRUCTION_EXECUTED;
}


// ==============================================================================
// src/cpu/instructions_ctrl_flow.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_stack.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_stack.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// PUSH and POP instructions
// ============================================================================

// PUSH AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecutePushRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode - 0x50);
  YCPUOperand src;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &src);
  YCPUPushSourceOperand(ctx->cpu, &src);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// POP AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecutePopRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode - 0x58);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &dest);
  YCPUOperandValue value = YCPUPop(ctx->cpu);
  YCPUWriteOperandAddress(ctx, &dest.address, YCPUFromOperandValue(value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// PUSH ES/CS/SS/DS
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecutePushSegmentRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(((ctx->instruction->opcode >> 3) & 0x03) + 8);
  YCPUOperand src;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &src);
  YCPUPushValue(ctx->cpu, src.value);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// POP ES/CS/SS/DS
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecutePopSegmentRegister(const YCPUInstructionContext* ctx) {
  // The segment register field is only two bits wide, which is what makes
  // 0x0F decode as POP CS on the 8086/8088. Popping into CS is legal there -
  // it just makes the next instruction fetch come from the new segment.
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(((ctx->instruction->opcode >> 3) & 0x03) + 8);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &dest);
  YCPUOperandValue value = YCPUPop(ctx->cpu);
  YCPUWriteOperandAddress(ctx, &dest.address, YCPUFromOperandValue(value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// PUSHF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePushFlags(const YCPUInstructionContext* ctx) {
  YCPUPushValue(ctx->cpu, ctx->cpu->flags);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// POPF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePopFlags(const YCPUInstructionContext* ctx) {
  YCPUOperandValue value = YCPUPop(ctx->cpu);
  ctx->cpu->flags = YCPUToFlagsRegisterValue(YCPUFromOperandValue(value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// POP r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePopRegisterOrMemory(const YCPUInstructionContext* ctx) {
  // The 8086/8088 does not decode the REG field of 0x8F at all, so every value
  // pops. Only REG 0 is documented.
  //
  // The destination address is resolved before the pop, because SP and BP can
  // both address it and the 8086 computes the effective address first.
  YCPUOperandAddress dest = YCPUGetRegisterOrMemoryOperandAddress(ctx);
  YCPUOperandValue value = YCPUPop(ctx->cpu);
  YCPUWriteOperandAddress(ctx, &dest, YCPUFromOperandValue(value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ============================================================================
// LAHF and SAHF
// ============================================================================

// Returns the AH register address.
Y_FILE_PRIVATE const YCPUOperandAddress* YCPUGetAHRegisterAddress(void) {
  static YCPUOperandAddress ah = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER,
      .register_index = Y_CPU_AX,
      .offset = 8,
  };
  return &ah;
}

// LAHF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoadAHFromFlags(const YCPUInstructionContext* ctx) {
  YCPUWriteRegisterOperandByte(
      ctx->cpu, YCPUGetAHRegisterAddress(),
      (YCPUOperandValue)(ctx->cpu->flags & 0x00FF));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// SAHF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteStoreAHToFlags(const YCPUInstructionContext* ctx) {
  YCPUOperandValue value =
      YCPUReadRegisterOperandByte(ctx->cpu, YCPUGetAHRegisterAddress());
  // Clear the lower byte of flags and set it to the value in AH
  ctx->cpu->flags =
      YCPUToFlagsRegisterValue((ctx->cpu->flags & 0xFF00) | value);
  return Y_CPU_INSTRUCTION_EXECUTED;
}


// ==============================================================================
// src/cpu/instructions_stack.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_flags.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_flags.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// CLC, STC, CLI, STI, CLD, STD instructions
// ============================================================================

// Table of flags corresponding to the CLC, STC, CLI, STI, CLD, and STD
// instructions, indexed by (opcode - 0xF8) / 2.
Y_FILE_PRIVATE const YCPUFlag YCPUFlagsForClearAndSetInstructions[] = {
    Y_CPU_CF,  // CLC, STC
    Y_CPU_IF,  // CLI, STI
    Y_CPU_DF,  // CLD, STD
};

Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteClearOrSetFlag(const YCPUInstructionContext* ctx) {
  uint8_t opcode_index = ctx->instruction->opcode - 0xF8;
  YCPUFlag flag = YCPUFlagsForClearAndSetInstructions[opcode_index / 2];
  bool value = (opcode_index & 0x1) != 0;
  YCPUSetFlag(ctx->cpu, flag, value);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ============================================================================
// CMC instruction
// ============================================================================

// CMC
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteComplementCarryFlag(const YCPUInstructionContext* ctx) {
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, !YCPUGetFlag(ctx->cpu, Y_CPU_CF));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ============================================================================
// SALC instruction
// ============================================================================

// SALC - Set AL from Carry
//
// Undocumented on every x86 generation, but consistently implemented: AL
// becomes 0xFF if CF is set and 0x00 otherwise. No flags are affected.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSetALFromCarry(const YCPUInstructionContext* ctx) {
  const uint8_t value = YCPUGetFlag(ctx->cpu, Y_CPU_CF) ? 0xFF : 0x00;
  ctx->cpu->registers[Y_CPU_AX] =
      (ctx->cpu->registers[Y_CPU_AX] & 0xFF00) | value;
  return Y_CPU_INSTRUCTION_EXECUTED;
}


// ==============================================================================
// src/cpu/instructions_flags.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_io.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_io.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// IN and OUT instructions
// ============================================================================

// Read a byte from an I/O port.
Y_FILE_PRIVATE Y_HOT YCPUOperandValue
YCPUReadByteFromPort(YCPUState* cpu, uint16_t port) {
  return (YCPUOperandValue)(cpu->config.read_port
                                ? cpu->config.read_port(cpu, port)
                                : 0xFF);
}

// Read a word from an I/O port as a uint16_t. The 8088 has an 8-bit data bus,
// so a word access is two byte accesses to consecutive ports. Peripherals rely
// on this: writing a 6845 register index and its data in one OUT DX, AX is the
// standard idiom, and it is what the BIOS uses.
Y_FILE_PRIVATE YCPUOperandValue
YCPUReadWordFromPort(YCPUState* cpu, uint16_t port) {
  uint8_t low = (uint8_t)YCPUReadByteFromPort(cpu, port);
  uint8_t high = (uint8_t)YCPUReadByteFromPort(cpu, (uint16_t)(port + 1));
  return (YCPUOperandValue)((high << 8) | low);
}

// Table of functions to read from an I/O port, indexed by data width.
Y_FILE_PRIVATE YCPUOperandValue (*const YCPUReadFromPortFns[])(
    YCPUState*, uint16_t) = {
  YCPUReadByteFromPort,  // Y_CPU_BYTE
  YCPUReadWordFromPort,  // Y_CPU_WORD
};

// Common logic for IN instructions.
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteIn(const YCPUInstructionContext* ctx, uint16_t port) {
  YCPUOperandValue value =
      YCPUReadFromPortFns[ctx->metadata->width](ctx->cpu, port);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperandValue(value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// IN AL, imm8
// IN AX, imm8
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteInImmediate(const YCPUInstructionContext* ctx) {
  YCPUOperandValue port = YCPUReadImmediateOperandByte(ctx->instruction);
  return YCPUExecuteIn(ctx, YCPUFromOperandValue(port));
}

// IN AL, DX
// IN AX, DX
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteInDX(const YCPUInstructionContext* ctx) {
  return YCPUExecuteIn(ctx, ctx->cpu->registers[Y_CPU_DX]);
}

// Write a byte to an I/O port.
Y_FILE_PRIVATE void YCPUWriteByteToPort(
    YCPUState* cpu, uint16_t port, YCPUOperandValue value) {
  if (!cpu->config.write_port) {
    return;
  }
  cpu->config.write_port(cpu, port, YCPUFromOperandValue(value));
}

// Write a word to an I/O port. As with reads, this is two byte accesses to
// consecutive ports.
Y_FILE_PRIVATE void YCPUWriteWordToPort(
    YCPUState* cpu, uint16_t port, YCPUOperandValue value) {
  uint32_t raw_value = YCPUFromOperandValue(value);
  YCPUWriteByteToPort(cpu, port, (YCPUOperandValue)(raw_value & 0xFF));
  YCPUWriteByteToPort(
      cpu, (uint16_t)(port + 1), (YCPUOperandValue)((raw_value >> 8) & 0xFF));
}

// Table of functions to write to an I/O port, indexed by data width.
Y_FILE_PRIVATE void (*const YCPUWriteToPortFns[])(
    YCPUState*, uint16_t, YCPUOperandValue) = {
    YCPUWriteByteToPort,  // Y_CPU_BYTE
    YCPUWriteWordToPort,  // Y_CPU_WORD
};

// Common logic for OUT instructions.
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteOut(const YCPUInstructionContext* ctx, uint16_t port) {
  YCPUOperand src;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &src);
  YCPUWriteToPortFns[ctx->metadata->width](ctx->cpu, port, src.value);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// OUT imm8, AL
// OUT imm8, AX
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteOutImmediate(const YCPUInstructionContext* ctx) {
  YCPUOperandValue port = YCPUReadImmediateOperandByte(ctx->instruction);
  return YCPUExecuteOut(ctx, YCPUFromOperandValue(port));
}

// OUT DX, AL
// OUT DX, AX
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteOutDX(const YCPUInstructionContext* ctx) {
  return YCPUExecuteOut(ctx, ctx->cpu->registers[Y_CPU_DX]);
}


// ==============================================================================
// src/cpu/instructions_io.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_string.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_string.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "cycles.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// String instructions
// ============================================================================

// Get the repetition prefix of a string instruction, if any.
Y_FILE_PRIVATE inline uint8_t YCPUGetRepetitionPrefix(
    const YCPUInstructionContext* ctx) {
  return ctx->instruction->repetition_prefix;
}

// Get the source operand for string instructions. Typically DS:SI but can be
// overridden by a segment override prefix.
Y_FILE_PRIVATE void YCPUGetStringSourceOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand) {
  YCPUOperandAddress address = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
      .register_index = Y_CPU_DS,
      .offset = ctx->cpu->registers[Y_CPU_SI],
  };
  YCPUApplySegmentOverride(ctx->instruction, &address.register_index);
  operand->address = address;
  operand->value = YCPUReadOperandValue(ctx, &address);
}

// Get the destination operand address for string instructions. Always ES:DI.
Y_FILE_PRIVATE YCPUOperandAddress
YCPUGetStringDestinationOperandAddress(const YCPUInstructionContext* ctx) {
  YCPUOperandAddress address = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
      .register_index = Y_CPU_ES,
      .offset = ctx->cpu->registers[Y_CPU_DI],
  };
  return address;
}

// Get the destination operand for string instructions. Always ES:DI.
Y_FILE_PRIVATE void YCPUGetStringDestinationOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand) {
  YCPUOperandAddress address = YCPUGetStringDestinationOperandAddress(ctx);
  operand->address = address;
  operand->value = YCPUReadOperandValue(ctx, &address);
}

// Update the source address register (SI) after a string operation.
Y_FILE_PRIVATE void YCPUUpdateStringSourceAddress(
    const YCPUInstructionContext* ctx) {
  if (YCPUGetFlag(ctx->cpu, Y_CPU_DF)) {
    ctx->cpu->registers[Y_CPU_SI] -= YCPUNumBytes[ctx->metadata->width];
  } else {
    ctx->cpu->registers[Y_CPU_SI] += YCPUNumBytes[ctx->metadata->width];
  }
}

// Update the destination address register (DI) after a string operation.
Y_FILE_PRIVATE void YCPUUpdateStringDestinationAddress(
    const YCPUInstructionContext* ctx) {
  if (YCPUGetFlag(ctx->cpu, Y_CPU_DF)) {
    ctx->cpu->registers[Y_CPU_DI] -= YCPUNumBytes[ctx->metadata->width];
  } else {
    ctx->cpu->registers[Y_CPU_DI] += YCPUNumBytes[ctx->metadata->width];
  }
}

// ============================================================================
// Bulk runs
// ============================================================================

// A repeated MOVS or STOS carried out as a run over guest memory, rather than
// an element at a time through the operand machinery.
//
// An iteration of the element path builds an operand address, resolves a
// segment through it, dispatches on width and charges the bus, twice over, to
// move two bytes - and calls through a function pointer to do it. Where every
// byte the repeat touches lies in the window the CPU indexes directly, the
// whole repeat is a loop over that window instead.
//
// Three things hold for every loop below. Each width gets its own loop rather
// than one loop testing the width, since that test would otherwise be made
// once per element on a part with no branch predictor to hide it. Each visits
// elements in the order the element path visits them, which is what makes a
// repeat whose two ends overlap come out the same. And each walks a pointer
// rather than indexing an array, which is what keeps the compiler from
// rewriting it: written as memory[index], gcc -O3 recognizes the byte-wide
// STOS loop and emits a call to memset(). That measures the same either way,
// so what the index form costs is not speed but a link-time dependency on libc
// the core does not otherwise have.
//
// The copy loops are out of reach of that transform for a second reason: the
// step is a runtime value the compiler cannot prove positive, so it cannot
// conclude the two ranges do not overlap. That one is load-bearing. A memcpy()
// may copy in any order, which is precisely what the overlapping cases depend
// on it not doing, so specializing a loop on a forward direction would hand
// the compiler the proof it needs to introduce one.
typedef struct YCPUBulkStringRun {
  // Linear address of the first element at each end. source is left alone for
  // STOS, which reads no memory.
  uint32_t source;
  uint32_t destination;
  // Distance from one element to the next, negative while DF is set.
  int32_t step;
  uint16_t count;
  uint8_t element_size;
} YCPUBulkStringRun;

// Whether every byte a run touches is reachable by indexing the direct data
// window, starting from offset within its segment and linear within memory.
//
// Two things can put a byte out of reach, and both have to be ruled out. The
// offset is 16 bits and wraps within its segment, where a linear address does
// not, so a run that wraps carries on somewhere a straight run over memory
// would not reach - the element path recomputes the address from the wrapped
// offset every iteration. And a run that leaves the window has to go through
// the memory map for the part outside it.
Y_FILE_PRIVATE bool YCPUIsBulkRunAddressable(
    uint32_t window_end, uint16_t offset, uint32_t linear, uint16_t count,
    uint8_t element_size, bool backwards) {
  // Distance from the first element to the last, which is one element short of
  // the whole run.
  const uint32_t reach = (uint32_t)(count - 1) * element_size;
  if (backwards) {
    // Elements run downwards, so the last one is the lowest and the first one
    // reaches highest.
    return (uint32_t)offset + element_size <= Y_CPU_SEGMENT_SIZE &&
           (uint32_t)offset >= reach && linear >= reach &&
           linear + element_size <= window_end;
  }
  return (uint32_t)offset + reach + element_size <= Y_CPU_SEGMENT_SIZE &&
         linear + reach + element_size <= window_end;
}

// Works out whether a repeat can run in bulk, and describes it if so.
Y_FILE_PRIVATE bool YCPUPlanBulkStringRun(
    const YCPUInstructionContext* ctx, bool has_source,
    YCPUBulkStringRun* run) {
  const uint8_t prefix = YCPUGetRepetitionPrefix(ctx);
  if (prefix != Y_CPU_PREFIX_REP && prefix != Y_CPU_PREFIX_REPNZ) {
    return false;
  }
  YCPUState* const cpu = ctx->cpu;
  const uint16_t count = cpu->registers[Y_CPU_CX];
  // end is 0 exactly when the window is closed, so this is the whole test for
  // whether there is a window to index.
  const uint32_t window_end = cpu->direct_data_window.end;
  if (count == 0 || window_end == 0) {
    return false;
  }
  const uint8_t element_size = YCPUNumBytes[ctx->metadata->width];
  const bool backwards = YCPUGetFlag(cpu, Y_CPU_DF);

  const uint16_t destination_offset = cpu->registers[Y_CPU_DI];
  const uint32_t destination =
      YCPUToRawAddress(cpu, Y_CPU_ES, destination_offset);
  if (!YCPUIsBulkRunAddressable(
          window_end, destination_offset, destination, count, element_size,
          backwards)) {
    return false;
  }

  if (has_source) {
    uint8_t source_segment = Y_CPU_DS;
    YCPUApplySegmentOverride(ctx->instruction, &source_segment);
    const uint16_t source_offset = cpu->registers[Y_CPU_SI];
    const uint32_t source =
        YCPUToRawAddress(cpu, source_segment, source_offset);
    if (!YCPUIsBulkRunAddressable(
            window_end, source_offset, source, count, element_size,
            backwards)) {
      return false;
    }
    run->source = source;
  }

  run->destination = destination;
  run->step = backwards ? -(int32_t)element_size : (int32_t)element_size;
  run->count = count;
  run->element_size = element_size;
  return true;
}

// Tells the decode cache that the run has written over whatever was decoded
// from the pages it covers.
//
// The element path reports every byte through YCPUWriteRawMemoryByte(). A
// page's counter only has to *change* for a decode taken from it to be
// discarded, not to change a particular number of times, so one report per page
// says the same thing - and leaves the counter coming back round after 256 runs
// rather than after 256 bytes, which is a flush avoided rather than a flush
// missed.
Y_FILE_PRIVATE void YCPUNotifyBulkStringWrite(
    YCPUState* cpu, const YCPUBulkStringRun* run) {
  const uint32_t reach = (uint32_t)(run->count - 1) * run->element_size;
  const uint32_t low =
      run->step < 0 ? run->destination - reach : run->destination;
  const uint32_t high = low + reach + run->element_size - 1;
  for (uint32_t address = low & ~(uint32_t)Y_CPU_CODE_PAGE_OFFSET_MASK;
       address <= high; address += Y_CPU_CODE_PAGE_SIZE) {
    YCPUNotifyMemoryWrite(cpu, address);
  }
}

// Leaves CX, SI and DI where the element path would have left them, and
// charges what it would have charged.
Y_FILE_PRIVATE void YCPUFinishBulkStringRun(
    YCPUState* cpu, const YCPUBulkStringRun* run, uint8_t bus_bytes_per_element,
    bool has_source) {
  const int32_t total = run->step * (int32_t)run->count;
  if (has_source) {
    cpu->registers[Y_CPU_SI] =
        (uint16_t)((int32_t)cpu->registers[Y_CPU_SI] + total);
  }
  cpu->registers[Y_CPU_DI] =
      (uint16_t)((int32_t)cpu->registers[Y_CPU_DI] + total);
  cpu->registers[Y_CPU_CX] = 0;
  // The element path charges this through YCPUAddBusCycles() once per access,
  // into a counter 16 bits wide that a long repeat runs past the top of.
  // Summing first and adding once wraps exactly where adding one at a time
  // wraps, which is what keeps the two costing the same.
  cpu->pending_cycles +=
      (uint16_t)((uint32_t)run->count * bus_bytes_per_element *
                 Y_CPU_BUS_CYCLES_PER_BYTE);
}

// Execute a string instruction with optional REP prefix.
//
// MOVS, STOS and LODS set no flags, so a repetition prefix has no zero flag to
// test and the 8086/8088 does not tell the two prefixes apart here: 0xF2
// repeats exactly as 0xF3 does, counting CX down to zero. Only the comparison
// string instructions read the prefix as a condition.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteStringInstructionWithREPPrefix(
    const YCPUInstructionContext* ctx,
    YCPUInstructionResult (*fn)(const YCPUInstructionContext*)) {
  uint8_t prefix = YCPUGetRepetitionPrefix(ctx);
  if (prefix != Y_CPU_PREFIX_REP && prefix != Y_CPU_PREFIX_REPNZ) {
    return fn(ctx);
  }
  while (ctx->cpu->registers[Y_CPU_CX]) {
    YCPUInstructionResult status = fn(ctx);
    if (status != Y_CPU_INSTRUCTION_EXECUTED) {
      return status;
    }
    --ctx->cpu->registers[Y_CPU_CX];
  }
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Single MOVS iteration.
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteMovsIteration(const YCPUInstructionContext* ctx) {
  YCPUOperand src;
  YCPUGetStringSourceOperand(ctx, &src);
  YCPUOperandAddress dest_address = YCPUGetStringDestinationOperandAddress(ctx);
  YCPUWriteOperandAddress(ctx, &dest_address, YCPUFromOperand(&src));
  YCPUUpdateStringSourceAddress(ctx);
  YCPUUpdateStringDestinationAddress(ctx);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// A whole repeated MOVS as one run, where the repeat qualifies for one.
// Returns whether it did.
Y_FILE_PRIVATE bool YCPUExecuteMovsBulkRun(const YCPUInstructionContext* ctx) {
  YCPUBulkStringRun run;
  if (!YCPUPlanBulkStringRun(ctx, /*has_source=*/true, &run)) {
    return false;
  }
  YCPUState* const cpu = ctx->cpu;
  uint8_t* const memory = cpu->direct_data_window.data;
  const uint8_t* source = memory + run.source;
  uint8_t* destination = memory + run.destination;
  if (run.element_size == YCPUNumBytes[Y_CPU_WORD]) {
    for (uint16_t remaining = run.count; remaining > 0; --remaining) {
      // Both bytes are read before either is written, which is the order the
      // element path reads and writes a word in - and is what a copy whose
      // ends overlap by exactly one byte comes out of differently otherwise.
      const uint8_t low = source[0];
      const uint8_t high = source[1];
      destination[0] = low;
      destination[1] = high;
      source += run.step;
      destination += run.step;
    }
  } else {
    for (uint16_t remaining = run.count; remaining > 0; --remaining) {
      *destination = *source;
      source += run.step;
      destination += run.step;
    }
  }
  YCPUNotifyBulkStringWrite(cpu, &run);
  // Every byte is read and written.
  YCPUFinishBulkStringRun(
      cpu, &run, (uint8_t)(run.element_size * 2), /*has_source=*/true);
  return true;
}

// MOVS
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteMovs(const YCPUInstructionContext* ctx) {
  if (YCPUExecuteMovsBulkRun(ctx)) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  return YCPUExecuteStringInstructionWithREPPrefix(
      ctx, YCPUExecuteMovsIteration);
}

// Single STOS iteration.
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteStosIteration(const YCPUInstructionContext* ctx) {
  YCPUOperand src;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &src);
  YCPUOperandAddress dest_address = YCPUGetStringDestinationOperandAddress(ctx);
  YCPUWriteOperandAddress(ctx, &dest_address, YCPUFromOperand(&src));
  YCPUUpdateStringDestinationAddress(ctx);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// A whole repeated STOS as one run, where the repeat qualifies for one.
// Returns whether it did.
Y_FILE_PRIVATE bool YCPUExecuteStosBulkRun(const YCPUInstructionContext* ctx) {
  YCPUBulkStringRun run;
  if (!YCPUPlanBulkStringRun(ctx, /*has_source=*/false, &run)) {
    return false;
  }
  YCPUState* const cpu = ctx->cpu;
  uint8_t* const memory = cpu->direct_data_window.data;
  const uint16_t value = cpu->registers[Y_CPU_AX];
  uint8_t* destination = memory + run.destination;
  if (run.element_size == YCPUNumBytes[Y_CPU_WORD]) {
    for (uint16_t remaining = run.count; remaining > 0; --remaining) {
      destination[0] = (uint8_t)value;
      destination[1] = (uint8_t)(value >> 8);
      destination += run.step;
    }
  } else {
    // A byte-wide STOS stores AL, which is the low byte of the same register.
    for (uint16_t remaining = run.count; remaining > 0; --remaining) {
      *destination = (uint8_t)value;
      destination += run.step;
    }
  }
  YCPUNotifyBulkStringWrite(cpu, &run);
  // Every byte is written, and none is read.
  YCPUFinishBulkStringRun(cpu, &run, run.element_size, /*has_source=*/false);
  return true;
}

// STOS
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteStos(const YCPUInstructionContext* ctx) {
  if (YCPUExecuteStosBulkRun(ctx)) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  return YCPUExecuteStringInstructionWithREPPrefix(
      ctx, YCPUExecuteStosIteration);
}

// Single LODS iteration.
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteLodsIteration(const YCPUInstructionContext* ctx) {
  YCPUOperand src;
  YCPUGetStringSourceOperand(ctx, &src);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperand(&src));
  YCPUUpdateStringSourceAddress(ctx);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// LODS
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteLods(const YCPUInstructionContext* ctx) {
  return YCPUExecuteStringInstructionWithREPPrefix(
      ctx, YCPUExecuteLodsIteration);
}

// Execute a string instruction with optional REPZ/REPE or REPNZ/REPNE prefix.
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteStringInstructionWithREPZOrRepNZPrefix(
    const YCPUInstructionContext* ctx,
    YCPUInstructionResult (*fn)(const YCPUInstructionContext*)) {
  uint8_t prefix = YCPUGetRepetitionPrefix(ctx);
  if (prefix != Y_CPU_PREFIX_REP && prefix != Y_CPU_PREFIX_REPNZ) {
    return fn(ctx);
  }
  bool terminate_zf_value = prefix == Y_CPU_PREFIX_REPNZ;
  while (ctx->cpu->registers[Y_CPU_CX]) {
    YCPUInstructionResult status = fn(ctx);
    if (status != Y_CPU_INSTRUCTION_EXECUTED) {
      return status;
    }
    --ctx->cpu->registers[Y_CPU_CX];
    if (YCPUGetFlag(ctx->cpu, Y_CPU_ZF) == terminate_zf_value) {
      break;
    }
  }
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Single SCAS iteration.
Y_FILE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteScasIteration(const YCPUInstructionContext* ctx) {
  YCPUOperand src;
  YCPUGetStringDestinationOperand(ctx, &src);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUExecuteCmp(ctx, &dest, src.value);
  YCPUUpdateStringDestinationAddress(ctx);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// SCAS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteScas(const YCPUInstructionContext* ctx) {
  return YCPUExecuteStringInstructionWithREPZOrRepNZPrefix(
      ctx, YCPUExecuteScasIteration);
}

// Single CMPS iteration.
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteCmpsIteration(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUGetStringSourceOperand(ctx, &dest);
  YCPUOperand src;
  YCPUGetStringDestinationOperand(ctx, &src);
  YCPUExecuteCmp(ctx, &dest, src.value);
  YCPUUpdateStringSourceAddress(ctx);
  YCPUUpdateStringDestinationAddress(ctx);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// CMPS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCmps(const YCPUInstructionContext* ctx) {
  return YCPUExecuteStringInstructionWithREPZOrRepNZPrefix(
      ctx, YCPUExecuteCmpsIteration);
}


// ==============================================================================
// src/cpu/instructions_string.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_bcd_ascii.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_bcd_ascii.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// BCD and ASCII arithmetic instructions
// ============================================================================

// The value DAA and DAS compare AL against to decide whether the high digit
// needs adjusting. Both test AL as it was on entry, before the low digit was
// adjusted, so a carry out of the low digit does not drag the high one along
// with it.
//
// The limit is one higher when the auxiliary carry flag was already set on
// entry. That is not what Intel's published pseudocode says, which uses 0x99
// throughout, but it is what the 8086/8088 does - and with AL between 0x9A and
// 0x9F it is the difference between adjusting and not.
Y_FILE_PRIVATE uint8_t YCPUGetBCDHighDigitLimit(bool auxiliary_carry) {
  return auxiliary_carry ? 0x9F : 0x99;
}

// AAA
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAaa(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[Y_CPU_AX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[Y_CPU_AX] >> 8) & 0xFF;
  uint8_t al_low = al & 0x0F;
  if (al_low > 9 || YCPUGetFlag(ctx->cpu, Y_CPU_AF)) {
    al += 6;
    ++ah;
    YCPUSetFlag(ctx->cpu, Y_CPU_AF, true);
    YCPUSetFlag(ctx->cpu, Y_CPU_CF, true);
  } else {
    YCPUSetFlag(ctx->cpu, Y_CPU_AF, false);
    YCPUSetFlag(ctx->cpu, Y_CPU_CF, false);
  }
  al &= 0x0F;
  ctx->cpu->registers[Y_CPU_AX] = (ah << 8) | al;
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// AAS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAas(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[Y_CPU_AX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[Y_CPU_AX] >> 8) & 0xFF;
  uint8_t al_low = al & 0x0F;
  if (al_low > 9 || YCPUGetFlag(ctx->cpu, Y_CPU_AF)) {
    al -= 6;
    --ah;
    YCPUSetFlag(ctx->cpu, Y_CPU_AF, true);
    YCPUSetFlag(ctx->cpu, Y_CPU_CF, true);
  } else {
    YCPUSetFlag(ctx->cpu, Y_CPU_AF, false);
    YCPUSetFlag(ctx->cpu, Y_CPU_CF, false);
  }
  al &= 0x0F;
  ctx->cpu->registers[Y_CPU_AX] = (ah << 8) | al;
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// AAM
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAam(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[Y_CPU_AX] & 0xFF;
  YCPUOperandValue base = YCPUReadImmediate(ctx);
  uint16_t base_value = YCPUFromOperandValue(base);
  if (base_value == 0) {
    // AAM divides by its immediate operand, so a base of 0 raises a divide
    // error just like DIV by zero does, rather than being an invalid encoding.
    // The division never produces a result, and the flags are left set as
    // though it had produced zero.
    YCPUSetCommonFlagsAfterInstruction(ctx, 0);
    YCPUSetFlag(ctx->cpu, Y_CPU_CF, false);
    YCPUSetFlag(ctx->cpu, Y_CPU_AF, false);
    YCPUSetFlag(ctx->cpu, Y_CPU_OF, false);
    YCPURaiseInternalInterrupt(ctx->cpu, Y_CPU_INTERRUPT_DIVIDE_ERROR);
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  uint8_t ah = al / base_value;
  al %= base_value;
  ctx->cpu->registers[Y_CPU_AX] = (ah << 8) | al;
  YCPUSetCommonFlagsAfterInstruction(ctx, al);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// AAD
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAad(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[Y_CPU_AX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[Y_CPU_AX] >> 8) & 0xFF;
  YCPUOperandValue base = YCPUReadImmediate(ctx);
  uint8_t base_value = YCPUFromOperandValue(base);
  al += ah * base_value;
  ah = 0;
  ctx->cpu->registers[Y_CPU_AX] = (ah << 8) | al;
  YCPUSetCommonFlagsAfterInstruction(ctx, al);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// DAA
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDaa(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[Y_CPU_AX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[Y_CPU_AX] >> 8) & 0xFF;
  const uint8_t original_al = al;
  const bool original_carry = YCPUGetFlag(ctx->cpu, Y_CPU_CF);
  const uint8_t high_digit_limit =
      YCPUGetBCDHighDigitLimit(YCPUGetFlag(ctx->cpu, Y_CPU_AF));

  if ((al & 0x0F) > 9 || YCPUGetFlag(ctx->cpu, Y_CPU_AF)) {
    al += 6;
    YCPUSetFlag(ctx->cpu, Y_CPU_AF, true);
  } else {
    YCPUSetFlag(ctx->cpu, Y_CPU_AF, false);
  }
  if (original_al > high_digit_limit || original_carry) {
    al += 0x60;
    YCPUSetFlag(ctx->cpu, Y_CPU_CF, true);
  } else {
    YCPUSetFlag(ctx->cpu, Y_CPU_CF, false);
  }
  ctx->cpu->registers[Y_CPU_AX] = (ah << 8) | al;
  YCPUSetCommonFlagsAfterInstruction(ctx, al);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// DAS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDas(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[Y_CPU_AX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[Y_CPU_AX] >> 8) & 0xFF;
  const uint8_t original_al = al;
  const bool original_carry = YCPUGetFlag(ctx->cpu, Y_CPU_CF);
  const uint8_t high_digit_limit =
      YCPUGetBCDHighDigitLimit(YCPUGetFlag(ctx->cpu, Y_CPU_AF));

  if ((al & 0x0F) > 9 || YCPUGetFlag(ctx->cpu, Y_CPU_AF)) {
    al -= 6;
    YCPUSetFlag(ctx->cpu, Y_CPU_AF, true);
  } else {
    YCPUSetFlag(ctx->cpu, Y_CPU_AF, false);
  }
  if (original_al > high_digit_limit || original_carry) {
    al -= 0x60;
    YCPUSetFlag(ctx->cpu, Y_CPU_CF, true);
  } else {
    YCPUSetFlag(ctx->cpu, Y_CPU_CF, false);
  }
  ctx->cpu->registers[Y_CPU_AX] = (ah << 8) | al;
  YCPUSetCommonFlagsAfterInstruction(ctx, al);
  return Y_CPU_INSTRUCTION_EXECUTED;
}


// ==============================================================================
// src/cpu/instructions_bcd_ascii.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_group_1.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_group_1.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 1 - ADD, OR, ADC, SBB, AND, SUB, XOR, CMP
// ============================================================================

typedef YCPUInstructionResult (*YCPUGroup1ExecuteInstructionFn)(
    const YCPUInstructionContext* ctx, YCPUOperand* dest, YCPUOperandValue src);

// Group 1 instruction implementations, indexed by the corresponding REG field
// value in the YCPUModRM byte.
Y_FILE_PRIVATE const YCPUGroup1ExecuteInstructionFn
    YCPUGroup1ExecuteInstructionFns[] = {
        YCPUExecuteAdd,            // 0 - ADD
        YCPUExecuteBooleanOr,      // 1 - OR
        YCPUExecuteAddWithCarry,   // 2 - ADC
        YCPUExecuteSubWithBorrow,  // 3 - SBB
        YCPUExecuteBooleanAnd,     // 4 - AND
        YCPUExecuteSub,            // 5 - SUB
        YCPUExecuteBooleanXor,     // 6 - XOR
        YCPUExecuteCmp,            // 7 - CMP
};

// Group 1 instruction handler.
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteGroup1Instruction(const YCPUInstructionContext* ctx) {
  const YCPUGroup1ExecuteInstructionFn fn =
      YCPUGroup1ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return fn(ctx, &dest, src_value);
}

// Group 1 instruction handler, but sign-extends the 8-bit immediate value.
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteGroup1InstructionWithSignExtension(
    const YCPUInstructionContext* ctx) {
  const YCPUGroup1ExecuteInstructionFn fn =
      YCPUGroup1ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperandValue src_value = YCPUReadImmediateOperandByte(
      ctx->instruction);  // immediate is always 8-bit
  YCPUOperandValue src_value_extended =
      (YCPUOperandValue)((int16_t)((int8_t)src_value));
  // Sign-extend the immediate value to the destination width.
  return fn(ctx, &dest, src_value_extended);
}


// ==============================================================================
// src/cpu/instructions_group_1.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_group_2.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_group_2.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "cycles.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 2 - ROL, ROR, RCL, RCR, SHL, SHR, SAL, SAR
// ============================================================================

typedef YCPUInstructionResult (*YCPUGroup2ExecuteInstructionFn)(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count);

// The 8086/8088 shifts one bit at a time and recomputes the overflow flag on
// every pass, so a multi-bit shift leaves behind the flag from its last pass
// rather than an undefined value. Each rule below is that last pass written in
// closed form.

// Overflow after a left shift or rotate: the bit shifted out of the top
// differs from the sign bit left behind, which is to say the last pass changed
// the sign of the value.
Y_FILE_PRIVATE void YCPUSetOverflowFlagAfterLeftShift(
    const YCPUInstructionContext* ctx, uint32_t result, bool carry) {
  const bool result_sign = (result & YCPUSignBit[ctx->metadata->width]) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_OF, carry != result_sign);
}

// Overflow after a right rotate: the top two bits of the result differ. The
// bit rotated into the top came from the bottom, so this again says the last
// pass changed the sign of the value.
Y_FILE_PRIVATE void YCPUSetOverflowFlagAfterRightRotate(
    const YCPUInstructionContext* ctx, uint32_t result) {
  const uint32_t sign_bit = YCPUSignBit[ctx->metadata->width];
  const bool result_sign = (result & sign_bit) != 0;
  const bool below_sign = (result & (sign_bit >> 1)) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_OF, result_sign != below_sign);
}

// The 8086/8088 does not mask the shift count the way the 80186 and later do,
// so a count taken from CL can be as large as 255. Once an operand has been
// shifted past its own width there is nothing left in it and nothing left to
// fall out of it, so every larger count behaves alike. Clamping to just past
// the width keeps that true while holding the shifts below the width of the
// intermediate they are computed in, where C leaves them undefined.
Y_FILE_PRIVATE uint8_t
YCPUClampShiftCount(const YCPUInstructionContext* ctx, uint8_t count) {
  const uint8_t limit = YCPUNumBits[ctx->metadata->width] + 1;
  return count > limit ? limit : count;
}

// SHL r/m8, 1
// SHL r/m16, 1
// SHL r/m8, CL
// SHL r/m16, CL
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Shl(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  count = YCPUClampShiftCount(ctx, count);
  uint32_t value = YCPUFromOperand(op);
  uint32_t result = (value << count) & YCPUMaxValue[ctx->metadata->width];
  YCPUWriteOperand(ctx, op, result);
  bool last_msb =
      ((value << (count - 1)) & YCPUSignBit[ctx->metadata->width]) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_msb);
  YCPUSetOverflowFlagAfterLeftShift(ctx, result, last_msb);
  YCPUSetCommonFlagsAfterInstruction(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// SHR r/m8, 1
// SHR r/m16, 1
// SHR r/m8, CL
// SHR r/m16, CL
Y_FILE_PRIVATE Y_HOT YCPUInstructionResult YCPUExecuteGroup2Shr(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  count = YCPUClampShiftCount(ctx, count);
  uint32_t value = YCPUFromOperand(op);
  uint32_t result = value >> count;
  YCPUWriteOperand(ctx, op, result);
  bool last_lsb = ((value >> (count - 1)) & 1) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_lsb);
  // A right shift clears the sign bit on its first pass, so only a shift by
  // one can leave a sign change behind.
  bool original_msb = ((value & YCPUSignBit[ctx->metadata->width]) != 0);
  YCPUSetFlag(ctx->cpu, Y_CPU_OF, count == 1 && original_msb);
  YCPUSetCommonFlagsAfterInstruction(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// SAR r/m8, 1
// SAR r/m16, 1
// SAR r/m8, CL
// SAR r/m16, CL
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Sar(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  count = YCPUClampShiftCount(ctx, count);
  int32_t value = YCPUFromSignedOperand(ctx->metadata->width, op);
  int32_t result = value >> count;
  YCPUWriteOperand(ctx, op, result);
  bool last_lsb = ((value >> (count - 1)) & 1) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_lsb);
  // An arithmetic right shift preserves the sign bit, so it can never
  // overflow.
  YCPUSetFlag(ctx->cpu, Y_CPU_OF, false);
  YCPUSetCommonFlagsAfterInstruction(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ROL r/m8, 1
// ROL r/m16, 1
// ROL r/m8, CL
// ROL r/m16, CL
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Rol(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  // The 8086 computes the modulus of the count after the zero check, which is
  // different than the 80286 and later processors.
  uint8_t effective_count = count % YCPUNumBits[ctx->metadata->width];
  uint32_t value = YCPUFromOperand(op);
  uint32_t result =
      (value << effective_count) |
      (value >> (YCPUNumBits[ctx->metadata->width] - effective_count));
  YCPUWriteOperand(ctx, op, result);
  bool last_msb = (result & 1) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_msb);
  YCPUSetOverflowFlagAfterLeftShift(ctx, result, last_msb);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ROR r/m8, 1
// ROR r/m16, 1
// ROR r/m8, CL
// ROR r/m16, CL
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Ror(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  // The 8086 computes the modulus of the count after the zero check, which is
  // different than the 80286 and later processors.
  uint8_t effective_count = count % YCPUNumBits[ctx->metadata->width];
  uint32_t value = YCPUFromOperand(op);
  uint32_t result =
      (value >> effective_count) |
      (value << (YCPUNumBits[ctx->metadata->width] - effective_count));
  YCPUWriteOperand(ctx, op, result);
  bool last_lsb = (result & YCPUSignBit[ctx->metadata->width]) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_lsb);
  YCPUSetOverflowFlagAfterRightRotate(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// RCL r/m8, 1
// RCL r/m16, 1
// RCL r/m8, CL
// RCL r/m16, CL
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Rcl(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  const YCPUWidth width = ctx->metadata->width;
  // Rotating through the carry flag cycles over one more bit than the operand
  // is wide. A whole number of cycles puts the value and the carry back the
  // way they were, but the passes still happened, so the overflow flag is
  // still left over from the last one.
  uint8_t effective_count = count % (YCPUNumBits[width] + 1);
  uint32_t value = YCPUFromOperand(op);
  uint32_t result = value;
  bool last_msb = YCPUGetFlag(ctx->cpu, Y_CPU_CF);
  if (effective_count > 0) {
    uint32_t cf_value = last_msb ? (1 << (effective_count - 1)) : 0;
    result = ((value << effective_count) | cf_value |
              (value >> (YCPUNumBits[width] - (effective_count - 1)))) &
             YCPUMaxValue[width];
    YCPUWriteOperand(ctx, op, result);
    last_msb = ((value << (effective_count - 1)) & YCPUSignBit[width]) != 0;
  }
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_msb);
  YCPUSetOverflowFlagAfterLeftShift(ctx, result, last_msb);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// RCR r/m8, 1
// RCR r/m16, 1
// RCR r/m8, CL
// RCR r/m16, CL
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Rcr(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  const YCPUWidth width = ctx->metadata->width;
  // As with RCL, a whole number of cycles restores the value and the carry but
  // still leaves the overflow flag from the last pass.
  uint8_t effective_count = count % (YCPUNumBits[width] + 1);
  uint32_t value = YCPUFromOperand(op);
  uint32_t result = value;
  bool last_lsb = YCPUGetFlag(ctx->cpu, Y_CPU_CF);
  if (effective_count > 0) {
    uint32_t cf_value =
        last_lsb ? (YCPUSignBit[width] >> (effective_count - 1)) : 0;
    result = ((value >> effective_count) | cf_value |
              (value << (YCPUNumBits[width] - (effective_count - 1)))) &
             YCPUMaxValue[width];
    YCPUWriteOperand(ctx, op, result);
    last_lsb = ((value >> (effective_count - 1)) & 1) != 0;
  }
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_lsb);
  YCPUSetOverflowFlagAfterRightRotate(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// SETMO r/m8, 1
// SETMO r/m16, 1
// SETMOC r/m8, CL
// SETMOC r/m16, CL
//
// Undocumented. REG 6 of the shift group is a distinct operation on the
// 8086/8088 rather than an alias of SAL: it sets every bit of the operand,
// hence "set minus one". The count still gates it, so the CL forms - SETMOC,
// "set minus one conditional" - do nothing at all when CL is zero.
//
// Nothing an IBM PC/XT runs uses this, but leaving REG 6 aliased to SAL would
// silently give a different answer than the hardware for the same encoding,
// and the operation is a single store.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Setmo(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  const uint32_t result = YCPUMaxValue[ctx->metadata->width];
  YCPUWriteOperand(ctx, op, result);
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, false);
  YCPUSetFlag(ctx->cpu, Y_CPU_AF, false);
  YCPUSetFlag(ctx->cpu, Y_CPU_OF, false);
  YCPUSetCommonFlagsAfterInstruction(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

Y_FILE_PRIVATE const YCPUGroup2ExecuteInstructionFn
    YCPUGroup2ExecuteInstructionFns[] = {
        YCPUExecuteGroup2Rol,    // 0 - ROL
        YCPUExecuteGroup2Ror,    // 1 - ROR
        YCPUExecuteGroup2Rcl,    // 2 - RCL
        YCPUExecuteGroup2Rcr,    // 3 - RCR
        YCPUExecuteGroup2Shl,    // 4 - SHL
        YCPUExecuteGroup2Shr,    // 5 - SHR
        YCPUExecuteGroup2Setmo,  // 6 - SETMO / SETMOC
        YCPUExecuteGroup2Sar,    // 7 - SAR
};

// Group 2 shift / rotate by 1.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup2ShiftOrRotateBy1Instruction(
    const YCPUInstructionContext* ctx) {
  const YCPUGroup2ExecuteInstructionFn fn =
      YCPUGroup2ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand op;
  YCPUReadRegisterOrMemoryOperand(ctx, &op);
  return fn(ctx, &op, 1);
}

// Group 2 shift / rotate by CL.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup2ShiftOrRotateByCLInstruction(
    const YCPUInstructionContext* ctx) {
  const YCPUGroup2ExecuteInstructionFn fn =
      YCPUGroup2ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand op;
  YCPUReadRegisterOrMemoryOperand(ctx, &op);
  const uint8_t count = ctx->cpu->registers[Y_CPU_CX] & 0xFF;
  // A shift by CL works through the count a bit at a time.
  YCPUAddCycles(ctx->cpu, (uint16_t)count * Y_CPU_SHIFT_CYCLES_PER_BIT);
  return fn(ctx, &op, count);
}


// ==============================================================================
// src/cpu/instructions_group_2.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_group_3.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_group_3.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 3 - TEST, NOT, NEG, MUL, IMUL, DIV, IDIV
// ============================================================================

typedef YCPUInstructionResult (*YCPUGroup3ExecuteInstructionFn)(
    const YCPUInstructionContext* ctx, YCPUOperand* op);

// TEST r/m8, imm8
// TEST r/m16, imm16
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup3Test(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteTest(ctx, op, src_value);
}

// NOT r/m8
// NOT r/m16
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteNot(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  YCPUWriteOperand(ctx, op, ~YCPUFromOperand(op));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// NEG r/m8
// NEG r/m16
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteNeg(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  int32_t op_value = YCPUFromSignedOperand(ctx->metadata->width, op);
  int32_t result_value = -op_value;
  YCPUWriteOperand(ctx, op, result_value);
  YCPUSetFlagsAfterSub(ctx, 0, op_value, result_value, false);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Table of where to store the higher half of the result for
// MUL, IMUL, DIV, and IDIV instructions, indexed by the data width.
Y_FILE_PRIVATE const YCPUOperandAddress
    YCPUMulDivResultHighHalfAddress[Y_CPU_NUM_WIDTHS] = {
        {.type = Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER,
         .register_index = Y_CPU_AX,
         .offset = 8},
        {.type = Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER,
         .register_index = Y_CPU_DX,
         .offset = 0},
};

// Number of bits to shift to extract the high part of the result of MUL, IMUL,
// DIV, and IDIV instructions, indexed by the data width.
Y_FILE_PRIVATE const uint8_t
    YCPUMulDivResultHighHalfShiftWidth[Y_CPU_NUM_WIDTHS] = {
        8,   // Y_CPU_BYTE
        16,  // Y_CPU_WORD
};

// What the multiply and divide instructions cost. These dominate their own
// timing by an order of magnitude - the 8086/8088 works through them a bit at
// a time in microcode - so unlike everything else in the cycle table they are
// worth charging individually. The published figures are ranges that depend on
// the operands; these are the low end of each.
Y_FILE_PRIVATE const uint16_t YCPUMulDivCycles[Y_CPU_NUM_WIDTHS][2] = {
    // Y_CPU_BYTE: unsigned, signed
    {70, 80},
    // Y_CPU_WORD: unsigned, signed
    {118, 128},
};

// Common logic for MUL and IMUL instructions.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteMulCommon(
    const YCPUInstructionContext* ctx, YCPUOperand* dest, uint32_t result,
    bool overflow) {
  YCPUWidth width = ctx->metadata->width;

  uint32_t result_low_half = result & YCPUMaxValue[width];
  YCPUWriteOperand(ctx, dest, result_low_half);

  uint32_t result_high_half =
      (result >> YCPUMulDivResultHighHalfShiftWidth[width]) &
      YCPUMaxValue[width];
  YCPUWriteOperandAddress(
      ctx, &YCPUMulDivResultHighHalfAddress[width], result_high_half);

  YCPUSetFlag(ctx->cpu, Y_CPU_CF, overflow);
  YCPUSetFlag(ctx->cpu, Y_CPU_OF, overflow);

  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MUL r/m8
// MUL r/m16
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteMul(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  YCPUAddCycles(ctx->cpu, YCPUMulDivCycles[ctx->metadata->width][0]);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  uint32_t result = YCPUFromOperand(&dest) * YCPUFromOperand(op);
  return YCPUExecuteMulCommon(
      ctx, &dest, result, result > YCPUMaxValue[ctx->metadata->width]);
}

// IMUL r/m8
// IMUL r/m16
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteImul(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  YCPUAddCycles(ctx->cpu, YCPUMulDivCycles[ctx->metadata->width][1]);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  const YCPUWidth width = ctx->metadata->width;
  int32_t result =
      YCPUFromSignedOperand(width, &dest) * YCPUFromSignedOperand(width, op);
  return YCPUExecuteMulCommon(
      ctx, &dest, result,
      result > YCPUMaxSignedValue[ctx->metadata->width] ||
          result < YCPUMinSignedValue[ctx->metadata->width]);
}

Y_FILE_PRIVATE YCPUInstructionResult YCPUWriteDivResult(
    const YCPUInstructionContext* ctx, YCPUOperand* dest, uint32_t quotient,
    uint32_t remainder) {
  YCPUWriteOperand(ctx, dest, quotient);
  YCPUWriteOperandAddress(
      ctx, &YCPUMulDivResultHighHalfAddress[ctx->metadata->width], remainder);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// DIV r/m8
// DIV r/m16
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteDiv(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  YCPUAddCycles(ctx->cpu, YCPUMulDivCycles[ctx->metadata->width][0]);
  uint32_t divisor = YCPUFromOperand(op);
  if (divisor == 0) {
    YCPURaiseInternalInterrupt(ctx->cpu, Y_CPU_INTERRUPT_DIVIDE_ERROR);
    return Y_CPU_INSTRUCTION_EXECUTED;
  }

  YCPUWidth width = ctx->metadata->width;
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);

  YCPUOperandValue dest_high_half =
      YCPUReadOperandValue(ctx, &YCPUMulDivResultHighHalfAddress[width]);
  uint32_t dividend =
      YCPUFromOperand(&dest) | (YCPUFromOperandValue(dest_high_half)
                                << YCPUMulDivResultHighHalfShiftWidth[width]);
  uint32_t quotient = dividend / divisor;
  if (quotient > YCPUMaxValue[ctx->metadata->width]) {
    YCPURaiseInternalInterrupt(ctx->cpu, Y_CPU_INTERRUPT_DIVIDE_ERROR);
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  return YCPUWriteDivResult(ctx, &dest, quotient, dividend % divisor);
}

// IDIV r/m8
// IDIV r/m16
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteIdiv(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  YCPUAddCycles(ctx->cpu, YCPUMulDivCycles[ctx->metadata->width][1]);
  int32_t divisor = YCPUFromSignedOperand(ctx->metadata->width, op);
  if (divisor == 0) {
    YCPURaiseInternalInterrupt(ctx->cpu, Y_CPU_INTERRUPT_DIVIDE_ERROR);
    return Y_CPU_INSTRUCTION_EXECUTED;
  }

  YCPUWidth width = ctx->metadata->width;
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);

  YCPUOperandValue dest_high_half =
      YCPUReadOperandValue(ctx, &YCPUMulDivResultHighHalfAddress[width]);
  int32_t dividend = YCPUFromOperand(&dest) |
                     (YCPUFromSignedOperandValue(width, dest_high_half)
                      << YCPUMulDivResultHighHalfShiftWidth[width]);
  int32_t quotient = dividend / divisor;
  // The 8086/8088 divides the magnitudes and checks the result against the
  // largest quotient that fits, so it rejects a quotient of -128 or -32768
  // even though those are representable. The valid range is symmetric, one
  // narrower at the bottom than the operand's own range.
  if (quotient > YCPUMaxSignedValue[ctx->metadata->width] ||
      quotient < -YCPUMaxSignedValue[ctx->metadata->width]) {
    YCPURaiseInternalInterrupt(ctx->cpu, Y_CPU_INTERRUPT_DIVIDE_ERROR);
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  return YCPUWriteDivResult(ctx, &dest, quotient, dividend % divisor);
}

// Group 3 instruction implementations, indexed by the corresponding REG field
// value in the YCPUModRM byte and data width.
Y_FILE_PRIVATE const YCPUGroup3ExecuteInstructionFn
    YCPUGroup3ExecuteInstructionFns[] = {
        YCPUExecuteGroup3Test,  // 0 - TEST
        // REG 1 is an undocumented alias of REG 0: the 8086/8088 does not
        // decode bit 0 of the REG field for this group.
        YCPUExecuteGroup3Test,  // 1 - TEST
        YCPUExecuteNot,         // 2 - NOT
        YCPUExecuteNeg,         // 3 - NEG
        YCPUExecuteMul,         // 4 - MUL
        YCPUExecuteImul,        // 5 - IMUL
        YCPUExecuteDiv,         // 6 - DIV
        YCPUExecuteIdiv,        // 7 - IDIV
};

// Group 3 instruction handler.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup3Instruction(const YCPUInstructionContext* ctx) {
  const YCPUGroup3ExecuteInstructionFn fn =
      YCPUGroup3ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  return fn(ctx, &dest);
}


// ==============================================================================
// src/cpu/instructions_group_3.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_group_4.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_group_4.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 4 - INC, DEC
// ============================================================================

typedef YCPUInstructionResult (*YCPUGroup4ExecuteInstructionFn)(
    const YCPUInstructionContext* ctx, YCPUOperand* dest);

// Group 4 instruction implementations, indexed by the corresponding REG field
// value in the YCPUModRM byte.
Y_FILE_PRIVATE const YCPUGroup4ExecuteInstructionFn
    YCPUGroup4ExecuteInstructionFns[] = {
        YCPUExecuteInc,  // 0 - INC
        YCPUExecuteDec,  // 1 - DEC
};

enum {
  // Number of documented REG field values for this group.
  Y_CPU_NUM_GROUP4_INSTRUCTIONS = 2,
};

// Group 4 instruction handler.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup4Instruction(const YCPUInstructionContext* ctx) {
  // On real hardware REG 2-7 decode as byte-operand forms of the Group 5
  // instructions rather than being rejected. That behavior is deliberately not
  // emulated: it is unverifiable without hardware, and no assembler emits
  // these encodings. The bounds check matters regardless, because the REG
  // field is three bits wide and this table has only two entries.
  if (ctx->instruction->mod_rm.reg >= Y_CPU_NUM_GROUP4_INSTRUCTIONS) {
    return Y_CPU_INSTRUCTION_INVALID;
  }
  const YCPUGroup4ExecuteInstructionFn fn =
      YCPUGroup4ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  return fn(ctx, &dest);
}


// ==============================================================================
// src/cpu/instructions_group_4.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_group_5.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_group_5.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 5 - INC, DEC, CALL, JMP, PUSH
// ============================================================================

// Helper to get the segment register value for far JMP and CALL instructions.
Y_FILE_PRIVATE void YCPUGetSegmentRegisterOperandForIndirectFarJumpOrCall(
    const YCPUInstructionContext* ctx, const YCPUOperand* offset,
    YCPUOperand* operand) {
  YCPUOperandAddress segment_address = offset->address;
  segment_address.offset += 2;  // Skip the offset
  operand->address = segment_address;
  operand->value = YCPUReadMemoryOperandWord(ctx->cpu, &segment_address);
}

// JMP ptr16
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteIndirectNearJump(
    const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  ctx->cpu->registers[Y_CPU_IP] = YCPUFromOperandValue(dest->value);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// CALL ptr16
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteIndirectNearCall(
    const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[Y_CPU_IP]);
  return YCPUExecuteIndirectNearJump(ctx, dest);
}

// CALL ptr16:16
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteIndirectFarCall(
    const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUOperand segment;
  YCPUGetSegmentRegisterOperandForIndirectFarJumpOrCall(ctx, dest, &segment);
  return YCPUExecuteFarCall(ctx, segment.value, dest->value);
}

// JMP ptr16:16
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteIndirectFarJump(
    const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUOperand segment;
  YCPUGetSegmentRegisterOperandForIndirectFarJumpOrCall(ctx, dest, &segment);
  return YCPUExecuteFarJump(ctx, segment.value, dest->value);
}

// PUSH r/m16
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteIndirectPush(const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUPushSourceOperand(ctx->cpu, dest);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

typedef YCPUInstructionResult (*YCPUGroup5ExecuteInstructionFn)(
    const YCPUInstructionContext* ctx, YCPUOperand* dest);

// Group 5 instruction implementations, indexed by the corresponding REG
// field value in the YCPUModRM byte.
Y_FILE_PRIVATE const YCPUGroup5ExecuteInstructionFn
    YCPUGroup5ExecuteInstructionFns[] = {
        YCPUExecuteInc,               // 0 - INC r/m8/r/m16
        YCPUExecuteDec,               // 1 - DEC r/m8/r/m16
        YCPUExecuteIndirectNearCall,  // 2 - CALL rel16
        YCPUExecuteIndirectFarCall,   // 3 - CALL ptr16:16
        YCPUExecuteIndirectNearJump,  // 4 - JMP ptr16
        YCPUExecuteIndirectFarJump,   // 5 - JMP ptr16:16
        YCPUExecuteIndirectPush,      // 6 - PUSH r/m16
        // REG 7 is an undocumented alias of REG 6. Without this entry the
        // lookup below would read past the end of the table, since the REG
        // field is three bits wide.
        YCPUExecuteIndirectPush,  // 7 - PUSH r/m16
};

// Group 5 instruction handler.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup5Instruction(const YCPUInstructionContext* ctx) {
  const YCPUGroup5ExecuteInstructionFn fn =
      YCPUGroup5ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  return fn(ctx, &dest);
}


// ==============================================================================
// src/cpu/instructions_group_5.c end
// ==============================================================================

// ==============================================================================
// src/cpu/opcode_table.c start
// ==============================================================================

#line 1 "./src/cpu/opcode_table.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Opcode table
// ============================================================================

// Global opcode metadata lookup table.
//
// Const because nothing writes it, which on a target that executes from flash
// keeps 2KB of it out of RAM.
//
// base_cycles is part 1 of the cycle model, and is only half of what an
// instruction costs before it runs - cycles.c has the other half and the
// rules these figures were derived under, which is where to read them
// against.
Y_MODULE_PRIVATE const YCPUOpcodeMetadata YCPUOpcodeTable[256] = {
    // ADD r/m8, r8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteAddRegisterToRegisterOrMemory},
    // ADD r/m16, r16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteAddRegisterToRegisterOrMemory},
    // ADD r8, r/m8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteAddRegisterOrMemoryToRegister},
    // ADD r16, r/m16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteAddRegisterOrMemoryToRegister},
    // ADD AL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteAddImmediateToALOrAX},
    // ADD AX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteAddImmediateToALOrAX},
    // PUSH ES
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushSegmentRegister},
    // POP ES
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopSegmentRegister},
    // OR r/m8, r8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteBooleanOrRegisterToRegisterOrMemory},
    // OR r/m16, r16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteBooleanOrRegisterToRegisterOrMemory},
    // OR r8, r/m8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteBooleanOrRegisterOrMemoryToRegister},
    // OR r16, r/m16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteBooleanOrRegisterOrMemoryToRegister},
    // OR AL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteBooleanOrImmediateToALOrAX},
    // OR AX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteBooleanOrImmediateToALOrAX},
    // PUSH CS
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushSegmentRegister},
    // POP CS
    //
    // Undocumented, but real: on the 8086/8088 the segment register field of a
    // POP sreg is only two bits wide, so 0x0F decodes as POP CS. Later x86
    // parts repurposed 0x0F as the two-byte opcode prefix.
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopSegmentRegister},
    // ADC r/m8, r8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteAddRegisterToRegisterOrMemoryWithCarry},
    // ADC r/m16, r16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteAddRegisterToRegisterOrMemoryWithCarry},
    // ADC r8, r/m8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteAddRegisterOrMemoryToRegisterWithCarry},
    // ADC r16, r/m16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteAddRegisterOrMemoryToRegisterWithCarry},
    // ADC AL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteAddImmediateToALOrAXWithCarry},
    // ADC AX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteAddImmediateToALOrAXWithCarry},
    // PUSH SS
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushSegmentRegister},
    // POP SS
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopSegmentRegister},
    // SBB r/m8, r8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSubRegisterFromRegisterOrMemoryWithBorrow},
    // SBB r/m16, r16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteSubRegisterFromRegisterOrMemoryWithBorrow},
    // SBB r8, r/m8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSubRegisterOrMemoryFromRegisterWithBorrow},
    // SBB r16, r/m16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteSubRegisterOrMemoryFromRegisterWithBorrow},
    // SBB AL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSubImmediateFromALOrAXWithBorrow},
    // SBB AX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteSubImmediateFromALOrAXWithBorrow},
    // PUSH DS
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushSegmentRegister},
    // POP DS
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopSegmentRegister},
    // AND r/m8, r8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteBooleanAndRegisterToRegisterOrMemory},
    // AND r/m16, r16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteBooleanAndRegisterToRegisterOrMemory},
    // AND r8, r/m8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteBooleanAndRegisterOrMemoryToRegister},
    // AND r16, r/m16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteBooleanAndRegisterOrMemoryToRegister},
    // AND AL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteBooleanAndImmediateToALOrAX},
    // AND AX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteBooleanAndImmediateToALOrAX},
    // ES prefix - 0x26
    {.base_cycles = 2, .handler = YCPUExecuteInvalidOpcode},
    // DAA
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteDaa},
    // SUB r/m8, r8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSubRegisterFromRegisterOrMemory},
    // SUB r/m16, r16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteSubRegisterFromRegisterOrMemory},
    // SUB r8, r/m8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSubRegisterOrMemoryFromRegister},
    // SUB r16, r/m16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteSubRegisterOrMemoryFromRegister},
    // SUB AL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSubImmediateFromALOrAX},
    // SUB AX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteSubImmediateFromALOrAX},
    // CS prefix - 0x2E
    {.base_cycles = 2, .handler = YCPUExecuteInvalidOpcode},
    // DAS
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteDas},
    // XOR r/m8, r8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteBooleanXorRegisterToRegisterOrMemory},
    // XOR r/m16, r16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteBooleanXorRegisterToRegisterOrMemory},
    // XOR r8, r/m8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteBooleanXorRegisterOrMemoryToRegister},
    // XOR r16, r/m16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteBooleanXorRegisterOrMemoryToRegister},
    // XOR AL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteBooleanXorImmediateToALOrAX},
    // XOR AX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteBooleanXorImmediateToALOrAX},
    // SS prefix - 0x36
    {.base_cycles = 2, .handler = YCPUExecuteInvalidOpcode},
    // AAA
    {.base_cycles = 8,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteAaa},
    // CMP r/m8, r8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteCmpRegisterToRegisterOrMemory},
    // CMP r/m16, r16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteCmpRegisterToRegisterOrMemory},
    // CMP r8, r/m8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteCmpRegisterOrMemoryToRegister},
    // CMP r16, r/m16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteCmpRegisterOrMemoryToRegister},
    // CMP AL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteCmpImmediateToALOrAX},
    // CMP AX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteCmpImmediateToALOrAX},
    // DS prefix - 0x3E
    {.base_cycles = 2, .handler = YCPUExecuteInvalidOpcode},
    // AAS
    {.base_cycles = 8,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteAas},
    // INC AX
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteIncRegister},
    // INC CX
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteIncRegister},
    // INC DX
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteIncRegister},
    // INC BX
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteIncRegister},
    // INC SP
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteIncRegister},
    // INC BP
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteIncRegister},
    // INC SI
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteIncRegister},
    // INC DI
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteIncRegister},
    // DEC AX
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDecRegister},
    // DEC CX
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDecRegister},
    // DEC DX
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDecRegister},
    // DEC BX
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDecRegister},
    // DEC SP
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDecRegister},
    // DEC BP
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDecRegister},
    // DEC SI
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDecRegister},
    // DEC DI
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDecRegister},
    // PUSH AX
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushRegister},
    // PUSH CX
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushRegister},
    // PUSH DX
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushRegister},
    // PUSH BX
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushRegister},
    // PUSH SP
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushRegister},
    // PUSH BP
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushRegister},
    // PUSH SI
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushRegister},
    // PUSH DI
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushRegister},
    // POP AX
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopRegister},
    // POP CX
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopRegister},
    // POP DX
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopRegister},
    // POP BX
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopRegister},
    // POP SP
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopRegister},
    // POP BP
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopRegister},
    // POP SI
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopRegister},
    // POP DI
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopRegister},
    // 0x60 - 0x6F - UNSUPPORTED
    // 0x60-0x6F - conditional jumps, aliases of 0x70-0x7F
    //
    // Undocumented, but real: the 8086/8088 decoder ignores bit 4 of these
    // opcodes, so each one behaves exactly like its 0x7X counterpart. They are
    // two bytes wide, so decoding them as unknown single-byte opcodes would
    // desynchronize the instruction stream.
    // JO rel8 (alias of 0x70)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNO rel8 (alias of 0x71)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JB/JNAE/JC rel8 (alias of 0x72)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNB/JAE/JNC rel8 (alias of 0x73)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JE/JZ rel8 (alias of 0x74)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNE/JNZ rel8 (alias of 0x75)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JBE/JNA rel8 (alias of 0x76)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNBE/JA rel8 (alias of 0x77)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JS rel8 (alias of 0x78)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNS rel8 (alias of 0x79)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JP/JPE rel8 (alias of 0x7A)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNP/JPO rel8 (alias of 0x7B)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JL/JNGE rel8 (alias of 0x7C)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSignedConditionalJumpJLOrJNL},
    // JNL/JGE rel8 (alias of 0x7D)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSignedConditionalJumpJLOrJNL},
    // JLE/JNG rel8 (alias of 0x7E)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSignedConditionalJumpJLEOrJNLE},
    // JNLE/JG rel8 (alias of 0x7F)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSignedConditionalJumpJLEOrJNLE},
    // JO rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNO rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JB/JNAE/JC rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNB/JAE/JNC rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JE/JZ rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNE/JNZ rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JBE/JNA rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNBE/JA rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JS rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNS rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JP/JPE rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNP/JPO rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JL/JNGE rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSignedConditionalJumpJLOrJNL},
    // JNL/JGE rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSignedConditionalJumpJLOrJNL},
    // JLE/JNG rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSignedConditionalJumpJLEOrJNLE},
    // JNLE/JG rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSignedConditionalJumpJLEOrJNLE},
    // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m8, imm8 (Group 1)
    {.base_cycles = 4,
     .has_modrm = true,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteGroup1Instruction},
    // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m16, imm16 (Group 1)
    {.base_cycles = 4,
     .has_modrm = true,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteGroup1Instruction},
    // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m8, imm8 (Group 1)
    {.base_cycles = 4,
     .has_modrm = true,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteGroup1Instruction},
    // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m16, imm8 (Group 1)
    {.base_cycles = 4,
     .has_modrm = true,
     // This is a special case - the immediate is 8 bits but the destination is
     // 16 bits.
     .immediate_size = 1,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteGroup1InstructionWithSignExtension},
    // TEST r/m8, r8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteTestRegisterToRegisterOrMemory},
    // TEST r/m16, r16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteTestRegisterToRegisterOrMemory},
    // XCHG r/m8, r8
    {.base_cycles = 4,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteExchangeRegisterOrMemory},
    // XCHG r/m16, r16
    {.base_cycles = 4,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteExchangeRegisterOrMemory},
    // MOV r/m8, r8
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveRegisterToRegisterOrMemory},
    // MOV r/m16, r16
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveRegisterToRegisterOrMemory},
    // MOV r8, r/m8
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveRegisterOrMemoryToRegister},
    // MOV r16, r/m16
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveRegisterOrMemoryToRegister},
    // MOV r/m16, sreg
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveSegmentRegisterToRegisterOrMemory},
    // LEA r16, m
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteLoadEffectiveAddress},
    // MOV sreg, r/m16
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveRegisterOrMemoryToSegmentRegister},
    // POP r/m16
    {.base_cycles = 0,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopRegisterOrMemory},
    // XCHG AX, AX (NOP)
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteExchangeRegister},
    // XCHG AX, CX
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteExchangeRegister},
    // XCHG AX, DX
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteExchangeRegister},
    // XCHG AX, BX
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteExchangeRegister},
    // XCHG AX, SP
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteExchangeRegister},
    // XCHG AX, BP
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteExchangeRegister},
    // XCHG AX, SI
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteExchangeRegister},
    // XCHG AX, DI
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteExchangeRegister},
    // CBW
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteCbw},
    // CWD
    {.base_cycles = 5,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteCwd},
    // CALL ptr16:16 (4 bytes: 2 for offset, 2 for segment)
    {.base_cycles = 12,
     .has_modrm = false,
     .immediate_size = 4,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDirectFarCall},
    // WAIT
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteNoOp},
    // PUSHF
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushFlags},
    // POPF
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopFlags},
    // SAHF
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteStoreAHToFlags},
    // LAHF
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteLoadAHFromFlags},
    // MOV AL, moffs16
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveMemoryOffsetToALOrAX},
    // MOV AX, moffs16
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveMemoryOffsetToALOrAX},
    // MOV moffs16, AL
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveALOrAXToMemoryOffset},
    // MOV moffs16, AX
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveALOrAXToMemoryOffset},
    // MOVSB
    {.base_cycles = 10,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMovs},
    // MOVSW
    {.base_cycles = 10,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMovs},
    // CMPSB
    {.base_cycles = 14,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteCmps},
    // CMPSW
    {.base_cycles = 14,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteCmps},
    // TEST AL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteTestImmediateToALOrAX},
    // TEST AX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteTestImmediateToALOrAX},
    // STOSB
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteStos},
    // STOSW
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteStos},
    // LODSB
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteLods},
    // LODSW
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteLods},
    // SCASB
    {.base_cycles = 7,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteScas},
    // SCASW
    {.base_cycles = 7,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteScas},
    // MOV AL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV CL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV DL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV BL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV AH, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV CH, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV DH, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV BH, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV AX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV CX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV DX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV BX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV SP, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV BP, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV SI, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV DI, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // RET imm16 (alias of 0xC2)
    //
    // Undocumented, but real: the 8086/8088 decoder ignores bit 1 of these
    // opcodes. 0xC0 takes a 16-bit immediate, so decoding it as an unknown
    // single-byte opcode would desynchronize the instruction stream.
    {.base_cycles = 12,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteNearReturnAndPop},
    // RET (alias of 0xC3)
    {.base_cycles = 8,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteNearReturn},
    // RET imm16
    {.base_cycles = 12,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteNearReturnAndPop},
    // RET
    {.base_cycles = 8,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteNearReturn},
    // LES r16, m32
    {.base_cycles = 8,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteLoadESWithPointer},
    // LDS r16, m32
    {.base_cycles = 8,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteLoadDSWithPointer},
    // MOV r/m8, imm8
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveImmediateToRegisterOrMemory},
    // MOV r/m16, imm16
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveImmediateToRegisterOrMemory},
    // RETF imm16 (alias of 0xCA)
    {.base_cycles = 9,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteFarReturnAndPop},
    // RETF (alias of 0xCB)
    {.base_cycles = 10,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteFarReturn},
    // RETF imm16
    {.base_cycles = 9,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteFarReturnAndPop},
    // RETF
    {.base_cycles = 10,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteFarReturn},
    // INT 3
    {.base_cycles = 1,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteInt3},
    // INT imm8
    {.base_cycles = 1,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteIntN},
    // INTO
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteInto},
    // IRET
    {.base_cycles = 32,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteIret},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m8, 1 (Group 2)
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteGroup2ShiftOrRotateBy1Instruction},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m16, 1 (Group 2)
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteGroup2ShiftOrRotateBy1Instruction},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m8, CL (Group 2)
    {.base_cycles = 8,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteGroup2ShiftOrRotateByCLInstruction},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m16, CL (Group 2)
    {.base_cycles = 8,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteGroup2ShiftOrRotateByCLInstruction},
    // AAM
    {.base_cycles = 8,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteAam},
    // AAD
    {.base_cycles = 8,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteAad},
    // SALC - Set AL from Carry
    //
    // Undocumented, but real and stable across x86 generations: sets AL to
    // 0xFF if CF is set and 0x00 otherwise. Affects no flags.
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSetALFromCarry},
    // XLAT/XLATB
    {.base_cycles = 11,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteTranslateByte},
    // ESC instruction 0xD8 for 8087 numeric coprocessor
    {.base_cycles = 0,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = YCPUExecuteNoOp},
    // ESC instruction 0xD9 for 8087 numeric coprocessor
    {.base_cycles = 0,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = YCPUExecuteNoOp},
    // ESC instruction 0xDA for 8087 numeric coprocessor
    {.base_cycles = 0,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = YCPUExecuteNoOp},
    // ESC instruction 0xDB for 8087 numeric coprocessor
    {.base_cycles = 0,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = YCPUExecuteNoOp},
    // ESC instruction 0xDC for 8087 numeric coprocessor
    {.base_cycles = 0,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = YCPUExecuteNoOp},
    // ESC instruction 0xDD for 8087 numeric coprocessor
    {.base_cycles = 0,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = YCPUExecuteNoOp},
    // ESC instruction 0xDE for 8087 numeric coprocessor
    {.base_cycles = 0,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = YCPUExecuteNoOp},
    // ESC instruction 0xDF for 8087 numeric coprocessor
    {.base_cycles = 0,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = YCPUExecuteNoOp},
    // LOOPNE/LOOPNZ rel8
    {.base_cycles = 5,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteLoopZOrNZ},
    // LOOPE/LOOPZ rel8
    {.base_cycles = 6,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteLoopZOrNZ},
    // LOOP rel8
    {.base_cycles = 6,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteLoop},
    // JCXZ rel8
    {.base_cycles = 6,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteJumpIfCXIsZero},
    // IN AL, imm8
    {.base_cycles = 10,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteInImmediate},
    // IN AX, imm8
    {.base_cycles = 10,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteInImmediate},
    // OUT imm8, AL
    {.base_cycles = 10,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteOutImmediate},
    // OUT imm8, AX
    {.base_cycles = 10,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteOutImmediate},
    // CALL rel16
    {.base_cycles = 11,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDirectNearCall},
    // JMP rel16
    {.base_cycles = 15,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteShortOrNearJump},
    // JMP ptr16:16 (4 bytes: 2 for offset, 2 for segment)
    {.base_cycles = 15,
     .has_modrm = false,
     .immediate_size = 4,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDirectFarJump},
    // JMP rel8
    {.base_cycles = 15,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteShortOrNearJump},
    // IN AL, DX
    {.base_cycles = 8,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteInDX},
    // IN AX, DX
    {.base_cycles = 8,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteInDX},
    // OUT DX, AL
    {.base_cycles = 8,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteOutDX},
    // OUT DX, AX
    {.base_cycles = 8,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteOutDX},
    // 0xF0 - LOCK prefix
    {.base_cycles = 2, .handler = YCPUExecuteInvalidOpcode},
    // LOCK prefix (alias of 0xF0) - consumed by the prefix decoder.
    {.base_cycles = 2, .handler = YCPUExecuteInvalidOpcode},
    // 0xF2 - REPNE prefix
    {.base_cycles = 2, .handler = YCPUExecuteInvalidOpcode},
    // 0xF3 - REP/REPE prefix
    {.base_cycles = 2, .handler = YCPUExecuteInvalidOpcode},
    // HLT
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteHlt},
    // CMC
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteComplementCarryFlag},
    // TEST/NOT/NEG/MUL/IMUL/DIV/IDIV r/m8 (Group 3)
    // The immediate size depends on the ModR/M byte.
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteGroup3Instruction},
    // TEST/NOT/NEG/MUL/IMUL/DIV/IDIV r/m16 (Group 3)
    // The immediate size depends on the ModR/M byte.
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteGroup3Instruction},
    // CLC
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteClearOrSetFlag},
    // STC
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteClearOrSetFlag},
    // CLI
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteClearOrSetFlag},
    // STI
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteClearOrSetFlag},
    // CLD
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteClearOrSetFlag},
    // STD
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteClearOrSetFlag},
    // INC/DEC r/m8 (Group 4)
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteGroup4Instruction},
    // INC/DEC/CALL/JMP/PUSH r/m16 (Group 5)
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteGroup5Instruction},
};


// ==============================================================================
// src/cpu/opcode_table.c end
// ==============================================================================

// ==============================================================================
// src/cpu/cpu.c start
// ==============================================================================

#line 1 "./src/cpu/cpu.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "cycles.h"
#include "instructions.h"
#include "operands.h"
#include "public.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

#define Y_CPU_ERROR(...) \
  Y_LOG(cpu->config.logger, &YCPULogModule, Y_LOG_LEVEL_ERROR, __VA_ARGS__)
#define Y_CPU_WARN(...) \
  Y_LOG(cpu->config.logger, &YCPULogModule, Y_LOG_LEVEL_WARN, __VA_ARGS__)
#define Y_CPU_INFO(...) \
  Y_LOG(cpu->config.logger, &YCPULogModule, Y_LOG_LEVEL_INFO, __VA_ARGS__)
#define Y_CPU_DEBUG(...) \
  Y_LOG(cpu->config.logger, &YCPULogModule, Y_LOG_LEVEL_DEBUG, __VA_ARGS__)

// ============================================================================
// CPU state
// ============================================================================

Y_PUBLIC void YCPUInit(YCPUState* cpu) {
  cpu->flags = Y_CPU_INITIAL_FLAGS;

  // The only place the count is read, so a host gets told here or not at all.
  const uint32_t num_entries = cpu->config.decode_cache_num_entries;
  if (cpu->config.decode_cache == NULL) {
    return;
  }
  if (num_entries == 0 || (num_entries & (num_entries - 1)) != 0) {
    // The CPU runs without a cache.
    Y_CPU_ERROR(
        "decode_cache_size_not_power_of_two",
        Y_LOG_UINT("entries", num_entries));
    // The CPU's own copy, so a rejected cache is simply not there.
    cpu->config.decode_cache = NULL;
    return;
  }
  cpu->decode_cache_index_mask = num_entries - 1;
}

// ============================================================================
// Instruction decoding
// ============================================================================

// The two prefix groups are each a contiguous encoding family, so a masked
// compare identifies a whole group and the bits the mask leaves free say which
// member it is.
enum {
  // Segment overrides encode as 001ss110, where ss selects the segment.
  Y_CPU_SEGMENT_OVERRIDE_PREFIX_MASK = 0xE7,
  Y_CPU_SEGMENT_OVERRIDE_PREFIX_VALUE = 0x26,
  // Position of the ss field within a segment override prefix.
  Y_CPU_SEGMENT_OVERRIDE_PREFIX_SHIFT = 3,
  Y_CPU_SEGMENT_OVERRIDE_PREFIX_SEGMENT_MASK = 0x03,

  // Within the LOCK and repetition prefix group, bit 1 separates the
  // repetition prefixes (REPNZ, REP) from LOCK and its undocumented 0xF1
  // alias.
  Y_CPU_REPETITION_PREFIX_BIT = 0x02,
};

// Whether a byte is a segment override prefix. The mask pins every bit but the
// two that select the segment, so it matches those four bytes and nothing else.
Y_FILE_PRIVATE inline bool YCPUIsSegmentOverridePrefix(uint8_t byte) {
  return (byte & Y_CPU_SEGMENT_OVERRIDE_PREFIX_MASK) ==
         Y_CPU_SEGMENT_OVERRIDE_PREFIX_VALUE;
}

// Whether a byte is a LOCK or repetition prefix. These four are consecutive,
// so this is a range check rather than a mask - which is both clearer and one
// instruction cheaper, since a compiler folds it into a single subtract and
// compare.
Y_FILE_PRIVATE inline bool YCPUIsLockOrRepetitionPrefix(uint8_t byte) {
  return byte >= Y_CPU_PREFIX_LOCK && byte <= Y_CPU_PREFIX_REP;
}

// Record what a prefix byte selects, and report whether it was a prefix at
// all. Deciding which group a byte belongs to is the same test as deciding
// whether it is a prefix, so both happen here rather than the caller asking
// first and this asking again.
//
// The cheaper test goes first. A byte that is not a prefix runs both, and that
// is the common case by far - every instruction ends the loop with one.
Y_FILE_PRIVATE bool YCPUApplyPrefixByte(
    YCPUInstruction* instruction, uint8_t byte) {
  if (YCPUIsLockOrRepetitionPrefix(byte)) {
    // LOCK and its 0xF1 alias advance IP, but nothing acts on them, so only a
    // repetition prefix is worth recording.
    if (byte & Y_CPU_REPETITION_PREFIX_BIT) {
      instruction->repetition_prefix = byte;
    }
    return true;
  }
  if (YCPUIsSegmentOverridePrefix(byte)) {
    // The segment field is in the 8086's sreg encoding order, which is the
    // order Y_CPU_ES through Y_CPU_DS are numbered in, so the register index is
    // an offset from Y_CPU_ES.
    instruction->segment_override =
        (uint8_t)(Y_CPU_ES + ((byte >> Y_CPU_SEGMENT_OVERRIDE_PREFIX_SHIFT) &
                              Y_CPU_SEGMENT_OVERRIDE_PREFIX_SEGMENT_MASK));
    return true;
  }
  return false;
}

// Where one call to YCPUFetchNextInstruction() has got to.
//
// Scratch for a single decode, and nothing more. What survives between
// instructions is the window itself, in YCPUState.instruction_fetch_window;
// this is the cursor walking it, and it is thrown away when the instruction
// has been decoded.
//
// It is a struct because YCPUFetchNextInstructionByte() has to advance the
// cursor and is called from five places in the decode, so the cursor has to be
// passed by pointer. Kept as three loose locals it would have to be passed as
// three out-parameters, and taking the address of each is what stops a
// compiler keeping them in registers - which is the whole point of the
// arrangement, since this is the hottest loop in the emulator.
//
// The two byte positions are the same position expressed twice, deliberately:
//
//   - next_byte_offset is where the cursor is in the segment, and is what
//     yields the instruction's size and what the byte-at-a-time path needs. It
//     is not the CPU's IP register, which has to keep naming the instruction
//     being decoded until YCPUTick() advances it by instruction.size - so a
//     failed decode leaves IP alone, and the control flow instructions can add
//     their displacement to the address of the instruction after this one.
//   - next_byte is where the cursor is in the host's memory, and is cached
//     rather than derived because deriving it costs a shift, an add, a
//     subtract and an add per byte where advancing it costs an increment.
//
// bytes_remaining bounds the direct reads. Where the host hands out no window
// it is zero, and every byte takes the ordinary path through read_memory_byte
// - an indirect call per byte, two to six times per instruction, which is what
// the window exists to avoid.
typedef struct YCPUInstructionFetchState {
  // The offset within CS of the next byte to read.
  uint16_t next_byte_offset;
  // The next byte to read, and how many may be read directly from it. NULL and
  // zero where there is no window.
  const uint8_t* next_byte;
  uint32_t bytes_remaining;
} YCPUInstructionFetchState;

// Reads the next instruction byte, from the window where one is open and from
// the memory map where one is not.
//
// Either way the read goes straight to memory rather than through
// YCPUReadMemoryOperandByte, so that the fetch is not charged for time on the
// data bus. The 8088 fetches ahead into a queue while the previous instruction
// executes, so most of that time is already paid for by the instruction being
// executed - and the published per-instruction figures the cycle table is
// built from assume the queue is full.
Y_FILE_PRIVATE inline Y_HOT uint8_t YCPUFetchNextInstructionByte(
    YCPUState* cpu, YCPUInstructionFetchState* fetch_state) {
  if (fetch_state->bytes_remaining > 0) {
    --fetch_state->bytes_remaining;
    ++fetch_state->next_byte_offset;
    return *fetch_state->next_byte++;
  }
  return YCPUReadRawMemoryByte(
      cpu, YCPUToRawAddress(cpu, Y_CPU_CS, fetch_state->next_byte_offset++));
}

// Points a fetch at whatever can be read directly from CS:ip.
Y_FILE_PRIVATE Y_HOT void YCPUInitInstructionFetchState(
    YCPUState* cpu, uint16_t ip, YCPUInstructionFetchState* fetch_state) {
  fetch_state->next_byte_offset = ip;
  fetch_state->next_byte = NULL;
  fetch_state->bytes_remaining = 0;

  // Note that there is deliberately no early out for a host that supplies no
  // get_instruction_fetch_window, even though everything below is wasted on
  // one. Such a host reads every byte through an indirect call already, so it
  // would save a handful of arithmetic against several calls - and the test to
  // skip it would be paid by the hosts that do supply one, which is every host
  // that cares about the speed. Measured on x86-64, adding it grew
  // YCPUFetchNextInstruction by 11 bytes.
  const uint32_t raw_address = YCPUToRawAddress(cpu, Y_CPU_CS, ip);

  const YCPUInstructionFetchWindow* const window =
      &cpu->instruction_fetch_window;
  if (window->data == NULL || raw_address < window->start ||
      raw_address >= window->end) {
    // Nothing open covers this address, so ask for a window that does. The
    // host fills one in, or sets its data to NULL to decline.
    if (cpu->config.get_instruction_fetch_window != NULL) {
      cpu->config.get_instruction_fetch_window(cpu, raw_address);
    } else {
      cpu->instruction_fetch_window.data = NULL;
    }
  }
  // Usually the window was already open and already covered the address, which
  // costs a compare rather than a call: a window spans a whole memory region,
  // so both straight-line execution and a jump backwards within that region
  // land inside the one already open.
  if (window->data != NULL && raw_address >= window->start &&
      raw_address < window->end) {
    fetch_state->next_byte = window->data + (raw_address - window->start);
    fetch_state->bytes_remaining = window->end - raw_address;
  }

  // IP is 16 bits and wraps within the segment where the linear address does
  // not, so the fetch has to stop where the wrap would be. Past that point the
  // ordinary path recomputes the address from the wrapped IP and gets it
  // right.
  const uint32_t bytes_until_wrap = (uint32_t)Y_CPU_SEGMENT_SIZE - (uint32_t)ip;
  if (fetch_state->bytes_remaining > bytes_until_wrap) {
    fetch_state->bytes_remaining = bytes_until_wrap;
  }
}

// Returns the number of displacement bytes based on the ModR/M byte.
Y_FILE_PRIVATE uint8_t YCPUGetDisplacementSize(uint8_t mod, uint8_t rm) {
  switch (mod) {
    case 0:
      // Special case: 16-bit displacement
      return rm == 6 ? 2 : 0;
    case 1:
    case 2:
      // 8 or 16-bit displacement
      return mod;
    default:
      // No displacement
      return 0;
  }
}

// Returns the number of immediate bytes in an instruction.
Y_FILE_PRIVATE uint8_t YCPUGetImmediateSize(
    const YCPUOpcodeMetadata* metadata, uint8_t opcode, uint8_t reg) {
  switch (opcode) {
    // TEST r/m8, imm8
    case 0xF6:
    // TEST r/m16, imm16
    case 0xF7:
      // REG 0 and REG 1 are both TEST, which carries an immediate; the other
      // REG values do not. The 8086/8088 does not decode bit 0 of the REG
      // field here, which is what makes REG 1 an alias of REG 0.
      return reg <= 1 ? opcode - 0xF5 : 0;
    default:
      return metadata->immediate_size;
  }
}

Y_PUBLIC Y_HOT YCPUFetchNextInstructionStatus
YCPUFetchNextInstruction(YCPUState* cpu, YCPUInstruction* instruction) {
  // The prefix fields, which YCPUApplyPrefixByte() writes only where a prefix
  // is actually present. Every other field a decode could leave behind is
  // settled where it becomes known rather than here: opcode, size and
  // immediate_size are assigned on every path that returns success, has_mod_rm
  // and displacement_size in both arms of the ModR/M branch below, mod_rm is
  // read only where has_mod_rm is set, and the displacement and immediate
  // arrays are read no further than their size fields say.
  instruction->segment_override = Y_CPU_NO_SEGMENT_OVERRIDE;
  instruction->repetition_prefix = 0;

  uint8_t current_byte;
  const uint16_t original_ip = cpu->registers[Y_CPU_IP];
  YCPUInstructionFetchState fetch_state;
  YCPUInitInstructionFetchState(cpu, original_ip, &fetch_state);

  // Prefix
  //
  // The count is local because nothing outside this loop wants it: what the
  // prefixes selected is in the instruction, and the bytes they occupied are
  // in its size. It exists only to stop a run of prefix bytes from fetching
  // forever - see Y_CPU_MAX_PREFIX_BYTES.
  uint8_t prefix_size = 0;
  current_byte = YCPUFetchNextInstructionByte(cpu, &fetch_state);
  while (YCPUApplyPrefixByte(instruction, current_byte)) {
    if (++prefix_size > Y_CPU_MAX_PREFIX_BYTES) {
      return Y_CPU_FETCH_PREFIX_TOO_LONG;
    }
    current_byte = YCPUFetchNextInstructionByte(cpu, &fetch_state);
  }

  // Opcode
  instruction->opcode = current_byte;
  const YCPUOpcodeMetadata* metadata = &YCPUOpcodeTable[instruction->opcode];

  // ModR/M
  //
  // The REG field is kept in a local, because the immediate size below is
  // computed whether or not the instruction carries a ModR/M byte.
  uint8_t reg = 0;
  if (metadata->has_modrm) {
    uint8_t mod_rm_byte = YCPUFetchNextInstructionByte(cpu, &fetch_state);
    reg = (mod_rm_byte >> 3) & 0x07;  // Bits 3-5
    instruction->has_mod_rm = true;
    instruction->mod_rm.mod = (mod_rm_byte >> 6) & 0x03;  // Bits 6-7
    instruction->mod_rm.reg = reg;
    instruction->mod_rm.rm = mod_rm_byte & 0x07;  // Bits 0-2

    // Displacement
    const uint8_t displacement_size = YCPUGetDisplacementSize(
        instruction->mod_rm.mod, instruction->mod_rm.rm);
    instruction->displacement_size = displacement_size;
    for (uint8_t i = 0; i < displacement_size; ++i) {
      instruction->displacement[i] =
          YCPUFetchNextInstructionByte(cpu, &fetch_state);
    }
  } else {
    // Cleared in the arm that skips them rather than before the decode starts,
    // so that a decode writes each of them exactly once whichever arm it
    // takes. Clearing them up front costs the ModR/M arm a second write of
    // both.
    instruction->has_mod_rm = false;
    instruction->displacement_size = 0;
  }

  // Immediate operand
  //
  // No entry in the opcode table exceeds the immediate[] array - the widest is
  // the 4 of a far pointer - but a compiler that cannot see it is right to warn
  // about the writes below. Bounding it by the array is what makes the
  // invariant explicit.
  uint8_t immediate_size =
      YCPUGetImmediateSize(metadata, instruction->opcode, reg);
  if (immediate_size > Y_CPU_MAX_IMMEDIATE_BYTES) {
    immediate_size = Y_CPU_MAX_IMMEDIATE_BYTES;
  }
  instruction->immediate_size = immediate_size;
  for (uint8_t i = 0; i < immediate_size; ++i) {
    instruction->immediate[i] = YCPUFetchNextInstructionByte(cpu, &fetch_state);
  }

  instruction->size = (uint8_t)(fetch_state.next_byte_offset - original_ip);

  // What the instruction costs before it runs. Settled here because metadata
  // is already in hand, so the base cost is read from an entry the decode has
  // loaded rather than indexed again from the caller.
  instruction->base_cycles = (uint16_t)metadata->base_cycles +
                             YCPUGetEffectiveAddressCycles(instruction);

  return Y_CPU_FETCH_SUCCESS;
}

Y_PUBLIC void YCPUInvalidateDecodeCache(YCPUState* cpu) {
  YCPUDecodeCacheEntry* const cache = cpu->config.decode_cache;
  if (cache == NULL) {
    return;
  }
  for (uint32_t i = 0; i <= cpu->decode_cache_index_mask; ++i) {
    cache[i].valid = false;
  }
}

// Whether an entry holds a decode of the instruction at address that is still
// current. The generation is passed in because both callers have it already.
Y_FILE_PRIVATE Y_ALWAYS_INLINE bool YCPUIsDecodeCacheHit(
    const YCPUDecodeCacheEntry* entry, uint32_t address, uint8_t generation) {
  return entry->valid && entry->address == address &&
         entry->generation == generation;
}

// Fetches the next instruction, from the decode cache where it is there.
//
// What comes back through entry is the entry holding the instruction to run:
// the cache entry on a hit, scratch where there is no cache, and on a miss the
// entry the decode went straight into. Nothing is ever copied.
//
// Handing back the entry rather than the YCPUInstruction inside it is worth
// 0.68% at -O3, even though YCPUTick() reads nothing from an entry but
// ->instruction. One entry pointer addresses both the key fields and every
// instruction field, all inside the 5 bit immediate offset a Cortex-M0+
// encodes; handing back the YCPUInstruction makes the probe at the bottom of
// the run loop hold the entry for its hit test and entry->instruction to carry
// across the back-edge, which is two live pointers where there was one.
// YCPUTick() has no register to spare for that - its frame grows from 52 bytes
// to 60 and the loop picks up a spill and reload - so the smaller-looking
// signature is the slower one.
//
// A caller must not hold the pointer across another fetch.
Y_FILE_PRIVATE Y_HOT YCPUFetchNextInstructionStatus
YCPUFetchNextInstructionCached(
    YCPUState* cpu, YCPUDecodeCacheEntry* scratch,
    YCPUDecodeCacheEntry** entry) {
  const uint16_t ip = cpu->registers[Y_CPU_IP];
  // NULL covers both having no cache and having asked for an unusable one,
  // since YCPUInit() clears its own copy of a count that did not pass.
  YCPUDecodeCacheEntry* const cache = cpu->config.decode_cache;

  // Where the decode is going to land, and the key it would be kept under. The
  // two paths below join up at the decode rather than each making their own
  // call, because a second call site is enough for GCC to stop inlining what
  // it inlines into one.
  YCPUDecodeCacheEntry* target = scratch;
  uint32_t address = 0;
  uint8_t generation = 0;

  if (cache != NULL) {
    address = YCPUToRawAddress(cpu, Y_CPU_CS, ip);
    generation = cpu->code_page_generation[address >> Y_CPU_CODE_PAGE_SHIFT];
    target = &cache[address & cpu->decode_cache_index_mask];
    if (YCPUIsDecodeCacheHit(target, address, generation)) {
      *entry = target;
      return Y_CPU_FETCH_SUCCESS;
    }
    // A decode that fails partway leaves the entry holding whatever it got to,
    // so the entry disowns its contents before the decode rather than after.
    target->valid = false;
  }

  *entry = target;
  const YCPUFetchNextInstructionStatus status =
      YCPUFetchNextInstruction(cpu, &target->instruction);
  if (status != Y_CPU_FETCH_SUCCESS) {
    return status;
  }

  // Kept only when the whole instruction came from the page the generation was
  // read for, and did not run off the end of the segment. Past either boundary
  // the bytes are somewhere the key says nothing about, and both are too rare
  // to be worth a second check on every hit. Staying inside the page also
  // rules out wrapping the top of the address space.
  const uint32_t size = target->instruction.size;
  if (cache != NULL &&
      (address & Y_CPU_CODE_PAGE_OFFSET_MASK) + size <= Y_CPU_CODE_PAGE_SIZE &&
      (uint32_t)ip + size <= Y_CPU_SEGMENT_SIZE) {
    target->address = address;
    target->generation = generation;
    target->valid = true;
  }
  return Y_CPU_FETCH_SUCCESS;
}

// ============================================================================
// Execution
// ============================================================================

// Runs an instruction whose opcode table entry the caller already has, and
// which the caller has already established the entry agrees with.
//
// Kept out of line. Inlined into YCPUTick() this measured 31% slower on a
// Cortex-M0+: the execute path wants registers, the core has few, and folding
// the two together makes both spill. It only shows up once the hot path is in
// SRAM - from flash the XIP cache dominates and hides it.
Y_FILE_PRIVATE Y_HOT Y_NOINLINE YCPUInstructionResult
YCPUExecuteDecodedInstruction(
    YCPUState* cpu, YCPUInstruction* instruction,
    const YCPUOpcodeMetadata* metadata) {
  // Run the instruction handler.
  YCPUInstructionContext context = {
      .cpu = cpu,
      .instruction = instruction,
      .metadata = metadata,
  };
  return metadata->handler(&context);
}

// Checks an instruction against the opcode table before running it.
//
// For a caller that built the YCPUInstruction itself rather than decoding one -
// YCPUTick() goes straight to YCPUExecuteDecodedInstruction(), because its own
// decode is what produced the encoding these checks would be re-examining.
Y_PUBLIC Y_HOT YCPUInstructionResult
YCPUExecuteInstruction(YCPUState* cpu, YCPUInstruction* instruction) {
  const YCPUOpcodeMetadata* metadata = &YCPUOpcodeTable[instruction->opcode];

  // Check the encoded instruction against the expected format for its opcode.
  // Nothing checks that the opcode has a handler, because every entry in the
  // table has one: the eight bytes that are prefixes rather than instructions
  // are handled by YCPUExecuteInvalidOpcode(), which returns what a missing
  // handler used to.
  if (instruction->has_mod_rm != metadata->has_modrm) {
    return Y_CPU_INSTRUCTION_INVALID;
  }
  if (instruction->immediate_size !=
      (metadata->has_modrm
           ? YCPUGetImmediateSize(
                 metadata, instruction->opcode, instruction->mod_rm.reg)
           : metadata->immediate_size)) {
    return Y_CPU_INSTRUCTION_INVALID;
  }

  return YCPUExecuteDecodedInstruction(cpu, instruction, metadata);
}

// Save state and vector to the handler for an interrupt.
Y_FILE_PRIVATE void YCPUDispatchInterrupt(
    YCPUState* cpu, uint8_t interrupt_number) {
  // Prepare for interrupt processing.
  cpu->is_halted = false;
  YCPUPushValue(cpu, cpu->flags);
  YCPUSetFlag(cpu, Y_CPU_IF, false);
  YCPUSetFlag(cpu, Y_CPU_TF, false);
  YCPUPushValue(cpu, cpu->registers[Y_CPU_CS]);
  YCPUPushValue(cpu, cpu->registers[Y_CPU_IP]);

  // Invoke the interrupt handler callback first. If the caller did not provide
  // an interrupt handler callback, handle the interrupt within the VM using the
  // Interrupt Vector Table.
  YCPUInterruptHandlerResult interrupt_handler_result =
      cpu->config.handle_interrupt
          ? cpu->config.handle_interrupt(cpu, interrupt_number)
          : Y_CPU_INTERRUPT_HANDLER_UNHANDLED;

  if (interrupt_handler_result == Y_CPU_INTERRUPT_HANDLER_HANDLED) {
    // If the interrupt was handled by the caller-provided interrupt handler
    // callback, restore state and continue execution.
    YCPUExecuteReturnFromInterrupt(cpu);
    return;
  }

  // If the interrupt was not handled by the caller-provided interrupt handler
  // callback, handle it within the VM using the Interrupt Vector Table.
  uint16_t ivt_entry_offset = interrupt_number << 2;
  cpu->registers[Y_CPU_IP] = YCPUReadRawMemoryWord(cpu, ivt_entry_offset);
  cpu->registers[Y_CPU_CS] = YCPUReadRawMemoryWord(cpu, ivt_entry_offset + 2);
}

// Take a pending interrupt, if any. Returns whether one was dispatched.
Y_FILE_PRIVATE bool YCPUExecutePendingInterrupt(YCPUState* cpu) {
  // An internal interrupt goes first. It was raised by the instruction that
  // just executed, and taking it clears IF, which correctly holds off any
  // external request until the handler re-enables interrupts.
  if (cpu->has_pending_internal_interrupt) {
    const uint8_t interrupt_number = cpu->pending_internal_interrupt_number;
    YCPUClearInternalInterrupt(cpu);
    YCPUDispatchInterrupt(cpu, interrupt_number);
    return true;
  }

  // An external request on the INTR pin is only taken while interrupts are
  // enabled. Acknowledging it is what produces its vector - there is nothing to
  // latch beforehand, and the controller keeps requesting until acknowledged.
  //
  // The hint is read first where the host supplies one. It is a load against an
  // indirect call into a controller that almost always reports nothing, and
  // this runs at every instruction boundary. A host that supplies none is
  // asked every time.
  const bool* const request_hint = cpu->config.interrupt_request_hint;
  uint8_t intr_vector;
  if (YCPUGetFlag(cpu, Y_CPU_IF) && (request_hint == NULL || *request_hint) &&
      cpu->config.acknowledge_interrupt &&
      cpu->config.acknowledge_interrupt(cpu, &intr_vector)) {
    YCPUDispatchInterrupt(cpu, intr_vector);
    return true;
  }

  return false;
}

// ============================================================================
// Runs of instructions
// ============================================================================
//
// A tick runs several instructions back to back where it can. What that saves
// is the round trip: YPlatformTick()'s deadline test and stop checks, and
// YCPUTick()'s own prologue and epilogue, all of which are per tick rather than
// per instruction.
//
// Two things bound a run: YCPUTick()'s max_run_cycles, so that a device
// deadline is not passed over, and Y_CPU_MAX_INSTRUCTIONS_PER_TICK.

enum {
  // IN and OUT are two families of four - 0xE4-0xE7 takes the port number as
  // an immediate, 0xEC-0xEF takes it in DX - differing only in bit 3. Leaving
  // that bit free along with the two that select direction and width matches
  // both families and nothing else.
  Y_CPU_PORT_INSTRUCTION_MASK = 0xF4,
  Y_CPU_PORT_INSTRUCTION_VALUE = 0xE4,
};

// Whether an opcode reads or writes an I/O port.
Y_FILE_PRIVATE Y_ALWAYS_INLINE bool YCPUIsPortInstruction(uint8_t opcode) {
  return (opcode & Y_CPU_PORT_INSTRUCTION_MASK) == Y_CPU_PORT_INSTRUCTION_VALUE;
}

// Whether a run may carry on into the instruction after the one just executed.
//
// Everything here is something the end of a tick would otherwise have dealt
// with, and which running another instruction first would deal with too late.
Y_FILE_PRIVATE Y_ALWAYS_INLINE bool YCPUCanContinueRun(
    const YCPUState* cpu, uint8_t opcode, uint16_t max_run_cycles) {
  return
      // A budget of zero is how a host asks for one instruction per tick.
      cpu->pending_cycles < max_run_cycles &&
      // INT n, INTO and a divide error dispatch at the end of the tick.
      !cpu->has_pending_internal_interrupt &&
      // The test that skips execution while halted is made before the run
      // starts.
      !cpu->is_halted &&
      // IN and OUT can reprogram a device and move the deadline the budget was
      // computed from. The other half of this rule is at the call site.
      !YCPUIsPortInstruction(opcode) &&
      // An instruction that sets TF - POPF, IRET - must be the last of its
      // run, or the next one runs without trapping.
      !YCPUGetFlag(cpu, Y_CPU_TF) &&
      // STI, POPF and IRET can enable interrupts with a request already
      // waiting. Without a hint that cannot be told without an acknowledge
      // cycle, so a host supplying none ends the run here.
      cpu->config.interrupt_request_hint != NULL &&
      !*cpu->config.interrupt_request_hint;
}

// The entry holding the decoded instruction at CS:IP if the cache holds a
// current one, and NULL otherwise. The entry rather than the instruction for
// the same reason the fetch hands one back - see there.
//
// A run carries on only into a cached instruction, which keeps a step cheap
// and keeps a cold CPU to one instruction per tick however large its budget.
Y_FILE_PRIVATE Y_HOT YCPUDecodeCacheEntry* YCPUCachedEntryAtIP(YCPUState* cpu) {
  YCPUDecodeCacheEntry* const cache = cpu->config.decode_cache;
  if (cache == NULL) {
    return NULL;
  }
  const uint32_t address =
      YCPUToRawAddress(cpu, Y_CPU_CS, cpu->registers[Y_CPU_IP]);
  YCPUDecodeCacheEntry* const entry =
      &cache[address & cpu->decode_cache_index_mask];
  if (!YCPUIsDecodeCacheHit(
          entry, address,
          cpu->code_page_generation[address >> Y_CPU_CODE_PAGE_SHIFT])) {
    return NULL;
  }
  return entry;
}

Y_PUBLIC Y_HOT YCPUTickResult
YCPUTick(YCPUState* cpu, uint16_t max_run_cycles) {
  // Whether this tick ran an instruction. A halted CPU runs none until an
  // interrupt wakes it.
  bool executed_instruction = false;

  // A halted CPU still consumes time - it is sitting in a wait state, not
  // stopped - so a tick that runs no instruction still has to advance the
  // clock, or the timer that is meant to wake it would never tick either.
  cpu->pending_cycles = 0;
  cpu->cycles_this_tick = Y_CPU_HALTED_CYCLES;

  // The trap flag is sampled before the instruction runs, not after. An
  // instruction that sets TF - POPF or IRET - must not trap on itself, and one
  // that clears TF still traps once for the instruction it was set during.
  const bool trap_flag_was_set = YCPUGetFlag(cpu, Y_CPU_TF);

  // Execute next CPU instruction if not halted.
  if (!cpu->is_halted) {
    // Step 1: Fetch the next instruction.
    //
    // The local entry is only where a decode lands when there is no cache to
    // decode into. What runs is whatever the fetch points at - the cost of the
    // instruction rides in the YCPUInstruction, so nothing here reads the entry
    // for anything but that.
    YCPUDecodeCacheEntry scratch;
    YCPUDecodeCacheEntry* entry;
    YCPUFetchNextInstructionStatus fetch_status =
        YCPUFetchNextInstructionCached(cpu, &scratch, &entry);
    if (fetch_status != Y_CPU_FETCH_SUCCESS) {
      // CS:IP, which the logger adds, is where the fetch started.
      Y_CPU_ERROR(
          "instruction_fetch_failed", Y_LOG_INT("status", fetch_status));
      return Y_CPU_TICK_INVALID;
    }

    // A successful fetch is always followed by an instruction: the only way
    // out of the loop below without running one is a return.
    executed_instruction = true;

    // A tick that has to trap runs exactly one instruction: TF was set when it
    // started, so the single-step interrupt belongs to the instruction about
    // to run, and one that clears TF still owes that trap.
    uint8_t remaining =
        trap_flag_was_set ? 1 : (uint8_t)Y_CPU_MAX_INSTRUCTIONS_PER_TICK;

    for (;;) {
      YCPUInstruction* const instruction = &entry->instruction;
      const uint16_t instruction_cs = cpu->registers[Y_CPU_CS];
      const uint16_t instruction_ip = cpu->registers[Y_CPU_IP];
      cpu->registers[Y_CPU_IP] += instruction->size;

      // What the instruction costs before it runs came with the decode; its
      // bus traffic and anything that depends on its operands it charges
      // itself as it runs. A run accumulates all of it, which is what the
      // budget below is spent against.
      YCPUAddCycles(cpu, instruction->base_cycles);

      // Step 2: Execute the instruction. The fetch above derived has_mod_rm
      // and immediate_size from this same table entry, so the checks
      // YCPUExecuteInstruction() makes cannot fail here.
      const YCPUOpcodeMetadata* const metadata =
          &YCPUOpcodeTable[instruction->opcode];
      if (YCPUExecuteDecodedInstruction(cpu, instruction, metadata) !=
          Y_CPU_INSTRUCTION_EXECUTED) {
        // IP has already moved past the instruction, so its own address
        // goes in as well.
        Y_CPU_ERROR(
            "invalid_instruction", Y_LOG_HEX8("opcode", instruction->opcode),
            Y_LOG_HEX16("instruction_cs", instruction_cs),
            Y_LOG_HEX16("instruction_ip", instruction_ip));
        return Y_CPU_TICK_INVALID;
      }
      // The carry is a branch not taken until 2^32 instructions have run;
      // see instructions_retired_low for why the count is split.
      if (++cpu->instructions_retired_low == 0) {
        ++cpu->instructions_retired_high;
      }

      // Step 3: Carry on into the next instruction where nothing needs the
      // host's attention and the cache already holds it.
      if (--remaining == 0 ||
          !YCPUCanContinueRun(cpu, instruction->opcode, max_run_cycles)) {
        break;
      }
      entry = YCPUCachedEntryAtIP(cpu);
      // A port handler calls YPlatformSync(), making it the one instruction
      // that looks at the clock - and part way through a run the clock stops
      // short of the instructions already run in it, since a tick's cost is
      // reported once it is over. Ending before one leaves every port access
      // the first instruction of its tick.
      if (entry == NULL || YCPUIsPortInstruction(entry->instruction.opcode)) {
        break;
      }
    }
    cpu->cycles_this_tick = cpu->pending_cycles;
  }

  // Step 4: Handle a pending interrupt. This runs even while halted, because
  // an interrupt is the only thing that can clear the halted state -
  // YCPUExecutePendingInterrupt() resets is_halted when it dispatches one.
  const bool dispatched_interrupt = YCPUExecutePendingInterrupt(cpu);

  // Step 5: The trap flag raises a single-step interrupt after an instruction
  // executes, so a halted CPU must not trap - otherwise the trap would wake it
  // and then fire again on every subsequent tick.
  //
  // Single-stepping is the lowest priority of the interrupt sources recognized
  // at an instruction boundary, so an interrupt dispatched above takes its
  // place rather than both firing.
  if (executed_instruction && trap_flag_was_set && !dispatched_interrupt) {
    YCPURaiseInternalInterrupt(cpu, Y_CPU_INTERRUPT_SINGLE_STEP);
    YCPUExecutePendingInterrupt(cpu);
  }

  // This reports what the tick did, not what state the CPU ended up in. A tick
  // that executes HLT ran an instruction, so it reports Y_CPU_TICK_EXECUTED
  // even though the CPU is now halted; the ticks that follow report
  // Y_CPU_TICK_HALTED.
  return executed_instruction ? Y_CPU_TICK_EXECUTED : Y_CPU_TICK_HALTED;
}


// ==============================================================================
// src/cpu/cpu.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_CPU_BUNDLE_H

// ==============================================================================
// YAX86 DMA MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_DMA_BUNDLE_H
#define YAX86_DMA_BUNDLE_H

#ifndef YAX86_UTIL_BUNDLE_H
#include "util.h"
#endif  // YAX86_UTIL_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/dma/public.h start
// ==============================================================================

#line 1 "./src/dma/public.h"
// Public interface for the DMA (Direct Memory Access) module.
#ifndef YAX86_DMA_PUBLIC_H
#define YAX86_DMA_PUBLIC_H

// This module emulates the Intel 8237 DMA controller used in the IBM PC/XT.
// The DMA controller allows peripherals to transfer data directly to and from
// memory without involving the CPU, which is critical for high-speed devices
// like disk drives.
//
// The standard channel assignments are:
// - Channel 0: DRAM Refresh
// - Channel 1: Unused / Expansion
// - Channel 2: Floppy Disk Controller
// - Channel 3: Hard Disk Controller
//
// Note that we do not support all features of the 8237, only those needed to
// support GLaBIOS and basic PC/XT peripherals. Specifically:
// - DRAM Refresh on Channel 0 is not implemented, as it is disabled in the
//   target GLaBIOS build for emulators.
// - Memory-to-memory transfers are not supported.
// - Cascade Mode for multiple DMA controllers is not supported.
// - Advanced transfer modes (Demand, Block) and priorities (Rotating) are not
//   supported. Only Single Cycle mode with Fixed Priority is implemented.

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_DMA_BUNDLE_H
#include "../util/log.h"
#endif  // YAX86_DMA_BUNDLE_H

enum {
  // Log module ID for the DMA.
  Y_DMA_LOG_MODULE_ID = 6,
};

// Log module for the DMA.
Y_PUBLIC_HEADER const YLogModule YDMALogModule = {
    .id = Y_DMA_LOG_MODULE_ID,
    .name = "dma",
};

// I/O ports for the 8237 DMA Controller and Page Registers.
typedef enum YDMAPort {
  // --- 8237 DMA Controller ---
  // Channel 0 base and current address
  Y_DMA_PORT_CHANNEL0_ADDRESS = 0x00,
  // Channel 0 base and current word count
  Y_DMA_PORT_CHANNEL0_COUNT = 0x01,
  // Channel 1 base and current address
  Y_DMA_PORT_CHANNEL1_ADDRESS = 0x02,
  // Channel 1 base and current word count
  Y_DMA_PORT_CHANNEL1_COUNT = 0x03,
  // Channel 2 base and current address
  Y_DMA_PORT_CHANNEL2_ADDRESS = 0x04,
  // Channel 2 base and current word count
  Y_DMA_PORT_CHANNEL2_COUNT = 0x05,
  // Channel 3 base and current address
  Y_DMA_PORT_CHANNEL3_ADDRESS = 0x06,
  // Channel 3 base and current word count
  Y_DMA_PORT_CHANNEL3_COUNT = 0x07,
  // Read: Status Register / Write: Command Register
  Y_DMA_PORT_COMMAND_STATUS = 0x08,
  // Write: Request Register
  Y_DMA_PORT_REQUEST = 0x09,
  // Write: Set/Clear a single channel's mask bit
  Y_DMA_PORT_SINGLE_MASK = 0x0A,
  // Write: Mode Register
  Y_DMA_PORT_MODE = 0x0B,
  // Write: Clear Byte Pointer Flip-Flop
  Y_DMA_PORT_FLIP_FLOP_RESET = 0x0C,
  // Write: Master Reset
  Y_DMA_PORT_MASTER_RESET = 0x0D,
  // Write: Mask Register (for all channels)
  Y_DMA_PORT_ALL_MASK = 0x0F,

  // --- 74LS670 Page Registers ---
  // Page register for Channel 2 (Floppy)
  Y_DMA_PORT_PAGE_CHANNEL2 = 0x81,
  // Page register for Channel 3 (Hard Drive)
  Y_DMA_PORT_PAGE_CHANNEL3 = 0x82,
  // Page register for Channel 1
  Y_DMA_PORT_PAGE_CHANNEL1 = 0x83,
  // Page register for Channel 0
  Y_DMA_PORT_PAGE_CHANNEL0 = 0x87,
} YDMAPort;

// Bit definitions for the Mode Register (Port 0x0B)
enum {
  // --- Channel Select (bits 0-1) ---
  // Select channel 0
  Y_DMA_MODE_SELECT_CHANNEL0 = 0x00,
  // Select channel 1
  Y_DMA_MODE_SELECT_CHANNEL1 = 0x01,
  // Select channel 2
  Y_DMA_MODE_SELECT_CHANNEL2 = 0x02,
  // Select channel 3
  Y_DMA_MODE_SELECT_CHANNEL3 = 0x03,

  // --- Transfer Type (bits 2-3) ---
  // Verify transfer (no data is moved)
  Y_DMA_MODE_TRANSFER_TYPE_VERIFY = 0x00,
  // Write to memory (device -> memory)
  Y_DMA_MODE_TRANSFER_TYPE_WRITE = 0x04,
  // Read from memory (memory -> device)
  Y_DMA_MODE_TRANSFER_TYPE_READ = 0x08,

  // --- Auto-initialization (bit 4) ---
  // If set, the channel reloads its base address and count after a transfer.
  Y_DMA_MODE_AUTO_INITIALIZE = 0x10,

  // --- Address Direction (bit 5) ---
  // If set, the memory address is decremented; otherwise, it is incremented.
  Y_DMA_MODE_ADDRESS_DECREMENT = 0x20,

  // --- Transfer Mode (bits 6-7) ---
  // Demand mode: transfer bytes until the DREQ line becomes inactive.
  Y_DMA_MODE_DEMAND = 0x00,
  // Single mode: transfer one byte for each DREQ signal.
  Y_DMA_MODE_SINGLE = 0x40,
  // Block mode: transfer an entire block of data in response to a single DREQ.
  Y_DMA_MODE_BLOCK = 0x80,
  // Cascade mode: used for chaining multiple DMA controllers (not supported).
  Y_DMA_MODE_CASCADE = 0xC0,
};

enum {
  // Number of DMA channels in the controller.
  Y_DMA_NUM_CHANNELS = 4,
};

// ============================================================================
// DMA state
// ============================================================================

struct YDMAState;

// Caller-provided runtime configuration for the DMA controller.
typedef struct YDMAConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger for this module. May be NULL.
  YLogger* logger;

  // Callback to read a byte from system memory.
  uint8_t (*read_memory_byte)(void* context, uint32_t address);
  // Callback to write a byte to system memory.
  void (*write_memory_byte)(void* context, uint32_t address, uint8_t value);

  // Callback to read a byte from a peripheral for a specific DMA channel.
  uint8_t (*read_device_byte)(void* context, uint8_t channel);
  // Callback to write a byte to a peripheral for a specific DMA channel.
  void (*write_device_byte)(void* context, uint8_t channel, uint8_t value);

  // Callback to notify the system that a channel has reached its terminal
  // count. This corresponds to the EOP (End of Process) signal on the 8237,
  // which is connected to the TC (Terminal Count) pin on devices like the FDC.
  void (*on_terminal_count)(void* context, uint8_t channel);
} YDMAConfig;

// State for a single DMA channel.
typedef struct YDMAChannelState {
  // Base address register, reloaded on auto-initialization.
  uint16_t base_address;
  // Current address register, updated during a transfer.
  uint16_t current_address;
  // Base count register, reloaded on auto-initialization.
  uint16_t base_count;
  // Current count register, updated during a transfer.
  uint16_t current_count;
  // Mode register for this channel.
  uint8_t mode;
  // High-order address bits from the page register.
  uint8_t page_register;
} YDMAChannelState;

// Which register byte to read/write next.
typedef enum YDMARegisterByte {
  // Read or write the lower byte next.
  Y_DMA_REGISTER_LSB = 0,
  // Read or write the upper byte next.
  Y_DMA_REGISTER_MSB = 1,
} YDMARegisterByte;

// State for the entire 8237 DMA controller.
typedef struct YDMAState {
  // Pointer to the DMA configuration.
  YDMAConfig config;

  // The four DMA channels.
  YDMAChannelState channels[Y_DMA_NUM_CHANNELS];

  // Command register for the controller.
  uint8_t command_register;
  // Status register (Terminal Count and Request flags).
  uint8_t status_register;
  // Software request register.
  uint8_t request_register;
  // Mask register for all four channels.
  uint8_t mask_register;

  // Internal byte flip-flop for 16-bit register access.
  YDMARegisterByte rw_byte;
} YDMAState;

// ============================================================================
// DMA interface
// ============================================================================

// Initializes the DMA state to its power-on default.
Y_PUBLIC void YDMAInit(YDMAState* dma);

// Handles reads from the DMA's I/O ports.
Y_PUBLIC uint8_t YDMAReadPort(YDMAState* dma, uint16_t port);

// Handles writes to the DMA's I/O ports.
Y_PUBLIC void YDMAWritePort(YDMAState* dma, uint16_t port, uint8_t value);

// Executes a single-byte transfer for the specified channel. This function
// should be called by the platform in response to a DREQ signal from a
// peripheral.
Y_PUBLIC void YDMATransferByte(YDMAState* dma, uint8_t channel_index);

#endif  // YAX86_DMA_PUBLIC_H


// ==============================================================================
// src/dma/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/dma/dma.c start
// ==============================================================================

#line 1 "./src/dma/dma.c"
#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

#define Y_DMA_ERROR(...) \
  Y_LOG(dma->config.logger, &YDMALogModule, Y_LOG_LEVEL_ERROR, __VA_ARGS__)
#define Y_DMA_WARN(...) \
  Y_LOG(dma->config.logger, &YDMALogModule, Y_LOG_LEVEL_WARN, __VA_ARGS__)
#define Y_DMA_INFO(...) \
  Y_LOG(dma->config.logger, &YDMALogModule, Y_LOG_LEVEL_INFO, __VA_ARGS__)
#define Y_DMA_DEBUG(...) \
  Y_LOG(dma->config.logger, &YDMALogModule, Y_LOG_LEVEL_DEBUG, __VA_ARGS__)

Y_PUBLIC void YDMAInit(YDMAState* dma) {
  // Mask all channels by default on power-on.
  dma->mask_register = 0x0F;
}

// Helper to read a 16-bit value byte-by-byte using the flip-flop.
Y_FILE_PRIVATE inline uint8_t YDMAReadRegisterByte(
    YDMAState* dma, uint16_t value) {
  uint8_t byte;
  if (dma->rw_byte == Y_DMA_REGISTER_MSB) {
    byte = (value >> 8) & 0xFF;
    dma->rw_byte = Y_DMA_REGISTER_LSB;
  } else {
    byte = value & 0xFF;
    dma->rw_byte = Y_DMA_REGISTER_MSB;
  }
  return byte;
}

Y_PUBLIC uint8_t YDMAReadPort(YDMAState* dma, uint16_t port) {
  switch (port) {
    // Channel Address and Count Registers (ports 0x00-0x07)
    case Y_DMA_PORT_CHANNEL0_ADDRESS:
    case Y_DMA_PORT_CHANNEL0_COUNT:
    case Y_DMA_PORT_CHANNEL1_ADDRESS:
    case Y_DMA_PORT_CHANNEL1_COUNT:
    case Y_DMA_PORT_CHANNEL2_ADDRESS:
    case Y_DMA_PORT_CHANNEL2_COUNT:
    case Y_DMA_PORT_CHANNEL3_ADDRESS:
    case Y_DMA_PORT_CHANNEL3_COUNT: {
      const int channel_index = port / 2;
      const bool is_count_register = port % 2;
      const YDMAChannelState* channel = &dma->channels[channel_index];
      return YDMAReadRegisterByte(
          dma, is_count_register ? channel->current_count
                                 : channel->current_address);
    }

    // Status Register (port 0x08)
    case Y_DMA_PORT_COMMAND_STATUS: {
      uint8_t status = dma->status_register;
      dma->status_register = 0;  // Clear TC flags on read
      return status;
    }

    // All other ports are write-only or unused for reads.
    default:
      return 0xFF;
  }
}

// Helper to write a 16-bit value byte-by-byte using the flip-flop.
// Note: Writes update both the 'base' and 'current' registers.
Y_FILE_PRIVATE inline void YDMAWriteRegisterByte(
    YDMAState* dma, uint16_t* base_reg, uint16_t* current_reg, uint8_t value) {
  if (dma->rw_byte == Y_DMA_REGISTER_MSB) {
    // Second write sets the high byte.
    *base_reg = (*base_reg & 0x00FF) | ((uint16_t)value << 8);
    dma->rw_byte = Y_DMA_REGISTER_LSB;
  } else {
    // First write sets the low byte.
    *base_reg = (*base_reg & 0xFF00) | value;
    dma->rw_byte = Y_DMA_REGISTER_MSB;
  }
  // The 'current' register always mirrors the 'base' register after a write.
  *current_reg = *base_reg;
}

Y_PUBLIC void YDMAWritePort(YDMAState* dma, uint16_t port, uint8_t value) {
  switch (port) {
    // Channel Address and Count Registers (ports 0x00-0x07)
    case Y_DMA_PORT_CHANNEL0_ADDRESS:
    case Y_DMA_PORT_CHANNEL0_COUNT:
    case Y_DMA_PORT_CHANNEL1_ADDRESS:
    case Y_DMA_PORT_CHANNEL1_COUNT:
    case Y_DMA_PORT_CHANNEL2_ADDRESS:
    case Y_DMA_PORT_CHANNEL2_COUNT:
    case Y_DMA_PORT_CHANNEL3_ADDRESS:
    case Y_DMA_PORT_CHANNEL3_COUNT: {
      const int channel_index = port / 2;
      const bool is_count_register = port % 2;
      YDMAChannelState* channel = &dma->channels[channel_index];

      if (is_count_register) {
        YDMAWriteRegisterByte(
            dma, &channel->base_count, &channel->current_count, value);
      } else {
        YDMAWriteRegisterByte(
            dma, &channel->base_address, &channel->current_address, value);
      }
      break;
    }

    // Command Register (port 0x08)
    case Y_DMA_PORT_COMMAND_STATUS:
      dma->command_register = value;
      break;

    // Request Register (port 0x09)
    case Y_DMA_PORT_REQUEST:
      dma->request_register = value;
      break;

    // Single Mask Register (port 0x0A)
    case Y_DMA_PORT_SINGLE_MASK: {
      const int channel_index = value & 0x03;
      const bool should_mask = (value >> 2) & 1;
      if (should_mask) {
        dma->mask_register |= (1 << channel_index);
      } else {
        dma->mask_register &= ~(1 << channel_index);
        // Unmasking is the point at which a channel becomes live, so the
        // address, count and page registers are final here.
        const YDMAChannelState* channel = &dma->channels[channel_index];
        Y_DMA_DEBUG(
            "channel_enabled", Y_LOG_UINT("channel", channel_index),
            Y_LOG_HEX8("page", channel->page_register),
            Y_LOG_HEX16("address", channel->current_address),
            Y_LOG_HEX16("count", channel->current_count),
            Y_LOG_HEX8("mode", channel->mode));
      }
      break;
    }

    // Mode Register (port 0x0B)
    case Y_DMA_PORT_MODE: {
      const int channel_index = value & 0x03;
      dma->channels[channel_index].mode = value;
      break;
    }

    // Clear Byte Pointer Flip-Flop (port 0x0C)
    case Y_DMA_PORT_FLIP_FLOP_RESET:
      dma->rw_byte = Y_DMA_REGISTER_LSB;
      break;

    // Master Reset (port 0x0D)
    case Y_DMA_PORT_MASTER_RESET:
      // A master reset returns the controller to its power-on state, so
      // unlike YDMAInit() this does have to clear what is there. The config is
      // what the host wired up and survives - a reset is the chip's, not the
      // machine's. Members left out of an initializer are zeroed, so naming
      // the one that stays is the whole of it.
      *dma = (YDMAState){.config = dma->config};
      YDMAInit(dma);
      break;

    // Mask Register for all channels (port 0x0F)
    case Y_DMA_PORT_ALL_MASK:
      dma->mask_register = value & 0x0F;
      break;

    // Page Registers
    case Y_DMA_PORT_PAGE_CHANNEL0:
      dma->channels[0].page_register = value;
      break;
    case Y_DMA_PORT_PAGE_CHANNEL1:
      dma->channels[1].page_register = value;
      break;
    case Y_DMA_PORT_PAGE_CHANNEL2:
      dma->channels[2].page_register = value;
      break;
    case Y_DMA_PORT_PAGE_CHANNEL3:
      dma->channels[3].page_register = value;
      break;

    default:
      // Ignore writes to read-only or unused ports.
      break;
  }
}

Y_PUBLIC Y_HOT void YDMATransferByte(YDMAState* dma, uint8_t channel_index) {
  if (channel_index >= Y_DMA_NUM_CHANNELS) {
    return;
  }
  YDMAChannelState* channel = &dma->channels[channel_index];

  // Check if controller is disabled (bit 2 of command register).
  if ((dma->command_register & 0x04) != 0) {
    return;
  }

  // If channel is masked, do nothing.
  if ((dma->mask_register & (1 << channel_index)) != 0) {
    return;
  }

  // Construct full 20-bit memory address
  const uint32_t address =
      ((uint32_t)channel->page_register << 16) | channel->current_address;

  // Perform transfer based on type (bits 2-3 of mode register)
  const uint8_t transfer_type = channel->mode & (0x03 << 2);
  switch (transfer_type) {
    case Y_DMA_MODE_TRANSFER_TYPE_VERIFY:  // Verify - no actual transfer
      break;
    case Y_DMA_MODE_TRANSFER_TYPE_WRITE:  // Write to memory (device -> memory)
      if (dma->config.read_device_byte && dma->config.write_memory_byte) {
        const uint8_t data =
            dma->config.read_device_byte(dma->config.context, channel_index);
        dma->config.write_memory_byte(dma->config.context, address, data);
      }
      break;
    case Y_DMA_MODE_TRANSFER_TYPE_READ:  // Read from memory (memory -> device)
      if (dma->config.read_memory_byte && dma->config.write_device_byte) {
        const uint8_t data =
            dma->config.read_memory_byte(dma->config.context, address);
        dma->config.write_device_byte(dma->config.context, channel_index, data);
      }
      break;
    default:
      // Invalid/reserved mode, do nothing
      break;
  }

  // Update address register
  if ((channel->mode & Y_DMA_MODE_ADDRESS_DECREMENT) == 0) {
    ++channel->current_address;
  } else {
    --channel->current_address;
  }

  // Update count register and check for Terminal Count (TC)
  --channel->current_count;
  if (channel->current_count == 0xFFFF) {
    // Set TC bit in status register
    dma->status_register |= (1 << channel_index);

    // Notify the system that TC has been reached.
    if (dma->config.on_terminal_count) {
      dma->config.on_terminal_count(dma->config.context, channel_index);
    }

    // Handle auto-initialization or mask the channel
    if ((channel->mode & Y_DMA_MODE_AUTO_INITIALIZE) != 0) {
      channel->current_address = channel->base_address;
      channel->current_count = channel->base_count;
    } else {
      dma->mask_register |= (1 << channel_index);
    }
  }
}


// ==============================================================================
// src/dma/dma.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_DMA_BUNDLE_H

// ==============================================================================
// YAX86 FDC MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_FDC_BUNDLE_H
#define YAX86_FDC_BUNDLE_H

#ifndef YAX86_UTIL_BUNDLE_H
#include "util.h"
#endif  // YAX86_UTIL_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/fdc/public.h start
// ==============================================================================

#line 1 "./src/fdc/public.h"
// Public interface for the Floppy Disk Controller (FDC) module.
#ifndef YAX86_FDC_PUBLIC_H
#define YAX86_FDC_PUBLIC_H

// This module emulates the NEC uPD765 Floppy Disk Controller. It handles I/O
// port communication and DMA transfers for floppy operations. Actual disk
// image access is delegated to the platform via callbacks.

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_FDC_BUNDLE_H
#include "../util/log.h"
#include "../util/static_vector.h"
#endif  // YAX86_FDC_BUNDLE_H

enum {
  // Log module ID for the FDC.
  Y_FDC_LOG_MODULE_ID = 5,
};

// Log module for the FDC.
Y_PUBLIC_HEADER const YLogModule YFDCLogModule = {
    .id = Y_FDC_LOG_MODULE_ID,
    .name = "fdc",
};

// Floppy disk format configuration.
typedef struct YFDCDiskFormat {
  // Number of heads (1 or 2).
  uint8_t num_heads;
  // Number of tracks.
  uint8_t num_tracks;
  // Number of sectors per track.
  uint8_t num_sectors_per_track;
  // Size of each sector in bytes.
  uint16_t sector_size;
} YFDCDiskFormat;

// 5.25" 360KB double-sided double-density floppy disk format.
Y_PUBLIC_HEADER const YFDCDiskFormat YFDCFormat360KB = {
    .num_heads = 2,
    .num_tracks = 40,
    .num_sectors_per_track = 9,
    .sector_size = 512,
};

struct YFDCState;

enum {
  // Number of floppy drives supported by the FDC.
  Y_FDC_NUM_DRIVES = 4,
  // Maximum size of a command request.
  Y_FDC_COMMAND_BUFFER_SIZE = 9,
  // Maximum size of a command result.
  Y_FDC_RESULT_BUFFER_SIZE = 7,
};

// Command phases of the FDC.
typedef enum YFDCCommandPhase {
  // No command in progress.
  Y_FDC_PHASE_IDLE = 0,
  // Command has been issued, waiting for parameters.
  Y_FDC_PHASE_COMMAND,
  // Command parameters received, executing command.
  Y_FDC_PHASE_EXECUTION,
  // Command execution complete, sending result bytes.
  Y_FDC_PHASE_RESULT,
} YFDCCommandPhase;

// I/O ports for the FDC.
typedef enum YFDCPort {
  // Digital Output Register (write-only).
  Y_FDC_PORT_DOR = 0x3F2,
  // Main Status Register (read-only).
  Y_FDC_PORT_MSR = 0x3F4,
  // Data Register (read/write).
  Y_FDC_PORT_DATA = 0x3F5,
} YFDCPort;

// Flags for the Main Status Register (MSR).
enum {
  // Drive 0 is busy with a seek or recalibrate command.
  Y_FDC_MSR_DRIVE0_BUSY = 1 << 0,
  // Drive 1 is busy with a seek or recalibrate command.
  Y_FDC_MSR_DRIVE1_BUSY = 1 << 1,
  // Drive 2 is busy with a seek or recalibrate command.
  Y_FDC_MSR_DRIVE2_BUSY = 1 << 2,
  // Drive 3 is busy with a seek or recalibrate command.
  Y_FDC_MSR_DRIVE3_BUSY = 1 << 3,
  // A command is in progress.
  Y_FDC_MSR_BUSY = 1 << 4,
  // The FDC is in non-DMA mode.
  Y_FDC_MSR_NON_DMA_MODE = 1 << 5,
  // Indicates direction of data transfer. 0 = write to FDC, 1 = read from FDC.
  Y_FDC_MSR_DATA_DIRECTION = 1 << 6,
  // The Data Register is ready to send or receive data to/from the CPU.
  Y_FDC_MSR_REQUEST_FOR_MASTER = 1 << 7,
};

// Flags for the Digital Output Register (DOR).
enum {
  // Drive selection (0-3).
  Y_FDC_DOR_DRIVE_SELECT_MASK = 0x03,
  // Controller reset (0 = Reset active, 1 = Controller enabled).
  Y_FDC_DOR_RESET = 1 << 2,
  // DMA and Interrupt enable (1 = enabled).
  Y_FDC_DOR_INTERRUPT_ENABLE = 1 << 3,
  // Motor enable flags for drives 0-3.
  Y_FDC_DOR_MOTOR0_ENABLE = 1 << 4,
  Y_FDC_DOR_MOTOR1_ENABLE = 1 << 5,
  Y_FDC_DOR_MOTOR2_ENABLE = 1 << 6,
  Y_FDC_DOR_MOTOR3_ENABLE = 1 << 7,
};

// Flags for Status Register 0 (ST0).
enum {
  // Bits 7-6: Interrupt Code
  // 00 = Normal termination
  // 01 = Abnormal termination
  // 10 = Invalid command
  // 11 = Abnormal termination due to polling (Post-reset)
  Y_FDC_ST0_INTERRUPT_CODE_MASK = 0xC0,
  Y_FDC_ST0_NORMAL_TERMINATION = 0x00,
  Y_FDC_ST0_ABNORMAL_TERMINATION = 0x40,
  Y_FDC_ST0_INVALID_COMMAND = 0x80,
  Y_FDC_ST0_ABNORMAL_TERMINATION_POLLING = 0xC0,

  // Bit 5: Seek End
  Y_FDC_ST0_SEEK_END = 1 << 5,
  // Bit 4: Equipment Check
  Y_FDC_ST0_EQUIPMENT_CHECK = 1 << 4,
  // Bit 3: Not Ready
  Y_FDC_ST0_NOT_READY = 1 << 3,
  // Bit 2: Head Address
  Y_FDC_ST0_HEAD_ADDRESS = 1 << 2,
  // Bits 1-0: Drive Select
  Y_FDC_ST0_UNIT_SELECT_MASK = 0x03,
};

// Flags for Status Register 1 (ST1).
enum {
  // Bit 7: End of Cylinder
  Y_FDC_ST1_END_OF_CYLINDER = 1 << 7,
  // Bit 5: Data Error
  Y_FDC_ST1_DATA_ERROR = 1 << 5,
  // Bit 4: Overrun
  Y_FDC_ST1_OVERRUN = 1 << 4,
  // Bit 2: No Data
  Y_FDC_ST1_NO_DATA = 1 << 2,
  // Bit 1: Not Writable
  Y_FDC_ST1_NOT_WRITABLE = 1 << 1,
  // Bit 0: Missing Address Mark
  Y_FDC_ST1_MISSING_ADDRESS_MARK = 1 << 0,
};

// Flags for Status Register 2 (ST2).
enum {
  // Bit 6: Control Mark
  Y_FDC_ST2_CONTROL_MARK = 1 << 6,
  // Bit 5: Data Error in Data Field
  Y_FDC_ST2_DATA_ERROR_IN_DATA_FIELD = 1 << 5,
  // Bit 4: Wrong Cylinder
  Y_FDC_ST2_WRONG_CYLINDER = 1 << 4,
  // Bit 3: Scan Equal Hit
  Y_FDC_ST2_SCAN_EQUAL_HIT = 1 << 3,
  // Bit 2: Scan Not Satisfied
  Y_FDC_ST2_SCAN_NOT_SATISFIED = 1 << 2,
  // Bit 1: Bad Cylinder
  Y_FDC_ST2_BAD_CYLINDER = 1 << 1,
  // Bit 0: Missing Address Mark in Data Field
  Y_FDC_ST2_MISSING_ADDRESS_MARK_IN_DATA_FIELD = 1 << 0,
};

// State for a single floppy drive.
typedef struct YFDCDriveState {
  // Whether there is a disk inserted in the drive, i.e. whether an image is
  // mounted. The real hardware doesn't actually know this and will attempt to
  // access the disk and time out.
  bool present;
  // The format of the disk currently inserted in the drive.
  const YFDCDiskFormat* format;
  // The track the read/write head is currently on.
  uint8_t track;
  // The currently active head (0 or 1).
  uint8_t head;
  // Whether the drive is currently busy.
  bool busy;
  // Status Register 0 (ST0) for the last completed Seek or Recalibrate
  // operation on this drive.
  uint8_t st0;
  // Whether there is a pending interrupt from a completed Seek or Recalibrate
  // operation on this drive.
  bool has_pending_interrupt;
} YFDCDriveState;

// Caller-provided runtime configuration for the FDC.
typedef struct YFDCConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger for this module. May be NULL.
  YLogger* logger;

  // Callback to raise an IRQ6 (FDC interrupt) to the CPU.
  void (*raise_irq6)(void* context);

  // Callback to signal the platform to execute a DMA cycle for Channel 2.
  // This represents the DREQ (DMA Request) signal.
  void (*request_dma)(void* context);

  // Callback to read a byte from a floppy image.
  uint8_t (*read_image_byte)(
      void* context,
      // 0 to Y_FDC_NUM_DRIVES-1
      uint8_t drive,
      // byte offset within the image
      uint32_t offset);

  // Callback to write a byte to a floppy image.
  void (*write_image_byte)(
      void* context,
      // 0 to Y_FDC_NUM_DRIVES-1
      uint8_t drive,
      // byte offset within the image
      uint32_t offset,
      // byte value to write
      uint8_t value);
} YFDCConfig;

Y_STATIC_VECTOR_TYPE(YFDCCommandBuffer, uint8_t, Y_FDC_COMMAND_BUFFER_SIZE)
Y_STATIC_VECTOR_TYPE(YFDCResultBuffer, uint8_t, Y_FDC_RESULT_BUFFER_SIZE)

struct YFDCCommandMetadata;

// State of the Floppy Disk Controller.
typedef struct YFDCState {
  // Pointer to the FDC configuration.
  YFDCConfig config;

  // Value of the Digital Output Register (DOR) from the last write to port
  // 0x3F2.
  uint8_t dor;

  // Per-drive state.
  YFDCDriveState drives[Y_FDC_NUM_DRIVES];

  // Current command phase.
  YFDCCommandPhase phase;

  // Command buffer to receive command and parameters from the CPU.
  YFDCCommandBuffer command_buffer;
  // Metadata for the command currently being processed.
  const struct YFDCCommandMetadata* current_command;
  // How many ticks the current command has been executing.
  uint32_t current_command_ticks;

  // Result buffer to send to the CPU.
  YFDCResultBuffer result_buffer;
  // Next index to read from result buffer.
  uint8_t next_result_byte_index;

  // State specific to the execution of a data transfer command
  // (Read/Write/Format).
  struct {
    // Current logical position on the disk during transfer.
    uint8_t cylinder;
    uint8_t head;
    uint8_t sector;

    // Command parameters governing the transfer.
    uint8_t sector_size_code;  // N
    uint8_t eot;               // End of Track sector number
    bool multi_track;          // MT bit set (read/write across heads)

    // Internal tracking.
    uint32_t current_offset;  // Current byte offset in the disk image.
    uint16_t
        sector_byte_index;  // Current byte index within the current sector.
    uint8_t data_register;  // Buffer for the byte currently being transferred.
    bool dma_request_active;  // DREQ is asserted, waiting for DMA access.
    bool tc_received;         // TC (Terminal Count) signal received from DMA.
  } transfer;
} YFDCState;

// Initializes the FDC to its power-on state.
Y_PUBLIC void YFDCInit(YFDCState* fdc);

// Handles reads from the FDC's I/O ports.
Y_PUBLIC uint8_t YFDCReadPort(YFDCState* fdc, uint16_t port);

// Handles writes to the FDC's I/O ports.
Y_PUBLIC void YFDCWritePort(YFDCState* fdc, uint16_t port, uint8_t value);

// Signals to the FDC that the DMA controller has reached the terminal count.
// This represents the TC signal.
Y_PUBLIC void YFDCHandleTC(YFDCState* fdc);

// Inserts a disk with the given format into the specified drive.
Y_PUBLIC void YFDCInsertDisk(
    YFDCState* fdc, uint8_t drive, const YFDCDiskFormat* format);

// Ejects the disk from the specified drive.
Y_PUBLIC void YFDCEjectDisk(YFDCState* fdc, uint8_t drive);

// Simulates a tick of the FDC, handling any timed operations.
Y_PUBLIC void YFDCTick(YFDCState* fdc);

#endif  // YAX86_FDC_PUBLIC_H


// ==============================================================================
// src/fdc/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/fdc/fdc.c start
// ==============================================================================

#line 1 "./src/fdc/fdc.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

#define Y_FDC_ERROR(...) \
  Y_LOG(fdc->config.logger, &YFDCLogModule, Y_LOG_LEVEL_ERROR, __VA_ARGS__)
#define Y_FDC_WARN(...) \
  Y_LOG(fdc->config.logger, &YFDCLogModule, Y_LOG_LEVEL_WARN, __VA_ARGS__)
#define Y_FDC_INFO(...) \
  Y_LOG(fdc->config.logger, &YFDCLogModule, Y_LOG_LEVEL_INFO, __VA_ARGS__)
#define Y_FDC_DEBUG(...) \
  Y_LOG(fdc->config.logger, &YFDCLogModule, Y_LOG_LEVEL_DEBUG, __VA_ARGS__)

#include <stddef.h>

enum {
  // Invalid offset.
  Y_FDC_INVALID_OFFSET = 0xFFFFFFF,
};

// FDC command opcodes.
typedef enum YFDCCommand {
  // Read a Track
  Y_FDC_CMD_READ_TRACK = 0x02,
  // Specify
  Y_FDC_CMD_SPECIFY = 0x03,
  // Sense Drive Status
  Y_FDC_CMD_SENSE_DRIVE_STATUS = 0x04,
  // Write Data
  Y_FDC_CMD_WRITE_DATA = 0x05,
  // Read Data
  Y_FDC_CMD_READ_DATA = 0x06,
  // Recalibrate
  Y_FDC_CMD_RECALIBRATE = 0x07,
  // Sense Interrupt Status
  Y_FDC_CMD_SENSE_INTERRUPT_STATUS = 0x08,
  // Write Deleted Data
  Y_FDC_CMD_WRITE_DELETED_DATA = 0x09,
  // Read ID
  Y_FDC_CMD_READ_ID = 0x0A,
  // Read Deleted Data
  Y_FDC_CMD_READ_DELETED_DATA = 0x0C,
  // Format a Track
  Y_FDC_CMD_FORMAT_TRACK = 0x0D,
  // Seek
  Y_FDC_CMD_SEEK = 0x0F,
  // Scan Equal
  Y_FDC_CMD_SCAN_EQUAL = 0x11,
  // Scan Low or Equal
  Y_FDC_CMD_SCAN_LOW_OR_EQUAL = 0x19,
  // Scan High or Equal
  Y_FDC_CMD_SCAN_HIGH_OR_EQUAL = 0x1D,
} YFDCCommand;

// Metadata for each FDC command.
typedef struct YFDCCommandMetadata {
  // Base opcode of the command (lower 5 bits).
  uint8_t opcode;
  // Number of parameter bytes expected.
  uint8_t num_param_bytes;
  // Handler to execute the command.
  void (*handler)(YFDCState* fdc);
} YFDCCommandMetadata;

// Helper to raise an IRQ6 if the callback is set and interrupts are enabled in
// DOR.
Y_FILE_PRIVATE inline void YFDCRaiseIRQ6(YFDCState* fdc) {
  if (fdc->config.raise_irq6 && (fdc->dor & Y_FDC_DOR_INTERRUPT_ENABLE)) {
    fdc->config.raise_irq6(fdc->config.context);
  }
}

// Transition into execution phase.
Y_FILE_PRIVATE inline void YFDCStartCommandExecution(YFDCState* fdc) {
  Y_FDC_DEBUG(
      "command_started",
      Y_LOG_HEX8(
          "command", fdc->current_command ? fdc->current_command->opcode : 0),
      Y_LOG_UINT(
          "parameter_bytes",
          YFDCCommandBufferLength(&fdc->command_buffer) - 1));
  fdc->phase = Y_FDC_PHASE_EXECUTION;
  fdc->current_command_ticks = 0;
}

// Transition into result phase after command execution.
Y_FILE_PRIVATE inline void YFDCFinishCommandExecution(YFDCState* fdc) {
  Y_FDC_DEBUG(
      "command_finished",
      Y_LOG_UINT("result_bytes", YFDCResultBufferLength(&fdc->result_buffer)),
      Y_LOG_HEX8(
          "st0", YFDCResultBufferLength(&fdc->result_buffer) > 0
                     ? *YFDCResultBufferGet(&fdc->result_buffer, 0)
                     : 0));
  if (YFDCResultBufferLength(&fdc->result_buffer) == 0) {
    // No result bytes to send, go back to idle phase.
    fdc->phase = Y_FDC_PHASE_IDLE;
  } else {
    // Has result bytes to send, go to result phase.
    fdc->phase = Y_FDC_PHASE_RESULT;
    fdc->next_result_byte_index = 0;
  }
}

// Helper to perform a seek operation (for Seek and Recalibrate).
Y_FILE_PRIVATE void YFDCPerformSeek(
    YFDCState* fdc, uint8_t drive_index, uint8_t target_track) {
  YFDCDriveState* drive = &fdc->drives[drive_index];

  // On initial tick, start seeking.
  if (fdc->current_command_ticks == 0) {
    drive->busy = true;
    return;
  }

  // On 2nd tick, seek is complete.
  drive->track = target_track;
  drive->busy = false;

  // Set Status Register 0 (ST0)
  // Bits 7-6: Interrupt Code = 00 (Normal Termination)
  // Bit 5: Seek End = 1
  // Bits 1-0: Unit Select (Drive Index)
  drive->st0 = Y_FDC_ST0_NORMAL_TERMINATION | Y_FDC_ST0_SEEK_END | drive_index;

  // Set interrupt pending flag for this drive.
  drive->has_pending_interrupt = true;

  // Raise Interrupt (IRQ6).
  YFDCRaiseIRQ6(fdc);

  YFDCFinishCommandExecution(fdc);
}

// Compute the byte offset within a disk image for the given address. Returns
// Y_FDC_INVALID_OFFSET if the address is out of range.
//
// In a raw image file, the data is laid out track by track, starting from the
// outermost track (Track 0). Within each track, it reads all the data from the
// first head (Head 0, the top side) and then all the data from the second head
// (Head 1, the bottom side) before moving to the next track.
// In other words, the layout is an array of
// [num_tracks][num_heads][num_sectors_per_track].
Y_FILE_PRIVATE inline uint32_t YFDCComputeOffset(
    YFDCDiskFormat format, uint8_t head, uint8_t track, uint8_t sector,
    uint16_t sector_offset) {
  if (head >= format.num_heads || track >= format.num_tracks || sector == 0 ||
      sector > format.num_sectors_per_track ||
      sector_offset >= format.sector_size) {
    return Y_FDC_INVALID_OFFSET;
  }

  uint32_t offset = 0;
  // Seek to start of the track
  offset += (uint32_t)track * format.num_heads * format.num_sectors_per_track *
            format.sector_size;
  // Seek to start of the head within the track
  offset += (uint32_t)head * format.num_sectors_per_track * format.sector_size;
  // Seek to start of the sector within the head
  offset += (uint32_t)(sector - 1) * format.sector_size;
  // Add byte offset within the sector
  offset += (uint32_t)sector_offset;

  return offset;
}

// Helper to finish a read/write command.
Y_FILE_PRIVATE void YFDCFinishReadWrite(
    YFDCState* fdc, uint8_t st0, uint8_t st1, uint8_t st2) {
  YFDCResultBufferAppend(&fdc->result_buffer, &st0);
  YFDCResultBufferAppend(&fdc->result_buffer, &st1);
  YFDCResultBufferAppend(&fdc->result_buffer, &st2);
  YFDCResultBufferAppend(&fdc->result_buffer, &fdc->transfer.cylinder);
  YFDCResultBufferAppend(&fdc->result_buffer, &fdc->transfer.head);
  YFDCResultBufferAppend(&fdc->result_buffer, &fdc->transfer.sector);
  YFDCResultBufferAppend(&fdc->result_buffer, &fdc->transfer.sector_size_code);

  YFDCRaiseIRQ6(fdc);
  YFDCFinishCommandExecution(fdc);
}

// Handler for Write Data command.
Y_FILE_PRIVATE void YFDCHandleWriteData(YFDCState* fdc) {
  if (fdc->current_command_ticks == 0) {
    // Initialization.
    uint8_t cmd_byte = *YFDCCommandBufferGet(&fdc->command_buffer, 0);
    fdc->transfer.multi_track = (cmd_byte & 0x80) != 0;

    uint8_t drive_head = *YFDCCommandBufferGet(&fdc->command_buffer, 1);
    uint8_t drive_index = drive_head & 0x03;
    uint8_t head_address = (drive_head >> 2) & 0x01;

    fdc->transfer.cylinder = *YFDCCommandBufferGet(&fdc->command_buffer, 2);
    fdc->transfer.head = *YFDCCommandBufferGet(&fdc->command_buffer, 3);
    fdc->transfer.sector = *YFDCCommandBufferGet(&fdc->command_buffer, 4);
    fdc->transfer.sector_size_code =
        *YFDCCommandBufferGet(&fdc->command_buffer, 5);
    fdc->transfer.eot = *YFDCCommandBufferGet(&fdc->command_buffer, 6);
    // GPL and DTL are ignored.

    YFDCDriveState* drive = &fdc->drives[drive_index];
    if (!drive->present) {
      // Drive not ready.
      YFDCFinishReadWrite(
          fdc,
          Y_FDC_ST0_ABNORMAL_TERMINATION | Y_FDC_ST0_NOT_READY |
              head_address << 2 | drive_index,
          0, 0);
      return;
    }

    // Calculate offset for the first byte.
    fdc->transfer.current_offset = YFDCComputeOffset(
        *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
        fdc->transfer.sector, 0);

    if (fdc->transfer.current_offset == Y_FDC_INVALID_OFFSET) {
      YFDCFinishReadWrite(
          fdc, Y_FDC_ST0_ABNORMAL_TERMINATION | head_address << 2 | drive_index,
          Y_FDC_ST1_NO_DATA, 0);
      return;
    }

    fdc->transfer.sector_byte_index = 0;
    // For Write, we need to request the first byte immediately.
    fdc->transfer.dma_request_active = true;
    fdc->transfer.tc_received = false;
    if (fdc->config.request_dma) {
      fdc->config.request_dma(fdc->config.context);
    }
    return;
  }

  // Execution Loop.

  // Check for Terminal Count (TC).
  if (fdc->transfer.tc_received) {
    // Transfer complete.
    uint8_t drive_index = *YFDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
    uint8_t head_address = (fdc->transfer.head & 0x01);
    YFDCFinishReadWrite(
        fdc, Y_FDC_ST0_NORMAL_TERMINATION | head_address << 2 | drive_index, 0,
        0);
    return;
  }

  // If DREQ is active, wait for the system to service it (write byte to us).
  if (fdc->transfer.dma_request_active) {
    return;
  }

  // Data has arrived in data_register. Write it to image.
  uint8_t drive_index = *YFDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
  if (fdc->config.write_image_byte) {
    fdc->config.write_image_byte(
        fdc->config.context, drive_index, fdc->transfer.current_offset,
        fdc->transfer.data_register);
  }

  // Advance pointers.
  fdc->transfer.current_offset++;
  fdc->transfer.sector_byte_index++;

  // Calculate sector size.
  uint16_t sector_size;
  uint8_t dtl = *YFDCCommandBufferGet(&fdc->command_buffer, 8);
  if (fdc->transfer.sector_size_code == 0) {
    sector_size = dtl;
  } else {
    sector_size = 128 << fdc->transfer.sector_size_code;
  }

  // Check for sector boundary.
  if (fdc->transfer.sector_byte_index >= sector_size) {
    // Sector done.
    if (fdc->transfer.sector >= fdc->transfer.eot) {
      // End of Track reached.
      if (fdc->transfer.multi_track && (fdc->transfer.head & 1) == 0) {
        // Multi-Track rollover.
        fdc->transfer.head ^= 1;
        fdc->transfer.sector = 1;
        fdc->transfer.sector_byte_index = 0;

        // Recompute offset.
        YFDCDriveState* drive = &fdc->drives[drive_index];
        fdc->transfer.current_offset = YFDCComputeOffset(
            *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
            fdc->transfer.sector, 0);

        if (fdc->transfer.current_offset == Y_FDC_INVALID_OFFSET) {
          fdc->transfer.tc_received = true;
          return;  // Stop here, don't request next byte.
        }
      } else {
        // Terminate.
        fdc->transfer.sector++;
        fdc->transfer.tc_received = true;
        return;  // Stop here.
      }
    } else {
      // Next sector.
      fdc->transfer.sector++;
      fdc->transfer.sector_byte_index = 0;
      YFDCDriveState* drive = &fdc->drives[drive_index];
      fdc->transfer.current_offset = YFDCComputeOffset(
          *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
          fdc->transfer.sector, 0);

      if (fdc->transfer.current_offset == Y_FDC_INVALID_OFFSET) {
        fdc->transfer.tc_received = true;
        return;
      }
    }
  }

  // Request next byte via DMA.
  fdc->transfer.dma_request_active = true;
  if (fdc->config.request_dma) {
    fdc->config.request_dma(fdc->config.context);
  }
}

// Handler for Read Data command.
Y_FILE_PRIVATE Y_HOT void YFDCHandleReadData(YFDCState* fdc) {
  if (fdc->current_command_ticks == 0) {
    // Initialization.
    uint8_t cmd_byte = *YFDCCommandBufferGet(&fdc->command_buffer, 0);
    fdc->transfer.multi_track = (cmd_byte & 0x80) != 0;

    uint8_t drive_head = *YFDCCommandBufferGet(&fdc->command_buffer, 1);
    uint8_t drive_index = drive_head & 0x03;
    uint8_t head_address = (drive_head >> 2) & 0x01;

    fdc->transfer.cylinder = *YFDCCommandBufferGet(&fdc->command_buffer, 2);
    fdc->transfer.head = *YFDCCommandBufferGet(&fdc->command_buffer, 3);
    fdc->transfer.sector = *YFDCCommandBufferGet(&fdc->command_buffer, 4);
    fdc->transfer.sector_size_code =
        *YFDCCommandBufferGet(&fdc->command_buffer, 5);
    fdc->transfer.eot = *YFDCCommandBufferGet(&fdc->command_buffer, 6);
    // GPL and DTL are ignored as we don't simulate gap timings or use DTL for
    // init.

    YFDCDriveState* drive = &fdc->drives[drive_index];
    if (!drive->present) {
      // Drive not ready.
      YFDCFinishReadWrite(
          fdc,
          Y_FDC_ST0_ABNORMAL_TERMINATION | Y_FDC_ST0_NOT_READY |
              head_address << 2 | drive_index,
          0, 0);
      return;
    }

    // Calculate offset for the first byte.
    fdc->transfer.current_offset = YFDCComputeOffset(
        *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
        fdc->transfer.sector, 0);

    if (fdc->transfer.current_offset == Y_FDC_INVALID_OFFSET) {
      // Invalid sector/track.
      // If the sector is physically out of bounds for the format, report No
      // Data (Sector Not Found).
      YFDCFinishReadWrite(
          fdc, Y_FDC_ST0_ABNORMAL_TERMINATION | head_address << 2 | drive_index,
          Y_FDC_ST1_NO_DATA, 0);
      return;
    }

    fdc->transfer.sector_byte_index = 0;
    fdc->transfer.dma_request_active = false;
    fdc->transfer.tc_received = false;
    return;
  }

  // Execution Loop.

  // Check for Terminal Count (TC).
  if (fdc->transfer.tc_received) {
    // Transfer complete.
    uint8_t drive_index = *YFDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
    uint8_t head_address = (fdc->transfer.head & 0x01);
    YFDCFinishReadWrite(
        fdc, Y_FDC_ST0_NORMAL_TERMINATION | head_address << 2 | drive_index, 0,
        0);
    return;
  }

  // If DREQ is active, wait for the system to service it.
  if (fdc->transfer.dma_request_active) {
    return;
  }

  // Read next byte.
  uint8_t drive_index = *YFDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
  if (fdc->config.read_image_byte) {
    fdc->transfer.data_register = fdc->config.read_image_byte(
        fdc->config.context, drive_index, fdc->transfer.current_offset);
  } else {
    fdc->transfer.data_register = 0;
  }

  // Advance pointers.
  fdc->transfer.current_offset++;
  fdc->transfer.sector_byte_index++;

  // Request DMA transfer.
  fdc->transfer.dma_request_active = true;
  if (fdc->config.request_dma) {
    fdc->config.request_dma(fdc->config.context);
  }

  // Calculate sector size again for boundary check.
  uint16_t sector_size;
  uint8_t dtl = *YFDCCommandBufferGet(&fdc->command_buffer, 8);
  if (fdc->transfer.sector_size_code == 0) {
    sector_size = dtl;
  } else {
    sector_size = 128 << fdc->transfer.sector_size_code;
  }

  // Check for sector boundary.
  if (fdc->transfer.sector_byte_index >= sector_size) {
    // Sector done.
    if (fdc->transfer.sector >= fdc->transfer.eot) {
      // End of Track reached.
      if (fdc->transfer.multi_track && (fdc->transfer.head & 1) == 0) {
        // Multi-Track rollover: Side 0 -> Side 1.
        fdc->transfer.head ^= 1;   // Flip to Head 1
        fdc->transfer.sector = 1;  // Reset to Sector 1
        fdc->transfer.sector_byte_index = 0;

        // Recompute offset for new head/sector.
        YFDCDriveState* drive = &fdc->drives[drive_index];
        fdc->transfer.current_offset = YFDCComputeOffset(
            *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
            fdc->transfer.sector, 0);

        if (fdc->transfer.current_offset == Y_FDC_INVALID_OFFSET) {
          fdc->transfer.tc_received = true;
        }
      } else {
        // Standard termination (MT=0 or already on Head 1).
        // Increment sector so result phase reports the *next* logical sector.
        fdc->transfer.sector++;
        fdc->transfer.tc_received = true;
      }
    } else {
      // Move to next sector.
      fdc->transfer.sector++;
      fdc->transfer.sector_byte_index = 0;
      // Recompute offset for new sector.
      YFDCDriveState* drive = &fdc->drives[drive_index];
      fdc->transfer.current_offset = YFDCComputeOffset(
          *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
          fdc->transfer.sector, 0);

      if (fdc->transfer.current_offset == Y_FDC_INVALID_OFFSET) {
        // Should not happen if EOT is correct, but if we ran off the end
        // of the image despite EOT, terminate.
        fdc->transfer.tc_received = true;
      }
    }
  }
}

// Handler for Recalibrate command.
Y_FILE_PRIVATE void YFDCHandleRecalibrate(YFDCState* fdc) {
  // Recalibrate command has one parameter byte: drive number (0-3).
  uint8_t drive_index = *YFDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
  YFDCPerformSeek(fdc, drive_index, 0);
}

// Handler for Seek command.
Y_FILE_PRIVATE void YFDCHandleSeek(YFDCState* fdc) {
  // Seek command parameters:
  // Byte 1: Drive number (0-3) and Head address (ignored for seek).
  // Byte 2: New Cylinder Number (NCN).
  uint8_t drive_index = *YFDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
  uint8_t target_track = *YFDCCommandBufferGet(&fdc->command_buffer, 2);
  YFDCPerformSeek(fdc, drive_index, target_track);
}

// Handler for Specify command.
Y_FILE_PRIVATE void YFDCHandleSpecify(YFDCState* fdc) {
  // We don't currently support changing timings or non-DMA mode, so we just
  // ignore the parameters.
  YFDCFinishCommandExecution(fdc);
}

// Handler for Sense Interrupt Status command.
Y_FILE_PRIVATE void YFDCHandleSenseInterruptStatus(YFDCState* fdc) {
  // Check for any pending interrupts.
  for (int i = 0; i < Y_FDC_NUM_DRIVES; ++i) {
    YFDCDriveState* drive = &fdc->drives[i];
    if (drive->has_pending_interrupt) {
      // Clear the pending interrupt flag.
      drive->has_pending_interrupt = false;

      // Result Byte 0: ST0 (Status Register 0)
      YFDCResultBufferAppend(&fdc->result_buffer, &drive->st0);
      // Result Byte 1: PCN (Present Cylinder Number)
      YFDCResultBufferAppend(&fdc->result_buffer, &drive->track);

      YFDCFinishCommandExecution(fdc);
      return;
    }
  }

  // No pending interrupts found. This is treated as an invalid command.
  uint8_t st0 = Y_FDC_ST0_INVALID_COMMAND;
  YFDCResultBufferAppend(&fdc->result_buffer, &st0);
  YFDCFinishCommandExecution(fdc);
}

// List of supported FDC commands.
// The opcodes here represent the base 5-bit command.
Y_FILE_PRIVATE const YFDCCommandMetadata YFDCCommandMetadataTable[] = {
    // Read a Track
    {.opcode = Y_FDC_CMD_READ_TRACK, .num_param_bytes = 8, .handler = NULL},
    // Specify
    {.opcode = Y_FDC_CMD_SPECIFY,
     .num_param_bytes = 2,
     .handler = YFDCHandleSpecify},
    // Sense Drive Status
    {.opcode = Y_FDC_CMD_SENSE_DRIVE_STATUS,
     .num_param_bytes = 1,
     .handler = NULL},
    // Write Data
    {.opcode = Y_FDC_CMD_WRITE_DATA,
     .num_param_bytes = 8,
     .handler = YFDCHandleWriteData},
    // Read Data
    {.opcode = Y_FDC_CMD_READ_DATA,
     .num_param_bytes = 8,
     .handler = YFDCHandleReadData},
    // Recalibrate
    {.opcode = Y_FDC_CMD_RECALIBRATE,
     .num_param_bytes = 1,
     .handler = YFDCHandleRecalibrate},
    // Sense Interrupt Status
    {.opcode = Y_FDC_CMD_SENSE_INTERRUPT_STATUS,
     .num_param_bytes = 0,
     .handler = YFDCHandleSenseInterruptStatus},
    // Write Deleted Data
    {.opcode = Y_FDC_CMD_WRITE_DELETED_DATA,
     .num_param_bytes = 8,
     .handler = NULL},
    // Read ID
    {.opcode = Y_FDC_CMD_READ_ID, .num_param_bytes = 1, .handler = NULL},
    // Read Deleted Data
    {.opcode = Y_FDC_CMD_READ_DELETED_DATA,
     .num_param_bytes = 8,
     .handler = NULL},
    // Format a Track
    {.opcode = Y_FDC_CMD_FORMAT_TRACK, .num_param_bytes = 5, .handler = NULL},
    // Seek
    {.opcode = Y_FDC_CMD_SEEK, .num_param_bytes = 2, .handler = YFDCHandleSeek},
    // Scan Equal
    {.opcode = Y_FDC_CMD_SCAN_EQUAL, .num_param_bytes = 8, .handler = NULL},
    // Scan Low or Equal
    {.opcode = Y_FDC_CMD_SCAN_LOW_OR_EQUAL,
     .num_param_bytes = 8,
     .handler = NULL},
    // Scan High or Equal
    {.opcode = Y_FDC_CMD_SCAN_HIGH_OR_EQUAL,
     .num_param_bytes = 8,
     .handler = NULL},
};

// Nothing to do: an FDC powers on with every field at zero, and the caller has
// already zeroed the state. Kept so that every module is brought up the same
// way, and so that a non-zero default acquired later has somewhere to go.
Y_PUBLIC void YFDCInit(Y_UNUSED YFDCState* fdc) {}

// Looks up command metadata by opcode. Returns NULL if not found. This is a
// linear search, but the command table is small enough that this is fine.
Y_FILE_PRIVATE const YFDCCommandMetadata* YFDCFindCommandMetadata(
    uint8_t opcode) {
  for (size_t i = 0;
       i < sizeof(YFDCCommandMetadataTable) / sizeof(YFDCCommandMetadata);
       ++i) {
    if (YFDCCommandMetadataTable[i].opcode == opcode) {
      return &YFDCCommandMetadataTable[i];
    }
  }
  return NULL;
}

Y_FILE_PRIVATE uint8_t YFDCReadMSRPort(YFDCState* fdc) {
  uint8_t msr = 0;
  switch (fdc->phase) {
    case Y_FDC_PHASE_IDLE:
    case Y_FDC_PHASE_COMMAND:
      // FDC is ready to receive a command or parameter byte.
      msr = Y_FDC_MSR_REQUEST_FOR_MASTER;
      break;
    case Y_FDC_PHASE_RESULT:
      // FDC has result bytes to send and is still busy with the command.
      msr = Y_FDC_MSR_REQUEST_FOR_MASTER | Y_FDC_MSR_DATA_DIRECTION |
            Y_FDC_MSR_BUSY;
      break;
    case Y_FDC_PHASE_EXECUTION:
      // FDC is busy executing a command.
      msr |= Y_FDC_MSR_BUSY;
      break;
  }
  // Set drive busy flags.
  for (int i = 0; i < Y_FDC_NUM_DRIVES; ++i) {
    if (fdc->drives[i].busy) {
      msr |= (1 << i);
    }
  }
  return msr;
}

Y_FILE_PRIVATE uint8_t YFDCReadDataPort(YFDCState* fdc) {
  switch (fdc->phase) {
    case Y_FDC_PHASE_EXECUTION:
      // DMA or Polling read during execution.
      fdc->transfer.dma_request_active = false;
      return fdc->transfer.data_register;

    case Y_FDC_PHASE_RESULT: {
      if (fdc->next_result_byte_index >=
          YFDCResultBufferLength(&fdc->result_buffer)) {
        return 0xFF;  // All result bytes have been read.
      }
      uint8_t value = *YFDCResultBufferGet(
          &fdc->result_buffer, fdc->next_result_byte_index);
      fdc->next_result_byte_index++;
      if (fdc->next_result_byte_index >=
          YFDCResultBufferLength(&fdc->result_buffer)) {
        // Last result byte was read, reset to idle.
        fdc->phase = Y_FDC_PHASE_IDLE;
        fdc->next_result_byte_index = 0;
        YFDCResultBufferClear(&fdc->result_buffer);
      }
      return value;
    }

    default:
      return 0xFF;  // Invalid read.
  }
}

Y_PUBLIC uint8_t YFDCReadPort(YFDCState* fdc, uint16_t port) {
  switch (port) {
    case Y_FDC_PORT_MSR:  // Main Status Register (MSR)
      return YFDCReadMSRPort(fdc);
    case Y_FDC_PORT_DATA:  // Data Register
      return YFDCReadDataPort(fdc);
    default:
      // Per convention for reads from unused/invalid ports.
      return 0xFF;
  }
}

Y_FILE_PRIVATE void YFDCWriteDORPort(YFDCState* fdc, uint8_t value) {
  uint8_t old_dor = fdc->dor;
  fdc->dor = value;

  bool old_reset_bit = (old_dor & Y_FDC_DOR_RESET) != 0;
  bool new_reset_bit = (value & Y_FDC_DOR_RESET) != 0;

  if (!new_reset_bit && old_reset_bit) {
    // Entering reset state (1 -> 0).
    fdc->phase = Y_FDC_PHASE_IDLE;
    YFDCCommandBufferClear(&fdc->command_buffer);
    YFDCResultBufferClear(&fdc->result_buffer);
    for (int i = 0; i < Y_FDC_NUM_DRIVES; ++i) {
      fdc->drives[i].busy = false;
      fdc->drives[i].has_pending_interrupt = false;
    }
  } else if (new_reset_bit && !old_reset_bit) {
    // Exiting reset state (0 -> 1).
    // The FDC generates an interrupt and sets up status for Sense
    // Interrupt Status for all drives.
    for (int i = 0; i < Y_FDC_NUM_DRIVES; ++i) {
      fdc->drives[i].has_pending_interrupt = true;
      fdc->drives[i].st0 = Y_FDC_ST0_ABNORMAL_TERMINATION_POLLING | (uint8_t)i;
    }
    YFDCRaiseIRQ6(fdc);
  }
}

Y_FILE_PRIVATE void YFDCWriteDataPort(YFDCState* fdc, uint8_t value) {
  switch (fdc->phase) {
    case Y_FDC_PHASE_IDLE: {
      // This is the first byte of a new command.
      // Extract the opcode (lower 5 bits).
      uint8_t opcode = value & 0x1F;
      fdc->current_command = YFDCFindCommandMetadata(opcode);

      if (!fdc->current_command) {
        Y_FDC_WARN("invalid_command", Y_LOG_HEX8("command", opcode));
        // Invalid command. Set up the result phase with an error.
        YFDCResultBufferClear(&fdc->result_buffer);
        uint8_t status = Y_FDC_ST0_INVALID_COMMAND;
        YFDCResultBufferAppend(&fdc->result_buffer, &status);
        YFDCFinishCommandExecution(fdc);
        return;
      }

      // Clear previous command and store the first byte.
      YFDCCommandBufferClear(&fdc->command_buffer);
      YFDCCommandBufferAppend(&fdc->command_buffer, &value);

      if (fdc->current_command->num_param_bytes == 0) {
        // Command has no parameters, move directly to execution.
        YFDCStartCommandExecution(fdc);
      } else {
        // Wait for parameter bytes.
        fdc->phase = Y_FDC_PHASE_COMMAND;
      }
      break;
    }

    case Y_FDC_PHASE_COMMAND: {
      // This is a parameter byte for the current command.
      if (!fdc->current_command) {
        // Should not happen, but as a safeguard, reset to idle.
        fdc->phase = Y_FDC_PHASE_IDLE;
        return;
      }

      // Store the parameter byte.
      YFDCCommandBufferAppend(&fdc->command_buffer, &value);

      // Check if all parameters have been received.
      // Total bytes = 1 (command) + num_param_bytes.
      if (YFDCCommandBufferLength(&fdc->command_buffer) >=
          (uint8_t)(fdc->current_command->num_param_bytes + 1)) {
        // All bytes received, move to execution phase.
        YFDCStartCommandExecution(fdc);
      }
      break;
    }

    case Y_FDC_PHASE_EXECUTION:
      // DMA or Polling write during execution.
      fdc->transfer.data_register = value;
      fdc->transfer.dma_request_active = false;
      break;

    case Y_FDC_PHASE_RESULT:
      // The FDC is busy. Ignore any writes to the data port.
      break;
  }
}

Y_PUBLIC void YFDCWritePort(YFDCState* fdc, uint16_t port, uint8_t value) {
  switch (port) {
    case Y_FDC_PORT_DOR:  // Digital Output Register
      YFDCWriteDORPort(fdc, value);
      break;

    case Y_FDC_PORT_DATA:  // Data Register
      // Logic will be based on the current FDC phase.
      YFDCWriteDataPort(fdc, value);
      break;

    default:
      // Ignore writes to other ports.
      break;
  }
}

Y_PUBLIC void YFDCHandleTC(YFDCState* fdc) { fdc->transfer.tc_received = true; }

Y_PUBLIC void YFDCInsertDisk(
    YFDCState* fdc, uint8_t drive, const YFDCDiskFormat* format) {
  if (drive >= Y_FDC_NUM_DRIVES) {
    return;
  }
  YFDCDriveState* drive_state = &fdc->drives[drive];
  drive_state->present = true;
  drive_state->format = format;
  drive_state->head = 0;
  drive_state->track = 0;
}

Y_PUBLIC void YFDCEjectDisk(YFDCState* fdc, uint8_t drive) {
  if (drive >= Y_FDC_NUM_DRIVES) {
    return;
  }
  YFDCDriveState* drive_state = &fdc->drives[drive];
  drive_state->present = false;
  drive_state->format = NULL;
}

Y_PUBLIC void YFDCTick(YFDCState* fdc) {
  if (fdc->phase != Y_FDC_PHASE_EXECUTION) {
    return;
  }

  // Run the command handler if defined.
  if (fdc->current_command && fdc->current_command->handler) {
    fdc->current_command->handler(fdc);
    ++fdc->current_command_ticks;
  } else {
    // No handler defined, finish execution immediately.
    YFDCResultBufferClear(&fdc->result_buffer);
    YFDCFinishCommandExecution(fdc);
  }
}


// ==============================================================================
// src/fdc/fdc.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_FDC_BUNDLE_H

// ==============================================================================
// YAX86 HDC MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_HDC_BUNDLE_H
#define YAX86_HDC_BUNDLE_H

#ifndef YAX86_UTIL_BUNDLE_H
#include "util.h"
#endif  // YAX86_UTIL_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/hdc/public.h start
// ==============================================================================

#line 1 "./src/hdc/public.h"
// Public interface for the Hard Disk Controller (HDC) module.
#ifndef YAX86_HDC_PUBLIC_H
#define YAX86_HDC_PUBLIC_H

// This module emulates an XT-IDE rev 2 hard disk controller - an 8-bit ATA
// task file operated in PIO mode, with no DMA and no interrupt line.
//
// The controller card also carries an option ROM, which is where the guest's
// INT 13h support for drives 80h and up actually comes from: GLaBIOS itself
// implements INT 13h for floppies only and rejects drive numbers above 3. Its
// POST scans C800:0000 and up for option ROMs and calls into whatever it
// finds, so the ROM this module maps is what makes a hard disk visible to DOS.
//
// The ROM is the XTIDE Universal BIOS, compiled into the library the same way
// the system BIOS is. Reading it from a file at run time would not work on
// targets like the Raspberry Pi Pico, which have no file system.

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_HDC_BUNDLE_H
#include "../util/log.h"
#endif  // YAX86_HDC_BUNDLE_H

enum {
  // Log module ID for the HDC.
  Y_HDC_LOG_MODULE_ID = 9,
};

// Log module for the HDC.
Y_PUBLIC_HEADER const YLogModule YHDCLogModule = {
    .id = Y_HDC_LOG_MODULE_ID,
    .name = "hdc",
};

enum {
  // Start address of the option ROM. GLaBIOS scans for option ROMs starting
  // here, on 2KB boundaries.
  Y_HDC_OPTION_ROM_START_ADDRESS = 0xC8000,
};

// Value returned for reads that no device answers.
enum {
  Y_HDC_OPEN_BUS_VALUE = 0xFF,
};

// I/O ports for the HDC.
enum {
  // Base I/O port of the task file. This is the XT-IDE's stock configuration,
  // which the option ROM reports at POST as "Master at 300h".
  Y_HDC_PORT_BASE = 0x300,
  // Number of I/O ports the card decodes.
  Y_HDC_NUM_PORTS = 16,
};

// Task file registers, given as offsets from Y_HDC_PORT_BASE after the card's
// address line swap has been undone. See YHDCPortOffsetToRegister().
typedef enum YHDCRegister {
  // Data register, low byte of each 16-bit word.
  Y_HDC_REGISTER_DATA = 0x0,
  // Error on read, features on write.
  Y_HDC_REGISTER_ERROR = 0x1,
  Y_HDC_REGISTER_SECTOR_COUNT = 0x2,
  // Sector number, or bits 7-0 of an LBA.
  Y_HDC_REGISTER_SECTOR_NUMBER = 0x3,
  // Cylinder low, or bits 15-8 of an LBA.
  Y_HDC_REGISTER_CYLINDER_LOW = 0x4,
  // Cylinder high, or bits 23-16 of an LBA.
  Y_HDC_REGISTER_CYLINDER_HIGH = 0x5,
  // Drive select and head, or bits 27-24 of an LBA.
  Y_HDC_REGISTER_DRIVE_HEAD = 0x6,
  // Status on read, command on write.
  Y_HDC_REGISTER_STATUS = 0x7,
  // Data register, high byte of each 16-bit word. Not an ATA register: the
  // drive's data port is 16 bits wide and the card is on an 8-bit bus, so the
  // card latches the high byte here.
  Y_HDC_REGISTER_DATA_HIGH = 0x8,
  // Alternate status on read, device control on write. The alternate status
  // register reads exactly like the status register; a guest polls it when it
  // wants the status without the side effects a status read has on a drive
  // that raises interrupts.
  Y_HDC_REGISTER_DEVICE_CONTROL = 0xE,
} YHDCRegister;

// Bits of the status register.
enum {
  // An error occurred; the error register says which.
  Y_HDC_STATUS_ERROR = 1 << 0,
  Y_HDC_STATUS_INDEX = 1 << 1,
  Y_HDC_STATUS_CORRECTED_DATA = 1 << 2,
  // The drive is ready to transfer a byte through the data register.
  Y_HDC_STATUS_DATA_REQUEST = 1 << 3,
  Y_HDC_STATUS_SEEK_COMPLETE = 1 << 4,
  Y_HDC_STATUS_DEVICE_FAULT = 1 << 5,
  // The drive is spun up and ready to accept a command.
  Y_HDC_STATUS_READY = 1 << 6,
  // The drive owns the task file and the other bits are not valid.
  Y_HDC_STATUS_BUSY = 1 << 7,
};

// Bits of the error register.
enum {
  Y_HDC_ERROR_ADDRESS_MARK_NOT_FOUND = 1 << 0,
  Y_HDC_ERROR_TRACK0_NOT_FOUND = 1 << 1,
  // The command was not recognized, or is not valid right now.
  Y_HDC_ERROR_ABORTED = 1 << 2,
  // The requested sector does not exist on this drive.
  Y_HDC_ERROR_ID_NOT_FOUND = 1 << 4,
  Y_HDC_ERROR_UNCORRECTABLE = 1 << 6,
  Y_HDC_ERROR_BAD_BLOCK = 1 << 7,
};

// Bits of the device control register.
enum {
  // Stops the drive raising interrupts. This controller has no interrupt line
  // in the first place, so it is accepted and ignored.
  Y_HDC_DEVICE_CONTROL_NO_INTERRUPT = 1 << 1,
  // Software reset, which the guest asserts and then releases.
  Y_HDC_DEVICE_CONTROL_SOFTWARE_RESET = 1 << 2,
};

// Bits of the drive/head register.
enum {
  // Head number, or bits 27-24 of an LBA.
  Y_HDC_DRIVE_HEAD_HEAD_MASK = 0x0F,
  // Selects drive 1 when set, drive 0 when clear.
  Y_HDC_DRIVE_HEAD_DRIVE_SELECT = 1 << 4,
  // The address registers hold an LBA rather than a cylinder/head/sector.
  Y_HDC_DRIVE_HEAD_LBA = 1 << 6,
};

// ATA commands.
typedef enum YHDCCommand {
  // Seek to cylinder 0. The low nibble is a step rate that has no meaning
  // here, so 0x10 through 0x1F are all this command.
  Y_HDC_COMMAND_RECALIBRATE = 0x10,
  Y_HDC_COMMAND_READ_SECTORS = 0x20,
  Y_HDC_COMMAND_READ_SECTORS_NO_RETRY = 0x21,
  Y_HDC_COMMAND_WRITE_SECTORS = 0x30,
  Y_HDC_COMMAND_WRITE_SECTORS_NO_RETRY = 0x31,
  Y_HDC_COMMAND_READ_VERIFY_SECTORS = 0x40,
  Y_HDC_COMMAND_READ_VERIFY_SECTORS_NO_RETRY = 0x41,
  // Seek to the addressed cylinder. As with recalibrate, the low nibble is a
  // step rate, so 0x70 through 0x7F are all this command.
  Y_HDC_COMMAND_SEEK = 0x70,
  // Sets the head and sector counts used to translate a cylinder/head/sector
  // address into a sector number.
  Y_HDC_COMMAND_INITIALIZE_DEVICE_PARAMETERS = 0x91,
  // Returns a 512-byte block describing the drive.
  Y_HDC_COMMAND_IDENTIFY_DEVICE = 0xEC,
} YHDCCommand;

enum {
  // Number of drives the controller supports: a master and a slave.
  Y_HDC_NUM_DRIVES = 2,
  // Bytes per sector. ATA drives of this era have no other sector size, and
  // the option ROM assumes this one.
  Y_HDC_SECTOR_SIZE = 512,
};

// Physical geometry of an attached drive.
typedef struct YHDCDriveGeometry {
  uint16_t num_cylinders;
  uint8_t num_heads;
  uint8_t num_sectors_per_track;
} YHDCDriveGeometry;

// Geometry of the 10MB fixed disk the IBM PC/XT shipped with. Era-accurate,
// comfortably under the 32MB per-partition ceiling of MS-DOS 3.3, and small
// enough for a host to keep an image of one in memory.
enum {
  Y_HDC_GEOMETRY_10MB_NUM_CYLINDERS = 306,
  Y_HDC_GEOMETRY_10MB_NUM_HEADS = 4,
  Y_HDC_GEOMETRY_10MB_NUM_SECTORS_PER_TRACK = 17,
  // Size in bytes of an image of such a disk, so that a host can size a buffer
  // for one at compile time.
  Y_HDC_GEOMETRY_10MB_IMAGE_SIZE = Y_HDC_GEOMETRY_10MB_NUM_CYLINDERS *
      Y_HDC_GEOMETRY_10MB_NUM_HEADS *
      Y_HDC_GEOMETRY_10MB_NUM_SECTORS_PER_TRACK * Y_HDC_SECTOR_SIZE,
};
Y_PUBLIC_HEADER const YHDCDriveGeometry YHDCGeometry10MB = {
    .num_cylinders = Y_HDC_GEOMETRY_10MB_NUM_CYLINDERS,
    .num_heads = Y_HDC_GEOMETRY_10MB_NUM_HEADS,
    .num_sectors_per_track = Y_HDC_GEOMETRY_10MB_NUM_SECTORS_PER_TRACK,
};

// What the data register is currently moving.
typedef enum YHDCTransfer {
  // Nothing; a read returns zero and a write is discarded.
  Y_HDC_TRANSFER_NONE = 0,
  // The Identify Device block, out of the sector buffer.
  Y_HDC_TRANSFER_IDENTIFY,
  // Sectors from the drive's image to the guest.
  Y_HDC_TRANSFER_READ,
  // Sectors from the guest to the drive's image.
  Y_HDC_TRANSFER_WRITE,
} YHDCTransfer;

// State of a single drive.
typedef struct YHDCDriveState {
  // Whether a drive is attached.
  bool present;

  // Physical geometry of the drive.
  YHDCDriveGeometry geometry;

  // Head and sector counts the guest asked for with Initialize Device
  // Parameters, which is what cylinder/head/sector addresses are translated
  // against. Initialized to the physical geometry, since a guest that never
  // issues the command expects the geometry the drive reported.
  uint8_t num_translated_heads;
  uint8_t num_translated_sectors_per_track;
} YHDCDriveState;

// Caller-provided runtime configuration for the HDC.
typedef struct YHDCConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger, or NULL to disable logging.
  YLogger* logger;

  // Callbacks to read and write a byte of a drive's image, where offset is a
  // byte offset from the start of the image. The controller range checks every
  // address against the drive's geometry before starting a transfer, so these
  // are only ever called with an offset that lies within the drive.
  //
  // A drive with no callbacks reads back as zeroes and discards writes.
  uint8_t (*read_image_byte)(void* context, uint8_t drive, uint32_t offset);
  void (*write_image_byte)(
      void* context, uint8_t drive, uint32_t offset, uint8_t value);
} YHDCConfig;

// State of the HDC.
typedef struct YHDCState {
  // Pointer to caller-provided runtime configuration.
  YHDCConfig config;

  // Attached drives.
  YHDCDriveState drives[Y_HDC_NUM_DRIVES];

  // Task file registers.
  uint8_t error;
  uint8_t features;
  uint8_t sector_count;
  uint8_t sector_number;
  uint8_t cylinder_low;
  uint8_t cylinder_high;
  uint8_t drive_head;
  uint8_t status;

  // Buffer holding the Identify Device block. Sector data is streamed straight
  // through the image callbacks rather than staged here.
  uint8_t sector_buffer[Y_HDC_SECTOR_SIZE];

  // What the data register is currently moving, if anything.
  YHDCTransfer transfer;
  // Index of the next byte within the sector being transferred. Only
  // meaningful while Y_HDC_STATUS_DATA_REQUEST is set.
  uint16_t transfer_byte_index;
  // Byte offset into the image of the sector being transferred.
  uint32_t transfer_offset;
  // Drive the transfer is against, which is latched when the command starts so
  // that it cannot change underneath a transfer in progress.
  uint8_t transfer_drive;
  // Sectors still to transfer, including the one in progress.
  uint16_t transfer_sectors_remaining;
  // The card's latch for the high byte of a word, which serves both
  // directions: a read of the low byte port fills it, and a write of the high
  // byte port loads it for the following write of the low byte to commit. See
  // YHDCReadDataRegister() and YHDCWriteDataRegister().
  uint8_t data_high_latch;
} YHDCState;

// Initializes the HDC to its power-on state.
Y_PUBLIC void YHDCInit(YHDCState* hdc);

// Returns the size of the option ROM in bytes.
Y_PUBLIC uint32_t YHDCGetOptionROMSize(void);

// Returns a pointer to the option ROM image, YHDCGetOptionROMSize() bytes of
// it. The image is a constant array compiled into the library, so the platform
// maps it directly rather than reading it a byte at a time through a callback.
Y_PUBLIC const uint8_t* YHDCGetOptionROMData(void);

// Handles reads from the HDC's I/O ports.
Y_PUBLIC uint8_t YHDCReadPort(YHDCState* hdc, uint16_t port);

// Handles writes to the HDC's I/O ports.
Y_PUBLIC void YHDCWritePort(YHDCState* hdc, uint16_t port, uint8_t value);

// Attaches a drive with the given geometry. Drive 0 is the master and drive 1
// is the slave.
Y_PUBLIC void YHDCAttachDrive(
    YHDCState* hdc, uint8_t drive, const YHDCDriveGeometry* geometry);

// Detaches the drive in the given slot.
Y_PUBLIC void YHDCDetachDrive(YHDCState* hdc, uint8_t drive);

// Maps an I/O port offset from Y_HDC_PORT_BASE to the task file register it
// reaches. An XT-IDE rev 2 card crosses address lines A0 and A3, so a register
// is reached at the port offset with bits 0 and 3 swapped - the status
// register, ATA register 7, is read at port offset 0xE. The mapping is its own
// inverse.
Y_PUBLIC_HEADER inline uint8_t YHDCPortOffsetToRegister(uint8_t offset) {
  return (uint8_t)((offset & ~0x09) | ((offset & 0x01) << 3) |
                   ((offset >> 3) & 0x01));
}

#endif  // YAX86_HDC_PUBLIC_H


// ==============================================================================
// src/hdc/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/hdc/hdc_option_rom_data.h start
// ==============================================================================

#line 1 "./src/hdc/hdc_option_rom_data.h"
// ROM data automatically generated by generate-rom-data-files.js
// Source: XTIDE_Universal_BIOS_XT_r631.rom

#ifndef YAX86_HDC_OPTION_ROM_DATA_H
#define YAX86_HDC_OPTION_ROM_DATA_H

#include <stdint.h>

enum {
  Y_HDC_OPTION_ROM_DATA_SIZE = 12288,
};

// In unbundled builds, external callers within the module need an extern
// declaration. In bundled builds, the array is static and defined earlier in
// the same translation unit (via bundle.json), so an extern declaration
// would conflict and is omitted.
#ifndef YAX86_HDC_BUNDLE_H
extern const uint8_t YHDCOptionROMData[Y_HDC_OPTION_ROM_DATA_SIZE];
#endif // YAX86_HDC_BUNDLE_H

#endif // YAX86_HDC_OPTION_ROM_DATA_H


// ==============================================================================
// src/hdc/hdc_option_rom_data.h end
// ==============================================================================

// ==============================================================================
// src/hdc/hdc_option_rom_data.c start
// ==============================================================================

#line 1 "./src/hdc/hdc_option_rom_data.c"
// ROM data automatically generated by generate-rom-data-files.js
// Source: XTIDE_Universal_BIOS_XT_r631.rom

#include <stdint.h>

#ifndef YAX86_HDC_BUNDLE_H
#include "../util/common.h"
#endif // YAX86_HDC_BUNDLE_H

Y_MODULE_PRIVATE const uint8_t YHDCOptionROMData[] = {
0x55,0xaa,0x18,0xe9,0xe4,0x0a,0x58,0x55,0x42,0x32,0x31,0x30,0x2d,0x3d,0x58,0x54,
0x49,0x44,0x45,0x20,0x55,0x6e,0x69,0x76,0x65,0x72,0x73,0x61,0x6c,0x20,0x42,0x49,
0x4f,0x53,0x20,0x28,0x58,0x54,0x29,0x3d,0x2d,0x00,0x00,0x00,0x72,0x36,0x33,0x31,
0x20,0x28,0x32,0x30,0x32,0x35,0x2d,0x30,0x32,0x2d,0x30,0x31,0x29,0x00,0x97,0x05,
0xe0,0xee,0x30,0x00,0x04,0x00,0x22,0x02,0x01,0x00,0x00,0x01,0x00,0x00,0x00,0x03,
0x08,0x03,0x08,0x00,0x00,0x00,0x00,0x04,0x10,0x3f,0x0c,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x1d,0x00,0x00,0x00,0x00,0x00,0x1d,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1d,0x00,0x00,0x00,0x00,0x00,0x1d,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1d,0x00,0x00,0x00,0x00,0x00,
0x1d,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x53,0x67,0x79,0x7a,0x6b,0xb8,0x59,0x72,
0x67,0x7c,0x6b,0x00,0x3d,0x20,0x67,0xfa,0x3d,0x40,0x00,0x49,0x55,0x53,0x3c,0x1d,
0x20,0x4a,0x6b,0x7a,0x6b,0x69,0xba,0x2a,0x35,0x35,0x33,0x13,0x2a,0x35,0x29,0x35,
0x91,0x17,0x28,0x00,0x39,0x3d,0x20,0xc6,0x37,0x39,0x3d,0x39,0x58,0x6b,0x72,0x6b,
0x67,0x79,0x6b,0xea,0x7b,0x74,0x6a,0x6b,0xf8,0x4d,0x54,0xdb,0x4d,0x56,0xd2,0x7c,
0x2c,0x39,0x19,0x3d,0x3c,0x39,0x1d,0x3e,0x19,0x48,0x75,0x75,0x7a,0x6f,0x74,0xed,
0x3c,0x24,0x3c,0x19,0x48,0x75,0x75,0xfa,0x79,0x6b,0x69,0x7a,0x75,0xf8,0x74,0x75,
0xfa,0x6c,0x75,0x7b,0x74,0x6a,0x19,0x4b,0x78,0x78,0x75,0xf8,0x37,0x25,0x19,0x4c,
0x4a,0xca,0x61,0x3c,0xa3,0x4e,0x4a,0xca,0x61,0x3c,0xa3,0x48,0x75,0x75,0x7a,0x53,
0x74,0x7b,0x1c,0x3b,0x3c,0x3c,0x3b,0x3d,0x3b,0x00,0x49,0x75,0x73,0x4a,0x7a,0x69,
0x7a,0x1c,0x20,0x28,0x20,0x20,0x58,0x75,0x73,0x3c,0x48,0x75,0x75,0xba,0x5b,0x74,
0x71,0x74,0x75,0x7d,0xb4,0x2e,0x21,0x26,0x20,0x75,0xf8,0x2d,0x22,0x26,0x20,0x4a,
0x8a,0x3d,0x26,0x27,0x20,0x35,0x20,0x71,0x6f,0x88,0x2d,0x02,0x2e,0x01,0x54,0x55,
0x58,0x53,0x47,0x92,0x52,0x47,0x58,0x4d,0x4b,0x00,0x52,0x48,0xc7,0x20,0x00,0x20,
0x2b,0x0f,0x20,0x2d,0x0c,0x20,0x20,0x10,0x4a,0x30,0x00,0x5e,0x30,0x00,0x5e,0x30,
0x95,0x5a,0x30,0x00,0x5a,0x30,0x88,0x2b,0x2f,0x88,0x30,0x53,0x87,0x53,0x30,0x00,
0x53,0x30,0x00,0x59,0x4b,0x98,0x31,0x32,0x3b,0x59,0x6b,0x72,0x6b,0x69,0x7a,0x6f,
0x75,0xf4,0x6f,0xf4,0x3a,0x20,0xb9,0x20,0x37,0x20,0x1d,0x20,0x37,0x20,0x1e,0x4c,
0x72,0x75,0x76,0x76,0xff,0x4a,0x78,0x6f,0x7c,0xeb,0x1c,0x4c,0x75,0x78,0x6b,0x6f,
0x6d,0xf4,0x4e,0x67,0x78,0xea,0x4a,0x6f,0x79,0xb1,0x49,0x67,0x76,0x67,0x69,0x6f,
0x7a,0xff,0xc0,0x1d,0x36,0x29,0x35,0x20,0x3c,0x6f,0x88,0x3d,0x39,0x47,0x6a,0x6a,
0x78,0x29,0x20,0x23,0x48,0x72,0x75,0x69,0x71,0x23,0x48,0x7b,0x79,0x23,0x4f,0x58,
0x57,0x23,0x58,0x6b,0x79,0x6b,0x7a,0x39,0x3d,0x23,0x36,0x23,0x3d,0x23,0x20,0x34,
0x23,0x18,0x81,0x3e,0x63,0x04,0xd4,0x03,0x75,0x08,0x80,0x3e,0x84,0x04,0x18,0x73,
0x01,0xc3,0xf8,0xc3,0xe8,0x47,0x00,0x74,0x0f,0x88,0xc4,0xfa,0xec,0xd0,0xe8,0x72,
0xfb,0xec,0xd0,0xe8,0x73,0xfb,0x88,0xe0,0xaa,0xfb,0xc3,0x53,0xe8,0x2f,0x00,0x74,
0x0d,0x93,0xfa,0xec,0xd0,0xe8,0x72,0xfb,0xec,0xd0,0xe8,0x73,0xfb,0x93,0xab,0xfb,
0x5b,0xc3,0xe8,0x19,0x00,0x74,0x11,0xfa,0xec,0xd0,0xe8,0x72,0xfb,0xec,0xd0,0xe8,
0x73,0xfb,0x26,0xa4,0xfb,0xe2,0xf0,0xc3,0xfa,0xf3,0x26,0xa4,0xfb,0xc3,0xf6,0x06,
0x5f,0x04,0x02,0x74,0x03,0xba,0xda,0x03,0xc3,0x06,0x1e,0x52,0xfc,0x31,0xd2,0x8e,
0xda,0x89,0xfa,0xc4,0x3e,0x54,0x04,0xff,0xd2,0x89,0x3e,0x54,0x04,0x5a,0x1f,0x07,
0xc3,0x58,0xe8,0x69,0x01,0x89,0x3e,0x54,0x04,0x5a,0x1f,0x07,0x58,0x89,0xec,0xff,
0xe0,0x3c,0x20,0x72,0x06,0xeb,0x94,0x3c,0x20,0x73,0x1b,0x50,0xe8,0x26,0x01,0x58,
0x53,0xb4,0x0e,0x8a,0x3e,0x62,0x04,0xcd,0x10,0x5b,0xe8,0xf4,0x00,0xe9,0xcc,0x00,
0x86,0xc4,0x47,0xe9,0x5e,0xff,0xe8,0x5b,0xff,0x47,0xc3,0xe9,0x6d,0xff,0x83,0x3e,
0x5a,0x04,0x00,0x74,0x05,0xaa,0xff,0x0e,0x5a,0x04,0xc3,0xc7,0x06,0x58,0x04,0xa1,
0x02,0xc6,0x06,0x5e,0x04,0x07,0xe8,0x91,0x00,0xa3,0x5c,0x04,0xb8,0x00,0xb8,0x80,
0x3e,0x49,0x04,0x07,0x75,0x02,0xb4,0xb0,0xa3,0x56,0x04,0xb2,0x01,0xe8,0x12,0xff,
0x73,0x02,0xb2,0x03,0x88,0x16,0x5f,0x04,0xe8,0xa6,0x00,0xe8,0x7e,0x00,0xe8,0xd9,
0x00,0xe8,0xab,0x00,0xe9,0xbe,0x00,0x8c,0xdf,0x31,0xc0,0x8e,0xd8,0x58,0xff,0x36,
0x54,0x04,0xff,0x36,0x56,0x04,0xff,0x36,0x58,0x04,0xff,0x36,0x5a,0x04,0xff,0x36,
0x5c,0x04,0xff,0x36,0x5e,0x04,0x8e,0xdf,0xff,0xe0,0x8c,0xdf,0x31,0xc0,0x8e,0xd8,
0x58,0x8f,0x06,0x5e,0x04,0x8f,0x06,0x5c,0x04,0x8f,0x06,0x5a,0x04,0x8f,0x06,0x58,
0x04,0x8f,0x06,0x56,0x04,0x8f,0x06,0x54,0x04,0x50,0x52,0xe8,0xb0,0xff,0x5a,0x8e,
0xdf,0xc3,0x80,0xe3,0x01,0x80,0x26,0x5f,0x04,0xfe,0x08,0x1e,0x5f,0x04,0xa3,0x58,
0x04,0xc3,0xa2,0x5e,0x04,0xc3,0xa3,0x5a,0x04,0xc3,0xb8,0x07,0x06,0x80,0x3e,0x49,
0x04,0x07,0x75,0x03,0xb8,0x0c,0x0b,0xc3,0xa3,0x5c,0x04,0xc3,0x92,0xa1,0x4a,0x04,
0xf6,0xe6,0x30,0xf6,0x01,0xd0,0xd1,0xe0,0x03,0x06,0x4e,0x04,0xa3,0x54,0x04,0x97,
0xc3,0xa1,0x54,0x04,0x2b,0x06,0x4e,0x04,0xd1,0xe8,0xf6,0x36,0x4a,0x04,0x86,0xc4,
0xc3,0x51,0x53,0xb4,0x03,0x8a,0x3e,0x62,0x04,0xcd,0x10,0x92,0x5b,0x59,0xc3,0x8b,
0x16,0x5c,0x04,0x3b,0x16,0x60,0x04,0x74,0x0b,0x51,0x89,0xd1,0xa0,0x49,0x04,0xb4,
0x01,0xcd,0x10,0x59,0xc3,0xe8,0xc9,0xff,0x53,0x92,0xb4,0x02,0x8a,0x3e,0x62,0x04,
0xcd,0x10,0x5b,0xc3,0xa0,0x4a,0x04,0xb4,0x19,0xc3,0x31,0xd2,0xa1,0x54,0x04,0xf7,
0x36,0x4c,0x04,0x3a,0x06,0x62,0x04,0x74,0x04,0xb4,0x05,0xcd,0x10,0xc3,0x55,0x56,
0x51,0x53,0xff,0x36,0x5e,0x04,0x4d,0x4d,0xe8,0x9a,0x00,0x58,0xa2,0x5e,0x04,0x5b,
0x59,0x5e,0x5d,0xc3,0x57,0x51,0x91,0x31,0xc0,0xe8,0x70,0xff,0xe8,0xc5,0xff,0xf6,
0xe4,0x91,0xf3,0xab,0x59,0x5f,0x89,0x3e,0x54,0x04,0xc3,0xe3,0x09,0x51,0x50,0xe8,
0x0e,0x00,0x58,0xe2,0xf9,0x59,0xc3,0x45,0x45,0xb0,0x0a,0xe8,0x02,0x00,0xb0,0x0d,
0x84,0xc0,0x74,0x1c,0x8a,0x26,0x5e,0x04,0xff,0x26,0x58,0x04,0x56,0x51,0x31,0xc9,
0x8e,0xdb,0xac,0x8e,0xd9,0x84,0xc0,0x74,0x05,0xe8,0xe4,0xff,0xeb,0xf2,0x59,0x5e,
0xc3,0xa2,0x5e,0x04,0xc3,0x31,0xdb,0x96,0xeb,0xe2,0xbe,0x10,0x00,0xbb,0x68,0x04,
0x3d,0xb7,0x02,0x53,0x88,0xfb,0x31,0xd2,0xf7,0xf6,0x52,0xfe,0xcf,0x75,0xf7,0x58,
0xfe,0xcb,0x74,0x0d,0x78,0xba,0x08,0xc7,0x75,0x07,0xf6,0xc5,0x02,0x75,0xf0,0xb0,
0x89,0x3c,0x0a,0x1c,0x69,0x2f,0xe8,0xa7,0xff,0xeb,0xe4,0x85,0xc0,0x75,0xd2,0xbe,
0xd2,0x00,0xbb,0x89,0xc6,0x83,0xfe,0x7f,0x8c,0xcb,0x72,0xa0,0x2e,0xac,0x88,0xc5,
0xa8,0xc0,0x74,0x14,0x24,0x3f,0x04,0x3a,0xe8,0x85,0xff,0xd0,0xe5,0x79,0xa5,0x73,
0xeb,0xb0,0x20,0xe8,0x7a,0xff,0xeb,0xe4,0xd0,0xe5,0x83,0xe0,0x1f,0xbb,0xcb,0x0a,
0x01,0xc3,0x3c,0x14,0x2e,0x8a,0x07,0x72,0xdf,0xba,0xa3,0x04,0x29,0xc2,0x8b,0x46,
0x00,0x4d,0x4d,0xbb,0x00,0x05,0x56,0x51,0xbe,0x0a,0x00,0xff,0xd2,0x59,0x5e,0xeb,
0xca,0xb4,0x01,0xcd,0x16,0xc3,0xe8,0xf8,0xff,0x74,0x06,0x30,0xe4,0xcd,0x16,0x85,
0xc0,0xc3,0xe8,0x13,0x00,0x04,0x04,0x92,0xe8,0xbc,0x02,0xd1,0xe0,0x01,0xd0,0x92,
0x88,0xd4,0xbf,0x76,0x03,0xe9,0x61,0xfd,0xe8,0x7a,0x02,0x30,0xe4,0xd1,0xe0,0xc3,
0x50,0x8a,0x16,0x4a,0x04,0xd0,0xe2,0x89,0xf8,0xf6,0xf2,0x38,0x26,0x5b,0x04,0x58,
0xc3,0x56,0x51,0x50,0x89,0xf9,0x89,0xfe,0x4e,0x4e,0x26,0x80,0x3c,0x20,0x75,0xf8,
0x46,0x46,0x29,0xf1,0xe8,0x9d,0x01,0xe3,0x12,0x56,0x51,0xe8,0x04,0xfd,0x59,0x5e,
0x87,0xfe,0xd1,0xe9,0xb0,0x20,0xe8,0xd2,0xfe,0x89,0xf7,0x58,0x59,0x5e,0xc3,0x56,
0x52,0x51,0x53,0xff,0xd7,0x5b,0x59,0x5a,0x5e,0xc3,0xe8,0x06,0x00,0xbf,0x72,0x03,
0xe9,0x06,0xfd,0x1e,0x31,0xc0,0x8e,0xd8,0xa0,0x49,0x04,0x3c,0x07,0x74,0x0a,0xd0,
0xe8,0x72,0x0c,0x81,0xc6,0x9e,0x05,0xeb,0x0a,0x81,0xc6,0xa5,0x05,0xeb,0x04,0x81,
0xc6,0x97,0x05,0x2e,0xac,0x1f,0xc3,0x1e,0x08,0x1f,0x17,0x3f,0x94,0x12,0x0f,0x08,
0x0f,0x07,0x7f,0x8f,0x07,0x0f,0x78,0x0f,0x07,0x70,0x8f,0x07,0xe8,0x22,0x00,0xe8,
0x41,0x00,0xe8,0x4d,0x00,0xe8,0x42,0x00,0xe8,0x4f,0x00,0xe8,0x69,0x00,0xeb,0x7f,
0xe8,0x0e,0x00,0xe8,0xd8,0x01,0xbf,0x8c,0x03,0xe8,0xad,0xfc,0xe8,0x24,0x00,0xeb,
0x56,0xb3,0x01,0xb8,0xdd,0x06,0xbf,0x62,0x03,0xe8,0x9d,0xfc,0xe8,0x39,0xff,0xbf,
0x76,0x03,0xe8,0x94,0xfc,0xe8,0xad,0x01,0xbf,0x8c,0x03,0xe8,0x8b,0xfc,0x31,0xf6,
0xe9,0x77,0xff,0xba,0xfd,0x00,0x02,0x56,0x06,0xc3,0xe8,0x25,0x00,0xe8,0xdf,0x02,
0xeb,0x10,0xe8,0x15,0x00,0x8a,0x4e,0x04,0xeb,0x06,0xe8,0x15,0x00,0x8a,0x4e,0x05,
0x30,0xed,0xe3,0x05,0xe8,0x3d,0x00,0xe2,0xfb,0xc3,0xbe,0xc2,0x06,0xe8,0x65,0x00,
0xeb,0x40,0xbe,0xc5,0x06,0xeb,0x30,0xbe,0xc8,0x06,0xf6,0x46,0x0e,0x08,0x74,0x27,
0xe8,0x6f,0x00,0x52,0x83,0xea,0x13,0xbe,0xcb,0x06,0xe8,0x1a,0x00,0x5a,0xc3,0xbf,
0xa1,0x03,0xe8,0x34,0xfc,0x40,0xbf,0x8c,0x03,0xe8,0x2d,0xfc,0x42,0x42,0xe8,0x17,
0x00,0x4a,0x4a,0xc3,0xbe,0xce,0x06,0xe8,0x2b,0x00,0x52,0xba,0x01,0x00,0xe8,0x07,
0x00,0x5a,0xbf,0x39,0x04,0xe9,0x11,0xfc,0xe8,0xac,0xfc,0xbe,0x01,0x00,0xe8,0xf9,
0xfe,0x53,0xb3,0x01,0xb8,0xc0,0x02,0xbf,0x62,0x03,0xe8,0xfc,0xfb,0x5b,0xe8,0x16,
0x00,0xe8,0xb6,0xfc,0xc3,0x2e,0xac,0xe8,0x07,0x00,0x2e,0xac,0xe8,0x08,0x00,0x2e,
0xac,0xbf,0x40,0x04,0xe9,0xe2,0xfb,0x51,0x89,0xd1,0xbf,0x2b,0x04,0xe8,0xd9,0xfb,
0x59,0xc3,0xe8,0x59,0x03,0x97,0xbe,0x06,0x00,0x83,0xff,0x03,0x83,0xde,0x00,0xe8,
0xc1,0xfe,0x55,0x89,0xe5,0xbe,0xa6,0x01,0x50,0x57,0xbf,0x91,0x02,0xe8,0xb9,0xfb,
0x5d,0xc3,0xc9,0xcd,0xbb,0xc7,0xc4,0xb6,0xc8,0xcd,0xbc,0xc6,0xcd,0xbc,0xba,0x20,
0xba,0xe8,0x4c,0xfe,0x73,0x07,0x3c,0x20,0x72,0x1b,0xe8,0x54,0xfe,0x3c,0x0d,0x74,
0x08,0xe9,0xbd,0xfb,0xb0,0x0a,0xe8,0xc2,0xfb,0xb0,0x0d,0xe8,0xbd,0xfb,0xa0,0x5a,
0x04,0x30,0xe4,0x01,0xc7,0xc3,0x16,0x1f,0x89,0xee,0xb3,0x00,0xeb,0x28,0xb3,0xda,
0xb9,0xb3,0xe2,0x8b,0x4e,0x02,0xeb,0x1e,0xb3,0xa2,0xeb,0x1a,0x89,0xca,0x87,0x56,
0x02,0x52,0xb3,0x41,0xe8,0x0f,0x00,0x58,0xe8,0x28,0x02,0x8b,0x46,0x02,0xe9,0x22,
0x02,0xb3,0x93,0xba,0xb3,0x9d,0x06,0x1e,0x57,0x56,0x51,0x30,0xff,0xff,0x56,0x08,
0x59,0x5e,0x5f,0x1f,0x07,0xc3,0x06,0x1e,0x91,0xe8,0xdb,0xfb,0xb8,0x14,0x00,0x55,
0x29,0xc4,0x89,0xe5,0x91,0xe8,0x34,0x03,0xe8,0x0e,0x00,0x8b,0x56,0x02,0x83,0xc4,
0x14,0x5d,0xe8,0xe5,0xfb,0x92,0x1f,0x07,0xc3,0x89,0x5e,0x08,0x89,0x46,0x0a,0x89,
0x56,0x0c,0xb8,0x00,0x20,0xbf,0x88,0x03,0xe8,0x0e,0xfb,0xe8,0x88,0xff,0xe8,0x02,
0x00,0xeb,0x5b,0xe8,0x36,0xfe,0xe8,0x90,0x01,0xe8,0xb8,0x01,0xe9,0x96,0x01,0x80,
0x4e,0x0e,0x01,0xc3,0x2b,0x46,0x12,0x86,0xc4,0x05,0x02,0x01,0x3d,0x31,0xc0,0xeb,
0x1b,0xb8,0x02,0x01,0x3d,0x31,0xc0,0xeb,0x17,0xb8,0x02,0x01,0xeb,0x06,0x31,0xc0,
0xf9,0x12,0x66,0x05,0x51,0xe8,0x37,0x01,0x41,0x00,0xcc,0x59,0xf9,0x12,0x66,0x04,
0x57,0x50,0xbf,0xe4,0x03,0xe8,0xc1,0xfa,0x2a,0x46,0x06,0x2a,0x66,0x07,0xd0,0xe8,
0xd0,0xec,0x5f,0x01,0xf8,0x5f,0xc3,0xb8,0xfb,0x00,0x02,0x46,0x06,0xc3,0xe8,0x0a,
0x00,0xe8,0x0d,0x00,0xf6,0x46,0x0e,0x01,0x74,0xf4,0xc3,0xe8,0x18,0xfd,0x75,0x08,
0xc3,0xe8,0xff,0x01,0x73,0xfa,0xb0,0x0d,0x91,0xe8,0xea,0x01,0x91,0xe8,0x05,0x00,
0x72,0xee,0xe9,0x2c,0xff,0x3c,0x0d,0x75,0x08,0x8b,0x4e,0x02,0xe8,0x25,0xff,0xf9,
0xc3,0xf6,0x46,0x0e,0x04,0x75,0xf9,0x86,0xe0,0x3c,0x49,0x74,0x18,0x3c,0x51,0x74,
0x27,0x3c,0x47,0x74,0x1c,0x3c,0x4f,0x74,0x2c,0x3c,0x48,0x74,0x31,0x3c,0x50,0x74,
0x30,0xf8,0x86,0xe0,0xc3,0xe8,0xb7,0x00,0xf7,0xd9,0x89,0xc8,0x03,0x4e,0x02,0x7d,
0x23,0x8b,0x46,0x02,0xf7,0xd8,0xeb,0x1c,0xe8,0xa4,0x00,0x89,0xc8,0x03,0x4e,0x02,
0x3b,0x4e,0x00,0x72,0x0f,0xf9,0x8b,0x46,0x00,0x1b,0x46,0x02,0xeb,0x06,0xb0,0xff,
0xb9,0xb0,0x01,0x98,0xe8,0x2d,0x00,0xf9,0xc3,0x91,0xe8,0x82,0x00,0x3b,0x4e,0x00,
0x91,0xc3,0xe8,0x7a,0x00,0x8b,0x46,0x12,0xe8,0x13,0x00,0x39,0xc7,0x72,0x05,0xe8,
0x09,0x00,0x39,0xf8,0xb0,0xb1,0x72,0x02,0xb0,0xdb,0xc3,0xe8,0x4b,0x00,0xf7,0xe1,
0xf7,0x76,0x00,0xc3,0x8b,0x56,0x00,0x03,0x46,0x02,0x91,0x78,0x04,0x29,0xd1,0x73,
0x02,0x01,0xd1,0xe8,0x25,0x00,0x72,0x20,0x8b,0x56,0x12,0x2b,0x56,0x02,0x51,0xe8,
0x3d,0x00,0x8b,0x46,0x00,0x29,0xc8,0x59,0x01,0xca,0x79,0x01,0x99,0x39,0xd0,0x72,
0x01,0x92,0x89,0x46,0x12,0xe8,0x7c,0x00,0xe9,0x51,0xfe,0x39,0x4e,0x12,0x77,0x08,
0xe8,0x06,0x00,0x39,0xc1,0x77,0x01,0xf9,0xc3,0x91,0xe8,0x06,0x00,0x91,0x48,0x03,
0x46,0x12,0xc3,0xe8,0x09,0x00,0x3b,0x4e,0x00,0x72,0x03,0x8b,0x4e,0x00,0xc3,0xb9,
0xfb,0x00,0x02,0x4e,0x07,0x2a,0x4e,0x04,0x2a,0x4e,0x05,0xc3,0xe8,0x28,0xfa,0xe8,
0x37,0x00,0x8a,0x4e,0x05,0x8a,0x46,0x06,0x2c,0x04,0xf6,0xe1,0x91,0xb0,0x20,0xbf,
0x2b,0x04,0xe8,0x74,0xf9,0xe8,0x32,0xfa,0xc3,0x80,0x7e,0x04,0x00,0x74,0x33,0xe8,
0x0f,0x00,0xe9,0xe9,0xfd,0x80,0x7e,0x05,0x00,0x74,0x27,0xe8,0x0b,0x00,0xe9,0xe0,
0xfd,0xbe,0x02,0x00,0xe8,0x6a,0xfe,0xeb,0x06,0xbe,0x02,0x00,0xe8,0x6a,0xfe,0xba,
0xd1,0x06,0xeb,0x31,0x51,0xe8,0x9b,0xff,0x8b,0x46,0x12,0xe8,0x05,0x00,0x40,0xe2,
0xfa,0x59,0xc3,0x51,0x50,0x91,0xe8,0x72,0xff,0x73,0x0c,0xe8,0x0c,0x00,0xe8,0x29,
0x00,0xe8,0xb4,0xfd,0xe8,0x4e,0x00,0x58,0x59,0xc3,0x89,0xc8,0xe8,0x36,0x00,0xe8,
0x22,0xfe,0xba,0xdd,0x06,0xbf,0x8c,0x03,0xe8,0x0e,0xf9,0x92,0xb3,0x01,0xbf,0x62,
0x03,0xe8,0x05,0xf9,0xe8,0x8b,0xfb,0xe9,0xf0,0xfb,0xbf,0xa1,0x03,0xe8,0xf9,0xf8,
0x93,0xe8,0x6f,0xfc,0x83,0xea,0x02,0x2e,0xa0,0xcf,0x06,0xe8,0x09,0xfd,0x93,0xbf,
0x8c,0x03,0xe9,0xe4,0xf8,0xbe,0x03,0x00,0xf6,0x46,0x0e,0x02,0x75,0x06,0x3b,0x4e,
0x02,0x75,0x01,0x46,0xc3,0xe8,0xb1,0xfe,0x72,0x01,0xc3,0xe8,0x23,0xfc,0x89,0xc8,
0xe8,0xd1,0xfd,0x02,0x46,0x06,0x2c,0x04,0xbf,0x8c,0x03,0xe8,0xbb,0xf8,0x89,0xcf,
0x2b,0x7e,0x12,0xe8,0x9c,0xfe,0xe9,0xc8,0xfc,0x1e,0xe8,0x49,0x00,0xe8,0x5c,0x00,
0x80,0x4e,0x0e,0x08,0x1f,0xc3,0xf6,0x46,0x0e,0x08,0x74,0x21,0x80,0x66,0x0e,0xf7,
0xe9,0xdd,0xfb,0xf6,0x46,0x0e,0x08,0x74,0x14,0x1e,0xe8,0x29,0x00,0xe8,0x44,0x00,
0x1f,0x73,0x06,0x80,0x66,0x0e,0xf7,0xf9,0xc3,0xe8,0xc4,0xfb,0xf8,0xc3,0x1e,0x52,
0x51,0x53,0xe8,0x11,0x00,0xe8,0x2c,0x00,0x72,0x05,0x92,0xe8,0x17,0x00,0xba,0x31,
0xc0,0x5b,0x59,0x5a,0x1f,0xc3,0x16,0x1f,0x8d,0x5e,0x10,0xc3,0x3c,0x61,0x72,0x03,
0x3c,0x7b,0xc3,0xf8,0xc3,0xb8,0x10,0x0e,0xf7,0xe2,0x92,0xc3,0x89,0x07,0xe8,0x0e,
0x00,0x01,0x07,0xc3,0x52,0x8b,0x17,0xe8,0x05,0x00,0x92,0x29,0xd0,0x5a,0xc3,0x1e,
0x31,0xc0,0x8e,0xd8,0xa1,0x6c,0x04,0x1f,0xc3,0x1e,0x06,0x1f,0x07,0x87,0xf7,0xc3,
0x16,0x07,0x89,0xef,0xc3,0x31,0xdb,0x8e,0xdb,0xa3,0x72,0x04,0xb0,0xfe,0xe6,0x64,
0xeb,0x00,0x31,0xc0,0x50,0x9d,0x8e,0xd8,0x8e,0xc0,0x8e,0xd0,0xea,0x00,0x00,0xff,
0xff,0x31,0xc0,0xd1,0xe9,0xf3,0xab,0xd1,0xd1,0xf3,0xaa,0xc3,0x06,0x57,0x50,0xe8,
0xce,0xff,0xe8,0xec,0xff,0x58,0x5f,0x07,0xc3,0x57,0x56,0xbf,0x8b,0x0a,0x57,0x89,
0xdf,0x09,0xd7,0x75,0x1f,0x3d,0x10,0x27,0x73,0x1a,0x83,0xc4,0x02,0xbb,0x80,0x02,
0x93,0xf7,0xe6,0x92,0x91,0x93,0x2e,0x8a,0x97,0xae,0x0a,0x5e,0x5f,0xc3,0x20,0x6b,
0x4d,0x47,0x54,0x50,0x41,0xbe,0xff,0x03,0x21,0xc6,0x51,0xb1,0x0a,0xe8,0x04,0x00,
0xe2,0xfb,0x59,0xc3,0xd1,0xeb,0xd1,0xda,0xd1,0xd8,0xc3,0x20,0xac,0xab,0xb3,0xaf,
0x21,0x22,0x2c,0x2d,0x2e,0x2f,0x31,0x32,0x33,0x35,0x36,0x38,0xc8,0xb5,0x30,0x08,
0x30,0x30,0x39,0x39,0x6c,0x32,0x42,0x63,0x00,0x3e,0x9c,0xfb,0x1e,0x50,0x31,0xc0,
0x8e,0xd8,0xf6,0x06,0x17,0x04,0x04,0x75,0x0a,0xc7,0x06,0x64,0x00,0xe6,0x14,0x8c,
0x0e,0x66,0x00,0x58,0x1f,0x9d,0xcb,0xe8,0x90,0x04,0xe8,0xd8,0x04,0xe8,0x6e,0x00,
0xe8,0xef,0x00,0xe8,0x2f,0x00,0xe8,0xc9,0x03,0x8c,0x06,0x04,0x00,0x2e,0xf6,0x06,
0x40,0x00,0x01,0x74,0x1f,0xbb,0x04,0x01,0xb2,0x80,0xe8,0xba,0x01,0x72,0x0c,0x52,
0xe8,0xfc,0x04,0x5a,0x26,0x89,0x37,0x26,0x89,0x47,0x02,0x42,0x83,0xc3,0x14,0x80,
0xfa,0x82,0x72,0xe6,0xc3,0xb0,0x19,0xbe,0xd3,0x15,0xe8,0x22,0x00,0x26,0xa1,0x4e,
0x00,0xa3,0x02,0x00,0x92,0x26,0xa1,0x4c,0x00,0xa3,0x00,0x00,0xe8,0xc3,0x00,0x72,
0x09,0x26,0xa3,0x00,0x01,0x26,0x89,0x16,0x02,0x01,0xb0,0x13,0xbe,0xa3,0x21,0xb3,
0x04,0xf6,0xe3,0x93,0xfa,0x26,0x89,0x37,0x26,0x8c,0x4f,0x02,0xfb,0xc3,0x06,0x2e,
0xa1,0x42,0x00,0x85,0xc0,0x75,0x10,0x8e,0xd8,0x2e,0xa0,0x4b,0x00,0x29,0x06,0x13,
0x04,0xb0,0x40,0xf7,0x26,0x13,0x04,0x31,0xff,0x8e,0xd8,0x8e,0xc0,0xb9,0x10,0x00,
0xe8,0xce,0xfe,0xc7,0x06,0x04,0x00,0x5a,0x5a,0xc7,0x06,0x06,0x00,0x58,0x75,0xe8,
0x2e,0x07,0x07,0xc3,0x2e,0x8e,0x1e,0x42,0x00,0x8c,0xdf,0x85,0xff,0x75,0x17,0x8b,
0x3e,0x13,0x04,0x51,0xb1,0x06,0xd3,0xe7,0x59,0x8e,0xdf,0x83,0xc7,0x40,0x81,0x3e,
0x06,0x00,0x58,0x75,0x75,0xf3,0xc3,0xe8,0x1f,0x00,0x1e,0x31,0xdb,0x8e,0xdb,0x8a,
0x1e,0x75,0x04,0x38,0xd8,0x77,0x02,0x88,0xd8,0x1f,0xc3,0xa0,0x0c,0x00,0xd0,0xf8,
0xc3,0x2e,0x8a,0x0e,0x48,0x00,0x30,0xed,0xc3,0xa1,0x0a,0x00,0x00,0xe0,0x83,0xe0,
0x7f,0xc3,0xb0,0x2c,0x2e,0xf6,0x26,0x48,0x00,0x91,0xbf,0x04,0x7f,0x83,0xc1,0x0a,
0xe8,0x5e,0xfe,0xe9,0xf8,0x05,0xe8,0x18,0x00,0x04,0x41,0x3c,0x43,0x77,0x02,0xb0,
0x43,0xc3,0x26,0x80,0x3e,0x03,0x01,0xc0,0xf5,0xc3,0xb4,0x08,0x31,0xdb,0xcd,0x40,
0xc3,0xe8,0xb7,0xff,0x78,0x04,0x14,0x01,0xeb,0x03,0xe8,0x0d,0x00,0x2e,0x8a,0x26,
0x4a,0x00,0x38,0xe0,0x77,0x02,0x88,0xe0,0x98,0xc3,0x1e,0x31,0xc0,0x8e,0xd8,0xa0,
0x10,0x04,0x24,0xc1,0xd4,0x40,0x00,0xe0,0x1f,0xc3,0xe8,0xd6,0x00,0xe8,0x7c,0x00,
0x88,0xf8,0x83,0xe0,0x10,0x89,0x05,0xe8,0x26,0x02,0xe8,0x61,0x01,0x89,0x45,0x06,
0x89,0x5d,0x08,0x88,0xd0,0xd0,0xe0,0xd0,0xe0,0x08,0xc1,0xe8,0x9f,0x01,0x4a,0x7e,
0x03,0x80,0xc9,0x40,0x75,0x06,0xe8,0xc1,0x01,0x83,0xd3,0xff,0x08,0x0d,0x89,0x5d,
0x0a,0x89,0x45,0x0c,0xf6,0xc1,0x40,0x74,0x33,0xe8,0x0b,0x01,0x2d,0x01,0x40,0x81,
0xda,0xec,0x00,0x83,0xdb,0x00,0x73,0x16,0xe8,0xfc,0x00,0x91,0x8a,0x45,0x0a,0xf6,
0x65,0x0b,0x91,0xf7,0xf1,0x89,0x45,0x0c,0xf7,0xe1,0x31,0xdb,0xeb,0x05,0xe8,0xe6,
0x00,0x08,0x0d,0x89,0x45,0x0e,0x89,0x55,0x10,0x89,0x5d,0x12,0xe8,0x96,0x09,0x80,
0x16,0x0c,0x00,0x00,0x75,0x04,0xfe,0x06,0x0b,0x00,0xf8,0xc3,0x89,0x6d,0x02,0x2e,
0x8b,0x46,0x00,0x89,0x45,0x04,0xc3,0x97,0xa1,0x0a,0x00,0x00,0xc4,0x38,0xe2,0x73,
0x04,0x38,0xc2,0x73,0x10,0xe8,0xf3,0xfe,0x78,0x24,0x98,0x10,0xc4,0x38,0xd4,0x74,
0x04,0x38,0xd0,0x75,0x19,0x52,0xa1,0x0a,0x00,0x84,0xd2,0x78,0x05,0xe8,0xdb,0xfe,
0x00,0xe2,0x28,0xc2,0xb0,0x16,0xf6,0xe2,0x83,0xc0,0x10,0x5a,0x97,0xc3,0x31,0xc0,
0xf9,0x97,0xc3,0xe8,0x07,0x00,0x72,0x04,0xbe,0x2d,0x0d,0xf9,0xc3,0x3a,0x55,0x02,
0x75,0xf9,0xc3,0x52,0x8b,0x16,0x0b,0x00,0x00,0xf2,0xeb,0xd8,0x84,0x5d,0x01,0x75,
0x01,0xf9,0xc3,0xb3,0x04,0xbe,0x3c,0x0d,0xeb,0x08,0xbe,0x2d,0x0d,0xeb,0x03,0xbe,
0x23,0x0d,0x51,0xbf,0x10,0x00,0x8a,0x0e,0x0b,0x00,0x30,0xed,0xe3,0x09,0xff,0xd6,
0x73,0x06,0x83,0xc7,0x16,0xe2,0xf7,0xf9,0x59,0xc3,0x8a,0x05,0x24,0x50,0x0c,0xa0,
0xc3,0xb0,0x02,0xc3,0x8b,0x45,0x06,0x8b,0x5d,0x08,0xc3,0x8a,0x65,0x14,0x80,0xfc,
0x0c,0x72,0x04,0x80,0xfc,0x13,0xf5,0xc3,0x8b,0x45,0x0e,0x8b,0x55,0x10,0x8b,0x5d,
0x12,0xc3,0xe8,0x0c,0x00,0x83,0xc3,0x0c,0xf6,0x05,0x10,0x75,0x03,0x83,0xeb,0x06,
0xc3,0x8a,0x5d,0x02,0x30,0xff,0xc3,0xbb,0x49,0x0a,0xff,0xd3,0x53,0xf6,0x85,0xa7,
0x00,0x04,0x74,0x0f,0xb1,0x80,0x8b,0x85,0xc8,0x00,0x8b,0x95,0xca,0x00,0x8b,0x9d,
0xcc,0x00,0xc3,0x30,0xc9,0x31,0xdb,0x8b,0x45,0x78,0x8b,0x55,0x7a,0xc3,0xe8,0x4c,
0x00,0x4a,0x79,0x04,0xe8,0x7c,0x00,0x42,0x74,0x50,0x4a,0x74,0x0c,0x3d,0x00,0x04,
0x76,0x57,0x26,0xf6,0x44,0x63,0x02,0x74,0x41,0x93,0xf6,0xe4,0xf7,0xe3,0xb9,0x3f,
0x00,0x31,0xdb,0x93,0x92,0xf7,0xf1,0x93,0xf7,0xf1,0x89,0xda,0x50,0x52,0x31,0xdb,
0x83,0xe8,0x01,0x19,0xda,0xe8,0xb2,0xfc,0x5a,0x40,0xb3,0x10,0x39,0xd8,0x76,0x05,
0xd1,0xe3,0x7b,0xf8,0x4b,0x58,0xf7,0xf3,0x86,0xf9,0xb2,0x02,0xc3,0x26,0x8b,0x44,
0x02,0x26,0x8a,0x5c,0x06,0x26,0x8a,0x7c,0x0c,0xc3,0xe8,0x1d,0x00,0x72,0x0a,0x88,
0xd9,0x30,0xed,0x4b,0xf7,0xe1,0x49,0xf7,0xf1,0x99,0x31,0xc9,0x3d,0x00,0x04,0x76,
0xdb,0xd1,0xe8,0xd0,0xe3,0x41,0xb2,0x01,0xeb,0xf2,0x3d,0x01,0x20,0x72,0x03,0x80,
0xfb,0x10,0xc3,0x3d,0x00,0x04,0x72,0x03,0xb8,0x00,0x04,0xc3,0xe8,0x17,0x00,0x1e,
0x56,0x06,0x1f,0x83,0xc6,0x36,0x89,0xdf,0xb9,0x0f,0x00,0xad,0x86,0xc4,0xab,0xe2,
0xfa,0x91,0xab,0x5e,0x1f,0xc3,0x8d,0x9d,0x77,0x3f,0xd1,0xe3,0xc3,0x5e,0x07,0x72,
0x0e,0xb9,0xff,0x3f,0x26,0x39,0x4c,0x02,0x72,0x04,0x26,0x89,0x4c,0x02,0xf8,0xc3,
0xe8,0xff,0xfe,0x1e,0x0e,0x1f,0x8a,0x17,0x8b,0x47,0x02,0x8b,0x5f,0x04,0x06,0x1f,
0xf6,0xc2,0x20,0x74,0x18,0x89,0x44,0x02,0x88,0xd8,0x30,0xe4,0x89,0x44,0x06,0x88,
0xf8,0x89,0x44,0x0c,0x80,0x64,0x63,0xfd,0x80,0xa4,0xa7,0x00,0xfb,0xf6,0xc2,0x40,
0x74,0x17,0x3b,0x5c,0x7a,0x77,0x12,0x72,0x05,0x3b,0x44,0x78,0x77,0x0b,0x89,0x44,
0x78,0x89,0x5c,0x7a,0x80,0xa4,0xa7,0x00,0xfb,0x83,0xe2,0x0c,0xd1,0xea,0xd1,0xea,
0x1f,0xc3,0xe8,0x0c,0xfd,0xbd,0x4e,0x00,0x51,0xb9,0xa8,0x00,0xb7,0xa0,0xe8,0x86,
0x00,0x2e,0xf6,0x46,0x0c,0x80,0x75,0x08,0xb9,0xae,0x00,0xb7,0xb0,0xe8,0x77,0x00,
0xe8,0x3c,0x03,0x59,0x83,0xc5,0x12,0xe3,0x1b,0xe2,0xdd,0xe8,0x35,0xfe,0x73,0x14,
0xbd,0x96,0x00,0x3c,0x40,0x74,0xd1,0x2e,0xa0,0x40,0x00,0x26,0x0a,0x06,0x17,0x04,
0x24,0x08,0x75,0xc4,0x8b,0x0e,0x0b,0x00,0x26,0xa0,0x75,0x04,0x2e,0xf6,0x06,0x40,
0x00,0x02,0x74,0x02,0x30,0xc0,0x00,0xc1,0x26,0x88,0x0e,0x75,0x04,0x0c,0x80,0xa2,
0x0a,0x00,0xfe,0xcd,0x88,0xe8,0x78,0x2b,0xe8,0xff,0xfc,0x50,0x00,0xe8,0x3c,0x03,
0x72,0x02,0xb0,0x03,0xd0,0xc8,0xd0,0xc8,0x40,0xb4,0x3e,0x26,0x22,0x26,0x10,0x04,
0x08,0xe0,0x26,0xa2,0x10,0x04,0xb0,0x1e,0xbe,0xfc,0x23,0xe8,0x01,0xfc,0x58,0xd0,
0xed,0xd0,0xd0,0xa2,0x0c,0x00,0xc3,0xe8,0x26,0x00,0xbe,0x00,0x7c,0x06,0x56,0x52,
0x53,0xe8,0xf6,0x06,0x5b,0x5a,0x5e,0x07,0x73,0x06,0xbe,0x0e,0x01,0xe9,0x93,0x00,
0xe8,0xc7,0xfc,0x72,0xf5,0xe8,0xc4,0xfe,0xeb,0x42,0xbf,0xdb,0x02,0xe9,0xd9,0xf2,
0x2e,0x8b,0x46,0x00,0x2e,0x8b,0x56,0x03,0x55,0x89,0xe5,0x51,0xb1,0xd1,0x80,0xfe,
0x18,0x75,0x21,0xb1,0xbb,0x51,0x52,0xb1,0xc0,0x84,0xd2,0x74,0x17,0xb1,0xcc,0x88,
0xe0,0x98,0x96,0xb8,0x80,0x04,0x99,0xf7,0xf6,0xbe,0x0a,0x00,0xf7,0xf6,0x39,0xf0,
0x73,0x02,0xb1,0xc7,0x51,0x50,0x52,0xbe,0xb4,0x00,0xeb,0x4a,0x55,0x89,0xe5,0x53,
0xbe,0xf7,0x00,0xeb,0x41,0xbe,0xd4,0x00,0x8c,0xcf,0x55,0x89,0xe5,0xb8,0x0c,0x00,
0x50,0x57,0xb0,0x2c,0x50,0xeb,0x2f,0x55,0x89,0xe5,0x88,0xe1,0x30,0xed,0x51,0xbe,
0x17,0x01,0xeb,0x22,0x55,0x89,0xe5,0xe8,0x9c,0x02,0x88,0xd6,0xe8,0x97,0x02,0xe8,
0x84,0x02,0x86,0xd6,0xe8,0x7f,0x02,0x52,0x86,0xd6,0x52,0xe8,0x67,0x02,0xbe,0xf9,
0x00,0xeb,0x03,0x55,0x89,0xe5,0xbf,0x91,0x02,0xe8,0x4d,0xf2,0xf9,0x5d,0xc3,0x06,
0xe8,0x3e,0xfd,0x91,0x89,0xfe,0xe8,0x17,0x00,0xd1,0xea,0x73,0x03,0x83,0xc7,0x10,
0xe8,0x1e,0x00,0x87,0xfe,0x8c,0xc0,0x07,0xc3,0xe8,0x04,0x00,0x83,0xc7,0x20,0xc3,
0x1e,0x07,0x2e,0x8b,0x3e,0x4b,0x00,0x51,0xb1,0x0a,0xd3,0xe7,0x59,0x83,0xef,0x30,
0xc3,0x8b,0x44,0x06,0xe8,0xec,0xfd,0xf6,0x04,0x0c,0x74,0x39,0x99,0xe8,0xac,0x00,
0x8a,0x44,0x08,0xb4,0xa0,0xe8,0xa4,0x00,0x8a,0x44,0x0b,0x30,0xe4,0xe8,0x9c,0x00,
0x88,0xe0,0xab,0x91,0x8a,0x64,0x0c,0xe8,0x92,0x00,0x8a,0x44,0x0d,0x8a,0x64,0x0a,
0xe8,0x89,0x00,0x91,0xab,0x8a,0x44,0x09,0xe8,0x02,0x00,0xeb,0x1e,0x88,0xc4,0x00,
0xd4,0xf6,0xdc,0xab,0xc3,0xab,0x8a,0x44,0x08,0x30,0xe4,0xab,0x88,0xe0,0xab,0xab,
0x88,0xc8,0xab,0x88,0xe0,0xab,0xab,0x8a,0x44,0x09,0xab,0x83,0xef,0x10,0xc3,0xe8,
0x87,0xff,0xb9,0x10,0x00,0x80,0x7c,0x14,0x02,0x75,0x02,0xb1,0x90,0x30,0xd2,0x55,
0xbd,0x1c,0x11,0x8b,0x44,0x04,0xff,0xd5,0x8a,0x5c,0x02,0x30,0xff,0x2e,0x8b,0x47,
0x02,0xff,0xd5,0x87,0xfe,0xe8,0x82,0xfc,0x87,0xf7,0xff,0xd5,0x2e,0x8a,0x47,0x05,
0x8a,0x64,0x15,0x9e,0x72,0x03,0x80,0xc9,0x04,0xff,0xd5,0x31,0xc0,0xab,0x8a,0x04,
0x24,0x0c,0x74,0x0a,0x80,0xc9,0x08,0xa8,0x08,0x74,0x03,0x80,0xcd,0x02,0x91,0xff,
0xd5,0x5d,0x31,0xc0,0xab,0xb0,0x11,0x8d,0x5d,0xf2,0xeb,0x81,0xab,0x00,0xc2,0x00,
0xe2,0xc3,0x06,0x50,0x31,0xc0,0x8e,0xc0,0x9c,0x26,0xff,0x1e,0x04,0x7f,0x26,0xf6,
0x06,0x6c,0x04,0x03,0x75,0x10,0x1e,0x57,0x56,0x52,0x51,0xe8,0x76,0xfa,0xe8,0x08,
0x00,0x59,0x5a,0x5e,0x5f,0x1f,0x58,0x07,0xcf,0xe8,0xf3,0x00,0x3c,0x01,0x75,0x05,
0xc6,0x06,0x08,0x00,0x00,0xbf,0xa1,0x03,0xe8,0x1e,0xf1,0x50,0x8c,0xc0,0xe8,0x62,
0x00,0xe8,0xcd,0xfa,0x91,0xe3,0x13,0xb8,0x41,0xaf,0x26,0x8a,0x0e,0x0a,0x7f,0xbf,
0x1f,0x01,0x26,0x8a,0x2e,0x0c,0x7f,0xe8,0x4f,0x00,0xe8,0x99,0xfa,0xb4,0xaf,0x26,
0x8b,0x0e,0x0b,0x7f,0x80,0xf5,0x01,0xbf,0x25,0x01,0xe8,0x3c,0x00,0xb8,0x3c,0x32,
0xbf,0x2b,0x01,0xe8,0x41,0x00,0xb8,0x40,0x36,0xbf,0x3a,0x01,0xe8,0x38,0x00,0xb8,
0x42,0x38,0xbf,0x46,0x01,0xe8,0x2f,0x00,0xbf,0xe4,0x03,0xe8,0xcb,0xf0,0x88,0xc1,
0x30,0xed,0xbf,0xa1,0x03,0xe8,0xc1,0xf0,0x28,0xc1,0xb0,0x20,0xbf,0x2b,0x04,0xe8,
0xb7,0xf0,0x58,0xbf,0x8c,0x03,0xe9,0xb0,0xf0,0x26,0x80,0x3e,0x0d,0x7f,0x3b,0x73,
0x14,0x84,0xed,0x75,0x10,0xeb,0x09,0x31,0xc9,0x26,0x38,0x06,0x0d,0x7f,0xb0,0x46,
0xbe,0x04,0x00,0x74,0x03,0xbe,0x03,0x00,0x92,0xe8,0x87,0xf3,0x92,0x55,0x89,0xe5,
0xbe,0x0f,0x00,0x56,0x50,0x88,0xe0,0x50,0x52,0x57,0x51,0x56,0xbe,0x33,0x01,0xe9,
0x24,0xfe,0xe8,0x11,0xfa,0xb4,0x41,0x86,0xc4,0x26,0xa3,0x0a,0x7f,0xc3,0x1e,0x06,
0x1f,0xa1,0x20,0x00,0xa3,0x04,0x7f,0xa1,0x22,0x00,0xa3,0x06,0x7f,0xb0,0x08,0xbe,
0x22,0x11,0xe8,0x4a,0xf9,0xa1,0x6c,0x04,0xa3,0x08,0x7f,0x1f,0xe8,0xd3,0xff,0x2e,
0x8a,0x16,0x49,0x00,0xe8,0x5f,0x00,0x88,0xd0,0x30,0xe4,0x0c,0x20,0xeb,0x09,0xe8,
0xb4,0xf2,0x74,0x28,0xb9,0x3f,0x12,0x51,0xbf,0x0d,0x7f,0x26,0x88,0x25,0xe8,0xcb,
0xf7,0x73,0x19,0x24,0xdf,0x4f,0x26,0x80,0x25,0xfe,0x91,0xe8,0xb8,0xf9,0x38,0xc1,
0x72,0x04,0x26,0x80,0x0d,0x01,0x83,0xdf,0x01,0x91,0xaa,0xf9,0x26,0xa0,0x0d,0x7f,
0xc3,0x26,0x8b,0x16,0x0a,0x7f,0x26,0xf6,0x06,0x0c,0x7f,0x01,0x75,0x02,0x86,0xd6,
0xe8,0x02,0x00,0x86,0xd6,0xe8,0x8e,0xf9,0x38,0xc2,0x72,0x06,0x28,0xc2,0x80,0xca,
0x80,0xc3,0x80,0xea,0x41,0xc3,0x80,0xf2,0x80,0x78,0x07,0xe8,0x78,0xf9,0x00,0xc2,
0xf9,0xc3,0x80,0xea,0x3f,0xc3,0x97,0xb4,0x80,0xa0,0x0f,0x00,0x84,0xe2,0x75,0x04,
0xa0,0x0e,0x00,0x98,0x38,0xd4,0x74,0x06,0x38,0xd0,0x75,0x22,0x88,0xe0,0x88,0xc2,
0x3c,0x81,0x75,0x1a,0x1e,0x31,0xc0,0x8e,0xd8,0xa1,0x04,0x01,0x87,0x06,0x18,0x01,
0xa3,0x04,0x01,0xa1,0x06,0x01,0x87,0x06,0x1a,0x01,0xa3,0x06,0x01,0x1f,0x97,0xc3,
0xc7,0x06,0x0e,0x00,0x00,0x80,0xc3,0x84,0xd2,0x78,0x05,0x88,0x16,0x0e,0x00,0xc3,
0x88,0x16,0x0f,0x00,0xc3,0xe8,0xe8,0xff,0xbb,0x2f,0x13,0xbf,0x36,0x07,0xe8,0x5e,
0xf2,0x91,0xb8,0x20,0x07,0xbf,0x14,0x04,0xe8,0x6e,0xef,0x88,0xca,0xe8,0x21,0xf9,
0x38,0xc2,0x72,0x0b,0x0c,0x80,0x28,0xc2,0xe8,0xbc,0xf8,0x0c,0x80,0x38,0xc2,0xc3,
0x30,0xf6,0x84,0xd2,0x79,0x08,0xe8,0x08,0xf9,0x80,0xe2,0x7f,0x01,0xc2,0xc3,0x31,
0xff,0x8e,0xc7,0xe8,0x7e,0xf8,0x81,0xc3,0x3f,0x13,0xff,0xe3,0x31,0xd2,0xc3,0xe8,
0x95,0xf8,0x91,0xe8,0xeb,0xf8,0x01,0xc8,0x40,0x89,0x46,0x00,0xc7,0x46,0x04,0x02,
0x03,0xc6,0x46,0x06,0x28,0x04,0x0a,0x91,0xbf,0xe4,0x03,0xe8,0x1b,0xef,0x38,0xcc,
0x72,0x02,0x88,0xcc,0x88,0x66,0x07,0x2e,0xa1,0x46,0x00,0xbf,0xc9,0x09,0xe8,0xee,
0xf1,0x2e,0x8a,0x16,0x49,0x00,0x30,0xf6,0xe8,0xa5,0xff,0x89,0x56,0x02,0xf9,0xc3,
0x51,0xe8,0x7e,0xfe,0xe8,0x59,0xff,0xe8,0x81,0xff,0x73,0x08,0xe8,0x58,0xff,0xe8,
0xa2,0xfe,0xeb,0x05,0xb4,0x42,0xe8,0xaf,0xfe,0xb0,0x07,0xbf,0x72,0x03,0xe8,0xd8,
0xee,0xb3,0x01,0xb8,0xa1,0x02,0xbf,0x62,0x03,0xe8,0xcd,0xee,0xe8,0xa6,0xfd,0x31,
0xc0,0xbf,0x43,0x09,0xe8,0xa8,0xf1,0xb2,0x80,0xe8,0x64,0xff,0x92,0xbf,0x43,0x09,
0xe8,0x9c,0xf1,0x58,0xbf,0x43,0x09,0xe8,0x95,0xf1,0xbf,0x15,0x09,0xe8,0x8f,0xf1,
0xf9,0xc3,0x80,0xfc,0x3c,0x74,0x08,0xe8,0x6e,0xfe,0x73,0x03,0xe8,0xa0,0xf3,0xf9,
0xc3,0x55,0x89,0xe5,0xe8,0x24,0xff,0xbe,0x42,0x01,0xb0,0x20,0x73,0x28,0xe8,0xf6,
0xf8,0x72,0x08,0xe8,0x80,0xfa,0xbe,0xbc,0x01,0xeb,0x0c,0xbe,0xb8,0x01,0xbb,0xcb,
0x01,0x84,0xd2,0x78,0x02,0xb3,0xbf,0x89,0xd0,0xe8,0x9a,0xfe,0x80,0xfa,0x10,0x83,
0xde,0x00,0x52,0x53,0x04,0x41,0x50,0xeb,0x51,0x31,0xff,0xbe,0xf3,0x00,0xe9,0xc9,
0xfb,0xbf,0xec,0x08,0xe8,0x38,0xf1,0xe8,0xe1,0xfe,0x73,0xb3,0x55,0x89,0xe5,0xbe,
0xda,0x01,0xe8,0xb2,0xf8,0xfe,0xc2,0xfe,0xca,0x78,0x32,0x73,0x05,0xe8,0xea,0xf7,
0xeb,0x03,0xe8,0x9a,0x0f,0xb8,0x55,0x01,0x84,0xdb,0x74,0x1d,0xb0,0x4e,0x80,0xfb,
0x06,0x77,0x16,0xb0,0x61,0x50,0xb0,0x6a,0x80,0xfb,0x02,0x77,0x02,0xb0,0x6c,0x50,
0x30,0xff,0xb0,0x78,0x2e,0xf6,0xa7,0xdf,0x14,0x50,0xe9,0xb9,0xfb,0x72,0x13,0xb8,
0xeb,0x01,0x50,0xb8,0x88,0x14,0x50,0xf6,0x05,0x40,0x74,0x03,0xe9,0x09,0xf9,0xe9,
0xac,0x10,0xe8,0x21,0xfe,0xe8,0x9f,0x10,0xe8,0x39,0xf6,0xb9,0xe4,0x01,0x51,0xb9,
0x01,0x00,0xe8,0xf4,0xf5,0x50,0x51,0x52,0x85,0xff,0x74,0xce,0x8a,0x05,0x83,0xe0,
0x0c,0xbb,0x80,0x01,0xf7,0xe3,0x86,0xc4,0x05,0x6e,0x01,0x50,0xb8,0x01,0x00,0xf6,
0x45,0x01,0x02,0x74,0x03,0x8a,0x45,0x15,0x50,0xe8,0xe5,0xf8,0x2e,0x8a,0x67,0x04,
0xf6,0x45,0x01,0x04,0x75,0x03,0x8a,0x65,0x14,0xb0,0x03,0xf6,0xe4,0xd1,0xe8,0x05,
0x7f,0x01,0x50,0x2e,0x8a,0x47,0x05,0x98,0x50,0x8a,0x45,0x03,0x50,0xe9,0x46,0xfb,
0x03,0x0a,0x06,0x0c,0x18,0x18,0xfb,0xfc,0x31,0xc0,0x8e,0xc0,0x26,0x89,0x26,0x00,
0x7f,0x26,0x8c,0x16,0x02,0x7f,0xfa,0x31,0xe4,0x8e,0xd4,0xbc,0x00,0x7f,0xfb,0x2e,
0xa1,0x44,0x00,0x3c,0x04,0x74,0x02,0xcd,0x10,0xe8,0x8e,0xfa,0xe8,0xf8,0xf5,0x1e,
0x06,0x1f,0xa1,0x6c,0x04,0x2b,0x06,0x08,0x7f,0x83,0xf8,0x48,0x72,0xf4,0xfa,0xa1,
0x04,0x7f,0xa3,0x20,0x00,0xa1,0x06,0x7f,0xa3,0x22,0x00,0xfb,0x1f,0x31,0xc0,0xb2,
0x80,0xcd,0x13,0x26,0xa0,0x0d,0x7f,0x3c,0x42,0x74,0x28,0x3c,0x3c,0x74,0x1a,0xe8,
0x2f,0xfd,0xe8,0xa2,0xfd,0x86,0xd6,0xe8,0x9d,0xfd,0x86,0xd6,0xe8,0x57,0xfd,0x52,
0xe8,0x2f,0x00,0x5a,0x88,0xf2,0xe8,0x29,0x00,0xe8,0x99,0xfd,0x73,0x05,0xe8,0x7f,
0xfd,0xeb,0xdc,0xe8,0x7a,0xfd,0xf9,0x8c,0xc0,0xfa,0x8e,0xd0,0x36,0x8b,0x26,0x00,
0x7f,0x36,0x8e,0x16,0x02,0x7f,0xfb,0x8c,0xc0,0x8e,0xd8,0x72,0x03,0x06,0x53,0xcb,
0xcd,0x18,0xe8,0x7f,0xfa,0xe8,0x25,0x00,0x42,0xfe,0xca,0x73,0x0f,0x78,0x0a,0x80,
0xfc,0x80,0x74,0x2f,0x80,0xfc,0x31,0x74,0x2a,0xe9,0x5b,0xfa,0x79,0xc9,0x26,0x81,
0xbf,0xfe,0x01,0x55,0xaa,0x74,0xc0,0xbe,0x04,0x01,0xe9,0x76,0xfa,0x31,0xdb,0x8e,
0xc3,0xbb,0x00,0x7c,0xbf,0x03,0x00,0xb8,0x01,0x02,0xb9,0x01,0x00,0x30,0xf6,0xcd,
0x13,0x72,0x01,0xc3,0x4f,0x74,0xfc,0x31,0xc0,0x84,0xd2,0x79,0x02,0xb4,0x0d,0xcd,
0x13,0xeb,0xe4,0x31,0xd2,0xe8,0xd5,0xff,0x72,0x08,0x81,0xbf,0xfe,0x01,0x55,0xaa,
0x74,0x95,0xb8,0x34,0x12,0xe9,0x6d,0xf4,0x58,0x83,0xec,0x0c,0x89,0xe5,0xff,0xe0,
0x26,0x8a,0x44,0x02,0x88,0x46,0x02,0x88,0x66,0x06,0x26,0x8a,0x44,0x08,0x88,0x46,
0x03,0x26,0x8b,0x44,0x09,0x89,0x46,0x04,0x26,0x8a,0x64,0x0b,0x88,0x66,0x09,0x26,
0x8b,0x4c,0x0c,0x89,0x4e,0x0a,0x80,0xe4,0x0f,0xe8,0x4e,0xf7,0x08,0xe0,0x88,0x46,
0x01,0x26,0xc4,0x74,0x04,0xeb,0x19,0x88,0x46,0x02,0x88,0x66,0x06,0x53,0xe8,0xf6,
0x0f,0xe8,0x36,0xf7,0x08,0xf8,0x88,0x46,0x01,0x88,0x5e,0x03,0x89,0x4e,0x04,0x5b,
0xe8,0x2e,0xf7,0x88,0x46,0x07,0xeb,0x4a,0x89,0x76,0x00,0x88,0x46,0x06,0x89,0x56,
0x02,0x89,0x4e,0x04,0x80,0xe4,0x0f,0xe8,0x10,0xf7,0x08,0xe0,0x88,0x46,0x01,0xc6,
0x46,0x07,0x02,0xeb,0x2d,0x2e,0x80,0x7e,0x04,0x18,0x74,0x03,0xe9,0x8c,0x08,0xe9,
0x1e,0x0b,0xf6,0x45,0x01,0x04,0x75,0x2f,0xeb,0x6b,0x81,0xf9,0x58,0x75,0x74,0x06,
0x06,0x56,0xb9,0x7d,0x0e,0x51,0x2e,0x80,0x7e,0x04,0x18,0x74,0x02,0xeb,0x76,0xe9,
0x7f,0x0a,0xf6,0x45,0x01,0x04,0x75,0x03,0xe9,0xab,0x00,0xe9,0x24,0x09,0xf6,0x45,
0x01,0x04,0x75,0x03,0xe9,0xe5,0x00,0x31,0xc0,0xc3,0x88,0x0e,0x08,0x00,0xe8,0x29,
0x00,0x88,0x0e,0x09,0x00,0xc3,0xe8,0x21,0x00,0x3a,0x0e,0x09,0x00,0x74,0x09,0x88,
0x0e,0x09,0x00,0x80,0x2e,0x08,0x00,0x01,0xc3,0xb8,0x02,0x00,0xfb,0xe8,0x0a,0x00,
0x01,0xc8,0xe8,0x05,0x00,0x39,0xc1,0x75,0xf9,0xc3,0x1e,0x31,0xc9,0x8e,0xd9,0x8b,
0x0e,0x6c,0x04,0x1f,0xc3,0xb0,0x06,0xb2,0x06,0xe8,0x65,0x08,0xb8,0x06,0x00,0xe8,
0xd7,0xff,0xb0,0x02,0xb2,0x06,0xe8,0x58,0x08,0xb8,0x34,0x08,0xe8,0xca,0xff,0xbb,
0x80,0xff,0xe9,0x8a,0x07,0xe8,0x2b,0xf6,0x88,0xf8,0x30,0xe4,0x89,0x05,0xe8,0xcb,
0xf5,0xe8,0x04,0x08,0xc6,0x45,0x15,0x01,0x83,0xfd,0x4e,0x75,0x0b,0xf6,0xc7,0x10,
0x75,0x06,0xbb,0x80,0xb5,0xe8,0x67,0x07,0x55,0xe8,0xbc,0xfe,0x56,0xe8,0x9b,0x0d,
0xe8,0x54,0x0e,0xe8,0x9e,0x0d,0x5e,0xb2,0x01,0xb0,0xec,0xbb,0x08,0xff,0xe8,0x07,
0xff,0x8d,0x66,0x0c,0x5d,0xc3,0x53,0xe8,0x42,0x00,0x72,0x3e,0x8a,0x46,0x07,0xb2,
0x06,0xe8,0xfd,0x07,0x8a,0x46,0x00,0xb2,0x01,0xe8,0x28,0x08,0x8a,0x66,0x09,0x30,
0xc0,0x8b,0x4e,0x0a,0xe8,0x49,0x00,0x8b,0x46,0x02,0x8b,0x4e,0x04,0xe8,0x40,0x00,
0x8a,0x46,0x06,0xb2,0x07,0xe8,0x0c,0x08,0x5b,0x80,0xfb,0x08,0x75,0x09,0x80,0x7d,
0x14,0x14,0x73,0x4d,0xe9,0x7f,0x01,0xe9,0x05,0x07,0x5b,0xc3,0xb9,0x40,0xb5,0x81,
0x3e,0x04,0x00,0x5a,0x5a,0x75,0x02,0xb5,0x09,0x8a,0x46,0x01,0xb2,0x06,0xe8,0xe3,
0x07,0x89,0xcb,0xe8,0xe9,0x06,0x80,0xfc,0x80,0x74,0x03,0x31,0xc0,0xc3,0xf9,0xc3,
0xb2,0x02,0xe8,0xcf,0x07,0x88,0xe0,0xb2,0x03,0xe8,0xc8,0x07,0x88,0xc8,0xb2,0x04,
0xe8,0xc1,0x07,0x88,0xe8,0xb2,0x05,0xe8,0xba,0x07,0xc3,0xb2,0x03,0xe8,0x52,0x07,
0xc3,0x91,0x31,0xc0,0x88,0x46,0x08,0x8a,0x76,0x02,0x88,0x76,0x06,0x8a,0x45,0x15,
0x89,0x46,0x00,0x89,0x7e,0x02,0x8c,0x5e,0x04,0xe8,0xe5,0x01,0x80,0x7d,0x14,0x14,
0x8e,0x5d,0x04,0xbf,0x00,0x1e,0x75,0x03,0xbf,0x00,0x3c,0xf6,0xc1,0x10,0x75,0x4a,
0x80,0xf9,0xc5,0x74,0x45,0x87,0xf7,0xe8,0x8b,0x00,0x72,0x35,0x8b,0x56,0x00,0x38,
0x56,0x06,0x76,0x16,0xe8,0x70,0x00,0xe8,0x7b,0x00,0x72,0x25,0x8b,0x56,0x00,0x28,
0x56,0x06,0x00,0x56,0x08,0x38,0x56,0x06,0x77,0xea,0x8a,0x56,0x06,0x52,0xe8,0x56,
0x00,0xc5,0x7e,0x02,0xbb,0x80,0xff,0xe8,0x55,0x06,0x59,0x72,0x04,0x02,0x4e,0x08,
0xc3,0xc5,0x7e,0x02,0xb5,0x00,0x8a,0x4e,0x08,0xc3,0x06,0x1e,0x07,0x1f,0xe8,0x44,
0x00,0x72,0xee,0x8b,0x56,0x00,0x38,0x56,0x06,0x76,0x16,0xe8,0x1b,0x00,0xe8,0x34,
0x00,0x72,0xde,0x8b,0x56,0x00,0x28,0x56,0x06,0x00,0x56,0x08,0x38,0x56,0x06,0x77,
0xea,0x8a,0x56,0x06,0x52,0xbb,0x31,0x18,0x53,0x89,0xfb,0x31,0xc9,0xfe,0xc5,0xf3,
0xa5,0x89,0xdf,0x4a,0x75,0xf7,0xc3,0x89,0xf3,0x31,0xc9,0xfe,0xc5,0xf3,0xa5,0x89,
0xde,0x4a,0x75,0xf7,0xc3,0x1e,0x57,0xc5,0x7e,0x02,0xe8,0xef,0x05,0x5f,0x1f,0xc3,
0x87,0xf7,0xb3,0x0b,0xe8,0x05,0x00,0x87,0xfe,0xc3,0xb3,0x07,0x80,0xc2,0x1e,0x86,
0xcd,0xd1,0xe1,0x89,0xf8,0xf7,0xd8,0x73,0x0c,0x39,0xc1,0x76,0x08,0x91,0x29,0xc8,
0x50,0xe8,0x01,0x00,0x59,0xb0,0x07,0xfa,0xe6,0x0a,0x88,0xd8,0xe6,0x0b,0x8c,0xc0,
0xe6,0x82,0x89,0xf8,0xe6,0x0c,0xe6,0x06,0x88,0xe0,0xe6,0x06,0x89,0xc8,0x48,0xe6,
0x07,0x88,0xe0,0xe6,0x07,0xb0,0x03,0xe6,0x0a,0xfb,0x8c,0xc0,0x01,0xcf,0x14,0x00,
0x8e,0xc0,0x83,0xc1,0x0f,0x91,0xb1,0x04,0xd3,0xe8,0x91,0xee,0xe2,0xfd,0x41,0xe4,
0x08,0xa8,0x08,0x74,0xf6,0xc3,0xa8,0x10,0x8a,0x66,0x02,0x75,0x4f,0x3c,0xc5,0x74,
0x4b,0xbb,0xe4,0x19,0xe8,0x82,0x00,0xe8,0x72,0x05,0x72,0x3a,0x87,0xf7,0x8b,0x4e,
0x04,0x8b,0x56,0x00,0x38,0x4e,0x06,0x76,0x17,0xff,0x56,0x02,0x87,0xfe,0xe8,0x5b,
0x05,0x72,0x23,0x87,0xf7,0x8b,0x4e,0x04,0x28,0x4e,0x06,0x00,0x4e,0x08,0xeb,0xe1,
0x8a,0x4e,0x06,0x51,0xff,0x56,0x02,0x89,0xf7,0xbb,0x80,0xff,0xe8,0x40,0x05,0x59,
0x72,0x04,0x03,0x4e,0x08,0xc3,0x8a,0x4e,0x08,0xb5,0x00,0xc3,0xbb,0xf8,0x19,0xe8,
0x37,0x00,0xbb,0x08,0xff,0xe8,0x27,0x05,0x72,0xec,0x8b,0x4e,0x04,0x8b,0x56,0x00,
0x38,0x4e,0x06,0x76,0x17,0x1e,0x06,0x1f,0xff,0x56,0x02,0x1f,0xe8,0x0d,0x05,0x72,
0xd5,0x8b,0x4e,0x04,0x28,0x4e,0x06,0x00,0x4e,0x08,0xeb,0xe1,0x8a,0x4e,0x06,0x51,
0x1e,0x06,0x1f,0xff,0x56,0x02,0x1f,0xeb,0xb0,0x88,0x66,0x06,0x8b,0x45,0x04,0x89,
0x46,0x00,0x8a,0x45,0x15,0x30,0xe4,0x89,0x46,0x04,0x88,0x66,0x08,0x8a,0x45,0x14,
0x01,0xc3,0x3c,0x12,0x2e,0x8b,0x07,0x89,0x46,0x02,0x72,0x15,0xb0,0x04,0x8c,0xc2,
0x91,0xd3,0xc2,0x89,0xd1,0x91,0x24,0xf0,0x31,0xc2,0x01,0xc6,0x10,0xf2,0x8e,0xc2,
0xc3,0x89,0xf2,0x83,0xe6,0x0f,0x51,0xb1,0x04,0xd3,0xea,0x59,0x8c,0xc0,0x01,0xd0,
0x8e,0xc0,0xc3,0x90,0x75,0x1c,0x00,0x00,0xe8,0x1b,0x0c,0x1a,0x75,0x1c,0x1b,0x1b,
0xe8,0x1b,0x75,0x1c,0x75,0x1c,0xaa,0x18,0x63,0x1e,0x00,0x00,0x1a,0x1e,0xbe,0x1c,
0x7d,0x1d,0x7d,0x1d,0x1a,0x1e,0x63,0x1e,0x63,0x1e,0xa0,0x18,0xd1,0xe1,0xd1,0xe1,
0xd1,0xe1,0xb3,0x08,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,
0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,
0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,
0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,
0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,
0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,
0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,
0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,
0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,
0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,
0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,
0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,
0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,
0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,
0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,
0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,0xec,0x30,0xda,0xaa,
0xec,0x30,0xda,0xaa,0x49,0x74,0x03,0xe9,0xfa,0xfe,0xc3,0xd1,0xe1,0xd1,0xe1,0xd1,
0xe1,0xec,0x42,0xaa,0xec,0x4a,0xaa,0xec,0x42,0xaa,0xec,0x4a,0xaa,0xec,0x42,0xaa,
0xec,0x4a,0xaa,0xec,0x42,0xaa,0xec,0x4a,0xaa,0xec,0x42,0xaa,0xec,0x4a,0xaa,0xec,
0x42,0xaa,0xec,0x4a,0xaa,0xec,0x42,0xaa,0xec,0x4a,0xaa,0xec,0x42,0xaa,0xec,0x4a,
0xaa,0xec,0x42,0xaa,0xec,0x4a,0xaa,0xec,0x42,0xaa,0xec,0x4a,0xaa,0xec,0x42,0xaa,
0xec,0x4a,0xaa,0xec,0x42,0xaa,0xec,0x4a,0xaa,0xec,0x42,0xaa,0xec,0x4a,0xaa,0xec,
0x42,0xaa,0xec,0x4a,0xaa,0xec,0x42,0xaa,0xec,0x4a,0xaa,0xec,0x42,0xaa,0xec,0x4a,
0xaa,0xec,0x42,0xaa,0xec,0x4a,0xaa,0xec,0x42,0xaa,0xec,0x4a,0xaa,0xec,0x42,0xaa,
0xec,0x4a,0xaa,0xec,0x42,0xaa,0xec,0x4a,0xaa,0xec,0x42,0xaa,0xec,0x4a,0xaa,0xec,
0x42,0xaa,0xec,0x4a,0xaa,0xec,0x42,0xaa,0xec,0x4a,0xaa,0xec,0x42,0xaa,0xec,0x4a,
0xaa,0xec,0x42,0xaa,0xec,0x4a,0xaa,0xec,0x42,0xaa,0xec,0x4a,0xaa,0xec,0x42,0xaa,
0xec,0x4a,0xaa,0xec,0x42,0xaa,0xec,0x4a,0xaa,0xec,0x42,0xaa,0xec,0x4a,0xaa,0xec,
0x42,0xaa,0xec,0x4a,0xaa,0xec,0x42,0xaa,0xec,0x4a,0xaa,0xec,0x42,0xaa,0xec,0x4a,
0xaa,0x49,0x74,0x03,0xe9,0x3a,0xff,0xc3,0xd1,0xe1,0xd1,0xe1,0xd1,0xe1,0xec,0xaa,
0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,
0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,
0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,
0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,
0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,
0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,
0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,
0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0xec,0xaa,0x49,0x74,
0x03,0xe9,0x7a,0xff,0xc3,0xd1,0xe1,0xd1,0xe1,0xd1,0xe1,0xed,0xab,0xed,0xab,0xed,
0xab,0xed,0xab,0xed,0xab,0xed,0xab,0xed,0xab,0xed,0xab,0xed,0xab,0xed,0xab,0xed,
0xab,0xed,0xab,0xed,0xab,0xed,0xab,0xed,0xab,0xed,0xab,0xed,0xab,0xed,0xab,0xed,
0xab,0xed,0xab,0xed,0xab,0xed,0xab,0xed,0xab,0xed,0xab,0xed,0xab,0xed,0xab,0xed,
0xab,0xed,0xab,0xed,0xab,0xed,0xab,0xed,0xab,0xed,0xab,0xe2,0xbe,0xc3,0x88,0xcd,
0xb1,0x04,0xd3,0xe9,0xb3,0x08,0xad,0x30,0xda,0x86,0xc4,0xee,0x30,0xda,0x88,0xe0,
0xee,0xad,0x30,0xda,0x86,0xc4,0xee,0x30,0xda,0x88,0xe0,0xee,0xad,0x30,0xda,0x86,
0xc4,0xee,0x30,0xda,0x88,0xe0,0xee,0xad,0x30,0xda,0x86,0xc4,0xee,0x30,0xda,0x88,
0xe0,0xee,0xad,0x30,0xda,0x86,0xc4,0xee,0x30,0xda,0x88,0xe0,0xee,0xad,0x30,0xda,
0x86,0xc4,0xee,0x30,0xda,0x88,0xe0,0xee,0xad,0x30,0xda,0x86,0xc4,0xee,0x30,0xda,
0x88,0xe0,0xee,0xad,0x30,0xda,0x86,0xc4,0xee,0x30,0xda,0x88,0xe0,0xee,0xad,0x30,
0xda,0x86,0xc4,0xee,0x30,0xda,0x88,0xe0,0xee,0xad,0x30,0xda,0x86,0xc4,0xee,0x30,
0xda,0x88,0xe0,0xee,0xad,0x30,0xda,0x86,0xc4,0xee,0x30,0xda,0x88,0xe0,0xee,0xad,
0x30,0xda,0x86,0xc4,0xee,0x30,0xda,0x88,0xe0,0xee,0xad,0x30,0xda,0x86,0xc4,0xee,
0x30,0xda,0x88,0xe0,0xee,0xad,0x30,0xda,0x86,0xc4,0xee,0x30,0xda,0x88,0xe0,0xee,
0xad,0x30,0xda,0x86,0xc4,0xee,0x30,0xda,0x88,0xe0,0xee,0xad,0x30,0xda,0x86,0xc4,
0xee,0x30,0xda,0x88,0xe0,0xee,0x49,0x74,0x03,0xe9,0x4a,0xff,0xc3,0x88,0xcd,0xb1,
0x04,0xd3,0xe9,0xad,0x42,0x86,0xc4,0xee,0x4a,0x88,0xe0,0xee,0xad,0x42,0x86,0xc4,
0xee,0x4a,0x88,0xe0,0xee,0xad,0x42,0x86,0xc4,0xee,0x4a,0x88,0xe0,0xee,0xad,0x42,
0x86,0xc4,0xee,0x4a,0x88,0xe0,0xee,0xad,0x42,0x86,0xc4,0xee,0x4a,0x88,0xe0,0xee,
0xad,0x42,0x86,0xc4,0xee,0x4a,0x88,0xe0,0xee,0xad,0x42,0x86,0xc4,0xee,0x4a,0x88,
0xe0,0xee,0xad,0x42,0x86,0xc4,0xee,0x4a,0x88,0xe0,0xee,0xad,0x42,0x86,0xc4,0xee,
0x4a,0x88,0xe0,0xee,0xad,0x42,0x86,0xc4,0xee,0x4a,0x88,0xe0,0xee,0xad,0x42,0x86,
0xc4,0xee,0x4a,0x88,0xe0,0xee,0xad,0x42,0x86,0xc4,0xee,0x4a,0x88,0xe0,0xee,0xad,
0x42,0x86,0xc4,0xee,0x4a,0x88,0xe0,0xee,0xad,0x42,0x86,0xc4,0xee,0x4a,0x88,0xe0,
0xee,0xad,0x42,0x86,0xc4,0xee,0x4a,0x88,0xe0,0xee,0xad,0x42,0x86,0xc4,0xee,0x4a,
0x88,0xe0,0xee,0x49,0x74,0x03,0xe9,0x6a,0xff,0xc3,0x88,0xcd,0xb1,0x04,0xd3,0xe9,
0xac,0xee,0xac,0xee,0xac,0xee,0xac,0xee,0xac,0xee,0xac,0xee,0xac,0xee,0xac,0xee,
0xac,0xee,0xac,0xee,0xac,0xee,0xac,0xee,0xac,0xee,0xac,0xee,0xac,0xee,0xac,0xee,
0xac,0xee,0xac,0xee,0xac,0xee,0xac,0xee,0xac,0xee,0xac,0xee,0xac,0xee,0xac,0xee,
0xac,0xee,0xac,0xee,0xac,0xee,0xac,0xee,0xac,0xee,0xac,0xee,0xac,0xee,0xac,0xee,
0xe2,0xbe,0xc3,0x88,0xcd,0xb1,0x04,0xd3,0xe9,0xad,0xef,0xad,0xef,0xad,0xef,0xad,
0xef,0xad,0xef,0xad,0xef,0xad,0xef,0xad,0xef,0xad,0xef,0xad,0xef,0xad,0xef,0xad,
0xef,0xad,0xef,0xad,0xef,0xad,0xef,0xad,0xef,0xe2,0xde,0xc3,0xbb,0x08,0xff,0x88,
0xdc,0x88,0xf9,0xe8,0x14,0xf8,0xe8,0x87,0x00,0x80,0xe4,0x7f,0x74,0x19,0xe8,0x7f,
0x00,0x84,0xc0,0x78,0x04,0x84,0xe0,0x75,0x1a,0xe8,0x0a,0xf8,0x73,0xf0,0xe8,0x12,
0x00,0x72,0x03,0xb4,0x80,0xf9,0xc3,0xe8,0x66,0x00,0x84,0xc0,0x79,0x05,0xe8,0xf5,
0xf7,0x73,0xf4,0x88,0xc4,0xb2,0x01,0xe8,0x58,0x00,0x86,0xc4,0xa8,0xa5,0x75,0x02,
0x98,0xc3,0x78,0x0d,0xa8,0x01,0x75,0x0d,0xb4,0x11,0xa8,0x04,0x75,0x05,0xb4,0x20,
0xbb,0xb4,0x80,0xf9,0xc3,0xf9,0xbb,0xf1,0x1e,0x43,0xd0,0xdc,0x73,0xfb,0x2e,0x8a,
0x27,0xc3,0x02,0x40,0x01,0xb0,0x04,0xb1,0x10,0x0a,0xe0,0xb8,0x08,0x08,0x26,0x22,
0xa4,0xa6,0x00,0x26,0x22,0x84,0xa4,0x00,0xd0,0xe4,0x08,0xe0,0x08,0x45,0x01,0xe8,
0x06,0x00,0xc6,0x45,0x15,0x01,0xf8,0xc3,0x2e,0x8a,0x46,0x04,0x88,0x45,0x14,0xc3,
0xb2,0x07,0x30,0xf6,0x8a,0x45,0x14,0x3c,0x08,0x72,0x21,0x89,0xd3,0x3c,0x0a,0x76,
0x16,0xd1,0xe2,0x3c,0x14,0x72,0x15,0xb7,0x3e,0x74,0x04,0x88,0xd3,0xb7,0x1d,0x1e,
0x8e,0x5d,0x04,0x8a,0x07,0x1f,0xc3,0x2e,0x8a,0x97,0xba,0x1f,0x03,0x55,0x04,0xec,
0xc3,0x30,0xf6,0x8a,0x5d,0x14,0x80,0xfb,0x08,0x72,0x20,0x80,0xfb,0x0a,0x76,0x0f,
0x83,0xc2,0x08,0x80,0xfb,0x14,0x72,0x4b,0xbb,0x00,0x3e,0x74,0x34,0xeb,0x2e,0xe8,
0x2f,0xee,0x2e,0x03,0x57,0x02,0x80,0xf2,0x09,0xee,0xc3,0xe8,0x23,0xee,0x2e,0x03,
0x57,0x02,0xee,0xc3,0x30,0xf6,0x8a,0x5d,0x14,0x80,0xfb,0x08,0x72,0x27,0x80,0xfb,
0x0a,0x76,0x18,0x80,0xfb,0x14,0x72,0x1b,0xbb,0x00,0x3e,0x74,0x04,0xd1,0xe2,0xb7,
0x1d,0x01,0xd3,0x1e,0x8e,0x5d,0x04,0x88,0x07,0x1f,0xc3,0x89,0xd3,0x2e,0x8a,0x97,
0xba,0x1f,0xbb,0xd1,0xe2,0x03,0x55,0x04,0xee,0xc3,0x00,0x08,0x02,0x0a,0x04,0x0c,
0x06,0x0e,0xb4,0xa2,0x8a,0x46,0x06,0x3c,0x20,0x74,0x09,0xfe,0xc4,0x3c,0x30,0x74,
0x03,0x30,0xe4,0xc3,0x88,0x66,0x00,0xe8,0xf7,0xf9,0x8b,0x55,0x14,0x56,0x57,0x55,
0x88,0xf5,0x30,0xf6,0xd1,0xe2,0xd1,0xe2,0x8a,0x46,0x02,0x8a,0x66,0x00,0x50,0x50,
0x88,0xd3,0xb0,0x83,0x80,0xc2,0x03,0xee,0x88,0xe8,0x88,0xda,0xee,0x30,0xc0,0x42,
0x52,0xee,0xb0,0x47,0x42,0xee,0xb0,0x03,0x42,0xee,0xb0,0x0b,0x42,0xee,0x42,0x88,
0xd7,0x5a,0x30,0xc0,0xee,0x06,0x56,0x89,0xee,0x16,0x07,0xbf,0xff,0xff,0x89,0xfd,
0xb9,0x04,0x00,0xfa,0xe8,0x9d,0x00,0x5f,0x07,0x58,0x50,0xbe,0xff,0xff,0x89,0xf5,
0xb9,0x01,0x01,0x9e,0x73,0x21,0x87,0xf7,0xe8,0x89,0x00,0x41,0xeb,0x19,0x50,0xb4,
0x01,0xe8,0xaa,0x00,0x58,0xf6,0xc2,0x01,0x74,0x16,0xeb,0x1e,0xab,0x01,0xc5,0x83,
0xd5,0x00,0x01,0xee,0x83,0xd6,0x00,0x88,0xfa,0xec,0xd0,0xe8,0x88,0xda,0x73,0xde,
0xec,0x88,0xc4,0x88,0xfa,0xec,0xd0,0xe8,0x73,0xd4,0x88,0xda,0xec,0x86,0xc4,0xe2,
0xdb,0xfb,0x95,0x30,0xc4,0x89,0xf1,0x30,0xe9,0x88,0xc8,0x39,0xe8,0x75,0x0d,0x58,
0xfe,0xc8,0x50,0x74,0x19,0xfa,0xee,0xeb,0xa2,0x83,0xc4,0x08,0x31,0xc9,0x88,0xfa,
0xec,0x88,0xda,0xa8,0x8f,0x74,0x05,0xa8,0x01,0xec,0xe0,0xf2,0xb0,0x01,0x88,0xc4,
0x5b,0x59,0x30,0xed,0x28,0xd9,0x5d,0x5f,0x5e,0x9e,0xc3,0x26,0xad,0xee,0x01,0xc5,
0x83,0xd5,0x00,0x01,0xef,0x83,0xd7,0x00,0x88,0xfa,0xec,0xa8,0x20,0x74,0x20,0x88,
0xda,0x88,0xe0,0xee,0x88,0xfa,0xec,0xa8,0x20,0x88,0xda,0x74,0x1b,0xe2,0xdc,0x89,
0xf8,0x30,0xe0,0xee,0xe8,0x15,0x00,0x89,0xe8,0x30,0xe0,0xee,0x87,0xf7,0xc3,0x88,
0xe2,0xe8,0x08,0x00,0x88,0xd4,0xeb,0xd7,0xb8,0xcd,0x20,0x50,0xb4,0x20,0x51,0x52,
0x31,0xc9,0x88,0xfa,0xec,0x84,0xe0,0x75,0x13,0xe2,0xf7,0xfb,0xb1,0x14,0xe8,0xa9,
0xf5,0xe8,0xb2,0xf5,0x72,0x83,0xec,0x84,0xe0,0x74,0xf6,0xfa,0x5a,0x59,0xc3,0xf9,
0xc3,0x2e,0x8b,0x56,0x00,0x31,0xc0,0x56,0xe8,0x28,0xec,0x5e,0x73,0x06,0x38,0x06,
0x0c,0x00,0x74,0x03,0x8b,0x45,0x14,0xf6,0xc7,0x10,0x74,0x04,0x85,0xc0,0x74,0xdf,
0x85,0xd2,0x75,0x01,0x92,0xb9,0x01,0x00,0x85,0xd2,0x75,0x44,0xbf,0x77,0x21,0xb5,
0x01,0x47,0xb6,0x40,0x2e,0x8a,0x15,0xd1,0xe2,0xd1,0xe2,0x74,0x2a,0x52,0x80,0xc2,
0x03,0xb0,0x9a,0xee,0xec,0x5a,0x3c,0x9a,0x75,0xe7,0xb0,0x0c,0xee,0xec,0x3c,0x0c,
0x75,0xdf,0xb6,0x60,0x2e,0x8a,0x15,0xd0,0xee,0x74,0xd6,0x80,0xfe,0x06,0x75,0x02,
0xb6,0x04,0xe8,0x0b,0x00,0x72,0xf0,0xc3,0xb8,0xf8,0xbc,0xba,0xfa,0xbe,0xfe,0x00,
0x55,0x52,0x51,0xb3,0xa0,0x53,0x89,0xe5,0xe8,0x52,0xfe,0x5b,0x59,0x5a,0x5d,0xc3,
0x26,0x8b,0x84,0x3e,0x01,0x89,0x45,0x14,0x26,0x8a,0x84,0x3c,0x01,0xd0,0xe0,0x88,
0x45,0x01,0xc3,0xfb,0xfc,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x1e,0x06,0x83,
0xec,0x0c,0x89,0xe5,0xe8,0xfd,0xe9,0xe8,0xec,0xf0,0xe8,0x2a,0xeb,0x72,0x24,0x88,
0xe3,0x30,0xff,0xd1,0xe3,0x80,0xfc,0x25,0x77,0x05,0x2e,0xff,0xa7,0x8e,0x22,0xf6,
0x05,0x40,0x74,0x18,0x80,0xeb,0x82,0x72,0x13,0x80,0xfc,0x48,0x77,0x0e,0x2e,0xff,
0xa7,0xda,0x22,0x84,0xe4,0x74,0xd8,0x80,0xfc,0x08,0x74,0xd3,0x88,0x16,0x0d,0x00,
0xff,0x36,0x0e,0x00,0xe8,0xe9,0xf0,0x55,0xff,0x76,0x24,0x0e,0xbf,0x15,0x22,0x57,
0xff,0x36,0x02,0x00,0xff,0x36,0x00,0x00,0x8b,0x5e,0x18,0x8b,0x7e,0x10,0x8e,0x5e,
0x0e,0x8b,0x6e,0x14,0xcb,0x5d,0x8c,0x46,0x0c,0x8c,0x5e,0x0e,0x89,0x7e,0x10,0x89,
0x76,0x12,0x89,0x5e,0x18,0x88,0x76,0x1b,0x89,0x4e,0x1c,0x89,0x46,0x1e,0x9c,0x8f,
0x46,0x24,0xe8,0x7f,0xe9,0x8f,0x06,0x0e,0x00,0x3a,0x16,0x0d,0x00,0x74,0x16,0x88,
0x56,0x1a,0xeb,0x11,0x84,0xd2,0x78,0xa4,0x30,0xe4,0xeb,0x03,0x88,0x4e,0x1e,0x8a,
0x46,0x1a,0xe8,0x18,0x00,0x80,0x4e,0x25,0x02,0x8d,0x66,0x0c,0x07,0x1f,0x5f,0x5e,
0x5d,0x58,0x5b,0x5a,0x59,0x58,0xcf,0x9c,0xff,0x1e,0x00,0x00,0xc3,0xbb,0x74,0x04,
0x84,0xc0,0x78,0x02,0xb3,0x41,0x31,0xff,0x8e,0xdf,0x88,0x27,0x88,0x66,0x1f,0x84,
0xe4,0x75,0x05,0x80,0x66,0x24,0xfe,0xc3,0x80,0x4e,0x24,0x01,0xc3,0x90,0xea,0x22,
0x48,0x23,0x60,0x23,0x74,0x23,0x88,0x23,0x44,0x22,0xec,0x21,0xec,0x21,0xa9,0x23,
0x0a,0x24,0xec,0x21,0xec,0x21,0xea,0x24,0x0a,0x24,0xec,0x21,0xec,0x21,0xf8,0x24,
0xfe,0x24,0xec,0x21,0xec,0x21,0xf8,0x24,0x0b,0x25,0xec,0x21,0xec,0x21,0xec,0x21,
0xec,0x21,0xec,0x21,0xec,0x21,0xec,0x21,0xec,0x21,0x3f,0x25,0xec,0x21,0xec,0x21,
0xec,0x21,0xec,0x21,0x90,0x25,0xbe,0x25,0xf2,0x25,0x93,0x27,0xa6,0x26,0xc0,0x26,
0xd6,0x26,0xec,0x21,0xec,0x21,0xf0,0x26,0x04,0x27,0x88,0xd3,0x30,0xff,0x30,0xe4,
0xe8,0x74,0xff,0xe8,0x1b,0x00,0xe8,0xf2,0xe8,0x98,0x99,0x10,0xc2,0xe8,0x11,0x00,
0x88,0xc2,0xe8,0x0c,0x00,0x84,0xdb,0x79,0x03,0xe8,0x0c,0x00,0x88,0xfc,0xe9,0x3e,
0xff,0x38,0xda,0x75,0x02,0x88,0xe7,0xc3,0x30,0xdb,0x85,0xff,0x74,0x03,0x8a,0x5d,
0x02,0xb2,0x4e,0xb9,0x05,0x00,0xe8,0x21,0xea,0x72,0x17,0x51,0x52,0x53,0xe8,0x41,
0xf3,0x5b,0x5a,0xe8,0xda,0x00,0xe8,0xd8,0xff,0xe8,0x13,0xea,0x72,0x03,0xe8,0xcf,
0x00,0x59,0x80,0xc2,0x12,0xe2,0xdf,0xc3,0x31,0xc0,0x8e,0xd8,0x84,0xd2,0x78,0x06,
0x8a,0x26,0x41,0x04,0xeb,0x04,0x86,0x26,0x74,0x04,0xe8,0x1f,0xff,0xe9,0xf5,0xfe,
0xe8,0x1b,0x03,0xe8,0x11,0x03,0x2e,0x8a,0xa7,0x9a,0x26,0xbb,0x08,0xff,0xe8,0xb6,
0xf2,0xe9,0xd8,0xfe,0xe8,0x07,0x03,0xe8,0xfd,0x02,0x2e,0x8a,0xa7,0x9e,0x26,0xbb,
0x08,0xff,0xe8,0xa2,0xf2,0xe9,0xc4,0xfe,0xb4,0x40,0xe8,0xf7,0x02,0xbb,0x40,0xff,
0x50,0xe8,0x93,0xf2,0x59,0x73,0x03,0xe8,0x03,0x00,0xe9,0xaf,0xfe,0x91,0xe8,0x2a,
0xf4,0x2a,0x46,0x03,0x30,0xe4,0x91,0xf9,0xc3,0x85,0xff,0x74,0x2a,0xe8,0x2f,0x00,
0x0e,0x07,0xbf,0xfc,0x23,0xe8,0x41,0xe8,0xf6,0x46,0x1a,0x80,0x75,0x0c,0x88,0x5e,
0x18,0x8c,0x46,0x0c,0x89,0x7e,0x10,0xe8,0x67,0xe8,0x88,0xf4,0x89,0x4e,0x1c,0x87,
0x46,0x1a,0x30,0xe4,0xe9,0x7b,0xfe,0xe8,0x8d,0xfe,0x73,0xd9,0xe9,0x70,0xfe,0xe8,
0x92,0xe9,0xe8,0x6e,0xea,0x48,0x4b,0x91,0x86,0xcd,0xd0,0xc9,0xd0,0xc9,0x08,0xf9,
0x88,0xde,0x8a,0x5d,0x01,0x51,0xb1,0x05,0xd2,0xeb,0x59,0xc3,0xdf,0x02,0x25,0x02,
0x09,0x2a,0xff,0x50,0xf6,0x0f,0x08,0x27,0x80,0x01,0xe8,0x03,0x00,0xe9,0x3f,0xfe,
0x31,0xc0,0xf6,0x45,0x01,0x04,0x74,0x01,0xc3,0x06,0x56,0x52,0x53,0xe8,0x4a,0xe9,
0x88,0x46,0x01,0xe8,0x78,0xf2,0xb0,0x01,0xe8,0xb3,0x00,0x73,0x03,0xe9,0x93,0x00,
0xe8,0x54,0x01,0xe8,0x9e,0x00,0xb0,0x80,0xe8,0xa3,0x00,0xe8,0x8d,0x00,0xb0,0x80,
0xe8,0x9b,0x00,0x8b,0x45,0x0a,0x48,0x86,0xe0,0x88,0xc2,0xb0,0x91,0xbb,0x80,0xb5,
0xe8,0xf5,0xf1,0xb0,0x02,0xe8,0x86,0x00,0xe8,0x37,0xe9,0x2e,0x8a,0x1f,0x53,0x83,
0xe3,0x03,0x74,0x0d,0x2e,0x8b,0xb7,0xc8,0x24,0xe8,0x2e,0x01,0xb0,0x04,0xe8,0x6d,
0x00,0xe8,0x90,0x00,0xb0,0x08,0xe8,0x65,0x00,0x58,0xa8,0x10,0x74,0x12,0xb3,0x80,
0x88,0xd8,0xe8,0x3f,0x01,0x73,0x09,0xd0,0xeb,0x73,0xf5,0xb0,0x10,0xe8,0x4e,0x00,
0x8a,0x75,0x01,0xf6,0xc6,0x08,0x74,0x2b,0x2e,0x8a,0x16,0x4c,0x00,0x52,0xf6,0xc6,
0x10,0x74,0x03,0xe8,0x0a,0x01,0x5a,0x52,0xb0,0xe2,0xbb,0x80,0xb5,0xe8,0x98,0xf1,
0xb0,0x40,0xe8,0x29,0x00,0x5a,0xb0,0xe3,0xbb,0x80,0xb5,0xe8,0x8a,0xf1,0xb0,0x40,
0xe8,0x1b,0x00,0x5b,0x5a,0x5e,0x07,0xc3,0x00,0x82,0x02,0x80,0x7d,0x14,0x04,0x75,
0x0b,0xe9,0xce,0x00,0xe8,0xa4,0xe8,0x72,0x03,0xe9,0x8c,0x00,0x30,0xe4,0x73,0x09,
0x80,0xfc,0x01,0x76,0xf7,0x08,0x45,0x03,0xf9,0xc3,0xe8,0x03,0x00,0xe9,0x5f,0xfd,
0xb4,0x70,0xbb,0x40,0xff,0xe9,0x2f,0xf1,0xe8,0xa3,0xf1,0xe9,0x51,0xfd,0xe8,0x03,
0x00,0xe9,0x4b,0xfd,0xb9,0x01,0x00,0x30,0xf6,0xeb,0xe5,0xb1,0x01,0x84,0xd2,0x79,
0x0d,0xe8,0x1a,0x00,0x89,0x56,0x1c,0x87,0x46,0x1a,0x30,0xe4,0xb1,0x03,0xe8,0x4c,
0xfd,0x88,0x4e,0x1f,0xe9,0x2e,0xfd,0xb4,0x08,0xe8,0x3b,0xfd,0xeb,0x03,0xe8,0xae,
0xfe,0xe8,0xcd,0x00,0x88,0xf8,0x41,0x40,0xf6,0xe3,0xf7,0xe1,0x31,0xdb,0xc3,0xe8,
0x03,0x00,0xe9,0x0a,0xfd,0xe8,0x33,0xe8,0x72,0x17,0x25,0xff,0x00,0x74,0x11,0x48,
0x74,0x13,0x48,0x75,0x0c,0xe8,0x2f,0x00,0x88,0xc6,0x8a,0x55,0x15,0x89,0x56,0x1a,
0xc3,0xf9,0xb4,0x01,0xc3,0x8a,0x46,0x1b,0x3c,0x03,0x77,0xf5,0xd0,0xe0,0x04,0x0c,
0x88,0x45,0x14,0x3c,0x10,0x74,0x30,0x3c,0x12,0x75,0x27,0xb0,0x40,0x38,0x45,0x15,
0x76,0x20,0xe8,0x3f,0x00,0xeb,0x1b,0xb8,0xf4,0x00,0x02,0x45,0x14,0xd0,0xe8,0xc3,
0x96,0x8b,0x56,0x18,0xe8,0x03,0x00,0xe9,0xb5,0xfc,0xb0,0xef,0xbb,0x80,0xb5,0xe9,
0xa6,0xf0,0xbe,0x01,0x00,0xeb,0xf3,0xbe,0x81,0x00,0xe8,0xed,0xff,0x30,0xe4,0xc3,
0xbe,0x05,0x00,0x84,0xd2,0x75,0x03,0xb2,0xfe,0x3d,0xb2,0xbf,0xeb,0xdc,0xe8,0x03,
0x00,0xe9,0x8b,0xfc,0x3c,0x40,0x76,0x06,0x80,0x7d,0x14,0x12,0x74,0x93,0x53,0x50,
0x92,0xb0,0xc6,0xbb,0x40,0xb5,0xe8,0x6f,0xf0,0x5b,0x72,0x0a,0x43,0x7b,0x07,0x4b,
0x80,0x4d,0x01,0x02,0xeb,0x07,0x80,0x65,0x01,0xfd,0xb3,0x01,0xf9,0x88,0x5d,0x15,
0x5b,0xc3,0xb8,0x01,0xec,0xe8,0x86,0x00,0xbb,0x08,0xff,0xe8,0x29,0xf0,0xe9,0x4e,
0xfc,0xb3,0x3f,0x20,0xcb,0x28,0xd9,0xd0,0xc1,0xd0,0xc1,0x88,0xf7,0x86,0xcd,0xc3,
0x88,0xf8,0x30,0xe4,0xf6,0x75,0x0a,0x88,0xe7,0x30,0xe4,0x89,0xca,0xb1,0x03,0x22,
0x0d,0xd3,0xe2,0x01,0xd0,0x91,0xc3,0xe8,0xd7,0xff,0x8a,0x05,0x24,0x0c,0x74,0xf6,
0xa8,0x08,0x74,0xdc,0xb8,0x3f,0x00,0x91,0x99,0x8a,0x55,0x08,0xf7,0xe2,0x00,0xf8,
0x10,0xf4,0x10,0xf2,0x92,0xf6,0xe1,0x88,0xc7,0x92,0xf7,0xe1,0x00,0xfa,0x88,0xef,
0x4b,0x01,0xc3,0x10,0xd5,0x88,0xf9,0x88,0xf7,0xc3,0x8e,0x46,0x0e,0x26,0x80,0x3c,
0x10,0x72,0x2f,0x26,0x8a,0x44,0x02,0x84,0xc0,0x74,0x2c,0x78,0x25,0xe8,0x07,0x00,
0x8a,0x05,0xd0,0xe0,0x10,0xfb,0xc3,0x8a,0x5d,0x01,0x83,0xe3,0x02,0xc3,0x8b,0x76,
0x18,0x8e,0x46,0x0c,0x84,0xc0,0x7e,0x01,0xc3,0x3c,0x80,0x75,0x05,0x85,0xf6,0x75,
0x04,0xc3,0xb4,0x01,0x3d,0xb4,0x09,0xe9,0xb5,0xfb,0x20,0x24,0xc4,0x29,0x30,0x34,
0xc5,0x39,0x40,0x42,0x40,0x42,0xe8,0xb1,0xff,0x2e,0x8a,0xa7,0x9a,0x26,0xbb,0x08,
0xff,0xe8,0x3c,0xef,0x8e,0x5e,0x0e,0x8b,0x76,0x12,0x89,0x4c,0x02,0xe9,0x8f,0xfb,
0x80,0x7e,0x1e,0x02,0x73,0xcc,0xe8,0x91,0xff,0x2e,0x8a,0xa7,0x9e,0x26,0xbb,0x08,
0xff,0xe8,0x1c,0xef,0xeb,0xde,0xe8,0x81,0xff,0x26,0xff,0x74,0x02,0x2e,0x8a,0xa7,
0xa2,0x26,0xbb,0x40,0xff,0xe8,0x08,0xef,0x59,0x73,0x03,0xe8,0xaf,0xfc,0xeb,0xc4,
0x8e,0x46,0x0e,0x26,0x80,0x3c,0x10,0x72,0x99,0xb4,0x70,0xbb,0x40,0xff,0xe8,0xef,
0xee,0xe9,0x4b,0xfb,0x89,0xfe,0x1e,0x07,0x31,0xdb,0x4b,0xe8,0xa3,0x00,0xf6,0xc1,
0x04,0x74,0x03,0xe8,0xa9,0xe9,0x8b,0x7e,0x12,0x8e,0x5e,0x0e,0xb8,0x1a,0x00,0x8b,
0x0d,0x39,0xc1,0x73,0x03,0xe9,0x6a,0xff,0x89,0x05,0xb0,0x1e,0x39,0xc1,0x72,0x0f,
0x88,0x05,0x89,0x5d,0x1a,0x8c,0x45,0x1c,0x43,0x75,0x04,0x4b,0x89,0x5d,0x1c,0xc7,
0x45,0x02,0x01,0x00,0xe8,0x02,0xe3,0xe8,0x3e,0xe6,0xe8,0xfc,0xe2,0x89,0x45,0x10,
0x89,0x55,0x12,0x89,0x5d,0x14,0x31,0xc9,0x89,0x4d,0x16,0xc7,0x45,0x18,0x00,0x02,
0x2d,0x01,0x40,0x81,0xda,0xec,0x00,0x19,0xcb,0x73,0x04,0x80,0x4d,0x02,0x02,0x26,
0x8a,0x54,0x0a,0x30,0xf6,0x89,0x55,0x08,0x89,0x4d,0x0a,0x26,0x8a,0x54,0x0b,0x89,
0x55,0x0c,0x89,0x4d,0x0e,0x26,0x8b,0x54,0x0c,0x89,0x55,0x04,0x89,0x4d,0x06,0x91,
0xe9,0xbc,0xfa,0x81,0x7e,0x18,0xaa,0x55,0x75,0x14,0xc6,0x46,0x1f,0x21,0xf7,0x56,
0x18,0xe8,0x0d,0x00,0x89,0x4e,0x1c,0x80,0x66,0x24,0xfe,0xe9,0xa7,0xfa,0xe9,0x3b,
0xfa,0xb9,0x01,0x00,0x2e,0xf6,0x06,0x40,0x00,0x01,0x74,0x09,0x80,0x7d,0x14,0x04,
0x73,0x03,0x80,0xc9,0x04,0xc3,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xac,
};


// ==============================================================================
// src/hdc/hdc_option_rom_data.c end
// ==============================================================================

// ==============================================================================
// src/hdc/hdc.c start
// ==============================================================================

#line 1 "./src/hdc/hdc.c"
#ifndef YAX86_IMPLEMENTATION
#include "hdc_option_rom_data.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

#define Y_HDC_ERROR(...) \
  Y_LOG(hdc->config.logger, &YHDCLogModule, Y_LOG_LEVEL_ERROR, __VA_ARGS__)
#define Y_HDC_WARN(...) \
  Y_LOG(hdc->config.logger, &YHDCLogModule, Y_LOG_LEVEL_WARN, __VA_ARGS__)
#define Y_HDC_INFO(...) \
  Y_LOG(hdc->config.logger, &YHDCLogModule, Y_LOG_LEVEL_INFO, __VA_ARGS__)
#define Y_HDC_DEBUG(...) \
  Y_LOG(hdc->config.logger, &YHDCLogModule, Y_LOG_LEVEL_DEBUG, __VA_ARGS__)

#include <stddef.h>

// Status of a drive that is idle and has nothing to report.
enum {
  Y_HDC_STATUS_IDLE = Y_HDC_STATUS_READY | Y_HDC_STATUS_SEEK_COMPLETE,
};

// Word offsets into the Identify Device block. The block is 256 words, most
// of which are reserved or describe capabilities no drive of this era has, so
// only the ones that are filled in are named here.
typedef enum YHDCIdentifyWord {
  // General configuration. Bit 6 marks the device as a non-removable fixed
  // disk, which is the only kind this controller has.
  Y_HDC_IDENTIFY_WORD_GENERAL_CONFIGURATION = 0,
  Y_HDC_IDENTIFY_WORD_NUM_CYLINDERS = 1,
  Y_HDC_IDENTIFY_WORD_NUM_HEADS = 3,
  Y_HDC_IDENTIFY_WORD_BYTES_PER_TRACK = 4,
  Y_HDC_IDENTIFY_WORD_BYTES_PER_SECTOR = 5,
  Y_HDC_IDENTIFY_WORD_NUM_SECTORS_PER_TRACK = 6,
  Y_HDC_IDENTIFY_WORD_SERIAL_NUMBER = 10,
  Y_HDC_IDENTIFY_WORD_BUFFER_TYPE = 20,
  Y_HDC_IDENTIFY_WORD_BUFFER_SIZE = 21,
  Y_HDC_IDENTIFY_WORD_NUM_ECC_BYTES = 22,
  Y_HDC_IDENTIFY_WORD_FIRMWARE_REVISION = 23,
  Y_HDC_IDENTIFY_WORD_MODEL_NUMBER = 27,
  Y_HDC_IDENTIFY_WORD_MAX_SECTORS_PER_TRANSFER = 47,
  Y_HDC_IDENTIFY_WORD_CAPABILITIES = 49,
  Y_HDC_IDENTIFY_WORD_PIO_TIMING = 51,
  Y_HDC_IDENTIFY_WORD_FIELD_VALIDITY = 53,
  Y_HDC_IDENTIFY_WORD_CURRENT_NUM_CYLINDERS = 54,
  Y_HDC_IDENTIFY_WORD_CURRENT_NUM_HEADS = 55,
  Y_HDC_IDENTIFY_WORD_CURRENT_NUM_SECTORS_PER_TRACK = 56,
  Y_HDC_IDENTIFY_WORD_CURRENT_CAPACITY = 57,
  Y_HDC_IDENTIFY_WORD_NUM_USER_ADDRESSABLE_SECTORS = 60,
} YHDCIdentifyWord;

// Lengths in words of the string fields of the Identify Device block.
enum {
  Y_HDC_IDENTIFY_SERIAL_NUMBER_WORDS = 10,
  Y_HDC_IDENTIFY_FIRMWARE_REVISION_WORDS = 4,
  Y_HDC_IDENTIFY_MODEL_NUMBER_WORDS = 20,
};

enum {
  // Marks the device as a non-removable fixed disk.
  Y_HDC_IDENTIFY_GENERAL_CONFIGURATION_FIXED_DISK = 1 << 6,
  // The drive accepts logical block addresses as well as cylinder/head/sector
  // ones.
  Y_HDC_IDENTIFY_CAPABILITIES_LBA_SUPPORTED = 1 << 9,
  // Words 54 through 58 hold the current translated geometry.
  Y_HDC_IDENTIFY_FIELD_VALIDITY_CURRENT_GEOMETRY = 1 << 0,
  // PIO data transfer cycle timing mode, in the high byte.
  Y_HDC_IDENTIFY_PIO_TIMING_MODE2 = 2 << 8,
  // Sectors moved per Read Multiple or Write Multiple. Zero is how ATA says
  // multiple mode is not supported, which matches Set Multiple Mode aborting.
  Y_HDC_IDENTIFY_MAX_SECTORS_PER_TRANSFER = 0,
  // A dual ported buffer with look-ahead, which is what an ATA drive of this
  // era reports.
  Y_HDC_IDENTIFY_BUFFER_TYPE_DUAL_PORTED = 3,
  // Buffer size in 512-byte increments.
  Y_HDC_IDENTIFY_BUFFER_SIZE = 1,
  // Bytes reserved for ECC on a long read or write.
  Y_HDC_IDENTIFY_NUM_ECC_BYTES = 4,
};

Y_FILE_PRIVATE const char* const YHDCModelNumber = "YAX86 HARD DISK";
Y_FILE_PRIVATE const char* const YHDCSerialNumber = "YAX86-0000001";
Y_FILE_PRIVATE const char* const YHDCFirmwareRevision = "1.0";

// Returns the drive the drive/head register currently selects.
Y_FILE_PRIVATE YHDCDriveState* YHDCSelectedDrive(YHDCState* hdc) {
  return &hdc->drives
              [(hdc->drive_head & Y_HDC_DRIVE_HEAD_DRIVE_SELECT) ? 1 : 0];
}

// Completes a command successfully.
Y_FILE_PRIVATE void YHDCFinishCommand(YHDCState* hdc) {
  hdc->error = 0;
  hdc->status = Y_HDC_STATUS_IDLE;
}

// Fails a command, leaving the given bits in the error register.
Y_FILE_PRIVATE void YHDCFailCommand(YHDCState* hdc, uint8_t error) {
  hdc->error = error;
  hdc->status = Y_HDC_STATUS_IDLE | Y_HDC_STATUS_ERROR;
  hdc->transfer = Y_HDC_TRANSFER_NONE;
  hdc->transfer_byte_index = 0;
}

// Total number of sectors on a drive.
Y_FILE_PRIVATE uint32_t YHDCDriveNumSectors(const YHDCDriveState* drive) {
  return (uint32_t)drive->geometry.num_cylinders *
         (uint32_t)drive->geometry.num_heads *
         (uint32_t)drive->geometry.num_sectors_per_track;
}

// Translates the address in the task file to a sector number, and returns
// whether it addresses a sector that exists on the drive.
//
// The address is an LBA when the drive/head register says so, and a
// cylinder/head/sector triple otherwise. A CHS triple is translated against
// the geometry the guest asked for with Initialize Device Parameters rather
// than the physical geometry, which is what lets a guest address the drive
// with a geometry of its own choosing.
Y_FILE_PRIVATE bool YHDCComputeSectorNumber(
    YHDCState* hdc, const YHDCDriveState* drive, uint32_t* sector_number) {
  uint32_t lba;
  if (hdc->drive_head & Y_HDC_DRIVE_HEAD_LBA) {
    lba = ((uint32_t)(hdc->drive_head & Y_HDC_DRIVE_HEAD_HEAD_MASK) << 24) |
          ((uint32_t)hdc->cylinder_high << 16) |
          ((uint32_t)hdc->cylinder_low << 8) | (uint32_t)hdc->sector_number;
  } else {
    const uint32_t cylinder =
        ((uint32_t)hdc->cylinder_high << 8) | (uint32_t)hdc->cylinder_low;
    const uint32_t head = hdc->drive_head & Y_HDC_DRIVE_HEAD_HEAD_MASK;
    const uint32_t sector = hdc->sector_number;
    // Sectors are numbered from one, and sector zero does not exist.
    if (sector == 0 || head >= drive->num_translated_heads ||
        sector > drive->num_translated_sectors_per_track) {
      return false;
    }
    lba = (cylinder * (uint32_t)drive->num_translated_heads + head) *
              (uint32_t)drive->num_translated_sectors_per_track +
          (sector - 1);
  }

  if (lba >= YHDCDriveNumSectors(drive)) {
    return false;
  }
  *sector_number = lba;
  return true;
}

// Writes a 16-bit word into the Identify Device block, which is little endian.
Y_FILE_PRIVATE void YHDCWriteIdentifyWord(
    YHDCState* hdc, uint16_t word_index, uint16_t value) {
  hdc->sector_buffer[word_index * 2] = (uint8_t)(value & 0xFF);
  hdc->sector_buffer[word_index * 2 + 1] = (uint8_t)(value >> 8);
}

// Writes a space padded string into the Identify Device block. ATA strings
// are byte swapped within each word, so the first character of a pair lands in
// the high byte.
Y_FILE_PRIVATE void YHDCWriteIdentifyString(
    YHDCState* hdc, uint16_t word_index, const char* text, uint16_t num_words) {
  const char* next = text;
  for (uint16_t i = 0; i < num_words; ++i) {
    const char first = *next ? *next++ : ' ';
    const char second = *next ? *next++ : ' ';
    YHDCWriteIdentifyWord(
        hdc, (uint16_t)(word_index + i),
        (uint16_t)(((uint16_t)(uint8_t)first << 8) | (uint8_t)second));
  }
}

// Fills the sector buffer with the drive's Identify Device block.
Y_FILE_PRIVATE void YHDCBuildIdentifyBlock(
    YHDCState* hdc, const YHDCDriveState* drive) {
  for (uint16_t i = 0; i < Y_HDC_SECTOR_SIZE; ++i) {
    hdc->sector_buffer[i] = 0;
  }

  const uint32_t num_sectors = YHDCDriveNumSectors(drive);

  // Cylinder count in the geometry the guest asked for, which is however many
  // whole cylinders of that shape fit on the drive. Reporting the physical
  // count instead would describe a drive larger than this one whenever the
  // guest picks a different head or sector count, and addresses in the part
  // that does not exist would come back as ID not found. Deriving it means
  // every address the reported geometry allows is one the drive really has:
  // the largest is num_translated_cylinders * heads * sectors - 1, which is at
  // most num_sectors - 1 by construction.
  const uint32_t num_sectors_per_cylinder =
      (uint32_t)drive->num_translated_heads *
      (uint32_t)drive->num_translated_sectors_per_track;
  const uint16_t num_translated_cylinders =
      (uint16_t)(num_sectors / num_sectors_per_cylinder);
  // Capacity in that same geometry, which ATA defines as the product of the
  // three current geometry words. It is at most the physical capacity, and
  // less when the head and sector counts leave a partial cylinder over.
  const uint32_t num_translated_sectors =
      (uint32_t)num_translated_cylinders * num_sectors_per_cylinder;

  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_GENERAL_CONFIGURATION,
      Y_HDC_IDENTIFY_GENERAL_CONFIGURATION_FIXED_DISK);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_NUM_CYLINDERS, drive->geometry.num_cylinders);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_NUM_HEADS, drive->geometry.num_heads);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_BYTES_PER_TRACK,
      (uint16_t)(Y_HDC_SECTOR_SIZE * drive->geometry.num_sectors_per_track));
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_BYTES_PER_SECTOR, Y_HDC_SECTOR_SIZE);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_NUM_SECTORS_PER_TRACK,
      drive->geometry.num_sectors_per_track);
  YHDCWriteIdentifyString(
      hdc, Y_HDC_IDENTIFY_WORD_SERIAL_NUMBER, YHDCSerialNumber,
      Y_HDC_IDENTIFY_SERIAL_NUMBER_WORDS);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_BUFFER_TYPE,
      Y_HDC_IDENTIFY_BUFFER_TYPE_DUAL_PORTED);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_BUFFER_SIZE, Y_HDC_IDENTIFY_BUFFER_SIZE);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_NUM_ECC_BYTES, Y_HDC_IDENTIFY_NUM_ECC_BYTES);
  YHDCWriteIdentifyString(
      hdc, Y_HDC_IDENTIFY_WORD_FIRMWARE_REVISION, YHDCFirmwareRevision,
      Y_HDC_IDENTIFY_FIRMWARE_REVISION_WORDS);
  YHDCWriteIdentifyString(
      hdc, Y_HDC_IDENTIFY_WORD_MODEL_NUMBER, YHDCModelNumber,
      Y_HDC_IDENTIFY_MODEL_NUMBER_WORDS);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_MAX_SECTORS_PER_TRANSFER,
      Y_HDC_IDENTIFY_MAX_SECTORS_PER_TRANSFER);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_CAPABILITIES,
      Y_HDC_IDENTIFY_CAPABILITIES_LBA_SUPPORTED);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_PIO_TIMING, Y_HDC_IDENTIFY_PIO_TIMING_MODE2);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_FIELD_VALIDITY,
      Y_HDC_IDENTIFY_FIELD_VALIDITY_CURRENT_GEOMETRY);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_CURRENT_NUM_CYLINDERS, num_translated_cylinders);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_CURRENT_NUM_HEADS, drive->num_translated_heads);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_CURRENT_NUM_SECTORS_PER_TRACK,
      drive->num_translated_sectors_per_track);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_CURRENT_CAPACITY,
      (uint16_t)(num_translated_sectors & 0xFFFF));
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_CURRENT_CAPACITY + 1,
      (uint16_t)(num_translated_sectors >> 16));
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_NUM_USER_ADDRESSABLE_SECTORS,
      (uint16_t)(num_sectors & 0xFFFF));
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_NUM_USER_ADDRESSABLE_SECTORS + 1,
      (uint16_t)(num_sectors >> 16));
}

// Opens a transfer through the data register.
Y_FILE_PRIVATE void YHDCStartTransfer(
    YHDCState* hdc, YHDCTransfer transfer, uint32_t offset,
    uint16_t num_sectors) {
  hdc->transfer = transfer;
  hdc->transfer_byte_index = 0;
  hdc->transfer_offset = offset;
  hdc->transfer_drive =
      (hdc->drive_head & Y_HDC_DRIVE_HEAD_DRIVE_SELECT) ? 1 : 0;
  hdc->transfer_sectors_remaining = num_sectors;
  hdc->error = 0;
  hdc->status = Y_HDC_STATUS_IDLE | Y_HDC_STATUS_DATA_REQUEST;
}

// Advances past the byte just transferred, moving on to the next sector or
// ending the transfer as needed.
Y_FILE_PRIVATE void YHDCAdvanceTransfer(YHDCState* hdc) {
  ++hdc->transfer_byte_index;
  if (hdc->transfer_byte_index < Y_HDC_SECTOR_SIZE) {
    return;
  }
  hdc->transfer_byte_index = 0;

  if (hdc->transfer == Y_HDC_TRANSFER_IDENTIFY) {
    // A single block, with no sector count behind it.
    hdc->transfer = Y_HDC_TRANSFER_NONE;
    hdc->status = Y_HDC_STATUS_IDLE;
    return;
  }

  hdc->transfer_offset += Y_HDC_SECTOR_SIZE;
  --hdc->transfer_sectors_remaining;
  // The guest reads the sector count register back to see how much of the
  // transfer got through, so it counts down as sectors complete.
  hdc->sector_count = (uint8_t)hdc->transfer_sectors_remaining;
  if (hdc->transfer_sectors_remaining == 0) {
    hdc->transfer = Y_HDC_TRANSFER_NONE;
    hdc->status = Y_HDC_STATUS_IDLE;
  }
}

Y_FILE_PRIVATE void YHDCHandleIdentifyDevice(
    YHDCState* hdc, YHDCDriveState* drive) {
  YHDCBuildIdentifyBlock(hdc, drive);
  YHDCStartTransfer(hdc, Y_HDC_TRANSFER_IDENTIFY, 0, 1);
}

// Works out the run of sectors a command addresses, and returns whether all of
// it lies on the drive. The whole run has to fit, not just its first sector: a
// byte callback has no way to refuse an address, so a command that would walk
// off the end has to fail before it moves anything rather than part way
// through.
Y_FILE_PRIVATE bool YHDCComputeSectorRange(
    YHDCState* hdc, const YHDCDriveState* drive, uint32_t* sector_number,
    uint16_t* num_sectors) {
  if (!YHDCComputeSectorNumber(hdc, drive, sector_number)) {
    return false;
  }
  // A sector count of zero asks for the full 256 sectors.
  *num_sectors = hdc->sector_count == 0 ? 256 : (uint16_t)hdc->sector_count;
  return *sector_number + *num_sectors <= YHDCDriveNumSectors(drive);
}

// Sets up a read or write of one or more sectors starting at the address in
// the task file. A sector count of zero means 256 sectors, which is how ATA
// encodes the largest transfer a single command can make.
Y_FILE_PRIVATE void YHDCHandleReadWriteSectors(
    YHDCState* hdc, YHDCDriveState* drive, YHDCTransfer transfer) {
  uint32_t sector_number = 0;
  uint16_t num_sectors = 0;
  if (!YHDCComputeSectorRange(hdc, drive, &sector_number, &num_sectors)) {
    YHDCFailCommand(hdc, Y_HDC_ERROR_ID_NOT_FOUND);
    return;
  }
  YHDCStartTransfer(
      hdc, transfer, sector_number * Y_HDC_SECTOR_SIZE, num_sectors);
}

Y_FILE_PRIVATE void YHDCHandleInitializeDeviceParameters(
    YHDCState* hdc, YHDCDriveState* drive) {
  const uint8_t num_heads = (hdc->drive_head & Y_HDC_DRIVE_HEAD_HEAD_MASK) + 1;
  const uint8_t num_sectors_per_track = hdc->sector_count;
  if (num_sectors_per_track == 0) {
    YHDCFailCommand(hdc, Y_HDC_ERROR_ABORTED);
    return;
  }
  drive->num_translated_heads = num_heads;
  drive->num_translated_sectors_per_track = num_sectors_per_track;
  Y_HDC_DEBUG(
      "translated_geometry_set",
      Y_LOG_UINT(
          "drive", (hdc->drive_head & Y_HDC_DRIVE_HEAD_DRIVE_SELECT) ? 1 : 0),
      Y_LOG_UINT("heads", num_heads),
      Y_LOG_UINT("sectors_per_track", num_sectors_per_track));
  YHDCFinishCommand(hdc);
}

// Checks that the addressed sectors exist without transferring them. The
// sector count counts here just as it does for a real transfer, so a verify
// that runs off the end of the drive fails rather than reporting success.
Y_FILE_PRIVATE void YHDCHandleReadVerifySectors(
    YHDCState* hdc, YHDCDriveState* drive) {
  uint32_t sector_number = 0;
  uint16_t num_sectors = 0;
  if (!YHDCComputeSectorRange(hdc, drive, &sector_number, &num_sectors)) {
    YHDCFailCommand(hdc, Y_HDC_ERROR_ID_NOT_FOUND);
    return;
  }
  YHDCFinishCommand(hdc);
}

Y_FILE_PRIVATE void YHDCExecuteCommand(YHDCState* hdc, uint8_t opcode) {
  YHDCDriveState* drive = YHDCSelectedDrive(hdc);
  if (!drive->present) {
    // An absent drive never answers, so the command simply goes unheard. The
    // status register is shared by both drives, and YHDCReadPort already
    // reports an absent one as never ready, so writing to it here would do
    // nothing except strand the other drive at a status of zero.
    return;
  }

  Y_HDC_DEBUG(
      "command",
      Y_LOG_UINT(
          "drive", (hdc->drive_head & Y_HDC_DRIVE_HEAD_DRIVE_SELECT) ? 1 : 0),
      Y_LOG_HEX8("command", opcode),
      Y_LOG_UINT(
          "cylinder", ((uint16_t)hdc->cylinder_high << 8) | hdc->cylinder_low),
      Y_LOG_UINT("head", hdc->drive_head & Y_HDC_DRIVE_HEAD_HEAD_MASK),
      Y_LOG_UINT("sector", hdc->sector_number),
      Y_LOG_UINT("count", hdc->sector_count));

  // The low nibble of recalibrate and seek is a stepping rate, so the whole
  // range is one command.
  const uint8_t command = ((opcode & 0xF0) == Y_HDC_COMMAND_RECALIBRATE ||
                           (opcode & 0xF0) == Y_HDC_COMMAND_SEEK)
                              ? (uint8_t)(opcode & 0xF0)
                              : opcode;

  switch (command) {
    case Y_HDC_COMMAND_RECALIBRATE:
    case Y_HDC_COMMAND_SEEK:
      // Seek time is not modelled, so the heads are already where they need
      // to be.
      YHDCFinishCommand(hdc);
      break;

    case Y_HDC_COMMAND_READ_VERIFY_SECTORS:
    case Y_HDC_COMMAND_READ_VERIFY_SECTORS_NO_RETRY:
      YHDCHandleReadVerifySectors(hdc, drive);
      break;

    case Y_HDC_COMMAND_READ_SECTORS:
    case Y_HDC_COMMAND_READ_SECTORS_NO_RETRY:
      YHDCHandleReadWriteSectors(hdc, drive, Y_HDC_TRANSFER_READ);
      break;

    case Y_HDC_COMMAND_WRITE_SECTORS:
    case Y_HDC_COMMAND_WRITE_SECTORS_NO_RETRY:
      YHDCHandleReadWriteSectors(hdc, drive, Y_HDC_TRANSFER_WRITE);
      break;

    case Y_HDC_COMMAND_INITIALIZE_DEVICE_PARAMETERS:
      YHDCHandleInitializeDeviceParameters(hdc, drive);
      break;

    case Y_HDC_COMMAND_IDENTIFY_DEVICE:
      YHDCHandleIdentifyDevice(hdc, drive);
      break;

    default:
      Y_HDC_WARN("unsupported_command", Y_LOG_HEX8("command", opcode));
      YHDCFailCommand(hdc, Y_HDC_ERROR_ABORTED);
      break;
  }
}

// Returns the byte at the current transfer position, without advancing.
Y_FILE_PRIVATE uint8_t YHDCReadTransferByte(YHDCState* hdc) {
  switch (hdc->transfer) {
    case Y_HDC_TRANSFER_IDENTIFY:
      return hdc->sector_buffer[hdc->transfer_byte_index];
    case Y_HDC_TRANSFER_READ:
      if (hdc->config.read_image_byte) {
        return hdc->config.read_image_byte(
            hdc->config.context, hdc->transfer_drive,
            hdc->transfer_offset + hdc->transfer_byte_index);
      }
      return 0;
    default:
      return 0;
  }
}

// Hands the guest the low byte of the next word, latching the high byte.
//
// The drive's data port is 16 bits wide and the card is on an 8-bit bus, so a
// read of the low byte port is what runs the bus cycle: the whole word comes
// off the drive at once, the low half goes to the guest, and the card holds
// the high half in its latch for the guest to collect from the other port.
// Reading the two ports is therefore not the same thing, and the card consumes
// a word per low byte read rather than a byte per read of either port.
Y_FILE_PRIVATE uint8_t YHDCReadDataRegister(YHDCState* hdc) {
  if (!(hdc->status & Y_HDC_STATUS_DATA_REQUEST)) {
    Y_HDC_WARN("data_read_without_transfer");
    return 0;
  }
  if (hdc->transfer == Y_HDC_TRANSFER_WRITE) {
    Y_HDC_WARN("data_read_during_write");
    return 0;
  }

  // Sectors are a whole number of words, so a word never straddles a sector
  // boundary and both halves come from the same sector.
  const uint8_t low_byte = YHDCReadTransferByte(hdc);
  YHDCAdvanceTransfer(hdc);
  hdc->data_high_latch = YHDCReadTransferByte(hdc);
  YHDCAdvanceTransfer(hdc);
  return low_byte;
}

// Hands back the high byte of the word the last low byte read fetched.
//
// This port is wired to the card's latch and runs no bus cycle, so reading it
// twice returns the same value and reading it without having read the low byte
// port first returns whatever the previous word left behind. Nothing here
// touches the transfer, which is what keeps a guest that reads the ports out
// of order from sliding the transfer out of step.
Y_FILE_PRIVATE uint8_t YHDCReadDataHighRegister(YHDCState* hdc) {
  return hdc->data_high_latch;
}

// Writes one byte of the drive's image.
Y_FILE_PRIVATE void YHDCWriteImageByte(YHDCState* hdc, uint8_t value) {
  if (hdc->config.write_image_byte) {
    hdc->config.write_image_byte(
        hdc->config.context, hdc->transfer_drive,
        hdc->transfer_offset + hdc->transfer_byte_index, value);
  }
  YHDCAdvanceTransfer(hdc);
}

// Takes the next byte of a write from the guest.
//
// Writes are not the mirror image of reads. A read of the low byte port is
// what runs the bus cycle, so the drive hands over the low byte and the card
// latches the high one for the guest to collect afterwards - low byte first.
// A write is the other way round: the card cannot start the cycle until it has
// the whole word, so the guest writes the high byte to the latch first and the
// write of the low byte is what commits the pair. Streaming bytes in the order
// they arrive would put every word on the disk back to front.
Y_FILE_PRIVATE void YHDCWriteDataRegister(
    YHDCState* hdc, bool is_high_byte, uint8_t value) {
  if (!(hdc->status & Y_HDC_STATUS_DATA_REQUEST) ||
      hdc->transfer != Y_HDC_TRANSFER_WRITE) {
    Y_HDC_WARN("data_write_without_write");
    return;
  }

  if (is_high_byte) {
    hdc->data_high_latch = value;
    return;
  }

  // A word never straddles a sector boundary, so the two halves always land in
  // the same sector.
  YHDCWriteImageByte(hdc, value);
  YHDCWriteImageByte(hdc, hdc->data_high_latch);
}

// What the status and alternate status registers read back as. An empty drive
// slot leaves the bus undriven, which the guest reads as a drive that is never
// ready.
Y_FILE_PRIVATE uint8_t YHDCReadStatusRegister(YHDCState* hdc) {
  return YHDCSelectedDrive(hdc)->present ? hdc->status : 0;
}

// Handles a write to the device control register.
Y_FILE_PRIVATE void YHDCWriteDeviceControlRegister(
    YHDCState* hdc, uint8_t value) {
  // A software reset abandons whatever the drive was doing, which is the point
  // of it: it is how a guest recovers a transfer it cannot finish. Only the
  // assertion matters, not the release - the reset completes inside this port
  // write like every other command here, so there is no window in which the
  // guest could catch the drive busy. The device signature and diagnostic code
  // a reset leaves in the task file are not modelled, since nothing reads
  // them without an ATAPI device to tell apart.
  if (value & Y_HDC_DEVICE_CONTROL_SOFTWARE_RESET) {
    hdc->transfer = Y_HDC_TRANSFER_NONE;
    hdc->transfer_byte_index = 0;
    hdc->transfer_sectors_remaining = 0;
    hdc->data_high_latch = 0;
    hdc->error = 0;
    hdc->status = Y_HDC_STATUS_IDLE;
  }
}

Y_PUBLIC uint8_t YHDCReadPort(YHDCState* hdc, uint16_t port) {
  const uint8_t offset =
      (uint8_t)((port - Y_HDC_PORT_BASE) & (Y_HDC_NUM_PORTS - 1));
  switch (YHDCPortOffsetToRegister(offset)) {
    case Y_HDC_REGISTER_DATA:
      return YHDCReadDataRegister(hdc);
    case Y_HDC_REGISTER_DATA_HIGH:
      return YHDCReadDataHighRegister(hdc);
    case Y_HDC_REGISTER_ERROR:
      return hdc->error;
    case Y_HDC_REGISTER_SECTOR_COUNT:
      return hdc->sector_count;
    case Y_HDC_REGISTER_SECTOR_NUMBER:
      return hdc->sector_number;
    case Y_HDC_REGISTER_CYLINDER_LOW:
      return hdc->cylinder_low;
    case Y_HDC_REGISTER_CYLINDER_HIGH:
      return hdc->cylinder_high;
    case Y_HDC_REGISTER_DRIVE_HEAD:
      return hdc->drive_head;
    case Y_HDC_REGISTER_STATUS:
    case Y_HDC_REGISTER_DEVICE_CONTROL:
      return YHDCReadStatusRegister(hdc);
    default:
      return Y_HDC_OPEN_BUS_VALUE;
  }
}

Y_PUBLIC void YHDCWritePort(YHDCState* hdc, uint16_t port, uint8_t value) {
  const uint8_t offset =
      (uint8_t)((port - Y_HDC_PORT_BASE) & (Y_HDC_NUM_PORTS - 1));
  switch (YHDCPortOffsetToRegister(offset)) {
    case Y_HDC_REGISTER_DATA:
      YHDCWriteDataRegister(hdc, /*is_high_byte=*/false, value);
      break;
    case Y_HDC_REGISTER_DATA_HIGH:
      YHDCWriteDataRegister(hdc, /*is_high_byte=*/true, value);
      break;
    case Y_HDC_REGISTER_ERROR:
      hdc->features = value;
      break;
    case Y_HDC_REGISTER_SECTOR_COUNT:
      hdc->sector_count = value;
      break;
    case Y_HDC_REGISTER_SECTOR_NUMBER:
      hdc->sector_number = value;
      break;
    case Y_HDC_REGISTER_CYLINDER_LOW:
      hdc->cylinder_low = value;
      break;
    case Y_HDC_REGISTER_CYLINDER_HIGH:
      hdc->cylinder_high = value;
      break;
    case Y_HDC_REGISTER_DRIVE_HEAD:
      hdc->drive_head = value;
      break;
    case Y_HDC_REGISTER_STATUS:
      YHDCExecuteCommand(hdc, value);
      break;
    case Y_HDC_REGISTER_DEVICE_CONTROL:
      YHDCWriteDeviceControlRegister(hdc, value);
      break;
    default:
      break;
  }
}

Y_PUBLIC void YHDCAttachDrive(
    YHDCState* hdc, uint8_t drive, const YHDCDriveGeometry* geometry) {
  if (drive >= Y_HDC_NUM_DRIVES) {
    return;
  }
  YHDCDriveState* drive_state = &hdc->drives[drive];
  drive_state->present = true;
  drive_state->geometry = *geometry;
  // Until the guest says otherwise, addresses are translated against the
  // drive's own geometry.
  drive_state->num_translated_heads = geometry->num_heads;
  drive_state->num_translated_sectors_per_track =
      geometry->num_sectors_per_track;
}

Y_PUBLIC void YHDCDetachDrive(YHDCState* hdc, uint8_t drive) {
  if (drive >= Y_HDC_NUM_DRIVES) {
    return;
  }
  static const YHDCDriveState empty_drive_state = {0};
  hdc->drives[drive] = empty_drive_state;
}

Y_PUBLIC void YHDCInit(YHDCState* hdc) { hdc->status = Y_HDC_STATUS_IDLE; }

Y_PUBLIC uint32_t YHDCGetOptionROMSize(void) {
  return Y_HDC_OPTION_ROM_DATA_SIZE;
}

Y_PUBLIC const uint8_t* YHDCGetOptionROMData(void) { return YHDCOptionROMData; }


// ==============================================================================
// src/hdc/hdc.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_HDC_BUNDLE_H

// ==============================================================================
// YAX86 KEYBOARD MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_KEYBOARD_BUNDLE_H
#define YAX86_KEYBOARD_BUNDLE_H

#ifndef YAX86_UTIL_BUNDLE_H
#include "util.h"
#endif  // YAX86_UTIL_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/keyboard/public.h start
// ==============================================================================

#line 1 "./src/keyboard/public.h"
// Public interface for the Keyboard module.
#ifndef YAX86_KEYBOARD_PUBLIC_H
#define YAX86_KEYBOARD_PUBLIC_H

// This module emulates a PC/XT keyboard and its interface to the 8255 PPI.
//
// During initialization:
// 1. [0, 0]
//    The BIOS sets both control bits to false and holds them there for at
//    least 20ms. The keyboard detects the clock_low line is held low, and
//    performs a self test. Key presses or releases that arrive while the
//    keyboard is held in reset are dropped rather than buffered.
// 2. -> [1, 1] -> [0, 1]
//    The BIOS restores the clock_low line to true, releasing the reset signal.
//    It pulses the enable_clear line high then low to trigger the next scan
//    code, just like in normal operation.
// 3. The pulse triggers the keyboard to send the self-test OK scancode (0xAA)
//    to the PPI.
// 4. -> [1, 1] -> [0, 1]
//    The BIOS acknowledges the self-test OK scancode by pulsing the
//    enable_clear line again, just like in normal operation.
// 5. -> [1, 1]
//    The BIOS sets both control bits to true to inhibit the keyboard for the
//    rest of the POST process.
// 6. -> [0, 1]
//    At the end of POST, the BIOS enables the keyboard by setting it to normal
//    operational state.
//
// In normal operation:
// 1. [0, 1]
//    In steady state, the control bits are set to enable_clear = false,
//    clock_low = true.
// 2. [0, 1]
//    On key press, the keyboard sends the scancode to the PPI and raises IRQ1.
//    At this point, the control bits are unchanged.
// 3. -> [1, 1] -> [0, 1]
//    The BIOS's IRQ handler sends an ack by briefly pulsing the enable_clear
//    line from false to true to false. This pulse tells the keyboard that it
//    can now send the next scancode.

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_KEYBOARD_BUNDLE_H
#include "../util/log.h"
#include "../util/static_vector.h"
#endif  // YAX86_KEYBOARD_BUNDLE_H

enum {
  // Log module ID for the Keyboard.
  Y_KEYBOARD_LOG_MODULE_ID = 7,
};

// Log module for the Keyboard.
Y_PUBLIC_HEADER const YLogModule YKeyboardLogModule = {
    .id = Y_KEYBOARD_LOG_MODULE_ID,
    .name = "keyboard",
};

struct YKeyboardState;

// Caller-provided runtime configuration for the Keyboard.
typedef struct YKeyboardConfig {
  // Opaque context pointer, passed to all callbacks.
  void* context;

  // Logger for this module. May be NULL.
  YLogger* logger;

  // Callback to send a scancode to the PPI.
  void (*send_scancode)(void* context, uint8_t scancode);
  // Callback to raise an IRQ1 (keyboard interrupt) to the CPU.
  void (*raise_irq1)(void* context);
} YKeyboardConfig;

enum {
  // Maximum number of keys to buffer. Additional key presses will be dropped.
  Y_KEYBOARD_BUFFER_SIZE = 16,
  // Threshold required to trigger keyboard reset when clock line is held low.
  Y_KEYBOARD_RESET_THRESHOLD_MS = 20,
};
Y_STATIC_VECTOR_TYPE(YKeyboardBuffer, uint8_t, Y_KEYBOARD_BUFFER_SIZE)

// State of the Keyboard.
typedef struct YKeyboardState {
  // Pointer to the keyboard configuration.
  YKeyboardConfig config;

  // State of PPI Port B bit 7, or PBKB in GLaBIOS.
  // - false = enable keyboard
  // - true  = clear keyboard (reset)
  bool enable_clear;

  // Current state of PPI Port B bit 6, or PBKC in GLaBIOS.
  // - false = hold keyboard clock low
  // - true  = enabled (normal operation)
  bool clock_low;

  // Number of ms since the clock_low line was set to false (enabled). This is
  // used to detect the reset signal from the BIOS, which is holding the clock
  // low for at least 20ms.
  //   - 0 = clock line is high (normal operation)
  //   - 0xFF = clock line has been low for at least 20ms
  uint8_t clock_low_ms;

  // Whether we are currently waiting for ack from BIOS before sending the next
  // scancode. The keyboard will not send any further scancodes until the BIOS
  // pulses the enable_clear line high then low.
  bool waiting_for_ack;

  // Buffer of key presses received.
  YKeyboardBuffer buffer;
} YKeyboardState;

// Initializes the keyboard to its power-on state.
Y_PUBLIC void YKeyboardInit(YKeyboardState* keyboard);

// Receive keyboard control bits from the PPI (bits 6 and 7 of Port B).
Y_PUBLIC void YKeyboardHandleControl(
    YKeyboardState* keyboard, bool enable_clear, bool clock_low);

// Handles a real key press event.
Y_PUBLIC void YKeyboardHandleKeyPress(
    YKeyboardState* keyboard, uint8_t scancode);

// Simulates a 1ms tick. This is needed to respond to reset commands and to
// send buffered scancodes.
Y_PUBLIC void YKeyboardTickMs(YKeyboardState* keyboard);

#endif  // YAX86_KEYBOARD_PUBLIC_H


// ==============================================================================
// src/keyboard/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/keyboard/keyboard.c start
// ==============================================================================

#line 1 "./src/keyboard/keyboard.c"
#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

enum {
  // Value for clock_low_ms indicating that a reset has already been triggered.
  Y_KEYBOARD_RESET_TRIGGERED = 0xFF,
  // Scan code indicating successful self-test.
  Y_KEYBOARD_SELF_TEST_OK = 0xAA,
};

Y_PUBLIC void YKeyboardInit(YKeyboardState* keyboard) {
  // Default to keyboard enabled (enable_clear = false) with clock held low
  // (clock_low = true). This allows us to detect a falling edge on clock_low
  // which triggers the reset timer.
  keyboard->enable_clear = false;
  keyboard->clock_low = true;
  keyboard->clock_low_ms = 0;
  YKeyboardBufferInit(&keyboard->buffer);
  keyboard->waiting_for_ack = false;
}

// Helper to send a scancode to the PPI and raise IRQ1 if needed.
Y_FILE_PRIVATE inline void YKeyboardSendScancode(
    YKeyboardState* keyboard, uint8_t scancode) {
  if (keyboard->config.send_scancode) {
    keyboard->config.send_scancode(keyboard->config.context, scancode);
  }
  if (keyboard->config.raise_irq1) {
    keyboard->config.raise_irq1(keyboard->config.context);
  }
  keyboard->waiting_for_ack = true;
}

// Helper to send the next scancode in the buffer if available.
Y_FILE_PRIVATE inline void YKeyboardSendNextScancode(YKeyboardState* keyboard) {
  // Can only send in state [0, 1], i.e. enable_clear = false clock_low = true
  if (!(keyboard->enable_clear == false && keyboard->clock_low == true)) {
    return;
  }
  // Can only send after previous scancode has been acked.
  if (keyboard->waiting_for_ack) {
    return;
  }
  // If buffer is empty, nothing to send.
  if (YKeyboardBufferLength(&keyboard->buffer) == 0) {
    return;
  }

  // Send the next scancode in the buffer.
  uint8_t scancode = *YKeyboardBufferGet(&keyboard->buffer, 0);
  YKeyboardBufferRemove(&keyboard->buffer, 0);
  YKeyboardSendScancode(keyboard, scancode);
}

Y_PUBLIC void YKeyboardHandleControl(
    YKeyboardState* keyboard, bool enable_clear, bool clock_low) {
  // Save previous state.
  bool old_clock_low = keyboard->clock_low;
  bool old_enable_clear = keyboard->enable_clear;

  // Update state.
  keyboard->enable_clear = enable_clear;
  keyboard->clock_low = clock_low;

  // Falling edge of enable_clear bit indicates ack from BIOS. we clear the
  // waiting_for_ack bit, allowing the next queued scancode to be sent on the
  // next tick.
  if (old_enable_clear == true && keyboard->enable_clear == false &&
      keyboard->clock_low == true) {
    keyboard->waiting_for_ack = false;
  }

  // Falling edge of clock_low bit possibly indicates the start of a reset
  // command from BIOS. We reset the timer at 0ms.
  if (old_clock_low == true && keyboard->clock_low == false) {
    keyboard->clock_low_ms = 0;
  }
}

Y_PUBLIC void YKeyboardHandleKeyPress(
    YKeyboardState* keyboard, uint8_t scancode) {
  // Drop key presses that occur while the keyboard is running its self test.
  // Queueing it would let it resurface once the reset ends, which lands it in
  // the middle of the BIOS's stuck key test.
  if (!keyboard->clock_low &&
      keyboard->clock_low_ms == Y_KEYBOARD_RESET_TRIGGERED) {
    return;
  }
  YKeyboardBufferAppend(&keyboard->buffer, &scancode);
}

Y_PUBLIC void YKeyboardTickMs(YKeyboardState* keyboard) {
  // If clock_low line is being held low, update timer and trigger reset if
  // reached threshold.
  if (keyboard->clock_low == false) {
    if (keyboard->clock_low_ms == Y_KEYBOARD_RESET_TRIGGERED) {
      // Reset already triggered, nothing to do.
      return;
    }

    // Increment timer since clock line was held low.
    ++keyboard->clock_low_ms;

    // Haven't reached threshold yet, nothing to do.
    if (keyboard->clock_low_ms < Y_KEYBOARD_RESET_THRESHOLD_MS) {
      return;
    }

    // Reached threshold, trigger reset.
    YKeyboardBufferClear(&keyboard->buffer);
    keyboard->waiting_for_ack = false;
    // Set to special value indicating reset has been triggered.
    keyboard->clock_low_ms = Y_KEYBOARD_RESET_TRIGGERED;
    // Send self-test passed scancode on next falling edge of enable_clear.
    uint8_t scancode = Y_KEYBOARD_SELF_TEST_OK;
    YKeyboardBufferAppend(&keyboard->buffer, &scancode);
    return;
  }

  // Normal operation.
  YKeyboardSendNextScancode(keyboard);
}


// ==============================================================================
// src/keyboard/keyboard.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_KEYBOARD_BUNDLE_H

// ==============================================================================
// YAX86 PIC MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_PIC_BUNDLE_H
#define YAX86_PIC_BUNDLE_H

#ifndef YAX86_UTIL_BUNDLE_H
#include "util.h"
#endif  // YAX86_UTIL_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/pic/public.h start
// ==============================================================================

#line 1 "./src/pic/public.h"
// Public interface for the PIC (Programmable Interrupt Controller) module.
#ifndef YAX86_PIC_PUBLIC_H
#define YAX86_PIC_PUBLIC_H

// This module emulates the Intel 8259 PIC(s) on the IBM PC series. There are
// two possible configurations:
//
// 1. Single PIC - IBM PC and PC/XT
//    The system has a single PIC at I/O ports 0x20/0x21, handling IRQs 0-7,
//    connected to the CPU.
//
// 2. Cascaded PICs - IBM PC/AT and PS/2
//    The system has a master PIC at I/O ports 0x20/0x21 handling IRQs 0-7,
//    and a slave PIC at I/O ports 0xA0/0xA1 handling IRQs 8-15. The slave PIC
//    is connected to the master's IRQ2 line. Only the master PIC is directly
//    connected to the CPU.
//
// Note that we do not support all features of the 8259 PIC, such as auto EOI,
// rotating priorities, etc., as they are not used by MS-DOS or the IBM PC
// BIOS.

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_PIC_BUNDLE_H
#include "../util/log.h"
#endif  // YAX86_PIC_BUNDLE_H

enum {
  // Log module ID for the PIC.
  Y_PIC_LOG_MODULE_ID = 2,
};

// Log module for the PIC.
Y_PUBLIC_HEADER const YLogModule YPICLogModule = {
    .id = Y_PIC_LOG_MODULE_ID,
    .name = "pic",
};

// ============================================================================
// PIC state
// ============================================================================

// The mode of a PIC - single, master, or slave.
typedef enum YPICMode {
  // Single PIC on IBM PC and PC/XT
  Y_PIC_SINGLE = 0,
  // Master PIC on IBM PC/AT and PS/2
  Y_PIC_MASTER,
  // Slave PIC on IBM PC/AT and PS/2
  Y_PIC_SLAVE,
  // Number of PIC modes
  Y_PIC_NUM_MODES,
} YPICMode;

// Initialization state of a PIC.
typedef enum YPICInitState {
  // Uninitialized - waiting for ICW1.
  Y_PIC_EXPECT_ICW1 = 0,
  // ICW1 received - waiting for ICW2.
  Y_PIC_EXPECT_ICW2,
  // ICW2 received - waiting for ICW3 (if needed).
  Y_PIC_EXPECT_ICW3,
  // ICW3 received - waiting for ICW4 (if needed) or fully initialized.
  Y_PIC_EXPECT_ICW4,
  // Fully initialized.
  Y_PIC_READY,
} YPICInitState;

enum {
  // Indicates no pending interrupt. In normal operation, valid ranges of
  // interrupt vectors are 0x08-0x0F for a single PIC or master PIC, and
  // 0x70-0x77 for a slave PIC.
  Y_PIC_NO_PENDING_INTERRUPT = 0xFF,
};

struct YPICState;

// Caller-provided runtime configuration.
typedef struct YPICConfig {
  // Logger for this module. May be NULL.
  YLogger* logger;

  // State of the SP pin.
  // - Single PIC on IBM PC and PC/XT => false
  // - Master PIC on IBM PC/AT and PS/2 => false
  // - Slave PIC on IBM PC/AT and PS/2 => true
  bool sp;
} YPICConfig;

// The register to read on the next read from the data port.
typedef enum YPICReadRegister {
  Y_PIC_READ_IMR = 0,  // Default: read Interrupt Mask Register
  Y_PIC_READ_IRR = 1,  // Read Interrupt Request Register on next read
  Y_PIC_READ_ISR = 2,  // Read In-Service Register on next read
} YPICReadRegister;

// State of a single 8259 PIC chip.
typedef struct YPICState {
  // Pointer to caller-provided runtime configuration.
  YPICConfig config;

  // Initialization state.
  YPICInitState init_state;
  // Received initialization words.
  uint8_t icw1;
  uint8_t icw2;
  uint8_t icw3;
  // We don't store ICW4 as its extra features are not used by MS-DOS or the
  // IBM PC BIOS.

  // Interrupt Request Register - pending interrupts. Bit i is set if IRQ i is
  // pending.
  uint8_t irr;
  // In-Service Register - interrupts currently being serviced. Bit i is set if
  // IRQ i is being serviced.
  uint8_t isr;
  // Interrupt Mask Register - masked interrupts. Bit i is set if IRQ i is
  // masked.
  uint8_t imr;

  // Whether any requested interrupt is unmasked - that is, whether irr & ~imr
  // is non-zero. Recomputed by YPICUpdateUnmaskedRequest(), which is the sole
  // writer, wherever irr or imr changes.
  //
  // This exists so that a CPU can tell in a load whether asking for an
  // interrupt could produce one. It asks on every instruction it runs with
  // interrupts enabled, which is nearly all of them, and the answer is almost
  // always no.
  //
  // It is deliberately conservative rather than exact: it ignores priority and
  // the in-service register, so it can be true where YPICGetPendingInterrupt()
  // would still decline. That costs only a call that reports nothing. What it
  // may never be is falsely false, because a reader takes it as permission not
  // to ask at all.
  bool has_unmasked_request;

  // The register to read on the next read from the data port.
  YPICReadRegister read_register;

  // Pointer to master PIC if this is a slave, or to slave PIC if this is a
  // master. NULL if this is a single PIC.
  struct YPICState* cascade_pic;
} YPICState;

// ============================================================================
// PIC initialization
// ============================================================================

// Initialize a PIC with the provided configuration.
Y_PUBLIC void YPICInit(YPICState* pic);

// ============================================================================
// IRQ line control
// ============================================================================

// Raise an IRQ line (0-7) on this PIC. If this is a slave PIC, also raises
// the cascade IRQ on the master PIC.
Y_PUBLIC void YPICRaiseIRQ(YPICState* pic, uint8_t irq);

// Lower an IRQ line (0-7) on this PIC. If this is a slave PIC and no interrupts
// are pending, also lowers the cascade IRQ on the master PIC.
Y_PUBLIC void YPICLowerIRQ(YPICState* pic, uint8_t irq);

// ============================================================================
// I/O port interface
// ============================================================================

// Read from a PIC I/O port.
// For master PIC: port should be 0x20 (command) or 0x21 (data).
// For slave PIC: port should be 0xA0 (command) or 0xA1 (data).
Y_PUBLIC uint8_t YPICReadPort(YPICState* pic, uint16_t port);

// Write to a PIC I/O port.
// For master PIC: port should be 0x20 (command) or 0x21 (data).
// For slave PIC: port should be 0xA0 (command) or 0xA1 (data).
Y_PUBLIC void YPICWritePort(YPICState* pic, uint16_t port, uint8_t value);

// ============================================================================
// Interrupt handling
// ============================================================================

// Get the highest priority pending interrupt vector number from this PIC. If
// this is a master PIC, this will consider pending interrupts from the slave
// PIC as well. If no interrupts are pending, returns
// Y_PIC_NO_PENDING_INTERRUPT.
Y_PUBLIC uint8_t YPICGetPendingInterrupt(YPICState* pic);

#endif  // YAX86_PIC_PUBLIC_H


// ==============================================================================
// src/pic/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/pic/pic.c start
// ==============================================================================

#line 1 "./src/pic/pic.c"
#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

#define Y_PIC_ERROR(...) \
  Y_LOG(pic->config.logger, &YPICLogModule, Y_LOG_LEVEL_ERROR, __VA_ARGS__)
#define Y_PIC_WARN(...) \
  Y_LOG(pic->config.logger, &YPICLogModule, Y_LOG_LEVEL_WARN, __VA_ARGS__)
#define Y_PIC_INFO(...) \
  Y_LOG(pic->config.logger, &YPICLogModule, Y_LOG_LEVEL_INFO, __VA_ARGS__)
#define Y_PIC_DEBUG(...) \
  Y_LOG(pic->config.logger, &YPICLogModule, Y_LOG_LEVEL_DEBUG, __VA_ARGS__)

// ============================================================================
// Constants
// ============================================================================

enum {
  // ICW bits
  Y_PIC_ICW1_IC4 = (1 << 0),   // 1 = ICW4 needed
  Y_PIC_ICW1_SNGL = (1 << 1),  // 1 = single PIC, 0 = cascaded
  Y_PIC_ICW1_INIT = (1 << 4),  // 1 = initialization mode
  Y_PIC_ICW2_BASE = 0xF8,  // Upper 5 bits of ICW2 = the interrupt vector base

  // OCW bits
  Y_PIC_OCW_SELECT = (1 << 3),  // 1 = OCW3, 0 = OCW2
  Y_PIC_OCW2_EOI = (1 << 5),    // End of Interrupt
  Y_PIC_OCW2_SL = (1 << 6),     // Specific Level
  Y_PIC_OCW3_RR = (1 << 1),     // 1 = Read Register command
  Y_PIC_OCW3_RIS = (1 << 0),    // 1 = Read ISR, 0 = Read IRR

  // Master PIC cascade IRQ line
  Y_PIC_MASTER_CASCADE_IRQ = 2,
};

// The I/O port of a PIC.
typedef enum YPICPort {
  Y_PIC_PORT_COMMAND = 0,
  Y_PIC_PORT_DATA = 1,

  // Number of PIC ports.
  Y_PIC_NUM_PORTS,
  // Invalid port.
  Y_PIC_PORT_INVALID = -1,
} YPICPort;

// Map a PIC mode to its base I/O port.
Y_FILE_PRIVATE const uint16_t YPICBasePorts[Y_PIC_NUM_MODES] = {
    0x20,  // Y_PIC_SINGLE
    0x20,  // Y_PIC_MASTER
    0xA0,  // Y_PIC_SLAVE
};

// ============================================================================
// Helper functions
// ============================================================================

// Returns the mode of a PIC based on its ICWs.
Y_FILE_PRIVATE inline YPICMode YPICGetMode(YPICState* pic) {
  // If SNGL bit set in ICW1, we are single PIC.
  if (pic->icw1 & Y_PIC_ICW1_SNGL) {
    return Y_PIC_SINGLE;
  }

  // Otherwise, we are cascaded.
  // If SP pin is set, we are slave; otherwise, master.
  return pic->config.sp ? Y_PIC_SLAVE : Y_PIC_MASTER;
}

// Returns if the PIC is configured as a single PIC.
Y_FILE_PRIVATE inline bool YPICIsSingle(YPICState* pic) {
  return YPICGetMode(pic) == Y_PIC_SINGLE;
}

// Returns if the PIC is a master PIC.
Y_FILE_PRIVATE inline bool YPICIsMaster(YPICState* pic) {
  return YPICGetMode(pic) == Y_PIC_MASTER;
}

// Returns if the PIC is a slave PIC.
Y_FILE_PRIVATE inline bool YPICIsSlave(YPICState* pic) {
  return YPICGetMode(pic) == Y_PIC_SLAVE;
}

// Returns the I/O port corresponding to a given port number.
Y_FILE_PRIVATE inline YPICPort YPICGetPort(YPICState* pic, uint16_t port) {
  uint16_t port_offset = port - YPICBasePorts[YPICGetMode(pic)];
  if (port_offset >= Y_PIC_NUM_PORTS) {
    return Y_PIC_PORT_INVALID;
  }
  return (YPICPort)port_offset;
}

// Returns the IRQ number of the parent PIC connected to a slave PIC.
// Only valid if pic is a slave PIC.
Y_FILE_PRIVATE inline uint8_t YPICGetCascadeIRQ(YPICState* pic) {
  return pic->icw3 & 0x07;
}

// ============================================================================
// PIC initialization
// ============================================================================

// Recomputes YPICState.has_unmasked_request. This is its sole writer, and every
// path that changes irr or imr calls it, so the flag can never disagree with
// the registers it summarizes.
Y_FILE_PRIVATE inline void YPICUpdateUnmaskedRequest(YPICState* pic) {
  pic->has_unmasked_request = (pic->irr & ~pic->imr) != 0;
}

Y_PUBLIC void YPICInit(YPICState* pic) {
  // All interrupts masked by default.
  pic->imr = 0xFF;
  YPICUpdateUnmaskedRequest(pic);
}

// ============================================================================
// IRQ line control
// ============================================================================

Y_PUBLIC void YPICRaiseIRQ(YPICState* pic, uint8_t irq) {
  if (irq > 7) {
    Y_PIC_WARN("irq_out_of_range", Y_LOG_UINT("irq", irq));
    return;
  }
  Y_PIC_DEBUG(
      "irq_raised", Y_LOG_UINT("irq", irq), Y_LOG_HEX8("imr", pic->imr),
      Y_LOG_HEX8("isr", pic->isr));
  pic->irr |= (1 << irq);
  YPICUpdateUnmaskedRequest(pic);

  // If this is a slave PIC, also raise the cascade IRQ on the master.
  if (YPICIsSlave(pic) && pic->cascade_pic) {
    YPICRaiseIRQ(pic->cascade_pic, YPICGetCascadeIRQ(pic));
  }
}

Y_PUBLIC void YPICLowerIRQ(YPICState* pic, uint8_t irq) {
  if (irq > 7) {
    return;
  }
  pic->irr &= ~(1 << irq);
  YPICUpdateUnmaskedRequest(pic);

  // If this is a slave PIC and no interrupts are pending, lower the cascade
  // IRQ on the master.
  if (YPICIsSlave(pic) && pic->irr == 0 && pic->cascade_pic) {
    YPICLowerIRQ(pic->cascade_pic, YPICGetCascadeIRQ(pic));
  }
}

// ============================================================================
// I/O port interface
// ============================================================================

Y_PUBLIC uint8_t YPICReadPort(YPICState* pic, uint16_t port) {
  YPICPort pic_port = YPICGetPort(pic, port);
  switch (pic_port) {
    case Y_PIC_PORT_COMMAND:
      // Reading from the command port is not a defined operation.
      return 0x00;

    case Y_PIC_PORT_DATA: {
      uint8_t value;
      switch (pic->read_register) {
        case Y_PIC_READ_IRR:
          value = pic->irr;
          break;
        case Y_PIC_READ_ISR:
          value = pic->isr;
          break;
        default:
          value = pic->imr;
          break;
      }
      pic->read_register = Y_PIC_READ_IMR;
      return value;
    }

    default:
      // Invalid port.
      return 0x00;
  }
}

Y_PUBLIC void YPICWritePort(YPICState* pic, uint16_t port, uint8_t value) {
  YPICPort pic_port = YPICGetPort(pic, port);
  switch (pic_port) {
    case Y_PIC_PORT_COMMAND:
      if (value & Y_PIC_ICW1_INIT) {
        // This is ICW1, which starts the initialization sequence.
        pic->icw1 = value;
        pic->irr = 0x00;
        pic->isr = 0x00;
        // All interrupts masked by default.
        pic->imr = 0xFF;
        YPICUpdateUnmaskedRequest(pic);

        // The next write to the data port will be ICW2.
        pic->init_state = Y_PIC_EXPECT_ICW2;
      } else {
        // This is an OCW (Operational Command Word).
        if (value & Y_PIC_OCW_SELECT) {
          // This is OCW3.
          if (value & Y_PIC_OCW3_RR) {
            // This is a Read Register command.
            pic->read_register =
                value & Y_PIC_OCW3_RIS ? Y_PIC_READ_ISR : Y_PIC_READ_IRR;
          } else {
            // Other OCW3 commands (e.g. Special Mask Mode) are not
            // implemented.
          }
        } else {
          // This is OCW2.
          if (value & Y_PIC_OCW2_EOI) {
            if (value & Y_PIC_OCW2_SL) {
              // Specific EOI: clear specified ISR bit.
              uint8_t irq = value & 0x07;
              Y_PIC_DEBUG("specific_eoi", Y_LOG_UINT("irq", irq));
              pic->isr &= ~(1 << irq);
            } else {
              // Non-Specific EOI: clear highest priority ISR bit.
              for (uint8_t i = 0, isr_mask = 1; i < 8; ++i, isr_mask <<= 1) {
                if (pic->isr & isr_mask) {
                  Y_PIC_DEBUG("non_specific_eoi", Y_LOG_UINT("irq", i));
                  pic->isr &= ~isr_mask;
                  break;
                }
              }
            }
          } else {
            // Other OCW2 commands (Rotate) are not implemented as they are not
            // used by MS-DOS or the IBM PC BIOS.
          }
        }
      }
      break;

    case Y_PIC_PORT_DATA:
      switch (pic->init_state) {
        case Y_PIC_EXPECT_ICW2:
          // This is ICW2. It sets the interrupt vector base.
          // The PIC uses the upper 5 bits of this value.
          pic->icw2 = value;
          if (YPICIsSingle(pic)) {
            // Single mode -> no ICW3, ICW4 optional depending on ICW1.
            pic->init_state =
                pic->icw1 & Y_PIC_ICW1_IC4 ? Y_PIC_EXPECT_ICW4 : Y_PIC_READY;
          } else {
            // Cascaded mode. Expect ICW3 next.
            pic->init_state = Y_PIC_EXPECT_ICW3;
          }
          break;

        case Y_PIC_EXPECT_ICW3:
          // This is ICW3.
          // For master, it's a bitmask of slaves.
          // For slave, it's the 3-bit slave ID.
          pic->icw3 = value;
          // ICW4 is optional depending on ICW1.
          pic->init_state =
              pic->icw1 & Y_PIC_ICW1_IC4 ? Y_PIC_EXPECT_ICW4 : Y_PIC_READY;
          break;

        case Y_PIC_EXPECT_ICW4:
          // This is ICW4.
          pic->init_state = Y_PIC_READY;
          break;

        default:
          // This is an OCW1, which sets the IMR.
          pic->imr = value;
          YPICUpdateUnmaskedRequest(pic);
          break;
      }
      break;

    default:
      // Invalid port - ignore.
      break;
  }
}

// ============================================================================
// Interrupt handling
// ============================================================================

Y_PUBLIC Y_HOT uint8_t YPICGetPendingInterrupt(YPICState* pic) {
  // Find highest priority requested and unmasked interrupt.
  uint8_t irr = pic->irr & ~pic->imr;
  if (irr == 0) {
    return Y_PIC_NO_PENDING_INTERRUPT;
  }
  uint8_t pending_irq = 0, pending_irq_mask = 1;
  for (; pending_irq < 8; ++pending_irq, pending_irq_mask <<= 1) {
    if (irr & pending_irq_mask) {
      break;
    }
  }

  // If there is already an interrupt being serviced, the new pending interrupt
  // must have higher priority (lower IRQ number) to be serviced now.
  if (pic->isr > 0) {
    uint8_t in_service_irq = 0, in_service_irq_mask = 1;
    for (; in_service_irq < 8; ++in_service_irq, in_service_irq_mask <<= 1) {
      if (pic->isr & in_service_irq_mask) {
        break;
      }
    }
    if (pending_irq >= in_service_irq) {
      // New interrupt does not have higher priority than in-service interrupt.
      return Y_PIC_NO_PENDING_INTERRUPT;
    }
  }

  // If this is the master PIC and the interrupt is from the slave, return the
  // slave PIC's interrupt vector.
  if (YPICIsMaster(pic) && pending_irq == Y_PIC_MASTER_CASCADE_IRQ &&
      pic->cascade_pic) {
    uint8_t slave_vector = YPICGetPendingInterrupt(pic->cascade_pic);
    if (slave_vector != Y_PIC_NO_PENDING_INTERRUPT) {
      pic->isr |= pending_irq_mask;
    }
    return slave_vector;
  }

  // This is a normal interrupt on this PIC (or it's a slave reporting up).
  pic->isr |= pending_irq_mask;
  pic->irr &= ~pending_irq_mask;
  YPICUpdateUnmaskedRequest(pic);

  return (pic->icw2 & Y_PIC_ICW2_BASE) + pending_irq;
}


// ==============================================================================
// src/pic/pic.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_PIC_BUNDLE_H

// ==============================================================================
// YAX86 PIT MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_PIT_BUNDLE_H
#define YAX86_PIT_BUNDLE_H

#ifndef YAX86_UTIL_BUNDLE_H
#include "util.h"
#endif  // YAX86_UTIL_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

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

// ==============================================================================
// YAX86 PLATFORM MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_PLATFORM_BUNDLE_H
#define YAX86_PLATFORM_BUNDLE_H

#ifndef YAX86_UTIL_BUNDLE_H
#include "util.h"
#endif  // YAX86_UTIL_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/platform/public.h start
// ==============================================================================

#line 1 "./src/platform/public.h"
// Public interface for the Platform module.
#ifndef YAX86_PLATFORM_PUBLIC_H
#define YAX86_PLATFORM_PUBLIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef YAX86_PLATFORM_BUNDLE_H
#include "../util/log.h"
#include "../util/static_vector.h"
#endif  // YAX86_PLATFORM_BUNDLE_H

enum {
  // Log module ID for the Platform.
  Y_PLATFORM_LOG_MODULE_ID = 0,
};

// Log module for the Platform.
Y_PUBLIC_HEADER const YLogModule YPlatformLogModule = {
    .id = Y_PLATFORM_LOG_MODULE_ID,
    .name = "platform",
};

#include "cpu.h"
#include "dma.h"
#include "fdc.h"
#include "hdc.h"
#include "keyboard.h"
#include "pic.h"
#include "pit.h"
#include "ppi.h"
#include "video.h"

struct YPlatformState;

// ============================================================================
// Memory mapping
// ============================================================================

// Type ID of a memory map entry.
typedef uint8_t YPlatformMemoryMapEntryType;

enum {
  // Conventional memory - first 640KB of physical memory, mapped to 0x00000 to
  // 0x9FFFF (640KB).
  Y_PLATFORM_MEMORY_MAP_ENTRY_CONVENTIONAL = 0,
  // BIOS ROM - mapped to 0xF0000 to 0xFFFFF (64KB).
  Y_PLATFORM_MEMORY_MAP_ENTRY_BIOS_ROM = 0x01,
  // HDC option ROM - mapped starting at 0xC8000.
  Y_PLATFORM_MEMORY_MAP_ENTRY_HDC_OPTION_ROM = 0x0C,
  // Video RAM - mapped to 0xB0000 (MDA) or 0xB8000 (CGA).
  Y_PLATFORM_MEMORY_MAP_ENTRY_VRAM = 0x10,

  // Maximum number of memory map entries.
  Y_PLATFORM_MAX_MEMORY_MAP_ENTRIES = 16,

  // How many decoded instructions the CPU is given room to keep. 256 entries
  // is 6KB, and skips the decode on roughly two thirds of the instructions the
  // machine runs.
  //
  // Not the fastest size measured: 512 is 0.11% faster, 1024 is 0.04% slower
  // and 128 is 0.83% slower. 0.11% does not buy another 6KB on a 256KB part
  // where that memory is otherwise guest RAM.
  Y_PLATFORM_DECODE_CACHE_ENTRIES = 256,

  // The memory map is indexed by page so that a lookup is a load rather than a
  // walk. 4KB pages over the 8086's 1MB address space, which is 256 bytes of
  // index - small enough to keep in the platform state, and coarse enough that
  // every region the machine registers is page aligned.
  Y_PLATFORM_MEMORY_PAGE_SHIFT = 12,
  Y_PLATFORM_MEMORY_PAGE_SIZE = 1 << Y_PLATFORM_MEMORY_PAGE_SHIFT,
  Y_PLATFORM_MEMORY_ADDRESS_SPACE_SIZE = 0x100000,
  Y_PLATFORM_NUM_MEMORY_PAGES = Y_PLATFORM_MEMORY_ADDRESS_SPACE_SIZE >>
      Y_PLATFORM_MEMORY_PAGE_SHIFT,
  // A page no entry covers.
  Y_PLATFORM_MEMORY_PAGE_UNMAPPED = 0xFF,
  // A page more than one entry has a share of, which has to be resolved by
  // address. Nothing registers such a region today - every one of them is page
  // aligned - but the index must not quietly answer the wrong entry if one
  // ever does. Note that this is never the answer for an address above the
  // address space: no entry may reach one, so it is unmapped instead.
  Y_PLATFORM_MEMORY_PAGE_STRADDLED = 0xFE,

  // Maximum size of physical memory in bytes.
  Y_PLATFORM_MAX_PHYSICAL_MEMORY_SIZE = 640 * 1024,
  // Minimum size of physical memory in bytes.
  Y_PLATFORM_MIN_PHYSICAL_MEMORY_SIZE = 64 * 1024,
};

// A memory map entry for a region in logical address space. Memory regions
// should not overlap.
typedef struct YPlatformMemoryMapEntry {
  // Custom data passed through to callbacks.
  void* context;

  // The memory map entry type, such as
  // Y_PLATFORM_MEMORY_MAP_ENTRY_CONVENTIONAL.
  YPlatformMemoryMapEntryType entry_type;
  // Start address of the memory region.
  uint32_t start;
  // Inclusive end address of the memory region.
  uint32_t end;

  // Direct pointer to the bytes backing this region, covering [start, end].
  //
  // Most regions are plain storage - RAM, and ROM images the caller already
  // holds in an array - and reading them through a callback costs an indirect
  // call per byte for no benefit. When these are set, accesses go straight to
  // the buffer and the callbacks below are not consulted.
  //
  // The buffer must remain valid, and must not move, for the lifetime of the
  // platform. A region whose accesses do something other than load or store a
  // value must leave the corresponding pointer NULL and supply a callback
  // instead. The two are independent: a region may serve reads directly while
  // routing writes through write_byte_fn, which is what an adapter that has
  // to notice writes - to track which parts of a framebuffer are dirty, say -
  // would want.
  //
  // read_data and write_data are separate pointers, rather than one pointer
  // plus a read/write/both flag, so that a read-only region can point at a
  // genuinely const array - the BIOS and option ROM images are compiled in as
  // const uint8_t[] - instead of casting away const to fit a single
  // non-const field. That keeps "this region cannot be written" a property
  // the compiler checks at the assignment into write_data, rather than a
  // runtime flag that a bug could get past; on a target where ROM is XIP
  // flash, writing through it faults rather than silently doing nothing.
  const uint8_t* read_data;
  // Direct pointer for writes. NULL for a read-only region such as a ROM,
  // whose writes are discarded.
  uint8_t* write_data;

  // Callback to read a byte from the memory map entry, where address is
  // relative to the start of the entry. Ignored if read_data is set.
  uint8_t (*read_byte_fn)(
      struct YPlatformMemoryMapEntry* entry, uint32_t relative_address);
  // Callback to write a byte to memory, where address is relative to the start
  // address. Ignored if write_data is set.
  void (*write_byte_fn)(
      struct YPlatformMemoryMapEntry* entry, uint32_t relative_address,
      uint8_t value);
} YPlatformMemoryMapEntry;

// Register a memory map entry in the platform state. Returns true if the entry
// was successfully registered, or false if:
//   - The entry's memory region does not lie within the address space.
//   - There already exists a memory map entry with the same type.
//   - The new entry's memory region overlaps with an existing entry.
//   - The number of memory map entries would exceed
//   Y_PLATFORM_MAX_MEMORY_MAP_ENTRIES.
//
// This does not tell the CPU that the map has changed. Follow a registration,
// or a run of them, with YPlatformUpdateAfterMemoryMapChange(). Skipping it
// leaves the CPU free to reuse a decoded instruction it took from an address
// that was unmapped at the time and so read as open bus - which no page
// generation catches, because nothing was written. YPlatformInit() registers
// every region the machine has and updates once at the end.
Y_PUBLIC bool YPlatformRegisterMemoryMapEntry(
    struct YPlatformState* platform, const YPlatformMemoryMapEntry* entry);
// Discard what the CPU derives from the memory map: its instruction fetch
// window, its direct data window and its decode cache.
Y_PUBLIC void YPlatformUpdateAfterMemoryMapChange(
    struct YPlatformState* platform);
// Look up the memory map entry corresponding to an address. Returns NULL if the
// address is not mapped to a known memory map entry.
Y_PUBLIC YPlatformMemoryMapEntry* YPlatformGetMemoryMapEntryForAddress(
    struct YPlatformState* platform, uint32_t address);
// Look up a memory map entry by type. Returns NULL if no entry found with the
// specified type.
Y_PUBLIC YPlatformMemoryMapEntry* YPlatformGetMemoryMapEntryByType(
    struct YPlatformState* platform, YPlatformMemoryMapEntryType entry_type);

// Read a byte from a logical memory address, either directly from the
// corresponding memory map entry's read_data buffer or via its read_byte_fn
// callback.
//
// On the 8086, accessing an invalid memory address will yield garbage data
// rather than causing a page fault. This interface mirrors that behavior.
Y_PUBLIC uint8_t
YPlatformReadMemoryByte(struct YPlatformState* platform, uint32_t address);
// Read a word from a logical memory address, either directly from the
// corresponding memory map entry's read_data buffer or via its read_byte_fn
// callback.
Y_PUBLIC uint16_t
YPlatformReadMemoryWord(struct YPlatformState* platform, uint32_t address);
// Write a byte to a logical memory address, either directly to the
// corresponding memory map entry's write_data buffer or via its
// write_byte_fn callback.
//
// On the 8086, accessing an invalid memory address will yield garbage data
// rather than causing a page fault. This interface mirrors that behavior.
Y_PUBLIC void YPlatformWriteMemoryByte(
    struct YPlatformState* platform, uint32_t address, uint8_t value);

// ============================================================================
// I/O port mapping
// ============================================================================

// Type ID of an I/O port map entry.
typedef uint16_t YPlatformPortMapEntryType;

enum {
  // Maximum number of I/O port mapping entries.
  Y_PLATFORM_MAX_PORT_MAP_ENTRIES = 16,
  // I/O port map entry for the master PIC (ports 0x20-0x21).
  Y_PLATFORM_PORT_MAP_ENTRY_PIC = 0x20,
  // I/O port map entry for the PIT (ports 0x40-0x43).
  Y_PLATFORM_PORT_MAP_ENTRY_PIT = 0x40,
  // I/O port map entry for the PPI (ports 0x60-0x63).
  Y_PLATFORM_PORT_MAP_ENTRY_PPI = 0x60,
  // I/O port map entry for the video adapter (ports 0x3B0-0x3BF or
  // 0x3D0-0x3DF).
  Y_PLATFORM_PORT_MAP_ENTRY_VIDEO = 0x10,
  // I/O port map entry for the FDC (ports 0x3F0-0x3F7).
  Y_PLATFORM_PORT_MAP_ENTRY_FDC = 0x3F0,
  // I/O port map entry for the HDC (ports 0x300-0x30F).
  Y_PLATFORM_PORT_MAP_ENTRY_HDC = 0x300,
  // I/O port map entry for the DMA controller (ports 0x00-0x0F).
  Y_PLATFORM_PORT_MAP_ENTRY_DMA = 0x00,
  // I/O port map entry for the DMA Page Registers (ports 0x80-0x8F).
  Y_PLATFORM_PORT_MAP_ENTRY_DMA_PAGE = 0x80,
};

// An I/O port map entry. Entries should not overlap.
typedef struct YPlatformPortMapEntry {
  // Custom data passed through to callbacks.
  void* context;

  // The I/O port map entry type.
  YPlatformPortMapEntryType entry_type;
  // Start of the I/O port range.
  uint16_t start;
  // Inclusive end of the I/O port range.
  uint16_t end;
  // Callback to read a byte from an I/O port within the range.
  uint8_t (*read_byte)(struct YPlatformPortMapEntry* entry, uint16_t port);
  // Callback to write a byte an I/O port within the range.
  void (*write_byte)(
      struct YPlatformPortMapEntry* entry, uint16_t port, uint8_t value);
} YPlatformPortMapEntry;

// Register an I/O port map entry in the platform state. Returns true if the
// entry was successfully registered, or false if:
//   - There already exists an I/O port map entry with the same type.
//   - The new entry's I/O port range overlaps with an existing entry.
Y_PUBLIC bool YPlatformRegisterPortMapEntry(
    struct YPlatformState* platform, const YPlatformPortMapEntry* entry);
// Look up the I/O port map entry corresponding to a port. Returns NULL if the
// port is not mapped to a known I/O port map entry.
Y_PUBLIC YPlatformPortMapEntry* YPlatformGetPortMapEntryForPort(
    struct YPlatformState* platform, uint16_t port);
// Look up an I/O port map entry by type. Returns NULL if no entry found with
// the specified type.
Y_PUBLIC YPlatformPortMapEntry* YPlatformGetPortMapEntryByType(
    struct YPlatformState* platform, YPlatformPortMapEntryType entry_type);

// Read a byte from an I/O port by invoking the corresponding I/O port map
// entry's read_byte callback.
Y_PUBLIC uint8_t
YPlatformReadPortByte(struct YPlatformState* platform, uint16_t port);
// Write a byte to an I/O port by invoking the corresponding I/O port map
// entry's write_byte callback.
Y_PUBLIC void YPlatformWritePortByte(
    struct YPlatformState* platform, uint16_t port, uint8_t value);

// ============================================================================
// Execution control
// ============================================================================

// Result of running the platform.
typedef enum YPlatformRunStatus {
  // The machine is running normally. Also returned by YPlatformRun() when it
  // exhausts its tick budget without stopping.
  Y_PLATFORM_RUNNING = 0,
  // The instruction at CS:IP could not be fetched or executed.
  Y_PLATFORM_INVALID,
  // The CPU halted with interrupts disabled and no interrupt pending, so
  // nothing can ever wake it. This is how GLaBIOS signals a fatal error.
  Y_PLATFORM_HUNG,
} YPlatformRunStatus;

// ============================================================================
// Platform state
// ============================================================================

// Caller-provided runtime configuration.
typedef struct YPlatformConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger configuration, shared by the platform and every module it owns. If
  // NULL, logging is disabled. The configuration is owned by the caller and
  // must outlive the platform.
  //
  // The platform stamps each log with its tick count and the CPU's CS:IP
  // itself - see YLoggerSource - so a host only says where lines go and
  // which of them it wants.
  YLoggerConfig* logger_config;

  // Physical memory size in bytes. Must be between 64K and 640K.
  uint32_t physical_memory_size;

  // The video adapter installed in the machine. This also determines the
  // display type reported by the PPI's DIP switches, which is what the BIOS
  // branches on when it programs the adapter.
  YVideoAdapter video_adapter;

  // The number of floppy drives, 1 to 4, which the PPI's DIP switches report
  // to the BIOS. 0, the default, is taken as 1.
  uint8_t floppy_drives;

  // The machine's conventional memory, physical_memory_size bytes of it.
  // Required - YPlatformInit() fails without it.
  //
  // The caller owns the buffer and it must outlive the platform. Conventional
  // memory is plain storage, so the platform reads and writes it directly
  // rather than through a callback; regions that need a callback register one
  // themselves via YPlatformMemoryMapEntry.
  //
  // Addresses above physical_memory_size are not backed by this buffer and are
  // not an error: on the 8086 an access outside installed memory yields
  // garbage rather than faulting, and unmapped reads here return 0xFF.
  uint8_t* physical_memory;

  // The video adapter's memory, at least vram_size bytes for the adapter named
  // by video_adapter. Required - YPlatformInit() fails without it.
  //
  // Separate from physical_memory because video memory sits above conventional
  // memory in the address space, at 0xB0000 or 0xB8000. A host whose memory
  // buffer spans the whole megabyte can point this into it.
  //
  // The caller owns the buffer and it must outlive the platform.
  uint8_t* vram;

  // The font text modes are drawn in, for a display too narrow for the
  // adapter's own - see YVideoTextFont. All zeros, the default, keeps the
  // adapter's font. The caller owns the glyphs and they must outlive the
  // platform.
  YVideoTextFont video_text_font;

  // Callback invoked when the PC speaker's output changes. frequency_hz is the
  // square wave frequency the speaker should emit, or 0 to turn it off. May be
  // NULL, in which case the speaker is silent.
  //
  // The speaker sounds when PIT channel 2 is producing a tone and both PPI
  // port B bits 0 and 1 are set. This reports current state rather than a
  // stream of events - see YPITConfig.set_pc_speaker_frequency.
  //
  // A frequency deliberately cannot express everything the hardware does. On a
  // real PC the speaker line is the AND of channel 2's output and port B bit
  // 1, driving the cone directly, so turning the speaker off parks that line
  // at a constant level and the cone audibly settles. Neither that click nor
  // the digitized audio some software produces by toggling bit 1 directly can
  // be represented here.
  //
  // However, we're choosing this approach so that basic PC speaker audio will
  // work without requiring PIT to be perfectly cycle accurate.
  void (*set_pc_speaker_frequency)(
      struct YPlatformState* platform, uint32_t frequency_hz);

} YPlatformConfig;

Y_STATIC_VECTOR_TYPE(
    YPlatformMemoryMap, YPlatformMemoryMapEntry,
    Y_PLATFORM_MAX_MEMORY_MAP_ENTRIES)
Y_STATIC_VECTOR_TYPE(
    YPlatformPortMap, YPlatformPortMapEntry, Y_PLATFORM_MAX_PORT_MAP_ENTRIES)

// State of the platform.
enum {
  // The CPU clock, in cycles per second. 4.77MHz is the IBM PC/XT's 14.318MHz
  // crystal divided by three.
  Y_PLATFORM_CPU_CYCLES_PER_SECOND = 4772727,
  // Cycles per millisecond, near enough for the devices that want one.
  Y_PLATFORM_CYCLES_PER_MILLISECOND = Y_PLATFORM_CPU_CYCLES_PER_SECOND / 1000,
  // The PIT is clocked at 1.193MHz, the same crystal divided by twelve, which
  // is a quarter of the CPU clock.
  Y_PLATFORM_CYCLES_PER_PIT_TICK = 4,
  // How often to step the floppy controller's state machine. This is not a
  // clock ratio - the controller is a state machine here rather than a
  // modelled device - so it keeps the rate it had before cycles were counted,
  // to leave floppy timing where it was.
  Y_PLATFORM_CYCLES_PER_FDC_TICK = 20,
};

typedef struct YPlatformState {
  // Caller-provided runtime configuration. Filled in before YPlatformInit(),
  // which is where it is checked, and read-only afterwards.
  YPlatformConfig config;

  // Logger shared by the platform and every module it owns.
  YLogger logger;

  // CPU state, which holds its own configuration.
  YCPUState cpu;
  // Storage for the CPU's decode cache, handed to it through
  // YCPUConfig.decode_cache.
  YCPUDecodeCacheEntry cpu_decode_cache[Y_PLATFORM_DECODE_CACHE_ENTRIES];

  // PIC state.
  YPICState pic;

  // PIT state.
  YPITState pit;

  // PPI state.
  YPPIState ppi;

  // Keyboard state.
  YKeyboardState keyboard;

  // DMA controller runtime configuration.
  // DMA controller state.
  YDMAState dma;

  // FDC state.
  YFDCState fdc;

  // HDC state.
  YHDCState hdc;

  // Video state.
  YVideoState video;

  // Memory map.
  YPlatformMemoryMap memory_map;
  // Which memory map entry owns each page of the address space, or
  // Y_PLATFORM_MEMORY_PAGE_UNMAPPED / Y_PLATFORM_MEMORY_PAGE_STRADDLED.
  // Extended by each registration, which only happens while the machine is
  // being set up.
  uint8_t memory_page_map[Y_PLATFORM_NUM_MEMORY_PAGES];
  // I/O port map.
  YPlatformPortMap io_port_map;

  // How many CPU clock cycles have run, at 4.77MHz. Every device in the
  // machine is clocked from this, so that what the guest measures with the
  // timer matches how long it spent executing.
  uint32_t ticks;

  // Cycles counted towards each device's next tick but not yet used by it.
  uint32_t pit_cycles;
  uint32_t fdc_cycles;
  uint32_t keyboard_cycles;

  // Devices are driven from deadlines rather than clocked on every
  // instruction. The PIT alone would otherwise want ticking about 2.5 times
  // per instruction, three channels at a time, almost always to decrement a
  // counter nothing is watching.
  //
  // Instead each device is brought up to date only when it has something to
  // do, and the instruction path does one comparison against the earliest
  // deadline of any of them. Anything that reads a device's state has to bring
  // that device up to date first, which is what the YPlatformSync* functions
  // do.

  // Whether the DOS idle skip is on - see YPlatformSetDOSIdleSkip(), which is
  // its sole writer. Off after YPlatformInit().
  bool is_dos_idle_skip_enabled;
  // Set when the guest has said it has nothing to do, and cleared by
  // YPlatformRun() once it has skipped the idle time. Only ever set while the
  // idle skip is on.
  bool is_guest_idle;

  // Value of ticks when the devices were last brought up to date.
  uint32_t last_sync_ticks;
  // Value of ticks at which the earliest device deadline falls due. Compared
  // as a signed difference against ticks so that it stays correct when the
  // 32-bit cycle counter wraps.
  uint32_t next_event_ticks;

} YPlatformState;

// Initialize the platform state. Returns true if the platform state was
// successfully initialized, or false if:
//   - The physical memory size is not between 64K and 640K.
//   - No physical memory buffer was provided.
//   - No video memory buffer was provided.
// The caller zero-initializes the YPlatformState and fills in the fields of its
// config first. Nothing here zeroes anything: every module's state is a field
// of this one, so the caller's single zeroing covers all of them, and each
// module's init sets only what wants a non-zero value.
Y_PUBLIC bool YPlatformInit(YPlatformState* platform);

// Raise a hardware interrupt to the CPU via the PIC. Returns true if the
// IRQ was successfully raised, or false if the IRQ number is invalid.
Y_PUBLIC bool YPlatformRaiseIRQ(YPlatformState* platform, uint8_t irq);

// Execute one instruction, and bring any device whose deadline has come due up
// to date with the cycles it took.
//
// Note that this is an instruction, not a cycle: a tick runs the instruction at
// CS:IP to completion and charges the clock whatever that cost, so a REP string
// instruction can retire as a single tick worth thousands of cycles. A halted
// CPU retires no instruction but still advances the clock, so that whatever is
// meant to wake it can.
//
// Exactly one, which is what makes this the entry point to step a machine
// with: a host that wants to stop somewhere drives this in a loop and inspects
// the CPU itself. YPlatformRun() batches instructions into a tick instead.
//
// Returns Y_PLATFORM_RUNNING if the machine should keep running.
Y_PUBLIC YPlatformRunStatus YPlatformTick(YPlatformState* platform);

// Run up to max_ticks cycles of the platform, stopping early if a tick returns
// anything other than Y_PLATFORM_RUNNING. Returns the status of the tick that
// stopped the run, or Y_PLATFORM_RUNNING if the full budget was consumed.
//
// Unlike YPlatformTick(), a tick here may run several instructions back to
// back, never past the point where a device is due to be serviced or something
// else needs acting on at an instruction boundary. Nothing observable moves:
// every device still sees every cycle and an interrupt is still delivered
// where it would have been. What is saved is the per-tick work.
//
// max_cycles must be well under 2^31. Progress is measured as an unsigned
// difference from the tick count this call started at, which is what keeps it
// correct across the 32-bit cycle counter wrapping - every 15 minutes of
// emulated time - but that also means a budget approaching 2^32 can never be
// reached, because the difference wraps to zero first. A budget of a display
// frame or so, which is what a host driving the machine in real time passes,
// is nowhere near this.
Y_PUBLIC YPlatformRunStatus
YPlatformRun(YPlatformState* platform, uint32_t max_cycles);

// Turn on or off skipping emulated time when the guest says it has nothing to
// do. Off after YPlatformInit(), and may be switched at any point after it.
//
// MS-DOS waits for a keystroke by polling, not by halting, so an idle command
// prompt costs exactly as much to emulate as a running program - about 390,000
// guest instructions a second, all of them the same loop. It does however
// issue INT 28h, the documented DOS idle interrupt, on each pass: measured at
// the MS-DOS 3.30 prompt, 783 times a second.
//
// With this on, that interrupt makes YPlatformRun() advance the clock straight
// to whichever comes first, the next device deadline or the end of the budget
// it was given, instead of executing the loop until it gets there. The guest's
// own handler still runs; only the waiting is skipped.
//
// Time is advanced rather than discarded, so every device still sees every
// cycle and the guest's timer tick count is unchanged. What does change is
// that a program timing a loop from inside its own INT 28h handler would see
// time jump, which is why this is opt in.
//
// A setter rather than a config field, because the skip is a callback on the
// CPU's interrupt path as well as a flag, and a host may want it only once DOS
// has reached its prompt, which leaves everything before that exactly as it
// would be without it.
Y_PUBLIC void YPlatformSetDOSIdleSkip(YPlatformState* platform, bool enable);

// Bring every device up to date with the cycles that have run so far.
//
// Devices are driven from deadlines rather than clocked on every instruction,
// so between deadlines their state lags the CPU. Everything inside the
// platform that reads device state does this for itself, so a caller only
// needs it before inspecting a device directly - most usefully before
// YVideoRender(), so that the frame reflects where the CRT beam has actually
// reached.
Y_PUBLIC void YPlatformSync(YPlatformState* platform);

#endif  // YAX86_PLATFORM_PUBLIC_H


// ==============================================================================
// src/platform/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/platform/platform.c start
// ==============================================================================

#line 1 "./src/platform/platform.c"
#include "bios.h"
#include "pic.h"
#include "ppi.h"

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

#define Y_PLATFORM_ERROR(...) \
  Y_LOG(&platform->logger, &YPlatformLogModule, Y_LOG_LEVEL_ERROR, __VA_ARGS__)
#define Y_PLATFORM_WARN(...) \
  Y_LOG(&platform->logger, &YPlatformLogModule, Y_LOG_LEVEL_WARN, __VA_ARGS__)
#define Y_PLATFORM_INFO(...) \
  Y_LOG(&platform->logger, &YPlatformLogModule, Y_LOG_LEVEL_INFO, __VA_ARGS__)
#define Y_PLATFORM_DEBUG(...) \
  Y_LOG(&platform->logger, &YPlatformLogModule, Y_LOG_LEVEL_DEBUG, __VA_ARGS__)

// The memory and port accessors' logs, out of line. Built where they are
// logged, a log's fields take room in the accessor around them whether or not
// they are ever written, and the accessors are on the hot path.
Y_FILE_PRIVATE Y_NOINLINE void YPlatformLogUnmappedMemoryRead(
    YPlatformState* platform, uint32_t address) {
  Y_PLATFORM_DEBUG("unmapped_memory_read", Y_LOG_HEX20("address", address));
}

Y_FILE_PRIVATE Y_NOINLINE void YPlatformLogDiscardedMemoryWrite(
    YPlatformState* platform, uint32_t address, uint8_t value) {
  Y_PLATFORM_DEBUG(
      "discarded_memory_write", Y_LOG_HEX20("address", address),
      Y_LOG_HEX8("value", value));
}

Y_FILE_PRIVATE Y_NOINLINE void YPlatformLogUnmappedPortRead(
    YPlatformState* platform, uint16_t port) {
  Y_PLATFORM_WARN("unmapped_port_read", Y_LOG_HEX16("port", port));
}

Y_FILE_PRIVATE Y_NOINLINE void YPlatformLogUnmappedPortWrite(
    YPlatformState* platform, uint16_t port, uint8_t value) {
  Y_PLATFORM_WARN(
      "unmapped_port_write", Y_LOG_HEX16("port", port),
      Y_LOG_HEX8("value", value));
}

// How long until the next device needs attention, in CPU cycles from now.
Y_FILE_PRIVATE uint32_t YPlatformCyclesUntilNextEvent(
    const YPlatformState* platform, uint32_t max_cycles);

// Hand the CPU conventional memory to index directly, or take it away again.
Y_FILE_PRIVATE void YPlatformUpdateDirectDataWindow(YPlatformState* platform);

enum {
  // Never let a deadline sit further out than this, so that a machine in which
  // nothing is scheduled still comes back regularly.
  //
  // Note that this is not a bound on how many cycles accumulate between syncs.
  // A deadline is only tested between instructions, and a REP string
  // instruction retires as a single tick charging thousands of cycles, so a
  // sync interval routinely runs past it - measured at 27,844 cycles, or
  // 5.8ms, over a DOS boot. Anything relying on a bound has to impose its own;
  // see Y_PLATFORM_MAX_IDLE_SKIP_CYCLES.
  Y_PLATFORM_MAX_EVENT_INTERVAL = Y_PLATFORM_CYCLES_PER_MILLISECOND,

  // The furthest an idle skip may move the clock in one go.
  //
  // A skip is otherwise bounded only by the budget its caller passed to
  // YPlatformRun(), which nothing checks. Every device's catch-up arithmetic in
  // YPlatformSync() is 32-bit and accumulates into a counter holding a
  // remainder, so a jump approaching 2^32 would overflow it, and the keyboard
  // is caught up a millisecond per iteration, so it would also spend ~900,000
  // of them in a single call. A tenth of a second is far more than any sane
  // budget and far less than either limit.
  Y_PLATFORM_MAX_IDLE_SKIP_CYCLES = Y_PLATFORM_CPU_CYCLES_PER_SECOND / 10,
};

// Record a newly registered entry in the page index.
//
// Takes the entry's index rather than the entry itself because the index is
// what goes into the map - the entry is derivable from it, and not the other
// way round.
//
// Only the pages the entry itself touches can change. Entries may not overlap,
// so nothing already in the index can have a share of one of them, and a
// registration never has to look at the rest of the map. Building the whole
// index therefore costs one pass over the address space between them, rather
// than one pass per entry.
Y_FILE_PRIVATE void YPlatformUpdateMemoryPageMapForEntry(
    YPlatformState* platform, uint8_t entry_index) {
  const YPlatformMemoryMapEntry* entry =
      YPlatformMemoryMapGet(&platform->memory_map, entry_index);
  for (uint32_t page = entry->start >> Y_PLATFORM_MEMORY_PAGE_SHIFT,
                last_page = entry->end >> Y_PLATFORM_MEMORY_PAGE_SHIFT;
       page <= last_page; ++page) {
    const uint32_t page_start = page << Y_PLATFORM_MEMORY_PAGE_SHIFT;
    const uint32_t page_end = page_start + Y_PLATFORM_MEMORY_PAGE_SIZE - 1;
    platform->memory_page_map[page] =
        entry->start <= page_start && entry->end >= page_end
            ? entry_index
            : Y_PLATFORM_MEMORY_PAGE_STRADDLED;
  }
}

// Register a memory map entry in the platform state. Returns true if the entry
// was successfully registered, or false if:
//   - The entry's memory region does not lie within the address space.
//   - There already exists a memory map entry with the same type.
//   - The new entry's memory region overlaps with an existing entry.
//   - The number of memory map entries would exceed
//   Y_PLATFORM_MAX_MEMORY_MAP_ENTRIES.
// Records a memory map entry without telling anything that the map has
// changed. What the CPU derives from the map - its two windows and its decode
// cache - is left to the caller, so that a run of registrations pays for one
// recompute rather than one each.
Y_PUBLIC bool YPlatformRegisterMemoryMapEntry(
    YPlatformState* platform, const YPlatformMemoryMapEntry* entry) {
  // The index has a slot per page of the address space and none above it, so
  // an entry reaching past the top could not be recorded in it. Rejecting one
  // here is what lets the lookup treat the index as the whole answer.
  if (entry->end >= Y_PLATFORM_MEMORY_ADDRESS_SPACE_SIZE ||
      entry->start > entry->end) {
    return false;
  }
  if (YPlatformMemoryMapLength(&platform->memory_map) >=
      Y_PLATFORM_MAX_MEMORY_MAP_ENTRIES) {
    return false;
  }
  for (uint8_t i = 0; i < YPlatformMemoryMapLength(&platform->memory_map);
       ++i) {
    YPlatformMemoryMapEntry* existing_entry =
        YPlatformMemoryMapGet(&platform->memory_map, i);
    if (existing_entry->entry_type == entry->entry_type) {
      return false;
    }
    if (!(existing_entry->start > entry->end ||
          entry->start > existing_entry->end)) {
      return false;
    }
  }
  if (!YPlatformMemoryMapAppend(&platform->memory_map, entry)) {
    return false;
  }
  YPlatformUpdateMemoryPageMapForEntry(
      platform, YPlatformMemoryMapLength(&platform->memory_map) - 1);
  return true;
}

Y_PUBLIC void YPlatformUpdateAfterMemoryMapChange(YPlatformState* platform) {
  // The data window is whichever entry covers address 0, which a registration
  // may have just become.
  YPlatformUpdateDirectDataWindow(platform);
  // A decode taken before a region was mapped came from open bus. No page
  // generation moved, because nothing was written - what the bytes at those
  // addresses mean has changed.
  YCPUInvalidateDecodeCache(&platform->cpu);
}

// What the page index has to say about an address: the entry covering the whole
// of its page, Y_PLATFORM_MEMORY_PAGE_UNMAPPED where no entry covers it, or
// Y_PLATFORM_MEMORY_PAGE_STRADDLED where more than one entry has a share of the
// page and the caller has to walk the map. An address above the address space
// has no page, and no entry may reach it, so it is unmapped by definition.
//
// Inline rather than a function of its own: the body is a compare, a shift and
// a load, where a call on a core with no cheap way to do any of it is a push, a
// branch, a pop and a return.
Y_FILE_PRIVATE inline uint8_t YPlatformGetMemoryPageMapIndex(
    const YPlatformState* platform, uint32_t address) {
  return address < Y_PLATFORM_MEMORY_ADDRESS_SPACE_SIZE
             ? platform
                   ->memory_page_map[address >> Y_PLATFORM_MEMORY_PAGE_SHIFT]
             : Y_PLATFORM_MEMORY_PAGE_UNMAPPED;
}

// Look up the memory region corresponding to an address. Returns NULL if the
// address is not mapped to a known memory region.
Y_PUBLIC Y_HOT YPlatformMemoryMapEntry* YPlatformGetMemoryMapEntryForAddress(
    YPlatformState* platform, uint32_t address) {
  const uint8_t index = YPlatformGetMemoryPageMapIndex(platform, address);
  if (index < Y_PLATFORM_MAX_MEMORY_MAP_ENTRIES) {
    return YPlatformMemoryMapGet(&platform->memory_map, index);
  }
  if (index == Y_PLATFORM_MEMORY_PAGE_UNMAPPED) {
    return NULL;
  }
  // A page more than one entry has a share of, which the index cannot answer.
  for (uint8_t i = 0; i < YPlatformMemoryMapLength(&platform->memory_map);
       ++i) {
    YPlatformMemoryMapEntry* entry =
        YPlatformMemoryMapGet(&platform->memory_map, i);
    if (address >= entry->start && address <= entry->end) {
      return entry;
    }
  }
  return NULL;
}

// Look up a memory region by type. Returns NULL if no region found with the
// specified type.
Y_PUBLIC Y_HOT YPlatformMemoryMapEntry* YPlatformGetMemoryMapEntryByType(
    YPlatformState* platform, uint8_t entry_type) {
  for (uint8_t i = 0; i < YPlatformMemoryMapLength(&platform->memory_map);
       ++i) {
    YPlatformMemoryMapEntry* entry =
        YPlatformMemoryMapGet(&platform->memory_map, i);
    if (entry->entry_type == entry_type) {
      return entry;
    }
  }
  return NULL;
}

// Read a byte from a logical memory address.
Y_PUBLIC Y_HOT uint8_t
YPlatformReadMemoryByte(YPlatformState* platform, uint32_t address) {
  YPlatformMemoryMapEntry* entry =
      YPlatformGetMemoryMapEntryForAddress(platform, address);
  if (entry) {
    // Plain storage, which is what every region except video memory is. Going
    // straight to the buffer saves an indirect call on the hottest path in the
    // emulator: every byte of every instruction is fetched through here.
    if (entry->read_data) {
      return entry->read_data[address - entry->start];
    }
    if (entry->read_byte_fn) {
      return entry->read_byte_fn(entry, address - entry->start);
    }
  }
  // Logged at debug rather than warning level: scanning unmapped memory is
  // normal on a PC/XT. GLaBIOS reads every byte of 0xF6000-0xF7FFF looking
  // for option ROMs, for instance.
  YPlatformLogUnmappedMemoryRead(platform, address);
  return 0xFF;
}

// Read a word from a logical memory address.
Y_PUBLIC uint16_t
YPlatformReadMemoryWord(YPlatformState* platform, uint32_t address) {
  uint8_t low_byte = YPlatformReadMemoryByte(platform, address);
  uint8_t high_byte = YPlatformReadMemoryByte(platform, address + 1);
  return (high_byte << 8) | low_byte;
}

// Write a byte to a logical memory address.
Y_PUBLIC Y_HOT void YPlatformWriteMemoryByte(
    YPlatformState* platform, uint32_t address, uint8_t value) {
  // Writes that do not come from the CPU arrive here, and can land on bytes it
  // has already decoded. DMA is the one that matters: DOS loads itself over
  // the boot sector that way.
  YCPUNotifyMemoryWrite(&platform->cpu, address);
  YPlatformMemoryMapEntry* entry =
      YPlatformGetMemoryMapEntryForAddress(platform, address);
  if (entry) {
    if (entry->write_data) {
      entry->write_data[address - entry->start] = value;
      return;
    }
    if (entry->write_byte_fn) {
      entry->write_byte_fn(entry, address - entry->start, value);
      return;
    }
  }
  // Either unmapped, or a read-only region such as a ROM. Both discard the
  // write, and both are logged.
  YPlatformLogDiscardedMemoryWrite(platform, address, value);
}

// Register an I/O port map entry in the platform state. Returns true if the
// entry was successfully registered, or false if:
//   - There already exists an I/O port map entry with the same type.
//   - The new entry's I/O port range overlaps with an existing entry.
Y_PUBLIC bool YPlatformRegisterPortMapEntry(
    YPlatformState* platform, const YPlatformPortMapEntry* entry) {
  if (YPlatformPortMapLength(&platform->io_port_map) >=
      Y_PLATFORM_MAX_PORT_MAP_ENTRIES) {
    return false;
  }
  for (uint8_t i = 0; i < YPlatformPortMapLength(&platform->io_port_map); ++i) {
    YPlatformPortMapEntry* existing_entry =
        YPlatformPortMapGet(&platform->io_port_map, i);
    if (existing_entry->entry_type == entry->entry_type) {
      return false;
    }
    if (!(existing_entry->start > entry->end ||
          entry->start > existing_entry->end)) {
      return false;
    }
  }
  return YPlatformPortMapAppend(&platform->io_port_map, entry);
}

// Look up the I/O port map entry corresponding to a port. Returns NULL if the
// port is not mapped to a known I/O port map entry.
Y_PUBLIC YPlatformPortMapEntry* YPlatformGetPortMapEntryForPort(
    YPlatformState* platform, uint16_t port) {
  for (uint8_t i = 0; i < YPlatformPortMapLength(&platform->io_port_map); ++i) {
    YPlatformPortMapEntry* entry =
        YPlatformPortMapGet(&platform->io_port_map, i);
    if (port >= entry->start && port <= entry->end) {
      return entry;
    }
  }
  return NULL;
}
// Look up an I/O port map entry by type. Returns NULL if no entry found with
// the specified type.
Y_PUBLIC Y_HOT YPlatformPortMapEntry* YPlatformGetPortMapEntryByType(
    YPlatformState* platform, YPlatformPortMapEntryType entry_type) {
  for (uint8_t i = 0; i < YPlatformPortMapLength(&platform->io_port_map); ++i) {
    YPlatformPortMapEntry* entry =
        YPlatformPortMapGet(&platform->io_port_map, i);
    if (entry->entry_type == entry_type) {
      return entry;
    }
  }
  return NULL;
}

// Read a byte from an I/O port by invoking the corresponding I/O port map
// entry's read_byte callback.
Y_PUBLIC Y_HOT uint8_t
YPlatformReadPortByte(YPlatformState* platform, uint16_t port) {
  YPlatformPortMapEntry* entry =
      YPlatformGetPortMapEntryForPort(platform, port);
  if (!entry || !entry->read_byte) {
    // Unlike unmapped memory, an unmapped port usually means a device is
    // missing from the port map, so this stays at warning level.
    YPlatformLogUnmappedPortRead(platform, port);
    return 0xFF;
  }
  return entry->read_byte(entry, port);
}

// Write a byte to an I/O port by invoking the corresponding I/O port map
// entry's write_byte callback.
Y_PUBLIC Y_HOT void YPlatformWritePortByte(
    YPlatformState* platform, uint16_t port, uint8_t value) {
  YPlatformPortMapEntry* entry =
      YPlatformGetPortMapEntryForPort(platform, port);
  if (!entry || !entry->write_byte) {
    YPlatformLogUnmappedPortWrite(platform, port, value);
    return;
  }
  entry->write_byte(entry, port, value);
}

// ============================================================================
// Callbacks for CPU module
// ============================================================================

Y_FILE_PRIVATE uint8_t
YPlatformCPUCallbackReadMemoryByte(YCPUState* cpu, uint32_t address) {
  return YPlatformReadMemoryByte((YPlatformState*)cpu->config.context, address);
}

Y_FILE_PRIVATE void YPlatformCPUCallbackWriteMemoryByte(
    YCPUState* cpu, uint32_t address, uint8_t value) {
  YPlatformWriteMemoryByte(
      (YPlatformState*)cpu->config.context, address, value);
}

Y_FILE_PRIVATE uint8_t
YPlatformCPUCallbackReadPortByte(YCPUState* cpu, uint16_t port) {
  return YPlatformReadPortByte((YPlatformState*)cpu->config.context, port);
}

Y_FILE_PRIVATE void YPlatformCPUCallbackWritePortByte(
    YCPUState* cpu, uint16_t port, uint8_t value) {
  YPlatformWritePortByte((YPlatformState*)cpu->config.context, port, value);
}

// ============================================================================
// Callbacks for 8259 PIC module
// ============================================================================

Y_FILE_PRIVATE uint8_t
YPlatformPICCallbackReadPortByte(YPlatformPortMapEntry* entry, uint16_t port) {
  return YPICReadPort((YPICState*)entry->context, port);
}

Y_FILE_PRIVATE Y_HOT void YPlatformPICCallbackWritePortByte(
    YPlatformPortMapEntry* entry, uint16_t port, uint8_t value) {
  YPICWritePort((YPICState*)entry->context, port, value);
}

Y_FILE_PRIVATE void YPlatformPICCallbackRaiseIRQ0(void* context) {
  YPlatformState* platform = (YPlatformState*)context;
  YPlatformRaiseIRQ(platform, 0);
}

// ============================================================================
// Callbacks for 8253 PIT module
// ============================================================================

Y_FILE_PRIVATE Y_HOT uint8_t
YPlatformPITCallbackReadPortByte(YPlatformPortMapEntry* entry, uint16_t port) {
  // A guest timing loop reads the counter expecting it to have moved, so the
  // PIT has to be caught up before it is read.
  YPlatformState* platform = (YPlatformState*)entry->context;
  YPlatformSync(platform);
  return YPITReadPort(&platform->pit, port);
}

Y_FILE_PRIVATE void YPlatformPITCallbackWritePortByte(
    YPlatformPortMapEntry* entry, uint16_t port, uint8_t value) {
  // Syncing first applies the cycles that ran under the old configuration;
  // syncing again afterwards reschedules against the new one.
  YPlatformState* platform = (YPlatformState*)entry->context;
  YPlatformSync(platform);
  YPITWritePort(&platform->pit, port, value);
  YPlatformSync(platform);
}

Y_FILE_PRIVATE void YPlatformPITCallbackSetPCSpeakerFrequency(
    void* context, uint32_t frequency_hz) {
  YPlatformState* platform = (YPlatformState*)context;
  YPPISetPCSpeakerFrequencyFromPIT(&platform->ppi, frequency_hz);
}

// ============================================================================
// Callbacks for 8255 PPI module
// ============================================================================

Y_FILE_PRIVATE uint8_t
YPlatformPPICallbackReadPortByte(YPlatformPortMapEntry* entry, uint16_t port) {
  YPlatformState* platform = (YPlatformState*)entry->context;
  YPlatformSync(platform);
  return YPPIReadPort(&platform->ppi, port);
}

Y_FILE_PRIVATE void YPlatformPPICallbackWritePortByte(
    YPlatformPortMapEntry* entry, uint16_t port, uint8_t value) {
  // Port B gates the speaker against PIT channel 2, so what the guest hears
  // depends on the channel's output state being current.
  YPlatformState* platform = (YPlatformState*)entry->context;
  YPlatformSync(platform);
  YPPIWritePort(&platform->ppi, port, value);
  YPlatformSync(platform);
}

Y_FILE_PRIVATE void YPlatformPPICallbackSetKeyboardControl(
    void* context, bool keyboard_enable_clear, bool keyboard_clock_low) {
  YPlatformState* platform = (YPlatformState*)context;
  YKeyboardHandleControl(
      &platform->keyboard, keyboard_enable_clear, keyboard_clock_low);
}

Y_FILE_PRIVATE void YPlatformPPICallbackSetPCSpeakerFrequency(
    void* context, uint32_t frequency_hz) {
  YPlatformState* platform = (YPlatformState*)context;
  if (platform->config.set_pc_speaker_frequency) {
    platform->config.set_pc_speaker_frequency(platform, frequency_hz);
  }
}

// ============================================================================
// Callbacks for Keyboard module
// ============================================================================

Y_FILE_PRIVATE void YPlatformKeyboardCallbackRaiseIRQ1(void* context) {
  YPlatformState* platform = (YPlatformState*)context;
  YPlatformRaiseIRQ(platform, 1);
}

Y_FILE_PRIVATE void YPlatformKeyboardCallbackSendScancode(
    void* context, uint8_t scancode) {
  YPlatformState* platform = (YPlatformState*)context;
  YPPISetScancode(&platform->ppi, scancode);
}

// ============================================================================
// Callbacks for uPD765 FDC module
// ============================================================================

enum {
  Y_PLATFORM_DMA_CHANNEL_FLOPPY = 2,
};

Y_FILE_PRIVATE void YPlatformFDCCallbackRaiseIRQ6(void* context) {
  YPlatformState* platform = (YPlatformState*)context;
  YPlatformRaiseIRQ(platform, 6);
}

Y_FILE_PRIVATE Y_HOT void YPlatformFDCCallbackRequestDMA(void* context) {
  YPlatformState* platform = (YPlatformState*)context;
  YDMATransferByte(&platform->dma, Y_PLATFORM_DMA_CHANNEL_FLOPPY);
}

Y_FILE_PRIVATE uint8_t
YPlatformFDCCallbackReadPortByte(YPlatformPortMapEntry* entry, uint16_t port) {
  YPlatformState* platform = (YPlatformState*)entry->context;
  YPlatformSync(platform);
  return YFDCReadPort(&platform->fdc, port);
}

Y_FILE_PRIVATE void YPlatformFDCCallbackWritePortByte(
    YPlatformPortMapEntry* entry, uint16_t port, uint8_t value) {
  // A write can start a command, which is what puts the controller into the
  // execution phase the scheduler watches for.
  YPlatformState* platform = (YPlatformState*)entry->context;
  YPlatformSync(platform);
  YFDCWritePort(&platform->fdc, port, value);
  YPlatformSync(platform);
}

// ============================================================================
// Callbacks for DMA module
// ============================================================================

Y_FILE_PRIVATE uint8_t
YPlatformDMACallbackReadMemoryByte(void* context, uint32_t address) {
  YPlatformState* platform = (YPlatformState*)context;
  return YPlatformReadMemoryByte(platform, address);
}

Y_FILE_PRIVATE void YPlatformDMACallbackWriteMemoryByte(
    void* context, uint32_t address, uint8_t value) {
  YPlatformState* platform = (YPlatformState*)context;
  YPlatformWriteMemoryByte(platform, address, value);
}

Y_FILE_PRIVATE uint8_t
YPlatformDMACallbackReadDeviceByte(void* context, uint8_t channel) {
  YPlatformState* platform = (YPlatformState*)context;
  switch (channel) {
    case Y_PLATFORM_DMA_CHANNEL_FLOPPY:
      return YFDCReadPort(&platform->fdc, Y_FDC_PORT_DATA);
    default:
      return 0xFF;
  }
}

Y_FILE_PRIVATE void YPlatformDMACallbackWriteDeviceByte(
    void* context, uint8_t channel, uint8_t value) {
  YPlatformState* platform = (YPlatformState*)context;
  switch (channel) {
    case Y_PLATFORM_DMA_CHANNEL_FLOPPY:
      YFDCWritePort(&platform->fdc, Y_FDC_PORT_DATA, value);
      break;
    default:
      break;
  }
}

Y_FILE_PRIVATE void YPlatformDMACallbackOnTerminalCount(
    void* context, uint8_t channel) {
  YPlatformState* platform = (YPlatformState*)context;
  switch (channel) {
    case Y_PLATFORM_DMA_CHANNEL_FLOPPY:
      YFDCHandleTC(&platform->fdc);
      break;
    default:
      break;
  }
}

Y_FILE_PRIVATE uint8_t
YPlatformDMACallbackReadPortByte(YPlatformPortMapEntry* entry, uint16_t port) {
  return YDMAReadPort((YDMAState*)entry->context, port);
}

Y_FILE_PRIVATE void YPlatformDMACallbackWritePortByte(
    YPlatformPortMapEntry* entry, uint16_t port, uint8_t value) {
  YDMAWritePort((YDMAState*)entry->context, port, value);
}

// ============================================================================
// Callbacks for Video module
// ============================================================================

Y_FILE_PRIVATE Y_HOT uint8_t YPlatformVideoCallbackReadPortByte(
    YPlatformPortMapEntry* entry, uint16_t port) {
  // The status port reports where the CRT beam is, which is only meaningful
  // once the beam has been advanced to now. Guests poll this to wait for
  // retrace.
  YPlatformState* platform = (YPlatformState*)entry->context;
  YPlatformSync(platform);
  return YVideoReadPort(&platform->video, port);
}

Y_FILE_PRIVATE void YPlatformVideoCallbackWritePortByte(
    YPlatformPortMapEntry* entry, uint16_t port, uint8_t value) {
  YPlatformState* platform = (YPlatformState*)entry->context;
  YPlatformSync(platform);
  YVideoWritePort(&platform->video, port, value);
}

Y_FILE_PRIVATE void YPlatformVideoCallbackWriteVRAMByte(
    YPlatformMemoryMapEntry* entry, uint32_t address, uint8_t value) {
  YVideoWriteVRAM((YVideoState*)entry->context, address, value);
}

// ============================================================================
// Callbacks for HDC module
// ============================================================================

Y_FILE_PRIVATE uint8_t
YPlatformHDCCallbackReadPortByte(YPlatformPortMapEntry* entry, uint16_t port) {
  return YHDCReadPort((YHDCState*)entry->context, port);
}

Y_FILE_PRIVATE void YPlatformHDCCallbackWritePortByte(
    YPlatformPortMapEntry* entry, uint16_t port, uint8_t value) {
  YHDCWritePort((YHDCState*)entry->context, port, value);
}

// ============================================================================
// Initialization
// ============================================================================

Y_FILE_PRIVATE void YPlatformInitBIOS(YPlatformState* platform) {
  uint32_t bios_size = YBIOSGetROMSize();
  YPlatformMemoryMapEntry bios_rom = {
      .context = NULL,
      .entry_type = Y_PLATFORM_MEMORY_MAP_ENTRY_BIOS_ROM,
      .start = Y_BIOS_ROM_START_ADDRESS,
      .end = Y_BIOS_ROM_START_ADDRESS + bios_size - 1,
      // The ROM image is a constant array in the library, so it is read
      // directly. write_data is left NULL because the BIOS ROM is read-only.
      .read_data = YBIOSGetROMData(),
  };
  YPlatformRegisterMemoryMapEntry(platform, &bios_rom);
}

// Runs the CPU's interrupt acknowledge cycle against the PIC. This is the
// only path by which an external interrupt reaches the CPU, and the PIC marks
// the interrupt in service as part of it - so a vector is never produced
// unless the CPU is taking it right now.
Y_FILE_PRIVATE bool YPlatformCPUCallbackAcknowledgeInterrupt(
    YCPUState* cpu, uint8_t* vector) {
  YPlatformState* platform = (YPlatformState*)cpu->config.context;
  const uint8_t interrupt_vector = YPICGetPendingInterrupt(&platform->pic);
  if (interrupt_vector == Y_PIC_NO_PENDING_INTERRUPT) {
    return false;
  } else {
    *vector = interrupt_vector;
    return true;
  }
}

enum {
  // The DOS idle interrupt. MS-DOS issues it from the loops in which it waits
  // for input, to tell anything listening that the machine has nothing to do.
  Y_PLATFORM_DOS_IDLE_INTERRUPT = 0x28,
};

// Notes the guest declaring itself idle. Nothing is serviced here - the guest's
// own handler runs as usual - so this only ever reports the interrupt onwards.
//
// Only installed while the idle skip is enabled, so a machine without it pays
// nothing per interrupt. YPlatformSetDOSIdleSkip() is what installs it.
Y_FILE_PRIVATE Y_HOT YCPUInterruptHandlerResult
YPlatformCPUCallbackHandleInterrupt(YCPUState* cpu, uint8_t interrupt_number) {
  if (interrupt_number == Y_PLATFORM_DOS_IDLE_INTERRUPT) {
    YPlatformState* platform = (YPlatformState*)cpu->config.context;
    platform->is_guest_idle = true;
  }
  return Y_CPU_INTERRUPT_HANDLER_UNHANDLED;
}

// Hands the CPU a direct window for instruction fetch.
//
// Declines wherever a read has to be observed or computed rather than loaded:
// a device region, unmapped memory, or a page shared by two entries.
Y_FILE_PRIVATE void YPlatformCPUCallbackGetInstructionFetchWindow(
    YCPUState* cpu, uint32_t address) {
  YCPUInstructionFetchWindow* window = &cpu->instruction_fetch_window;
  window->data = NULL;
  YPlatformState* platform = (YPlatformState*)cpu->config.context;
  // Anything that is not a single entry's page - unmapped, or shared by more
  // than one entry - has no window to hand out.
  const uint8_t index = YPlatformGetMemoryPageMapIndex(platform, address);
  if (index >= Y_PLATFORM_MAX_MEMORY_MAP_ENTRIES) {
    return;
  }
  YPlatformMemoryMapEntry* entry =
      YPlatformMemoryMapGet(&platform->memory_map, index);
  if (entry->read_data == NULL) {
    return;
  }
  // The whole region, not the tail of it from this address, so that a jump
  // backwards within the region still lands inside the window.
  window->data = entry->read_data;
  window->start = entry->start;
  window->end = entry->end + 1;
}

Y_FILE_PRIVATE void YPlatformInitCPU(YPlatformState* platform) {
  platform->cpu.config.context = platform;
  platform->cpu.config.logger = &platform->logger;
  platform->cpu.config.read_memory_byte = YPlatformCPUCallbackReadMemoryByte;
  platform->cpu.config.get_instruction_fetch_window =
      YPlatformCPUCallbackGetInstructionFetchWindow;
  platform->cpu.config.write_memory_byte = YPlatformCPUCallbackWriteMemoryByte;
  platform->cpu.config.acknowledge_interrupt =
      YPlatformCPUCallbackAcknowledgeInterrupt;
  // Points into the PIC's own state, so the CPU sees a change the moment the
  // PIC makes one. YPICInit() below runs after this, which is fine - the
  // pointer is to storage that already exists, not to a value read now.
  platform->cpu.config.interrupt_request_hint =
      &platform->pic.has_unmasked_request;
  platform->cpu.config.decode_cache = platform->cpu_decode_cache;
  platform->cpu.config.decode_cache_num_entries =
      Y_PLATFORM_DECODE_CACHE_ENTRIES;
  platform->cpu.config.read_port = YPlatformCPUCallbackReadPortByte;
  platform->cpu.config.write_port = YPlatformCPUCallbackWritePortByte;
  YCPUInit(&platform->cpu);

  // Initialize CPU registers.
  // CS:IP points to the BIOS entry point at 0xFFFF0.
  platform->cpu.registers[Y_CPU_CS] = 0xF000;
  platform->cpu.registers[Y_CPU_IP] = 0xFFF0;
  platform->cpu.registers[Y_CPU_DS] = 0x0000;
  platform->cpu.registers[Y_CPU_SS] = 0x0000;
  platform->cpu.registers[Y_CPU_ES] = 0x0000;
  platform->cpu.registers[Y_CPU_SP] = 0xFFFE;
}

Y_FILE_PRIVATE void YPlatformInitMemoryMap(YPlatformState* platform) {
  YPlatformMemoryMapInit(&platform->memory_map);
  for (uint32_t page = 0; page < Y_PLATFORM_NUM_MEMORY_PAGES; ++page) {
    platform->memory_page_map[page] = Y_PLATFORM_MEMORY_PAGE_UNMAPPED;
  }
  YPlatformMemoryMapEntry conventional_memory = {
      .context = platform,
      .entry_type = Y_PLATFORM_MEMORY_MAP_ENTRY_CONVENTIONAL,
      .start = 0x0000,
      .end = platform->config.physical_memory_size - 1,
      // Conventional memory is the caller's buffer, accessed directly.
      .read_data = platform->config.physical_memory,
      .write_data = platform->config.physical_memory};
  YPlatformRegisterMemoryMapEntry(platform, &conventional_memory);
}

Y_FILE_PRIVATE void YPlatformInitPIC(YPlatformState* platform) {
  platform->pic.config.sp = false;
  platform->pic.config.logger = &platform->logger;
  YPICInit(&platform->pic);
  YPlatformPortMapEntry pic_entry = {
      .entry_type = Y_PLATFORM_PORT_MAP_ENTRY_PIC,
      .start = 0x20,
      .end = 0x21,
      .read_byte = YPlatformPICCallbackReadPortByte,
      .write_byte = YPlatformPICCallbackWritePortByte,
      .context = &platform->pic,
  };
  YPlatformRegisterPortMapEntry(platform, &pic_entry);
}

Y_FILE_PRIVATE void YPlatformInitPIT(YPlatformState* platform) {
  platform->pit.config.context = platform;
  platform->pit.config.logger = &platform->logger;
  platform->pit.config.raise_irq_0 = YPlatformPICCallbackRaiseIRQ0;
  platform->pit.config.set_pc_speaker_frequency =
      YPlatformPITCallbackSetPCSpeakerFrequency;
  YPITInit(&platform->pit);
  YPlatformPortMapEntry pit_entry = {
      .entry_type = Y_PLATFORM_PORT_MAP_ENTRY_PIT,
      .start = 0x40,
      .end = 0x43,
      .read_byte = YPlatformPITCallbackReadPortByte,
      .write_byte = YPlatformPITCallbackWritePortByte,
      .context = platform,
  };
  YPlatformRegisterPortMapEntry(platform, &pit_entry);
}

Y_FILE_PRIVATE void YPlatformInitPPI(YPlatformState* platform) {
  platform->ppi.config.context = platform;
  platform->ppi.config.logger = &platform->logger;
  platform->ppi.config.num_floppy_drives =
      platform->config.floppy_drives > 0 ? platform->config.floppy_drives : 1;
  platform->ppi.config.memory_size = Y_PPI_MEMORY_SIZE_256KB;
  // The DIP switches are what the BIOS branches on to decide which adapter to
  // program, so they have to agree with the adapter the platform registers.
  platform->ppi.config.display_mode =
      platform->config.video_adapter == Y_VIDEO_ADAPTER_CGA
          ? Y_PPI_DISPLAY_CGA_80X25
          : Y_PPI_DISPLAY_MDA;
  platform->ppi.config.fpu_installed = false;
  platform->ppi.config.set_pc_speaker_frequency =
      YPlatformPPICallbackSetPCSpeakerFrequency;
  platform->ppi.config.set_keyboard_control =
      YPlatformPPICallbackSetKeyboardControl;
  YPPIInit(&platform->ppi);
  YPlatformPortMapEntry ppi_entry = {
      .entry_type = Y_PLATFORM_PORT_MAP_ENTRY_PPI,
      .start = 0x60,
      .end = 0x63,
      .read_byte = YPlatformPPICallbackReadPortByte,
      .write_byte = YPlatformPPICallbackWritePortByte,
      .context = platform,
  };
  YPlatformRegisterPortMapEntry(platform, &ppi_entry);
}

Y_FILE_PRIVATE void YPlatformInitKeyboard(YPlatformState* platform) {
  platform->keyboard.config.context = platform;
  platform->keyboard.config.logger = &platform->logger;
  platform->keyboard.config.raise_irq1 = YPlatformKeyboardCallbackRaiseIRQ1;
  platform->keyboard.config.send_scancode =
      YPlatformKeyboardCallbackSendScancode;
  YKeyboardInit(&platform->keyboard);
}

Y_FILE_PRIVATE void YPlatformInitFDC(YPlatformState* platform) {
  platform->fdc.config.context = platform;
  platform->fdc.config.logger = &platform->logger;
  platform->fdc.config.raise_irq6 = YPlatformFDCCallbackRaiseIRQ6;
  platform->fdc.config.request_dma = YPlatformFDCCallbackRequestDMA;
  platform->fdc.config.read_image_byte = NULL;
  platform->fdc.config.write_image_byte = NULL;
  YFDCInit(&platform->fdc);
  YPlatformPortMapEntry fdc_entry = {
      .entry_type = (YPlatformPortMapEntryType)Y_PLATFORM_PORT_MAP_ENTRY_FDC,
      .start = 0x3F0,
      .end = 0x3F7,
      .read_byte = YPlatformFDCCallbackReadPortByte,
      .write_byte = YPlatformFDCCallbackWritePortByte,
      .context = platform,
  };
  YPlatformRegisterPortMapEntry(platform, &fdc_entry);
}

Y_FILE_PRIVATE void YPlatformInitHDC(YPlatformState* platform) {
  platform->hdc.config.context = platform;
  platform->hdc.config.logger = &platform->logger;
  YHDCInit(&platform->hdc);

  const uint32_t option_rom_size = YHDCGetOptionROMSize();
  YPlatformMemoryMapEntry option_rom_entry = {
      .context = &platform->hdc,
      .entry_type = Y_PLATFORM_MEMORY_MAP_ENTRY_HDC_OPTION_ROM,
      .start = Y_HDC_OPTION_ROM_START_ADDRESS,
      .end = Y_HDC_OPTION_ROM_START_ADDRESS + option_rom_size - 1,
      // As with the BIOS ROM, a constant array read directly. write_data is
      // left NULL because the option ROM is read-only.
      .read_data = YHDCGetOptionROMData(),
  };
  YPlatformRegisterMemoryMapEntry(platform, &option_rom_entry);

  YPlatformPortMapEntry port_entry = {
      .context = &platform->hdc,
      .entry_type = (YPlatformPortMapEntryType)Y_PLATFORM_PORT_MAP_ENTRY_HDC,
      .start = Y_HDC_PORT_BASE,
      .end = Y_HDC_PORT_BASE + Y_HDC_NUM_PORTS - 1,
      .read_byte = YPlatformHDCCallbackReadPortByte,
      .write_byte = YPlatformHDCCallbackWritePortByte,
  };
  YPlatformRegisterPortMapEntry(platform, &port_entry);
}

Y_FILE_PRIVATE void YPlatformInitDMA(YPlatformState* platform) {
  platform->dma.config.context = platform;
  platform->dma.config.logger = &platform->logger;
  platform->dma.config.read_memory_byte = YPlatformDMACallbackReadMemoryByte;
  platform->dma.config.write_memory_byte = YPlatformDMACallbackWriteMemoryByte;
  platform->dma.config.read_device_byte = YPlatformDMACallbackReadDeviceByte;
  platform->dma.config.write_device_byte = YPlatformDMACallbackWriteDeviceByte;
  platform->dma.config.on_terminal_count = YPlatformDMACallbackOnTerminalCount;
  YDMAInit(&platform->dma);
  YPlatformPortMapEntry dma_entry = {
      .entry_type = (YPlatformPortMapEntryType)Y_PLATFORM_PORT_MAP_ENTRY_DMA,
      .start = 0x00,
      .end = 0x0F,
      .read_byte = YPlatformDMACallbackReadPortByte,
      .write_byte = YPlatformDMACallbackWritePortByte,
      .context = &platform->dma,
  };
  YPlatformRegisterPortMapEntry(platform, &dma_entry);
  YPlatformPortMapEntry dma_page_entry = {
      .entry_type =
          (YPlatformPortMapEntryType)Y_PLATFORM_PORT_MAP_ENTRY_DMA_PAGE,
      .start = 0x80,
      .end = 0x8F,
      .read_byte = YPlatformDMACallbackReadPortByte,
      .write_byte = YPlatformDMACallbackWritePortByte,
      .context = &platform->dma,
  };
  YPlatformRegisterPortMapEntry(platform, &dma_page_entry);
}

Y_FILE_PRIVATE void YPlatformInitVideo(YPlatformState* platform) {
  platform->video.config = YVideoDefaultConfig;
  platform->video.config.context = platform;
  platform->video.config.logger = &platform->logger;
  platform->video.config.adapter = platform->config.video_adapter;
  platform->video.config.vram = platform->config.vram;
  platform->video.config.text_font = platform->config.video_text_font;
  YVideoInit(&platform->video);

  const YVideoAdapterMetadata* adapter =
      YVideoGetAdapterMetadata(&platform->video);

  YPlatformMemoryMapEntry vram_entry = {
      .context = &platform->video,
      .entry_type = Y_PLATFORM_MEMORY_MAP_ENTRY_VRAM,
      .start = adapter->vram_address,
      .end = adapter->vram_address + adapter->vram_size - 1,
      // Reads go straight to the buffer: nothing observes them, and the guest
      // reads back what it wrote. Writes keep a callback so the adapter can
      // see them - see YVideoWriteVRAMByte.
      .read_data = platform->config.vram,
      .write_byte_fn = YPlatformVideoCallbackWriteVRAMByte,
  };
  YPlatformRegisterMemoryMapEntry(platform, &vram_entry);

  YPlatformPortMapEntry port_entry = {
      .context = platform,
      .entry_type = Y_PLATFORM_PORT_MAP_ENTRY_VIDEO,
      .start = adapter->port_start,
      .end = adapter->port_end,
      .read_byte = YPlatformVideoCallbackReadPortByte,
      .write_byte = YPlatformVideoCallbackWritePortByte,
  };
  YPlatformRegisterPortMapEntry(platform, &port_entry);
}

// What the platform's logger stamps each log with: the tick count, and where
// the CPU is. YCPUTick() moves IP past an instruction before running it, so
// while an instruction runs - making the port or memory access a log is
// about - CS:IP is the instruction after it, which is also what an 8086
// pushes when an interrupt arrives.
Y_FILE_PRIVATE uint64_t YPlatformGetLogTick(void* context) {
  return ((YPlatformState*)context)->ticks;
}

Y_FILE_PRIVATE bool YPlatformGetLogLocation(
    void* context, uint16_t* cs, uint16_t* ip) {
  const YPlatformState* platform = (const YPlatformState*)context;
  *cs = platform->cpu.registers[Y_CPU_CS];
  *ip = platform->cpu.registers[Y_CPU_IP];
  return true;
}

// Initialize the platform state with the provided configuration. Returns true
// if the platform state was successfully initialized, or false if:
//   - The physical memory size is not between 64K and 640K.
Y_PUBLIC bool YPlatformInit(YPlatformState* platform) {
  // Initialized first, ahead of validation, so that a rejected config can
  // still be logged.
  YLoggerInit(&platform->logger, platform->config.logger_config);

  if (platform->config.physical_memory_size <
          Y_PLATFORM_MIN_PHYSICAL_MEMORY_SIZE ||
      platform->config.physical_memory_size >
          Y_PLATFORM_MAX_PHYSICAL_MEMORY_SIZE) {
    Y_PLATFORM_ERROR(
        "invalid_physical_memory_size",
        Y_LOG_UINT("size", platform->config.physical_memory_size),
        Y_LOG_UINT("min", Y_PLATFORM_MIN_PHYSICAL_MEMORY_SIZE),
        Y_LOG_UINT("max", Y_PLATFORM_MAX_PHYSICAL_MEMORY_SIZE));
    return false;
  }
  // A machine with no memory would run until its first instruction fetch came
  // back as open bus, so this is rejected here rather than left to fail
  // obscurely later.
  if (platform->config.physical_memory == NULL) {
    Y_PLATFORM_ERROR("missing_physical_memory");
    return false;
  }
  // The video adapter reads and writes this directly, so an absent buffer
  // would leave the screen permanently blank with nothing to say why.
  if (platform->config.vram == NULL) {
    Y_PLATFORM_ERROR("missing_vram");
    return false;
  }

  YPlatformInitCPU(platform);
  // Only once the CPU is up: a log from the config checks above would
  // otherwise give the location of an instruction that is not running.
  const YLoggerSource log_source = {
      .context = platform,
      .get_tick = YPlatformGetLogTick,
      .get_location = YPlatformGetLogLocation,
  };
  YLoggerSetSource(&platform->logger, &log_source);
  YPlatformInitMemoryMap(platform);
  YPlatformInitBIOS(platform);
  YPlatformInitPIC(platform);
  YPlatformInitPIT(platform);
  YPlatformInitPPI(platform);
  YPlatformInitKeyboard(platform);
  YPlatformInitFDC(platform);
  YPlatformInitHDC(platform);
  YPlatformInitDMA(platform);
  YPlatformInitVideo(platform);

  platform->ticks = 0;
  platform->pit_cycles = 0;
  platform->fdc_cycles = 0;
  platform->keyboard_cycles = 0;
  platform->last_sync_ticks = 0;
  // Schedule the first deadline from the devices' power-on state, so that the
  // first instruction is not treated as already overdue.
  platform->next_event_ticks =
      YPlatformCyclesUntilNextEvent(platform, Y_PLATFORM_MAX_EVENT_INTERVAL);

  // Set before the two calls below, because each of them recomputes the
  // enabled flags and one of those is derived from this.
  // Last, because every region the machine has is registered by now.
  YPlatformUpdateAfterMemoryMapChange(platform);

  return true;
}

Y_PUBLIC Y_HOT bool YPlatformRaiseIRQ(YPlatformState* platform, uint8_t irq) {
  if (irq >= 8) {
    return false;
  }
  YPICRaiseIRQ(&platform->pic, irq);
  return true;
}

// ============================================================================
// Device scheduling
// ============================================================================
//
// Devices are not clocked on every instruction. Each is brought up to date
// only when it next has something to do, and the instruction path compares
// against the earliest of those deadlines.
//
// The consequence is that device state is generally stale. Anything that reads
// it - a port handler, or the host asking what the CRT beam is doing - has to
// bring the device up to date first, which is what YPlatformSync() is
// for. Getting this wrong shows up as a guest timing loop reading a counter
// that never moves, so port handlers that touch a device call it.

// Half the range of the tick counter. A deadline this far behind the current
// tick or less has come due; anything further away is in the future with the
// counter having wrapped in between. Too large for an enum, which is an int.
//
// Shifted rather than written out, and shifted from a uint32_t rather than an
// int, because shifting a 1 into an int's sign bit is undefined.
Y_FILE_PRIVATE const uint32_t YPlatformTickCounterHalfRange = (uint32_t)1 << 31;

// Whether the earliest device deadline has come due. Compared as a difference
// so that a deadline stays in the future across the point where the 32-bit
// cycle counter wraps.
Y_FILE_PRIVATE inline bool YPlatformIsEventDue(const YPlatformState* platform) {
  // An unsigned comparison against half the range rather than a cast of the
  // difference to int32_t, because converting an out-of-range unsigned value is
  // only defined from C23 on and this is C99.
  return (uint32_t)(platform->ticks - platform->next_event_ticks) <
         YPlatformTickCounterHalfRange;
}

// Work out when the next device needs attention, in cycles from now, up to
// max_cycles. The instruction path passes Y_PLATFORM_MAX_EVENT_INTERVAL; the
// idle skip passes what is left of the budget it was given, because there the
// answer is how far the clock may be moved rather than how long until it is
// checked again.
Y_FILE_PRIVATE uint32_t YPlatformCyclesUntilNextEvent(
    const YPlatformState* platform, uint32_t max_cycles) {
  uint32_t cycles = max_cycles;

  // The PIT's next output change, converted from its own 1.19MHz clock and
  // offset by the cycles already credited towards its next tick.
  const uint32_t pit_ticks = YPITTicksUntilNextEvent(&platform->pit);
  if (pit_ticks != Y_PIT_NO_EVENT) {
    const uint32_t pit_cycles =
        pit_ticks * Y_PLATFORM_CYCLES_PER_PIT_TICK - platform->pit_cycles;
    if (pit_cycles < cycles) {
      cycles = pit_cycles;
    }
  }

  // The floppy controller only advances a command that is executing; when no
  // command is in flight there is nothing for a tick to do.
  if (platform->fdc.phase == Y_FDC_PHASE_EXECUTION) {
    const uint32_t fdc_cycles =
        Y_PLATFORM_CYCLES_PER_FDC_TICK - platform->fdc_cycles;
    if (fdc_cycles < cycles) {
      cycles = fdc_cycles;
    }
  }

  // Never return 0, which would leave the deadline permanently due.
  return cycles > 0 ? cycles : 1;
}

// Bring every device up to date with the cycles that have run since the last
// sync, and schedule the next deadline.
Y_PUBLIC Y_HOT void YPlatformSync(YPlatformState* platform) {
  const uint32_t elapsed = platform->ticks - platform->last_sync_ticks;
  platform->last_sync_ticks = platform->ticks;

  if (elapsed > 0) {
    platform->pit_cycles += elapsed;
    const uint32_t pit_ticks =
        platform->pit_cycles / Y_PLATFORM_CYCLES_PER_PIT_TICK;
    if (pit_ticks > 0) {
      platform->pit_cycles -= pit_ticks * Y_PLATFORM_CYCLES_PER_PIT_TICK;
      YPITAdvance(&platform->pit, pit_ticks);
    }

    platform->fdc_cycles += elapsed;
    const uint32_t fdc_ticks =
        platform->fdc_cycles / Y_PLATFORM_CYCLES_PER_FDC_TICK;
    if (fdc_ticks > 0) {
      platform->fdc_cycles -= fdc_ticks * Y_PLATFORM_CYCLES_PER_FDC_TICK;
      // YFDCTick() does nothing unless a command is executing, so an idle
      // controller does not need catching up at all - which matters because
      // an idle stretch is not bounded by an FDC deadline.
      if (platform->fdc.phase == Y_FDC_PHASE_EXECUTION) {
        for (uint32_t i = 0; i < fdc_ticks; ++i) {
          YFDCTick(&platform->fdc);
        }
      }
    }

    platform->keyboard_cycles += elapsed;
    const uint32_t keyboard_ticks =
        platform->keyboard_cycles / Y_PLATFORM_CYCLES_PER_MILLISECOND;
    if (keyboard_ticks > 0) {
      platform->keyboard_cycles -=
          keyboard_ticks * Y_PLATFORM_CYCLES_PER_MILLISECOND;
      for (uint32_t i = 0; i < keyboard_ticks; ++i) {
        YKeyboardTickMs(&platform->keyboard);
      }
    }

    // Advancing the beam costs the same whether it moved by one cycle or a
    // whole frame, so the video adapter needs no deadline of its own.
    YVideoTick(&platform->video, elapsed);
  }

  platform->next_event_ticks =
      platform->ticks +
      YPlatformCyclesUntilNextEvent(platform, Y_PLATFORM_MAX_EVENT_INTERVAL);
}

// The body of both entry points. YPlatformTick() promises its caller one
// instruction; YPlatformRun() is driving the machine rather than stepping it.
// Both pass a constant, so the tests below fold away in each.
Y_FILE_PRIVATE Y_HOT YPlatformRunStatus
YPlatformTickInternal(YPlatformState* platform, bool may_batch_instructions) {
  // How long the CPU may run before something in the machine needs to see it.
  // A deadline already due leaves no budget: the subtraction would otherwise
  // wrap and let the CPU run on past a device that is already waiting.
  const uint16_t max_run_cycles =
      may_batch_instructions && !YPlatformIsEventDue(platform)
          ? (uint16_t)(platform->next_event_ticks - platform->ticks)
          : 0;

  // Tick the CPU.
  YCPUTickResult cpu_result = YCPUTick(&platform->cpu, max_run_cycles);

  // The instruction took as long as it took, and every device is clocked from
  // that - but in arrears. Rather than offering each device its share of the
  // cycles on every instruction, this only checks whether the earliest device
  // deadline has come due, which is one comparison in the common case.
  const uint16_t cycles = platform->cpu.cycles_this_tick;
  platform->ticks += cycles;
  if (YPlatformIsEventDue(platform)) {
    YPlatformSync(platform);
  }

  if (cpu_result == Y_CPU_TICK_INVALID) {
    return Y_PLATFORM_INVALID;
  }

  // A halted CPU with interrupts disabled and nothing pending can never be
  // woken. Note that an ordinary halt is not reported as a stop: the PIT and
  // the rest of the machine must keep ticking so that an interrupt can wake
  // the CPU back up.
  if (platform->cpu.is_halted && !YCPUGetFlag(&platform->cpu, Y_CPU_IF) &&
      !platform->cpu.has_pending_internal_interrupt) {
    return Y_PLATFORM_HUNG;
  }

  return Y_PLATFORM_RUNNING;
}

Y_PUBLIC YPlatformRunStatus YPlatformTick(YPlatformState* platform) {
  return YPlatformTickInternal(platform, false);
}

// Advance the clock over time the guest has said it has no use for, up to
// max_cycles. Every device is brought up to date across the skipped interval
// rather than having it taken away from them, so the guest's timer tick count
// is the same either way - what is skipped is executing the loop it would have
// spent the interval in.
//
// The bound matters as much as the skip: without it a machine idling at a DOS
// prompt would be handed more emulated time per call than the caller asked for,
// and would run its clock fast. Y_PLATFORM_MAX_IDLE_SKIP_CYCLES bounds it a
// second time, in case the caller's budget is itself large enough to overflow
// the catch-up arithmetic.
Y_FILE_PRIVATE void YPlatformSkipIdleTime(
    YPlatformState* platform, uint32_t max_cycles) {
  if (max_cycles > Y_PLATFORM_MAX_IDLE_SKIP_CYCLES) {
    max_cycles = Y_PLATFORM_MAX_IDLE_SKIP_CYCLES;
  }
  const uint32_t cycles = YPlatformCyclesUntilNextEvent(platform, max_cycles);
  if (cycles == 0) {
    return;
  }
  platform->ticks += cycles;
  YPlatformSync(platform);
}

Y_PUBLIC Y_HOT YPlatformRunStatus
YPlatformRun(YPlatformState* platform, uint32_t max_cycles) {
  // Instructions are only ever run whole, so the last one of a run generally
  // takes the total a little past the budget. Unsigned subtraction keeps this
  // right across the counter wrapping.
  const uint32_t start = platform->ticks;
  uint32_t elapsed = 0;
  while (elapsed < max_cycles) {
    YPlatformRunStatus status = YPlatformTickInternal(platform, true);
    if (status != Y_PLATFORM_RUNNING) {
      return status;
    }
    elapsed = platform->ticks - start;
    // Skipping is done here rather than where the guest declared itself idle
    // because only this loop knows how much of the budget is left, which is
    // what bounds how far the clock may move.
    if (platform->is_guest_idle) {
      platform->is_guest_idle = false;
      if (elapsed < max_cycles) {
        YPlatformSkipIdleTime(platform, max_cycles - elapsed);
        elapsed = platform->ticks - start;
      }
    }
  }
  return Y_PLATFORM_RUNNING;
}

Y_PUBLIC void YPlatformSetDOSIdleSkip(YPlatformState* platform, bool enable) {
  platform->is_dos_idle_skip_enabled = enable;
  platform->cpu.config.handle_interrupt =
      enable ? YPlatformCPUCallbackHandleInterrupt : NULL;
  // An idle declared before the skip went off must not be acted on after it.
  platform->is_guest_idle = false;
}

// ============================================================================
// The CPU's view of guest memory
// ============================================================================

Y_FILE_PRIVATE void YPlatformUpdateDirectDataWindow(YPlatformState* platform) {
  YCPUInvalidateDirectDataWindow(&platform->cpu);
  // The window is a prefix of the address space, so it is whichever region
  // covers address 0 and only while that region is plain storage reached
  // through one buffer in both directions. Conventional memory is that region;
  // a read-only region, or one with a callback in either direction, is not,
  // and is left to the memory map.
  const YPlatformMemoryMapEntry* entry =
      YPlatformGetMemoryMapEntryForAddress(platform, 0);
  if (entry == NULL || entry->start != 0 || entry->read_data == NULL ||
      entry->write_data != entry->read_data) {
    return;
  }
  // A map entry's end is the last address in the region; a window's is one
  // past it.
  YCPUSetDirectDataWindow(&platform->cpu, entry->write_data, entry->end + 1);
}


// ==============================================================================
// src/platform/platform.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_PLATFORM_BUNDLE_H

// ==============================================================================
// YAX86 PPI MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_PPI_BUNDLE_H
#define YAX86_PPI_BUNDLE_H

#ifndef YAX86_UTIL_BUNDLE_H
#include "util.h"
#endif  // YAX86_UTIL_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

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

// ==============================================================================
// YAX86 VIDEO MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_VIDEO_BUNDLE_H
#define YAX86_VIDEO_BUNDLE_H

#ifndef YAX86_UTIL_BUNDLE_H
#include "util.h"
#endif  // YAX86_UTIL_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

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

