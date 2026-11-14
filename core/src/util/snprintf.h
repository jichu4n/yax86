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
  size_t num_chars = 0;
  size_t len = 0;
  const char* tmp = s;
  while (*tmp++) len++;

  int pad = width - (int)len;
  if (pad < 0) pad = 0;

  // Strings always use space padding (zero flag is ignored per standard)
  while (pad-- > 0) {
    num_chars += YSNPrintFPutC(buffer, size, pos, ' ');
  }

  while (*s) {
    num_chars += YSNPrintFPutC(buffer, size, pos, *s++);
  }
  return num_chars;
}

Y_MODULE_PRIVATE_HEADER size_t YSNPrintFPutUI(
    char* buffer, size_t size, size_t* pos, unsigned long long value, int base,
    int is_uppercase, int width, int should_pad_zero, int is_negative) {
  char temp[64];
  int i = 0;
  size_t num_chars = 0;

  if (value == 0) {
    temp[i++] = '0';
  } else {
    while (value != 0) {
      int digit = value % base;
      if (digit < 10) {
        temp[i++] = digit + '0';
      } else {
        temp[i++] = digit - 10 + (is_uppercase ? 'A' : 'a');
      }
      value /= base;
    }
  }

  int len = i;
  if (is_negative) len++;

  int pad = width - len;
  if (pad < 0) pad = 0;

  // If zero padding is requested, sign should be printed before padding
  if (should_pad_zero) {
    if (is_negative) {
      num_chars += YSNPrintFPutC(buffer, size, pos, '-');
      // The sign is written, so it is not written again below.
      is_negative = 0;
    }
    while (pad-- > 0) {
      num_chars += YSNPrintFPutC(buffer, size, pos, '0');
    }
  } else {
    while (pad-- > 0) {
      num_chars += YSNPrintFPutC(buffer, size, pos, ' ');
    }
  }

  if (is_negative) {
    num_chars += YSNPrintFPutC(buffer, size, pos, '-');
  }

  while (i > 0) {
    num_chars += YSNPrintFPutC(buffer, size, pos, temp[--i]);
  }
  return num_chars;
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
    int should_pad_zero = 0;
    if (*format == '0') {
      should_pad_zero = 1;
      format++;
    }

    // Width
    int width = 0;
    while (*format >= '0' && *format <= '9') {
      width = width * 10 + (*format - '0');
      format++;
    }

    // The length modifier. length_l counts the l's, 0 for an int, 1 for a long
    // and 2 for a long long. has_length_z is set by a z, for a size_t.
    int length_l = 0;
    int has_length_z = 0;
    if (*format == 'l') {
      length_l++;
      format++;
      if (*format == 'l') {
        length_l++;
        format++;
      }
    } else if (*format == 'z') {
      has_length_z = 1;
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
        if (has_length_z)
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

        int is_negative = 0;
        unsigned long long uval;
        if (val < 0) {
          is_negative = 1;
          // Negated as unsigned: the magnitude of the most negative value
          // does not fit in a long long.
          uval = 0ULL - (unsigned long long)val;
        } else {
          uval = (unsigned long long)val;
        }
        YSNPrintFPutUI(
            buffer, size, &pos, uval, 10, 0, width, should_pad_zero,
            is_negative);
        break;
      }
      case 'u':
      case 'x':
      case 'X': {
        unsigned long long val;
        int base = 10;
        int is_uppercase = 0;

        if (*format == 'x') {
          base = 16;
        } else if (*format == 'X') {
          base = 16;
          is_uppercase = 1;
        }

        if (has_length_z)
          val = va_arg(args, size_t);
        else if (length_l == 2)
          val = va_arg(args, unsigned long long);
        else if (length_l == 1)
          val = va_arg(args, unsigned long);
        else
          val = va_arg(args, unsigned int);

        YSNPrintFPutUI(
            buffer, size, &pos, val, base, is_uppercase, width, should_pad_zero,
            0);
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
            buffer, size, &pos, val, 16, 0, adjusted_width, should_pad_zero, 0);
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
