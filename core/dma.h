// ==============================================================================
// YAX86 DMA MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_DMA_BUNDLE_H
#define YAX86_DMA_BUNDLE_H

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

