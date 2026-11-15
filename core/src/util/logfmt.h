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

// A line being built.
typedef struct YLogfmtLine {
  char* buffer;
  size_t size;
  // Characters in the line so far, not counting the terminating null byte.
  size_t length;
  // Whether a pair has been dropped for want of room.
  bool is_truncated;
} YLogfmtLine;

enum {
  // Room kept free at the end of the buffer for the truncation marker
  // (" truncated=true") and the terminating null byte.
  Y_LOGFMT_RESERVED_LENGTH = sizeof(" truncated=true"),
};

// Start an empty line in buffer, which has room for size characters including
// the terminating null byte. A buffer too small to hold even the truncation
// marker takes no pairs at all.
Y_PUBLIC void YLogfmtLineInit(YLogfmtLine* line, char* buffer, size_t size);

Y_PUBLIC void YLogfmtLineAddUInt(
    YLogfmtLine* line, const char* key, uint64_t value);

Y_PUBLIC void YLogfmtLineAddInt(
    YLogfmtLine* line, const char* key, int64_t value);

// A number in hexadecimal, with a 0x prefix, upper case digits, and at least
// num_digits of them - 0x0201 for a port, 0x9A for a byte.
Y_PUBLIC void YLogfmtLineAddHex(
    YLogfmtLine* line, const char* key, uint64_t value, uint8_t num_digits);

Y_PUBLIC void YLogfmtLineAddBool(
    YLogfmtLine* line, const char* key, bool value);

// A string, quoted and escaped if it has to be. A NULL string is written as an
// empty one.
Y_PUBLIC void YLogfmtLineAddString(
    YLogfmtLine* line, const char* key, const char* value);

// Finish the line, marking it if anything was dropped, and return its length.
// The line is null terminated whether or not this is called.
Y_PUBLIC size_t YLogfmtLineFinish(YLogfmtLine* line);

// ============================================================================
// Reading
// ============================================================================

// A pair read from a line: pointers into the line, which the parse has cut up
// and unescaped in place.
typedef struct YLogfmtPair {
  const char* key;
  const char* value;
} YLogfmtPair;

// Read a line into pairs, cutting it up and unescaping it in place, so that
// the pairs point into the line. Returns whether the line is well formed: a
// token that is not a key=value pair, an empty key, a quoted value left open
// or with an escape this does not write, and more pairs than max_pairs all
// make it not. An empty line is well formed and has no pairs.
Y_PUBLIC bool YLogfmtParse(
    char* line, YLogfmtPair* pairs, uint8_t max_pairs, uint8_t* num_pairs);

// The value of the first pair with the given key, or NULL if there is none.
Y_PUBLIC const char* YLogfmtFind(
    const YLogfmtPair* pairs, uint8_t num_pairs, const char* key);

// Read a number, decimal or hex with a 0x prefix, as the writer writes them.
// Returns whether the whole value was one that fits.
Y_PUBLIC bool YLogfmtParseUInt(const char* value, uint64_t* number);

#endif  // YAX86_UTIL_LOGFMT_H
