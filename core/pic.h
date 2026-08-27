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
  kLogModuleIDPIC = 2,
};

// Log module for the PIC.
static const LogModule kLogModulePIC = {
    .id = kLogModuleIDPIC,
    .name = "PIC",
};

// ============================================================================
// PIC state
// ============================================================================

// The mode of a PIC - single, master, or slave.
typedef enum PICMode {
  // Single PIC on IBM PC and PC/XT
  kPICSingle = 0,
  // Master PIC on IBM PC/AT and PS/2
  kPICMaster,
  // Slave PIC on IBM PC/AT and PS/2
  kPICSlave,
  // Number of PIC modes
  kPICNumModes,
} PICMode;

// Initialization state of a PIC.
typedef enum PICInitState {
  // Uninitialized - waiting for ICW1.
  kPICExpectICW1 = 0,
  // ICW1 received - waiting for ICW2.
  kPICExpectICW2,
  // ICW2 received - waiting for ICW3 (if needed).
  kPICExpectICW3,
  // ICW3 received - waiting for ICW4 (if needed) or fully initialized.
  kPICExpectICW4,
  // Fully initialized.
  kPICReady,
} PICInitState;

enum {
  // Indicates no pending interrupt. In normal operation, valid ranges of
  // interrupt vectors are 0x08-0x0F for a single PIC or master PIC, and
  // 0x70-0x77 for a slave PIC.
  kPICNoPendingInterrupt = 0xFF,
};

struct PICState;

// Caller-provided runtime configuration.
typedef struct PICConfig {
  // Logger for this module. May be NULL.
  Logger* logger;

  // State of the SP pin.
  // - Single PIC on IBM PC and PC/XT => false
  // - Master PIC on IBM PC/AT and PS/2 => false
  // - Slave PIC on IBM PC/AT and PS/2 => true
  bool sp;
} PICConfig;

// The register to read on the next read from the data port.
typedef enum PICReadRegister {
  kPICReadIMR = 0,  // Default: read Interrupt Mask Register
  kPICReadIRR = 1,  // Read Interrupt Request Register on next read
  kPICReadISR = 2,  // Read In-Service Register on next read
} PICReadRegister;

// State of a single 8259 PIC chip.
typedef struct PICState {
  // Pointer to caller-provided runtime configuration.
  PICConfig* config;

  // Initialization state.
  PICInitState init_state;
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

  // The register to read on the next read from the data port.
  PICReadRegister read_register;

  // Pointer to master PIC if this is a slave, or to slave PIC if this is a
  // master. NULL if this is a single PIC.
  struct PICState* cascade_pic;
} PICState;

// ============================================================================
// PIC initialization
// ============================================================================

// Initialize a PIC with the provided configuration.
void PICInit(PICState* pic, PICConfig* config);

// ============================================================================
// IRQ line control
// ============================================================================

// Raise an IRQ line (0-7) on this PIC. If this is a slave PIC, also raises
// the cascade IRQ on the master PIC.
void PICRaiseIRQ(PICState* pic, uint8_t irq);

// Lower an IRQ line (0-7) on this PIC. If this is a slave PIC and no interrupts
// are pending, also lowers the cascade IRQ on the master PIC.
void PICLowerIRQ(PICState* pic, uint8_t irq);

// ============================================================================
// I/O port interface
// ============================================================================

// Read from a PIC I/O port.
// For master PIC: port should be 0x20 (command) or 0x21 (data).
// For slave PIC: port should be 0xA0 (command) or 0xA1 (data).
uint8_t PICReadPort(PICState* pic, uint16_t port);

// Write to a PIC I/O port.
// For master PIC: port should be 0x20 (command) or 0x21 (data).
// For slave PIC: port should be 0xA0 (command) or 0xA1 (data).
void PICWritePort(PICState* pic, uint16_t port, uint8_t value);

// ============================================================================
// Interrupt handling
// ============================================================================

// Get the highest priority pending interrupt vector number from this PIC. If
// this is a master PIC, this will consider pending interrupts from the slave
// PIC as well. If no interrupts are pending, returns kPICNoPendingInterrupt.
uint8_t PICGetPendingInterrupt(PICState* pic);

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

#define YAX86_PIC_LOG(level, ...) \
  YAX86_LOG(pic->config->logger, &kLogModulePIC, level, __VA_ARGS__)

// ============================================================================
// Constants
// ============================================================================

enum {
  // ICW bits
  kICW1_IC4 = (1 << 0),   // 1 = ICW4 needed
  kICW1_SNGL = (1 << 1),  // 1 = single PIC, 0 = cascaded
  kICW1_INIT = (1 << 4),  // 1 = initialization mode
  kICW2_BASE = 0xF8,      // Upper 5 bits of ICW2 = the interrupt vector base

  // OCW bits
  kOCW_SELECT = (1 << 3),  // 1 = OCW3, 0 = OCW2
  kOCW2_EOI = (1 << 5),    // End of Interrupt
  kOCW2_SL = (1 << 6),     // Specific Level
  kOCW3_RR = (1 << 1),     // 1 = Read Register command
  kOCW3_RIS = (1 << 0),    // 1 = Read ISR, 0 = Read IRR

  // Master PIC cascade IRQ line
  kMasterCascadeIRQ = 2,
};

// The I/O port of a PIC.
typedef enum PICPort {
  kPICPortCommand = 0,
  kPICPortData = 1,

  // Number of PIC ports.
  kNumPICPorts,
  // Invalid port.
  kPICPortInvalid = -1,
} PICPort;

// Map a PIC mode to its base I/O port.
static const uint16_t kPICBasePorts[kPICNumModes] = {
    0x20,  // kPICSingle
    0x20,  // kPICMaster
    0xA0,  // kPICSlave
};

// ============================================================================
// Helper functions
// ============================================================================

// Returns the mode of a PIC based on its ICWs.
static inline PICMode PICGetMode(PICState* pic) {
  // If SNGL bit set in ICW1, we are single PIC.
  if (pic->icw1 & kICW1_SNGL) {
    return kPICSingle;
  }

  // Otherwise, we are cascaded.
  // If SP pin is set, we are slave; otherwise, master.
  return pic->config->sp ? kPICSlave : kPICMaster;
}

// Returns if the PIC is configured as a single PIC.
static inline bool PICIsSingle(PICState* pic) {
  return PICGetMode(pic) == kPICSingle;
}

// Returns if the PIC is a master PIC.
static inline bool PICIsMaster(PICState* pic) {
  return PICGetMode(pic) == kPICMaster;
}

// Returns if the PIC is a slave PIC.
static inline bool PICIsSlave(PICState* pic) {
  return PICGetMode(pic) == kPICSlave;
}

// Returns the I/O port corresponding to a given port number.
static inline PICPort PICGetPort(PICState* pic, uint16_t port) {
  uint16_t port_offset = port - kPICBasePorts[PICGetMode(pic)];
  if (port_offset >= kNumPICPorts) {
    return kPICPortInvalid;
  }
  return (PICPort)port_offset;
}

// Returns the IRQ number of the parent PIC connected to a slave PIC.
// Only valid if pic is a slave PIC.
static inline uint8_t PICGetCascadeIRQ(PICState* pic) {
  return pic->icw3 & 0x07;
}

// ============================================================================
// PIC initialization
// ============================================================================

void PICInit(PICState* pic, PICConfig* config) {
  // Zero out the PIC state.
  static const PICState zero_pic_state = {0};
  *pic = zero_pic_state;
  pic->config = config;

  // All interrupts masked by default.
  pic->imr = 0xFF;
}

// ============================================================================
// IRQ line control
// ============================================================================

void PICRaiseIRQ(PICState* pic, uint8_t irq) {
  if (irq > 7) {
    YAX86_PIC_LOG(kLogLevelWarn, "ignoring out of range IRQ %u", irq);
    return;
  }
  YAX86_PIC_LOG(
      kLogLevelDebug, "IRQ %u raised, imr %02X isr %02X", irq, pic->imr,
      pic->isr);
  pic->irr |= (1 << irq);

  // If this is a slave PIC, also raise the cascade IRQ on the master.
  if (PICIsSlave(pic) && pic->cascade_pic) {
    PICRaiseIRQ(pic->cascade_pic, PICGetCascadeIRQ(pic));
  }
}

void PICLowerIRQ(PICState* pic, uint8_t irq) {
  if (irq > 7) {
    return;
  }
  pic->irr &= ~(1 << irq);

  // If this is a slave PIC and no interrupts are pending, lower the cascade
  // IRQ on the master.
  if (PICIsSlave(pic) && pic->irr == 0 && pic->cascade_pic) {
    PICLowerIRQ(pic->cascade_pic, PICGetCascadeIRQ(pic));
  }
}

// ============================================================================
// I/O port interface
// ============================================================================

uint8_t PICReadPort(PICState* pic, uint16_t port) {
  PICPort pic_port = PICGetPort(pic, port);
  switch (pic_port) {
    case kPICPortCommand:
      // Reading from the command port is not a defined operation.
      return 0x00;

    case kPICPortData: {
      uint8_t value;
      switch (pic->read_register) {
        case kPICReadIRR:
          value = pic->irr;
          break;
        case kPICReadISR:
          value = pic->isr;
          break;
        default:
          value = pic->imr;
          break;
      }
      pic->read_register = kPICReadIMR;
      return value;
    }

    default:
      // Invalid port.
      return 0x00;
  }
}

void PICWritePort(PICState* pic, uint16_t port, uint8_t value) {
  PICPort pic_port = PICGetPort(pic, port);
  switch (pic_port) {
    case kPICPortCommand:
      if (value & kICW1_INIT) {
        // This is ICW1, which starts the initialization sequence.
        pic->icw1 = value;
        pic->irr = 0x00;
        pic->isr = 0x00;
        // All interrupts masked by default.
        pic->imr = 0xFF;

        // The next write to the data port will be ICW2.
        pic->init_state = kPICExpectICW2;
      } else {
        // This is an OCW (Operational Command Word).
        if (value & kOCW_SELECT) {
          // This is OCW3.
          if (value & kOCW3_RR) {
            // This is a Read Register command.
            pic->read_register = value & kOCW3_RIS ? kPICReadISR : kPICReadIRR;
          } else {
            // Other OCW3 commands (e.g. Special Mask Mode) are not
            // implemented.
          }
        } else {
          // This is OCW2.
          if (value & kOCW2_EOI) {
            if (value & kOCW2_SL) {
              // Specific EOI: clear specified ISR bit.
              uint8_t irq = value & 0x07;
              YAX86_PIC_LOG(kLogLevelDebug, "specific EOI for IRQ %u", irq);
              pic->isr &= ~(1 << irq);
            } else {
              // Non-Specific EOI: clear highest priority ISR bit.
              for (uint8_t i = 0, isr_mask = 1; i < 8; ++i, isr_mask <<= 1) {
                if (pic->isr & isr_mask) {
                  YAX86_PIC_LOG(
                      kLogLevelDebug, "non-specific EOI for IRQ %u", i);
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

    case kPICPortData:
      switch (pic->init_state) {
        case kPICExpectICW2:
          // This is ICW2. It sets the interrupt vector base.
          // The PIC uses the upper 5 bits of this value.
          pic->icw2 = value;
          if (PICIsSingle(pic)) {
            // Single mode -> no ICW3, ICW4 optional depending on ICW1.
            pic->init_state =
                pic->icw1 & kICW1_IC4 ? kPICExpectICW4 : kPICReady;
          } else {
            // Cascaded mode. Expect ICW3 next.
            pic->init_state = kPICExpectICW3;
          }
          break;

        case kPICExpectICW3:
          // This is ICW3.
          // For master, it's a bitmask of slaves.
          // For slave, it's the 3-bit slave ID.
          pic->icw3 = value;
          // ICW4 is optional depending on ICW1.
          pic->init_state = pic->icw1 & kICW1_IC4 ? kPICExpectICW4 : kPICReady;
          break;

        case kPICExpectICW4:
          // This is ICW4.
          pic->init_state = kPICReady;
          break;

        default:
          // This is an OCW1, which sets the IMR.
          pic->imr = value;
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

YAX86_HOT uint8_t PICGetPendingInterrupt(PICState* pic) {
  // Find highest priority requested and unmasked interrupt.
  uint8_t irr = pic->irr & ~pic->imr;
  if (irr == 0) {
    return kPICNoPendingInterrupt;
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
      return kPICNoPendingInterrupt;
    }
  }

  // If this is the master PIC and the interrupt is from the slave, return the
  // slave PIC's interrupt vector.
  if (PICIsMaster(pic) && pending_irq == kMasterCascadeIRQ &&
      pic->cascade_pic) {
    uint8_t slave_vector = PICGetPendingInterrupt(pic->cascade_pic);
    if (slave_vector != kPICNoPendingInterrupt) {
      pic->isr |= pending_irq_mask;
    }
    return slave_vector;
  }

  // This is a normal interrupt on this PIC (or it's a slave reporting up).
  pic->isr |= pending_irq_mask;
  pic->irr &= ~pending_irq_mask;

  return (pic->icw2 & kICW2_BASE) + pending_irq;
}


// ==============================================================================
// src/pic/pic.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_PIC_BUNDLE_H

