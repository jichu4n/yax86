// ==============================================================================
// YAX86 FDC MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_FDC_BUNDLE_H
#define YAX86_FDC_BUNDLE_H

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

