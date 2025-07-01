#ifndef YAX86_CPU_WIDTHS_H
#define YAX86_CPU_WIDTHS_H

#ifndef YAX86_IMPLEMENTATION
#include "cpu_public.h"
#endif  // YAX86_IMPLEMENTATION

// Data widths supported by the 8086 CPU.
typedef enum Width {
  kByte = 0,
  kWord,
} Width;

enum {
  // Number of data width types.
  kNumWidths = kWord + 1,
};

// Bitmask to extract the sign bit of a value.
static const uint32_t kSignBit[kNumWidths] = {
    1 << 7,   // kByte
    1 << 15,  // kWord
};

// Maximum unsigned value for each data width.
static const uint32_t kMaxValue[kNumWidths] = {
    0xFF,   // kByte
    0xFFFF  // kWord
};

// Maximum signed value for each data width.
static const int32_t kMaxSignedValue[kNumWidths] = {
    0x7F,   // kByte
    0x7FFF  // kWord
};

// Minimum signed value for each data width.
static const int32_t kMinSignedValue[kNumWidths] = {
    -0x80,   // kByte
    -0x8000  // kWord
};

// Number of bytes in each data width.
static const uint8_t kNumBytes[kNumWidths] = {
    1,  // kByte
    2,  // kWord
};

// Number of bits in each data width.
static const uint8_t kNumBits[kNumWidths] = {
    8,   // kByte
    16,  // kWord
};

#endif  // YAX86_CPU_WIDTHS_H
