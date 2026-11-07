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

// Part of a module's public interface. Declared in the module's public.h and
// defined in one of its source files.
#define YAX86_PUBLIC

// Part of a module's public interface, defined in a header rather than in a
// source file: one copy per translation unit.
#define YAX86_PUBLIC_HEADER static

// Visible to other files within the same module, but not publicly to users of
// the bundled library.
//
// This enables better IDE integration as it allows each source file to be
// compiled independently in unbundled form, but still keeps the symbols private
// when bundled.
#ifdef YAX86_IMPLEMENTATION
// When bundled, static linkage so that the symbol is only visible within the
// implementation file.
#define YAX86_MODULE_PRIVATE static
#else
// When unbundled, use default linkage.
#define YAX86_MODULE_PRIVATE
#endif  // YAX86_IMPLEMENTATION

// Shared between a module's source files, but defined in a header rather than
// in a source file: one copy per translation unit. Also used for internal
// utilities (like snprintf) that are included into multiple module bundles.
#define YAX86_MODULE_PRIVATE_HEADER static

// Visible only within the source file that defines it.
#define YAX86_FILE_PRIVATE static

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
#if !defined(YAX86_IMPLEMENTATION)
#define YAX86_ALWAYS_INLINE __attribute__((always_inline))
#else
#define YAX86_ALWAYS_INLINE __attribute__((always_inline)) inline
#endif
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

YAX86_MODULE_PRIVATE_HEADER int YVSNPrintF(
    char* buffer, size_t size, const char* format, va_list args) YAX86_UNUSED;

YAX86_MODULE_PRIVATE_HEADER int YSNPrintF(
    char* buffer, size_t size, const char* format, ...) YAX86_UNUSED;

// Helper to put a character into the buffer safely.
// Returns 1 (always counts the character, even if not written).
YAX86_MODULE_PRIVATE_HEADER size_t
YSNPrintFPutC(char* buffer, size_t size, size_t* pos, char c) {
  if (*pos < size) {
    buffer[*pos] = c;
  }
  (*pos)++;
  return 1;
}

YAX86_MODULE_PRIVATE_HEADER size_t YSNPrintFPutS(
    char* buffer, size_t size, size_t* pos, const char* s, int width) {
  size_t count = 0;
  size_t len = 0;
  const char* tmp = s;
  while (*tmp++) len++;

  int pad = width - (int)len;
  if (pad < 0) pad = 0;

  // Strings always use space padding (zero flag is ignored per standard)
  while (pad-- > 0) {
    count += YSNPrintFPutC(buffer, size, pos, ' ');
  }

  while (*s) {
    count += YSNPrintFPutC(buffer, size, pos, *s++);
  }
  return count;
}

YAX86_MODULE_PRIVATE_HEADER size_t YSNPrintFPutUI(
    char* buffer, size_t size, size_t* pos, unsigned long long value, int base,
    int uppercase, int width, int pad_zero, int negative) {
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
      count += YSNPrintFPutC(buffer, size, pos, '-');
      negative = 0; // Sign already handled
    }
    while (pad-- > 0) {
      count += YSNPrintFPutC(buffer, size, pos, '0');
    }
  } else {
    while (pad-- > 0) {
      count += YSNPrintFPutC(buffer, size, pos, ' ');
    }
  }

  if (negative) {
    count += YSNPrintFPutC(buffer, size, pos, '-');
  }

  while (i > 0) {
    count += YSNPrintFPutC(buffer, size, pos, temp[--i]);
  }
  return count;
}

YAX86_MODULE_PRIVATE_HEADER int YVSNPrintF(
    char* buffer, size_t size, const char* format, va_list args) {
  size_t pos = 0;

  while (*format) {
    if (*format != '%') {
      YSNPrintFPutC(buffer, size, &pos, *format++);
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
        YSNPrintFPutUI(
            buffer, size, &pos, uval, 10, 0, width, pad_zero, negative);
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

        YSNPrintFPutUI(
            buffer, size, &pos, val, base, uppercase, width, pad_zero, 0);
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
            buffer, size, &pos, val, 16, 0, adjusted_width, pad_zero, 0);
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

YAX86_MODULE_PRIVATE_HEADER int YSNPrintF(
    char* buffer, size_t size, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int ret = YVSNPrintF(buffer, size, format, args);
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
// Provides a YLogger that formats messages and hands them to a caller-provided
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
typedef enum YLogLevel {
  // Emulation is likely incorrect, e.g. an invalid opcode or an unmapped
  // memory access.
  Y_LOG_LEVEL_ERROR = 0,
  // Suspicious but handled, e.g. a read from an unmapped I/O port.
  Y_LOG_LEVEL_WARN,
  // Diagnostic detail.
  Y_LOG_LEVEL_DEBUG,
} YLogLevel;

enum {
  // Maximum length of a formatted log message, including the terminating null
  // byte. Longer messages are truncated.
  Y_LOG_MAX_LINE_LENGTH = 256,
  // Maximum number of distinct log modules, bounded by the width of
  // YLoggerConfig.enabled_modules.
  Y_LOG_MAX_MODULES = 32,
};

// Identifies the module a log message originated from.
//
// Each module declares its own YLogModule in its own public header, so that
// modules do not need to know about one another. IDs must be unique across
// modules - see the module ID test in core/tests/util.
typedef struct YLogModule {
  // Bit index used for mask-based filtering. Must be less than
  // Y_LOG_MAX_MODULES.
  uint8_t id;
  // Human-readable module name, e.g. "FDC".
  const char* name;
} YLogModule;

// Returns the filter mask bit for a module.
YAX86_PUBLIC_HEADER inline uint32_t YLogModuleMask(const YLogModule* module) {
  return (uint32_t)1 << module->id;
}

// ============================================================================
// Logger
// ============================================================================

// Caller-provided runtime configuration for a logger.
typedef struct YLoggerConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Callback to write a formatted log message. The message is null-terminated
  // and carries no prefix or trailing newline - the host composes the final
  // output line.
  void (*write_line)(
      void* context, const YLogModule* module, YLogLevel level, uint64_t tick,
      const char* message, size_t length);

  // Callback returning the current tick count. The platform wires this to its
  // own tick counter. May be NULL, in which case the tick passed to write_line
  // is 0.
  uint64_t (*get_tick)(void* context);

  // Bit mask of enabled modules, indexed by YLogModule.id.
  uint32_t enabled_modules;

  // Maximum severity level to emit. Messages with a level greater than this
  // are suppressed.
  YLogLevel min_level;
} YLoggerConfig;

// State of a logger.
typedef struct YLogger {
  // Pointer to caller-provided runtime configuration.
  YLoggerConfig* config;

  // Scratch buffer used to format a single message. A logger is therefore not
  // reentrant: a write_line callback must never log.
  char buffer[Y_LOG_MAX_LINE_LENGTH];
} YLogger;

// Initialize a logger with the provided configuration.
YAX86_PUBLIC_HEADER inline void YLoggerInit(
    YLogger* logger, YLoggerConfig* config) {
  logger->config = config;
  logger->buffer[0] = '\0';
}

// Whether a message with the given module and level would be emitted. This is
// checked before a message is formatted, so that disabled log statements cost
// only a few comparisons.
YAX86_PUBLIC_HEADER inline bool YLoggerIsEnabled(
    const YLogger* logger, const YLogModule* module, YLogLevel level) {
  return logger != NULL && logger->config != NULL &&
         logger->config->write_line != NULL &&
         level <= logger->config->min_level &&
         (logger->config->enabled_modules & YLogModuleMask(module)) != 0;
}

// Enable a module on a logger.
YAX86_PUBLIC_HEADER inline void YLoggerEnableModule(
    YLogger* logger, const YLogModule* module) {
  if (logger != NULL && logger->config != NULL) {
    logger->config->enabled_modules |= YLogModuleMask(module);
  }
}

// Disable a module on a logger.
YAX86_PUBLIC_HEADER inline void YLoggerDisableModule(
    YLogger* logger, const YLogModule* module) {
  if (logger != NULL && logger->config != NULL) {
    logger->config->enabled_modules &= ~YLogModuleMask(module);
  }
}

// Format and emit a log message. Prefer the YAX86_LOG macro, which skips
// formatting when the message would be suppressed.
YAX86_PUBLIC_HEADER void YLoggerWrite(
    YLogger* logger, const YLogModule* module, YLogLevel level,
    const char* format, ...) YAX86_UNUSED;

YAX86_PUBLIC_HEADER void YLoggerWrite(
    YLogger* logger, const YLogModule* module, YLogLevel level,
    const char* format, ...) {
  // Callers normally go through YAX86_LOG, which has already checked this, but
  // YLoggerWrite is also callable directly.
  if (!YLoggerIsEnabled(logger, module, level)) {
    return;
  }

  va_list args;
  va_start(args, format);
  int formatted_length =
      YVSNPrintF(logger->buffer, Y_LOG_MAX_LINE_LENGTH, format, args);
  va_end(args);

  // YVSNPrintF returns the length the message would have had, which may exceed
  // the buffer. Report the length actually written instead.
  size_t length = 0;
  if (formatted_length > 0) {
    length = (size_t)formatted_length < Y_LOG_MAX_LINE_LENGTH
                 ? (size_t)formatted_length
                 : Y_LOG_MAX_LINE_LENGTH - 1;
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
#define YAX86_LOG(logger, module, level, ...)                 \
  do {                                                        \
    if (YLoggerIsEnabled((logger), (module), (level))) {      \
      YLoggerWrite((logger), (module), (level), __VA_ARGS__); \
    }                                                         \
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
  Y_PIC_LOG_MODULE_ID = 2,
};

// Log module for the PIC.
YAX86_PUBLIC_HEADER const YLogModule YPICLogModule = {
    .id = Y_PIC_LOG_MODULE_ID,
    .name = "PIC",
};

// ============================================================================
// PIC state
// ============================================================================

// The mode of a PIC - single, master, or slave.
typedef enum YPICMode {
  // Single PIC on IBM PC and PC/XT
  Y_PIC_SINGLE = 0,
  // Master PIC on IBM PC/AT and PS/2
  Y_PIC_MASTER,
  // Slave PIC on IBM PC/AT and PS/2
  Y_PIC_SLAVE,
  // Number of PIC modes
  Y_PIC_NUM_MODES,
} YPICMode;

// Initialization state of a PIC.
typedef enum YPICInitState {
  // Uninitialized - waiting for ICW1.
  Y_PIC_EXPECT_ICW1 = 0,
  // ICW1 received - waiting for ICW2.
  Y_PIC_EXPECT_ICW2,
  // ICW2 received - waiting for ICW3 (if needed).
  Y_PIC_EXPECT_ICW3,
  // ICW3 received - waiting for ICW4 (if needed) or fully initialized.
  Y_PIC_EXPECT_ICW4,
  // Fully initialized.
  Y_PIC_READY,
} YPICInitState;

enum {
  // Indicates no pending interrupt. In normal operation, valid ranges of
  // interrupt vectors are 0x08-0x0F for a single PIC or master PIC, and
  // 0x70-0x77 for a slave PIC.
  Y_PIC_NO_PENDING_INTERRUPT = 0xFF,
};

struct YPICState;

// Caller-provided runtime configuration.
typedef struct YPICConfig {
  // Logger for this module. May be NULL.
  YLogger* logger;

  // State of the SP pin.
  // - Single PIC on IBM PC and PC/XT => false
  // - Master PIC on IBM PC/AT and PS/2 => false
  // - Slave PIC on IBM PC/AT and PS/2 => true
  bool sp;
} YPICConfig;

// The register to read on the next read from the data port.
typedef enum YPICReadRegister {
  Y_PIC_READ_IMR = 0,  // Default: read Interrupt Mask Register
  Y_PIC_READ_IRR = 1,  // Read Interrupt Request Register on next read
  Y_PIC_READ_ISR = 2,  // Read In-Service Register on next read
} YPICReadRegister;

// State of a single 8259 PIC chip.
typedef struct YPICState {
  // Pointer to caller-provided runtime configuration.
  YPICConfig config;

  // Initialization state.
  YPICInitState init_state;
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

  // Whether any requested interrupt is unmasked - that is, whether irr & ~imr
  // is non-zero. Recomputed by YPICUpdateUnmaskedRequest(), which is the sole
  // writer, wherever irr or imr changes.
  //
  // This exists so that a CPU can tell in a load whether asking for an
  // interrupt could produce one. It asks on every instruction it runs with
  // interrupts enabled, which is nearly all of them, and the answer is almost
  // always no.
  //
  // It is deliberately conservative rather than exact: it ignores priority and
  // the in-service register, so it can be true where YPICGetPendingInterrupt()
  // would still decline. That costs only a call that reports nothing. What it
  // may never be is falsely false, because a reader takes it as permission not
  // to ask at all.
  bool has_unmasked_request;

  // The register to read on the next read from the data port.
  YPICReadRegister read_register;

  // Pointer to master PIC if this is a slave, or to slave PIC if this is a
  // master. NULL if this is a single PIC.
  struct YPICState* cascade_pic;
} YPICState;

// ============================================================================
// PIC initialization
// ============================================================================

// Initialize a PIC with the provided configuration.
YAX86_PUBLIC void YPICInit(YPICState* pic);

// ============================================================================
// IRQ line control
// ============================================================================

// Raise an IRQ line (0-7) on this PIC. If this is a slave PIC, also raises
// the cascade IRQ on the master PIC.
YAX86_PUBLIC void YPICRaiseIRQ(YPICState* pic, uint8_t irq);

// Lower an IRQ line (0-7) on this PIC. If this is a slave PIC and no interrupts
// are pending, also lowers the cascade IRQ on the master PIC.
YAX86_PUBLIC void YPICLowerIRQ(YPICState* pic, uint8_t irq);

// ============================================================================
// I/O port interface
// ============================================================================

// Read from a PIC I/O port.
// For master PIC: port should be 0x20 (command) or 0x21 (data).
// For slave PIC: port should be 0xA0 (command) or 0xA1 (data).
YAX86_PUBLIC uint8_t YPICReadPort(YPICState* pic, uint16_t port);

// Write to a PIC I/O port.
// For master PIC: port should be 0x20 (command) or 0x21 (data).
// For slave PIC: port should be 0xA0 (command) or 0xA1 (data).
YAX86_PUBLIC void YPICWritePort(YPICState* pic, uint16_t port, uint8_t value);

// ============================================================================
// Interrupt handling
// ============================================================================

// Get the highest priority pending interrupt vector number from this PIC. If
// this is a master PIC, this will consider pending interrupts from the slave
// PIC as well. If no interrupts are pending, returns
// Y_PIC_NO_PENDING_INTERRUPT.
YAX86_PUBLIC uint8_t YPICGetPendingInterrupt(YPICState* pic);

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
  YAX86_LOG(pic->config.logger, &YPICLogModule, level, __VA_ARGS__)

// ============================================================================
// Constants
// ============================================================================

enum {
  // ICW bits
  Y_PIC_ICW1_IC4 = (1 << 0),   // 1 = ICW4 needed
  Y_PIC_ICW1_SNGL = (1 << 1),  // 1 = single PIC, 0 = cascaded
  Y_PIC_ICW1_INIT = (1 << 4),  // 1 = initialization mode
  Y_PIC_ICW2_BASE = 0xF8,  // Upper 5 bits of ICW2 = the interrupt vector base

  // OCW bits
  Y_PIC_OCW_SELECT = (1 << 3),  // 1 = OCW3, 0 = OCW2
  Y_PIC_OCW2_EOI = (1 << 5),    // End of Interrupt
  Y_PIC_OCW2_SL = (1 << 6),     // Specific Level
  Y_PIC_OCW3_RR = (1 << 1),     // 1 = Read Register command
  Y_PIC_OCW3_RIS = (1 << 0),    // 1 = Read ISR, 0 = Read IRR

  // Master PIC cascade IRQ line
  Y_PIC_MASTER_CASCADE_IRQ = 2,
};

// The I/O port of a PIC.
typedef enum YPICPort {
  Y_PIC_PORT_COMMAND = 0,
  Y_PIC_PORT_DATA = 1,

  // Number of PIC ports.
  Y_PIC_NUM_PORTS,
  // Invalid port.
  Y_PIC_PORT_INVALID = -1,
} YPICPort;

// Map a PIC mode to its base I/O port.
YAX86_FILE_PRIVATE const uint16_t YPICBasePorts[Y_PIC_NUM_MODES] = {
    0x20,  // Y_PIC_SINGLE
    0x20,  // Y_PIC_MASTER
    0xA0,  // Y_PIC_SLAVE
};

// ============================================================================
// Helper functions
// ============================================================================

// Returns the mode of a PIC based on its ICWs.
YAX86_FILE_PRIVATE inline YPICMode YPICGetMode(YPICState* pic) {
  // If SNGL bit set in ICW1, we are single PIC.
  if (pic->icw1 & Y_PIC_ICW1_SNGL) {
    return Y_PIC_SINGLE;
  }

  // Otherwise, we are cascaded.
  // If SP pin is set, we are slave; otherwise, master.
  return pic->config.sp ? Y_PIC_SLAVE : Y_PIC_MASTER;
}

// Returns if the PIC is configured as a single PIC.
YAX86_FILE_PRIVATE inline bool YPICIsSingle(YPICState* pic) {
  return YPICGetMode(pic) == Y_PIC_SINGLE;
}

// Returns if the PIC is a master PIC.
YAX86_FILE_PRIVATE inline bool YPICIsMaster(YPICState* pic) {
  return YPICGetMode(pic) == Y_PIC_MASTER;
}

// Returns if the PIC is a slave PIC.
YAX86_FILE_PRIVATE inline bool YPICIsSlave(YPICState* pic) {
  return YPICGetMode(pic) == Y_PIC_SLAVE;
}

// Returns the I/O port corresponding to a given port number.
YAX86_FILE_PRIVATE inline YPICPort YPICGetPort(YPICState* pic, uint16_t port) {
  uint16_t port_offset = port - YPICBasePorts[YPICGetMode(pic)];
  if (port_offset >= Y_PIC_NUM_PORTS) {
    return Y_PIC_PORT_INVALID;
  }
  return (YPICPort)port_offset;
}

// Returns the IRQ number of the parent PIC connected to a slave PIC.
// Only valid if pic is a slave PIC.
YAX86_FILE_PRIVATE inline uint8_t YPICGetCascadeIRQ(YPICState* pic) {
  return pic->icw3 & 0x07;
}

// ============================================================================
// PIC initialization
// ============================================================================

// Recomputes YPICState.has_unmasked_request. This is its sole writer, and every
// path that changes irr or imr calls it, so the flag can never disagree with
// the registers it summarizes.
YAX86_FILE_PRIVATE inline void YPICUpdateUnmaskedRequest(YPICState* pic) {
  pic->has_unmasked_request = (pic->irr & ~pic->imr) != 0;
}

YAX86_PUBLIC void YPICInit(YPICState* pic) {
  // All interrupts masked by default.
  pic->imr = 0xFF;
  YPICUpdateUnmaskedRequest(pic);
}

// ============================================================================
// IRQ line control
// ============================================================================

YAX86_PUBLIC void YPICRaiseIRQ(YPICState* pic, uint8_t irq) {
  if (irq > 7) {
    YAX86_PIC_LOG(Y_LOG_LEVEL_WARN, "ignoring out of range IRQ %u", irq);
    return;
  }
  YAX86_PIC_LOG(
      Y_LOG_LEVEL_DEBUG, "IRQ %u raised, imr %02X isr %02X", irq, pic->imr,
      pic->isr);
  pic->irr |= (1 << irq);
  YPICUpdateUnmaskedRequest(pic);

  // If this is a slave PIC, also raise the cascade IRQ on the master.
  if (YPICIsSlave(pic) && pic->cascade_pic) {
    YPICRaiseIRQ(pic->cascade_pic, YPICGetCascadeIRQ(pic));
  }
}

YAX86_PUBLIC void YPICLowerIRQ(YPICState* pic, uint8_t irq) {
  if (irq > 7) {
    return;
  }
  pic->irr &= ~(1 << irq);
  YPICUpdateUnmaskedRequest(pic);

  // If this is a slave PIC and no interrupts are pending, lower the cascade
  // IRQ on the master.
  if (YPICIsSlave(pic) && pic->irr == 0 && pic->cascade_pic) {
    YPICLowerIRQ(pic->cascade_pic, YPICGetCascadeIRQ(pic));
  }
}

// ============================================================================
// I/O port interface
// ============================================================================

YAX86_PUBLIC uint8_t YPICReadPort(YPICState* pic, uint16_t port) {
  YPICPort pic_port = YPICGetPort(pic, port);
  switch (pic_port) {
    case Y_PIC_PORT_COMMAND:
      // Reading from the command port is not a defined operation.
      return 0x00;

    case Y_PIC_PORT_DATA: {
      uint8_t value;
      switch (pic->read_register) {
        case Y_PIC_READ_IRR:
          value = pic->irr;
          break;
        case Y_PIC_READ_ISR:
          value = pic->isr;
          break;
        default:
          value = pic->imr;
          break;
      }
      pic->read_register = Y_PIC_READ_IMR;
      return value;
    }

    default:
      // Invalid port.
      return 0x00;
  }
}

YAX86_PUBLIC void YPICWritePort(YPICState* pic, uint16_t port, uint8_t value) {
  YPICPort pic_port = YPICGetPort(pic, port);
  switch (pic_port) {
    case Y_PIC_PORT_COMMAND:
      if (value & Y_PIC_ICW1_INIT) {
        // This is ICW1, which starts the initialization sequence.
        pic->icw1 = value;
        pic->irr = 0x00;
        pic->isr = 0x00;
        // All interrupts masked by default.
        pic->imr = 0xFF;
        YPICUpdateUnmaskedRequest(pic);

        // The next write to the data port will be ICW2.
        pic->init_state = Y_PIC_EXPECT_ICW2;
      } else {
        // This is an OCW (Operational Command Word).
        if (value & Y_PIC_OCW_SELECT) {
          // This is OCW3.
          if (value & Y_PIC_OCW3_RR) {
            // This is a Read Register command.
            pic->read_register =
                value & Y_PIC_OCW3_RIS ? Y_PIC_READ_ISR : Y_PIC_READ_IRR;
          } else {
            // Other OCW3 commands (e.g. Special Mask Mode) are not
            // implemented.
          }
        } else {
          // This is OCW2.
          if (value & Y_PIC_OCW2_EOI) {
            if (value & Y_PIC_OCW2_SL) {
              // Specific EOI: clear specified ISR bit.
              uint8_t irq = value & 0x07;
              YAX86_PIC_LOG(Y_LOG_LEVEL_DEBUG, "specific EOI for IRQ %u", irq);
              pic->isr &= ~(1 << irq);
            } else {
              // Non-Specific EOI: clear highest priority ISR bit.
              for (uint8_t i = 0, isr_mask = 1; i < 8; ++i, isr_mask <<= 1) {
                if (pic->isr & isr_mask) {
                  YAX86_PIC_LOG(
                      Y_LOG_LEVEL_DEBUG, "non-specific EOI for IRQ %u", i);
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

    case Y_PIC_PORT_DATA:
      switch (pic->init_state) {
        case Y_PIC_EXPECT_ICW2:
          // This is ICW2. It sets the interrupt vector base.
          // The PIC uses the upper 5 bits of this value.
          pic->icw2 = value;
          if (YPICIsSingle(pic)) {
            // Single mode -> no ICW3, ICW4 optional depending on ICW1.
            pic->init_state =
                pic->icw1 & Y_PIC_ICW1_IC4 ? Y_PIC_EXPECT_ICW4 : Y_PIC_READY;
          } else {
            // Cascaded mode. Expect ICW3 next.
            pic->init_state = Y_PIC_EXPECT_ICW3;
          }
          break;

        case Y_PIC_EXPECT_ICW3:
          // This is ICW3.
          // For master, it's a bitmask of slaves.
          // For slave, it's the 3-bit slave ID.
          pic->icw3 = value;
          // ICW4 is optional depending on ICW1.
          pic->init_state =
              pic->icw1 & Y_PIC_ICW1_IC4 ? Y_PIC_EXPECT_ICW4 : Y_PIC_READY;
          break;

        case Y_PIC_EXPECT_ICW4:
          // This is ICW4.
          pic->init_state = Y_PIC_READY;
          break;

        default:
          // This is an OCW1, which sets the IMR.
          pic->imr = value;
          YPICUpdateUnmaskedRequest(pic);
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

YAX86_PUBLIC YAX86_HOT uint8_t YPICGetPendingInterrupt(YPICState* pic) {
  // Find highest priority requested and unmasked interrupt.
  uint8_t irr = pic->irr & ~pic->imr;
  if (irr == 0) {
    return Y_PIC_NO_PENDING_INTERRUPT;
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
      return Y_PIC_NO_PENDING_INTERRUPT;
    }
  }

  // If this is the master PIC and the interrupt is from the slave, return the
  // slave PIC's interrupt vector.
  if (YPICIsMaster(pic) && pending_irq == Y_PIC_MASTER_CASCADE_IRQ &&
      pic->cascade_pic) {
    uint8_t slave_vector = YPICGetPendingInterrupt(pic->cascade_pic);
    if (slave_vector != Y_PIC_NO_PENDING_INTERRUPT) {
      pic->isr |= pending_irq_mask;
    }
    return slave_vector;
  }

  // This is a normal interrupt on this PIC (or it's a slave reporting up).
  pic->isr |= pending_irq_mask;
  pic->irr &= ~pending_irq_mask;
  YPICUpdateUnmaskedRequest(pic);

  return (pic->icw2 & Y_PIC_ICW2_BASE) + pending_irq;
}


// ==============================================================================
// src/pic/pic.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_PIC_BUNDLE_H

