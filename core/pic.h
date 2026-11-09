// ==============================================================================
// YAX86 PIC MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_PIC_BUNDLE_H
#define YAX86_PIC_BUNDLE_H

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

