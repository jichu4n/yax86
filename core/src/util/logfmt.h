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
// num_digits of them - 0x0201 for a port, 0x9A for a byte.
Y_PUBLIC_HEADER inline void YLogfmtLineAddHex(
    YLogfmtLine* line, const char* key, uint64_t value, uint8_t num_digits) {
  if (line->truncated) {
    return;
  }
  const size_t limit = YLogfmtLineLimit(line);
  size_t position = YLogfmtLineBeginPair(line, key);
  YSNPrintFPutC(line->buffer, limit, &position, '0');
  YSNPrintFPutC(line->buffer, limit, &position, 'x');
  YSNPrintFPutUI(
      line->buffer, limit, &position, value, 16, 1, num_digits, 1, 0);
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
