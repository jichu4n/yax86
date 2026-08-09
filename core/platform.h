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
  // Callback to read a byte from the memory map entry, where address is
  // relative to the start of the entry.
  uint8_t (*read_byte)(struct MemoryMapEntry* entry, uint32_t relative_address);
  // Callback to write a byte to memory, where address is relative to the start
  // address.
  void (*write_byte)(
      struct MemoryMapEntry* entry, uint32_t relative_address, uint8_t value);
} MemoryMapEntry;

// Register a memory map entry in the platform state. Returns true if the entry
// was successfully registered, or false if:
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

// Read a byte from a logical memory address by invoking the corresponding
// memory map entry's read_byte callback.
//
// On the 8086, accessing an invalid memory address will yield garbage data
// rather than causing a page fault. This callback interface mirrors that
// behavior.
uint8_t ReadMemoryByte(struct PlatformState* platform, uint32_t address);
// Read a word from a logical memory address by invoking the corresponding
// memory map entry's read_byte callback.
uint16_t ReadMemoryWord(struct PlatformState* platform, uint32_t address);
// Write a byte to a logical memory address by invoking the corresponding
// memory map entry's write_byte callback.
//
// On the 8086, accessing an invalid memory address will yield garbage data
// rather than causing a page fault. This callback interface mirrors that
// behavior.
void WriteMemoryByte(
    struct PlatformState* platform, uint32_t address, uint8_t value);
// Write a word to a logical memory address by invoking the corresponding
// memory map entry's write_byte callback.
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

  // Callback to read a byte from physical memory.
  //
  // On the 8086, accessing an invalid memory address will yield garbage data
  // rather than causing a page fault. This callback interface mirrors that
  // behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  uint8_t (*read_physical_memory_byte)(
      struct PlatformState* platform, uint32_t address);

  // Callback to write a byte to physical memory.
  //
  // On the 8086, accessing an invalid memory address will yield garbage data
  // rather than causing a page fault. This callback interface mirrors that
  // behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  void (*write_physical_memory_byte)(
      struct PlatformState* platform, uint32_t address, uint8_t value);

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

  // MDA runtime configuration.
  MDAConfig mda_config;
  // MDA state.
  MDAState mda;

  // Memory map.
  MemoryMap memory_map;
  // I/O port map.
  PortMap io_port_map;

  // How many CPU clock cycles have run, at 4.77MHz. Every device in the
  // machine is clocked from this, so that what the guest measures with the
  // timer matches how long it spent executing.
  uint32_t ticks;

  // Cycles counted towards each device's next tick but not yet used by it.
  uint16_t pit_cycles;
  uint16_t fdc_cycles;
  uint16_t keyboard_cycles;

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
bool PlatformInit(PlatformState* platform, PlatformConfig* config);

// Raise a hardware interrupt to the CPU via the PIC. Returns true if the
// IRQ was successfully raised, or false if the IRQ number is invalid.
bool PlatformRaiseIRQ(PlatformState* platform, uint8_t irq);

// Run a single cycle of the platform, including ticking all sub-modules. This
// should be called at the CPU clock rate (4.77MHz for the 8088).
//
// Returns kPlatformRunning if the machine should keep running.
PlatformRunStatus PlatformTick(PlatformState* platform);

// Run up to max_ticks cycles of the platform, stopping early if a tick returns
// anything other than kPlatformRunning. Returns the status of the tick that
// stopped the run, or kPlatformRunning if the full budget was consumed.
PlatformRunStatus PlatformRun(PlatformState* platform, uint32_t max_cycles);

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

// Register a memory map entry in the platform state. Returns true if the entry
// was successfully registered, or false if:
//   - There already exists a memory map entry with the same type.
//   - The new entry's memory region overlaps with an existing entry.
//   - The number of memory map entries would exceed kMaxMemoryMapEntries.
bool RegisterMemoryMapEntry(
    PlatformState* platform, const MemoryMapEntry* entry) {
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
  return MemoryMapAppend(&platform->memory_map, entry);
}

// Look up the memory region corresponding to an address. Returns NULL if the
// address is not mapped to a known memory region.
MemoryMapEntry* GetMemoryMapEntryForAddress(
    PlatformState* platform, uint32_t address) {
  // TODO: Use a more efficient data structure for lookups, such as a sorted
  // array with binary search.
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
MemoryMapEntry* GetMemoryMapEntryByType(
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
uint8_t ReadMemoryByte(PlatformState* platform, uint32_t address) {
  if (platform->has_enabled_memory_watchpoints) {
    PlatformCheckMemoryWatchpoints(platform, address, false);
  }
  MemoryMapEntry* entry = GetMemoryMapEntryForAddress(platform, address);
  if (!entry || !entry->read_byte) {
    // Logged at debug rather than warning level: scanning unmapped memory is
    // normal on a PC/XT. GLaBIOS reads every byte of 0xF6000-0xF7FFF looking
    // for option ROMs, for instance.
    YAX86_PLATFORM_LOG(
        kLogLevelDebug, "read from unmapped address %05X", address);
    return 0xFF;
  }
  return entry->read_byte(entry, address - entry->start);
}

// Read a word from a logical memory address.
uint16_t ReadMemoryWord(PlatformState* platform, uint32_t address) {
  uint8_t low_byte = ReadMemoryByte(platform, address);
  uint8_t high_byte = ReadMemoryByte(platform, address + 1);
  return (high_byte << 8) | low_byte;
}

// Write a byte to a logical memory address.
void WriteMemoryByte(PlatformState* platform, uint32_t address, uint8_t value) {
  if (platform->has_enabled_memory_watchpoints) {
    PlatformCheckMemoryWatchpoints(platform, address, true);
  }
  MemoryMapEntry* entry = GetMemoryMapEntryForAddress(platform, address);
  if (!entry || !entry->write_byte) {
    YAX86_PLATFORM_LOG(
        kLogLevelDebug, "write of %02X to unmapped address %05X", value,
        address);
    return;
  }
  entry->write_byte(entry, address - entry->start, value);
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
PortMapEntry* GetPortMapEntryByType(
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
uint8_t ReadPortByte(PlatformState* platform, uint16_t port) {
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
void WritePortByte(PlatformState* platform, uint16_t port, uint8_t value) {
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
// Callbacks for physical memory
// ============================================================================
static uint8_t ReadPhysicalMemoryByte(MemoryMapEntry* entry, uint32_t address) {
  PlatformState* platform = (PlatformState*)entry->context;
  if (platform->config && platform->config->read_physical_memory_byte) {
    return platform->config->read_physical_memory_byte(platform, address);
  }
  return 0xFF;
}

static void WritePhysicalMemoryByte(
    MemoryMapEntry* entry, uint32_t address, uint8_t value) {
  PlatformState* platform = (PlatformState*)entry->context;
  if (platform->config && platform->config->write_physical_memory_byte) {
    platform->config->write_physical_memory_byte(platform, address, value);
  }
}

// ============================================================================
// Callbacks for 8259 PIC module
// ============================================================================

static uint8_t PICCallbackReadPortByte(PortMapEntry* entry, uint16_t port) {
  return PICReadPort((PICState*)entry->context, port);
}

static void PICCallbackWritePortByte(
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

static uint8_t PITCallbackReadPortByte(PortMapEntry* entry, uint16_t port) {
  return PITReadPort((PITState*)entry->context, port);
}

static void PITCallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  PITWritePort((PITState*)entry->context, port, value);
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
  return PPIReadPort((PPIState*)entry->context, port);
}

static void PPICallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  PPIWritePort((PPIState*)entry->context, port, value);
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

static void FDCCallbackRequestDMA(void* context) {
  PlatformState* platform = (PlatformState*)context;
  DMATransferByte(&platform->dma, kPlatformDMAChannelFloppy);
}

static uint8_t FDCCallbackReadPortByte(PortMapEntry* entry, uint16_t port) {
  return FDCReadPort((FDCState*)entry->context, port);
}

static void FDCCallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  FDCWritePort((FDCState*)entry->context, port, value);
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
// Callbacks for MDA module
// ============================================================================

static uint8_t MDACallbackReadPortByte(PortMapEntry* entry, uint16_t port) {
  return MDAReadPort((MDAState*)entry->context, port);
}

static void MDACallbackWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  MDAWritePort((MDAState*)entry->context, port, value);
}

static uint8_t MDACallbackReadVRAMByte(
    MemoryMapEntry* entry, uint32_t address) {
  return MDAReadVRAM((MDAState*)entry->context, address);
}

static void MDACallbackWriteVRAMByte(
    MemoryMapEntry* entry, uint32_t address, uint8_t value) {
  MDAWriteVRAM((MDAState*)entry->context, address, value);
}

// ============================================================================
// Callbacks for HDC module
// ============================================================================

static uint8_t HDCCallbackReadOptionROMByte(
    YAX86_UNUSED MemoryMapEntry* entry, uint32_t address) {
  return HDCReadOptionROMByte(address);
}

// ============================================================================
// Callbacks for BIOS module
// ============================================================================

static uint8_t BIOSCallbackReadROMByte(
    YAX86_UNUSED MemoryMapEntry* entry, uint32_t address) {
  return BIOSReadROMByte(address);
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
      .read_byte = BIOSCallbackReadROMByte,
      .write_byte = NULL,  // BIOS ROM is read-only.
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

static void PlatformInitCPU(PlatformState* platform) {
  platform->cpu_config = kEmptyCPUConfig;
  platform->cpu_config.context = platform;
  platform->cpu_config.logger = &platform->logger;
  platform->cpu_config.read_memory_byte = CPUCallbackReadMemoryByte;
  platform->cpu_config.write_memory_byte = CPUCallbackWriteMemoryByte;
  platform->cpu_config.acknowledge_interrupt = CPUCallbackAcknowledgeInterrupt;
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
  MemoryMapEntry conventional_memory = {
      .context = platform,
      .entry_type = kMemoryMapEntryConventional,
      .start = 0x0000,
      .end = platform->config->physical_memory_size - 1,
      .read_byte = ReadPhysicalMemoryByte,
      .write_byte = WritePhysicalMemoryByte};
  MemoryMapAppend(&platform->memory_map, &conventional_memory);
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
      .context = &platform->pit,
  };
  RegisterPortMapEntry(platform, &pit_entry);
}

static void PlatformInitPPI(PlatformState* platform) {
  platform->ppi_config.context = platform;
  platform->ppi_config.logger = &platform->logger;
  platform->ppi_config.num_floppy_drives = 1;
  platform->ppi_config.memory_size = kPPIMemorySize256KB;
  platform->ppi_config.display_mode = kPPIDisplayMDA;
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
      .context = &platform->ppi,
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
      .context = &platform->fdc,
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
      .read_byte = HDCCallbackReadOptionROMByte,
      .write_byte = NULL,  // Option ROM is read-only.
  };
  RegisterMemoryMapEntry(platform, &option_rom_entry);
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

static void PlatformInitMDA(PlatformState* platform) {
  platform->mda_config = kDefaultMDAConfig;
  platform->mda_config.context = platform;
  platform->mda_config.logger = &platform->logger;
  MDAInit(&platform->mda, &platform->mda_config);

  MemoryMapEntry vram_entry = {
      .context = &platform->mda,
      .entry_type = kMemoryMapEntryMDAVRAM,
      .start = kMDAModeMetadata.vram_address,
      .end = kMDAModeMetadata.vram_address + kMDAModeMetadata.vram_size - 1,
      .read_byte = MDACallbackReadVRAMByte,
      .write_byte = MDACallbackWriteVRAMByte,
  };
  RegisterMemoryMapEntry(platform, &vram_entry);

  PortMapEntry port_entry = {
      .context = &platform->mda,
      .entry_type = kPortMapEntryMDA,
      .start = 0x3B0,
      .end = 0x3BF,
      .read_byte = MDACallbackReadPortByte,
      .write_byte = MDACallbackWritePortByte,
  };
  RegisterPortMapEntry(platform, &port_entry);
}

// Initialize the platform state with the provided configuration. Returns true
// if the platform state was successfully initialized, or false if:
//   - The physical memory size is not between 64K and 640K.
bool PlatformInit(PlatformState* platform, PlatformConfig* config) {
  if (config->physical_memory_size < kMinPhysicalMemorySize ||
      config->physical_memory_size > kMaxPhysicalMemorySize) {
    return false;
  }

  platform->config = config;
  LoggerInit(&platform->logger, config->logger_config);

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
  PlatformInitMDA(platform);

  platform->ticks = 0;

  PlatformClearBreakpoints(platform);
  PlatformClearMemoryWatchpoints(platform);
  platform->is_step_mode = false;
  platform->has_stop_info = false;
  platform->stop_pending = false;
  platform->skip_breakpoint_check = false;

  return true;
}

bool PlatformRaiseIRQ(PlatformState* platform, uint8_t irq) {
  if (irq >= 8) {
    return false;
  }
  PICRaiseIRQ(&platform->pic, irq);
  return true;
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

PlatformRunStatus PlatformTick(PlatformState* platform) {
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
  // that. Each keeps its own remainder, so a device whose period does not
  // divide the instruction's length still runs at its own rate on average
  // rather than drifting.
  const uint16_t cycles = platform->cpu.cycles_this_tick;
  platform->ticks += cycles;

  platform->pit_cycles += cycles;
  while (platform->pit_cycles >= kCyclesPerPITTick) {
    platform->pit_cycles -= kCyclesPerPITTick;
    PITTick(&platform->pit);
  }

  platform->fdc_cycles += cycles;
  while (platform->fdc_cycles >= kCyclesPerFDCTick) {
    platform->fdc_cycles -= kCyclesPerFDCTick;
    FDCTick(&platform->fdc);
  }

  platform->keyboard_cycles += cycles;
  while (platform->keyboard_cycles >= kCyclesPerMillisecond) {
    platform->keyboard_cycles -= kCyclesPerMillisecond;
    KeyboardTickMs(&platform->keyboard);
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

PlatformRunStatus PlatformRun(PlatformState* platform, uint32_t max_cycles) {
  // Instructions are only ever run whole, so the last one of a run generally
  // takes the total a little past the budget. Unsigned subtraction keeps this
  // right across the counter wrapping.
  const uint32_t start = platform->ticks;
  while (platform->ticks - start < max_cycles) {
    PlatformRunStatus status = PlatformTick(platform);
    if (status != kPlatformRunning) {
      return status;
    }
  }
  return kPlatformRunning;
}

// ============================================================================
// Breakpoints and watchpoints
// ============================================================================

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
    platform->has_enabled_breakpoints = true;
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
  platform->has_enabled_breakpoints = false;
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
    platform->has_enabled_memory_watchpoints = true;
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
  platform->has_enabled_memory_watchpoints = false;
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

