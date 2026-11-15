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

// Helper to put a character into the buffer safely.
// Returns 1 (always counts the character, even if not written).
Y_MODULE_PRIVATE_HEADER inline size_t YSNPrintFPutC(
    char* buffer, size_t size, size_t* pos, char c) {
  if (*pos < size) {
    buffer[*pos] = c;
  }
  ++*pos;
  return 1;
}

Y_MODULE_PRIVATE size_t
YSNPrintFPutS(char* buffer, size_t size, size_t* pos, const char* s, int width);

Y_MODULE_PRIVATE size_t YSNPrintFPutUI(
    char* buffer, size_t size, size_t* pos, unsigned long long value, int base,
    int is_uppercase, int width, int should_pad_zero, int is_negative);

Y_MODULE_PRIVATE int YVSNPrintF(
    char* buffer, size_t size, const char* format, va_list args) Y_UNUSED;

Y_MODULE_PRIVATE int YSNPrintF(
    char* buffer, size_t size, const char* format, ...) Y_UNUSED;

#endif  // YAX86_UTIL_SNPRINTF_H
