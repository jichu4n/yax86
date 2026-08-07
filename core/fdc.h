// ==============================================================================
// YAX86 FDC MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_FDC_BUNDLE_H
#define YAX86_FDC_BUNDLE_H

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
// src/fdc/public.h start
// ==============================================================================

#line 1 "./src/fdc/public.h"
// Public interface for the Floppy Disk Controller (FDC) module.
#ifndef YAX86_FDC_PUBLIC_H
#define YAX86_FDC_PUBLIC_H

// This module emulates the NEC uPD765 Floppy Disk Controller. It handles I/O
// port communication and DMA transfers for floppy operations. Actual disk
// image access is delegated to the platform via callbacks.

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_FDC_BUNDLE_H
#include "../util/log.h"
#include "../util/static_vector.h"
#endif  // YAX86_FDC_BUNDLE_H

enum {
  // Log module ID for the FDC.
  kLogModuleIDFDC = 5,
};

// Log module for the FDC.
static const LogModule kLogModuleFDC = {
    .id = kLogModuleIDFDC,
    .name = "FDC",
};

// Floppy disk format configuration.
typedef struct FDCDiskFormat {
  // Number of heads (1 or 2).
  uint8_t num_heads;
  // Number of tracks.
  uint8_t num_tracks;
  // Number of sectors per track.
  uint8_t num_sectors_per_track;
  // Size of each sector in bytes.
  uint16_t sector_size;
} FDCDiskFormat;

// 5.25" 360KB double-sided double-density floppy disk format.
static const FDCDiskFormat kFDCFormat360KB = {
    .num_heads = 2,
    .num_tracks = 40,
    .num_sectors_per_track = 9,
    .sector_size = 512,
};

struct FDCState;

enum {
  // Number of floppy drives supported by the FDC.
  kFDCNumDrives = 4,
  // Maximum size of a command request.
  kFDCCommandBufferSize = 9,
  // Maximum size of a command result.
  kFDCResultBufferSize = 7,
};

// Command phases of the FDC.
typedef enum FDCCommandPhase {
  // No command in progress.
  kFDCPhaseIdle = 0,
  // Command has been issued, waiting for parameters.
  kFDCPhaseCommand,
  // Command parameters received, executing command.
  kFDCPhaseExecution,
  // Command execution complete, sending result bytes.
  kFDCPhaseResult,
} FDCCommandPhase;

// I/O ports for the FDC.
typedef enum FDCPort {
  // Digital Output Register (write-only).
  kFDCPortDOR = 0x3F2,
  // Main Status Register (read-only).
  kFDCPortMSR = 0x3F4,
  // Data Register (read/write).
  kFDCPortData = 0x3F5,
} FDCPort;

// Flags for the Main Status Register (MSR).
enum {
  // Drive 0 is busy with a seek or recalibrate command.
  kFDCMSRDrive0Busy = 1 << 0,
  // Drive 1 is busy with a seek or recalibrate command.
  kFDCMSRDrive1Busy = 1 << 1,
  // Drive 2 is busy with a seek or recalibrate command.
  kFDCMSRDrive2Busy = 1 << 2,
  // Drive 3 is busy with a seek or recalibrate command.
  kFDCMSRDrive3Busy = 1 << 3,
  // A command is in progress.
  kFDCMSRBusy = 1 << 4,
  // The FDC is in non-DMA mode.
  kFDCMSRNonDMAMode = 1 << 5,
  // Indicates direction of data transfer. 0 = write to FDC, 1 = read from FDC.
  kFDCMSRDataDirection = 1 << 6,
  // The Data Register is ready to send or receive data to/from the CPU.
  kFDCMSRRequestForMaster = 1 << 7,
};

// Flags for the Digital Output Register (DOR).
enum {
  // Drive selection (0-3).
  kFDCDORDriveSelectMask = 0x03,
  // Controller reset (0 = Reset active, 1 = Controller enabled).
  kFDCDORReset = 1 << 2,
  // DMA and Interrupt enable (1 = enabled).
  kFDCDORInterruptEnable = 1 << 3,
  // Motor enable flags for drives 0-3.
  kFDCDORMotor0Enable = 1 << 4,
  kFDCDORMotor1Enable = 1 << 5,
  kFDCDORMotor2Enable = 1 << 6,
  kFDCDORMotor3Enable = 1 << 7,
};

// Flags for Status Register 0 (ST0).
enum {
  // Bits 7-6: Interrupt Code
  // 00 = Normal termination
  // 01 = Abnormal termination
  // 10 = Invalid command
  // 11 = Abnormal termination due to polling (Post-reset)
  kFDCST0InterruptCodeMask = 0xC0,
  kFDCST0NormalTermination = 0x00,
  kFDCST0AbnormalTermination = 0x40,
  kFDCST0InvalidCommand = 0x80,
  kFDCST0AbnormalTerminationPolling = 0xC0,

  // Bit 5: Seek End
  kFDCST0SeekEnd = 1 << 5,
  // Bit 4: Equipment Check
  kFDCST0EquipmentCheck = 1 << 4,
  // Bit 3: Not Ready
  kFDCST0NotReady = 1 << 3,
  // Bit 2: Head Address
  kFDCST0HeadAddress = 1 << 2,
  // Bits 1-0: Drive Select
  kFDCST0UnitSelectMask = 0x03,
};

// Flags for Status Register 1 (ST1).
enum {
  // Bit 7: End of Cylinder
  kFDCST1EndOfCylinder = 1 << 7,
  // Bit 5: Data Error
  kFDCST1DataError = 1 << 5,
  // Bit 4: Overrun
  kFDCST1Overrun = 1 << 4,
  // Bit 2: No Data
  kFDCST1NoData = 1 << 2,
  // Bit 1: Not Writable
  kFDCST1NotWritable = 1 << 1,
  // Bit 0: Missing Address Mark
  kFDCST1MissingAddressMark = 1 << 0,
};

// Flags for Status Register 2 (ST2).
enum {
  // Bit 6: Control Mark
  kFDCST2ControlMark = 1 << 6,
  // Bit 5: Data Error in Data Field
  kFDCST2DataErrorInDataField = 1 << 5,
  // Bit 4: Wrong Cylinder
  kFDCST2WrongCylinder = 1 << 4,
  // Bit 3: Scan Equal Hit
  kFDCST2ScanEqualHit = 1 << 3,
  // Bit 2: Scan Not Satisfied
  kFDCST2ScanNotSatisfied = 1 << 2,
  // Bit 1: Bad Cylinder
  kFDCST2BadCylinder = 1 << 1,
  // Bit 0: Missing Address Mark in Data Field
  kFDCST2MissingAddressMarkInDataField = 1 << 0,
};

// State for a single floppy drive.
typedef struct FDCDriveState {
  // Whether there is a disk inserted in the drive, i.e. whether an image is
  // mounted. The real hardware doesn't actually know this and will attempt to
  // access the disk and time out.
  bool present;
  // The format of the disk currently inserted in the drive.
  const FDCDiskFormat* format;
  // The track the read/write head is currently on.
  uint8_t track;
  // The currently active head (0 or 1).
  uint8_t head;
  // Whether the drive is currently busy.
  bool busy;
  // Status Register 0 (ST0) for the last completed Seek or Recalibrate
  // operation on this drive.
  uint8_t st0;
  // Whether there is a pending interrupt from a completed Seek or Recalibrate
  // operation on this drive.
  bool has_pending_interrupt;
} FDCDriveState;

// Caller-provided runtime configuration for the FDC.
typedef struct FDCConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger for this module. May be NULL.
  Logger* logger;

  // Callback to raise an IRQ6 (FDC interrupt) to the CPU.
  void (*raise_irq6)(void* context);

  // Callback to signal the platform to execute a DMA cycle for Channel 2.
  // This represents the DREQ (DMA Request) signal.
  void (*request_dma)(void* context);

  // Callback to read a byte from a floppy image.
  uint8_t (*read_image_byte)(
      void* context,
      // 0 to kFDCNumDrives-1
      uint8_t drive,
      // byte offset within the image
      uint32_t offset);

  // Callback to write a byte to a floppy image.
  void (*write_image_byte)(
      void* context,
      // 0 to kFDCNumDrives-1
      uint8_t drive,
      // byte offset within the image
      uint32_t offset,
      // byte value to write
      uint8_t value);
} FDCConfig;

STATIC_VECTOR_TYPE(FDCCommandBuffer, uint8_t, kFDCCommandBufferSize)
STATIC_VECTOR_TYPE(FDCResultBuffer, uint8_t, kFDCResultBufferSize)

struct FDCCommandMetadata;

// State of the Floppy Disk Controller.
typedef struct FDCState {
  // Pointer to the FDC configuration.
  FDCConfig* config;

  // Value of the Digital Output Register (DOR) from the last write to port
  // 0x3F2.
  uint8_t dor;

  // Per-drive state.
  FDCDriveState drives[kFDCNumDrives];

  // Current command phase.
  FDCCommandPhase phase;

  // Command buffer to receive command and parameters from the CPU.
  FDCCommandBuffer command_buffer;
  // Metadata for the command currently being processed.
  const struct FDCCommandMetadata* current_command;
  // How many ticks the current command has been executing.
  uint32_t current_command_ticks;

  // Result buffer to send to the CPU.
  FDCResultBuffer result_buffer;
  // Next index to read from result buffer.
  uint8_t next_result_byte_index;

  // State specific to the execution of a data transfer command
  // (Read/Write/Format).
  struct {
    // Current logical position on the disk during transfer.
    uint8_t cylinder;
    uint8_t head;
    uint8_t sector;

    // Command parameters governing the transfer.
    uint8_t sector_size_code;  // N
    uint8_t eot;               // End of Track sector number
    bool multi_track;          // MT bit set (read/write across heads)

    // Internal tracking.
    uint32_t current_offset;  // Current byte offset in the disk image.
    uint16_t
        sector_byte_index;  // Current byte index within the current sector.
    uint8_t data_register;  // Buffer for the byte currently being transferred.
    bool dma_request_active;  // DREQ is asserted, waiting for DMA access.
    bool tc_received;         // TC (Terminal Count) signal received from DMA.
  } transfer;
} FDCState;

// Initializes the FDC to its power-on state.
void FDCInit(FDCState* fdc, FDCConfig* config);

// Handles reads from the FDC's I/O ports.
uint8_t FDCReadPort(FDCState* fdc, uint16_t port);

// Handles writes to the FDC's I/O ports.
void FDCWritePort(FDCState* fdc, uint16_t port, uint8_t value);

// Signals to the FDC that the DMA controller has reached the terminal count.
// This represents the TC signal.
void FDCHandleTC(FDCState* fdc);

// Inserts a disk with the given format into the specified drive.
void FDCInsertDisk(FDCState* fdc, uint8_t drive, const FDCDiskFormat* format);

// Ejects the disk from the specified drive.
void FDCEjectDisk(FDCState* fdc, uint8_t drive);

// Simulates a tick of the FDC, handling any timed operations.
void FDCTick(FDCState* fdc);

#endif  // YAX86_FDC_PUBLIC_H


// ==============================================================================
// src/fdc/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/fdc/fdc.c start
// ==============================================================================

#line 1 "./src/fdc/fdc.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

#define YAX86_FDC_LOG(level, ...) \
  YAX86_LOG(fdc->config->logger, &kLogModuleFDC, level, __VA_ARGS__)

#include <stddef.h>

enum {
  // Invalid offset.
  kFDCInvalidOffset = 0xFFFFFFF,
};

// FDC command opcodes.
typedef enum FDCCommand {
  // Read a Track
  kFDCCmdReadTrack = 0x02,
  // Specify
  kFDCCmdSpecify = 0x03,
  // Sense Drive Status
  kFDCCmdSenseDriveStatus = 0x04,
  // Write Data
  kFDCCmdWriteData = 0x05,
  // Read Data
  kFDCCmdReadData = 0x06,
  // Recalibrate
  kFDCCmdRecalibrate = 0x07,
  // Sense Interrupt Status
  kFDCCmdSenseInterruptStatus = 0x08,
  // Write Deleted Data
  kFDCCmdWriteDeletedData = 0x09,
  // Read ID
  kFDCCmdReadID = 0x0A,
  // Read Deleted Data
  kFDCCmdReadDeletedData = 0x0C,
  // Format a Track
  kFDCCmdFormatTrack = 0x0D,
  // Seek
  kFDCCmdSeek = 0x0F,
  // Scan Equal
  kFDCCmdScanEqual = 0x11,
  // Scan Low or Equal
  kFDCCmdScanLowOrEqual = 0x19,
  // Scan High or Equal
  kFDCCmdScanHighOrEqual = 0x1D,
} FDCCommand;

// Metadata for each FDC command.
typedef struct FDCCommandMetadata {
  // Base opcode of the command (lower 5 bits).
  uint8_t opcode;
  // Number of parameter bytes expected.
  uint8_t num_param_bytes;
  // Handler to execute the command.
  void (*handler)(FDCState* fdc);
} FDCCommandMetadata;

// Helper to raise an IRQ6 if the callback is set and interrupts are enabled in
// DOR.
static inline void FDCRaiseIRQ6(FDCState* fdc) {
  if (fdc->config && fdc->config->raise_irq6 &&
      (fdc->dor & kFDCDORInterruptEnable)) {
    fdc->config->raise_irq6(fdc->config->context);
  }
}

// Transition into execution phase.
static inline void FDCStartCommandExecution(FDCState* fdc) {
  YAX86_FDC_LOG(
      kLogLevelDebug, "executing command %02X with %u parameter bytes",
      fdc->current_command ? fdc->current_command->opcode : 0,
      (unsigned)FDCCommandBufferLength(&fdc->command_buffer) - 1);
  fdc->phase = kFDCPhaseExecution;
  fdc->current_command_ticks = 0;
}

// Transition into result phase after command execution.
static inline void FDCFinishCommandExecution(FDCState* fdc) {
  YAX86_FDC_LOG(
      kLogLevelDebug, "command finished with %u result bytes, st0 %02X",
      (unsigned)FDCResultBufferLength(&fdc->result_buffer),
      FDCResultBufferLength(&fdc->result_buffer) > 0
          ? *FDCResultBufferGet(&fdc->result_buffer, 0)
          : 0);
  if (FDCResultBufferLength(&fdc->result_buffer) == 0) {
    // No result bytes to send, go back to idle phase.
    fdc->phase = kFDCPhaseIdle;
  } else {
    // Has result bytes to send, go to result phase.
    fdc->phase = kFDCPhaseResult;
    fdc->next_result_byte_index = 0;
  }
}

// Helper to perform a seek operation (for Seek and Recalibrate).
static void FDCPerformSeek(
    FDCState* fdc, uint8_t drive_index, uint8_t target_track) {
  FDCDriveState* drive = &fdc->drives[drive_index];

  // On initial tick, start seeking.
  if (fdc->current_command_ticks == 0) {
    drive->busy = true;
    return;
  }

  // On 2nd tick, seek is complete.
  drive->track = target_track;
  drive->busy = false;

  // Set Status Register 0 (ST0)
  // Bits 7-6: Interrupt Code = 00 (Normal Termination)
  // Bit 5: Seek End = 1
  // Bits 1-0: Unit Select (Drive Index)
  drive->st0 = kFDCST0NormalTermination | kFDCST0SeekEnd | drive_index;

  // Set interrupt pending flag for this drive.
  drive->has_pending_interrupt = true;

  // Raise Interrupt (IRQ6).
  FDCRaiseIRQ6(fdc);

  FDCFinishCommandExecution(fdc);
}

// Compute the byte offset within a disk image for the given address. Returns
// kFDCInvalidOffset if the address is out of range.
//
// In a raw image file, the data is laid out track by track, starting from the
// outermost track (Track 0). Within each track, it reads all the data from the
// first head (Head 0, the top side) and then all the data from the second head
// (Head 1, the bottom side) before moving to the next track.
// In other words, the layout is an array of
// [num_tracks][num_heads][num_sectors_per_track].
static inline uint32_t FDCComputeOffset(
    FDCDiskFormat format, uint8_t head, uint8_t track, uint8_t sector,
    uint16_t sector_offset) {
  if (head >= format.num_heads || track >= format.num_tracks || sector == 0 ||
      sector > format.num_sectors_per_track ||
      sector_offset >= format.sector_size) {
    return kFDCInvalidOffset;
  }

  uint32_t offset = 0;
  // Seek to start of the track
  offset += (uint32_t)track * format.num_heads * format.num_sectors_per_track *
            format.sector_size;
  // Seek to start of the head within the track
  offset += (uint32_t)head * format.num_sectors_per_track * format.sector_size;
  // Seek to start of the sector within the head
  offset += (uint32_t)(sector - 1) * format.sector_size;
  // Add byte offset within the sector
  offset += (uint32_t)sector_offset;

  return offset;
}

// Helper to finish a read/write command.
static void FDCFinishReadWrite(
    FDCState* fdc, uint8_t st0, uint8_t st1, uint8_t st2) {
  FDCResultBufferAppend(&fdc->result_buffer, &st0);
  FDCResultBufferAppend(&fdc->result_buffer, &st1);
  FDCResultBufferAppend(&fdc->result_buffer, &st2);
  FDCResultBufferAppend(&fdc->result_buffer, &fdc->transfer.cylinder);
  FDCResultBufferAppend(&fdc->result_buffer, &fdc->transfer.head);
  FDCResultBufferAppend(&fdc->result_buffer, &fdc->transfer.sector);
  FDCResultBufferAppend(&fdc->result_buffer, &fdc->transfer.sector_size_code);

  FDCRaiseIRQ6(fdc);
  FDCFinishCommandExecution(fdc);
}

// Handler for Write Data command.
static void FDCHandleWriteData(FDCState* fdc) {
  if (fdc->current_command_ticks == 0) {
    // Initialization.
    uint8_t cmd_byte = *FDCCommandBufferGet(&fdc->command_buffer, 0);
    fdc->transfer.multi_track = (cmd_byte & 0x80) != 0;

    uint8_t drive_head = *FDCCommandBufferGet(&fdc->command_buffer, 1);
    uint8_t drive_index = drive_head & 0x03;
    uint8_t head_address = (drive_head >> 2) & 0x01;

    fdc->transfer.cylinder = *FDCCommandBufferGet(&fdc->command_buffer, 2);
    fdc->transfer.head = *FDCCommandBufferGet(&fdc->command_buffer, 3);
    fdc->transfer.sector = *FDCCommandBufferGet(&fdc->command_buffer, 4);
    fdc->transfer.sector_size_code =
        *FDCCommandBufferGet(&fdc->command_buffer, 5);
    fdc->transfer.eot = *FDCCommandBufferGet(&fdc->command_buffer, 6);
    // GPL and DTL are ignored.

    FDCDriveState* drive = &fdc->drives[drive_index];
    if (!drive->present) {
      // Drive not ready.
      FDCFinishReadWrite(
          fdc,
          kFDCST0AbnormalTermination | kFDCST0NotReady | head_address << 2 |
              drive_index,
          0, 0);
      return;
    }

    // Calculate offset for the first byte.
    fdc->transfer.current_offset = FDCComputeOffset(
        *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
        fdc->transfer.sector, 0);

    if (fdc->transfer.current_offset == kFDCInvalidOffset) {
      FDCFinishReadWrite(
          fdc, kFDCST0AbnormalTermination | head_address << 2 | drive_index,
          kFDCST1NoData, 0);
      return;
    }

    fdc->transfer.sector_byte_index = 0;
    // For Write, we need to request the first byte immediately.
    fdc->transfer.dma_request_active = true;
    fdc->transfer.tc_received = false;
    if (fdc->config && fdc->config->request_dma) {
      fdc->config->request_dma(fdc->config->context);
    }
    return;
  }

  // Execution Loop.

  // Check for Terminal Count (TC).
  if (fdc->transfer.tc_received) {
    // Transfer complete.
    uint8_t drive_index = *FDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
    uint8_t head_address = (fdc->transfer.head & 0x01);
    FDCFinishReadWrite(
        fdc, kFDCST0NormalTermination | head_address << 2 | drive_index, 0, 0);
    return;
  }

  // If DREQ is active, wait for the system to service it (write byte to us).
  if (fdc->transfer.dma_request_active) {
    return;
  }

  // Data has arrived in data_register. Write it to image.
  uint8_t drive_index = *FDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
  if (fdc->config && fdc->config->write_image_byte) {
    fdc->config->write_image_byte(
        fdc->config->context, drive_index, fdc->transfer.current_offset,
        fdc->transfer.data_register);
  }

  // Advance pointers.
  fdc->transfer.current_offset++;
  fdc->transfer.sector_byte_index++;

  // Calculate sector size.
  uint16_t sector_size;
  uint8_t dtl = *FDCCommandBufferGet(&fdc->command_buffer, 8);
  if (fdc->transfer.sector_size_code == 0) {
    sector_size = dtl;
  } else {
    sector_size = 128 << fdc->transfer.sector_size_code;
  }

  // Check for sector boundary.
  if (fdc->transfer.sector_byte_index >= sector_size) {
    // Sector done.
    if (fdc->transfer.sector >= fdc->transfer.eot) {
      // End of Track reached.
      if (fdc->transfer.multi_track && (fdc->transfer.head & 1) == 0) {
        // Multi-Track rollover.
        fdc->transfer.head ^= 1;
        fdc->transfer.sector = 1;
        fdc->transfer.sector_byte_index = 0;

        // Recompute offset.
        FDCDriveState* drive = &fdc->drives[drive_index];
        fdc->transfer.current_offset = FDCComputeOffset(
            *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
            fdc->transfer.sector, 0);

        if (fdc->transfer.current_offset == kFDCInvalidOffset) {
          fdc->transfer.tc_received = true;
          return;  // Stop here, don't request next byte.
        }
      } else {
        // Terminate.
        fdc->transfer.sector++;
        fdc->transfer.tc_received = true;
        return;  // Stop here.
      }
    } else {
      // Next sector.
      fdc->transfer.sector++;
      fdc->transfer.sector_byte_index = 0;
      FDCDriveState* drive = &fdc->drives[drive_index];
      fdc->transfer.current_offset = FDCComputeOffset(
          *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
          fdc->transfer.sector, 0);

      if (fdc->transfer.current_offset == kFDCInvalidOffset) {
        fdc->transfer.tc_received = true;
        return;
      }
    }
  }

  // Request next byte via DMA.
  fdc->transfer.dma_request_active = true;
  if (fdc->config && fdc->config->request_dma) {
    fdc->config->request_dma(fdc->config->context);
  }
}

// Handler for Read Data command.
static void FDCHandleReadData(FDCState* fdc) {
  if (fdc->current_command_ticks == 0) {
    // Initialization.
    uint8_t cmd_byte = *FDCCommandBufferGet(&fdc->command_buffer, 0);
    fdc->transfer.multi_track = (cmd_byte & 0x80) != 0;

    uint8_t drive_head = *FDCCommandBufferGet(&fdc->command_buffer, 1);
    uint8_t drive_index = drive_head & 0x03;
    uint8_t head_address = (drive_head >> 2) & 0x01;

    fdc->transfer.cylinder = *FDCCommandBufferGet(&fdc->command_buffer, 2);
    fdc->transfer.head = *FDCCommandBufferGet(&fdc->command_buffer, 3);
    fdc->transfer.sector = *FDCCommandBufferGet(&fdc->command_buffer, 4);
    fdc->transfer.sector_size_code =
        *FDCCommandBufferGet(&fdc->command_buffer, 5);
    fdc->transfer.eot = *FDCCommandBufferGet(&fdc->command_buffer, 6);
    // GPL and DTL are ignored as we don't simulate gap timings or use DTL for
    // init.

    FDCDriveState* drive = &fdc->drives[drive_index];
    if (!drive->present) {
      // Drive not ready.
      FDCFinishReadWrite(
          fdc,
          kFDCST0AbnormalTermination | kFDCST0NotReady | head_address << 2 |
              drive_index,
          0, 0);
      return;
    }

    // Calculate offset for the first byte.
    fdc->transfer.current_offset = FDCComputeOffset(
        *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
        fdc->transfer.sector, 0);

    if (fdc->transfer.current_offset == kFDCInvalidOffset) {
      // Invalid sector/track.
      // If the sector is physically out of bounds for the format, report No
      // Data (Sector Not Found).
      FDCFinishReadWrite(
          fdc, kFDCST0AbnormalTermination | head_address << 2 | drive_index,
          kFDCST1NoData, 0);
      return;
    }

    fdc->transfer.sector_byte_index = 0;
    fdc->transfer.dma_request_active = false;
    fdc->transfer.tc_received = false;
    return;
  }

  // Execution Loop.

  // Check for Terminal Count (TC).
  if (fdc->transfer.tc_received) {
    // Transfer complete.
    uint8_t drive_index = *FDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
    uint8_t head_address = (fdc->transfer.head & 0x01);
    FDCFinishReadWrite(
        fdc, kFDCST0NormalTermination | head_address << 2 | drive_index, 0, 0);
    return;
  }

  // If DREQ is active, wait for the system to service it.
  if (fdc->transfer.dma_request_active) {
    return;
  }

  // Read next byte.
  uint8_t drive_index = *FDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
  if (fdc->config && fdc->config->read_image_byte) {
    fdc->transfer.data_register = fdc->config->read_image_byte(
        fdc->config->context, drive_index, fdc->transfer.current_offset);
  } else {
    fdc->transfer.data_register = 0;
  }

  // Advance pointers.
  fdc->transfer.current_offset++;
  fdc->transfer.sector_byte_index++;

  // Request DMA transfer.
  fdc->transfer.dma_request_active = true;
  if (fdc->config && fdc->config->request_dma) {
    fdc->config->request_dma(fdc->config->context);
  }

  // Calculate sector size again for boundary check.
  uint16_t sector_size;
  uint8_t dtl = *FDCCommandBufferGet(&fdc->command_buffer, 8);
  if (fdc->transfer.sector_size_code == 0) {
    sector_size = dtl;
  } else {
    sector_size = 128 << fdc->transfer.sector_size_code;
  }

  // Check for sector boundary.
  if (fdc->transfer.sector_byte_index >= sector_size) {
    // Sector done.
    if (fdc->transfer.sector >= fdc->transfer.eot) {
      // End of Track reached.
      if (fdc->transfer.multi_track && (fdc->transfer.head & 1) == 0) {
        // Multi-Track rollover: Side 0 -> Side 1.
        fdc->transfer.head ^= 1;   // Flip to Head 1
        fdc->transfer.sector = 1;  // Reset to Sector 1
        fdc->transfer.sector_byte_index = 0;

        // Recompute offset for new head/sector.
        FDCDriveState* drive = &fdc->drives[drive_index];
        fdc->transfer.current_offset = FDCComputeOffset(
            *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
            fdc->transfer.sector, 0);

        if (fdc->transfer.current_offset == kFDCInvalidOffset) {
          fdc->transfer.tc_received = true;
        }
      } else {
        // Standard termination (MT=0 or already on Head 1).
        // Increment sector so result phase reports the *next* logical sector.
        fdc->transfer.sector++;
        fdc->transfer.tc_received = true;
      }
    } else {
      // Move to next sector.
      fdc->transfer.sector++;
      fdc->transfer.sector_byte_index = 0;
      // Recompute offset for new sector.
      FDCDriveState* drive = &fdc->drives[drive_index];
      fdc->transfer.current_offset = FDCComputeOffset(
          *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
          fdc->transfer.sector, 0);

      if (fdc->transfer.current_offset == kFDCInvalidOffset) {
        // Should not happen if EOT is correct, but if we ran off the end
        // of the image despite EOT, terminate.
        fdc->transfer.tc_received = true;
      }
    }
  }
}

// Handler for Recalibrate command.
static void FDCHandleRecalibrate(FDCState* fdc) {
  // Recalibrate command has one parameter byte: drive number (0-3).
  uint8_t drive_index = *FDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
  FDCPerformSeek(fdc, drive_index, 0);
}

// Handler for Seek command.
static void FDCHandleSeek(FDCState* fdc) {
  // Seek command parameters:
  // Byte 1: Drive number (0-3) and Head address (ignored for seek).
  // Byte 2: New Cylinder Number (NCN).
  uint8_t drive_index = *FDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
  uint8_t target_track = *FDCCommandBufferGet(&fdc->command_buffer, 2);
  FDCPerformSeek(fdc, drive_index, target_track);
}

// Handler for Specify command.
static void FDCHandleSpecify(FDCState* fdc) {
  // We don't currently support changing timings or non-DMA mode, so we just
  // ignore the parameters.
  FDCFinishCommandExecution(fdc);
}

// Handler for Sense Interrupt Status command.
static void FDCHandleSenseInterruptStatus(FDCState* fdc) {
  // Check for any pending interrupts.
  for (int i = 0; i < kFDCNumDrives; ++i) {
    FDCDriveState* drive = &fdc->drives[i];
    if (drive->has_pending_interrupt) {
      // Clear the pending interrupt flag.
      drive->has_pending_interrupt = false;

      // Result Byte 0: ST0 (Status Register 0)
      FDCResultBufferAppend(&fdc->result_buffer, &drive->st0);
      // Result Byte 1: PCN (Present Cylinder Number)
      FDCResultBufferAppend(&fdc->result_buffer, &drive->track);

      FDCFinishCommandExecution(fdc);
      return;
    }
  }

  // No pending interrupts found. This is treated as an invalid command.
  uint8_t st0 = kFDCST0InvalidCommand;
  FDCResultBufferAppend(&fdc->result_buffer, &st0);
  FDCFinishCommandExecution(fdc);
}

// List of supported FDC commands.
// The opcodes here represent the base 5-bit command.
static const FDCCommandMetadata kFDCCommandMetadataTable[] = {
    // Read a Track
    {.opcode = kFDCCmdReadTrack, .num_param_bytes = 8, .handler = NULL},
    // Specify
    {.opcode = kFDCCmdSpecify,
     .num_param_bytes = 2,
     .handler = FDCHandleSpecify},
    // Sense Drive Status
    {.opcode = kFDCCmdSenseDriveStatus, .num_param_bytes = 1, .handler = NULL},
    // Write Data
    {.opcode = kFDCCmdWriteData,
     .num_param_bytes = 8,
     .handler = FDCHandleWriteData},
    // Read Data
    {.opcode = kFDCCmdReadData,
     .num_param_bytes = 8,
     .handler = FDCHandleReadData},
    // Recalibrate
    {.opcode = kFDCCmdRecalibrate,
     .num_param_bytes = 1,
     .handler = FDCHandleRecalibrate},
    // Sense Interrupt Status
    {.opcode = kFDCCmdSenseInterruptStatus,
     .num_param_bytes = 0,
     .handler = FDCHandleSenseInterruptStatus},
    // Write Deleted Data
    {.opcode = kFDCCmdWriteDeletedData, .num_param_bytes = 8, .handler = NULL},
    // Read ID
    {.opcode = kFDCCmdReadID, .num_param_bytes = 1, .handler = NULL},
    // Read Deleted Data
    {.opcode = kFDCCmdReadDeletedData, .num_param_bytes = 8, .handler = NULL},
    // Format a Track
    {.opcode = kFDCCmdFormatTrack, .num_param_bytes = 5, .handler = NULL},
    // Seek
    {.opcode = kFDCCmdSeek, .num_param_bytes = 2, .handler = FDCHandleSeek},
    // Scan Equal
    {.opcode = kFDCCmdScanEqual, .num_param_bytes = 8, .handler = NULL},
    // Scan Low or Equal
    {.opcode = kFDCCmdScanLowOrEqual, .num_param_bytes = 8, .handler = NULL},
    // Scan High or Equal
    {.opcode = kFDCCmdScanHighOrEqual, .num_param_bytes = 8, .handler = NULL},
};

void FDCInit(FDCState* fdc, FDCConfig* config) {
  static const FDCState zero_fdc_state = {0};
  *fdc = zero_fdc_state;

  fdc->config = config;
}

// Looks up command metadata by opcode. Returns NULL if not found. This is a
// linear search, but the command table is small enough that this is fine.
static const FDCCommandMetadata* FDCFindCommandMetadata(uint8_t opcode) {
  for (size_t i = 0;
       i < sizeof(kFDCCommandMetadataTable) / sizeof(FDCCommandMetadata); ++i) {
    if (kFDCCommandMetadataTable[i].opcode == opcode) {
      return &kFDCCommandMetadataTable[i];
    }
  }
  return NULL;
}

static uint8_t FDCReadMSRPort(FDCState* fdc) {
  uint8_t msr = 0;
  switch (fdc->phase) {
    case kFDCPhaseIdle:
    case kFDCPhaseCommand:
      // FDC is ready to receive a command or parameter byte.
      msr = kFDCMSRRequestForMaster;
      break;
    case kFDCPhaseResult:
      // FDC has result bytes to send and is still busy with the command.
      msr = kFDCMSRRequestForMaster | kFDCMSRDataDirection | kFDCMSRBusy;
      break;
    case kFDCPhaseExecution:
      // FDC is busy executing a command.
      msr |= kFDCMSRBusy;
      break;
  }
  // Set drive busy flags.
  for (int i = 0; i < kFDCNumDrives; ++i) {
    if (fdc->drives[i].busy) {
      msr |= (1 << i);
    }
  }
  return msr;
}

static uint8_t FDCReadDataPort(FDCState* fdc) {
  switch (fdc->phase) {
    case kFDCPhaseExecution:
      // DMA or Polling read during execution.
      fdc->transfer.dma_request_active = false;
      return fdc->transfer.data_register;

    case kFDCPhaseResult: {
      if (fdc->next_result_byte_index >=
          FDCResultBufferLength(&fdc->result_buffer)) {
        return 0xFF;  // All result bytes have been read.
      }
      uint8_t value =
          *FDCResultBufferGet(&fdc->result_buffer, fdc->next_result_byte_index);
      fdc->next_result_byte_index++;
      if (fdc->next_result_byte_index >=
          FDCResultBufferLength(&fdc->result_buffer)) {
        // Last result byte was read, reset to idle.
        fdc->phase = kFDCPhaseIdle;
        fdc->next_result_byte_index = 0;
        FDCResultBufferClear(&fdc->result_buffer);
      }
      return value;
    }

    default:
      return 0xFF;  // Invalid read.
  }
}

uint8_t FDCReadPort(FDCState* fdc, uint16_t port) {
  switch (port) {
    case kFDCPortMSR:  // Main Status Register (MSR)
      return FDCReadMSRPort(fdc);
    case kFDCPortData:  // Data Register
      return FDCReadDataPort(fdc);
    default:
      // Per convention for reads from unused/invalid ports.
      return 0xFF;
  }
}

static void FDCWriteDORPort(FDCState* fdc, uint8_t value) {
  uint8_t old_dor = fdc->dor;
  fdc->dor = value;

  bool old_reset_bit = (old_dor & kFDCDORReset) != 0;
  bool new_reset_bit = (value & kFDCDORReset) != 0;

  if (!new_reset_bit && old_reset_bit) {
    // Entering reset state (1 -> 0).
    fdc->phase = kFDCPhaseIdle;
    FDCCommandBufferClear(&fdc->command_buffer);
    FDCResultBufferClear(&fdc->result_buffer);
    for (int i = 0; i < kFDCNumDrives; ++i) {
      fdc->drives[i].busy = false;
      fdc->drives[i].has_pending_interrupt = false;
    }
  } else if (new_reset_bit && !old_reset_bit) {
    // Exiting reset state (0 -> 1).
    // The FDC generates an interrupt and sets up status for Sense
    // Interrupt Status for all drives.
    for (int i = 0; i < kFDCNumDrives; ++i) {
      fdc->drives[i].has_pending_interrupt = true;
      fdc->drives[i].st0 = kFDCST0AbnormalTerminationPolling | (uint8_t)i;
    }
    FDCRaiseIRQ6(fdc);
  }
}

static void FDCWriteDataPort(FDCState* fdc, uint8_t value) {
  switch (fdc->phase) {
    case kFDCPhaseIdle: {
      // This is the first byte of a new command.
      // Extract the opcode (lower 5 bits).
      uint8_t opcode = value & 0x1F;
      fdc->current_command = FDCFindCommandMetadata(opcode);

      if (!fdc->current_command) {
        YAX86_FDC_LOG(kLogLevelWarn, "invalid command opcode %02X", opcode);
        // Invalid command. Set up the result phase with an error.
        FDCResultBufferClear(&fdc->result_buffer);
        uint8_t status = kFDCST0InvalidCommand;
        FDCResultBufferAppend(&fdc->result_buffer, &status);
        FDCFinishCommandExecution(fdc);
        return;
      }

      // Clear previous command and store the first byte.
      FDCCommandBufferClear(&fdc->command_buffer);
      FDCCommandBufferAppend(&fdc->command_buffer, &value);

      if (fdc->current_command->num_param_bytes == 0) {
        // Command has no parameters, move directly to execution.
        FDCStartCommandExecution(fdc);
      } else {
        // Wait for parameter bytes.
        fdc->phase = kFDCPhaseCommand;
      }
      break;
    }

    case kFDCPhaseCommand: {
      // This is a parameter byte for the current command.
      if (!fdc->current_command) {
        // Should not happen, but as a safeguard, reset to idle.
        fdc->phase = kFDCPhaseIdle;
        return;
      }

      // Store the parameter byte.
      FDCCommandBufferAppend(&fdc->command_buffer, &value);

      // Check if all parameters have been received.
      // Total bytes = 1 (command) + num_param_bytes.
      if (FDCCommandBufferLength(&fdc->command_buffer) >=
          (uint8_t)(fdc->current_command->num_param_bytes + 1)) {
        // All bytes received, move to execution phase.
        FDCStartCommandExecution(fdc);
      }
      break;
    }

    case kFDCPhaseExecution:
      // DMA or Polling write during execution.
      fdc->transfer.data_register = value;
      fdc->transfer.dma_request_active = false;
      break;

    case kFDCPhaseResult:
      // The FDC is busy. Ignore any writes to the data port.
      break;
  }
}

void FDCWritePort(FDCState* fdc, uint16_t port, uint8_t value) {
  switch (port) {
    case kFDCPortDOR:  // Digital Output Register
      FDCWriteDORPort(fdc, value);
      break;

    case kFDCPortData:  // Data Register
      // Logic will be based on the current FDC phase.
      FDCWriteDataPort(fdc, value);
      break;

    default:
      // Ignore writes to other ports.
      break;
  }
}

void FDCHandleTC(FDCState* fdc) { fdc->transfer.tc_received = true; }

void FDCInsertDisk(FDCState* fdc, uint8_t drive, const FDCDiskFormat* format) {
  if (drive >= kFDCNumDrives) {
    return;
  }
  FDCDriveState* drive_state = &fdc->drives[drive];
  drive_state->present = true;
  drive_state->format = format;
  drive_state->head = 0;
  drive_state->track = 0;
}

void FDCEjectDisk(FDCState* fdc, uint8_t drive) {
  if (drive >= kFDCNumDrives) {
    return;
  }
  FDCDriveState* drive_state = &fdc->drives[drive];
  drive_state->present = false;
  drive_state->format = NULL;
}

void FDCTick(FDCState* fdc) {
  if (fdc->phase != kFDCPhaseExecution) {
    return;
  }

  // Run the command handler if defined.
  if (fdc->current_command && fdc->current_command->handler) {
    fdc->current_command->handler(fdc);
    ++fdc->current_command_ticks;
  } else {
    // No handler defined, finish execution immediately.
    FDCResultBufferClear(&fdc->result_buffer);
    FDCFinishCommandExecution(fdc);
  }
}


// ==============================================================================
// src/fdc/fdc.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_FDC_BUNDLE_H

