// Bytes as hex, for the replies that carry binary data: a logfmt line is
// text, and hex is the one form of a byte that never needs escaping.
#ifndef YAX86_PICOCALC_HEX_H
#define YAX86_PICOCALC_HEX_H

#include <stdint.h>

// Bytes as two hex digits each, upper case, into a buffer twice the count
// and one more long.
static inline void HexFormat(const uint8_t* bytes, uint32_t count, char* out) {
  static const char kDigits[] = "0123456789ABCDEF";
  for (uint32_t i = 0; i < count; ++i) {
    *out++ = kDigits[bytes[i] >> 4];
    *out++ = kDigits[bytes[i] & 0x0F];
  }
  *out = '\0';
}

#endif  // YAX86_PICOCALC_HEX_H
