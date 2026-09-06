// ==============================================================================
// YAX86 CPU MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_CPU_BUNDLE_H
#define YAX86_CPU_BUNDLE_H

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
// src/cpu/public.h start
// ==============================================================================

#line 1 "./src/cpu/public.h"
// Public interface for the CPU emulator module.
#ifndef YAX86_CPU_PUBLIC_H
#define YAX86_CPU_PUBLIC_H

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_CPU_BUNDLE_H
#include "../util/log.h"
#endif  // YAX86_CPU_BUNDLE_H

enum {
  // Log module ID for the CPU.
  kLogModuleIDCPU = 1,
};

// Log module for the CPU.
static const LogModule kLogModuleCPU = {
    .id = kLogModuleIDCPU,
    .name = "CPU",
};

// ============================================================================
// CPU state
// ============================================================================

// CPU registers.
// Note that the order / numeric values of these constants are important here as
// they must match how the registers are encoded in the ModR/M byte.
typedef enum RegisterIndex {
  // General-purpose and index registers.

  // Accumulator Register
  kAX = 0,
  // Counter Register
  kCX = 1,
  // Data Register
  kDX = 2,
  // Base Register
  kBX = 3,
  // Stack Pointer Register
  kSP = 4,
  // Base Pointer Register
  kBP = 5,
  // Source Index Register
  kSI = 6,
  // Destination Index Register
  kDI = 7,

  // Segment registers.

  // Extra Segment Register
  kES = 8,
  // Code Segment Register
  kCS = 9,
  // Stack Segment Register
  kSS = 10,
  // Data Segment Register
  kDS = 11,

  // Instruction Pointer Register
  kIP,
} RegisterIndex;

enum {
  // Number of registers.
  kNumRegisters = kIP + 1,
};

// CPU flag masks.
typedef enum Flag {
  // Carry Flag
  kCF = (1 << 0),
  // Parity Flag
  kPF = (1 << 2),
  // Auxiliary Carry Flag
  kAF = (1 << 4),
  // Zero Flag
  kZF = (1 << 6),
  // Sign Flag
  kSF = (1 << 7),
  // Trap Flag
  kTF = (1 << 8),
  // Interrupt Enable Flag
  kIF = (1 << 9),
  // Direction Flag
  kDF = (1 << 10),
  // Overflow Flag
  kOF = (1 << 11),
} Flag;

enum {
  // Bits of the flags register that are not flags. The 8086/8088 does not
  // store them: bit 1 and bits 12 through 15 always read as one, and bits 3
  // and 5 always read as zero, whatever gets written over them.
  kFlagsAlwaysSet = 0xF002,
  kFlagsAlwaysClear = 0x0028,
  // CPU flags value on reset - no flags set, and the bits that are not flags
  // reading the only way they can.
  kInitialFlags = kFlagsAlwaysSet,
};

// Standard interrupts.
typedef enum InterruptNumber {
  kInterruptDivideError = 0,
  kInterruptSingleStep = 1,
  kInterruptNMI = 2,
  kInterruptBreakpoint = 3,
  kInterruptOverflow = 4,
} InterruptNumer;

// Result of executing a single instruction.
typedef enum InstructionResult {
  // The instruction was executed.
  kInstructionExecuted = 0,
  // The instruction could not be executed, because its opcode has no handler or
  // because its encoding does not match the expected format for its opcode.
  kInstructionInvalid,
} InstructionResult;

// Result of a single CPU tick.
typedef enum CPUTickResult {
  // An instruction was executed. This includes HLT: the tick that halts the
  // CPU still ran an instruction.
  kCPUTickExecuted = 0,
  // The instruction at CS:IP could not be fetched or executed. The CPU is left
  // pointing past the offending instruction; it is up to the caller to decide
  // whether to continue.
  kCPUTickInvalid,
  // The CPU was already halted and executed no instruction this tick. It stays
  // halted until an interrupt wakes it, so the caller must keep ticking the
  // rest of the machine.
  kCPUTickHalted,
  // Execution was stopped part way through the tick via CPURequestStop().
  kCPUTickStopped,
} CPUTickResult;

// Result of the handle_interrupt callback, directing how the CPU should
// proceed with an interrupt.
typedef enum InterruptHandlerResult {
  // The callback serviced the interrupt itself. The CPU restores the state
  // saved on entry and resumes at the interrupted instruction.
  kInterruptHandlerHandled = 0,
  // The callback did not service the interrupt. The CPU dispatches it through
  // the Interrupt Vector Table instead.
  kInterruptHandlerUnhandled,
} InterruptHandlerResult;

struct CPUState;
struct Instruction;

// Caller-provided runtime configuration.
typedef struct CPUConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger for this module. May be NULL.
  Logger* logger;

  // Callback to read a byte from memory.
  //
  // On the 8086, accessing an invalid memory address will yield garbage data
  // rather than causing a page fault. This callback interface mirrors that
  // behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  uint8_t (*read_memory_byte)(struct CPUState* cpu, uint32_t address);

  // Callback handing the CPU a run of bytes it may read directly, for
  // instruction fetch. Optional - when NULL, every instruction byte is read
  // through read_memory_byte, which is what happens anyway wherever this
  // declines.
  //
  // Given a linear address the CPU wants to fetch from, fills in
  // CPUState.instruction_fetch_window with a range covering it, or sets its
  // data to NULL to decline. A host must decline wherever a read has to be
  // observed or computed rather than loaded: a device region, unmapped memory,
  // or an address covered by anything the host has to be told about.
  //
  // The range should be the whole of whatever region the address falls in
  // rather than the tail of it starting at the address, so that a jump
  // backwards within the same region still lands inside it.
  //
  // The window is kept across instructions, so a host that changes what an
  // address means - remapping memory, or turning on a watchpoint - has to call
  // CPUInvalidateInstructionFetchWindow(). Writes through the same buffer need
  // no such call, and self-modifying code keeps working, because the window is
  // a pointer into the host's own storage rather than a copy of it.
  void (*get_instruction_fetch_window)(struct CPUState* cpu, uint32_t address);

  // Callback to write a byte to memory.
  //
  // On the 8086, accessing an invalid memory address will yield garbage data
  // rather than causing a page fault. This callback interface mirrors that
  // behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  void (*write_memory_byte)(
      struct CPUState* cpu, uint32_t address, uint8_t value);

  // Callback modeling the interrupt acknowledge cycle the CPU runs in response
  // to a request on its INTR pin. Returns false if no external interrupt is
  // requested; otherwise stores the vector number and marks the interrupt in
  // service in the controller, exactly as the two INTA pulses do on real
  // hardware.
  //
  // The CPU only calls this at an instruction boundary with interrupts
  // enabled, so acknowledging and taking the interrupt are a single step and
  // no request can be latched, stranded, or overwritten in between. If NULL,
  // the CPU takes no external interrupts.
  bool (*acknowledge_interrupt)(struct CPUState* cpu, uint8_t* vector);

  // Callback to handle an interrupt. If NULL, every interrupt is dispatched
  // through the Interrupt Vector Table.
  InterruptHandlerResult (*handle_interrupt)(
      struct CPUState* cpu, uint8_t interrupt_number);

  // Callback invoked before executing an instruction. This can be used to
  // inspect or modify the instruction before it is executed, or to inject a
  // pending interrupt. To stop execution, call CPURequestStop().
  void (*on_before_execute_instruction)(
      struct CPUState* cpu, struct Instruction* instruction);

  // Callback invoked after executing an instruction. This can be used to
  // inspect the instruction after it is executed, or to inject a pending
  // interrupt. To stop execution, call CPURequestStop().
  void (*on_after_execute_instruction)(
      struct CPUState* cpu, const struct Instruction* instruction);

  // Callback to read a byte from an I/O port.
  //
  // On the 8086, accessing an invalid I/O port will most likely yield garbage
  // data. This callback interface mirrors that behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  uint8_t (*read_port)(struct CPUState* cpu, uint16_t port);

  // Callback to write a byte to an I/O port.
  //
  // On the 8086, accessing an invalid I/O port will most likely yield garbage
  // data. This callback interface mirrors that behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  void (*write_port)(struct CPUState* cpu, uint16_t port, uint8_t value);
} CPUConfig;

// A run of bytes instruction fetch may read directly, covering the linear
// addresses [start, end), with data pointing at the byte at start.
//
// A NULL data means there is no window and every byte takes the ordinary path
// through CPUConfig.read_memory_byte.
typedef struct CPUInstructionFetchWindow {
  const uint8_t* data;
  uint32_t start;
  uint32_t end;
} CPUInstructionFetchWindow;

// State of the emulated CPU.
typedef struct CPUState {
  // Pointer to caller-provided runtime configuration
  CPUConfig* config;

  // Register values
  uint16_t registers[kNumRegisters];
  // Flag values
  uint16_t flags;

  // An internal interrupt, raised by the CPU itself as a result of the
  // instruction it just executed: INT n, INT 3, INTO, a divide error, or a
  // single-step trap. IF does not gate these - INT 21h works with interrupts
  // disabled, which is how DOS calls inside critical sections behave.
  bool has_pending_internal_interrupt;
  // Interrupt number of the pending internal interrupt.
  uint8_t pending_internal_interrupt_number;

  // Whether the CPU is in halted state. When true, CPUTick() will not fetch
  // or execute any instructions until an external event (e.g., an interrupt)
  // clears this state.
  bool is_halted;

  // Instructions retired since CPUInit(). A halted tick retires none.
  //
  // Counted here rather than left to the caller because CPUTick() already
  // knows whether it ran an instruction, while a caller can only find out by
  // sampling is_halted before every tick - which is what every benchmark
  // harness did, at the cost of giving up batching. 64 bits because a machine
  // left running overflows 32 of them in under an hour.
  uint64_t instructions_retired;

  // Whether a stop has been requested during the current tick. See
  // CPURequestStop().
  bool stop_requested;

  // Cycles charged by the instruction currently executing on top of its base
  // cost: its time on the data bus, and whatever it adds for itself when its
  // cost depends on its operands. CPUTick clears this before each instruction
  // and folds it into cycles_this_tick afterwards.
  uint16_t pending_cycles;

  // Cycles the last call to CPUTick consumed, at the 4.77MHz CPU clock. The
  // caller drives the rest of the machine from this, so that everything timed
  // against the CPU keeps the ratio real hardware has.
  uint16_t cycles_this_tick;

  // The run of bytes instruction fetch is currently reading from, as handed
  // over by CPUConfig.get_instruction_fetch_window.
  //
  // Kept across instructions rather than re-derived per instruction: execution
  // is sequential and a window spans a whole memory region, so the next
  // instruction is almost always inside the one already open.
  CPUInstructionFetchWindow instruction_fetch_window;
} CPUState;

// Initialize CPU state.
void CPUInit(CPUState* cpu, CPUConfig* config);

// Get the value of a CPU flag.
static inline bool CPUGetFlag(const CPUState* cpu, Flag flag) {
  return (cpu->flags & flag) != 0;
}
// Set a CPU flag.
static inline void CPUSetFlag(CPUState* cpu, Flag flag, bool value) {
  if (value) {
    cpu->flags |= flag;
  } else {
    cpu->flags &= ~flag;
  }
}

// Raise an interrupt from within the CPU, to be taken at the end of the
// instruction currently executing. This is for the sources the 8086 calls
// internal - INT n, INT 3, INTO, a divide error, a single-step trap - which
// are not maskable by IF. External requests arrive on the INTR pin instead,
// via the acknowledge_interrupt callback.
static inline void CPURaiseInternalInterrupt(
    CPUState* cpu, uint8_t interrupt_number) {
  cpu->has_pending_internal_interrupt = true;
  cpu->pending_internal_interrupt_number = interrupt_number;
}

// Discard a pending internal interrupt without taking it.
static inline void CPUClearInternalInterrupt(CPUState* cpu) {
  cpu->has_pending_internal_interrupt = false;
  cpu->pending_internal_interrupt_number = 0;
}

// Charge the instruction currently executing for cycles beyond its base cost.
// Used by the instructions whose cost is not a property of the opcode alone -
// a conditional jump that is taken, a shift by a count in CL, a multiply or a
// divide.
void CPUAddCycles(CPUState* cpu, uint16_t cycles);

// Request that the current tick stop as soon as the instruction in progress
// finishes, causing CPUTick() to return kCPUTickStopped.
//
// This is intended to be called from within a CPU callback - a memory or I/O
// port access, an interrupt handler, or an instruction hook - which is why
// stopping is signalled out of band rather than through a return value: a
// watchpoint fires inside read_memory_byte, which returns a uint8_t and has no
// way to carry a status.
//
// The request applies only to the tick during which it was made. CPUTick()
// clears it on entry, so a request made outside a tick has no effect.
static inline void CPURequestStop(CPUState* cpu) { cpu->stop_requested = true; }

// Discards the window instruction fetch is reading from, so that the next
// fetch asks CPUConfig.get_instruction_fetch_window again.
//
// A host must call this whenever it changes what an address means - remapping
// memory, or enabling something that has to observe reads - because the window
// is a direct pointer that would otherwise outlive the change. Writing through
// the same buffer does not need it: the window is a pointer into the host's
// storage, not a copy.
static inline void CPUInvalidateInstructionFetchWindow(CPUState* cpu) {
  cpu->instruction_fetch_window.data = NULL;
}

// ============================================================================
// Instructions
// ============================================================================

enum {
  // Maximum number of displacement bytes in an 8086 instruction.
  kMaxDisplacementBytes = 2,
  // Maximum number of immediate data bytes in an 8086 instruction.
  kMaxImmediateBytes = 4,
  // Bytes an instruction carries besides its prefixes: the opcode, a ModR/M
  // byte, displacement, and immediate data.
  kMaxNonPrefixBytes = 2 + kMaxDisplacementBytes + kMaxImmediateBytes,
  // Maximum number of prefix bytes accepted.
  //
  // The 8086 puts no limit on this. A real one fetches prefixes for as long as
  // they keep coming, and a stream of nothing but prefixes hangs it outright -
  // a prefix is not an instruction boundary, so no interrupt is ever
  // recognized. Two things here cannot follow it that far: the fetch loop has
  // to hand control back to its caller, and Instruction.size is a uint8_t that
  // CPUTick adds to IP. Accepting as many prefixes as still leave the whole
  // encoding addressable by size satisfies both, and is far past anything an
  // assembler emits.
  kMaxPrefixBytes = 0xFF - kMaxNonPrefixBytes,
};

// Instruction prefixes.
typedef enum {
  kPrefixES = 0x26,    // ES segment override
  kPrefixCS = 0x2E,    // CS segment override
  kPrefixSS = 0x36,    // SS segment override
  kPrefixDS = 0x3E,    // DS segment override
  kPrefixLOCK = 0xF0,  // LOCK
  // Undocumented alias of LOCK on the 8086/8088, which does not decode bit 0
  // of this opcode.
  kPrefixLOCKAlt = 0xF1,
  kPrefixREPNZ = 0xF2,  // REPNE/REPNZ
  kPrefixREP = 0xF3,    // REP/REPE/REPZ
} InstructionPrefix;

enum {
  // Value of Instruction.segment_override when an instruction carries no
  // segment override prefix. Zero is kAX, which is never a segment register,
  // so a zero-initialized Instruction correctly has no override.
  kNoSegmentOverride = 0,
};

// The Mod R/M byte.
// The fields are stored a byte each rather than packed back into the bit
// positions they were decoded from. Every read of one would otherwise be a
// load, a shift and a mask, and mod and rm are read on the hottest path in the
// emulator - GetMemoryOperandAddress() takes them for every memory operand and
// GetEffectiveAddressCycles() for every instruction - while reg selects the
// handler for all five instruction groups.
typedef struct ModRM {
  // Mod field - bits 6 and 7 of the ModR/M byte.
  uint8_t mod;
  // REG field - bits 3 to 5 of the ModR/M byte.
  uint8_t reg;
  // R/M field - bits 0 to 2 of the ModR/M byte.
  uint8_t rm;
} ModRM;

// An encoded instruction.
//
// Prefixes are resolved into fields as they are fetched rather than kept as
// raw bytes. Every consumer wants to know what a prefix selected, not which
// byte encoded it, and a field spares each of them a walk over the bytes.
//
// How many prefixes there were is not among them - nothing needs it, and
// size already accounts for the bytes they occupied. LOCK and its
// undocumented 0xF1 alias are consumed but not recorded at all, because
// nothing acts on them: the bus is not shared on a PC/XT.
//
// This struct is zero-initialized on every instruction fetch, so its size is
// worth watching. Its flag bitfields total 6 bits; a ninth would cost a whole
// byte and take it from 12 to 13, which is measurable.
typedef struct Instruction {
  // The segment register selected by a segment override prefix, as a
  // RegisterIndex, or kNoSegmentOverride if the instruction carries none. A
  // later override wins over an earlier one, as on hardware.
  uint8_t segment_override;

  // The repetition prefix present - kPrefixREP or kPrefixREPNZ - or 0 if the
  // instruction carries neither. A later one wins over an earlier one.
  uint8_t repetition_prefix;

  // The primary opcode byte.
  uint8_t opcode;

  // The ModR/M byte, which specifies addressing modes. For some instructions,
  // the REG field within this byte acts as an opcode extension.
  ModRM mod_rm;

  // Raw displacement bytes. If displacement_size is 1, only disp_bytes[0] is
  // valid (value is typically sign-extended). If displacement_size is 2,
  // disp_bytes[0] is the low byte, disp_bytes[1] is the high byte.
  uint8_t displacement[kMaxDisplacementBytes];

  // Raw immediate data bytes.
  uint8_t immediate[kMaxImmediateBytes];

  // Flags

  // Flag indicating if a ModR/M byte is part of this instruction.
  bool has_mod_rm;
  // Number of displacement bytes present: 0, 1, or 2. A whole byte can express
  // more than displacement[] holds, so a consumer bounds itself by the array
  // rather than trusting this - see the fetch, which clamps its own writes.
  uint8_t displacement_size;
  // Number of immediate data bytes present: 0, 1, 2, or 4. Same caveat as
  // displacement_size.
  uint8_t immediate_size;

  // Total length of the original encoded instruction in bytes.
  uint8_t size;
} Instruction;

// ============================================================================
// Execution
// ============================================================================

// Result status from fetching the next instruction.
typedef enum CPUFetchNextInstructionStatus {
  kFetchSuccess = 0,
  // Prefix exceeds maximum allowed size.
  kFetchPrefixTooLong = -1,
} CPUFetchNextInstructionStatus;

// Fetch the next instruction from CS:IP.
//
// The instruction is decoded directly into dest_instruction, so on failure
// dest_instruction holds however much had been decoded when the fetch failed.
// Since this function is part of the core CPU execution loop, assembling and
// copying a whole instruction struct would have a measurable impact on
// performance.
CPUFetchNextInstructionStatus CPUFetchNextInstruction(
    CPUState* cpu, Instruction* instruction);

// Execute a single fetched instruction.
InstructionResult CPUExecuteInstruction(
    CPUState* cpu, Instruction* instruction);

// Run a single instruction cycle, including fetching and executing the next
// instruction at CS:IP, and handling interrupts.
CPUTickResult CPUTick(CPUState* cpu);

#endif  // YAX86_CPU_PUBLIC_H


// ==============================================================================
// src/cpu/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

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
// src/cpu/types.h start
// ==============================================================================

#line 1 "./src/cpu/types.h"
#ifndef YAX86_CPU_TYPES_H
#define YAX86_CPU_TYPES_H

#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Data width helpers.

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

// Operand types.

// The address of a register operand.
typedef struct RegisterAddress {
  // Register index.
  RegisterIndex register_index;
  // Byte offset within the register; only relevant for byte-sized operands.
  // 0 for low byte (AL, CL, DL, BL), 8 for high byte (AH, CH, DH, BH).
  uint8_t byte_offset;
} RegisterAddress;

// The address of a memory operand.
typedef struct MemoryAddress {
  // Segment register.
  RegisterIndex segment_register_index;
  // Effective address offset.
  uint16_t offset;
} MemoryAddress;

// Whether the operand is a register or memory operand.
typedef enum OperandAddressType {
  kOperandAddressTypeRegister = 0,
  kOperandAddressTypeMemory,
} OperandAddressType;

enum {
  // Number of operand address types.
  kNumOperandAddressTypes = kOperandAddressTypeMemory + 1,
};

// Operand address.
typedef struct OperandAddress {
  // Type of operand (register or memory).
  OperandAddressType type;
  // Address of the operand.
  union {
    RegisterAddress register_address;  // For register operands
    MemoryAddress memory_address;      // For memory operands
  } value;
} OperandAddress;

// Operand value.
typedef struct OperandValue {
  // Data width.
  Width width;
  // The value of the operand.
  union {
    uint8_t byte_value;   // For byte operands
    uint16_t word_value;  // For word operands
  } value;
} OperandValue;

// An operand.
typedef struct Operand {
  // Address of the operand.
  OperandAddress address;
  // Value of the operand.
  OperandValue value;
} Operand;

// Instruction types.

struct OpcodeMetadata;

// Context during instruction execution.
typedef struct {
  CPUState* cpu;
  const Instruction* instruction;
  const struct OpcodeMetadata* metadata;
} InstructionContext;

// Handler function for an opcode.
typedef InstructionResult (*OpcodeHandler)(const InstructionContext* context);

// An entry in the opcode lookup table.
typedef struct OpcodeMetadata {
  // Opcode.
  uint8_t opcode;

  // Instruction has ModR/M byte
  bool has_modrm : 1;
  // Number of immediate data bytes: 0, 1, 2, or 4
  uint8_t immediate_size : 3;

  // Width of the instruction's operands.
  Width width : 1;

  // Handler function.
  OpcodeHandler handler;
} OpcodeMetadata;

#endif  // YAX86_CPU_TYPES_H


// ==============================================================================
// src/cpu/types.h end
// ==============================================================================

// ==============================================================================
// src/cpu/cycles.h start
// ==============================================================================

#line 1 "./src/cpu/cycles.h"
#ifndef YAX86_CPU_CYCLES_H
#define YAX86_CPU_CYCLES_H

#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

enum {
  // What a tick costs while the CPU is halted. It is waiting for an interrupt
  // rather than stopped, so time still passes - and the timer that will wake
  // it is driven from that time.
  kHaltedCycles = 4,
  // Extra cycles a taken jump costs, for the prefetch queue it throws away.
  kJumpTakenCycles = 12,
  // Extra cycles a shift or rotate costs for each bit it moves, when the count
  // comes from CL rather than being 1.
  kShiftCyclesPerBit = 4,
};

#ifndef YAX86_IMPLEMENTATION

// Base execution cost per opcode, excluding the effective address calculation
// and time on the data bus.
extern const uint8_t kOpcodeBaseCycles[256];

// Cycles to compute the effective address of a ModR/M memory operand.
extern uint8_t GetEffectiveAddressCycles(const Instruction* instruction);

// Charge the instruction currently executing for time on the data bus.
extern void AddBusCycles(CPUState* cpu, uint8_t num_bytes);

#endif  // YAX86_IMPLEMENTATION

#endif  // YAX86_CPU_CYCLES_H


// ==============================================================================
// src/cpu/cycles.h end
// ==============================================================================

// ==============================================================================
// src/cpu/operands.h start
// ==============================================================================

#line 1 "./src/cpu/operands.h"
#ifndef YAX86_CPU_OPERANDS_H
#define YAX86_CPU_OPERANDS_H

#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#include "types.h"

// Helper function to construct an OperandValue for a byte.
extern OperandValue ByteValue(uint8_t byte_value);

// Helper function to construct OperandValue for a word.
extern OperandValue WordValue(uint16_t word_value);

// Helper function to construct OperandValue given a Width and a value.
extern OperandValue ToOperandValue(Width width, uint32_t raw_value);

// Helper function to zero-extend OperandValue to a 32-bit value. This makes it
// simpler to do direct arithmetic without worrying about overflow.
extern uint32_t FromOperandValue(const OperandValue* value);

// Helper function to sign-extend OperandValue to a 32-bit value. This makes it
// simpler to do direct arithmetic without worrying about overflow.
extern int32_t FromSignedOperandValue(const OperandValue* value);

// Helper function to extract a zero-extended value from an operand.
extern uint32_t FromOperand(const Operand* operand);

// Helper function to extract a sign-extended value from an operand.
extern int32_t FromSignedOperand(const Operand* operand);

// Computes the raw effective address corresponding to a MemoryAddress.
extern uint32_t ToRawAddress(const CPUState* cpu, const MemoryAddress* address);

// Read a byte from memory as a uint8_t.
extern uint8_t ReadRawMemoryByte(CPUState* cpu, uint32_t raw_address);

// Read a word from memory as a uint16_t.
extern uint16_t ReadRawMemoryWord(CPUState* cpu, uint32_t raw_address);

// Read a byte from memory as an OperandValue.
extern OperandValue ReadMemoryOperandByte(
    CPUState* cpu, const OperandAddress* address);

// Read a word from memory as an OperandValue.
extern OperandValue ReadMemoryOperandWord(
    CPUState* cpu, const OperandAddress* address);

// Read a byte from a register as an OperandValue.
extern OperandValue ReadRegisterOperandByte(
    CPUState* cpu, const OperandAddress* address);

// Read a word from a register as an OperandValue.
extern OperandValue ReadRegisterOperandWord(
    CPUState* cpu, const OperandAddress* address);

// Write a byte as uint8_t to memory.
extern void WriteRawMemoryByte(CPUState* cpu, uint32_t address, uint8_t value);

// Write a byte to memory.
extern void WriteMemoryOperandByte(
    CPUState* cpu, const OperandAddress* address, OperandValue value);

// Write a word to memory.
extern void WriteMemoryOperandWord(
    CPUState* cpu, const OperandAddress* address, OperandValue value);

// Write a byte to a register.
extern void WriteRegisterOperandByte(
    CPUState* cpu, const OperandAddress* address, OperandValue value);

// Write a word to a register.
extern void WriteRegisterOperandWord(
    CPUState* cpu, const OperandAddress* address, OperandValue value);

// Add an 8-bit signed relative offset to a 16-bit unsigned base address.
extern uint16_t AddSignedOffsetByte(uint16_t base, uint8_t raw_offset);

// Add a 16-bit signed relative offset to a 16-bit unsigned base address.
extern uint16_t AddSignedOffsetWord(uint16_t base, uint16_t raw_offset);

// Get the register operand for a byte instruction based on the ModR/M byte's
// reg or R/M field.
extern RegisterAddress GetRegisterAddressByte(CPUState* cpu, uint8_t reg_or_rm);

// Get the register operand for a word instruction based on the ModR/M byte's
// reg or R/M field.
extern RegisterAddress GetRegisterAddressWord(CPUState* cpu, uint8_t reg_or_rm);

// Apply segment override prefixes to a MemoryAddress.
extern void ApplySegmentOverride(
    const Instruction* instruction, MemoryAddress* address);

// Compute the memory address for an instruction.
extern MemoryAddress GetMemoryOperandAddress(
    CPUState* cpu, const Instruction* instruction);

// Get a register or memory operand address based on the ModR/M byte and
// displacement, without reading the value currently there.
extern OperandAddress GetRegisterOrMemoryOperandAddress(
    const InstructionContext* ctx);

// Read an 8-bit immediate value.
extern OperandValue ReadImmediateOperandByte(const Instruction* instruction);

// Read a 16-bit immediate value.
extern OperandValue ReadImmediateOperandWord(const Instruction* instruction);

// Read a value from an operand address.
extern OperandValue ReadOperandValue(
    const InstructionContext* ctx, const OperandAddress* address);

// Get a register or memory operand for an instruction based on the ModR/M
// byte and displacement.
extern Operand ReadRegisterOrMemoryOperand(const InstructionContext* ctx);

// Get a register operand for an instruction.
extern Operand ReadRegisterOperandForRegisterIndex(
    const InstructionContext* ctx, RegisterIndex register_index);

// Get a register operand for an instruction from the REG field of the Mod/RM
// byte.
extern Operand ReadRegisterOperand(const InstructionContext* ctx);

// Get a segment register operand for an instruction from the REG field of the
// Mod/RM byte.
extern Operand ReadSegmentRegisterOperand(const InstructionContext* ctx);

// Write a value to a register or memory operand address.
extern void WriteOperandAddress(
    const InstructionContext* ctx, const OperandAddress* address,
    uint32_t raw_value);

// Write a value to a register or memory operand.
extern void WriteOperand(
    const InstructionContext* ctx, const Operand* operand, uint32_t raw_value);

// Read an immediate value from the instruction.
extern OperandValue ReadImmediate(const InstructionContext* ctx);

#endif  // YAX86_IMPLEMENTATION

#endif  // YAX86_CPU_OPERANDS_H


// ==============================================================================
// src/cpu/operands.h end
// ==============================================================================

// ==============================================================================
// src/cpu/cycles.c start
// ==============================================================================

#line 1 "./src/cpu/cycles.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "cycles.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Instruction timing
// ============================================================================
//
// How long each instruction takes, in 4.77MHz CPU clock cycles. This is not a
// cycle-accurate model - it does not track the prefetch queue, and it charges
// a whole instruction at its boundary rather than spreading it over the bus
// cycles it really occupies. What it does give is a clock that means
// something: the ratio between time spent executing and time measured by the
// PIT comes out right, so a guest that calibrates a delay loop against the
// timer arrives at roughly the figure real hardware would.
//
// The cost of an instruction is built from three parts.
//
// 1. A base cost per opcode, below. These are the published 8086 figures for
//    the register form, less the time the figure already accounts for on the
//    bus, which is charged separately in part 3.
//
// 2. The effective address calculation, for instructions that address memory
//    through a ModR/M byte.
//
// 3. Four cycles for every byte the instruction moves over the data bus. The
//    8088 has an 8-bit bus, so a word costs twice a byte - this is the main
//    reason it is slower than the 8086 it shares timings with, and it is the
//    dominant term for most instructions. Charging it from the actual
//    accesses rather than from a table means the string instructions, the
//    stack and the interrupt sequence all cost what their traffic costs,
//    including when REP runs them many times over.
//
// Instructions whose cost depends on more than their operands - a conditional
// jump that is taken, a shift by a count in CL, a multiply or divide - add the
// difference themselves through CPUAddCycles().

// Cycles per byte transferred over the data bus.
enum { kBusCyclesPerByte = 4 };

// Base execution cost per opcode, excluding both the effective address
// calculation and time on the data bus.
//
// Where the published figure covers an instruction that necessarily touches
// memory - the stack instructions, the string instructions, the software
// interrupts - the bus time it includes has been taken back out, so that
// charging the traffic separately does not count it twice.
YAX86_PRIVATE const uint8_t kOpcodeBaseCycles[256] = {
    // 0x00: ALU r/m,r and r,r/m are 3; with an immediate, 4. PUSH sreg is 10
    // for a 2 byte write, POP sreg 8.
    3, 3, 3, 3, 4, 4, 2, 0,        // 00 ADD, 06 PUSH ES, 07 POP ES
    3, 3, 3, 3, 4, 4, 2, 0,        // 08 OR, 0E PUSH CS, 0F POP CS
    3, 3, 3, 3, 4, 4, 2, 0,        // 10 ADC, 16 PUSH SS, 17 POP SS
    3, 3, 3, 3, 4, 4, 2, 0,        // 18 SBB, 1E PUSH DS, 1F POP DS
    3, 3, 3, 3, 4, 4, 2, 4,        // 20 AND, 26 ES:, 27 DAA
    3, 3, 3, 3, 4, 4, 2, 4,        // 28 SUB, 2E CS:, 2F DAS
    3, 3, 3, 3, 4, 4, 2, 8,        // 30 XOR, 36 SS:, 37 AAA
    3, 3, 3, 3, 4, 4, 2, 8,        // 38 CMP, 3E DS:, 3F AAS
    // 0x40: INC and DEC of a 16 bit register are 2 each.
    2, 2, 2, 2, 2, 2, 2, 2,        // 40 INC r16
    2, 2, 2, 2, 2, 2, 2, 2,        // 48 DEC r16
    // 0x50: PUSH is 11 and POP 8, both less the 8 cycles of their word access.
    3, 3, 3, 3, 3, 3, 3, 3,        // 50 PUSH r16
    0, 0, 0, 0, 0, 0, 0, 0,        // 58 POP r16
    // 0x60: undocumented aliases of the conditional jumps at 0x70.
    4, 4, 4, 4, 4, 4, 4, 4,        // 60 Jcc alias
    4, 4, 4, 4, 4, 4, 4, 4,        // 68 Jcc alias
    // 0x70: not taken. A taken jump adds 12 for the flushed queue.
    4, 4, 4, 4, 4, 4, 4, 4,        // 70 Jcc
    4, 4, 4, 4, 4, 4, 4, 4,        // 78 Jcc
    // 0x80: group 1 with an immediate, TEST, XCHG, MOV.
    4, 4, 4, 4, 3, 3, 4, 4,        // 80 group 1, 84 TEST, 86 XCHG
    2, 2, 2, 2, 2, 2, 2, 0,        // 88 MOV, 8C MOV sreg, 8D LEA, 8F POP r/m
    // 0x90: NOP and XCHG with AX are 3. CALL far is 28 less its 4 byte write.
    3, 3, 3, 3, 3, 3, 3, 3,        // 90 NOP, 91 XCHG AX,r
    2, 5, 12, 3, 2, 0, 4, 4,       // 98 CBW, 99 CWD, 9A CALL far, 9C PUSHF
    // 0xA0: MOV to and from a direct address, and the string instructions,
    // all less their bus time.
    2, 2, 2, 2, 10, 10, 14, 14,    // A0 MOV moffs, A4 MOVS, A6 CMPS
    4, 4, 3, 3, 4, 4, 7, 7,        // A8 TEST, AA STOS, AC LODS, AE SCAS
    // 0xB0: MOV immediate into a register.
    4, 4, 4, 4, 4, 4, 4, 4,        // B0 MOV r8, imm8
    4, 4, 4, 4, 4, 4, 4, 4,        // B8 MOV r16, imm16
    // 0xC0: RET, LES, LDS, MOV r/m immediate.
    12, 8, 12, 8, 8, 8, 2, 2,      // C0 RET aliases, C2 RET, C4 LES, C6 MOV
    9, 10, 9, 10, 1, 1, 3, 32,     // C8 RETF aliases, CC INT3, CD INT, CF IRET
    // 0xD0: shifts and rotates. By CL adds 4 per bit, charged at execution.
    2, 2, 8, 8, 8, 8, 2, 11,       // D0 shift by 1, D2 shift by CL, D4 AAM
    0, 0, 0, 0, 0, 0, 0, 0,        // D8 ESC, no coprocessor is present
    // 0xE0: LOOP and the conditional jumps on CX, then IN, OUT, CALL and JMP.
    5, 6, 6, 6, 10, 10, 10, 10,    // E0 LOOPNZ, E3 JCXZ, E4 IN, E6 OUT
    11, 15, 15, 15, 8, 8, 8, 8,    // E8 CALL, E9 JMP, EC IN DX, EE OUT DX
    // 0xF0: prefixes, HLT, the group 3 and group 4/5 instructions.
    2, 2, 2, 2, 2, 2, 3, 3,        // F0 LOCK, F2 REPNZ, F4 HLT, F6 group 3
    2, 2, 2, 2, 2, 2, 3, 3,        // F8 CLC, FA CLI, FC CLD, FE group 4/5
};

// Cycles to compute an effective address, by addressing mode. The 8086 pays
// for each component it has to add together.
enum {
  // A displacement on its own.
  kEACyclesDisplacementOnly = 6,
  // A single base or index register.
  kEACyclesBaseOrIndex = 5,
  // A base or index register plus a displacement.
  kEACyclesBaseOrIndexAndDisplacement = 9,
  // Base plus index. BP+DI and BX+SI cost one cycle less than the other two
  // pairings, which this does not distinguish.
  kEACyclesBaseAndIndex = 8,
  // Base plus index plus a displacement.
  kEACyclesBaseAndIndexAndDisplacement = 12,
  // A segment override prefix costs two more, since the address has to be
  // formed against a different segment base.
  kEACyclesSegmentOverride = 2,
};

// Cycles to compute the effective address of a ModR/M memory operand.
YAX86_PRIVATE uint8_t GetEffectiveAddressCycles(const Instruction* instruction) {
  if (!instruction->has_mod_rm || instruction->mod_rm.mod == 0x03) {
    // A register operand needs no address computed.
    return 0;
  }

  const uint8_t mod = instruction->mod_rm.mod;
  const uint8_t rm = instruction->mod_rm.rm;
  const bool has_displacement =
      mod == 0x01 || mod == 0x02 || (mod == 0x00 && rm == 0x06);
  // R/M values 0 through 3 pair a base register with an index register. The
  // rest name a single register, except for the direct address at mod 0, rm 6.
  const bool has_base_and_index = rm <= 0x03;
  const bool is_direct_address = mod == 0x00 && rm == 0x06;

  uint8_t cycles;
  if (is_direct_address) {
    cycles = kEACyclesDisplacementOnly;
  } else if (has_base_and_index) {
    cycles = has_displacement ? kEACyclesBaseAndIndexAndDisplacement
                              : kEACyclesBaseAndIndex;
  } else {
    cycles = has_displacement ? kEACyclesBaseOrIndexAndDisplacement
                              : kEACyclesBaseOrIndex;
  }

  // Charged once for an instruction that carries a segment override, rather
  // than per override prefix. Only one can take effect, and real code never
  // emits more than one.
  if (instruction->segment_override != kNoSegmentOverride) {
    cycles += kEACyclesSegmentOverride;
  }
  return cycles;
}

YAX86_PRIVATE void AddBusCycles(CPUState* cpu, uint8_t num_bytes) {
  cpu->pending_cycles += (uint16_t)num_bytes * kBusCyclesPerByte;
}

void CPUAddCycles(CPUState* cpu, uint16_t cycles) {
  cpu->pending_cycles += cycles;
}


// ==============================================================================
// src/cpu/cycles.c end
// ==============================================================================

// ==============================================================================
// src/cpu/operands.c start
// ==============================================================================

#line 1 "./src/cpu/operands.c"
#ifndef YAX86_IMPLEMENTATION
#include "operands.h"

#include "../util/common.h"
#include "cycles.h"
#endif  // YAX86_IMPLEMENTATION

// Helper functions to construct OperandValue.
YAX86_PRIVATE OperandValue ByteValue(uint8_t byte_value) {
  OperandValue value = {
      .width = kByte,
      .value = {.byte_value = byte_value},
  };
  return value;
}

// Helper function to construct OperandValue for a word.
YAX86_PRIVATE OperandValue WordValue(uint16_t word_value) {
  OperandValue value = {
      .width = kWord,
      .value = {.word_value = word_value},
  };
  return value;
}

// Helper function to construct OperandValue given a Width and a value.
YAX86_PRIVATE OperandValue ToOperandValue(Width width, uint32_t raw_value) {
  switch (width) {
    case kByte:
      return ByteValue(raw_value & kMaxValue[width]);
    case kWord:
      return WordValue(raw_value & kMaxValue[width]);
  }
  // Should never reach here, but return a default value to avoid warnings.
  return ByteValue(0xFF);
}

// Helper function to zero-extend OperandValue to a 32-bit value. This makes it
// simpler to do direct arithmetic without worrying about overflow.
YAX86_PRIVATE uint32_t FromOperandValue(const OperandValue* value) {
  switch (value->width) {
    case kByte:
      return value->value.byte_value;
    case kWord:
      return value->value.word_value;
  }
  // Should never reach here, but return a default value to avoid warnings.
  return 0xFFFF;
}

// Helper function to sign-extend OperandValue to a 32-bit value. This makes it
// simpler to do direct arithmetic without worrying about overflow.
YAX86_PRIVATE int32_t FromSignedOperandValue(const OperandValue* value) {
  switch (value->width) {
    case kByte:
      return (int32_t)((int8_t)value->value.byte_value);
    case kWord:
      return (int32_t)((int16_t)value->value.word_value);
  }
  // Should never reach here, but return a default value to avoid warnings.
  return 0xFFFF;
}

// Helper function to extract a zero-extended value from an operand.
YAX86_PRIVATE uint32_t FromOperand(const Operand* operand) {
  return FromOperandValue(&operand->value);
}

// Helper function to extract a sign-extended value from an operand.
YAX86_PRIVATE int32_t FromSignedOperand(const Operand* operand) {
  return FromSignedOperandValue(&operand->value);
}

enum {
  // The address bus is 20 bits wide. A segment base plus an offset can sum to
  // as much as 0x10FFEF, and the carry out of bit 19 goes nowhere, so an
  // address past the top of memory wraps around to the bottom.
  kPhysicalAddressMask = 0xFFFFF,
};

// Computes the raw effective address corresponding to a MemoryAddress.
YAX86_PRIVATE uint32_t
ToRawAddress(const CPUState* cpu, const MemoryAddress* address) {
  uint16_t segment = cpu->registers[address->segment_register_index];
  return ((((uint32_t)segment) << 4) + (uint32_t)(address->offset)) &
         kPhysicalAddressMask;
}

// The address of the byte following a memory operand. The offset is 16 bits
// wide and wraps within the segment, so the high byte of a word at offset
// 0xFFFF comes from offset 0 of the same segment rather than from the
// paragraph above it.
static MemoryAddress NextMemoryAddress(const MemoryAddress* address) {
  MemoryAddress next_address = *address;
  ++next_address.offset;
  return next_address;
}

// Read a byte from memory as a uint8_t.
YAX86_PRIVATE uint8_t ReadRawMemoryByte(CPUState* cpu, uint32_t raw_address) {
  return cpu->config->read_memory_byte
             ? cpu->config->read_memory_byte(cpu, raw_address)
             : 0xFF;
}

// Read a word from memory as a uint16_t.
YAX86_PRIVATE uint16_t ReadRawMemoryWord(CPUState* cpu, uint32_t raw_address) {
  uint8_t low_byte_value = ReadRawMemoryByte(cpu, raw_address);
  uint8_t high_byte_value = ReadRawMemoryByte(cpu, raw_address + 1);
  return (((uint16_t)high_byte_value) << 8) | (uint16_t)low_byte_value;
}

// Read a byte from memory to an OperandValue.
YAX86_HOT YAX86_PRIVATE OperandValue
ReadMemoryOperandByte(CPUState* cpu, const OperandAddress* address) {
  AddBusCycles(cpu, 1);
  uint8_t byte_value =
      ReadRawMemoryByte(cpu, ToRawAddress(cpu, &address->value.memory_address));
  return ByteValue(byte_value);
}

// Read a word from memory to an OperandValue.
YAX86_HOT YAX86_PRIVATE OperandValue
ReadMemoryOperandWord(CPUState* cpu, const OperandAddress* address) {
  AddBusCycles(cpu, 2);
  const MemoryAddress* low_byte_address = &address->value.memory_address;
  const MemoryAddress high_byte_address = NextMemoryAddress(low_byte_address);
  uint8_t low_byte_value =
      ReadRawMemoryByte(cpu, ToRawAddress(cpu, low_byte_address));
  uint8_t high_byte_value =
      ReadRawMemoryByte(cpu, ToRawAddress(cpu, &high_byte_address));
  return WordValue(
      (((uint16_t)high_byte_value) << 8) | (uint16_t)low_byte_value);
}

// Read a memory operand of the given width to an OperandValue.
YAX86_PRIVATE OperandValue ReadMemoryOperandValue(
    CPUState* cpu, const OperandAddress* address, Width width) {
  switch (width) {
    case kByte:
      return ReadMemoryOperandByte(cpu, address);
    case kWord:
      return ReadMemoryOperandWord(cpu, address);
  }
  // Should never reach here, but return a default value to avoid warnings.
  return ByteValue(0xFF);
}

// Read a byte from a register to an OperandValue.
YAX86_HOT YAX86_PRIVATE OperandValue
ReadRegisterOperandByte(CPUState* cpu, const OperandAddress* address) {
  const RegisterAddress* register_address = &address->value.register_address;
  uint8_t byte_value = cpu->registers[register_address->register_index] >>
                       register_address->byte_offset;
  return ByteValue(byte_value);
}

// Read a word from a register to an OperandValue.
YAX86_HOT YAX86_PRIVATE OperandValue
ReadRegisterOperandWord(CPUState* cpu, const OperandAddress* address) {
  const RegisterAddress* register_address = &address->value.register_address;
  uint16_t word_value = cpu->registers[register_address->register_index];
  return WordValue(word_value);
}

// Read a register operand of the given width to an OperandValue.
YAX86_PRIVATE OperandValue ReadRegisterOperandValue(
    CPUState* cpu, const OperandAddress* address, Width width) {
  switch (width) {
    case kByte:
      return ReadRegisterOperandByte(cpu, address);
    case kWord:
      return ReadRegisterOperandWord(cpu, address);
  }
  // Should never reach here, but return a default value to avoid warnings.
  return ByteValue(0xFF);
}

// Write a byte as uint8_t to memory.
YAX86_PRIVATE void WriteRawMemoryByte(
    CPUState* cpu, uint32_t address, uint8_t value) {
  if (!cpu->config->write_memory_byte) {
    return;
  }
  cpu->config->write_memory_byte(cpu, address, value);
}

// Write a byte to memory.
YAX86_HOT YAX86_PRIVATE void WriteMemoryOperandByte(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  AddBusCycles(cpu, 1);
  WriteRawMemoryByte(
      cpu, ToRawAddress(cpu, &address->value.memory_address),
      value.value.byte_value);
}

// Write a word to memory.
YAX86_HOT YAX86_PRIVATE void WriteMemoryOperandWord(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  AddBusCycles(cpu, 2);
  const MemoryAddress* low_byte_address = &address->value.memory_address;
  const MemoryAddress high_byte_address = NextMemoryAddress(low_byte_address);
  WriteRawMemoryByte(
      cpu, ToRawAddress(cpu, low_byte_address), value.value.word_value & 0xFF);
  WriteRawMemoryByte(
      cpu, ToRawAddress(cpu, &high_byte_address),
      (value.value.word_value >> 8) & 0xFF);
}

// Write a memory operand of the given width.
YAX86_PRIVATE void WriteMemoryOperand(
    CPUState* cpu, const OperandAddress* address, OperandValue value,
    Width width) {
  switch (width) {
    case kByte:
      WriteMemoryOperandByte(cpu, address, value);
      return;
    case kWord:
      WriteMemoryOperandWord(cpu, address, value);
      return;
  }
  // Should never reach here. Writing nothing is the safe default.
}

// Write a byte to a register.
YAX86_HOT YAX86_PRIVATE void WriteRegisterOperandByte(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  const RegisterAddress* register_address = &address->value.register_address;
  const uint16_t updated_byte = ((uint16_t)value.value.byte_value)
                                << register_address->byte_offset;
  const uint16_t other_byte =
      cpu->registers[register_address->register_index] &
      (((uint16_t)0xFF) << (8 - register_address->byte_offset));
  cpu->registers[register_address->register_index] = other_byte | updated_byte;
}

// Write a word to a register.
YAX86_HOT YAX86_PRIVATE void WriteRegisterOperandWord(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  const RegisterAddress* register_address = &address->value.register_address;
  cpu->registers[register_address->register_index] = value.value.word_value;
}

// Write a register operand of the given width.
YAX86_PRIVATE void WriteRegisterOperand(
    CPUState* cpu, const OperandAddress* address, OperandValue value,
    Width width) {
  switch (width) {
    case kByte:
      WriteRegisterOperandByte(cpu, address, value);
      return;
    case kWord:
      WriteRegisterOperandWord(cpu, address, value);
      return;
  }
  // Should never reach here. Writing nothing is the safe default.
}

// Add an 8-bit signed relative offset to a 16-bit unsigned base address.
YAX86_PRIVATE uint16_t AddSignedOffsetByte(uint16_t base, uint8_t raw_offset) {
  // Sign-extend the offset to 32 bits
  int32_t signed_offset = (int32_t)((int8_t)raw_offset);
  // Zero-extend base to 32 bits
  int32_t signed_base = (int32_t)base;
  // Add the two 32-bit signed values then truncate back down to 16-bit unsigned
  return (uint16_t)(signed_base + signed_offset);
}

// Add a 16-bit signed relative offset to a 16-bit unsigned base address.
YAX86_PRIVATE uint16_t AddSignedOffsetWord(uint16_t base, uint16_t raw_offset) {
  // Sign-extend the offset to 32 bits
  int32_t signed_offset = (int32_t)((int16_t)raw_offset);
  // Zero-extend base to 32 bits
  int32_t signed_base = (int32_t)base;
  // Add the two 32-bit signed values then truncate back down to 16-bit unsigned
  return (uint16_t)(signed_base + signed_offset);
}

// Get the register operand for a byte instruction based on the ModR/M byte's
// reg or R/M field.
YAX86_HOT YAX86_PRIVATE RegisterAddress
GetRegisterAddressByte(YAX86_UNUSED CPUState* cpu, uint8_t reg_or_rm) {
  RegisterAddress address;
  if (reg_or_rm < 4) {
    // AL, CL, DL, BL
    address.register_index = (RegisterIndex)reg_or_rm;
    address.byte_offset = 0;
  } else {
    // AH, CH, DH, BH
    address.register_index = (RegisterIndex)(reg_or_rm - 4);
    address.byte_offset = 8;
  }
  return address;
}

// Get the register operand for a word instruction based on the ModR/M byte's
// reg or R/M field.
YAX86_PRIVATE RegisterAddress
GetRegisterAddressWord(YAX86_UNUSED CPUState* cpu, uint8_t reg_or_rm) {
  const RegisterAddress address = {
      .register_index = (RegisterIndex)reg_or_rm, .byte_offset = 0};
  return address;
}

// Get the register operand of the given width from the ModR/M byte's reg or
// R/M field.
YAX86_PRIVATE RegisterAddress
GetRegisterAddress(CPUState* cpu, uint8_t reg_or_rm, Width width) {
  switch (width) {
    case kByte:
      return GetRegisterAddressByte(cpu, reg_or_rm);
    case kWord:
      return GetRegisterAddressWord(cpu, reg_or_rm);
  }
  // Should never reach here. AL is in range for both widths, so a caller that
  // somehow got here names a real register rather than running off the array.
  const RegisterAddress fallback = {.register_index = kAX, .byte_offset = 0};
  return fallback;
}

// Apply segment override prefixes to a MemoryAddress.
YAX86_PRIVATE void ApplySegmentOverride(
    const Instruction* instruction, MemoryAddress* address) {
  if (instruction->segment_override != kNoSegmentOverride) {
    address->segment_register_index =
        (RegisterIndex)instruction->segment_override;
  }
}

// Compute the memory address for an instruction.
YAX86_HOT YAX86_PRIVATE MemoryAddress
GetMemoryOperandAddress(CPUState* cpu, const Instruction* instruction) {
  MemoryAddress address;
  uint8_t mod = instruction->mod_rm.mod;
  uint8_t rm = instruction->mod_rm.rm;
  switch (rm) {
    case 0:  // [BX + SI]
      address.offset = cpu->registers[kBX] + cpu->registers[kSI];
      address.segment_register_index = kDS;
      break;
    case 1:  // [BX + DI]
      address.offset = cpu->registers[kBX] + cpu->registers[kDI];
      address.segment_register_index = kDS;
      break;
    case 2:  // [BP + SI]
      address.offset = cpu->registers[kBP] + cpu->registers[kSI];
      address.segment_register_index = kSS;
      break;
    case 3:  // [BP + DI]
      address.offset = cpu->registers[kBP] + cpu->registers[kDI];
      address.segment_register_index = kSS;
      break;
    case 4:  // [SI]
      address.offset = cpu->registers[kSI];
      address.segment_register_index = kDS;
      break;
    case 5:  // [DI]
      address.offset = cpu->registers[kDI];
      address.segment_register_index = kDS;
      break;
    case 6:
      if (mod == 0) {
        // Direct memory address with 16-bit displacement
        address.offset = 0;
        address.segment_register_index = kDS;
      } else {
        // [BP]
        address.offset = cpu->registers[kBP];
        address.segment_register_index = kSS;
      }
      break;
    case 7:  // [BX]
      address.offset = cpu->registers[kBX];
      address.segment_register_index = kDS;
      break;
    default:
      // Not possible as RM field is 3 bits (0-7).
      address.offset = 0xFFFF;
      address.segment_register_index = kDS;  // Invalid RM field
      break;
  }

  // Apply segment override prefixes if present
  ApplySegmentOverride(instruction, &address);

  // Add displacement if present
  switch (instruction->displacement_size) {
    case 1: {
      uint8_t raw_displacement = instruction->displacement[0];
      address.offset = AddSignedOffsetByte(address.offset, raw_displacement);
      break;
    }
    case 2: {
      // Concatenate the two displacement bytes as an unsigned 16-bit integer
      uint16_t raw_displacement =
          ((uint16_t)instruction->displacement[0]) |
          (((uint16_t)instruction->displacement[1]) << 8);
      address.offset = AddSignedOffsetWord(address.offset, raw_displacement);
      break;
    }
    default:
      // No displacement
      break;
  }

  return address;
}

// Get a register or memory operand address based on the ModR/M byte and
// displacement, without reading the value currently there.
//
// An instruction that overwrites its destination completely - as opposed to a
// read-modify-write - resolves the address with this and stores through
// WriteOperandAddress(). Reading the destination on the way is not free: it
// charges the bus cycles of a memory access the 8088 never performs.
//
// Always inlined. With more than one caller, -Os and -O2 emit it out of line,
// which puts a call and its register shuffling on the hottest path in the
// emulator - 3.6% at -O2.
YAX86_ALWAYS_INLINE YAX86_PRIVATE OperandAddress
GetRegisterOrMemoryOperandAddress(const InstructionContext* ctx) {
  CPUState* cpu = ctx->cpu;
  const Instruction* instruction = ctx->instruction;
  OperandAddress address;
  uint8_t mod = instruction->mod_rm.mod;
  uint8_t rm = instruction->mod_rm.rm;
  if (mod == 3) {
    // Register operand
    address.type = kOperandAddressTypeRegister;
    address.value.register_address =
        GetRegisterAddress(cpu, rm, ctx->metadata->width);
  } else {
    // Memory operand
    address.type = kOperandAddressTypeMemory;
    address.value.memory_address = GetMemoryOperandAddress(cpu, instruction);
  }
  return address;
}

// Read an 8-bit immediate value.
YAX86_HOT YAX86_PRIVATE OperandValue
ReadImmediateOperandByte(const Instruction* instruction) {
  return ByteValue(instruction->immediate[0]);
}

// Read a 16-bit immediate value.
YAX86_HOT YAX86_PRIVATE OperandValue
ReadImmediateOperandWord(const Instruction* instruction) {
  return WordValue(
      ((uint16_t)instruction->immediate[0]) |
      (((uint16_t)instruction->immediate[1]) << 8));
}

// Read an immediate value of the given width.
YAX86_PRIVATE OperandValue
ReadImmediateOperand(const Instruction* instruction, Width width) {
  switch (width) {
    case kByte:
      return ReadImmediateOperandByte(instruction);
    case kWord:
      return ReadImmediateOperandWord(instruction);
  }
  // Should never reach here, but return a default value to avoid warnings.
  return ByteValue(0xFF);
}

// Read a value from an operand address.
YAX86_PRIVATE OperandValue
ReadOperandValue(const InstructionContext* ctx, const OperandAddress* address) {
  // Not a switch, unlike the width dispatch it calls into. OperandAddressType
  // is a plain enum field rather than a bitfield, so most of the values it can
  // hold are ones the enum does not name and the compiler cannot prove a
  // switch's default arm unreachable: the memory path, which is the common
  // one, ends up paying an extra compare and branch for a case that cannot
  // happen. Width is a one-bit bitfield, which has no unnamed value to
  // represent, so there the default arm disappears from the output entirely.
  // Switching this one too costs 1.06% at -O3.
  //
  // Narrowing type to a one-bit bitfield to make the switch free was measured
  // and is worse still - GetRegisterOrMemoryOperandAddress() writes this field
  // for every operand, and a write to a bitfield is a read-modify-write.
  // Widening OpcodeMetadata.width to a byte would likewise put the width
  // switches in this same position; see AGENTS.md before doing either.
  const Width width = ctx->metadata->width;
  if (address->type == kOperandAddressTypeRegister) {
    return ReadRegisterOperandValue(ctx->cpu, address, width);
  }
  return ReadMemoryOperandValue(ctx->cpu, address, width);
}

// Get a register or memory operand for an instruction based on the ModR/M
// byte and displacement.
YAX86_HOT YAX86_PRIVATE Operand
ReadRegisterOrMemoryOperand(const InstructionContext* ctx) {
  Operand operand;
  operand.address = GetRegisterOrMemoryOperandAddress(ctx);
  operand.value = ReadOperandValue(ctx, &operand.address);
  return operand;
}

// Get a register operand for an instruction.
YAX86_HOT YAX86_PRIVATE Operand ReadRegisterOperandForRegisterIndex(
    const InstructionContext* ctx, RegisterIndex register_index) {
  Width width = ctx->metadata->width;
  Operand operand = {
      .address = {
          .type = kOperandAddressTypeRegister,
          .value = {
              .register_address =
                  GetRegisterAddress(ctx->cpu, register_index, width),
          }}};
  operand.value = ReadOperandValue(ctx, &operand.address);
  return operand;
}

// Get a register operand for an instruction from the REG field of the Mod/RM
// byte.
YAX86_PRIVATE Operand ReadRegisterOperand(const InstructionContext* ctx) {
  return ReadRegisterOperandForRegisterIndex(
      ctx, (RegisterIndex)ctx->instruction->mod_rm.reg);
}

// Get a segment register operand for an instruction from the REG field of the
// Mod/RM byte.
YAX86_PRIVATE Operand
ReadSegmentRegisterOperand(const InstructionContext* ctx) {
  // The segment register field is only two bits wide. The 8086/8088 does not
  // decode the third bit at all, so REG 4 through 7 name the same four
  // registers over again - which is what makes 0x8C and 0x8E accept every REG
  // value. Masking it also keeps the index inside the register array, which
  // REG 4 and above would otherwise run past the end of.
  return ReadRegisterOperandForRegisterIndex(
      ctx, (RegisterIndex)(kES + (ctx->instruction->mod_rm.reg & 0x03)));
}

// Write a value to a register or memory operand address.
YAX86_HOT YAX86_PRIVATE void WriteOperandAddress(
    const InstructionContext* ctx, const OperandAddress* address,
    uint32_t raw_value) {
  const Width width = ctx->metadata->width;
  const OperandValue value = ToOperandValue(width, raw_value);
  // See ReadOperandValue() for why this one is not a switch.
  if (address->type == kOperandAddressTypeRegister) {
    WriteRegisterOperand(ctx->cpu, address, value, width);
  } else {
    WriteMemoryOperand(ctx->cpu, address, value, width);
  }
}

// Write a value to a register or memory operand.
YAX86_PRIVATE void WriteOperand(
    const InstructionContext* ctx, const Operand* operand, uint32_t raw_value) {
  WriteOperandAddress(ctx, &operand->address, raw_value);
}

// Read an immediate value from the instruction.
YAX86_PRIVATE OperandValue ReadImmediate(const InstructionContext* ctx) {
  return ReadImmediateOperand(ctx->instruction, ctx->metadata->width);
}


// ==============================================================================
// src/cpu/operands.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions.h start
// ==============================================================================

#line 1 "./src/cpu/instructions.h"
#ifndef YAX86_CPU_INSTRUCTIONS_H
#define YAX86_CPU_INSTRUCTIONS_H

#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#include "types.h"

// ============================================================================
// Helpers - instructions_helpers.h
// ============================================================================

// Set common CPU flags after an instruction. This includes:
// - Zero flag (ZF)
// - Sign flag (SF)
// - Parity Flag (PF)
extern void SetCommonFlagsAfterInstruction(
    const InstructionContext* ctx, uint32_t result);

// Apply the bits that are not flags to a value on its way into the flags
// register. POPF, IRET and SAHF all load flags from somewhere the guest
// controls, and none of them can change these bits.
extern uint16_t ToFlagsRegisterValue(uint16_t value);

// Push a value the caller has already worked out onto the stack - a return
// address, the flags, a segment register. Use this wherever the value does not
// depend on the stack pointer.
extern void PushValue(CPUState* cpu, OperandValue value);
// Push a PUSH instruction's source operand onto the stack. The operand is
// taken as of after the stack pointer has moved, rather than as of the start
// of the instruction, which is what makes PUSH SP store the decremented value.
extern void PushSourceOperand(CPUState* cpu, const Operand* src);
// Pop a value from the stack.
extern OperandValue Pop(CPUState* cpu);

// Dummy instruction for unsupported opcodes.
extern InstructionResult ExecuteNoOp(const InstructionContext* ctx);
// Handler for the opcode bytes that are prefixes rather than instructions.
extern InstructionResult ExecuteInvalidOpcode(const InstructionContext* ctx);

// ============================================================================
// Opcode table - opcode_table.h
// ============================================================================

// Global opcode metadata lookup table.
extern const OpcodeMetadata opcode_table[256];

// ============================================================================
// Move instructions - instructions_mov.h
// ============================================================================

// MOV r/m8, r8
// MOV r/m16, r16
extern InstructionResult ExecuteMoveRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// MOV r8, r/m8
// MOV r16, r/m16
extern InstructionResult ExecuteMoveRegisterOrMemoryToRegister(
    const InstructionContext* ctx);
// MOV r/m16, sreg
extern InstructionResult ExecuteMoveSegmentRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// MOV sreg, r/m16
extern InstructionResult ExecuteMoveRegisterOrMemoryToSegmentRegister(
    const InstructionContext* ctx);
// MOV AX/CX/DX/BX/SP/BP/SI/DI, imm16
// MOV AH/AL/CH/CL/DH/DL/BH/BL, imm8
extern InstructionResult ExecuteMoveImmediateToRegister(
    const InstructionContext* ctx);
// MOV AL, moffs16
// MOV AX, moffs16
extern InstructionResult ExecuteMoveMemoryOffsetToALOrAX(
    const InstructionContext* ctx);
// MOV moffs16, AL
// MOV moffs16, AX
extern InstructionResult ExecuteMoveALOrAXToMemoryOffset(
    const InstructionContext* ctx);
// MOV r/m8, imm8
// MOV r/m16, imm16
extern InstructionResult ExecuteMoveImmediateToRegisterOrMemory(
    const InstructionContext* ctx);
// XCHG AX, AX/CX/DX/BX/SP/BP/SI/DI
extern InstructionResult ExecuteExchangeRegister(const InstructionContext* ctx);
// XCHG r/m8, r8
// XCHG r/m16, r16
extern InstructionResult ExecuteExchangeRegisterOrMemory(
    const InstructionContext* ctx);
// XLAT
extern InstructionResult ExecuteTranslateByte(const InstructionContext* ctx);

// ============================================================================
// LEA instructions - instructions_lea.h
// ============================================================================

// LEA r16, m
extern InstructionResult ExecuteLoadEffectiveAddress(
    const InstructionContext* ctx);
// LES r16, m
extern InstructionResult ExecuteLoadESWithPointer(
    const InstructionContext* ctx);
// LDS r16, m
extern InstructionResult ExecuteLoadDSWithPointer(
    const InstructionContext* ctx);

// ============================================================================
// Addition instructions - instructions_add.h
// ============================================================================

// Common logic for ADD instructions
extern InstructionResult ExecuteAdd(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);
// Common logic for INC instructions
extern InstructionResult ExecuteInc(
    const InstructionContext* ctx, Operand* dest);
// Common logic for ADC instructions
extern InstructionResult ExecuteAddWithCarry(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);

// ADD r/m8, r8
// ADD r/m16, r16
extern InstructionResult ExecuteAddRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// ADD r8, r/m8
// ADD r16, r/m16
extern InstructionResult ExecuteAddRegisterOrMemoryToRegister(
    const InstructionContext* ctx);
// ADD AL, imm8
// ADD AX, imm16
extern InstructionResult ExecuteAddImmediateToALOrAX(
    const InstructionContext* ctx);
// ADC r/m8, r8
// ADC r/m16, r16
extern InstructionResult ExecuteAddRegisterToRegisterOrMemoryWithCarry(
    const InstructionContext* ctx);
// ADC r8, r/m8
// ADC r16, r/m16
extern InstructionResult ExecuteAddRegisterOrMemoryToRegisterWithCarry(
    const InstructionContext* ctx);
// ADC AL, imm8
// ADC AX, imm16
extern InstructionResult ExecuteAddImmediateToALOrAXWithCarry(
    const InstructionContext* ctx);
// INC AX/CX/DX/BX/SP/BP/SI/DI
extern InstructionResult ExecuteIncRegister(const InstructionContext* ctx);

// ============================================================================
// Subtraction instructions - instructions_sub.c
// ============================================================================

// Set CPU flags after a SUB, SBB, CMP, or NEG instruction.
extern void SetFlagsAfterSub(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_borrow);

// Common logic for SUB instructions
extern InstructionResult ExecuteSub(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);
// Common logic for SBB instructions
extern InstructionResult ExecuteSubWithBorrow(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);
// Common logic for DEC instructions
extern InstructionResult ExecuteDec(
    const InstructionContext* ctx, Operand* dest);

// SUB r/m8, r8
// SUB r/m16, r16
extern InstructionResult ExecuteSubRegisterFromRegisterOrMemory(
    const InstructionContext* ctx);
// SUB r8, r/m8
// SUB r16, r/m16
extern InstructionResult ExecuteSubRegisterOrMemoryFromRegister(
    const InstructionContext* ctx);
// SUB AL, imm8
// SUB AX, imm16
extern InstructionResult ExecuteSubImmediateFromALOrAX(
    const InstructionContext* ctx);
// SBB r/m8, r8
// SBB r/m16, r16
extern InstructionResult ExecuteSubRegisterFromRegisterOrMemoryWithBorrow(
    const InstructionContext* ctx);
// SBB r8, r/m8
// SBB r16, r/m16
extern InstructionResult ExecuteSubRegisterOrMemoryFromRegisterWithBorrow(
    const InstructionContext* ctx);
// SBB AL, imm8
// SBB AX, imm16
extern InstructionResult ExecuteSubImmediateFromALOrAXWithBorrow(
    const InstructionContext* ctx);
// DEC AX/CX/DX/BX/SP/BP/SI/DI
extern InstructionResult ExecuteDecRegister(const InstructionContext* ctx);

// ============================================================================
// Sign extension instructions - instructions_sign_ext.c
// ============================================================================

// CBW
extern InstructionResult ExecuteCbw(const InstructionContext* ctx);
// CWD
extern InstructionResult ExecuteCwd(const InstructionContext* ctx);

// ============================================================================
// CMP instructions - instructions_cmp.c
// ============================================================================

// Common logic for CMP instructions. Computes dest - src and sets flags.
extern InstructionResult ExecuteCmp(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);

// CMP r/m8, r8
// CMP r/m16, r16
extern InstructionResult ExecuteCmpRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// CMP r8, r/m8
// CMP r16, r/m16
extern InstructionResult ExecuteCmpRegisterOrMemoryToRegister(
    const InstructionContext* ctx);
// CMP AL, imm8
// CMP AX, imm16
extern InstructionResult ExecuteCmpImmediateToALOrAX(
    const InstructionContext* ctx);

// ============================================================================
// Boolean instructions - instructions_bool.c
// ============================================================================

extern void SetFlagsAfterBooleanInstruction(
    const InstructionContext* ctx, uint32_t result);
// Common logic for AND instructions.
extern InstructionResult ExecuteBooleanAnd(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);
// Common logic for OR instructions.
extern InstructionResult ExecuteBooleanOr(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);
// Common logic for XOR instructions.
extern InstructionResult ExecuteBooleanXor(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);
// Common logic for TEST instructions.
extern InstructionResult ExecuteTest(
    const InstructionContext* ctx, Operand* dest, OperandValue* src_value);

// AND r/m8, r8
// AND r/m16, r16
extern InstructionResult ExecuteBooleanAndRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// AND r8, r/m8
// AND r16, r/m16
extern InstructionResult ExecuteBooleanAndRegisterOrMemoryToRegister(
    const InstructionContext* ctx);
// AND AL, imm8
// AND AX, imm16
extern InstructionResult ExecuteBooleanAndImmediateToALOrAX(
    const InstructionContext* ctx);
// OR r/m8, r8
// OR r/m16, r16
extern InstructionResult ExecuteBooleanOrRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// OR r8, r/m8
// OR r16, r/m16
extern InstructionResult ExecuteBooleanOrRegisterOrMemoryToRegister(
    const InstructionContext* ctx);
// OR AL, imm8
// OR AX, imm16
extern InstructionResult ExecuteBooleanOrImmediateToALOrAX(
    const InstructionContext* ctx);
// XOR r/m8, r8
// XOR r/m16, r16
extern InstructionResult ExecuteBooleanXorRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// XOR r8, r/m8
// XOR r16, r/m16
extern InstructionResult ExecuteBooleanXorRegisterOrMemoryToRegister(
    const InstructionContext* ctx);
// XOR AL, imm8
// XOR AX, imm16
extern InstructionResult ExecuteBooleanXorImmediateToALOrAX(
    const InstructionContext* ctx);
// TEST r/m8, r8
// TEST r/m16, r16
extern InstructionResult ExecuteTestRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// TEST AL, imm8
// TEST AX, imm16
extern InstructionResult ExecuteTestImmediateToALOrAX(
    const InstructionContext* ctx);

// ============================================================================
// Control flow instructions - instructions_ctrl_flow.c
// ============================================================================

// Common logic for far jumps.
extern InstructionResult ExecuteFarJump(
    const InstructionContext* ctx, const OperandValue* segment,
    const OperandValue* offset);
// Common logic for far calls.
extern InstructionResult ExecuteFarCall(
    const InstructionContext* ctx, const OperandValue* segment,
    const OperandValue* offset);
// Common logic for returning from an interrupt.
extern InstructionResult ExecuteReturnFromInterrupt(CPUState* cpu);

// JMP rel8
// JMP rel16
extern InstructionResult ExecuteShortOrNearJump(const InstructionContext* ctx);
// JMP ptr16:16
extern InstructionResult ExecuteDirectFarJump(const InstructionContext* ctx);
// Unsigned conditional jumps.
extern InstructionResult ExecuteUnsignedConditionalJump(
    const InstructionContext* ctx);
// JL/JGNE and JNL/JGE
extern InstructionResult ExecuteSignedConditionalJumpJLOrJNL(
    const InstructionContext* ctx);
// JLE/JG and JNLE/JG
extern InstructionResult ExecuteSignedConditionalJumpJLEOrJNLE(
    const InstructionContext* ctx);
// LOOP rel8
extern InstructionResult ExecuteLoop(const InstructionContext* ctx);
// LOOPZ rel8
// LOOPNZ rel8
extern InstructionResult ExecuteLoopZOrNZ(const InstructionContext* ctx);
// JCXZ rel8
extern InstructionResult ExecuteJumpIfCXIsZero(const InstructionContext* ctx);
// CALL rel16
extern InstructionResult ExecuteDirectNearCall(const InstructionContext* ctx);
// CALL ptr16:16
extern InstructionResult ExecuteDirectFarCall(const InstructionContext* ctx);
// RET
extern InstructionResult ExecuteNearReturn(const InstructionContext* ctx);
// RET imm16
extern InstructionResult ExecuteNearReturnAndPop(const InstructionContext* ctx);
// RETF
extern InstructionResult ExecuteFarReturn(const InstructionContext* ctx);
// RETF imm16
extern InstructionResult ExecuteFarReturnAndPop(const InstructionContext* ctx);
// IRET
extern InstructionResult ExecuteIret(const InstructionContext* ctx);
// INT 3
extern InstructionResult ExecuteInt3(const InstructionContext* ctx);
// INTO
extern InstructionResult ExecuteInto(const InstructionContext* ctx);
// INT n
extern InstructionResult ExecuteIntN(const InstructionContext* ctx);
// HLT
extern InstructionResult ExecuteHlt(const InstructionContext* ctx);

// ============================================================================
// Stack instructions - instructions_stack.c
// ============================================================================

// PUSH AX/CX/DX/BX/SP/BP/SI/DI
extern InstructionResult ExecutePushRegister(const InstructionContext* ctx);
// POP AX/CX/DX/BX/SP/BP/SI/DI
extern InstructionResult ExecutePopRegister(const InstructionContext* ctx);
// PUSH ES/CS/SS/DS
extern InstructionResult ExecutePushSegmentRegister(
    const InstructionContext* ctx);
// POP ES/CS/SS/DS
extern InstructionResult ExecutePopSegmentRegister(
    const InstructionContext* ctx);
// PUSHF
extern InstructionResult ExecutePushFlags(const InstructionContext* ctx);
// POPF
extern InstructionResult ExecutePopFlags(const InstructionContext* ctx);
// POP r/m16
extern InstructionResult ExecutePopRegisterOrMemory(
    const InstructionContext* ctx);
// LAHF
extern InstructionResult ExecuteLoadAHFromFlags(const InstructionContext* ctx);
// SAHF
extern InstructionResult ExecuteStoreAHToFlags(const InstructionContext* ctx);

// ============================================================================
// Flag manipulation instructions - instructions_flags.c
// ============================================================================

// CLC, STC, CLI, STI, CLD, STD
extern InstructionResult ExecuteClearOrSetFlag(const InstructionContext* ctx);
// CMC
extern InstructionResult ExecuteComplementCarryFlag(
    const InstructionContext* ctx);
// SALC
extern InstructionResult ExecuteSetALFromCarry(const InstructionContext* ctx);

// ============================================================================
// IN and OUT instructions - instructions_io.c
// ============================================================================

// IN AL, imm8
// IN AX, imm8
extern InstructionResult ExecuteInImmediate(const InstructionContext* ctx);
// IN AL, DX
// IN AX, DX
extern InstructionResult ExecuteInDX(const InstructionContext* ctx);
// OUT imm8, AL
// OUT imm8, AX
extern InstructionResult ExecuteOutImmediate(const InstructionContext* ctx);
// OUT DX, AL
// OUT DX, AX
extern InstructionResult ExecuteOutDX(const InstructionContext* ctx);

// ============================================================================
// String instructions - instructions_string.c
// ============================================================================

// MOVS
extern InstructionResult ExecuteMovs(const InstructionContext* ctx);
// STOS
extern InstructionResult ExecuteStos(const InstructionContext* ctx);
// LODS
extern InstructionResult ExecuteLods(const InstructionContext* ctx);
// SCAS
extern InstructionResult ExecuteScas(const InstructionContext* ctx);
// CMPS
extern InstructionResult ExecuteCmps(const InstructionContext* ctx);

// ============================================================================
// BCD and ASCII arithmetic instructions - instructions_bcd_ascii.c
// ============================================================================

// AAA
extern InstructionResult ExecuteAaa(const InstructionContext* ctx);
// AAS
extern InstructionResult ExecuteAas(const InstructionContext* ctx);
// AAM
extern InstructionResult ExecuteAam(const InstructionContext* ctx);
// AAD
extern InstructionResult ExecuteAad(const InstructionContext* ctx);
// DAA
extern InstructionResult ExecuteDaa(const InstructionContext* ctx);
// DAS
extern InstructionResult ExecuteDas(const InstructionContext* ctx);

// ============================================================================
// Group 1 instructions - instructions_group_1.c
// ============================================================================

// Group 1 instruction handler.
extern InstructionResult ExecuteGroup1Instruction(
    const InstructionContext* ctx);

// Group 1 instruction handler, but sign-extends the 8-bit immediate value.
extern InstructionResult ExecuteGroup1InstructionWithSignExtension(
    const InstructionContext* ctx);

// ============================================================================
// Group 2 instructions - instructions_group_2.c
// ============================================================================

// Group 2 shift / rotate by 1.
extern InstructionResult ExecuteGroup2ShiftOrRotateBy1Instruction(
    const InstructionContext* ctx);
// Group 2 shift / rotate by CL.
extern InstructionResult ExecuteGroup2ShiftOrRotateByCLInstruction(
    const InstructionContext* ctx);

// ============================================================================
// Group 3 instructions - instructions_group_3.c
// ============================================================================

// Group 3 instruction handler.
extern InstructionResult ExecuteGroup3Instruction(
    const InstructionContext* ctx);

// ============================================================================
// Group 4 instructions - instructions_group_4.c
// ============================================================================

// Group 4 instruction handler.
extern InstructionResult ExecuteGroup4Instruction(
    const InstructionContext* ctx);

// ============================================================================
// Group 5 instructions - instructions_group_5.c
// ============================================================================

// Group 5 instruction handler.
extern InstructionResult ExecuteGroup5Instruction(
    const InstructionContext* ctx);

#endif  // YAX86_IMPLEMENTATION

#endif  // YAX86_CPU_INSTRUCTIONS_H


// ==============================================================================
// src/cpu/instructions.h end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_helpers.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_helpers.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// Set common CPU flags after an instruction. This includes:
// - Zero flag (ZF)
// - Sign flag (SF)
// - Parity Flag (PF)
YAX86_HOT YAX86_PRIVATE void SetCommonFlagsAfterInstruction(
    const InstructionContext* ctx, uint32_t result) {
  Width width = ctx->metadata->width;
  result &= kMaxValue[width];
  // Zero flag (ZF)
  CPUSetFlag(ctx->cpu, kZF, result == 0);
  // Sign flag (SF)
  CPUSetFlag(ctx->cpu, kSF, result & kSignBit[width]);
  // Parity flag (PF)
  // Set if the number of set bits in the least significant byte is even
  uint8_t parity = result & 0xFF;  // Check only the low byte for parity
  parity ^= parity >> 4;
  parity ^= parity >> 2;
  parity ^= parity >> 1;
  CPUSetFlag(ctx->cpu, kPF, (parity & 1) == 0);
}

YAX86_PRIVATE uint16_t ToFlagsRegisterValue(uint16_t value) {
  return (value | (uint16_t)kFlagsAlwaysSet) & ~(uint16_t)kFlagsAlwaysClear;
}

// Write a word where the stack pointer already points. The caller is
// responsible for having made room for it.
static void WriteToStackTop(CPUState* cpu, OperandValue value) {
  OperandAddress address = {
      .type = kOperandAddressTypeMemory,
      .value = {
          .memory_address = {
              .segment_register_index = kSS,
              .offset = cpu->registers[kSP],
          }}};
  WriteMemoryOperandWord(cpu, &address, value);
}

YAX86_PRIVATE void PushValue(CPUState* cpu, OperandValue value) {
  cpu->registers[kSP] -= 2;
  WriteToStackTop(cpu, value);
}

YAX86_PRIVATE void PushSourceOperand(CPUState* cpu, const Operand* src) {
  cpu->registers[kSP] -= 2;
  // The 8086/8088 moves the stack pointer before it reads the source, so
  // PUSH SP stores the value SP has after the decrement rather than the one it
  // had on entry. SP is the only source that can tell the difference. The
  // 80286 and later store the entry value instead.
  const bool source_is_stack_pointer =
      src->address.type == kOperandAddressTypeRegister &&
      src->address.value.register_address.register_index == kSP;
  const OperandValue value =
      source_is_stack_pointer ? WordValue(cpu->registers[kSP]) : src->value;
  WriteToStackTop(cpu, value);
}

YAX86_PRIVATE OperandValue Pop(CPUState* cpu) {
  OperandAddress address = {
      .type = kOperandAddressTypeMemory,
      .value = {
          .memory_address = {
              .segment_register_index = kSS,
              .offset = cpu->registers[kSP],
          }}};
  OperandValue value = ReadMemoryOperandWord(cpu, &address);
  cpu->registers[kSP] += 2;
  return value;
}

// Dummy instruction for unsupported opcodes.
YAX86_PRIVATE InstructionResult
ExecuteNoOp(YAX86_UNUSED const InstructionContext* ctx) {
  return kInstructionExecuted;
}

// Handler for the eight opcode bytes that are prefixes rather than
// instructions. The prefix decoder consumes those bytes before an opcode is
// looked up, so a decoded instruction never names one and this is unreachable
// from CPUTick(). It exists so that every entry in the opcode table is
// callable, which is what lets the tick path dispatch without first checking
// for a null handler.
YAX86_PRIVATE InstructionResult
ExecuteInvalidOpcode(YAX86_UNUSED const InstructionContext* ctx) {
  return kInstructionInvalid;
}


// ==============================================================================
// src/cpu/instructions_helpers.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_mov.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_mov.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// MOV instructions
// ============================================================================

// MOV r/m8, r8
// MOV r/m16, r16
YAX86_PRIVATE InstructionResult
ExecuteMoveRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  OperandAddress dest = GetRegisterOrMemoryOperandAddress(ctx);
  Operand src = ReadRegisterOperand(ctx);
  WriteOperandAddress(ctx, &dest, FromOperand(&src));
  return kInstructionExecuted;
}

// MOV r8, r/m8
// MOV r16, r/m16
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteMoveRegisterOrMemoryToRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  WriteOperand(ctx, &dest, FromOperand(&src));
  return kInstructionExecuted;
}

// MOV r/m16, sreg
YAX86_PRIVATE InstructionResult
ExecuteMoveSegmentRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  OperandAddress dest = GetRegisterOrMemoryOperandAddress(ctx);
  Operand src = ReadSegmentRegisterOperand(ctx);
  WriteOperandAddress(ctx, &dest, FromOperand(&src));
  return kInstructionExecuted;
}

// MOV sreg, r/m16
YAX86_PRIVATE InstructionResult
ExecuteMoveRegisterOrMemoryToSegmentRegister(const InstructionContext* ctx) {
  Operand dest = ReadSegmentRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  WriteOperand(ctx, &dest, FromOperand(&src));
  return kInstructionExecuted;
}

// MOV AX/CX/DX/BX/SP/BP/SI/DI, imm16
// MOV AH/AL/CH/CL/DH/DL/BH/BL, imm8
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteMoveImmediateToRegister(const InstructionContext* ctx) {
  static const uint8_t register_index_opcode_base[kNumWidths] = {
      0xB0,  // kByte
      0xB8,  // kWord
  };
  RegisterIndex register_index =
      (RegisterIndex)(ctx->instruction->opcode -
                      register_index_opcode_base[ctx->metadata->width]);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  OperandValue src_value = ReadImmediate(ctx);
  WriteOperand(ctx, &dest, FromOperandValue(&src_value));
  return kInstructionExecuted;
}

// MOV AL, moffs16
// MOV AX, moffs16
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteMoveMemoryOffsetToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  // Offset is always 16 bits, even though the data width of the operation may
  // be 8 bits.
  OperandValue src_offset_value = ReadImmediateOperandWord(ctx->instruction);
  // The source is DS:offset by default, but can be overridden by a segment
  // override prefix.
  OperandAddress src_address = {
      .type = kOperandAddressTypeMemory,
      .value = {
          .memory_address = {
              .segment_register_index = kDS,
              .offset = (uint16_t)FromOperandValue(&src_offset_value),
          }}};
  ApplySegmentOverride(ctx->instruction, &src_address.value.memory_address);
  OperandValue src_value = ReadOperandValue(ctx, &src_address);
  WriteOperand(ctx, &dest, FromOperandValue(&src_value));
  return kInstructionExecuted;
}

// MOV moffs16, AL
// MOV moffs16, AX
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteMoveALOrAXToMemoryOffset(const InstructionContext* ctx) {
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  // Offset is always 16 bits, even though the data width of the operation may
  // be 8 bits.
  OperandValue dest_offset_value = ReadImmediateOperandWord(ctx->instruction);
  // The destination is DS:offset by default, but can be overridden by a segment
  // override prefix.
  OperandAddress dest_address = {
      .type = kOperandAddressTypeMemory,
      .value = {
          .memory_address = {
              .segment_register_index = kDS,
              .offset = (uint16_t)FromOperandValue(&dest_offset_value),
          }}};
  ApplySegmentOverride(ctx->instruction, &dest_address.value.memory_address);
  WriteOperandAddress(ctx, &dest_address, FromOperand(&src));
  return kInstructionExecuted;
}

// MOV r/m8, imm8
// MOV r/m16, imm16
YAX86_PRIVATE InstructionResult
ExecuteMoveImmediateToRegisterOrMemory(const InstructionContext* ctx) {
  OperandAddress dest = GetRegisterOrMemoryOperandAddress(ctx);
  OperandValue src_value = ReadImmediate(ctx);
  WriteOperandAddress(ctx, &dest, FromOperandValue(&src_value));
  return kInstructionExecuted;
}

// ============================================================================
// XCHG instructions
// ============================================================================

// XCHG AX, AX/CX/DX/BX/SP/BP/SI/DI
YAX86_PRIVATE InstructionResult
ExecuteExchangeRegister(const InstructionContext* ctx) {
  RegisterIndex register_index =
      (RegisterIndex)(ctx->instruction->opcode - 0x90);
  if (register_index == kAX) {
    // No-op
    return kInstructionExecuted;
  }
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  uint32_t temp = FromOperand(&dest);
  WriteOperand(ctx, &dest, FromOperand(&src));
  WriteOperand(ctx, &src, temp);
  return kInstructionExecuted;
}

// XCHG r/m8, r8
// XCHG r/m16, r16
YAX86_PRIVATE InstructionResult
ExecuteExchangeRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  uint32_t temp = FromOperand(&dest);
  WriteOperand(ctx, &dest, FromOperand(&src));
  WriteOperand(ctx, &src, temp);
  return kInstructionExecuted;
}

// ============================================================================
// XLAT
// ============================================================================

// XLAT
YAX86_PRIVATE InstructionResult
ExecuteTranslateByte(const InstructionContext* ctx) {
  // Read the AL register
  Operand al = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  // The translation table is at DS:BX by default, but can be overridden by a
  // segment override prefix.
  OperandAddress src_address = {
      .type = kOperandAddressTypeMemory,
      .value =
          {.memory_address =
               {
                   .segment_register_index = kDS,
                   .offset =
                       (uint16_t)(ctx->cpu->registers[kBX] + FromOperand(&al)),
               }},
  };
  ApplySegmentOverride(ctx->instruction, &src_address.value.memory_address);
  OperandValue src_value = ReadMemoryOperandByte(ctx->cpu, &src_address);
  WriteOperandAddress(ctx, &al.address, FromOperandValue(&src_value));
  return kInstructionExecuted;
}


// ==============================================================================
// src/cpu/instructions_mov.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_lea.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_lea.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// LEA instruction
// ============================================================================

// LEA r16, m
YAX86_PRIVATE InstructionResult
ExecuteLoadEffectiveAddress(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  MemoryAddress memory_address =
      GetMemoryOperandAddress(ctx->cpu, ctx->instruction);
  // LEA yields the effective address - the offset within the segment - and
  // never resolves it against a segment or touches memory. Converting to a
  // linear address here would fold the segment base into the result, which is
  // invisible only while the segment register happens to be zero.
  WriteOperandAddress(ctx, &dest.address, memory_address.offset);
  return kInstructionExecuted;
}

// ============================================================================
// LES and LDS instructions
// ============================================================================

// Common logic for LES and LDS instructions.
static InstructionResult ExecuteLoadSegmentWithPointer(
    const InstructionContext* ctx, RegisterIndex segment_register_index) {
  Operand destRegister = ReadRegisterOperand(ctx);
  Operand destSegmentRegister =
      ReadRegisterOperandForRegisterIndex(ctx, segment_register_index);

  OperandAddress src_address = {
      .type = kOperandAddressTypeMemory,
      .value = {
          .memory_address = GetMemoryOperandAddress(ctx->cpu, ctx->instruction),
      }};
  OperandValue src_offset_value = ReadMemoryOperandWord(ctx->cpu, &src_address);
  src_address.value.memory_address.offset += 2;
  OperandValue src_segment_value =
      ReadMemoryOperandWord(ctx->cpu, &src_address);

  WriteOperand(ctx, &destRegister, FromOperandValue(&src_offset_value));
  WriteOperand(ctx, &destSegmentRegister, FromOperandValue(&src_segment_value));
  return kInstructionExecuted;
}

// LES r16, m
YAX86_PRIVATE InstructionResult
ExecuteLoadESWithPointer(const InstructionContext* ctx) {
  return ExecuteLoadSegmentWithPointer(ctx, kES);
}

// LDS r16, m
YAX86_PRIVATE InstructionResult
ExecuteLoadDSWithPointer(const InstructionContext* ctx) {
  return ExecuteLoadSegmentWithPointer(ctx, kDS);
}


// ==============================================================================
// src/cpu/instructions_lea.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_add.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_add.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// ADD, ADC, and INC instructions
// ============================================================================

// Set CPU flags after an INC instruction.
// Other than common flags, the INC instruction sets the following flags:
// - Overflow Flag (OF) - Set when result has wrong sign
// - Auxiliary Carry Flag (AF) - carry from bit 3 to bit 4
static void SetFlagsAfterInc(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_carry) {
  SetCommonFlagsAfterInstruction(ctx, result);

  // Overflow Flag (OF) Set when result has wrong sign (both operands have same
  // sign but result has different sign)
  uint32_t sign_bit = kSignBit[ctx->metadata->width];
  bool op1_sign = (op1 & sign_bit) != 0;
  bool op2_sign = (op2 & sign_bit) != 0;
  bool result_sign = (result & sign_bit) != 0;
  CPUSetFlag(
      ctx->cpu, kOF, (op1_sign == op2_sign) && (result_sign != op1_sign));

  // Auxiliary Carry Flag (AF) - carry from bit 3 to bit 4
  CPUSetFlag(
      ctx->cpu, kAF, ((op1 & 0xF) + (op2 & 0xF) + (did_carry ? 1 : 0)) > 0xF);
}

// Set CPU flags after an ADD or ADC instruction.
// Other than the flags set by the INC instruction, the ADD instruction sets the
// following flags:
// - Carry Flag (CF) - Set when result overflows the maximum width
static void SetFlagsAfterAdd(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_carry) {
  SetFlagsAfterInc(ctx, op1, op2, result, did_carry);
  // Carry Flag (CF)
  CPUSetFlag(ctx->cpu, kCF, result > kMaxValue[ctx->metadata->width]);
}

// Common signature of SetFlagsAfterAdd and SetFlagsAfterInc.
typedef void (*SetFlagsAfterAddFn)(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_carry);

// Common logic for ADD, ADC, and INC instructions.
static InstructionResult ExecuteAddCommon(
    const InstructionContext* ctx, Operand* dest, const OperandValue* src_value,
    bool carry, SetFlagsAfterAddFn set_flags_after_fn) {
  uint32_t raw_dest_value = FromOperand(dest);
  uint32_t raw_src_value = FromOperandValue(src_value);
  bool should_carry = carry && CPUGetFlag(ctx->cpu, kCF);
  uint32_t result = raw_dest_value + raw_src_value + (should_carry ? 1 : 0);
  WriteOperand(ctx, dest, result);
  (*set_flags_after_fn)(
      ctx, raw_dest_value, raw_src_value, result, should_carry);
  return kInstructionExecuted;
}

// Common logic for ADD instructions
YAX86_PRIVATE InstructionResult ExecuteAdd(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  return ExecuteAddCommon(
      ctx, dest, src_value, /* carry */ false, SetFlagsAfterAdd);
}

// ADD r/m8, r8
// ADD r/m16, r16
YAX86_PRIVATE InstructionResult
ExecuteAddRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteAdd(ctx, &dest, &src.value);
}

// ADD r8, r/m8
// ADD r16, r/m16
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteAddRegisterOrMemoryToRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteAdd(ctx, &dest, &src.value);
}

// ADD AL, imm8
// ADD AX, imm16
YAX86_PRIVATE InstructionResult
ExecuteAddImmediateToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteAdd(ctx, &dest, &src_value);
}

// Common logic for ADC instructions
YAX86_HOT YAX86_PRIVATE InstructionResult ExecuteAddWithCarry(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  return ExecuteAddCommon(
      ctx, dest, src_value, /* carry */ true, SetFlagsAfterAdd);
}

// ADC r/m8, r8
// ADC r/m16, r16
YAX86_PRIVATE InstructionResult
ExecuteAddRegisterToRegisterOrMemoryWithCarry(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteAddWithCarry(ctx, &dest, &src.value);
}
// ADC r8, r/m8
// ADC r16, r/m16
YAX86_PRIVATE InstructionResult
ExecuteAddRegisterOrMemoryToRegisterWithCarry(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteAddWithCarry(ctx, &dest, &src.value);
}

// ADC AL, imm8
// ADC AX, imm16
YAX86_PRIVATE InstructionResult
ExecuteAddImmediateToALOrAXWithCarry(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteAddWithCarry(ctx, &dest, &src_value);
}

// Common logic for INC instructions
YAX86_PRIVATE InstructionResult
ExecuteInc(const InstructionContext* ctx, Operand* dest) {
  OperandValue src_value = WordValue(1);
  return ExecuteAddCommon(
      ctx, dest, &src_value, /* carry */ false, SetFlagsAfterInc);
}

// INC AX/CX/DX/BX/SP/BP/SI/DI
YAX86_PRIVATE InstructionResult
ExecuteIncRegister(const InstructionContext* ctx) {
  RegisterIndex register_index =
      (RegisterIndex)(ctx->instruction->opcode - 0x40);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  return ExecuteInc(ctx, &dest);
}


// ==============================================================================
// src/cpu/instructions_add.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_sub.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_sub.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// SUB, SBB, and DEC instructions
// ============================================================================

// Set CPU flags after a DEC or SUB/SBB operation (base function).
// This function sets ZF, SF, PF, OF, AF. It does NOT affect CF.
// - OF is for the full operation op1 - (op2 + did_borrow).
// - AF is for the full operation op1 - (op2 + did_borrow).
static void SetFlagsAfterDec(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_borrow) {
  SetCommonFlagsAfterInstruction(ctx, result);

  uint32_t sign_bit = kSignBit[ctx->metadata->width];
  uint32_t max_val = kMaxValue[ctx->metadata->width];

  // Overflow Flag (OF)
  // A subtraction overflows when the operands have different signs and the
  // result takes the sign of the subtrahend rather than the minuend. The
  // borrow does not enter into it: it shifts the result by one, and the result
  // is already what the test looks at. Folding the borrow into the subtrahend
  // instead would change that operand's sign whenever it is 0x7F or 0x7FFF,
  // and give the wrong answer for exactly those values.
  bool op1_sign = (op1 & sign_bit) != 0;
  bool op2_sign = ((op2 & max_val) & sign_bit) != 0;
  bool result_sign = (result & sign_bit) != 0;
  CPUSetFlag(
      ctx->cpu, kOF, (op1_sign != op2_sign) && (result_sign != op1_sign));

  // Auxiliary Carry Flag (AF) - borrow from bit 3 to bit 4
  CPUSetFlag(ctx->cpu, kAF, (op1 & 0xF) < ((op2 & 0xF) + (did_borrow ? 1 : 0)));
}

// Set CPU flags after a SUB, SBB, CMP or NEG instruction.
// This calls SetFlagsAfterDec and then sets the Carry Flag (CF).
YAX86_PRIVATE void SetFlagsAfterSub(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_borrow) {
  SetFlagsAfterDec(ctx, op1, op2, result, did_borrow);

  // Carry Flag (CF) - Set when a borrow is generated
  // CF is set if op1 < (op2 + did_borrow) (unsigned comparison)
  uint32_t val_being_subtracted =
      (op2 & kMaxValue[ctx->metadata->width]) + (did_borrow ? 1 : 0);
  CPUSetFlag(ctx->cpu, kCF, op1 < val_being_subtracted);
}

// Common signature of SetFlagsAfterSub and SetFlagsAfterDec.
typedef void (*SetFlagsAfterSubFn)(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_borrow);

// Common logic for SUB, SBB, and DEC instructions.
YAX86_HOT static InstructionResult ExecuteSubCommon(
    const InstructionContext* ctx, Operand* dest, const OperandValue* src_value,
    bool borrow, SetFlagsAfterSubFn set_flags_after_fn) {
  uint32_t raw_dest_value = FromOperand(dest);
  uint32_t raw_src_value = FromOperandValue(src_value);
  bool should_borrow = borrow && CPUGetFlag(ctx->cpu, kCF);
  uint32_t result = raw_dest_value - raw_src_value - (should_borrow ? 1 : 0);
  WriteOperand(ctx, dest, result);
  (*set_flags_after_fn)(
      ctx, raw_dest_value, raw_src_value, result, should_borrow);
  return kInstructionExecuted;
}

// Common logic for SUB instructions
YAX86_PRIVATE InstructionResult ExecuteSub(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  return ExecuteSubCommon(
      ctx, dest, src_value, /* borrow */ false, SetFlagsAfterSub);
}

// SUB r/m8, r8
// SUB r/m16, r16
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteSubRegisterFromRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteSub(ctx, &dest, &src.value);
}

// SUB r8, r/m8
// SUB r16, r/m16
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteSubRegisterOrMemoryFromRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteSub(ctx, &dest, &src.value);
}

// SUB AL, imm8
// SUB AX, imm16
YAX86_PRIVATE InstructionResult
ExecuteSubImmediateFromALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteSub(ctx, &dest, &src_value);
}

// Common logic for SBB instructions
YAX86_PRIVATE InstructionResult ExecuteSubWithBorrow(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  return ExecuteSubCommon(
      ctx, dest, src_value, /* borrow */ true, SetFlagsAfterSub);
}

// SBB r/m8, r8
// SBB r/m16, r16
YAX86_PRIVATE InstructionResult
ExecuteSubRegisterFromRegisterOrMemoryWithBorrow(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteSubWithBorrow(ctx, &dest, &src.value);
}

// SBB r8, r/m8
// SBB r16, r/m16
YAX86_PRIVATE InstructionResult
ExecuteSubRegisterOrMemoryFromRegisterWithBorrow(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteSubWithBorrow(ctx, &dest, &src.value);
}

// SBB AL, imm8
// SBB AX, imm16
YAX86_PRIVATE InstructionResult
ExecuteSubImmediateFromALOrAXWithBorrow(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteSubWithBorrow(ctx, &dest, &src_value);
}

// Common logic for DEC instructions
YAX86_PRIVATE InstructionResult
ExecuteDec(const InstructionContext* ctx, Operand* dest) {
  OperandValue src_value = WordValue(1);
  return ExecuteSubCommon(
      ctx, dest, &src_value, /* borrow */ false, SetFlagsAfterDec);
}

// DEC AX/CX/DX/BX/SP/BP/SI/DI
YAX86_PRIVATE InstructionResult
ExecuteDecRegister(const InstructionContext* ctx) {
  RegisterIndex register_index =
      (RegisterIndex)(ctx->instruction->opcode - 0x48);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  return ExecuteDec(ctx, &dest);
}


// ==============================================================================
// src/cpu/instructions_sub.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_sign_ext.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_sign_ext.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Sign extension instructions
// ============================================================================

// CBW
YAX86_PRIVATE InstructionResult ExecuteCbw(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (al & kSignBit[kByte]) ? 0xFF : 0x00;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  return kInstructionExecuted;
}

// CWD
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteCwd(const InstructionContext* ctx) {
  ctx->cpu->registers[kDX] =
      (ctx->cpu->registers[kAX] & kSignBit[kWord]) ? 0xFFFF : 0x0000;
  return kInstructionExecuted;
}


// ==============================================================================
// src/cpu/instructions_sign_ext.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_cmp.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_cmp.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// CMP instructions
// ============================================================================

// Common logic for CMP instructions. Computes dest - src and sets flags.
YAX86_HOT YAX86_PRIVATE InstructionResult ExecuteCmp(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  uint32_t raw_dest_value = FromOperand(dest);
  uint32_t raw_src_value = FromOperandValue(src_value);
  uint32_t result = raw_dest_value - raw_src_value;
  SetFlagsAfterSub(ctx, raw_dest_value, raw_src_value, result, false);
  return kInstructionExecuted;
}

// CMP r/m8, r8
// CMP r/m16, r16
YAX86_PRIVATE InstructionResult
ExecuteCmpRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteCmp(ctx, &dest, &src.value);
}

// CMP r8, r/m8
// CMP r16, r/m16
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteCmpRegisterOrMemoryToRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteCmp(ctx, &dest, &src.value);
}

// CMP AL, imm8
// CMP AX, imm16
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteCmpImmediateToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteCmp(ctx, &dest, &src_value);
}


// ==============================================================================
// src/cpu/instructions_cmp.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_bool.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_bool.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Boolean AND, OR and XOR instructions
// ============================================================================

YAX86_PRIVATE void SetFlagsAfterBooleanInstruction(
    const InstructionContext* ctx, uint32_t result) {
  SetCommonFlagsAfterInstruction(ctx, result);
  // Carry Flag (CF) should be cleared
  CPUSetFlag(ctx->cpu, kCF, false);
  // Overflow Flag (OF) should be cleared
  CPUSetFlag(ctx->cpu, kOF, false);
}

// Common logic for AND instructions.
YAX86_PRIVATE InstructionResult ExecuteBooleanAnd(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  uint32_t result = FromOperand(dest) & FromOperandValue(src_value);
  WriteOperand(ctx, dest, result);
  SetFlagsAfterBooleanInstruction(ctx, result);
  return kInstructionExecuted;
}

// AND r/m8, r8
// AND r/m16, r16
YAX86_PRIVATE InstructionResult
ExecuteBooleanAndRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteBooleanAnd(ctx, &dest, &src.value);
}

// AND r8, r/m8
// AND r16, r/m16
YAX86_PRIVATE InstructionResult
ExecuteBooleanAndRegisterOrMemoryToRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteBooleanAnd(ctx, &dest, &src.value);
}

// AND AL, imm8
// AND AX, imm16
YAX86_PRIVATE InstructionResult
ExecuteBooleanAndImmediateToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteBooleanAnd(ctx, &dest, &src_value);
}

// Common logic for OR instructions.
YAX86_PRIVATE InstructionResult ExecuteBooleanOr(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  uint32_t result = FromOperand(dest) | FromOperandValue(src_value);
  WriteOperand(ctx, dest, result);
  SetFlagsAfterBooleanInstruction(ctx, result);
  return kInstructionExecuted;
}

// OR r/m8, r8
// OR r/m16, r16
YAX86_PRIVATE InstructionResult
ExecuteBooleanOrRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteBooleanOr(ctx, &dest, &src.value);
}

// OR r8, r/m8
// OR r16, r/m16
YAX86_PRIVATE InstructionResult
ExecuteBooleanOrRegisterOrMemoryToRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteBooleanOr(ctx, &dest, &src.value);
}

// OR AL, imm8
// OR AX, imm16
YAX86_PRIVATE InstructionResult
ExecuteBooleanOrImmediateToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteBooleanOr(ctx, &dest, &src_value);
}

// Common logic for XOR instructions.
YAX86_PRIVATE InstructionResult ExecuteBooleanXor(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  uint32_t result = FromOperand(dest) ^ FromOperandValue(src_value);
  WriteOperand(ctx, dest, result);
  SetFlagsAfterBooleanInstruction(ctx, result);
  return kInstructionExecuted;
}

// XOR r/m8, r8
// XOR r/m16, r16
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteBooleanXorRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteBooleanXor(ctx, &dest, &src.value);
}

// XOR r8, r/m8
// XOR r16, r/m16
YAX86_PRIVATE InstructionResult
ExecuteBooleanXorRegisterOrMemoryToRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteBooleanXor(ctx, &dest, &src.value);
}

// XOR AL, imm8
// XOR AX, imm16
YAX86_PRIVATE InstructionResult
ExecuteBooleanXorImmediateToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteBooleanXor(ctx, &dest, &src_value);
}

// ============================================================================
// TEST instructions
// ============================================================================

// Common logic for TEST instructions.
YAX86_PRIVATE InstructionResult ExecuteTest(
    const InstructionContext* ctx, Operand* dest, OperandValue* src_value) {
  uint32_t result = FromOperand(dest) & FromOperandValue(src_value);
  SetFlagsAfterBooleanInstruction(ctx, result);
  return kInstructionExecuted;
}

// TEST r/m8, r8
// TEST r/m16, r16
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteTestRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteTest(ctx, &dest, &src.value);
}

// TEST AL, imm8
// TEST AX, imm16
YAX86_PRIVATE InstructionResult
ExecuteTestImmediateToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteTest(ctx, &dest, &src_value);
}


// ==============================================================================
// src/cpu/instructions_bool.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_ctrl_flow.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_ctrl_flow.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// JMP instructions
// ============================================================================

// Jump to a relative signed byte offset.
YAX86_HOT static InstructionResult ExecuteRelativeJumpByte(
    const InstructionContext* ctx, const OperandValue* offset_value) {
  ctx->cpu->registers[kIP] = AddSignedOffsetByte(
      ctx->cpu->registers[kIP], FromOperandValue(offset_value));
  return kInstructionExecuted;
}

// Jump to a relative signed word offset.
YAX86_HOT static InstructionResult ExecuteRelativeJumpWord(
    const InstructionContext* ctx, const OperandValue* offset_value) {
  ctx->cpu->registers[kIP] = AddSignedOffsetWord(
      ctx->cpu->registers[kIP], FromOperandValue(offset_value));
  return kInstructionExecuted;
}

// Table of relative jump instructions, indexed by width.
static InstructionResult (*const kRelativeJumpFn[kNumWidths])(
    const InstructionContext* ctx, const OperandValue* offset_value) = {
    ExecuteRelativeJumpByte,  // kByte
    ExecuteRelativeJumpWord,  // kWord
};

// Common logic for JMP instructions.
static InstructionResult ExecuteRelativeJump(
    const InstructionContext* ctx, const OperandValue* offset_value) {
  return kRelativeJumpFn[ctx->metadata->width](ctx, offset_value);
}

// JMP rel8
// JMP rel16
YAX86_PRIVATE InstructionResult
ExecuteShortOrNearJump(const InstructionContext* ctx) {
  OperandValue offset_value = ReadImmediate(ctx);
  return ExecuteRelativeJump(ctx, &offset_value);
}

// Common logic for far jumps.
YAX86_PRIVATE InstructionResult ExecuteFarJump(
    const InstructionContext* ctx, const OperandValue* segment,
    const OperandValue* offset) {
  ctx->cpu->registers[kCS] = FromOperandValue(segment);
  ctx->cpu->registers[kIP] = FromOperandValue(offset);
  return kInstructionExecuted;
}

// JMP ptr16:16
YAX86_PRIVATE InstructionResult
ExecuteDirectFarJump(const InstructionContext* ctx) {
  OperandValue new_cs = WordValue(
      ((uint16_t)ctx->instruction->immediate[2]) |
      (((uint16_t)ctx->instruction->immediate[3]) << 8));
  OperandValue new_ip = WordValue(
      ((uint16_t)ctx->instruction->immediate[0]) |
      (((uint16_t)ctx->instruction->immediate[1]) << 8));
  return ExecuteFarJump(ctx, &new_cs, &new_ip);
}

// ============================================================================
// Conditional jumps
// ============================================================================

// Common logic for conditional jumps.
static InstructionResult ExecuteConditionalJump(
    const InstructionContext* ctx, bool value, bool success_value) {
  if (value == success_value) {
    // A taken jump throws away the prefetch queue and has to fill it again.
    // The base cost in the cycle table is for the branch not being taken.
    CPUAddCycles(ctx->cpu, kJumpTakenCycles);
    OperandValue offset_value = ReadImmediate(ctx);
    return ExecuteRelativeJump(ctx, &offset_value);
  }
  return kInstructionExecuted;
}

// Table of flag register bitmasks for conditional jumps. The index corresponds
// to (opcode & 0x0F) / 2, so that the undocumented 0x60-0x6F aliases share the
// entries of their 0x70-0x7F counterparts.
static const uint16_t kUnsignedConditionalJumpFlagBitmasks[] = {
    kOF,        // 0x70 - JO, 0x71 - JNO
    kCF,        // 0x72 - JC, 0x73 - JNC
    kZF,        // 0x74 - JE, 0x75 - JNE
    kCF | kZF,  // 0x76 - JBE, 0x77 - JNBE
    kSF,        // 0x78 - JS, 0x79 - JNS
    kPF,        // 0x7A - JP, 0x7B - JNP
};

// Unsigned conditional jumps.
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteUnsignedConditionalJump(const InstructionContext* ctx) {
  // Masking off the high nibble handles both 0x70-0x7F and their undocumented
  // 0x60-0x6F aliases, which the 8086/8088 decodes identically.
  uint16_t flag_mask = kUnsignedConditionalJumpFlagBitmasks
      [(ctx->instruction->opcode & 0x0F) / 2];
  bool flag_value = (ctx->cpu->flags & flag_mask) != 0;
  // Even opcode => jump if the flag is set
  // Odd opcode => jump if the flag is not set
  bool success_value = ((ctx->instruction->opcode & 0x1) == 0);
  return ExecuteConditionalJump(ctx, flag_value, success_value);
}

// JL/JGNE and JNL/JGE
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteSignedConditionalJumpJLOrJNL(const InstructionContext* ctx) {
  const bool is_greater_or_equal =
      CPUGetFlag(ctx->cpu, kSF) == CPUGetFlag(ctx->cpu, kOF);
  const bool success_value = (ctx->instruction->opcode & 0x1);
  return ExecuteConditionalJump(ctx, is_greater_or_equal, success_value);
}

// JLE/JG and JNLE/JG
YAX86_PRIVATE InstructionResult
ExecuteSignedConditionalJumpJLEOrJNLE(const InstructionContext* ctx) {
  const bool is_greater =
      !CPUGetFlag(ctx->cpu, kZF) &&
      (CPUGetFlag(ctx->cpu, kSF) == CPUGetFlag(ctx->cpu, kOF));
  const bool success_value = (ctx->instruction->opcode & 0x1);
  return ExecuteConditionalJump(ctx, is_greater, success_value);
}

// ============================================================================
// Loop instructions
// ============================================================================

// LOOP rel8
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteLoop(const InstructionContext* ctx) {
  return ExecuteConditionalJump(ctx, --(ctx->cpu->registers[kCX]) != 0, true);
}

// LOOPZ rel8
// LOOPNZ rel8
YAX86_PRIVATE InstructionResult
ExecuteLoopZOrNZ(const InstructionContext* ctx) {
  bool condition1 = --(ctx->cpu->registers[kCX]) != 0;
  bool condition2 =
      CPUGetFlag(ctx->cpu, kZF) == (bool)(ctx->instruction->opcode - 0xE0);
  return ExecuteConditionalJump(ctx, condition1 && condition2, true);
}

// JCXZ rel8
YAX86_PRIVATE InstructionResult
ExecuteJumpIfCXIsZero(const InstructionContext* ctx) {
  return ExecuteConditionalJump(ctx, ctx->cpu->registers[kCX] == 0, true);
}

// ============================================================================
// CALL and RET instructions
// ============================================================================

// Common logic for near calls.
static InstructionResult ExecuteNearCall(
    const InstructionContext* ctx, const OperandValue* offset) {
  PushValue(ctx->cpu, WordValue(ctx->cpu->registers[kIP]));
  return ExecuteRelativeJump(ctx, offset);
}

// CALL rel16
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteDirectNearCall(const InstructionContext* ctx) {
  OperandValue offset = ReadImmediate(ctx);
  return ExecuteNearCall(ctx, &offset);
}

// Common logic for far calls.
YAX86_PRIVATE InstructionResult ExecuteFarCall(
    const InstructionContext* ctx, const OperandValue* segment,
    const OperandValue* offset) {
  // Push the current CS and IP onto the stack.
  PushValue(ctx->cpu, WordValue(ctx->cpu->registers[kCS]));
  PushValue(ctx->cpu, WordValue(ctx->cpu->registers[kIP]));
  return ExecuteFarJump(ctx, segment, offset);
}

// CALL ptr16:16
YAX86_PRIVATE InstructionResult
ExecuteDirectFarCall(const InstructionContext* ctx) {
  PushValue(ctx->cpu, WordValue(ctx->cpu->registers[kCS]));
  PushValue(ctx->cpu, WordValue(ctx->cpu->registers[kIP]));
  return ExecuteDirectFarJump(ctx);
}

// Common logic for RET instructions.
static InstructionResult ExecuteNearReturnCommon(
    const InstructionContext* ctx, uint16_t arg_size) {
  OperandValue new_ip = Pop(ctx->cpu);
  ctx->cpu->registers[kIP] = FromOperandValue(&new_ip);
  ctx->cpu->registers[kSP] += arg_size;
  return kInstructionExecuted;
}

// RET
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteNearReturn(const InstructionContext* ctx) {
  return ExecuteNearReturnCommon(ctx, 0);
}

// RET imm16
YAX86_PRIVATE InstructionResult
ExecuteNearReturnAndPop(const InstructionContext* ctx) {
  OperandValue arg_size_value = ReadImmediate(ctx);
  return ExecuteNearReturnCommon(ctx, FromOperandValue(&arg_size_value));
}

// Common logic for RETF instructions.
static InstructionResult ExecuteFarReturnCommon(
    const InstructionContext* ctx, uint16_t arg_size) {
  OperandValue new_ip = Pop(ctx->cpu);
  OperandValue new_cs = Pop(ctx->cpu);
  ctx->cpu->registers[kIP] = FromOperandValue(&new_ip);
  ctx->cpu->registers[kCS] = FromOperandValue(&new_cs);
  ctx->cpu->registers[kSP] += arg_size;
  return kInstructionExecuted;
}

// RETF
YAX86_PRIVATE InstructionResult
ExecuteFarReturn(const InstructionContext* ctx) {
  return ExecuteFarReturnCommon(ctx, 0);
}

// RETF imm16
YAX86_PRIVATE InstructionResult
ExecuteFarReturnAndPop(const InstructionContext* ctx) {
  OperandValue arg_size_value = ReadImmediate(ctx);
  return ExecuteFarReturnCommon(ctx, FromOperandValue(&arg_size_value));
}

// ============================================================================
// Interrupt instructions
// ============================================================================

// Common logic for returning from an interrupt.
YAX86_PRIVATE InstructionResult ExecuteReturnFromInterrupt(CPUState* cpu) {
  OperandValue ip_value = Pop(cpu);
  cpu->registers[kIP] = FromOperandValue(&ip_value);
  OperandValue cs_value = Pop(cpu);
  cpu->registers[kCS] = FromOperandValue(&cs_value);
  OperandValue flags_value = Pop(cpu);
  cpu->flags = ToFlagsRegisterValue(FromOperandValue(&flags_value));
  return kInstructionExecuted;
}

// IRET
YAX86_PRIVATE InstructionResult ExecuteIret(const InstructionContext* ctx) {
  return ExecuteReturnFromInterrupt(ctx->cpu);
}

// INT 3
YAX86_PRIVATE InstructionResult ExecuteInt3(const InstructionContext* ctx) {
  CPURaiseInternalInterrupt(ctx->cpu, kInterruptBreakpoint);
  return kInstructionExecuted;
}

// INTO
YAX86_PRIVATE InstructionResult ExecuteInto(const InstructionContext* ctx) {
  if (CPUGetFlag(ctx->cpu, kOF)) {
    CPURaiseInternalInterrupt(ctx->cpu, kInterruptOverflow);
  }
  return kInstructionExecuted;
}

// INT n
YAX86_PRIVATE InstructionResult ExecuteIntN(const InstructionContext* ctx) {
  OperandValue interrupt_number_value = ReadImmediate(ctx);
  CPURaiseInternalInterrupt(
      ctx->cpu, FromOperandValue(&interrupt_number_value));
  return kInstructionExecuted;
}

// HLT
YAX86_PRIVATE InstructionResult
ExecuteHlt(YAX86_UNUSED const InstructionContext* ctx) {
  // HLT executes successfully - it just leaves the CPU halted until an
  // interrupt wakes it. CPUTick() reports the halted state to its caller.
  ctx->cpu->is_halted = true;
  return kInstructionExecuted;
}


// ==============================================================================
// src/cpu/instructions_ctrl_flow.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_stack.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_stack.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// PUSH and POP instructions
// ============================================================================

// PUSH AX/CX/DX/BX/SP/BP/SI/DI
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecutePushRegister(const InstructionContext* ctx) {
  RegisterIndex register_index =
      (RegisterIndex)(ctx->instruction->opcode - 0x50);
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  PushSourceOperand(ctx->cpu, &src);
  return kInstructionExecuted;
}

// POP AX/CX/DX/BX/SP/BP/SI/DI
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecutePopRegister(const InstructionContext* ctx) {
  RegisterIndex register_index =
      (RegisterIndex)(ctx->instruction->opcode - 0x58);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  OperandValue value = Pop(ctx->cpu);
  WriteOperandAddress(ctx, &dest.address, FromOperandValue(&value));
  return kInstructionExecuted;
}

// PUSH ES/CS/SS/DS
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecutePushSegmentRegister(const InstructionContext* ctx) {
  RegisterIndex register_index =
      (RegisterIndex)(((ctx->instruction->opcode >> 3) & 0x03) + 8);
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  PushValue(ctx->cpu, src.value);
  return kInstructionExecuted;
}

// POP ES/CS/SS/DS
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecutePopSegmentRegister(const InstructionContext* ctx) {
  // The segment register field is only two bits wide, which is what makes
  // 0x0F decode as POP CS on the 8086/8088. Popping into CS is legal there -
  // it just makes the next instruction fetch come from the new segment.
  RegisterIndex register_index =
      (RegisterIndex)(((ctx->instruction->opcode >> 3) & 0x03) + 8);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  OperandValue value = Pop(ctx->cpu);
  WriteOperandAddress(ctx, &dest.address, FromOperandValue(&value));
  return kInstructionExecuted;
}

// PUSHF
YAX86_PRIVATE InstructionResult
ExecutePushFlags(const InstructionContext* ctx) {
  PushValue(ctx->cpu, WordValue(ctx->cpu->flags));
  return kInstructionExecuted;
}

// POPF
YAX86_PRIVATE InstructionResult ExecutePopFlags(const InstructionContext* ctx) {
  OperandValue value = Pop(ctx->cpu);
  ctx->cpu->flags = ToFlagsRegisterValue(FromOperandValue(&value));
  return kInstructionExecuted;
}

// POP r/m16
YAX86_PRIVATE InstructionResult
ExecutePopRegisterOrMemory(const InstructionContext* ctx) {
  // The 8086/8088 does not decode the REG field of 0x8F at all, so every value
  // pops. Only REG 0 is documented.
  //
  // The destination address is resolved before the pop, because SP and BP can
  // both address it and the 8086 computes the effective address first.
  OperandAddress dest = GetRegisterOrMemoryOperandAddress(ctx);
  OperandValue value = Pop(ctx->cpu);
  WriteOperandAddress(ctx, &dest, FromOperandValue(&value));
  return kInstructionExecuted;
}

// ============================================================================
// LAHF and SAHF
// ============================================================================

// Returns the AH register address.
static const OperandAddress* GetAHRegisterAddress(void) {
  static OperandAddress ah = {
      .type = kOperandAddressTypeRegister,
      .value = {
          .register_address = {
              .register_index = kAX,
              .byte_offset = 8,
          }}};
  return &ah;
}

// LAHF
YAX86_PRIVATE InstructionResult
ExecuteLoadAHFromFlags(const InstructionContext* ctx) {
  WriteRegisterOperandByte(
      ctx->cpu, GetAHRegisterAddress(), ByteValue(ctx->cpu->flags & 0x00FF));
  return kInstructionExecuted;
}

// SAHF
YAX86_PRIVATE InstructionResult
ExecuteStoreAHToFlags(const InstructionContext* ctx) {
  OperandValue value =
      ReadRegisterOperandByte(ctx->cpu, GetAHRegisterAddress());
  // Clear the lower byte of flags and set it to the value in AH
  ctx->cpu->flags =
      ToFlagsRegisterValue((ctx->cpu->flags & 0xFF00) | value.value.byte_value);
  return kInstructionExecuted;
}


// ==============================================================================
// src/cpu/instructions_stack.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_flags.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_flags.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// CLC, STC, CLI, STI, CLD, STD instructions
// ============================================================================

// Table of flags corresponding to the CLC, STC, CLI, STI, CLD, and STD
// instructions, indexed by (opcode - 0xF8) / 2.
static const Flag kFlagsForClearAndSetInstructions[] = {
    kCF,  // CLC, STC
    kIF,  // CLI, STI
    kDF,  // CLD, STD
};

YAX86_PRIVATE InstructionResult
ExecuteClearOrSetFlag(const InstructionContext* ctx) {
  uint8_t opcode_index = ctx->instruction->opcode - 0xF8;
  Flag flag = kFlagsForClearAndSetInstructions[opcode_index / 2];
  bool value = (opcode_index & 0x1) != 0;
  CPUSetFlag(ctx->cpu, flag, value);
  return kInstructionExecuted;
}

// ============================================================================
// CMC instruction
// ============================================================================

// CMC
YAX86_PRIVATE InstructionResult
ExecuteComplementCarryFlag(const InstructionContext* ctx) {
  CPUSetFlag(ctx->cpu, kCF, !CPUGetFlag(ctx->cpu, kCF));
  return kInstructionExecuted;
}

// ============================================================================
// SALC instruction
// ============================================================================

// SALC - Set AL from Carry
//
// Undocumented on every x86 generation, but consistently implemented: AL
// becomes 0xFF if CF is set and 0x00 otherwise. No flags are affected.
YAX86_PRIVATE InstructionResult
ExecuteSetALFromCarry(const InstructionContext* ctx) {
  const uint8_t value = CPUGetFlag(ctx->cpu, kCF) ? 0xFF : 0x00;
  ctx->cpu->registers[kAX] = (ctx->cpu->registers[kAX] & 0xFF00) | value;
  return kInstructionExecuted;
}


// ==============================================================================
// src/cpu/instructions_flags.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_io.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_io.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// IN and OUT instructions
// ============================================================================

// Read a byte from an I/O port.
YAX86_HOT static OperandValue ReadByteFromPort(CPUState* cpu, uint16_t port) {
  return ByteValue(
      cpu->config->read_port ? cpu->config->read_port(cpu, port) : 0xFF);
}

// Read a word from an I/O port as a uint16_t. The 8088 has an 8-bit data bus,
// so a word access is two byte accesses to consecutive ports. Peripherals rely
// on this: writing a 6845 register index and its data in one OUT DX, AX is the
// standard idiom, and it is what the BIOS uses.
static OperandValue ReadWordFromPort(CPUState* cpu, uint16_t port) {
  uint8_t low = ReadByteFromPort(cpu, port).value.byte_value;
  uint8_t high = ReadByteFromPort(cpu, (uint16_t)(port + 1)).value.byte_value;
  return WordValue((high << 8) | low);
}

// Table of functions to read from an I/O port, indexed by data width.
static OperandValue (*const kReadFromPortFns[])(CPUState*, uint16_t) = {
    ReadByteFromPort,  // kByte
    ReadWordFromPort,  // kWord
};

// Common logic for IN instructions.
static InstructionResult ExecuteIn(
    const InstructionContext* ctx, uint16_t port) {
  OperandValue value = kReadFromPortFns[ctx->metadata->width](ctx->cpu, port);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  WriteOperand(ctx, &dest, FromOperandValue(&value));
  return kInstructionExecuted;
}

// IN AL, imm8
// IN AX, imm8
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteInImmediate(const InstructionContext* ctx) {
  OperandValue port = ReadImmediateOperandByte(ctx->instruction);
  return ExecuteIn(ctx, FromOperandValue(&port));
}

// IN AL, DX
// IN AX, DX
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteInDX(const InstructionContext* ctx) {
  return ExecuteIn(ctx, ctx->cpu->registers[kDX]);
}

// Write a byte to an I/O port.
static void WriteByteToPort(CPUState* cpu, uint16_t port, OperandValue value) {
  if (!cpu->config->write_port) {
    return;
  }
  cpu->config->write_port(cpu, port, FromOperandValue(&value));
}

// Write a word to an I/O port. As with reads, this is two byte accesses to
// consecutive ports.
static void WriteWordToPort(CPUState* cpu, uint16_t port, OperandValue value) {
  uint32_t raw_value = FromOperandValue(&value);
  WriteByteToPort(cpu, port, ByteValue(raw_value & 0xFF));
  WriteByteToPort(
      cpu, (uint16_t)(port + 1), ByteValue((raw_value >> 8) & 0xFF));
}

// Table of functions to write to an I/O port, indexed by data width.
static void (*const kWriteToPortFns[])(CPUState*, uint16_t, OperandValue) = {
    WriteByteToPort,  // kByte
    WriteWordToPort,  // kWord
};

// Common logic for OUT instructions.
static InstructionResult ExecuteOut(
    const InstructionContext* ctx, uint16_t port) {
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  kWriteToPortFns[ctx->metadata->width](ctx->cpu, port, src.value);
  return kInstructionExecuted;
}

// OUT imm8, AL
// OUT imm8, AX
YAX86_PRIVATE InstructionResult
ExecuteOutImmediate(const InstructionContext* ctx) {
  OperandValue port = ReadImmediateOperandByte(ctx->instruction);
  return ExecuteOut(ctx, FromOperandValue(&port));
}

// OUT DX, AL
// OUT DX, AX
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteOutDX(const InstructionContext* ctx) {
  return ExecuteOut(ctx, ctx->cpu->registers[kDX]);
}


// ==============================================================================
// src/cpu/instructions_io.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_string.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_string.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// String instructions
// ============================================================================

// Get the repetition prefix of a string instruction, if any.
static inline uint8_t GetRepetitionPrefix(const InstructionContext* ctx) {
  return ctx->instruction->repetition_prefix;
}

// Get the source operand for string instructions. Typically DS:SI but can be
// overridden by a segment override prefix.
static Operand GetStringSourceOperand(const InstructionContext* ctx) {
  OperandAddress address = {
      .type = kOperandAddressTypeMemory,
      .value =
          {
              .memory_address =
                  {
                      .segment_register_index = kDS,
                      .offset = ctx->cpu->registers[kSI],
                  },
          },
  };
  ApplySegmentOverride(ctx->instruction, &address.value.memory_address);
  Operand operand = {
      .address = address,
      .value = ReadOperandValue(ctx, &address),
  };
  return operand;
}

// Get the destination operand address for string instructions. Always ES:DI.
static OperandAddress GetStringDestinationOperandAddress(
    const InstructionContext* ctx) {
  OperandAddress address = {
      .type = kOperandAddressTypeMemory,
      .value =
          {
              .memory_address =
                  {
                      .segment_register_index = kES,
                      .offset = ctx->cpu->registers[kDI],
                  },
          },
  };
  return address;
}

// Get the destination operand for string instructions. Always ES:DI.
static Operand GetStringDestinationOperand(const InstructionContext* ctx) {
  OperandAddress address = GetStringDestinationOperandAddress(ctx);
  Operand operand = {
      .address = address,
      .value = ReadOperandValue(ctx, &address),
  };
  return operand;
}

// Update the source address register (SI) after a string operation.
static void UpdateStringSourceAddress(const InstructionContext* ctx) {
  if (CPUGetFlag(ctx->cpu, kDF)) {
    ctx->cpu->registers[kSI] -= kNumBytes[ctx->metadata->width];
  } else {
    ctx->cpu->registers[kSI] += kNumBytes[ctx->metadata->width];
  }
}

// Update the destination address register (DI) after a string operation.
static void UpdateStringDestinationAddress(const InstructionContext* ctx) {
  if (CPUGetFlag(ctx->cpu, kDF)) {
    ctx->cpu->registers[kDI] -= kNumBytes[ctx->metadata->width];
  } else {
    ctx->cpu->registers[kDI] += kNumBytes[ctx->metadata->width];
  }
}

// Execute a string instruction with optional REP prefix.
//
// MOVS, STOS and LODS set no flags, so a repetition prefix has no zero flag to
// test and the 8086/8088 does not tell the two prefixes apart here: 0xF2
// repeats exactly as 0xF3 does, counting CX down to zero. Only the comparison
// string instructions read the prefix as a condition.
static InstructionResult ExecuteStringInstructionWithREPPrefix(
    const InstructionContext* ctx,
    InstructionResult (*fn)(const InstructionContext*)) {
  uint8_t prefix = GetRepetitionPrefix(ctx);
  if (prefix != kPrefixREP && prefix != kPrefixREPNZ) {
    return fn(ctx);
  }
  while (ctx->cpu->registers[kCX]) {
    InstructionResult status = fn(ctx);
    if (status != kInstructionExecuted) {
      return status;
    }
    --ctx->cpu->registers[kCX];
  }
  return kInstructionExecuted;
}

// Single MOVS iteration.
static InstructionResult ExecuteMovsIteration(const InstructionContext* ctx) {
  Operand src = GetStringSourceOperand(ctx);
  OperandAddress dest_address = GetStringDestinationOperandAddress(ctx);
  WriteOperandAddress(ctx, &dest_address, FromOperand(&src));
  UpdateStringSourceAddress(ctx);
  UpdateStringDestinationAddress(ctx);
  return kInstructionExecuted;
}

// MOVS
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteMovs(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPPrefix(ctx, ExecuteMovsIteration);
}

// Single STOS iteration.
static InstructionResult ExecuteStosIteration(const InstructionContext* ctx) {
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandAddress dest_address = GetStringDestinationOperandAddress(ctx);
  WriteOperandAddress(ctx, &dest_address, FromOperand(&src));
  UpdateStringDestinationAddress(ctx);
  return kInstructionExecuted;
}

// STOS
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteStos(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPPrefix(ctx, ExecuteStosIteration);
}

// Single LODS iteration.
static InstructionResult ExecuteLodsIteration(const InstructionContext* ctx) {
  Operand src = GetStringSourceOperand(ctx);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  WriteOperand(ctx, &dest, FromOperand(&src));
  UpdateStringSourceAddress(ctx);
  return kInstructionExecuted;
}

// LODS
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteLods(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPPrefix(ctx, ExecuteLodsIteration);
}

// Execute a string instruction with optional REPZ/REPE or REPNZ/REPNE prefix.
static InstructionResult ExecuteStringInstructionWithREPZOrRepNZPrefix(
    const InstructionContext* ctx,
    InstructionResult (*fn)(const InstructionContext*)) {
  uint8_t prefix = GetRepetitionPrefix(ctx);
  if (prefix != kPrefixREP && prefix != kPrefixREPNZ) {
    return fn(ctx);
  }
  bool terminate_zf_value = prefix == kPrefixREPNZ;
  while (ctx->cpu->registers[kCX]) {
    InstructionResult status = fn(ctx);
    if (status != kInstructionExecuted) {
      return status;
    }
    --ctx->cpu->registers[kCX];
    if (CPUGetFlag(ctx->cpu, kZF) == terminate_zf_value) {
      break;
    }
  }
  return kInstructionExecuted;
}

// Single SCAS iteration.
YAX86_HOT static InstructionResult ExecuteScasIteration(
    const InstructionContext* ctx) {
  Operand src = GetStringDestinationOperand(ctx);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  ExecuteCmp(ctx, &dest, &src.value);
  UpdateStringDestinationAddress(ctx);
  return kInstructionExecuted;
}

// SCAS
YAX86_PRIVATE InstructionResult ExecuteScas(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPZOrRepNZPrefix(
      ctx, ExecuteScasIteration);
}

// Single CMPS iteration.
static InstructionResult ExecuteCmpsIteration(const InstructionContext* ctx) {
  Operand dest = GetStringSourceOperand(ctx);
  Operand src = GetStringDestinationOperand(ctx);
  ExecuteCmp(ctx, &dest, &src.value);
  UpdateStringSourceAddress(ctx);
  UpdateStringDestinationAddress(ctx);
  return kInstructionExecuted;
}

// CMPS
YAX86_PRIVATE InstructionResult ExecuteCmps(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPZOrRepNZPrefix(
      ctx, ExecuteCmpsIteration);
}


// ==============================================================================
// src/cpu/instructions_string.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_bcd_ascii.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_bcd_ascii.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// BCD and ASCII arithmetic instructions
// ============================================================================

// The value DAA and DAS compare AL against to decide whether the high digit
// needs adjusting. Both test AL as it was on entry, before the low digit was
// adjusted, so a carry out of the low digit does not drag the high one along
// with it.
//
// The limit is one higher when the auxiliary carry flag was already set on
// entry. That is not what Intel's published pseudocode says, which uses 0x99
// throughout, but it is what the 8086/8088 does - and with AL between 0x9A and
// 0x9F it is the difference between adjusting and not.
static uint8_t GetBCDHighDigitLimit(bool auxiliary_carry) {
  return auxiliary_carry ? 0x9F : 0x99;
}

// AAA
YAX86_PRIVATE InstructionResult ExecuteAaa(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  uint8_t al_low = al & 0x0F;
  if (al_low > 9 || CPUGetFlag(ctx->cpu, kAF)) {
    al += 6;
    ++ah;
    CPUSetFlag(ctx->cpu, kAF, true);
    CPUSetFlag(ctx->cpu, kCF, true);
  } else {
    CPUSetFlag(ctx->cpu, kAF, false);
    CPUSetFlag(ctx->cpu, kCF, false);
  }
  al &= 0x0F;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  return kInstructionExecuted;
}

// AAS
YAX86_PRIVATE InstructionResult ExecuteAas(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  uint8_t al_low = al & 0x0F;
  if (al_low > 9 || CPUGetFlag(ctx->cpu, kAF)) {
    al -= 6;
    --ah;
    CPUSetFlag(ctx->cpu, kAF, true);
    CPUSetFlag(ctx->cpu, kCF, true);
  } else {
    CPUSetFlag(ctx->cpu, kAF, false);
    CPUSetFlag(ctx->cpu, kCF, false);
  }
  al &= 0x0F;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  return kInstructionExecuted;
}

// AAM
YAX86_PRIVATE InstructionResult ExecuteAam(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  OperandValue base = ReadImmediate(ctx);
  uint16_t base_value = FromOperandValue(&base);
  if (base_value == 0) {
    // AAM divides by its immediate operand, so a base of 0 raises a divide
    // error just like DIV by zero does, rather than being an invalid encoding.
    // The division never produces a result, and the flags are left set as
    // though it had produced zero.
    SetCommonFlagsAfterInstruction(ctx, 0);
    CPUSetFlag(ctx->cpu, kCF, false);
    CPUSetFlag(ctx->cpu, kAF, false);
    CPUSetFlag(ctx->cpu, kOF, false);
    CPURaiseInternalInterrupt(ctx->cpu, kInterruptDivideError);
    return kInstructionExecuted;
  }
  uint8_t ah = al / base_value;
  al %= base_value;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  SetCommonFlagsAfterInstruction(ctx, al);
  return kInstructionExecuted;
}

// AAD
YAX86_PRIVATE InstructionResult ExecuteAad(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  OperandValue base = ReadImmediate(ctx);
  uint8_t base_value = FromOperandValue(&base);
  al += ah * base_value;
  ah = 0;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  SetCommonFlagsAfterInstruction(ctx, al);
  return kInstructionExecuted;
}

// DAA
YAX86_PRIVATE InstructionResult ExecuteDaa(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  const uint8_t original_al = al;
  const bool original_carry = CPUGetFlag(ctx->cpu, kCF);
  const uint8_t high_digit_limit =
      GetBCDHighDigitLimit(CPUGetFlag(ctx->cpu, kAF));

  if ((al & 0x0F) > 9 || CPUGetFlag(ctx->cpu, kAF)) {
    al += 6;
    CPUSetFlag(ctx->cpu, kAF, true);
  } else {
    CPUSetFlag(ctx->cpu, kAF, false);
  }
  if (original_al > high_digit_limit || original_carry) {
    al += 0x60;
    CPUSetFlag(ctx->cpu, kCF, true);
  } else {
    CPUSetFlag(ctx->cpu, kCF, false);
  }
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  SetCommonFlagsAfterInstruction(ctx, al);
  return kInstructionExecuted;
}

// DAS
YAX86_PRIVATE InstructionResult ExecuteDas(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  const uint8_t original_al = al;
  const bool original_carry = CPUGetFlag(ctx->cpu, kCF);
  const uint8_t high_digit_limit =
      GetBCDHighDigitLimit(CPUGetFlag(ctx->cpu, kAF));

  if ((al & 0x0F) > 9 || CPUGetFlag(ctx->cpu, kAF)) {
    al -= 6;
    CPUSetFlag(ctx->cpu, kAF, true);
  } else {
    CPUSetFlag(ctx->cpu, kAF, false);
  }
  if (original_al > high_digit_limit || original_carry) {
    al -= 0x60;
    CPUSetFlag(ctx->cpu, kCF, true);
  } else {
    CPUSetFlag(ctx->cpu, kCF, false);
  }
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  SetCommonFlagsAfterInstruction(ctx, al);
  return kInstructionExecuted;
}


// ==============================================================================
// src/cpu/instructions_bcd_ascii.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_group_1.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_group_1.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 1 - ADD, OR, ADC, SBB, AND, SUB, XOR, CMP
// ============================================================================

typedef InstructionResult (*Group1ExecuteInstructionFn)(
    const InstructionContext* ctx, Operand* dest, const OperandValue* src);

// Group 1 instruction implementations, indexed by the corresponding REG field
// value in the ModRM byte.
static const Group1ExecuteInstructionFn kGroup1ExecuteInstructionFns[] = {
    ExecuteAdd,            // 0 - ADD
    ExecuteBooleanOr,      // 1 - OR
    ExecuteAddWithCarry,   // 2 - ADC
    ExecuteSubWithBorrow,  // 3 - SBB
    ExecuteBooleanAnd,     // 4 - AND
    ExecuteSub,            // 5 - SUB
    ExecuteBooleanXor,     // 6 - XOR
    ExecuteCmp,            // 7 - CMP
};

// Group 1 instruction handler.
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteGroup1Instruction(const InstructionContext* ctx) {
  const Group1ExecuteInstructionFn fn =
      kGroup1ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  OperandValue src_value = ReadImmediate(ctx);
  return fn(ctx, &dest, &src_value);
}

// Group 1 instruction handler, but sign-extends the 8-bit immediate value.
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteGroup1InstructionWithSignExtension(const InstructionContext* ctx) {
  const Group1ExecuteInstructionFn fn =
      kGroup1ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  OperandValue src_value =
      ReadImmediateOperandByte(ctx->instruction);  // immediate is always 8-bit
  OperandValue src_value_extended =
      WordValue((uint16_t)((int16_t)((int8_t)src_value.value.byte_value)));
  // Sign-extend the immediate value to the destination width.
  return fn(ctx, &dest, &src_value_extended);
}


// ==============================================================================
// src/cpu/instructions_group_1.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_group_2.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_group_2.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "cycles.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 2 - ROL, ROR, RCL, RCR, SHL, SHR, SAL, SAR
// ============================================================================

typedef InstructionResult (*Group2ExecuteInstructionFn)(
    const InstructionContext* ctx, Operand* op, uint8_t count);

// The 8086/8088 shifts one bit at a time and recomputes the overflow flag on
// every pass, so a multi-bit shift leaves behind the flag from its last pass
// rather than an undefined value. Each rule below is that last pass written in
// closed form.

// Overflow after a left shift or rotate: the bit shifted out of the top
// differs from the sign bit left behind, which is to say the last pass changed
// the sign of the value.
static void SetOverflowFlagAfterLeftShift(
    const InstructionContext* ctx, uint32_t result, bool carry) {
  const bool result_sign = (result & kSignBit[ctx->metadata->width]) != 0;
  CPUSetFlag(ctx->cpu, kOF, carry != result_sign);
}

// Overflow after a right rotate: the top two bits of the result differ. The
// bit rotated into the top came from the bottom, so this again says the last
// pass changed the sign of the value.
static void SetOverflowFlagAfterRightRotate(
    const InstructionContext* ctx, uint32_t result) {
  const uint32_t sign_bit = kSignBit[ctx->metadata->width];
  const bool result_sign = (result & sign_bit) != 0;
  const bool below_sign = (result & (sign_bit >> 1)) != 0;
  CPUSetFlag(ctx->cpu, kOF, result_sign != below_sign);
}

// The 8086/8088 does not mask the shift count the way the 80186 and later do,
// so a count taken from CL can be as large as 255. Once an operand has been
// shifted past its own width there is nothing left in it and nothing left to
// fall out of it, so every larger count behaves alike. Clamping to just past
// the width keeps that true while holding the shifts below the width of the
// intermediate they are computed in, where C leaves them undefined.
static uint8_t ClampShiftCount(const InstructionContext* ctx, uint8_t count) {
  const uint8_t limit = kNumBits[ctx->metadata->width] + 1;
  return count > limit ? limit : count;
}

// SHL r/m8, 1
// SHL r/m16, 1
// SHL r/m8, CL
// SHL r/m16, CL
static InstructionResult ExecuteGroup2Shl(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kInstructionExecuted;
  }
  count = ClampShiftCount(ctx, count);
  uint32_t value = FromOperand(op);
  uint32_t result = (value << count) & kMaxValue[ctx->metadata->width];
  WriteOperand(ctx, op, result);
  bool last_msb =
      ((value << (count - 1)) & kSignBit[ctx->metadata->width]) != 0;
  CPUSetFlag(ctx->cpu, kCF, last_msb);
  SetOverflowFlagAfterLeftShift(ctx, result, last_msb);
  SetCommonFlagsAfterInstruction(ctx, result);
  return kInstructionExecuted;
}

// SHR r/m8, 1
// SHR r/m16, 1
// SHR r/m8, CL
// SHR r/m16, CL
YAX86_HOT static InstructionResult ExecuteGroup2Shr(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kInstructionExecuted;
  }
  count = ClampShiftCount(ctx, count);
  uint32_t value = FromOperand(op);
  uint32_t result = value >> count;
  WriteOperand(ctx, op, result);
  bool last_lsb = ((value >> (count - 1)) & 1) != 0;
  CPUSetFlag(ctx->cpu, kCF, last_lsb);
  // A right shift clears the sign bit on its first pass, so only a shift by
  // one can leave a sign change behind.
  bool original_msb = ((value & kSignBit[ctx->metadata->width]) != 0);
  CPUSetFlag(ctx->cpu, kOF, count == 1 && original_msb);
  SetCommonFlagsAfterInstruction(ctx, result);
  return kInstructionExecuted;
}

// SAR r/m8, 1
// SAR r/m16, 1
// SAR r/m8, CL
// SAR r/m16, CL
static InstructionResult ExecuteGroup2Sar(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kInstructionExecuted;
  }
  count = ClampShiftCount(ctx, count);
  int32_t value = FromSignedOperand(op);
  int32_t result = value >> count;
  WriteOperand(ctx, op, result);
  bool last_lsb = ((value >> (count - 1)) & 1) != 0;
  CPUSetFlag(ctx->cpu, kCF, last_lsb);
  // An arithmetic right shift preserves the sign bit, so it can never
  // overflow.
  CPUSetFlag(ctx->cpu, kOF, false);
  SetCommonFlagsAfterInstruction(ctx, result);
  return kInstructionExecuted;
}

// ROL r/m8, 1
// ROL r/m16, 1
// ROL r/m8, CL
// ROL r/m16, CL
static InstructionResult ExecuteGroup2Rol(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kInstructionExecuted;
  }
  // The 8086 computes the modulus of the count after the zero check, which is
  // different than the 80286 and later processors.
  uint8_t effective_count = count % kNumBits[ctx->metadata->width];
  uint32_t value = FromOperand(op);
  uint32_t result =
      (value << effective_count) |
      (value >> (kNumBits[ctx->metadata->width] - effective_count));
  WriteOperand(ctx, op, result);
  bool last_msb = (result & 1) != 0;
  CPUSetFlag(ctx->cpu, kCF, last_msb);
  SetOverflowFlagAfterLeftShift(ctx, result, last_msb);
  return kInstructionExecuted;
}

// ROR r/m8, 1
// ROR r/m16, 1
// ROR r/m8, CL
// ROR r/m16, CL
static InstructionResult ExecuteGroup2Ror(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kInstructionExecuted;
  }
  // The 8086 computes the modulus of the count after the zero check, which is
  // different than the 80286 and later processors.
  uint8_t effective_count = count % kNumBits[ctx->metadata->width];
  uint32_t value = FromOperand(op);
  uint32_t result =
      (value >> effective_count) |
      (value << (kNumBits[ctx->metadata->width] - effective_count));
  WriteOperand(ctx, op, result);
  bool last_lsb = (result & kSignBit[ctx->metadata->width]) != 0;
  CPUSetFlag(ctx->cpu, kCF, last_lsb);
  SetOverflowFlagAfterRightRotate(ctx, result);
  return kInstructionExecuted;
}

// RCL r/m8, 1
// RCL r/m16, 1
// RCL r/m8, CL
// RCL r/m16, CL
static InstructionResult ExecuteGroup2Rcl(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kInstructionExecuted;
  }
  const Width width = ctx->metadata->width;
  // Rotating through the carry flag cycles over one more bit than the operand
  // is wide. A whole number of cycles puts the value and the carry back the
  // way they were, but the passes still happened, so the overflow flag is
  // still left over from the last one.
  uint8_t effective_count = count % (kNumBits[width] + 1);
  uint32_t value = FromOperand(op);
  uint32_t result = value;
  bool last_msb = CPUGetFlag(ctx->cpu, kCF);
  if (effective_count > 0) {
    uint32_t cf_value = last_msb ? (1 << (effective_count - 1)) : 0;
    result = ((value << effective_count) | cf_value |
              (value >> (kNumBits[width] - (effective_count - 1)))) &
             kMaxValue[width];
    WriteOperand(ctx, op, result);
    last_msb = ((value << (effective_count - 1)) & kSignBit[width]) != 0;
  }
  CPUSetFlag(ctx->cpu, kCF, last_msb);
  SetOverflowFlagAfterLeftShift(ctx, result, last_msb);
  return kInstructionExecuted;
}

// RCR r/m8, 1
// RCR r/m16, 1
// RCR r/m8, CL
// RCR r/m16, CL
static InstructionResult ExecuteGroup2Rcr(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kInstructionExecuted;
  }
  const Width width = ctx->metadata->width;
  // As with RCL, a whole number of cycles restores the value and the carry but
  // still leaves the overflow flag from the last pass.
  uint8_t effective_count = count % (kNumBits[width] + 1);
  uint32_t value = FromOperand(op);
  uint32_t result = value;
  bool last_lsb = CPUGetFlag(ctx->cpu, kCF);
  if (effective_count > 0) {
    uint32_t cf_value =
        last_lsb ? (kSignBit[width] >> (effective_count - 1)) : 0;
    result = ((value >> effective_count) | cf_value |
              (value << (kNumBits[width] - (effective_count - 1)))) &
             kMaxValue[width];
    WriteOperand(ctx, op, result);
    last_lsb = ((value >> (effective_count - 1)) & 1) != 0;
  }
  CPUSetFlag(ctx->cpu, kCF, last_lsb);
  SetOverflowFlagAfterRightRotate(ctx, result);
  return kInstructionExecuted;
}

// SETMO r/m8, 1
// SETMO r/m16, 1
// SETMOC r/m8, CL
// SETMOC r/m16, CL
//
// Undocumented. REG 6 of the shift group is a distinct operation on the
// 8086/8088 rather than an alias of SAL: it sets every bit of the operand,
// hence "set minus one". The count still gates it, so the CL forms - SETMOC,
// "set minus one conditional" - do nothing at all when CL is zero.
//
// Nothing an IBM PC/XT runs uses this, but leaving REG 6 aliased to SAL would
// silently give a different answer than the hardware for the same encoding,
// and the operation is a single store.
static InstructionResult ExecuteGroup2Setmo(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kInstructionExecuted;
  }
  const uint32_t result = kMaxValue[ctx->metadata->width];
  WriteOperand(ctx, op, result);
  CPUSetFlag(ctx->cpu, kCF, false);
  CPUSetFlag(ctx->cpu, kAF, false);
  CPUSetFlag(ctx->cpu, kOF, false);
  SetCommonFlagsAfterInstruction(ctx, result);
  return kInstructionExecuted;
}

static const Group2ExecuteInstructionFn kGroup2ExecuteInstructionFns[] = {
    ExecuteGroup2Rol,    // 0 - ROL
    ExecuteGroup2Ror,    // 1 - ROR
    ExecuteGroup2Rcl,    // 2 - RCL
    ExecuteGroup2Rcr,    // 3 - RCR
    ExecuteGroup2Shl,    // 4 - SHL
    ExecuteGroup2Shr,    // 5 - SHR
    ExecuteGroup2Setmo,  // 6 - SETMO / SETMOC
    ExecuteGroup2Sar,    // 7 - SAR
};

// Group 2 shift / rotate by 1.
YAX86_PRIVATE InstructionResult
ExecuteGroup2ShiftOrRotateBy1Instruction(const InstructionContext* ctx) {
  const Group2ExecuteInstructionFn fn =
      kGroup2ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand op = ReadRegisterOrMemoryOperand(ctx);
  return fn(ctx, &op, 1);
}

// Group 2 shift / rotate by CL.
YAX86_PRIVATE InstructionResult
ExecuteGroup2ShiftOrRotateByCLInstruction(const InstructionContext* ctx) {
  const Group2ExecuteInstructionFn fn =
      kGroup2ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand op = ReadRegisterOrMemoryOperand(ctx);
  const uint8_t count = ctx->cpu->registers[kCX] & 0xFF;
  // A shift by CL works through the count a bit at a time.
  CPUAddCycles(ctx->cpu, (uint16_t)count * kShiftCyclesPerBit);
  return fn(ctx, &op, count);
}


// ==============================================================================
// src/cpu/instructions_group_2.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_group_3.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_group_3.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 3 - TEST, NOT, NEG, MUL, IMUL, DIV, IDIV
// ============================================================================

typedef InstructionResult (*Group3ExecuteInstructionFn)(
    const InstructionContext* ctx, Operand* op);

// TEST r/m8, imm8
// TEST r/m16, imm16
static InstructionResult ExecuteGroup3Test(
    const InstructionContext* ctx, Operand* op) {
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteTest(ctx, op, &src_value);
}

// NOT r/m8
// NOT r/m16
static InstructionResult ExecuteNot(
    const InstructionContext* ctx, Operand* op) {
  WriteOperand(ctx, op, ~FromOperand(op));
  return kInstructionExecuted;
}

// NEG r/m8
// NEG r/m16
static InstructionResult ExecuteNeg(
    const InstructionContext* ctx, Operand* op) {
  int32_t op_value = FromSignedOperand(op);
  int32_t result_value = -op_value;
  WriteOperand(ctx, op, result_value);
  SetFlagsAfterSub(ctx, 0, op_value, result_value, false);
  return kInstructionExecuted;
}

// Table of where to store the higher half of the result for
// MUL, IMUL, DIV, and IDIV instructions, indexed by the data width.
static const OperandAddress kMulDivResultHighHalfAddress[kNumWidths] = {
    {.type = kOperandAddressTypeRegister,
     .value =
         {
             .register_address =
                 {
                     .register_index = kAX,
                     .byte_offset = 8,
                 },
         }},
    {.type = kOperandAddressTypeRegister,
     .value = {
         .register_address =
             {
                 .register_index = kDX,
             },
     }}};

// Number of bits to shift to extract the high part of the result of MUL, IMUL,
// DIV, and IDIV instructions, indexed by the data width.
static const uint8_t kMulDivResultHighHalfShiftWidth[kNumWidths] = {
    8,   // kByte
    16,  // kWord
};

// What the multiply and divide instructions cost. These dominate their own
// timing by an order of magnitude - the 8086/8088 works through them a bit at
// a time in microcode - so unlike everything else in the cycle table they are
// worth charging individually. The published figures are ranges that depend on
// the operands; these are the low end of each.
static const uint16_t kMulDivCycles[kNumWidths][2] = {
    // kByte: unsigned, signed
    {70, 80},
    // kWord: unsigned, signed
    {118, 128},
};

// Common logic for MUL and IMUL instructions.
static InstructionResult ExecuteMulCommon(
    const InstructionContext* ctx, Operand* dest, uint32_t result,
    bool overflow) {
  Width width = ctx->metadata->width;

  uint32_t result_low_half = result & kMaxValue[width];
  WriteOperand(ctx, dest, result_low_half);

  uint32_t result_high_half =
      (result >> kMulDivResultHighHalfShiftWidth[width]) & kMaxValue[width];
  WriteOperandAddress(
      ctx, &kMulDivResultHighHalfAddress[width], result_high_half);

  CPUSetFlag(ctx->cpu, kCF, overflow);
  CPUSetFlag(ctx->cpu, kOF, overflow);

  return kInstructionExecuted;
}

// MUL r/m8
// MUL r/m16
static InstructionResult ExecuteMul(
    const InstructionContext* ctx, Operand* op) {
  CPUAddCycles(ctx->cpu, kMulDivCycles[ctx->metadata->width][0]);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  uint32_t result = FromOperand(&dest) * FromOperand(op);
  return ExecuteMulCommon(
      ctx, &dest, result, result > kMaxValue[ctx->metadata->width]);
}

// IMUL r/m8
// IMUL r/m16
static InstructionResult ExecuteImul(
    const InstructionContext* ctx, Operand* op) {
  CPUAddCycles(ctx->cpu, kMulDivCycles[ctx->metadata->width][1]);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  int32_t result = FromSignedOperand(&dest) * FromSignedOperand(op);
  return ExecuteMulCommon(
      ctx, &dest, result,
      result > kMaxSignedValue[ctx->metadata->width] ||
          result < kMinSignedValue[ctx->metadata->width]);
}

static InstructionResult WriteDivResult(
    const InstructionContext* ctx, Operand* dest, uint32_t quotient,
    uint32_t remainder) {
  WriteOperand(ctx, dest, quotient);
  WriteOperandAddress(
      ctx, &kMulDivResultHighHalfAddress[ctx->metadata->width], remainder);
  return kInstructionExecuted;
}

// DIV r/m8
// DIV r/m16
static InstructionResult ExecuteDiv(
    const InstructionContext* ctx, Operand* op) {
  CPUAddCycles(ctx->cpu, kMulDivCycles[ctx->metadata->width][0]);
  uint32_t divisor = FromOperand(op);
  if (divisor == 0) {
    CPURaiseInternalInterrupt(ctx->cpu, kInterruptDivideError);
    return kInstructionExecuted;
  }

  Width width = ctx->metadata->width;
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);

  OperandValue dest_high_half =
      ReadOperandValue(ctx, &kMulDivResultHighHalfAddress[width]);
  uint32_t dividend =
      FromOperand(&dest) | (FromOperandValue(&dest_high_half)
                            << kMulDivResultHighHalfShiftWidth[width]);
  uint32_t quotient = dividend / divisor;
  if (quotient > kMaxValue[ctx->metadata->width]) {
    CPURaiseInternalInterrupt(ctx->cpu, kInterruptDivideError);
    return kInstructionExecuted;
  }
  return WriteDivResult(ctx, &dest, quotient, dividend % divisor);
}

// IDIV r/m8
// IDIV r/m16
static InstructionResult ExecuteIdiv(
    const InstructionContext* ctx, Operand* op) {
  CPUAddCycles(ctx->cpu, kMulDivCycles[ctx->metadata->width][1]);
  int32_t divisor = FromSignedOperand(op);
  if (divisor == 0) {
    CPURaiseInternalInterrupt(ctx->cpu, kInterruptDivideError);
    return kInstructionExecuted;
  }

  Width width = ctx->metadata->width;
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);

  OperandValue dest_high_half =
      ReadOperandValue(ctx, &kMulDivResultHighHalfAddress[width]);
  int32_t dividend =
      FromOperand(&dest) | (FromSignedOperandValue(&dest_high_half)
                            << kMulDivResultHighHalfShiftWidth[width]);
  int32_t quotient = dividend / divisor;
  // The 8086/8088 divides the magnitudes and checks the result against the
  // largest quotient that fits, so it rejects a quotient of -128 or -32768
  // even though those are representable. The valid range is symmetric, one
  // narrower at the bottom than the operand's own range.
  if (quotient > kMaxSignedValue[ctx->metadata->width] ||
      quotient < -kMaxSignedValue[ctx->metadata->width]) {
    CPURaiseInternalInterrupt(ctx->cpu, kInterruptDivideError);
    return kInstructionExecuted;
  }
  return WriteDivResult(ctx, &dest, quotient, dividend % divisor);
}

// Group 3 instruction implementations, indexed by the corresponding REG field
// value in the ModRM byte and data width.
static const Group3ExecuteInstructionFn kGroup3ExecuteInstructionFns[] = {
    ExecuteGroup3Test,  // 0 - TEST
    // REG 1 is an undocumented alias of REG 0: the 8086/8088 does not decode
    // bit 0 of the REG field for this group.
    ExecuteGroup3Test,  // 1 - TEST
    ExecuteNot,         // 2 - NOT
    ExecuteNeg,         // 3 - NEG
    ExecuteMul,         // 4 - MUL
    ExecuteImul,        // 5 - IMUL
    ExecuteDiv,         // 6 - DIV
    ExecuteIdiv,        // 7 - IDIV
};

// Group 3 instruction handler.
YAX86_PRIVATE InstructionResult
ExecuteGroup3Instruction(const InstructionContext* ctx) {
  const Group3ExecuteInstructionFn fn =
      kGroup3ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  return fn(ctx, &dest);
}


// ==============================================================================
// src/cpu/instructions_group_3.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_group_4.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_group_4.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 4 - INC, DEC
// ============================================================================

typedef InstructionResult (*Group4ExecuteInstructionFn)(
    const InstructionContext* ctx, Operand* dest);

// Group 4 instruction implementations, indexed by the corresponding REG field
// value in the ModRM byte.
static const Group4ExecuteInstructionFn kGroup4ExecuteInstructionFns[] = {
    ExecuteInc,  // 0 - INC
    ExecuteDec,  // 1 - DEC
};

enum {
  // Number of documented REG field values for this group.
  kNumGroup4Instructions = 2,
};

// Group 4 instruction handler.
YAX86_PRIVATE InstructionResult
ExecuteGroup4Instruction(const InstructionContext* ctx) {
  // On real hardware REG 2-7 decode as byte-operand forms of the Group 5
  // instructions rather than being rejected. That behavior is deliberately not
  // emulated: it is unverifiable without hardware, and no assembler emits
  // these encodings. The bounds check matters regardless, because the REG
  // field is three bits wide and this table has only two entries.
  if (ctx->instruction->mod_rm.reg >= kNumGroup4Instructions) {
    return kInstructionInvalid;
  }
  const Group4ExecuteInstructionFn fn =
      kGroup4ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  return fn(ctx, &dest);
}


// ==============================================================================
// src/cpu/instructions_group_4.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_group_5.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_group_5.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 5 - INC, DEC, CALL, JMP, PUSH
// ============================================================================

// Helper to get the segment register value for far JMP and CALL instructions.
static Operand GetSegmentRegisterOperandForIndirectFarJumpOrCall(
    const InstructionContext* ctx, const Operand* offset) {
  OperandAddress segment_address = offset->address;
  segment_address.value.memory_address.offset += 2;  // Skip the offset
  OperandValue segment_value =
      ReadMemoryOperandWord(ctx->cpu, &segment_address);
  Operand operand = {
      .address = segment_address,
      .value = segment_value,
  };
  return operand;
}

// JMP ptr16
static InstructionResult ExecuteIndirectNearJump(
    const InstructionContext* ctx, Operand* dest) {
  ctx->cpu->registers[kIP] = FromOperandValue(&dest->value);
  return kInstructionExecuted;
}

// CALL ptr16
static InstructionResult ExecuteIndirectNearCall(
    const InstructionContext* ctx, Operand* dest) {
  PushValue(ctx->cpu, WordValue(ctx->cpu->registers[kIP]));
  return ExecuteIndirectNearJump(ctx, dest);
}

// CALL ptr16:16
static InstructionResult ExecuteIndirectFarCall(
    const InstructionContext* ctx, Operand* dest) {
  Operand segment =
      GetSegmentRegisterOperandForIndirectFarJumpOrCall(ctx, dest);
  return ExecuteFarCall(ctx, &segment.value, &dest->value);
}

// JMP ptr16:16
static InstructionResult ExecuteIndirectFarJump(
    const InstructionContext* ctx, Operand* dest) {
  Operand segment =
      GetSegmentRegisterOperandForIndirectFarJumpOrCall(ctx, dest);
  return ExecuteFarJump(ctx, &segment.value, &dest->value);
}

// PUSH r/m16
static InstructionResult ExecuteIndirectPush(
    const InstructionContext* ctx, Operand* dest) {
  PushSourceOperand(ctx->cpu, dest);
  return kInstructionExecuted;
}

typedef InstructionResult (*Group5ExecuteInstructionFn)(
    const InstructionContext* ctx, Operand* dest);

// Group 5 instruction implementations, indexed by the corresponding REG
// field value in the ModRM byte.
static const Group5ExecuteInstructionFn kGroup5ExecuteInstructionFns[] = {
    ExecuteInc,               // 0 - INC r/m8/r/m16
    ExecuteDec,               // 1 - DEC r/m8/r/m16
    ExecuteIndirectNearCall,  // 2 - CALL rel16
    ExecuteIndirectFarCall,   // 3 - CALL ptr16:16
    ExecuteIndirectNearJump,  // 4 - JMP ptr16
    ExecuteIndirectFarJump,   // 5 - JMP ptr16:16
    ExecuteIndirectPush,      // 6 - PUSH r/m16
    // REG 7 is an undocumented alias of REG 6. Without this entry the lookup
    // below would read past the end of the table, since the REG field is three
    // bits wide.
    ExecuteIndirectPush,  // 7 - PUSH r/m16
};

// Group 5 instruction handler.
YAX86_PRIVATE InstructionResult
ExecuteGroup5Instruction(const InstructionContext* ctx) {
  const Group5ExecuteInstructionFn fn =
      kGroup5ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  return fn(ctx, &dest);
}


// ==============================================================================
// src/cpu/instructions_group_5.c end
// ==============================================================================

// ==============================================================================
// src/cpu/opcode_table.c start
// ==============================================================================

#line 1 "./src/cpu/opcode_table.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Opcode table
// ============================================================================

// Global opcode metadata lookup table.
//
// Const because nothing writes it, which on a target that executes from flash
// keeps 2KB of it out of RAM.
YAX86_PRIVATE const OpcodeMetadata opcode_table[256] = {
    // ADD r/m8, r8
    {.opcode = 0x00,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteAddRegisterToRegisterOrMemory},
    // ADD r/m16, r16
    {.opcode = 0x01,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteAddRegisterToRegisterOrMemory},
    // ADD r8, r/m8
    {.opcode = 0x02,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteAddRegisterOrMemoryToRegister},
    // ADD r16, r/m16
    {.opcode = 0x03,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteAddRegisterOrMemoryToRegister},
    // ADD AL, imm8
    {.opcode = 0x04,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteAddImmediateToALOrAX},
    // ADD AX, imm16
    {.opcode = 0x05,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteAddImmediateToALOrAX},
    // PUSH ES
    {.opcode = 0x06,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushSegmentRegister},
    // POP ES
    {.opcode = 0x07,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopSegmentRegister},
    // OR r/m8, r8
    {.opcode = 0x08,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteBooleanOrRegisterToRegisterOrMemory},
    // OR r/m16, r16
    {.opcode = 0x09,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteBooleanOrRegisterToRegisterOrMemory},
    // OR r8, r/m8
    {.opcode = 0x0A,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteBooleanOrRegisterOrMemoryToRegister},
    // OR r16, r/m16
    {.opcode = 0x0B,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteBooleanOrRegisterOrMemoryToRegister},
    // OR AL, imm8
    {.opcode = 0x0C,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteBooleanOrImmediateToALOrAX},
    // OR AX, imm16
    {.opcode = 0x0D,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteBooleanOrImmediateToALOrAX},
    // PUSH CS
    {.opcode = 0x0E,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushSegmentRegister},
    // POP CS
    //
    // Undocumented, but real: on the 8086/8088 the segment register field of a
    // POP sreg is only two bits wide, so 0x0F decodes as POP CS. Later x86
    // parts repurposed 0x0F as the two-byte opcode prefix.
    {.opcode = 0x0F,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopSegmentRegister},
    // ADC r/m8, r8
    {.opcode = 0x10,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteAddRegisterToRegisterOrMemoryWithCarry},
    // ADC r/m16, r16
    {.opcode = 0x11,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteAddRegisterToRegisterOrMemoryWithCarry},
    // ADC r8, r/m8
    {.opcode = 0x12,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteAddRegisterOrMemoryToRegisterWithCarry},
    // ADC r16, r/m16
    {.opcode = 0x13,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteAddRegisterOrMemoryToRegisterWithCarry},
    // ADC AL, imm8
    {.opcode = 0x14,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteAddImmediateToALOrAXWithCarry},
    // ADC AX, imm16
    {.opcode = 0x15,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteAddImmediateToALOrAXWithCarry},
    // PUSH SS
    {.opcode = 0x16,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushSegmentRegister},
    // POP SS
    {.opcode = 0x17,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopSegmentRegister},
    // SBB r/m8, r8
    {.opcode = 0x18,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteSubRegisterFromRegisterOrMemoryWithBorrow},
    // SBB r/m16, r16
    {.opcode = 0x19,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteSubRegisterFromRegisterOrMemoryWithBorrow},
    // SBB r8, r/m8
    {.opcode = 0x1A,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteSubRegisterOrMemoryFromRegisterWithBorrow},
    // SBB r16, r/m16
    {.opcode = 0x1B,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteSubRegisterOrMemoryFromRegisterWithBorrow},
    // SBB AL, imm8
    {.opcode = 0x1C,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteSubImmediateFromALOrAXWithBorrow},
    // SBB AX, imm16
    {.opcode = 0x1D,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteSubImmediateFromALOrAXWithBorrow},
    // PUSH DS
    {.opcode = 0x1E,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushSegmentRegister},
    // POP DS
    {.opcode = 0x1F,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopSegmentRegister},
    // AND r/m8, r8
    {.opcode = 0x20,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteBooleanAndRegisterToRegisterOrMemory},
    // AND r/m16, r16
    {.opcode = 0x21,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteBooleanAndRegisterToRegisterOrMemory},
    // AND r8, r/m8
    {.opcode = 0x22,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteBooleanAndRegisterOrMemoryToRegister},
    // AND r16, r/m16
    {.opcode = 0x23,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteBooleanAndRegisterOrMemoryToRegister},
    // AND AL, imm8
    {.opcode = 0x24,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteBooleanAndImmediateToALOrAX},
    // AND AX, imm16
    {.opcode = 0x25,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteBooleanAndImmediateToALOrAX},
    // ES prefix - 0x26
    {.opcode = 0x26, .handler = ExecuteInvalidOpcode},
    // DAA
    {.opcode = 0x27,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteDaa},
    // SUB r/m8, r8
    {.opcode = 0x28,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteSubRegisterFromRegisterOrMemory},
    // SUB r/m16, r16
    {.opcode = 0x29,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteSubRegisterFromRegisterOrMemory},
    // SUB r8, r/m8
    {.opcode = 0x2A,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteSubRegisterOrMemoryFromRegister},
    // SUB r16, r/m16
    {.opcode = 0x2B,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteSubRegisterOrMemoryFromRegister},
    // SUB AL, imm8
    {.opcode = 0x2C,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteSubImmediateFromALOrAX},
    // SUB AX, imm16
    {.opcode = 0x2D,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteSubImmediateFromALOrAX},
    // CS prefix - 0x2E
    {.opcode = 0x2E, .handler = ExecuteInvalidOpcode},
    // DAS
    {.opcode = 0x2F,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteDas},
    // XOR r/m8, r8
    {.opcode = 0x30,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteBooleanXorRegisterToRegisterOrMemory},
    // XOR r/m16, r16
    {.opcode = 0x31,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteBooleanXorRegisterToRegisterOrMemory},
    // XOR r8, r/m8
    {.opcode = 0x32,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteBooleanXorRegisterOrMemoryToRegister},
    // XOR r16, r/m16
    {.opcode = 0x33,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteBooleanXorRegisterOrMemoryToRegister},
    // XOR AL, imm8
    {.opcode = 0x34,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteBooleanXorImmediateToALOrAX},
    // XOR AX, imm16
    {.opcode = 0x35,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteBooleanXorImmediateToALOrAX},
    // SS prefix - 0x36
    {.opcode = 0x36, .handler = ExecuteInvalidOpcode},
    // AAA
    {.opcode = 0x37,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteAaa},
    // CMP r/m8, r8
    {.opcode = 0x38,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteCmpRegisterToRegisterOrMemory},
    // CMP r/m16, r16
    {.opcode = 0x39,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteCmpRegisterToRegisterOrMemory},
    // CMP r8, r/m8
    {.opcode = 0x3A,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteCmpRegisterOrMemoryToRegister},
    // CMP r16, r/m16
    {.opcode = 0x3B,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteCmpRegisterOrMemoryToRegister},
    // CMP AL, imm8
    {.opcode = 0x3C,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteCmpImmediateToALOrAX},
    // CMP AX, imm16
    {.opcode = 0x3D,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteCmpImmediateToALOrAX},
    // DS prefix - 0x3E
    {.opcode = 0x3E, .handler = ExecuteInvalidOpcode},
    // AAS
    {.opcode = 0x3F,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteAas},
    // INC AX
    {.opcode = 0x40,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteIncRegister},
    // INC CX
    {.opcode = 0x41,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteIncRegister},
    // INC DX
    {.opcode = 0x42,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteIncRegister},
    // INC BX
    {.opcode = 0x43,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteIncRegister},
    // INC SP
    {.opcode = 0x44,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteIncRegister},
    // INC BP
    {.opcode = 0x45,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteIncRegister},
    // INC SI
    {.opcode = 0x46,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteIncRegister},
    // INC DI
    {.opcode = 0x47,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteIncRegister},
    // DEC AX
    {.opcode = 0x48,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteDecRegister},
    // DEC CX
    {.opcode = 0x49,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteDecRegister},
    // DEC DX
    {.opcode = 0x4A,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteDecRegister},
    // DEC BX
    {.opcode = 0x4B,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteDecRegister},
    // DEC SP
    {.opcode = 0x4C,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteDecRegister},
    // DEC BP
    {.opcode = 0x4D,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteDecRegister},
    // DEC SI
    {.opcode = 0x4E,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteDecRegister},
    // DEC DI
    {.opcode = 0x4F,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteDecRegister},
    // PUSH AX
    {.opcode = 0x50,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushRegister},
    // PUSH CX
    {.opcode = 0x51,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushRegister},
    // PUSH DX
    {.opcode = 0x52,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushRegister},
    // PUSH BX
    {.opcode = 0x53,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushRegister},
    // PUSH SP
    {.opcode = 0x54,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushRegister},
    // PUSH BP
    {.opcode = 0x55,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushRegister},
    // PUSH SI
    {.opcode = 0x56,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushRegister},
    // PUSH DI
    {.opcode = 0x57,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushRegister},
    // POP AX
    {.opcode = 0x58,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopRegister},
    // POP CX
    {.opcode = 0x59,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopRegister},
    // POP DX
    {.opcode = 0x5A,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopRegister},
    // POP BX
    {.opcode = 0x5B,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopRegister},
    // POP SP
    {.opcode = 0x5C,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopRegister},
    // POP BP
    {.opcode = 0x5D,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopRegister},
    // POP SI
    {.opcode = 0x5E,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopRegister},
    // POP DI
    {.opcode = 0x5F,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopRegister},
    // 0x60 - 0x6F - UNSUPPORTED
    // 0x60-0x6F - conditional jumps, aliases of 0x70-0x7F
    //
    // Undocumented, but real: the 8086/8088 decoder ignores bit 4 of these
    // opcodes, so each one behaves exactly like its 0x7X counterpart. They are
    // two bytes wide, so decoding them as unknown single-byte opcodes would
    // desynchronize the instruction stream.
    // JO rel8 (alias of 0x70)
    {.opcode = 0x60,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JNO rel8 (alias of 0x71)
    {.opcode = 0x61,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JB/JNAE/JC rel8 (alias of 0x72)
    {.opcode = 0x62,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JNB/JAE/JNC rel8 (alias of 0x73)
    {.opcode = 0x63,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JE/JZ rel8 (alias of 0x74)
    {.opcode = 0x64,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JNE/JNZ rel8 (alias of 0x75)
    {.opcode = 0x65,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JBE/JNA rel8 (alias of 0x76)
    {.opcode = 0x66,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JNBE/JA rel8 (alias of 0x77)
    {.opcode = 0x67,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JS rel8 (alias of 0x78)
    {.opcode = 0x68,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JNS rel8 (alias of 0x79)
    {.opcode = 0x69,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JP/JPE rel8 (alias of 0x7A)
    {.opcode = 0x6A,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JNP/JPO rel8 (alias of 0x7B)
    {.opcode = 0x6B,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JL/JNGE rel8 (alias of 0x7C)
    {.opcode = 0x6C,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteSignedConditionalJumpJLOrJNL},
    // JNL/JGE rel8 (alias of 0x7D)
    {.opcode = 0x6D,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteSignedConditionalJumpJLOrJNL},
    // JLE/JNG rel8 (alias of 0x7E)
    {.opcode = 0x6E,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteSignedConditionalJumpJLEOrJNLE},
    // JNLE/JG rel8 (alias of 0x7F)
    {.opcode = 0x6F,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteSignedConditionalJumpJLEOrJNLE},
    // JO rel8
    {.opcode = 0x70,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JNO rel8
    {.opcode = 0x71,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JB/JNAE/JC rel8
    {.opcode = 0x72,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JNB/JAE/JNC rel8
    {.opcode = 0x73,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JE/JZ rel8
    {.opcode = 0x74,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JNE/JNZ rel8
    {.opcode = 0x75,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JBE/JNA rel8
    {.opcode = 0x76,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JNBE/JA rel8
    {.opcode = 0x77,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JS rel8
    {.opcode = 0x78,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JNS rel8
    {.opcode = 0x79,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JP/JPE rel8
    {.opcode = 0x7A,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JNP/JPO rel8
    {.opcode = 0x7B,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JL/JNGE rel8
    {.opcode = 0x7C,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteSignedConditionalJumpJLOrJNL},
    // JNL/JGE rel8
    {.opcode = 0x7D,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteSignedConditionalJumpJLOrJNL},
    // JLE/JNG rel8
    {.opcode = 0x7E,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteSignedConditionalJumpJLEOrJNLE},
    // JNLE/JG rel8
    {.opcode = 0x7F,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteSignedConditionalJumpJLEOrJNLE},
    // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m8, imm8 (Group 1)
    {.opcode = 0x80,
     .has_modrm = true,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteGroup1Instruction},
    // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m16, imm16 (Group 1)
    {.opcode = 0x81,
     .has_modrm = true,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteGroup1Instruction},
    // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m8, imm8 (Group 1)
    {.opcode = 0x82,
     .has_modrm = true,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteGroup1Instruction},
    // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m16, imm8 (Group 1)
    {.opcode = 0x83,
     .has_modrm = true,
     // This is a special case - the immediate is 8 bits but the destination is
     // 16 bits.
     .immediate_size = 1,
     .width = kWord,
     .handler = ExecuteGroup1InstructionWithSignExtension},
    // TEST r/m8, r8
    {.opcode = 0x84,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteTestRegisterToRegisterOrMemory},
    // TEST r/m16, r16
    {.opcode = 0x85,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteTestRegisterToRegisterOrMemory},
    // XCHG r/m8, r8
    {.opcode = 0x86,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteExchangeRegisterOrMemory},
    // XCHG r/m16, r16
    {.opcode = 0x87,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteExchangeRegisterOrMemory},
    // MOV r/m8, r8
    {.opcode = 0x88,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteMoveRegisterToRegisterOrMemory},
    // MOV r/m16, r16
    {.opcode = 0x89,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteMoveRegisterToRegisterOrMemory},
    // MOV r8, r/m8
    {.opcode = 0x8A,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteMoveRegisterOrMemoryToRegister},
    // MOV r16, r/m16
    {.opcode = 0x8B,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteMoveRegisterOrMemoryToRegister},
    // MOV r/m16, sreg
    {.opcode = 0x8C,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteMoveSegmentRegisterToRegisterOrMemory},
    // LEA r16, m
    {.opcode = 0x8D,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteLoadEffectiveAddress},
    // MOV sreg, r/m16
    {.opcode = 0x8E,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteMoveRegisterOrMemoryToSegmentRegister},
    // POP r/m16
    {.opcode = 0x8F,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopRegisterOrMemory},
    // XCHG AX, AX (NOP)
    {.opcode = 0x90,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteExchangeRegister},
    // XCHG AX, CX
    {.opcode = 0x91,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteExchangeRegister},
    // XCHG AX, DX
    {.opcode = 0x92,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteExchangeRegister},
    // XCHG AX, BX
    {.opcode = 0x93,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteExchangeRegister},
    // XCHG AX, SP
    {.opcode = 0x94,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteExchangeRegister},
    // XCHG AX, BP
    {.opcode = 0x95,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteExchangeRegister},
    // XCHG AX, SI
    {.opcode = 0x96,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteExchangeRegister},
    // XCHG AX, DI
    {.opcode = 0x97,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteExchangeRegister},
    // CBW
    {.opcode = 0x98,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteCbw},
    // CWD
    {.opcode = 0x99,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteCwd},
    // CALL ptr16:16 (4 bytes: 2 for offset, 2 for segment)
    {.opcode = 0x9A,
     .has_modrm = false,
     .immediate_size = 4,
     .width = kWord,
     .handler = ExecuteDirectFarCall},
    // WAIT
    {.opcode = 0x9B,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteNoOp},
    // PUSHF
    {.opcode = 0x9C,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushFlags},
    // POPF
    {.opcode = 0x9D,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopFlags},
    // SAHF
    {.opcode = 0x9E,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteStoreAHToFlags},
    // LAHF
    {.opcode = 0x9F,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteLoadAHFromFlags},
    // MOV AL, moffs16
    {.opcode = 0xA0,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kByte,
     .handler = ExecuteMoveMemoryOffsetToALOrAX},
    // MOV AX, moffs16
    {.opcode = 0xA1,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveMemoryOffsetToALOrAX},
    // MOV moffs16, AL
    {.opcode = 0xA2,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kByte,
     .handler = ExecuteMoveALOrAXToMemoryOffset},
    // MOV moffs16, AX
    {.opcode = 0xA3,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveALOrAXToMemoryOffset},
    // MOVSB
    {.opcode = 0xA4,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteMovs},
    // MOVSW
    {.opcode = 0xA5,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteMovs},
    // CMPSB
    {.opcode = 0xA6,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteCmps},
    // CMPSW
    {.opcode = 0xA7,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteCmps},
    // TEST AL, imm8
    {.opcode = 0xA8,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteTestImmediateToALOrAX},
    // TEST AX, imm16
    {.opcode = 0xA9,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteTestImmediateToALOrAX},
    // STOSB
    {.opcode = 0xAA,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteStos},
    // STOSW
    {.opcode = 0xAB,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteStos},
    // LODSB
    {.opcode = 0xAC,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteLods},
    // LODSW
    {.opcode = 0xAD,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteLods},
    // SCASB
    {.opcode = 0xAE,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteScas},
    // SCASW
    {.opcode = 0xAF,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteScas},
    // MOV AL, imm8
    {.opcode = 0xB0,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV CL, imm8
    {.opcode = 0xB1,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV DL, imm8
    {.opcode = 0xB2,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV BL, imm8
    {.opcode = 0xB3,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV AH, imm8
    {.opcode = 0xB4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV CH, imm8
    {.opcode = 0xB5,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV DH, imm8
    {.opcode = 0xB6,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV BH, imm8
    {.opcode = 0xB7,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV AX, imm16
    {.opcode = 0xB8,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV CX, imm16
    {.opcode = 0xB9,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV DX, imm16
    {.opcode = 0xBA,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV BX, imm16
    {.opcode = 0xBB,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV SP, imm16
    {.opcode = 0xBC,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV BP, imm16
    {.opcode = 0xBD,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV SI, imm16
    {.opcode = 0xBE,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV DI, imm16
    {.opcode = 0xBF,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveImmediateToRegister},
    // RET imm16 (alias of 0xC2)
    //
    // Undocumented, but real: the 8086/8088 decoder ignores bit 1 of these
    // opcodes. 0xC0 takes a 16-bit immediate, so decoding it as an unknown
    // single-byte opcode would desynchronize the instruction stream.
    {.opcode = 0xC0,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteNearReturnAndPop},
    // RET (alias of 0xC3)
    {.opcode = 0xC1,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteNearReturn},
    // RET imm16
    {.opcode = 0xC2,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteNearReturnAndPop},
    // RET
    {.opcode = 0xC3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteNearReturn},
    // LES r16, m32
    {.opcode = 0xC4,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteLoadESWithPointer},
    // LDS r16, m32
    {.opcode = 0xC5,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteLoadDSWithPointer},
    // MOV r/m8, imm8
    {.opcode = 0xC6,
     .has_modrm = true,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteMoveImmediateToRegisterOrMemory},
    // MOV r/m16, imm16
    {.opcode = 0xC7,
     .has_modrm = true,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveImmediateToRegisterOrMemory},
    // RETF imm16 (alias of 0xCA)
    {.opcode = 0xC8,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteFarReturnAndPop},
    // RETF (alias of 0xCB)
    {.opcode = 0xC9,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteFarReturn},
    // RETF imm16
    {.opcode = 0xCA,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteFarReturnAndPop},
    // RETF
    {.opcode = 0xCB,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteFarReturn},
    // INT 3
    {.opcode = 0xCC,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteInt3},
    // INT imm8
    {.opcode = 0xCD,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteIntN},
    // INTO
    {.opcode = 0xCE,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteInto},
    // IRET
    {.opcode = 0xCF,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteIret},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m8, 1 (Group 2)
    {.opcode = 0xD0,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteGroup2ShiftOrRotateBy1Instruction},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m16, 1 (Group 2)
    {.opcode = 0xD1,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteGroup2ShiftOrRotateBy1Instruction},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m8, CL (Group 2)
    {.opcode = 0xD2,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteGroup2ShiftOrRotateByCLInstruction},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m16, CL (Group 2)
    {.opcode = 0xD3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteGroup2ShiftOrRotateByCLInstruction},
    // AAM
    {.opcode = 0xD4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteAam},
    // AAD
    {.opcode = 0xD5,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteAad},
    // SALC - Set AL from Carry
    //
    // Undocumented, but real and stable across x86 generations: sets AL to
    // 0xFF if CF is set and 0x00 otherwise. Affects no flags.
    {.opcode = 0xD6,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteSetALFromCarry},
    // XLAT/XLATB
    {.opcode = 0xD7,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteTranslateByte},
    // ESC instruction 0xD8 for 8087 numeric coprocessor
    {.opcode = 0xD8,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = ExecuteNoOp},
    // ESC instruction 0xD9 for 8087 numeric coprocessor
    {.opcode = 0xD9,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = ExecuteNoOp},
    // ESC instruction 0xDA for 8087 numeric coprocessor
    {.opcode = 0xDA,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = ExecuteNoOp},
    // ESC instruction 0xDB for 8087 numeric coprocessor
    {.opcode = 0xDB,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = ExecuteNoOp},
    // ESC instruction 0xDC for 8087 numeric coprocessor
    {.opcode = 0xDC,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = ExecuteNoOp},
    // ESC instruction 0xDD for 8087 numeric coprocessor
    {.opcode = 0xDD,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = ExecuteNoOp},
    // ESC instruction 0xDE for 8087 numeric coprocessor
    {.opcode = 0xDE,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = ExecuteNoOp},
    // ESC instruction 0xDF for 8087 numeric coprocessor
    {.opcode = 0xDF,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = ExecuteNoOp},
    // LOOPNE/LOOPNZ rel8
    {.opcode = 0xE0,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteLoopZOrNZ},
    // LOOPE/LOOPZ rel8
    {.opcode = 0xE1,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteLoopZOrNZ},
    // LOOP rel8
    {.opcode = 0xE2,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteLoop},
    // JCXZ rel8
    {.opcode = 0xE3,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteJumpIfCXIsZero},
    // IN AL, imm8
    {.opcode = 0xE4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteInImmediate},
    // IN AX, imm8
    {.opcode = 0xE5,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kWord,
     .handler = ExecuteInImmediate},
    // OUT imm8, AL
    {.opcode = 0xE6,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteOutImmediate},
    // OUT imm8, AX
    {.opcode = 0xE7,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kWord,
     .handler = ExecuteOutImmediate},
    // CALL rel16
    {.opcode = 0xE8,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteDirectNearCall},
    // JMP rel16
    {.opcode = 0xE9,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteShortOrNearJump},
    // JMP ptr16:16 (4 bytes: 2 for offset, 2 for segment)
    {.opcode = 0xEA,
     .has_modrm = false,
     .immediate_size = 4,
     .width = kWord,
     .handler = ExecuteDirectFarJump},
    // JMP rel8
    {.opcode = 0xEB,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteShortOrNearJump},
    // IN AL, DX
    {.opcode = 0xEC,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteInDX},
    // IN AX, DX
    {.opcode = 0xED,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteInDX},
    // OUT DX, AL
    {.opcode = 0xEE,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteOutDX},
    // OUT DX, AX
    {.opcode = 0xEF,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteOutDX},
    // 0xF0 - LOCK prefix
    {.opcode = 0xF0, .handler = ExecuteInvalidOpcode},
    // LOCK prefix (alias of 0xF0) - consumed by the prefix decoder.
    {.opcode = 0xF1, .handler = ExecuteInvalidOpcode},
    // 0xF2 - REPNE prefix
    {.opcode = 0xF2, .handler = ExecuteInvalidOpcode},
    // 0xF3 - REP/REPE prefix
    {.opcode = 0xF3, .handler = ExecuteInvalidOpcode},
    // HLT
    {.opcode = 0xF4,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteHlt},
    // CMC
    {.opcode = 0xF5,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteComplementCarryFlag},
    // TEST/NOT/NEG/MUL/IMUL/DIV/IDIV r/m8 (Group 3)
    // The immediate size depends on the ModR/M byte.
    {.opcode = 0xF6,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteGroup3Instruction},
    // TEST/NOT/NEG/MUL/IMUL/DIV/IDIV r/m16 (Group 3)
    // The immediate size depends on the ModR/M byte.
    {.opcode = 0xF7,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteGroup3Instruction},
    // CLC
    {.opcode = 0xF8,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteClearOrSetFlag},
    // STC
    {.opcode = 0xF9,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteClearOrSetFlag},
    // CLI
    {.opcode = 0xFA,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteClearOrSetFlag},
    // STI
    {.opcode = 0xFB,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteClearOrSetFlag},
    // CLD
    {.opcode = 0xFC,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteClearOrSetFlag},
    // STD
    {.opcode = 0xFD,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteClearOrSetFlag},
    // INC/DEC r/m8 (Group 4)
    {.opcode = 0xFE,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteGroup4Instruction},
    // INC/DEC/CALL/JMP/PUSH r/m16 (Group 5)
    {.opcode = 0xFF,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteGroup5Instruction},
};


// ==============================================================================
// src/cpu/opcode_table.c end
// ==============================================================================

// ==============================================================================
// src/cpu/cpu.c start
// ==============================================================================

#line 1 "./src/cpu/cpu.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "cycles.h"
#include "instructions.h"
#include "operands.h"
#include "public.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

#define YAX86_CPU_LOG(level, ...) \
  YAX86_LOG(cpu->config->logger, &kLogModuleCPU, level, __VA_ARGS__)

// ============================================================================
// CPU state
// ============================================================================

YAX86_HOT void CPUInit(CPUState* cpu, CPUConfig* config) {
  // Zero out the CPU state
  const CPUState zero_cpu_state = {0};
  *cpu = zero_cpu_state;
  cpu->flags = kInitialFlags;
  cpu->config = config;
}

// ============================================================================
// Instruction decoding
// ============================================================================

// The two prefix groups are each a contiguous encoding family, so a masked
// compare identifies a whole group and the bits the mask leaves free say which
// member it is.
enum {
  // Segment overrides encode as 001ss110, where ss selects the segment.
  kSegmentOverridePrefixMask = 0xE7,
  kSegmentOverridePrefixValue = 0x26,
  // Position of the ss field within a segment override prefix.
  kSegmentOverridePrefixShift = 3,
  kSegmentOverridePrefixSegmentMask = 0x03,

  // Within the LOCK and repetition prefix group, bit 1 separates the
  // repetition prefixes (REPNZ, REP) from LOCK and its undocumented 0xF1
  // alias.
  kRepetitionPrefixBit = 0x02,
};

// Whether a byte is a segment override prefix. The mask pins every bit but the
// two that select the segment, so it matches those four bytes and nothing else.
static inline bool IsSegmentOverridePrefix(uint8_t byte) {
  return (byte & kSegmentOverridePrefixMask) == kSegmentOverridePrefixValue;
}

// Whether a byte is a LOCK or repetition prefix. These four are consecutive,
// so this is a range check rather than a mask - which is both clearer and one
// instruction cheaper, since a compiler folds it into a single subtract and
// compare.
static inline bool IsLockOrRepetitionPrefix(uint8_t byte) {
  return byte >= kPrefixLOCK && byte <= kPrefixREP;
}

// Record what a prefix byte selects, and report whether it was a prefix at
// all. Deciding which group a byte belongs to is the same test as deciding
// whether it is a prefix, so both happen here rather than the caller asking
// first and this asking again.
//
// The cheaper test goes first. A byte that is not a prefix runs both, and that
// is the common case by far - every instruction ends the loop with one.
static bool ApplyPrefixByte(Instruction* instruction, uint8_t byte) {
  if (IsLockOrRepetitionPrefix(byte)) {
    // LOCK and its 0xF1 alias advance IP, but nothing acts on them, so only a
    // repetition prefix is worth recording.
    if (byte & kRepetitionPrefixBit) {
      instruction->repetition_prefix = byte;
    }
    return true;
  }
  if (IsSegmentOverridePrefix(byte)) {
    // The segment field is in the 8086's sreg encoding order, which is the
    // order kES through kDS are numbered in, so the register index is an
    // offset from kES.
    instruction->segment_override =
        (uint8_t)(kES + ((byte >> kSegmentOverridePrefixShift) &
                         kSegmentOverridePrefixSegmentMask));
    return true;
  }
  return false;
}

enum {
  // How many bytes a segment addresses. IP is 16 bits and wraps within the
  // segment, so this is also where a fetch through a window has to stop.
  kSegmentSize = 0x10000,
};

// Where one call to CPUFetchNextInstruction() has got to.
//
// Scratch for a single decode, and nothing more. What survives between
// instructions is the window itself, in CPUState.instruction_fetch_window;
// this is the cursor walking it, and it is thrown away when the instruction
// has been decoded.
//
// It is a struct because CPUFetchNextInstructionByte() has to advance the
// cursor and is called from five places in the decode, so the cursor has to be
// passed by pointer. Kept as three loose locals it would have to be passed as
// three out-parameters, and taking the address of each is what stops a
// compiler keeping them in registers - which is the whole point of the
// arrangement, since this is the hottest loop in the emulator.
//
// The two byte positions are the same position expressed twice, deliberately:
//
//   - next_byte_offset is where the cursor is in the segment, and is what
//     yields the instruction's size and what the byte-at-a-time path needs. It
//     is not the CPU's IP register, which has to keep naming the instruction
//     being decoded until CPUTick() advances it by instruction.size - so a
//     failed decode leaves IP alone, and the control flow instructions can add
//     their displacement to the address of the instruction after this one.
//   - next_byte is where the cursor is in the host's memory, and is cached
//     rather than derived because deriving it costs a shift, an add, a
//     subtract and an add per byte where advancing it costs an increment.
//
// bytes_remaining bounds the direct reads. Where the host hands out no window
// it is zero, and every byte takes the ordinary path through read_memory_byte
// - an indirect call per byte, two to six times per instruction, which is what
// the window exists to avoid.
typedef struct CPUInstructionFetchState {
  // The offset within CS of the next byte to read.
  uint16_t next_byte_offset;
  // The next byte to read, and how many may be read directly from it. NULL and
  // zero where there is no window.
  const uint8_t* next_byte;
  uint32_t bytes_remaining;
} CPUInstructionFetchState;

// Reads the next instruction byte, from the window where one is open and from
// the memory map where one is not.
//
// Either way the read goes straight to memory rather than through
// ReadMemoryOperandByte, so that the fetch is not charged for time on the data
// bus. The 8088 fetches ahead into a queue while the previous instruction
// executes, so most of that time is already paid for by the instruction being
// executed - and the published per-instruction figures the cycle table is
// built from assume the queue is full.
YAX86_HOT static inline uint8_t CPUFetchNextInstructionByte(
    CPUState* cpu, CPUInstructionFetchState* fetch_state) {
  if (fetch_state->bytes_remaining > 0) {
    --fetch_state->bytes_remaining;
    ++fetch_state->next_byte_offset;
    return *fetch_state->next_byte++;
  }
  const MemoryAddress address = {
      .segment_register_index = kCS,
      .offset = fetch_state->next_byte_offset++,
  };
  return ReadRawMemoryByte(cpu, ToRawAddress(cpu, &address));
}

// Points a fetch at whatever can be read directly from CS:ip.
YAX86_HOT static void CPUInitInstructionFetchState(
    CPUState* cpu, uint16_t ip, CPUInstructionFetchState* fetch_state) {
  fetch_state->next_byte_offset = ip;
  fetch_state->next_byte = NULL;
  fetch_state->bytes_remaining = 0;

  // Note that there is deliberately no early out for a host that supplies no
  // get_instruction_fetch_window, even though everything below is wasted on
  // one. Such a host reads every byte through an indirect call already, so it
  // would save a handful of arithmetic against several calls - and the test to
  // skip it would be paid by the hosts that do supply one, which is every host
  // that cares about the speed. Measured on x86-64, adding it grew
  // CPUFetchNextInstruction by 11 bytes.
  const MemoryAddress fetch_address = {
      .segment_register_index = kCS,
      .offset = ip,
  };
  const uint32_t raw_address = ToRawAddress(cpu, &fetch_address);

  const CPUInstructionFetchWindow* const window =
      &cpu->instruction_fetch_window;
  if (window->data == NULL || raw_address < window->start ||
      raw_address >= window->end) {
    // Nothing open covers this address, so ask for a window that does. The
    // host fills one in, or sets its data to NULL to decline.
    if (cpu->config->get_instruction_fetch_window != NULL) {
      cpu->config->get_instruction_fetch_window(cpu, raw_address);
    } else {
      cpu->instruction_fetch_window.data = NULL;
    }
  }
  // Usually the window was already open and already covered the address, which
  // costs a compare rather than a call: a window spans a whole memory region,
  // so both straight-line execution and a jump backwards within that region
  // land inside the one already open.
  if (window->data != NULL && raw_address >= window->start &&
      raw_address < window->end) {
    fetch_state->next_byte = window->data + (raw_address - window->start);
    fetch_state->bytes_remaining = window->end - raw_address;
  }

  // IP is 16 bits and wraps within the segment where the linear address does
  // not, so the fetch has to stop where the wrap would be. Past that point the
  // ordinary path recomputes the address from the wrapped IP and gets it
  // right.
  const uint32_t bytes_until_wrap = (uint32_t)kSegmentSize - (uint32_t)ip;
  if (fetch_state->bytes_remaining > bytes_until_wrap) {
    fetch_state->bytes_remaining = bytes_until_wrap;
  }
}

// Returns the number of displacement bytes based on the ModR/M byte.
static uint8_t GetDisplacementSize(uint8_t mod, uint8_t rm) {
  switch (mod) {
    case 0:
      // Special case: 16-bit displacement
      return rm == 6 ? 2 : 0;
    case 1:
    case 2:
      // 8 or 16-bit displacement
      return mod;
    default:
      // No displacement
      return 0;
  }
}

// Returns the number of immediate bytes in an instruction.
static uint8_t GetImmediateSize(const OpcodeMetadata* metadata, uint8_t reg) {
  switch (metadata->opcode) {
    // TEST r/m8, imm8
    case 0xF6:
    // TEST r/m16, imm16
    case 0xF7:
      // REG 0 and REG 1 are both TEST, which carries an immediate; the other
      // REG values do not. The 8086/8088 does not decode bit 0 of the REG
      // field here, which is what makes REG 1 an alias of REG 0.
      return reg <= 1 ? metadata->opcode - 0xF5 : 0;
    default:
      return metadata->immediate_size;
  }
}

YAX86_HOT CPUFetchNextInstructionStatus
CPUFetchNextInstruction(CPUState* cpu, Instruction* instruction) {
  // The prefix fields, which ApplyPrefixByte() writes only where a prefix is
  // actually present. Every other field a decode could leave behind is settled
  // where it becomes known rather than here: opcode, size and immediate_size
  // are assigned on every path that returns success, has_mod_rm and
  // displacement_size in both arms of the ModR/M branch below, mod_rm is read
  // only where has_mod_rm is set, and the displacement and immediate arrays
  // are read no further than their size fields say.
  instruction->segment_override = kNoSegmentOverride;
  instruction->repetition_prefix = 0;

  uint8_t current_byte;
  const uint16_t original_ip = cpu->registers[kIP];
  CPUInstructionFetchState fetch_state;
  CPUInitInstructionFetchState(cpu, original_ip, &fetch_state);

  // Prefix
  //
  // The count is local because nothing outside this loop wants it: what the
  // prefixes selected is in the instruction, and the bytes they occupied are
  // in its size. It exists only to stop a run of prefix bytes from fetching
  // forever - see kMaxPrefixBytes.
  uint8_t prefix_size = 0;
  current_byte = CPUFetchNextInstructionByte(cpu, &fetch_state);
  while (ApplyPrefixByte(instruction, current_byte)) {
    if (++prefix_size > kMaxPrefixBytes) {
      return kFetchPrefixTooLong;
    }
    current_byte = CPUFetchNextInstructionByte(cpu, &fetch_state);
  }

  // Opcode
  instruction->opcode = current_byte;
  const OpcodeMetadata* metadata = &opcode_table[instruction->opcode];

  // ModR/M
  //
  // The REG field is kept in a local, because the immediate size below is
  // computed whether or not the instruction carries a ModR/M byte.
  uint8_t reg = 0;
  if (metadata->has_modrm) {
    uint8_t mod_rm_byte = CPUFetchNextInstructionByte(cpu, &fetch_state);
    reg = (mod_rm_byte >> 3) & 0x07;  // Bits 3-5
    instruction->has_mod_rm = true;
    instruction->mod_rm.mod = (mod_rm_byte >> 6) & 0x03;  // Bits 6-7
    instruction->mod_rm.reg = reg;
    instruction->mod_rm.rm = mod_rm_byte & 0x07;  // Bits 0-2

    // Displacement
    const uint8_t displacement_size =
        GetDisplacementSize(instruction->mod_rm.mod, instruction->mod_rm.rm);
    instruction->displacement_size = displacement_size;
    for (uint8_t i = 0; i < displacement_size; ++i) {
      instruction->displacement[i] =
          CPUFetchNextInstructionByte(cpu, &fetch_state);
    }
  } else {
    // Cleared in the arm that skips them rather than before the decode starts,
    // so that a decode writes each of them exactly once whichever arm it
    // takes. Clearing them up front costs the ModR/M arm a second write of
    // both.
    instruction->has_mod_rm = false;
    instruction->displacement_size = 0;
  }

  // Immediate operand
  //
  // immediate_size is a three bit field, so it can express more bytes than
  // immediate[] holds. No entry in the opcode table does - the widest is the 4
  // of a far pointer - but nothing in the type says so, and a compiler that
  // cannot see it is right to warn about the writes below. Bounding it by the
  // array is what makes the invariant explicit.
  uint8_t immediate_size = GetImmediateSize(metadata, reg);
  if (immediate_size > kMaxImmediateBytes) {
    immediate_size = kMaxImmediateBytes;
  }
  instruction->immediate_size = immediate_size;
  for (uint8_t i = 0; i < immediate_size; ++i) {
    instruction->immediate[i] = CPUFetchNextInstructionByte(cpu, &fetch_state);
  }

  instruction->size = (uint8_t)(fetch_state.next_byte_offset - original_ip);

  return kFetchSuccess;
}

// ============================================================================
// Execution
// ============================================================================

// Runs an instruction whose opcode table entry the caller already has, and
// which the caller has already established the entry agrees with.
//
// Kept out of line. Inlined into CPUTick() this measured 31% slower on a
// Cortex-M0+: the execute path wants registers, the core has few, and folding
// the two together makes both spill. It only shows up once the hot path is in
// SRAM - from flash the XIP cache dominates and hides it.
YAX86_HOT YAX86_NOINLINE YAX86_PRIVATE InstructionResult
CPUExecuteDecodedInstruction(
    CPUState* cpu, Instruction* instruction, const OpcodeMetadata* metadata) {
  // Run the on_before_execute_instruction callback if provided.
  if (cpu->config->on_before_execute_instruction) {
    cpu->config->on_before_execute_instruction(cpu, instruction);
  }

  // Run the instruction handler.
  InstructionContext context = {
      .cpu = cpu,
      .instruction = instruction,
      .metadata = metadata,
  };
  InstructionResult result = metadata->handler(&context);
  if (result != kInstructionExecuted) {
    return result;
  }

  // Run the on_after_execute_instruction callback if provided.
  if (cpu->config->on_after_execute_instruction) {
    cpu->config->on_after_execute_instruction(cpu, instruction);
  }

  return kInstructionExecuted;
}

// Checks an instruction against the opcode table before running it.
//
// For a caller that built the Instruction itself rather than decoding one -
// CPUTick() goes straight to CPUExecuteDecodedInstruction(), because its own
// decode is what produced the encoding these checks would be re-examining.
YAX86_HOT InstructionResult
CPUExecuteInstruction(CPUState* cpu, Instruction* instruction) {
  const OpcodeMetadata* metadata = &opcode_table[instruction->opcode];

  // Check the encoded instruction against the expected format for its opcode.
  // Nothing checks that the opcode has a handler, because every entry in the
  // table has one: the eight bytes that are prefixes rather than instructions
  // are handled by ExecuteInvalidOpcode(), which returns what a missing handler
  // used to.
  if (instruction->has_mod_rm != metadata->has_modrm) {
    return kInstructionInvalid;
  }
  if (instruction->immediate_size !=
      (metadata->has_modrm ? GetImmediateSize(metadata, instruction->mod_rm.reg)
                           : metadata->immediate_size)) {
    return kInstructionInvalid;
  }

  return CPUExecuteDecodedInstruction(cpu, instruction, metadata);
}

// Save state and vector to the handler for an interrupt.
static void DispatchInterrupt(CPUState* cpu, uint8_t interrupt_number) {
  // Prepare for interrupt processing.
  cpu->is_halted = false;
  PushValue(cpu, WordValue(cpu->flags));
  CPUSetFlag(cpu, kIF, false);
  CPUSetFlag(cpu, kTF, false);
  PushValue(cpu, WordValue(cpu->registers[kCS]));
  PushValue(cpu, WordValue(cpu->registers[kIP]));

  // Invoke the interrupt handler callback first. If the caller did not provide
  // an interrupt handler callback, handle the interrupt within the VM using the
  // Interrupt Vector Table.
  InterruptHandlerResult interrupt_handler_result =
      cpu->config->handle_interrupt
          ? cpu->config->handle_interrupt(cpu, interrupt_number)
          : kInterruptHandlerUnhandled;

  if (interrupt_handler_result == kInterruptHandlerHandled) {
    // If the interrupt was handled by the caller-provided interrupt handler
    // callback, restore state and continue execution.
    ExecuteReturnFromInterrupt(cpu);
    return;
  }

  // If the interrupt was not handled by the caller-provided interrupt handler
  // callback, handle it within the VM using the Interrupt Vector Table.
  uint16_t ivt_entry_offset = interrupt_number << 2;
  cpu->registers[kIP] = ReadRawMemoryWord(cpu, ivt_entry_offset);
  cpu->registers[kCS] = ReadRawMemoryWord(cpu, ivt_entry_offset + 2);
}

// Take a pending interrupt, if any. Returns whether one was dispatched.
static bool ExecutePendingInterrupt(CPUState* cpu) {
  // An internal interrupt goes first. It was raised by the instruction that
  // just executed, and taking it clears IF, which correctly holds off any
  // external request until the handler re-enables interrupts.
  if (cpu->has_pending_internal_interrupt) {
    const uint8_t interrupt_number = cpu->pending_internal_interrupt_number;
    CPUClearInternalInterrupt(cpu);
    DispatchInterrupt(cpu, interrupt_number);
    return true;
  }

  // An external request on the INTR pin is only taken while interrupts are
  // enabled. Acknowledging it is what produces its vector - there is nothing to
  // latch beforehand, and the controller keeps requesting until acknowledged.
  uint8_t intr_vector;
  if (CPUGetFlag(cpu, kIF) && cpu->config->acknowledge_interrupt &&
      cpu->config->acknowledge_interrupt(cpu, &intr_vector)) {
    DispatchInterrupt(cpu, intr_vector);
    return true;
  }

  return false;
}

YAX86_HOT CPUTickResult CPUTick(CPUState* cpu) {
  // A stop request only applies to the tick during which it was made.
  cpu->stop_requested = false;

  // Whether this tick ran an instruction. A halted CPU runs none until an
  // interrupt wakes it.
  bool executed_instruction = false;

  // A halted CPU still consumes time - it is sitting in a wait state, not
  // stopped - so a tick that runs no instruction still has to advance the
  // clock, or the timer that is meant to wake it would never tick either.
  cpu->pending_cycles = 0;
  cpu->cycles_this_tick = kHaltedCycles;

  // The trap flag is sampled before the instruction runs, not after. An
  // instruction that sets TF - POPF or IRET - must not trap on itself, and one
  // that clears TF still traps once for the instruction it was set during.
  const bool trap_flag_was_set = CPUGetFlag(cpu, kTF);

  // Execute next CPU instruction if not halted.
  if (!cpu->is_halted) {
    // Step 1: Fetch the next instruction, and increment IP.
    Instruction instruction;
    uint16_t instruction_cs = cpu->registers[kCS];
    uint16_t instruction_ip = cpu->registers[kIP];
    CPUFetchNextInstructionStatus fetch_status =
        CPUFetchNextInstruction(cpu, &instruction);
    if (fetch_status != kFetchSuccess) {
      YAX86_CPU_LOG(
          kLogLevelError, "%04X:%04X failed to fetch instruction, status %d",
          instruction_cs, instruction_ip, (int)fetch_status);
      return kCPUTickInvalid;
    }
    cpu->registers[kIP] += instruction.size;

    // The cost of the instruction is its base cost plus the address it had to
    // compute, and then whatever it charges itself as it runs - its traffic on
    // the data bus, and any part of its cost that depends on its operands.
    CPUAddCycles(
        cpu, kOpcodeBaseCycles[instruction.opcode] +
                 GetEffectiveAddressCycles(&instruction));

    // Step 2: Execute the instruction. The fetch above derived has_mod_rm and
    // immediate_size from this same table entry, so the checks
    // CPUExecuteInstruction() makes cannot fail here.
    const OpcodeMetadata* const metadata = &opcode_table[instruction.opcode];
    if (CPUExecuteDecodedInstruction(cpu, &instruction, metadata) !=
        kInstructionExecuted) {
      YAX86_CPU_LOG(
          kLogLevelError, "%04X:%04X invalid instruction, opcode %02X",
          instruction_cs, instruction_ip, instruction.opcode);
      return kCPUTickInvalid;
    }
    executed_instruction = true;
    ++cpu->instructions_retired;
    cpu->cycles_this_tick = cpu->pending_cycles;
  }

  // Step 3: Handle a pending interrupt. This runs even while halted, because
  // an interrupt is the only thing that can clear the halted state -
  // ExecutePendingInterrupt() resets is_halted when it dispatches one.
  const bool dispatched_interrupt = ExecutePendingInterrupt(cpu);

  // Step 4: The trap flag raises a single-step interrupt after an instruction
  // executes, so a halted CPU must not trap - otherwise the trap would wake it
  // and then fire again on every subsequent tick.
  //
  // Single-stepping is the lowest priority of the interrupt sources recognized
  // at an instruction boundary, so an interrupt dispatched above takes its
  // place rather than both firing.
  if (executed_instruction && trap_flag_was_set && !dispatched_interrupt) {
    CPURaiseInternalInterrupt(cpu, kInterruptSingleStep);
    ExecutePendingInterrupt(cpu);
  }

  // A stop requested from within a callback takes precedence over everything
  // else: the caller asked to be handed control back at this exact point.
  if (cpu->stop_requested) {
    return kCPUTickStopped;
  }
  // This reports what the tick did, not what state the CPU ended up in. A tick
  // that executes HLT ran an instruction, so it reports kCPUTickExecuted even
  // though the CPU is now halted; the ticks that follow report kCPUTickHalted.
  return executed_instruction ? kCPUTickExecuted : kCPUTickHalted;
}


// ==============================================================================
// src/cpu/cpu.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_CPU_BUNDLE_H

