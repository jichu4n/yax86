// ==============================================================================
// YAX86 PLATFORM MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_PLATFORM_BUNDLE_H
#define YAX86_PLATFORM_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/util/common.h start
// ==============================================================================

#line 1 "./src/util/common.h"
#ifndef YAX86_UTIL_COMMON_H
#define YAX86_UTIL_COMMON_H

// Macro that expands to `static` when bundled. Use for variables and functions
// that need to be visible to other files within the same module, but not
// publicly to users of the bundled library.
//
// This enables better IDE integration as it allows each source file to be
// compiled independently in unbundled form, but still keeps the symbols private
// when bundled.
#ifdef YAX86_IMPLEMENTATION
// When bundled, static linkage so that the symbol is only visible within the
// implementation file.
#define YAX86_PRIVATE static
#else
// When unbundled, use default linkage.
#define YAX86_PRIVATE
#endif  // YAX86_IMPLEMENTATION

// Macro to mark a function or parameter as unused.
#if defined(__GNUC__) || defined(__clang__)
#define YAX86_UNUSED __attribute__((unused))
#else
#define YAX86_UNUSED
#endif  // defined(__GNUC__) || defined(__clang__)

// Macro to keep a function out of line.
//
// Only worth reaching for where inlining has been measured to hurt. On a core
// with few registers, folding a large callee into an already register-hungry
// caller makes both spill.
#if defined(__GNUC__) || defined(__clang__)
#define YAX86_NOINLINE __attribute__((noinline))
#else
#define YAX86_NOINLINE
#endif  // defined(__GNUC__) || defined(__clang__)

// Macro to keep a function inlined.
//
// Like YAX86_NOINLINE, only worth reaching for against a measurement. The case
// it exists for is a small helper on the hot path that the compiler inlines
// while it has a single caller and emits out of line once it has several - a
// change to the hot path that nothing in the source shows.
#if defined(__GNUC__) || defined(__clang__)
#define YAX86_ALWAYS_INLINE __attribute__((always_inline)) inline
#else
#define YAX86_ALWAYS_INLINE inline
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
#ifndef YAX86_HOT
#define YAX86_HOT
#endif  // YAX86_HOT

// The same, for data rather than code.
//
// Separate from YAX86_HOT because a compiler will not put executable code and
// read-only data in one section, and because a target may well want to place
// them differently even where it could.
#ifndef YAX86_HOT_DATA
#define YAX86_HOT_DATA
#endif  // YAX86_HOT_DATA

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

static int VSNPrintF(char* buffer, size_t size, const char* format,
                     va_list args) YAX86_UNUSED;

static int SNPrintF(char* buffer, size_t size, const char* format, ...)
    YAX86_UNUSED;

// Helper to put a character into the buffer safely.
// Returns 1 (always counts the character, even if not written).
static size_t SNPrintFPutC(char* buffer, size_t size, size_t* pos, char c) {
  if (*pos < size) {
    buffer[*pos] = c;
  }
  (*pos)++;
  return 1;
}

static size_t SNPrintFPutS(char* buffer, size_t size, size_t* pos,
                           const char* s, int width) {
  size_t count = 0;
  size_t len = 0;
  const char* tmp = s;
  while (*tmp++) len++;

  int pad = width - (int)len;
  if (pad < 0) pad = 0;

  // Strings always use space padding (zero flag is ignored per standard)
  while (pad-- > 0) {
    count += SNPrintFPutC(buffer, size, pos, ' ');
  }

  while (*s) {
    count += SNPrintFPutC(buffer, size, pos, *s++);
  }
  return count;
}

static size_t SNPrintFPutUI(char* buffer, size_t size, size_t* pos,
                            unsigned long long value, int base, int uppercase,
                            int width, int pad_zero, int negative) {
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
      count += SNPrintFPutC(buffer, size, pos, '-');
      negative = 0; // Sign already handled
    }
    while (pad-- > 0) {
      count += SNPrintFPutC(buffer, size, pos, '0');
    }
  } else {
    while (pad-- > 0) {
      count += SNPrintFPutC(buffer, size, pos, ' ');
    }
  }

  if (negative) {
    count += SNPrintFPutC(buffer, size, pos, '-');
  }

  while (i > 0) {
    count += SNPrintFPutC(buffer, size, pos, temp[--i]);
  }
  return count;
}

static int VSNPrintF(char* buffer, size_t size, const char* format,
                     va_list args) {
  size_t pos = 0;

  while (*format) {
    if (*format != '%') {
      SNPrintFPutC(buffer, size, &pos, *format++);
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
          SNPrintFPutC(buffer, size, &pos, ' ');
        }
        SNPrintFPutC(buffer, size, &pos, c);
        break;
      }
      case 's': {
        const char* s = va_arg(args, const char*);
        if (!s) s = "(null)";
        SNPrintFPutS(buffer, size, &pos, s, width);
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
          uval = (unsigned long long)-val;
        } else {
          uval = (unsigned long long)val;
        }
        SNPrintFPutUI(buffer, size, &pos, uval, 10, 0, width, pad_zero,
                        negative);
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

        SNPrintFPutUI(buffer, size, &pos, val, base, uppercase, width,
                        pad_zero, 0);
        break;
      }
      case 'p': {
        unsigned long long val =
            (unsigned long long)(uintptr_t)va_arg(args, void*);
        // Print 0x prefix
        SNPrintFPutC(buffer, size, &pos, '0');
        SNPrintFPutC(buffer, size, &pos, 'x');
        // Adjust width to account for "0x" prefix
        int adjusted_width = width > 2 ? width - 2 : 0;
        SNPrintFPutUI(buffer, size, &pos, val, 16, 0, adjusted_width, pad_zero,
                        0);
        break;
      }
      case '%': {
        SNPrintFPutC(buffer, size, &pos, '%');
        break;
      }
      default: {
        // Unknown specifier, print % and the specifier literally
        SNPrintFPutC(buffer, size, &pos, '%');
        SNPrintFPutC(buffer, size, &pos, *format);
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

static int SNPrintF(char* buffer, size_t size, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int ret = VSNPrintF(buffer, size, format, args);
  va_end(args);
  return ret;
}

#endif  // YAX86_UTIL_SNPRINTF_H


// ==============================================================================
// src/util/snprintf.h end
// ==============================================================================

// ==============================================================================
// src/util/log.h start
// ==============================================================================

#line 1 "./src/util/log.h"
// Logging library.
//
// Provides a Logger that formats messages and hands them to a caller-provided
// sink. Logging is always compiled in - even on MCU targets the sink can write
// to a debugging serial port - and is filtered entirely at runtime, by module
// and by severity level, before a message is formatted.

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
#ifndef YAX86_UTIL_SNPRINTF_H
#include "snprintf.h"
#endif  // YAX86_UTIL_SNPRINTF_H

// ============================================================================
// Levels and modules
// ============================================================================

// Log severity levels, in decreasing order of severity.
typedef enum LogLevel {
  // Emulation is likely incorrect, e.g. an invalid opcode or an unmapped
  // memory access.
  kLogLevelError = 0,
  // Suspicious but handled, e.g. a read from an unmapped I/O port.
  kLogLevelWarn,
  // Diagnostic detail.
  kLogLevelDebug,
} LogLevel;

enum {
  // Maximum length of a formatted log message, including the terminating null
  // byte. Longer messages are truncated.
  kLogMaxLineLength = 256,
  // Maximum number of distinct log modules, bounded by the width of
  // LoggerConfig.enabled_modules.
  kLogMaxModules = 32,
};

// Identifies the module a log message originated from.
//
// Each module declares its own LogModule in its own public header, so that
// modules do not need to know about one another. IDs must be unique across
// modules - see the module ID test in core/tests/util.
typedef struct LogModule {
  // Bit index used for mask-based filtering. Must be less than kLogMaxModules.
  uint8_t id;
  // Human-readable module name, e.g. "FDC".
  const char* name;
} LogModule;

// Returns the filter mask bit for a module.
static inline uint32_t LogModuleMask(const LogModule* module) {
  return (uint32_t)1 << module->id;
}

// ============================================================================
// Logger
// ============================================================================

// Caller-provided runtime configuration for a logger.
typedef struct LoggerConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Callback to write a formatted log message. The message is null-terminated
  // and carries no prefix or trailing newline - the host composes the final
  // output line.
  void (*write_line)(
      void* context, const LogModule* module, LogLevel level, uint64_t tick,
      const char* message, size_t length);

  // Callback returning the current tick count. The platform wires this to its
  // own tick counter. May be NULL, in which case the tick passed to write_line
  // is 0.
  uint64_t (*get_tick)(void* context);

  // Bit mask of enabled modules, indexed by LogModule.id.
  uint32_t enabled_modules;

  // Maximum severity level to emit. Messages with a level greater than this
  // are suppressed.
  LogLevel min_level;
} LoggerConfig;

// State of a logger.
typedef struct Logger {
  // Pointer to caller-provided runtime configuration.
  LoggerConfig* config;

  // Scratch buffer used to format a single message. A logger is therefore not
  // reentrant: a write_line callback must never log.
  char buffer[kLogMaxLineLength];
} Logger;

// Initialize a logger with the provided configuration.
static inline void LoggerInit(Logger* logger, LoggerConfig* config) {
  logger->config = config;
  logger->buffer[0] = '\0';
}

// Whether a message with the given module and level would be emitted. This is
// checked before a message is formatted, so that disabled log statements cost
// only a few comparisons.
static inline bool LoggerIsEnabled(
    const Logger* logger, const LogModule* module, LogLevel level) {
  return logger != NULL && logger->config != NULL &&
         logger->config->write_line != NULL &&
         level <= logger->config->min_level &&
         (logger->config->enabled_modules & LogModuleMask(module)) != 0;
}

// Enable a module on a logger.
static inline void LoggerEnableModule(Logger* logger, const LogModule* module) {
  if (logger != NULL && logger->config != NULL) {
    logger->config->enabled_modules |= LogModuleMask(module);
  }
}

// Disable a module on a logger.
static inline void LoggerDisableModule(
    Logger* logger, const LogModule* module) {
  if (logger != NULL && logger->config != NULL) {
    logger->config->enabled_modules &= ~LogModuleMask(module);
  }
}

// Format and emit a log message. Prefer the YAX86_LOG macro, which skips
// formatting when the message would be suppressed.
static void LoggerWrite(
    Logger* logger, const LogModule* module, LogLevel level, const char* format,
    ...) YAX86_UNUSED;

static void LoggerWrite(
    Logger* logger, const LogModule* module, LogLevel level, const char* format,
    ...) {
  // Callers normally go through YAX86_LOG, which has already checked this, but
  // LoggerWrite is also callable directly.
  if (!LoggerIsEnabled(logger, module, level)) {
    return;
  }

  va_list args;
  va_start(args, format);
  int formatted_length =
      VSNPrintF(logger->buffer, kLogMaxLineLength, format, args);
  va_end(args);

  // VSNPrintF returns the length the message would have had, which may exceed
  // the buffer. Report the length actually written instead.
  size_t length = 0;
  if (formatted_length > 0) {
    length = (size_t)formatted_length < kLogMaxLineLength
                 ? (size_t)formatted_length
                 : kLogMaxLineLength - 1;
  }

  uint64_t tick = logger->config->get_tick != NULL
                      ? logger->config->get_tick(logger->config->context)
                      : 0;
  logger->config->write_line(
      logger->config->context, module, level, tick, logger->buffer, length);
}

// Emit a log message, skipping formatting if it would be suppressed.
//
// This is a macro rather than a function because it takes a variable number of
// arguments and must avoid the cost of formatting a message that will be
// discarded.
#define YAX86_LOG(logger, module, level, ...)                \
  do {                                                       \
    if (LoggerIsEnabled((logger), (module), (level))) {      \
      LoggerWrite((logger), (module), (level), __VA_ARGS__); \
    }                                                        \
  } while (0)

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
typedef struct StaticVectorHeader {
  // Element size in bytes.
  size_t element_size;
  // Maximum number of elements the vector can hold.
  size_t max_length;
  // Number of elements currently in the vector.
  size_t length;
} StaticVectorHeader;

// Define a static vector type with an element type.
#define STATIC_VECTOR_TYPE(name, element_type, max_length_value)          \
  typedef struct name {                                                   \
    StaticVectorHeader header;                                            \
    element_type elements[max_length_value];                              \
  } name;                                                                 \
  static void name##Init(name* vector) __attribute__((unused));           \
  static void name##Init(name* vector) {                                  \
    static const StaticVectorHeader header = {                            \
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
  kLogModuleIDPlatform = 0,
};

// Log module for the Platform.
static const LogModule kLogModulePlatform = {
    .id = kLogModuleIDPlatform,
    .name = "PLATFORM",
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

struct PlatformState;

// ============================================================================
// Memory mapping
// ============================================================================

// Type ID of a memory map entry.
typedef uint8_t MemoryMapEntryType;

enum {
  // Conventional memory - first 640KB of physical memory, mapped to 0x00000 to
  // 0x9FFFF (640KB).
  kMemoryMapEntryConventional = 0,

  // Maximum number of memory map entries.
  kMaxMemoryMapEntries = 16,

  // The memory map is indexed by page so that a lookup is a load rather than a
  // walk. 4KB pages over the 8086's 1MB address space, which is 256 bytes of
  // index - small enough to keep in the platform state, and coarse enough that
  // every region the machine registers is page aligned.
  kMemoryPageShift = 12,
  kMemoryPageSize = 1 << kMemoryPageShift,
  kMemoryAddressSpaceSize = 0x100000,
  kNumMemoryPages = kMemoryAddressSpaceSize >> kMemoryPageShift,
  // A page no entry covers.
  kMemoryPageUnmapped = 0xFF,
  // A page more than one entry has a share of, which has to be resolved by
  // address. Nothing registers such a region today - every one of them is page
  // aligned - but the index must not quietly answer the wrong entry if one
  // ever does. Note that this is never the answer for an address above the
  // address space: no entry may reach one, so it is unmapped instead.
  kMemoryPageStraddled = 0xFE,

  // Maximum size of physical memory in bytes.
  kMaxPhysicalMemorySize = 640 * 1024,
  // Minimum size of physical memory in bytes.
  kMinPhysicalMemorySize = 64 * 1024,
};

// A memory map entry for a region in logical address space. Memory regions
// should not overlap.
typedef struct MemoryMapEntry {
  // Custom data passed through to callbacks.
  void* context;

  // The memory map entry type, such as kMemoryMapEntryConventional.
  MemoryMapEntryType entry_type;
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
      struct MemoryMapEntry* entry, uint32_t relative_address);
  // Callback to write a byte to memory, where address is relative to the start
  // address. Ignored if write_data is set.
  void (*write_byte_fn)(
      struct MemoryMapEntry* entry, uint32_t relative_address, uint8_t value);
} MemoryMapEntry;

// Register a memory map entry in the platform state. Returns true if the entry
// was successfully registered, or false if:
//   - The entry's memory region does not lie within the address space.
//   - There already exists a memory map entry with the same type.
//   - The new entry's memory region overlaps with an existing entry.
//   - The number of memory map entries would exceed kMaxMemoryMapEntries.
bool RegisterMemoryMapEntry(
    struct PlatformState* platform, const MemoryMapEntry* entry);
// Look up the memory map entry corresponding to an address. Returns NULL if the
// address is not mapped to a known memory map entry.
MemoryMapEntry* GetMemoryMapEntryForAddress(
    struct PlatformState* platform, uint32_t address);
// Look up a memory map entry by type. Returns NULL if no entry found with the
// specified type.
MemoryMapEntry* GetMemoryMapEntryByType(
    struct PlatformState* platform, MemoryMapEntryType entry_type);

// Read a byte from a logical memory address, either directly from the
// corresponding memory map entry's read_data buffer or via its read_byte_fn
// callback.
//
// On the 8086, accessing an invalid memory address will yield garbage data
// rather than causing a page fault. This interface mirrors that behavior.
uint8_t ReadMemoryByte(struct PlatformState* platform, uint32_t address);
// Read a word from a logical memory address, either directly from the
// corresponding memory map entry's read_data buffer or via its read_byte_fn
// callback.
uint16_t ReadMemoryWord(struct PlatformState* platform, uint32_t address);
// Write a byte to a logical memory address, either directly to the
// corresponding memory map entry's write_data buffer or via its
// write_byte_fn callback.
//
// On the 8086, accessing an invalid memory address will yield garbage data
// rather than causing a page fault. This interface mirrors that behavior.
void WriteMemoryByte(
    struct PlatformState* platform, uint32_t address, uint8_t value);
// Write a word to a logical memory address, either directly to the
// corresponding memory map entry's write_data buffer or via its
// write_byte_fn callback.
void WriteMemoryWord(
    struct PlatformState* platform, uint32_t address, uint16_t value);

// ============================================================================
// I/O port mapping
// ============================================================================

// Type ID of an I/O port map entry.
typedef uint16_t PortMapEntryType;

enum {
  // Maximum number of I/O port mapping entries.
  kMaxPortMapEntries = 16,
  // I/O port map entry for the master PIC (ports 0x20-0x21).
  kPortMapEntryPIC = 0x20,
  // I/O port map entry for the PIT (ports 0x40-0x43).
  kPortMapEntryPIT = 0x40,
  // I/O port map entry for the PPI (ports 0x60-0x63).
  kPortMapEntryPPI = 0x60,
  // I/O port map entry for the FDC (ports 0x3F0-0x3F7).
  kPortMapEntryFDC = 0x3F0,
  // I/O port map entry for the HDC (ports 0x300-0x30F).
  kPortMapEntryHDC = 0x300,
  // I/O port map entry for the DMA controller (ports 0x00-0x0F).
  kPortMapEntryDMA = 0x00,
  // I/O port map entry for the DMA Page Registers (ports 0x80-0x8F).
  kPortMapEntryDMAPage = 0x80,
};

// An I/O port map entry. Entries should not overlap.
typedef struct PortMapEntry {
  // Custom data passed through to callbacks.
  void* context;

  // The I/O port map entry type.
  PortMapEntryType entry_type;
  // Start of the I/O port range.
  uint16_t start;
  // Inclusive end of the I/O port range.
  uint16_t end;
  // Callback to read a byte from an I/O port within the range.
  uint8_t (*read_byte)(struct PortMapEntry* entry, uint16_t port);
  // Callback to write a byte an I/O port within the range.
  void (*write_byte)(struct PortMapEntry* entry, uint16_t port, uint8_t value);
} PortMapEntry;

// Register an I/O port map entry in the platform state. Returns true if the
// entry was successfully registered, or false if:
//   - There already exists an I/O port map entry with the same type.
//   - The new entry's I/O port range overlaps with an existing entry.
bool RegisterPortMapEntry(
    struct PlatformState* platform, const PortMapEntry* entry);
// Look up the I/O port map entry corresponding to a port. Returns NULL if the
// port is not mapped to a known I/O port map entry.
PortMapEntry* GetPortMapEntryForPort(
    struct PlatformState* platform, uint16_t port);
// Look up an I/O port map entry by type. Returns NULL if no entry found with
// the specified type.
PortMapEntry* GetPortMapEntryByType(
    struct PlatformState* platform, PortMapEntryType entry_type);

// Read a byte from an I/O port by invoking the corresponding I/O port map
// entry's read_byte callback.
uint8_t ReadPortByte(struct PlatformState* platform, uint16_t port);
// Read a word from an I/O port by invoking the corresponding I/O port map
// entry's read_byte callback. This reads two consecutive bytes from the port.
uint16_t ReadPortWord(struct PlatformState* platform, uint16_t port);
// Write a byte to an I/O port by invoking the corresponding I/O port map
// entry's write_byte callback.
void WritePortByte(
    struct PlatformState* platform, uint16_t port, uint8_t value);
// Write a word to an I/O port by invoking the corresponding I/O port map
// entry's write_byte callback. This writes two consecutive bytes to the port.
void WritePortWord(
    struct PlatformState* platform, uint16_t port, uint16_t value);

// ============================================================================
// Execution control
// ============================================================================

enum {
  // Maximum number of execution breakpoints.
  kMaxBreakpoints = 8,
  // Maximum number of memory watchpoints.
  kMaxMemoryWatchpoints = 8,
  // Returned by PlatformAddBreakpoint() and PlatformAddMemoryWatchpoint() when
  // no slot is available.
  kInvalidWatchIndex = -1,
};

// An execution breakpoint. Execution stops before the instruction at cs:ip is
// executed.
typedef struct PlatformBreakpoint {
  // Whether this slot is in use.
  bool enabled;
  // Code segment of the instruction to break on.
  uint16_t cs;
  // Instruction pointer of the instruction to break on.
  uint16_t ip;
} PlatformBreakpoint;

// A memory watchpoint. Execution stops once the instruction or DMA transfer
// that accessed the watched region has finished.
typedef struct PlatformMemoryWatchpoint {
  // Whether this slot is in use.
  bool enabled;
  // Start address of the watched region.
  uint32_t start;
  // Inclusive end address of the watched region.
  uint32_t end;
  // Whether to stop on reads from the region.
  bool on_read;
  // Whether to stop on writes to the region.
  bool on_write;
} PlatformMemoryWatchpoint;

// Why execution stopped.
typedef enum PlatformStopReason {
  // Reached an instruction with a breakpoint on it.
  kPlatformStopBreakpoint = 0,
  // Accessed a watched memory region.
  kPlatformStopMemoryWatchpoint,
  // Completed one instruction in step mode.
  kPlatformStopStep,
} PlatformStopReason;

// Details of the most recent stop.
typedef struct PlatformStopInfo {
  // Why execution stopped.
  PlatformStopReason reason;

  // CS:IP at the point of the stop. For a breakpoint stop this is the
  // breakpoint address itself, since the instruction has not run yet. For a
  // watchpoint or step stop the instruction has completed, so this points at
  // the next instruction.
  uint16_t cs;
  uint16_t ip;

  // Index of the breakpoint or watchpoint that fired. Unused for a step stop.
  uint8_t index;

  // For a memory watchpoint stop, the address that was accessed.
  uint32_t address;
  // For a memory watchpoint stop, whether the access was a write.
  bool is_write;
} PlatformStopInfo;

// Result of running the platform.
typedef enum PlatformRunStatus {
  // The machine is running normally. Also returned by PlatformRun() when it
  // exhausts its tick budget without stopping.
  kPlatformRunning = 0,
  // The instruction at CS:IP could not be fetched or executed.
  kPlatformInvalid,
  // The CPU halted with interrupts disabled and no interrupt pending, so
  // nothing can ever wake it. This is how GLaBIOS signals a fatal error.
  kPlatformHung,
  // Execution stopped at a breakpoint, watchpoint, or single step. See
  // PlatformGetStopInfo() for which, and where.
  kPlatformStopped,
} PlatformRunStatus;

// ============================================================================
// Platform state
// ============================================================================

// Caller-provided runtime configuration.
typedef struct PlatformConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger configuration, shared by the platform and every module it owns. If
  // NULL, logging is disabled. The configuration is owned by the caller and
  // must outlive the platform.
  //
  // Hosts that want tick numbers in their log output should wire get_tick to
  // return PlatformState.ticks.
  LoggerConfig* logger_config;

  // Physical memory size in bytes. Must be between 64K and 640K.
  uint32_t physical_memory_size;

  // The video adapter installed in the machine. This also determines the
  // display type reported by the PPI's DIP switches, which is what the BIOS
  // branches on when it programs the adapter.
  VideoAdapter video_adapter;

  // The machine's conventional memory, physical_memory_size bytes of it.
  // Required - PlatformInit() fails without it.
  //
  // The caller owns the buffer and it must outlive the platform. Conventional
  // memory is plain storage, so the platform reads and writes it directly
  // rather than through a callback; regions that need a callback register one
  // themselves via MemoryMapEntry.
  //
  // Addresses above physical_memory_size are not backed by this buffer and are
  // not an error: on the 8086 an access outside installed memory yields
  // garbage rather than faulting, and unmapped reads here return 0xFF.
  uint8_t* physical_memory;

  // The video adapter's memory, at least vram_size bytes for the adapter named
  // by video_adapter. Required - PlatformInit() fails without it.
  //
  // Separate from physical_memory because video memory sits above conventional
  // memory in the address space, at 0xB0000 or 0xB8000. A host whose memory
  // buffer spans the whole megabyte can point this into it.
  //
  // The caller owns the buffer and it must outlive the platform.
  uint8_t* vram;

  // Callback invoked when the PC speaker's output changes. frequency_hz is the
  // square wave frequency the speaker should emit, or 0 to turn it off. May be
  // NULL, in which case the speaker is silent.
  //
  // The speaker sounds when PIT channel 2 is producing a tone and both PPI
  // port B bits 0 and 1 are set. This reports current state rather than a
  // stream of events - see PITConfig.set_pc_speaker_frequency.
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
      struct PlatformState* platform, uint32_t frequency_hz);

  // Whether to skip emulated time when the guest says it has nothing to do.
  //
  // MS-DOS waits for a keystroke by polling, not by halting, so an idle command
  // prompt costs exactly as much to emulate as a running program - about
  // 390,000 guest instructions a second, all of them the same loop. It does
  // however issue INT 28h, the documented DOS idle interrupt, on each pass:
  // measured at the MS-DOS 3.30 prompt, 783 times a second.
  //
  // With this on, that interrupt makes PlatformRun() advance the clock straight
  // to whichever comes first, the next device deadline or the end of the budget
  // it was given, instead of executing the loop until it gets there. The
  // guest's own handler still runs; only the waiting is skipped.
  //
  // Time is advanced rather than discarded, so every device still sees every
  // cycle and the guest's timer tick count is unchanged. What does change is
  // that a program timing a loop from inside its own INT 28h handler would see
  // time jump, which is why this is opt in.
  bool enable_dos_idle_skip;
} PlatformConfig;

STATIC_VECTOR_TYPE(MemoryMap, MemoryMapEntry, kMaxMemoryMapEntries)
STATIC_VECTOR_TYPE(PortMap, PortMapEntry, kMaxPortMapEntries)

// State of the platform.
enum {
  // The CPU clock, in cycles per second. 4.77MHz is the IBM PC/XT's 14.318MHz
  // crystal divided by three.
  kCPUCyclesPerSecond = 4772727,
  // Cycles per millisecond, near enough for the devices that want one.
  kCyclesPerMillisecond = kCPUCyclesPerSecond / 1000,
  // The PIT is clocked at 1.193MHz, the same crystal divided by twelve, which
  // is a quarter of the CPU clock.
  kCyclesPerPITTick = 4,
  // How often to step the floppy controller's state machine. This is not a
  // clock ratio - the controller is a state machine here rather than a
  // modelled device - so it keeps the rate it had before cycles were counted,
  // to leave floppy timing where it was.
  kCyclesPerFDCTick = 20,
};

typedef struct PlatformState {
  // Pointer to caller-provided runtime configuration.
  PlatformConfig* config;

  // Logger shared by the platform and every module it owns.
  Logger logger;

  // CPU runtime configuration.
  CPUConfig cpu_config;
  // CPU state.
  CPUState cpu;

  // PIC runtime configuration.
  PICConfig pic_config;
  // PIC state.
  PICState pic;

  // PIT runtime configuration.
  PITConfig pit_config;
  // PIT state.
  PITState pit;

  // PPI runtime configuration.
  PPIConfig ppi_config;
  // PPI state.
  PPIState ppi;

  // Keyboard runtime configuration.
  KeyboardConfig keyboard_config;
  // Keyboard state.
  KeyboardState keyboard;

  // DMA controller runtime configuration.
  DMAConfig dma_config;
  // DMA controller state.
  DMAState dma;

  // FDC state.
  FDCConfig fdc_config;
  FDCState fdc;

  // HDC runtime configuration.
  HDCConfig hdc_config;
  // HDC state.
  HDCState hdc;

  // Video runtime configuration.
  VideoConfig video_config;
  // Video state.
  VideoState video;

  // Memory map.
  MemoryMap memory_map;
  // Which memory map entry owns each page of the address space, or
  // kMemoryPageUnmapped / kMemoryPageStraddled. Extended by each registration,
  // which only happens while the machine is being set up.
  uint8_t memory_page_map[kNumMemoryPages];
  // I/O port map.
  PortMap io_port_map;

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
  // that device up to date first, which is what the PlatformSync* functions
  // do.

  // Set when the guest has said it has nothing to do, and cleared by
  // PlatformRun() once it has skipped the idle time. Only ever set when
  // PlatformConfig.enable_dos_idle_skip is on.
  bool is_guest_idle;

  // Value of ticks when the devices were last brought up to date.
  uint32_t last_sync_ticks;
  // Value of ticks at which the earliest device deadline falls due. Compared
  // as a signed difference against ticks so that it stays correct when the
  // 32-bit cycle counter wraps.
  uint32_t next_event_ticks;

  // Execution breakpoints.
  PlatformBreakpoint breakpoints[kMaxBreakpoints];
  // Memory watchpoints.
  PlatformMemoryWatchpoint memory_watchpoints[kMaxMemoryWatchpoints];

  // Whether any breakpoint or memory watchpoint slot is in use. Cached so that
  // the instruction and memory access hot paths can skip the checks entirely
  // in the common case where nothing is being watched.
  bool has_enabled_breakpoints;
  bool has_enabled_memory_watchpoints;

  // Whether to stop after each instruction.
  bool is_step_mode;

  // Whether stop_info describes a stop that has occurred.
  bool has_stop_info;
  // Details of the most recent stop.
  PlatformStopInfo stop_info;

  // Whether a stop has been triggered but not yet reported to the caller.
  bool stop_pending;
  // Set when execution stopped at a breakpoint, so that resuming executes the
  // instruction instead of stopping again at the same address.
  bool skip_breakpoint_check;
} PlatformState;

// Initialize the platform state with the provided configuration. Returns true
// if the platform state was successfully initialized, or false if:
//   - The physical memory size is not between 64K and 640K.
//   - No physical memory buffer was provided.
//   - No video memory buffer was provided.
bool PlatformInit(PlatformState* platform, PlatformConfig* config);

// Raise a hardware interrupt to the CPU via the PIC. Returns true if the
// IRQ was successfully raised, or false if the IRQ number is invalid.
bool PlatformRaiseIRQ(PlatformState* platform, uint8_t irq);

// Execute one instruction, and bring any device whose deadline has come due up
// to date with the cycles it took.
//
// Note that this is an instruction, not a cycle: a tick runs the instruction at
// CS:IP to completion and charges the clock whatever that cost, so a REP string
// instruction can retire as a single tick worth thousands of cycles. A halted
// CPU retires no instruction but still advances the clock, so that whatever is
// meant to wake it can.
//
// Returns kPlatformRunning if the machine should keep running.
PlatformRunStatus PlatformTick(PlatformState* platform);

// Run up to max_ticks cycles of the platform, stopping early if a tick returns
// anything other than kPlatformRunning. Returns the status of the tick that
// stopped the run, or kPlatformRunning if the full budget was consumed.
//
// max_cycles must be well under 2^31. Progress is measured as an unsigned
// difference from the tick count this call started at, which is what keeps it
// correct across the 32-bit cycle counter wrapping - every 15 minutes of
// emulated time - but that also means a budget approaching 2^32 can never be
// reached, because the difference wraps to zero first. A budget of a display
// frame or so, which is what a host driving the machine in real time passes,
// is nowhere near this.
PlatformRunStatus PlatformRun(PlatformState* platform, uint32_t max_cycles);

// Bring every device up to date with the cycles that have run so far.
//
// Devices are driven from deadlines rather than clocked on every instruction,
// so between deadlines their state lags the CPU. Everything inside the
// platform that reads device state does this for itself, so a caller only
// needs it before inspecting a device directly - most usefully before
// VideoRender(), so that the frame reflects where the CRT beam has actually
// reached.
void PlatformSync(PlatformState* platform);

// Add an execution breakpoint at cs:ip. Returns the breakpoint index, or
// kInvalidWatchIndex if all kMaxBreakpoints slots are in use.
int8_t PlatformAddBreakpoint(PlatformState* platform, uint16_t cs, uint16_t ip);

// Remove the breakpoint with the given index. Returns false if the index is
// out of range or the slot is not in use.
bool PlatformRemoveBreakpoint(PlatformState* platform, uint8_t index);

// Remove all breakpoints.
void PlatformClearBreakpoints(PlatformState* platform);

// Add a memory watchpoint covering the physical addresses [start, end]
// inclusive. Returns the watchpoint index, or kInvalidWatchIndex if all
// kMaxMemoryWatchpoints slots are in use, if start is greater than end, or if
// neither on_read nor on_write is set.
//
// A watchpoint sees every access that goes through the memory map, which
// includes instruction fetches and DMA transfers as well as data accesses.
int8_t PlatformAddMemoryWatchpoint(
    PlatformState* platform, uint32_t start, uint32_t end, bool on_read,
    bool on_write);

// Remove the memory watchpoint with the given index. Returns false if the
// index is out of range or the slot is not in use.
bool PlatformRemoveMemoryWatchpoint(PlatformState* platform, uint8_t index);

// Remove all memory watchpoints.
void PlatformClearMemoryWatchpoints(PlatformState* platform);

// Enable or disable step mode. In step mode every tick that executes an
// instruction stops with reason kPlatformStopStep.
void PlatformSetStepMode(PlatformState* platform, bool is_step_mode);

// Returns details of the most recent stop, or NULL if execution has never
// stopped. The returned pointer stays valid until the next stop.
const PlatformStopInfo* PlatformGetStopInfo(const PlatformState* platform);

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

#define YAX86_PLATFORM_LOG(level, ...) \
  YAX86_LOG(&platform->logger, &kLogModulePlatform, level, __VA_ARGS__)

// How long until the next device needs attention, in CPU cycles from now.
static uint32_t PlatformCyclesUntilNextEvent(
    const PlatformState* platform, uint32_t max_cycles);

// Hand the CPU conventional memory to index directly, or take it away again.
static void PlatformUpdateDirectDataWindow(PlatformState* platform);

enum {
  // Never let a deadline sit further out than this, so that a machine in which
  // nothing is scheduled still comes back regularly.
  //
  // Note that this is not a bound on how many cycles accumulate between syncs.
  // A deadline is only tested between instructions, and a REP string
  // instruction retires as a single tick charging thousands of cycles, so a
  // sync interval routinely runs past it - measured at 27,844 cycles, or
  // 5.8ms, over a DOS boot. Anything relying on a bound has to impose its own;
  // see kMaxIdleSkipCycles.
  kMaxEventInterval = kCyclesPerMillisecond,

  // The furthest an idle skip may move the clock in one go.
  //
  // A skip is otherwise bounded only by the budget its caller passed to
  // PlatformRun(), which nothing checks. Every device's catch-up arithmetic in
  // PlatformSync() is 32-bit and accumulates into a counter holding a
  // remainder, so a jump approaching 2^32 would overflow it, and the keyboard
  // is caught up a millisecond per iteration, so it would also spend ~900,000
  // of them in a single call. A tenth of a second is far more than any sane
  // budget and far less than either limit.
  kMaxIdleSkipCycles = kCPUCyclesPerSecond / 10,
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
static void UpdateMemoryPageMapForEntry(
    PlatformState* platform, uint8_t entry_index) {
  const MemoryMapEntry* entry =
      MemoryMapGet(&platform->memory_map, entry_index);
  for (uint32_t page = entry->start >> kMemoryPageShift,
                last_page = entry->end >> kMemoryPageShift;
       page <= last_page; ++page) {
    const uint32_t page_start = page << kMemoryPageShift;
    const uint32_t page_end = page_start + kMemoryPageSize - 1;
    platform->memory_page_map[page] =
        entry->start <= page_start && entry->end >= page_end
            ? entry_index
            : kMemoryPageStraddled;
  }
}

// Register a memory map entry in the platform state. Returns true if the entry
// was successfully registered, or false if:
//   - The entry's memory region does not lie within the address space.
//   - There already exists a memory map entry with the same type.
//   - The new entry's memory region overlaps with an existing entry.
//   - The number of memory map entries would exceed kMaxMemoryMapEntries.
bool RegisterMemoryMapEntry(
    PlatformState* platform, const MemoryMapEntry* entry) {
  // The index has a slot per page of the address space and none above it, so
  // an entry reaching past the top could not be recorded in it. Rejecting one
  // here is what lets the lookup treat the index as the whole answer.
  if (entry->end >= kMemoryAddressSpaceSize || entry->start > entry->end) {
    return false;
  }
  if (MemoryMapLength(&platform->memory_map) >= kMaxMemoryMapEntries) {
    return false;
  }
  for (uint8_t i = 0; i < MemoryMapLength(&platform->memory_map); ++i) {
    MemoryMapEntry* existing_entry = MemoryMapGet(&platform->memory_map, i);
    if (existing_entry->entry_type == entry->entry_type) {
      return false;
    }
    if (!(existing_entry->start > entry->end ||
          entry->start > existing_entry->end)) {
      return false;
    }
  }
  if (!MemoryMapAppend(&platform->memory_map, entry)) {
    return false;
  }
  UpdateMemoryPageMapForEntry(
      platform, MemoryMapLength(&platform->memory_map) - 1);
  // An open fetch window points into whichever region used to own those
  // addresses, so it must not outlive a change to the map.
  CPUInvalidateInstructionFetchWindow(&platform->cpu);
  // The data window is derived from whichever entry covers address 0, which
  // this may have just become.
  PlatformUpdateDirectDataWindow(platform);
  return true;
}

// What the page index has to say about an address: the entry covering the whole
// of its page, kMemoryPageUnmapped where no entry covers it, or
// kMemoryPageStraddled where more than one entry has a share of the page and
// the caller has to walk the map. An address above the address space has no
// page, and no entry may reach it, so it is unmapped by definition.
//
// Inline rather than a function of its own: the body is a compare, a shift and
// a load, where a call on a core with no cheap way to do any of it is a push, a
// branch, a pop and a return.
static inline uint8_t GetMemoryPageMapIndex(
    const PlatformState* platform, uint32_t address) {
  return address < kMemoryAddressSpaceSize
             ? platform->memory_page_map[address >> kMemoryPageShift]
             : kMemoryPageUnmapped;
}

// Look up the memory region corresponding to an address. Returns NULL if the
// address is not mapped to a known memory region.
YAX86_HOT MemoryMapEntry* GetMemoryMapEntryForAddress(
    PlatformState* platform, uint32_t address) {
  const uint8_t index = GetMemoryPageMapIndex(platform, address);
  if (index < kMaxMemoryMapEntries) {
    return MemoryMapGet(&platform->memory_map, index);
  }
  if (index == kMemoryPageUnmapped) {
    return NULL;
  }
  // A page more than one entry has a share of, which the index cannot answer.
  for (uint8_t i = 0; i < MemoryMapLength(&platform->memory_map); ++i) {
    MemoryMapEntry* entry = MemoryMapGet(&platform->memory_map, i);
    if (address >= entry->start && address <= entry->end) {
      return entry;
    }
  }
  return NULL;
}

// Look up a memory region by type. Returns NULL if no region found with the
// specified type.
YAX86_HOT MemoryMapEntry* GetMemoryMapEntryByType(
    PlatformState* platform, uint8_t entry_type) {
  for (uint8_t i = 0; i < MemoryMapLength(&platform->memory_map); ++i) {
    MemoryMapEntry* entry = MemoryMapGet(&platform->memory_map, i);
    if (entry->entry_type == entry_type) {
      return entry;
    }
  }
  return NULL;
}

// Record the details of a stop for PlatformGetStopInfo().
static void PlatformRecordStop(
    PlatformState* platform, PlatformStopReason reason, uint8_t index,
    uint32_t address, bool is_write) {
  PlatformStopInfo* stop_info = &platform->stop_info;
  stop_info->reason = reason;
  stop_info->cs = platform->cpu.registers[kCS];
  stop_info->ip = platform->cpu.registers[kIP];
  stop_info->index = index;
  stop_info->address = address;
  stop_info->is_write = is_write;
  platform->has_stop_info = true;
}

// Stop if the access at address falls within an enabled memory watchpoint.
// Only called when has_enabled_memory_watchpoints is set.
static void PlatformCheckMemoryWatchpoints(
    PlatformState* platform, uint32_t address, bool is_write) {
  for (uint8_t i = 0; i < kMaxMemoryWatchpoints; ++i) {
    const PlatformMemoryWatchpoint* watchpoint =
        &platform->memory_watchpoints[i];
    if (!watchpoint->enabled || address < watchpoint->start ||
        address > watchpoint->end ||
        !(is_write ? watchpoint->on_write : watchpoint->on_read)) {
      continue;
    }
    PlatformRecordStop(
        platform, kPlatformStopMemoryWatchpoint, i, address, is_write);
    // Unlike a breakpoint or a step, a watchpoint fires in the middle of a
    // tick, so the stop is deferred to the end of that tick.
    platform->stop_pending = true;
    // Ask the CPU to hand control back as soon as the instruction in progress
    // finishes. A watchpoint can also fire from a DMA transfer, in which case
    // there is no CPU tick in progress and this is a no-op - PlatformTick()
    // picks the stop up from stop_pending either way.
    CPURequestStop(&platform->cpu);
    return;
  }
}

// Read a byte from a logical memory address.
YAX86_HOT uint8_t ReadMemoryByte(PlatformState* platform, uint32_t address) {
  if (platform->has_enabled_memory_watchpoints) {
    PlatformCheckMemoryWatchpoints(platform, address, false);
  }
  MemoryMapEntry* entry = GetMemoryMapEntryForAddress(platform, address);
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
  YAX86_PLATFORM_LOG(
      kLogLevelDebug, "read from unmapped address %05X", address);
  return 0xFF;
}

// Read a word from a logical memory address.
uint16_t ReadMemoryWord(PlatformState* platform, uint32_t address) {
  uint8_t low_byte = ReadMemoryByte(platform, address);
  uint8_t high_byte = ReadMemoryByte(platform, address + 1);
  return (high_byte << 8) | low_byte;
}

// Write a byte to a logical memory address.
YAX86_HOT void WriteMemoryByte(
    PlatformState* platform, uint32_t address, uint8_t value) {
  if (platform->has_enabled_memory_watchpoints) {
    PlatformCheckMemoryWatchpoints(platform, address, true);
  }
  MemoryMapEntry* entry = GetMemoryMapEntryForAddress(platform, address);
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
  // write, and both are logged, as they were before there was a direct path.
  YAX86_PLATFORM_LOG(
      kLogLevelDebug, "write of %02X to unmapped address %05X", value, address);
}

// Write a word to a logical memory address.
void WriteMemoryWord(
    PlatformState* platform, uint32_t address, uint16_t value) {
  WriteMemoryByte(platform, address, value & 0xFF);
  WriteMemoryByte(platform, address + 1, (value >> 8) & 0xFF);
}

// Register an I/O port map entry in the platform state. Returns true if the
// entry was successfully registered, or false if:
//   - There already exists an I/O port map entry with the same type.
//   - The new entry's I/O port range overlaps with an existing entry.
bool RegisterPortMapEntry(PlatformState* platform, const PortMapEntry* entry) {
  if (PortMapLength(&platform->io_port_map) >= kMaxPortMapEntries) {
    return false;
  }
  for (uint8_t i = 0; i < PortMapLength(&platform->io_port_map); ++i) {
    PortMapEntry* existing_entry = PortMapGet(&platform->io_port_map, i);
    if (existing_entry->entry_type == entry->entry_type) {
      return false;
    }
    if (!(existing_entry->start > entry->end ||
          entry->start > existing_entry->end)) {
      return false;
    }
  }
  return PortMapAppend(&platform->io_port_map, entry);
}

// Look up the I/O port map entry corresponding to a port. Returns NULL if the
// port is not mapped to a known I/O port map entry.
PortMapEntry* GetPortMapEntryForPort(PlatformState* platform, uint16_t port) {
  for (uint8_t i = 0; i < PortMapLength(&platform->io_port_map); ++i) {
    PortMapEntry* entry = PortMapGet(&platform->io_port_map, i);
    if (port >= entry->start && port <= entry->end) {
      return entry;
    }
  }
  return NULL;
}
// Look up an I/O port map entry by type. Returns NULL if no entry found with
// the specified type.
YAX86_HOT PortMapEntry* GetPortMapEntryByType(
    PlatformState* platform, PortMapEntryType entry_type) {
  for (uint8_t i = 0; i < PortMapLength(&platform->io_port_map); ++i) {
    PortMapEntry* entry = PortMapGet(&platform->io_port_map, i);
    if (entry->entry_type == entry_type) {
      return entry;
    }
  }
  return NULL;
}

// Read a byte from an I/O port by invoking the corresponding I/O port map
// entry's read_byte callback.
YAX86_HOT uint8_t ReadPortByte(PlatformState* platform, uint16_t port) {
  PortMapEntry* entry = GetPortMapEntryForPort(platform, port);
  if (!entry || !entry->read_byte) {
    // Unlike unmapped memory, an unmapped port usually means a device is
    // missing from the port map, so this stays at warning level.
    YAX86_PLATFORM_LOG(kLogLevelWarn, "read from unmapped port %04X", port);
    return 0xFF;
  }
  return entry->read_byte(entry, port);
}

// Read a word from an I/O port by invoking the corresponding I/O port map
// entry's read_byte callback. This reads two consecutive bytes from the port.
uint16_t ReadPortWord(PlatformState* platform, uint16_t port) {
  uint8_t low_byte = ReadPortByte(platform, port);
  uint8_t high_byte = ReadPortByte(platform, port + 1);
  return (high_byte << 8) | low_byte;
}

// Write a byte to an I/O port by invoking the corresponding I/O port map
// entry's write_byte callback.
YAX86_HOT void WritePortByte(
    PlatformState* platform, uint16_t port, uint8_t value) {
  PortMapEntry* entry = GetPortMapEntryForPort(platform, port);
  if (!entry || !entry->write_byte) {
    YAX86_PLATFORM_LOG(
        kLogLevelWarn, "write of %02X to unmapped port %04X", value, port);
    return;
  }
  entry->write_byte(entry, port, value);
}

// Write a word to an I/O port by invoking the corresponding I/O port map
// entry's write_byte callback. This writes two consecutive bytes to the port.
void WritePortWord(PlatformState* platform, uint16_t port, uint16_t value) {
  WritePortByte(platform, port, value & 0xFF);
  WritePortByte(platform, port + 1, (value >> 8) & 0xFF);
}

// ============================================================================
// Callbacks for CPU module
// ============================================================================

static uint8_t CPUCallbackReadMemoryByte(CPUState* cpu, uint32_t address) {
  return ReadMemoryByte((PlatformState*)cpu->config->context, address);
}

static void CPUCallbackWriteMemoryByte(
    CPUState* cpu, uint32_t address, uint8_t value) {
  WriteMemoryByte((PlatformState*)cpu->config->context, address, value);
}

static uint8_t CPUCallbackReadPortByte(CPUState* cpu, uint16_t port) {
  return ReadPortByte((PlatformState*)cpu->config->context, port);
}

static void CPUCallbackWritePortByte(
    CPUState* cpu, uint16_t port, uint8_t value) {
  WritePortByte((PlatformState*)cpu->config->context, port, value);
}

static const CPUConfig kEmptyCPUConfig = {0};

// ============================================================================
// Callbacks for 8259 PIC module
// ============================================================================

static uint8_t PICCallbackReadPortByte(PortMapEntry* entry, uint16_t port) {
  return PICReadPort((PICState*)entry->context, port);
}

YAX86_HOT static void PICCallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  PICWritePort((PICState*)entry->context, port, value);
}

static void PICCallbackPlatformRaiseIRQ0(void* context) {
  PlatformState* platform = (PlatformState*)context;
  PlatformRaiseIRQ(platform, 0);
}

// ============================================================================
// Callbacks for 8253 PIT module
// ============================================================================

YAX86_HOT static uint8_t PITCallbackReadPortByte(
    PortMapEntry* entry, uint16_t port) {
  // A guest timing loop reads the counter expecting it to have moved, so the
  // PIT has to be caught up before it is read.
  PlatformState* platform = (PlatformState*)entry->context;
  PlatformSync(platform);
  return PITReadPort(&platform->pit, port);
}

static void PITCallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  // Syncing first applies the cycles that ran under the old configuration;
  // syncing again afterwards reschedules against the new one.
  PlatformState* platform = (PlatformState*)entry->context;
  PlatformSync(platform);
  PITWritePort(&platform->pit, port, value);
  PlatformSync(platform);
}

static void PITCallbackSetPCSpeakerFrequency(
    void* context, uint32_t frequency_hz) {
  PlatformState* platform = (PlatformState*)context;
  PPISetPCSpeakerFrequencyFromPIT(&platform->ppi, frequency_hz);
}

// ============================================================================
// Callbacks for 8255 PPI module
// ============================================================================

static uint8_t PPICallbackReadPortByte(PortMapEntry* entry, uint16_t port) {
  PlatformState* platform = (PlatformState*)entry->context;
  PlatformSync(platform);
  return PPIReadPort(&platform->ppi, port);
}

static void PPICallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  // Port B gates the speaker against PIT channel 2, so what the guest hears
  // depends on the channel's output state being current.
  PlatformState* platform = (PlatformState*)entry->context;
  PlatformSync(platform);
  PPIWritePort(&platform->ppi, port, value);
  PlatformSync(platform);
}

static void PPICallbackSetKeyboardControl(
    void* context, bool keyboard_enable_clear, bool keyboard_clock_low) {
  PlatformState* platform = (PlatformState*)context;
  KeyboardHandleControl(
      &platform->keyboard, keyboard_enable_clear, keyboard_clock_low);
}

static void PPICallbackSetPCSpeakerFrequency(
    void* context, uint32_t frequency_hz) {
  PlatformState* platform = (PlatformState*)context;
  if (platform->config->set_pc_speaker_frequency) {
    platform->config->set_pc_speaker_frequency(platform, frequency_hz);
  }
}

// ============================================================================
// Callbacks for Keyboard module
// ============================================================================

static void KeyboardCallbackPlatformRaiseIRQ1(void* context) {
  PlatformState* platform = (PlatformState*)context;
  PlatformRaiseIRQ(platform, 1);
}

static void KeyboardCallbackSendScancode(void* context, uint8_t scancode) {
  PlatformState* platform = (PlatformState*)context;
  PPISetScancode(&platform->ppi, scancode);
}

// ============================================================================
// Callbacks for uPD765 FDC module
// ============================================================================

enum {
  kPlatformDMAChannelFloppy = 2,
};

static void FDCCallbackRaiseIRQ6(void* context) {
  PlatformState* platform = (PlatformState*)context;
  PlatformRaiseIRQ(platform, 6);
}

YAX86_HOT static void FDCCallbackRequestDMA(void* context) {
  PlatformState* platform = (PlatformState*)context;
  DMATransferByte(&platform->dma, kPlatformDMAChannelFloppy);
}

static uint8_t FDCCallbackReadPortByte(PortMapEntry* entry, uint16_t port) {
  PlatformState* platform = (PlatformState*)entry->context;
  PlatformSync(platform);
  return FDCReadPort(&platform->fdc, port);
}

static void FDCCallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  // A write can start a command, which is what puts the controller into the
  // execution phase the scheduler watches for.
  PlatformState* platform = (PlatformState*)entry->context;
  PlatformSync(platform);
  FDCWritePort(&platform->fdc, port, value);
  PlatformSync(platform);
}

// ============================================================================
// Callbacks for DMA module
// ============================================================================

static uint8_t DMACallbackReadMemoryByte(void* context, uint32_t address) {
  PlatformState* platform = (PlatformState*)context;
  return ReadMemoryByte(platform, address);
}

static void DMACallbackWriteMemoryByte(
    void* context, uint32_t address, uint8_t value) {
  PlatformState* platform = (PlatformState*)context;
  WriteMemoryByte(platform, address, value);
}

static uint8_t DMACallbackReadDeviceByte(void* context, uint8_t channel) {
  PlatformState* platform = (PlatformState*)context;
  switch (channel) {
    case kPlatformDMAChannelFloppy:
      return FDCReadPort(&platform->fdc, kFDCPortData);
    default:
      return 0xFF;
  }
}

static void DMACallbackWriteDeviceByte(
    void* context, uint8_t channel, uint8_t value) {
  PlatformState* platform = (PlatformState*)context;
  switch (channel) {
    case kPlatformDMAChannelFloppy:
      FDCWritePort(&platform->fdc, kFDCPortData, value);
      break;
    default:
      break;
  }
}

static void DMACallbackOnTerminalCount(void* context, uint8_t channel) {
  PlatformState* platform = (PlatformState*)context;
  switch (channel) {
    case kPlatformDMAChannelFloppy:
      FDCHandleTC(&platform->fdc);
      break;
    default:
      break;
  }
}

static uint8_t DMACallbackReadPortByte(PortMapEntry* entry, uint16_t port) {
  return DMAReadPort((DMAState*)entry->context, port);
}

static void DMACallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  DMAWritePort((DMAState*)entry->context, port, value);
}

// ============================================================================
// Callbacks for Video module
// ============================================================================

YAX86_HOT static uint8_t VideoCallbackReadPortByte(
    PortMapEntry* entry, uint16_t port) {
  // The status port reports where the CRT beam is, which is only meaningful
  // once the beam has been advanced to now. Guests poll this to wait for
  // retrace.
  PlatformState* platform = (PlatformState*)entry->context;
  PlatformSync(platform);
  return VideoReadPort(&platform->video, port);
}

static void VideoCallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  PlatformState* platform = (PlatformState*)entry->context;
  PlatformSync(platform);
  VideoWritePort(&platform->video, port, value);
}

static void VideoCallbackWriteVRAMByte(
    MemoryMapEntry* entry, uint32_t address, uint8_t value) {
  VideoWriteVRAM((VideoState*)entry->context, address, value);
}

// ============================================================================
// Callbacks for HDC module
// ============================================================================

static uint8_t HDCCallbackReadPortByte(PortMapEntry* entry, uint16_t port) {
  return HDCReadPort((HDCState*)entry->context, port);
}

static void HDCCallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  HDCWritePort((HDCState*)entry->context, port, value);
}

// ============================================================================
// Initialization
// ============================================================================

static void PlatformInitBIOS(PlatformState* platform) {
  uint32_t bios_size = BIOSGetROMSize();
  MemoryMapEntry bios_rom = {
      .context = NULL,
      .entry_type = kMemoryMapEntryBIOSROM,
      .start = kBIOSROMStartAddress,
      .end = kBIOSROMStartAddress + bios_size - 1,
      // The ROM image is a constant array in the library, so it is read
      // directly. write_data is left NULL because the BIOS ROM is read-only.
      .read_data = BIOSGetROMData(),
  };
  RegisterMemoryMapEntry(platform, &bios_rom);
}

// Runs the CPU's interrupt acknowledge cycle against the PIC. This is the
// only path by which an external interrupt reaches the CPU, and the PIC marks
// the interrupt in service as part of it - so a vector is never produced
// unless the CPU is taking it right now.
static bool CPUCallbackAcknowledgeInterrupt(CPUState* cpu, uint8_t* vector) {
  PlatformState* platform = (PlatformState*)cpu->config->context;
  const uint8_t interrupt_vector = PICGetPendingInterrupt(&platform->pic);
  if (interrupt_vector == kPICNoPendingInterrupt) {
    return false;
  } else {
    *vector = interrupt_vector;
    return true;
  }
}

enum {
  // The DOS idle interrupt. MS-DOS issues it from the loops in which it waits
  // for input, to tell anything listening that the machine has nothing to do.
  kDOSIdleInterrupt = 0x28,
};

// Notes the guest declaring itself idle. Nothing is serviced here - the guest's
// own handler runs as usual - so this only ever reports the interrupt onwards.
//
// Only installed when the idle skip is enabled, so a machine without it pays
// nothing per interrupt.
YAX86_HOT static InterruptHandlerResult CPUCallbackHandleInterrupt(
    CPUState* cpu, uint8_t interrupt_number) {
  if (interrupt_number == kDOSIdleInterrupt) {
    PlatformState* platform = (PlatformState*)cpu->config->context;
    platform->is_guest_idle = true;
  }
  return kInterruptHandlerUnhandled;
}

// Hands the CPU a direct window for instruction fetch.
//
// Declines wherever a read has to be observed or computed rather than loaded:
// a device region, unmapped memory, a page shared by two entries, or any
// access at all while a memory watchpoint is enabled - a direct read cannot
// fire one.
static void CPUCallbackGetInstructionFetchWindow(
    CPUState* cpu, uint32_t address) {
  CPUInstructionFetchWindow* window = &cpu->instruction_fetch_window;
  window->data = NULL;
  PlatformState* platform = (PlatformState*)cpu->config->context;
  if (platform->has_enabled_memory_watchpoints) {
    return;
  }
  // Anything that is not a single entry's page - unmapped, or shared by more
  // than one entry - has no window to hand out.
  const uint8_t index = GetMemoryPageMapIndex(platform, address);
  if (index >= kMaxMemoryMapEntries) {
    return;
  }
  MemoryMapEntry* entry = MemoryMapGet(&platform->memory_map, index);
  if (entry->read_data == NULL) {
    return;
  }
  // The whole region, not the tail of it from this address, so that a jump
  // backwards within the region still lands inside the window.
  window->data = entry->read_data;
  window->start = entry->start;
  window->end = entry->end + 1;
}

static void PlatformInitCPU(PlatformState* platform) {
  platform->cpu_config = kEmptyCPUConfig;
  platform->cpu_config.context = platform;
  platform->cpu_config.logger = &platform->logger;
  platform->cpu_config.read_memory_byte = CPUCallbackReadMemoryByte;
  platform->cpu_config.get_instruction_fetch_window =
      CPUCallbackGetInstructionFetchWindow;
  platform->cpu_config.write_memory_byte = CPUCallbackWriteMemoryByte;
  platform->cpu_config.acknowledge_interrupt = CPUCallbackAcknowledgeInterrupt;
  if (platform->config->enable_dos_idle_skip) {
    platform->cpu_config.handle_interrupt = CPUCallbackHandleInterrupt;
  }
  platform->cpu_config.read_port = CPUCallbackReadPortByte;
  platform->cpu_config.write_port = CPUCallbackWritePortByte;
  CPUInit(&platform->cpu, &platform->cpu_config);

  // Initialize CPU registers.
  // CS:IP points to the BIOS entry point at 0xFFFF0.
  platform->cpu.registers[kCS] = 0xF000;
  platform->cpu.registers[kIP] = 0xFFF0;
  platform->cpu.registers[kDS] = 0x0000;
  platform->cpu.registers[kSS] = 0x0000;
  platform->cpu.registers[kES] = 0x0000;
  platform->cpu.registers[kSP] = 0xFFFE;
}

static void PlatformInitMemoryMap(PlatformState* platform) {
  MemoryMapInit(&platform->memory_map);
  for (uint32_t page = 0; page < kNumMemoryPages; ++page) {
    platform->memory_page_map[page] = kMemoryPageUnmapped;
  }
  MemoryMapEntry conventional_memory = {
      .context = platform,
      .entry_type = kMemoryMapEntryConventional,
      .start = 0x0000,
      .end = platform->config->physical_memory_size - 1,
      // Conventional memory is the caller's buffer, accessed directly.
      .read_data = platform->config->physical_memory,
      .write_data = platform->config->physical_memory};
  RegisterMemoryMapEntry(platform, &conventional_memory);
}

static void PlatformInitPIC(PlatformState* platform) {
  platform->pic_config.sp = false;
  platform->pic_config.logger = &platform->logger;
  PICInit(&platform->pic, &platform->pic_config);
  PortMapEntry pic_entry = {
      .entry_type = kPortMapEntryPIC,
      .start = 0x20,
      .end = 0x21,
      .read_byte = PICCallbackReadPortByte,
      .write_byte = PICCallbackWritePortByte,
      .context = &platform->pic,
  };
  RegisterPortMapEntry(platform, &pic_entry);
}

static void PlatformInitPIT(PlatformState* platform) {
  platform->pit_config.context = platform;
  platform->pit_config.logger = &platform->logger;
  platform->pit_config.raise_irq_0 = PICCallbackPlatformRaiseIRQ0;
  platform->pit_config.set_pc_speaker_frequency =
      PITCallbackSetPCSpeakerFrequency;
  PITInit(&platform->pit, &platform->pit_config);
  PortMapEntry pit_entry = {
      .entry_type = kPortMapEntryPIT,
      .start = 0x40,
      .end = 0x43,
      .read_byte = PITCallbackReadPortByte,
      .write_byte = PITCallbackWritePortByte,
      .context = platform,
  };
  RegisterPortMapEntry(platform, &pit_entry);
}

static void PlatformInitPPI(PlatformState* platform) {
  platform->ppi_config.context = platform;
  platform->ppi_config.logger = &platform->logger;
  platform->ppi_config.num_floppy_drives = 1;
  platform->ppi_config.memory_size = kPPIMemorySize256KB;
  // The DIP switches are what the BIOS branches on to decide which adapter to
  // program, so they have to agree with the adapter the platform registers.
  platform->ppi_config.display_mode =
      platform->config->video_adapter == kVideoAdapterCGA ? kPPIDisplayCGA80x25
                                                          : kPPIDisplayMDA;
  platform->ppi_config.fpu_installed = false;
  platform->ppi_config.set_pc_speaker_frequency =
      PPICallbackSetPCSpeakerFrequency;
  platform->ppi_config.set_keyboard_control = PPICallbackSetKeyboardControl;
  PPIInit(&platform->ppi, &platform->ppi_config);
  PortMapEntry ppi_entry = {
      .entry_type = kPortMapEntryPPI,
      .start = 0x60,
      .end = 0x63,
      .read_byte = PPICallbackReadPortByte,
      .write_byte = PPICallbackWritePortByte,
      .context = platform,
  };
  RegisterPortMapEntry(platform, &ppi_entry);
}

static void PlatformInitKeyboard(PlatformState* platform) {
  platform->keyboard_config.context = platform;
  platform->keyboard_config.logger = &platform->logger;
  platform->keyboard_config.raise_irq1 = KeyboardCallbackPlatformRaiseIRQ1;
  platform->keyboard_config.send_scancode = KeyboardCallbackSendScancode;
  KeyboardInit(&platform->keyboard, &platform->keyboard_config);
}

static void PlatformInitFDC(PlatformState* platform) {
  platform->fdc_config.context = platform;
  platform->fdc_config.logger = &platform->logger;
  platform->fdc_config.raise_irq6 = FDCCallbackRaiseIRQ6;
  platform->fdc_config.request_dma = FDCCallbackRequestDMA;
  platform->fdc_config.read_image_byte = NULL;
  platform->fdc_config.write_image_byte = NULL;
  FDCInit(&platform->fdc, &platform->fdc_config);
  PortMapEntry fdc_entry = {
      .entry_type = (PortMapEntryType)kPortMapEntryFDC,
      .start = 0x3F0,
      .end = 0x3F7,
      .read_byte = FDCCallbackReadPortByte,
      .write_byte = FDCCallbackWritePortByte,
      .context = platform,
  };
  RegisterPortMapEntry(platform, &fdc_entry);
}

static void PlatformInitHDC(PlatformState* platform) {
  platform->hdc_config.context = platform;
  platform->hdc_config.logger = &platform->logger;
  HDCInit(&platform->hdc, &platform->hdc_config);

  const uint32_t option_rom_size = HDCGetOptionROMSize();
  MemoryMapEntry option_rom_entry = {
      .context = &platform->hdc,
      .entry_type = kMemoryMapEntryHDCOptionROM,
      .start = kHDCOptionROMStartAddress,
      .end = kHDCOptionROMStartAddress + option_rom_size - 1,
      // As with the BIOS ROM, a constant array read directly. write_data is
      // left NULL because the option ROM is read-only.
      .read_data = HDCGetOptionROMData(),
  };
  RegisterMemoryMapEntry(platform, &option_rom_entry);

  PortMapEntry port_entry = {
      .context = &platform->hdc,
      .entry_type = (PortMapEntryType)kPortMapEntryHDC,
      .start = kHDCPortBase,
      .end = kHDCPortBase + kHDCNumPorts - 1,
      .read_byte = HDCCallbackReadPortByte,
      .write_byte = HDCCallbackWritePortByte,
  };
  RegisterPortMapEntry(platform, &port_entry);
}

static void PlatformInitDMA(PlatformState* platform) {
  platform->dma_config.context = platform;
  platform->dma_config.logger = &platform->logger;
  platform->dma_config.read_memory_byte = DMACallbackReadMemoryByte;
  platform->dma_config.write_memory_byte = DMACallbackWriteMemoryByte;
  platform->dma_config.read_device_byte = DMACallbackReadDeviceByte;
  platform->dma_config.write_device_byte = DMACallbackWriteDeviceByte;
  platform->dma_config.on_terminal_count = DMACallbackOnTerminalCount;
  DMAInit(&platform->dma, &platform->dma_config);
  PortMapEntry dma_entry = {
      .entry_type = (PortMapEntryType)kPortMapEntryDMA,
      .start = 0x00,
      .end = 0x0F,
      .read_byte = DMACallbackReadPortByte,
      .write_byte = DMACallbackWritePortByte,
      .context = &platform->dma,
  };
  RegisterPortMapEntry(platform, &dma_entry);
  PortMapEntry dma_page_entry = {
      .entry_type = (PortMapEntryType)kPortMapEntryDMAPage,
      .start = 0x80,
      .end = 0x8F,
      .read_byte = DMACallbackReadPortByte,
      .write_byte = DMACallbackWritePortByte,
      .context = &platform->dma,
  };
  RegisterPortMapEntry(platform, &dma_page_entry);
}

static void PlatformInitVideo(PlatformState* platform) {
  platform->video_config = kDefaultVideoConfig;
  platform->video_config.context = platform;
  platform->video_config.logger = &platform->logger;
  platform->video_config.adapter = platform->config->video_adapter;
  platform->video_config.vram = platform->config->vram;
  VideoInit(&platform->video, &platform->video_config);

  const VideoAdapterMetadata* adapter =
      VideoGetAdapterMetadata(&platform->video);

  MemoryMapEntry vram_entry = {
      .context = &platform->video,
      .entry_type = kMemoryMapEntryVRAM,
      .start = adapter->vram_address,
      .end = adapter->vram_address + adapter->vram_size - 1,
      // Reads go straight to the buffer: nothing observes them, and the guest
      // reads back what it wrote. Writes keep a callback so the adapter can
      // see them - see VideoWriteVRAMByte.
      .read_data = platform->config->vram,
      .write_byte_fn = VideoCallbackWriteVRAMByte,
  };
  RegisterMemoryMapEntry(platform, &vram_entry);

  PortMapEntry port_entry = {
      .context = platform,
      .entry_type = kPortMapEntryVideo,
      .start = adapter->port_start,
      .end = adapter->port_end,
      .read_byte = VideoCallbackReadPortByte,
      .write_byte = VideoCallbackWritePortByte,
  };
  RegisterPortMapEntry(platform, &port_entry);
}

// Initialize the platform state with the provided configuration. Returns true
// if the platform state was successfully initialized, or false if:
//   - The physical memory size is not between 64K and 640K.
bool PlatformInit(PlatformState* platform, PlatformConfig* config) {
  platform->config = config;
  // Initialized first, ahead of validation, so that a rejected config can
  // still be logged.
  LoggerInit(&platform->logger, config->logger_config);

  if (config->physical_memory_size < kMinPhysicalMemorySize ||
      config->physical_memory_size > kMaxPhysicalMemorySize) {
    YAX86_LOG(
        &platform->logger, &kLogModulePlatform, kLogLevelError,
        "physical_memory_size %u is not between %u and %u bytes",
        (unsigned)config->physical_memory_size,
        (unsigned)kMinPhysicalMemorySize, (unsigned)kMaxPhysicalMemorySize);
    return false;
  }
  // A machine with no memory would run until its first instruction fetch came
  // back as open bus, so this is rejected here rather than left to fail
  // obscurely later.
  if (config->physical_memory == NULL) {
    YAX86_LOG(
        &platform->logger, &kLogModulePlatform, kLogLevelError,
        "no physical_memory buffer was provided");
    return false;
  }
  // The video adapter reads and writes this directly, so an absent buffer
  // would leave the screen permanently blank with nothing to say why.
  if (config->vram == NULL) {
    YAX86_LOG(
        &platform->logger, &kLogModulePlatform, kLogLevelError,
        "no vram buffer was provided");
    return false;
  }

  PlatformInitCPU(platform);
  PlatformInitMemoryMap(platform);
  PlatformInitBIOS(platform);
  PlatformInitPIC(platform);
  PlatformInitPIT(platform);
  PlatformInitPPI(platform);
  PlatformInitKeyboard(platform);
  PlatformInitFDC(platform);
  PlatformInitHDC(platform);
  PlatformInitDMA(platform);
  PlatformInitVideo(platform);

  platform->ticks = 0;
  platform->pit_cycles = 0;
  platform->fdc_cycles = 0;
  platform->keyboard_cycles = 0;
  platform->last_sync_ticks = 0;
  // Schedule the first deadline from the devices' power-on state, so that the
  // first instruction is not treated as already overdue.
  platform->next_event_ticks =
      PlatformCyclesUntilNextEvent(platform, kMaxEventInterval);

  PlatformClearBreakpoints(platform);
  PlatformClearMemoryWatchpoints(platform);
  platform->is_step_mode = false;
  platform->has_stop_info = false;
  platform->stop_pending = false;
  platform->skip_breakpoint_check = false;

  return true;
}

YAX86_HOT bool PlatformRaiseIRQ(PlatformState* platform, uint8_t irq) {
  if (irq >= 8) {
    return false;
  }
  PICRaiseIRQ(&platform->pic, irq);
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
// bring the device up to date first, which is what PlatformSync() is
// for. Getting this wrong shows up as a guest timing loop reading a counter
// that never moves, so port handlers that touch a device call it.

// Half the range of the tick counter. A deadline this far behind the current
// tick or less has come due; anything further away is in the future with the
// counter having wrapped in between. Too large for an enum, which is an int.
//
// Shifted rather than written out, and shifted from a uint32_t rather than an
// int, because shifting a 1 into an int's sign bit is undefined.
static const uint32_t kTickCounterHalfRange = (uint32_t)1 << 31;

// Whether the earliest device deadline has come due. Compared as a difference
// so that a deadline stays in the future across the point where the 32-bit
// cycle counter wraps.
static inline bool PlatformIsEventDue(const PlatformState* platform) {
  // An unsigned comparison against half the range rather than a cast of the
  // difference to int32_t, because converting an out-of-range unsigned value is
  // only defined from C23 on and this is C99.
  return (uint32_t)(platform->ticks - platform->next_event_ticks) <
         kTickCounterHalfRange;
}

// Work out when the next device needs attention, in cycles from now, up to
// max_cycles. The instruction path passes kMaxEventInterval; the idle skip
// passes what is left of the budget it was given, because there the answer is
// how far the clock may be moved rather than how long until it is checked
// again.
static uint32_t PlatformCyclesUntilNextEvent(
    const PlatformState* platform, uint32_t max_cycles) {
  uint32_t cycles = max_cycles;

  // The PIT's next output change, converted from its own 1.19MHz clock and
  // offset by the cycles already credited towards its next tick.
  const uint32_t pit_ticks = PITTicksUntilNextEvent(&platform->pit);
  if (pit_ticks != kPITNoEvent) {
    const uint32_t pit_cycles =
        pit_ticks * kCyclesPerPITTick - platform->pit_cycles;
    if (pit_cycles < cycles) {
      cycles = pit_cycles;
    }
  }

  // The floppy controller only advances a command that is executing; when no
  // command is in flight there is nothing for a tick to do.
  if (platform->fdc.phase == kFDCPhaseExecution) {
    const uint32_t fdc_cycles = kCyclesPerFDCTick - platform->fdc_cycles;
    if (fdc_cycles < cycles) {
      cycles = fdc_cycles;
    }
  }

  // Never return 0, which would leave the deadline permanently due.
  return cycles > 0 ? cycles : 1;
}

// Bring every device up to date with the cycles that have run since the last
// sync, and schedule the next deadline.
YAX86_HOT void PlatformSync(PlatformState* platform) {
  const uint32_t elapsed = platform->ticks - platform->last_sync_ticks;
  platform->last_sync_ticks = platform->ticks;

  if (elapsed > 0) {
    platform->pit_cycles += elapsed;
    const uint32_t pit_ticks = platform->pit_cycles / kCyclesPerPITTick;
    if (pit_ticks > 0) {
      platform->pit_cycles -= pit_ticks * kCyclesPerPITTick;
      PITAdvance(&platform->pit, pit_ticks);
    }

    platform->fdc_cycles += elapsed;
    const uint32_t fdc_ticks = platform->fdc_cycles / kCyclesPerFDCTick;
    if (fdc_ticks > 0) {
      platform->fdc_cycles -= fdc_ticks * kCyclesPerFDCTick;
      // FDCTick() does nothing unless a command is executing, so an idle
      // controller does not need catching up at all - which matters because
      // an idle stretch is not bounded by an FDC deadline.
      if (platform->fdc.phase == kFDCPhaseExecution) {
        for (uint32_t i = 0; i < fdc_ticks; ++i) {
          FDCTick(&platform->fdc);
        }
      }
    }

    platform->keyboard_cycles += elapsed;
    const uint32_t keyboard_ticks =
        platform->keyboard_cycles / kCyclesPerMillisecond;
    if (keyboard_ticks > 0) {
      platform->keyboard_cycles -= keyboard_ticks * kCyclesPerMillisecond;
      for (uint32_t i = 0; i < keyboard_ticks; ++i) {
        KeyboardTickMs(&platform->keyboard);
      }
    }

    // Advancing the beam costs the same whether it moved by one cycle or a
    // whole frame, so the video adapter needs no deadline of its own.
    VideoTick(&platform->video, elapsed);
  }

  platform->next_event_ticks =
      platform->ticks +
      PlatformCyclesUntilNextEvent(platform, kMaxEventInterval);
}

// Stop if there is an enabled breakpoint on the instruction about to execute.
// Only called when has_enabled_breakpoints is set. Returns true if execution
// should stop.
static bool PlatformCheckBreakpoints(PlatformState* platform) {
  const uint16_t cs = platform->cpu.registers[kCS];
  const uint16_t ip = platform->cpu.registers[kIP];
  for (uint8_t i = 0; i < kMaxBreakpoints; ++i) {
    const PlatformBreakpoint* breakpoint = &platform->breakpoints[i];
    if (breakpoint->enabled && breakpoint->cs == cs && breakpoint->ip == ip) {
      PlatformRecordStop(platform, kPlatformStopBreakpoint, i, 0, false);
      return true;
    }
  }
  return false;
}

YAX86_HOT PlatformRunStatus PlatformTick(PlatformState* platform) {
  // Stop before executing the instruction at a breakpoint. Nothing else in the
  // machine is ticked, because no time has passed yet.
  if (platform->has_enabled_breakpoints && !platform->cpu.is_halted) {
    if (platform->skip_breakpoint_check) {
      // Resuming from a breakpoint stop - execute this instruction rather than
      // stopping on it again.
      platform->skip_breakpoint_check = false;
    } else if (PlatformCheckBreakpoints(platform)) {
      platform->skip_breakpoint_check = true;
      return kPlatformStopped;
    }
  }

  // Tick the CPU.
  CPUTickResult cpu_result = CPUTick(&platform->cpu);

  // The instruction took as long as it took, and every device is clocked from
  // that - but in arrears. Rather than offering each device its share of the
  // cycles on every instruction, this only checks whether the earliest device
  // deadline has come due, which is one comparison in the common case.
  const uint16_t cycles = platform->cpu.cycles_this_tick;
  platform->ticks += cycles;
  if (PlatformIsEventDue(platform)) {
    PlatformSync(platform);
  }

  // A watchpoint may have fired from the CPU or from a DMA transfer.
  if (platform->stop_pending) {
    platform->stop_pending = false;
    return kPlatformStopped;
  }

  if (cpu_result == kCPUTickInvalid) {
    return kPlatformInvalid;
  }

  // A halted CPU with interrupts disabled and nothing pending can never be
  // woken. Note that an ordinary halt is not reported as a stop: the PIT and
  // the rest of the machine must keep ticking so that an interrupt can wake
  // the CPU back up.
  if (platform->cpu.is_halted && !CPUGetFlag(&platform->cpu, kIF) &&
      !platform->cpu.has_pending_internal_interrupt) {
    return kPlatformHung;
  }

  if (platform->is_step_mode && cpu_result == kCPUTickExecuted) {
    PlatformRecordStop(platform, kPlatformStopStep, 0, 0, false);
    return kPlatformStopped;
  }

  return kPlatformRunning;
}

// Advance the clock over time the guest has said it has no use for, up to
// max_cycles. Every device is brought up to date across the skipped interval
// rather than having it taken away from them, so the guest's timer tick count
// is the same either way - what is skipped is executing the loop it would have
// spent the interval in.
//
// The bound matters as much as the skip: without it a machine idling at a DOS
// prompt would be handed more emulated time per call than the caller asked for,
// and would run its clock fast. kMaxIdleSkipCycles bounds it a second time, in
// case the caller's budget is itself large enough to overflow the catch-up
// arithmetic.
static void PlatformSkipIdleTime(PlatformState* platform, uint32_t max_cycles) {
  if (max_cycles > kMaxIdleSkipCycles) {
    max_cycles = kMaxIdleSkipCycles;
  }
  const uint32_t cycles = PlatformCyclesUntilNextEvent(platform, max_cycles);
  if (cycles == 0) {
    return;
  }
  platform->ticks += cycles;
  PlatformSync(platform);
}

YAX86_HOT PlatformRunStatus
PlatformRun(PlatformState* platform, uint32_t max_cycles) {
  // Instructions are only ever run whole, so the last one of a run generally
  // takes the total a little past the budget. Unsigned subtraction keeps this
  // right across the counter wrapping.
  const uint32_t start = platform->ticks;
  uint32_t elapsed = 0;
  while (elapsed < max_cycles) {
    PlatformRunStatus status = PlatformTick(platform);
    if (status != kPlatformRunning) {
      return status;
    }
    elapsed = platform->ticks - start;
    // Skipping is done here rather than where the guest declared itself idle
    // because only this loop knows how much of the budget is left, which is
    // what bounds how far the clock may move.
    if (platform->is_guest_idle) {
      platform->is_guest_idle = false;
      if (elapsed < max_cycles) {
        PlatformSkipIdleTime(platform, max_cycles - elapsed);
        elapsed = platform->ticks - start;
      }
    }
  }
  return kPlatformRunning;
}

// ============================================================================
// Breakpoints and watchpoints
// ============================================================================

static void PlatformUpdateDirectDataWindow(PlatformState* platform) {
  CPUInvalidateDirectDataWindow(&platform->cpu);
  // An access through the window is a load or a store, so it cannot fire a
  // watchpoint. While any is enabled the CPU is given nothing and every access
  // goes through ReadMemoryByte() and WriteMemoryByte(), where the check is.
  if (platform->has_enabled_memory_watchpoints) {
    return;
  }
  // The window is a prefix of the address space, so it is whichever region
  // covers address 0 and only while that region is plain storage reached
  // through one buffer in both directions. Conventional memory is that region;
  // a read-only region, or one with a callback in either direction, is not,
  // and is left to the memory map.
  const MemoryMapEntry* entry = GetMemoryMapEntryForAddress(platform, 0);
  if (entry == NULL || entry->start != 0 || entry->read_data == NULL ||
      entry->write_data != entry->read_data) {
    return;
  }
  // A map entry's end is the last address in the region; a window's is one
  // past it.
  CPUSetDirectDataWindow(&platform->cpu, entry->write_data, entry->end + 1);
}

// Recompute the cached hot path early-out flags.
static void PlatformUpdateEnabledFlags(PlatformState* platform) {
  platform->has_enabled_breakpoints = false;
  for (uint8_t i = 0; i < kMaxBreakpoints; ++i) {
    if (platform->breakpoints[i].enabled) {
      platform->has_enabled_breakpoints = true;
      break;
    }
  }
  platform->has_enabled_memory_watchpoints = false;
  for (uint8_t i = 0; i < kMaxMemoryWatchpoints; ++i) {
    if (platform->memory_watchpoints[i].enabled) {
      platform->has_enabled_memory_watchpoints = true;
      break;
    }
  }
  // Instruction fetch reads through a direct window when one is open, which
  // cannot fire a watchpoint. Turning watchpoints on stops the platform
  // handing out new windows, and this discards whichever one is already open.
  CPUInvalidateInstructionFetchWindow(&platform->cpu);
  PlatformUpdateDirectDataWindow(platform);
}

int8_t PlatformAddBreakpoint(
    PlatformState* platform, uint16_t cs, uint16_t ip) {
  for (uint8_t i = 0; i < kMaxBreakpoints; ++i) {
    PlatformBreakpoint* breakpoint = &platform->breakpoints[i];
    if (breakpoint->enabled) {
      continue;
    }
    breakpoint->enabled = true;
    breakpoint->cs = cs;
    breakpoint->ip = ip;
    PlatformUpdateEnabledFlags(platform);
    return (int8_t)i;
  }
  return kInvalidWatchIndex;
}

bool PlatformRemoveBreakpoint(PlatformState* platform, uint8_t index) {
  if (index >= kMaxBreakpoints || !platform->breakpoints[index].enabled) {
    return false;
  }
  platform->breakpoints[index].enabled = false;
  PlatformUpdateEnabledFlags(platform);
  return true;
}

void PlatformClearBreakpoints(PlatformState* platform) {
  for (uint8_t i = 0; i < kMaxBreakpoints; ++i) {
    platform->breakpoints[i].enabled = false;
  }
  PlatformUpdateEnabledFlags(platform);
}

int8_t PlatformAddMemoryWatchpoint(
    PlatformState* platform, uint32_t start, uint32_t end, bool on_read,
    bool on_write) {
  if (start > end || (!on_read && !on_write)) {
    return kInvalidWatchIndex;
  }
  for (uint8_t i = 0; i < kMaxMemoryWatchpoints; ++i) {
    PlatformMemoryWatchpoint* watchpoint = &platform->memory_watchpoints[i];
    if (watchpoint->enabled) {
      continue;
    }
    watchpoint->enabled = true;
    watchpoint->start = start;
    watchpoint->end = end;
    watchpoint->on_read = on_read;
    watchpoint->on_write = on_write;
    PlatformUpdateEnabledFlags(platform);
    return (int8_t)i;
  }
  return kInvalidWatchIndex;
}

bool PlatformRemoveMemoryWatchpoint(PlatformState* platform, uint8_t index) {
  if (index >= kMaxMemoryWatchpoints ||
      !platform->memory_watchpoints[index].enabled) {
    return false;
  }
  platform->memory_watchpoints[index].enabled = false;
  PlatformUpdateEnabledFlags(platform);
  return true;
}

void PlatformClearMemoryWatchpoints(PlatformState* platform) {
  for (uint8_t i = 0; i < kMaxMemoryWatchpoints; ++i) {
    platform->memory_watchpoints[i].enabled = false;
  }
  PlatformUpdateEnabledFlags(platform);
}

void PlatformSetStepMode(PlatformState* platform, bool is_step_mode) {
  platform->is_step_mode = is_step_mode;
}

const PlatformStopInfo* PlatformGetStopInfo(const PlatformState* platform) {
  return platform->has_stop_info ? &platform->stop_info : NULL;
}


// ==============================================================================
// src/platform/platform.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_PLATFORM_BUNDLE_H

