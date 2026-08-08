// ==============================================================================
// YAX86 KEYBOARD MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_KEYBOARD_BUNDLE_H
#define YAX86_KEYBOARD_BUNDLE_H

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
// src/keyboard/public.h start
// ==============================================================================

#line 1 "./src/keyboard/public.h"
// Public interface for the Keyboard module.
#ifndef YAX86_KEYBOARD_PUBLIC_H
#define YAX86_KEYBOARD_PUBLIC_H

// This module emulates a PC/XT keyboard and its interface to the 8255 PPI.
//
// During initialization:
// 1. [0, 0]
//    The BIOS sets both control bits to false and holds them there for at
//    least 20ms. The keyboard detects the clock_low line is held low, and
//    performs a self test.
// 2. -> [1, 1] -> [0, 1]
//    The BIOS restores the clock_low line to true, releasing the reset signal.
//    It pulses the enable_clear line high then low to trigger the next scan
//    code, just like in normal operation.
// 3. The pulse triggers the keyboard to send the self-test OK scancode (0xAA)
//    to the PPI.
// 4. -> [1, 1] -> [0, 1]
//    The BIOS acknowledges the self-test OK scancode by pulsing the
//    enable_clear line again, just like in normal operation.
// 5. -> [1, 1]
//    The BIOS sets both control bits to true to inhibit the keyboard for the
//    rest of the POST process.
// 6. -> [0, 1]
//    At the end of POST, the BIOS enables the keyboard by setting it to normal
//    operational state.
//
// In normal operation:
// 1. [0, 1]
//    In steady state, the control bits are set to enable_clear = false,
//    clock_low = true.
// 2. [0, 1]
//    On key press, the keyboard sends the scancode to the PPI and raises IRQ1.
//    At this point, the control bits are unchanged.
// 3. -> [1, 1] -> [0, 1]
//    The BIOS's IRQ handler sends an ack by briefly pulsing the enable_clear
//    line from false to true to false. This pulse tells the keyboard that it
//    can now send the next scancode.
//
// Key presses that arrive while the keyboard is held in reset - that is, while
// the clock line is low - are dropped rather than buffered, because a keyboard
// being reset is not scanning its matrix. This matters more than it sounds: the
// BIOS follows the reset by clearing the interface and then checking that
// nothing arrives over the next few hundred milliseconds, and reports anything
// that does as a stuck key. A keystroke buffered across the reset would land
// squarely in that window. Being inhibited (enable_clear set) is a different
// state, and keys pressed then are buffered as usual.

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_KEYBOARD_BUNDLE_H
#include "../util/log.h"
#include "../util/static_vector.h"
#endif  // YAX86_KEYBOARD_BUNDLE_H

enum {
  // Log module ID for the Keyboard.
  kLogModuleIDKeyboard = 7,
};

// Log module for the Keyboard.
static const LogModule kLogModuleKeyboard = {
    .id = kLogModuleIDKeyboard,
    .name = "KEYBOARD",
};

struct KeyboardState;

// Caller-provided runtime configuration for the Keyboard.
typedef struct KeyboardConfig {
  // Opaque context pointer, passed to all callbacks.
  void* context;

  // Logger for this module. May be NULL.
  Logger* logger;

  // Callback to send a scancode to the PPI.
  void (*send_scancode)(void* context, uint8_t scancode);
  // Callback to raise an IRQ1 (keyboard interrupt) to the CPU.
  void (*raise_irq1)(void* context);
} KeyboardConfig;

enum {
  // Maximum number of keys to buffer. Additional key presses will be dropped.
  kKeyboardBufferSize = 16,
  // Threshold required to trigger keyboard reset when clock line is held low.
  kKeyboardResetThresholdMs = 20,
};
STATIC_VECTOR_TYPE(KeyboardBuffer, uint8_t, kKeyboardBufferSize)

// State of the Keyboard.
typedef struct KeyboardState {
  // Pointer to the keyboard configuration.
  KeyboardConfig* config;

  // State of PPI Port B bit 7, or PBKB in GLaBIOS.
  // - false = enable keyboard
  // - true  = clear keyboard (reset)
  bool enable_clear;

  // Current state of PPI Port B bit 6, or PBKC in GLaBIOS.
  // - false = hold keyboard clock low
  // - true  = enabled (normal operation)
  bool clock_low;

  // Number of ms since the clock_low line was set to false (enabled). This is
  // used to detect the reset signal from the BIOS, which is holding the clock
  // low for at least 20ms.
  //   - 0 = clock line is high (normal operation)
  //   - 0xFF = clock line has been low for at least 20ms
  uint8_t clock_low_ms;

  // Whether we are currently waiting for ack from BIOS before sending the next
  // scancode. The keyboard will not send any further scancodes until the BIOS
  // pulses the enable_clear line high then low.
  bool waiting_for_ack;

  // Buffer of key presses received.
  KeyboardBuffer buffer;
} KeyboardState;

// Initializes the keyboard to its power-on state.
void KeyboardInit(KeyboardState* keyboard, KeyboardConfig* config);

// Receive keyboard control bits from the PPI (bits 6 and 7 of Port B).
void KeyboardHandleControl(
    KeyboardState* keyboard, bool enable_clear, bool clock_low);

// Handles a real key press event.
void KeyboardHandleKeyPress(KeyboardState* keyboard, uint8_t scancode);

// Simulates a 1ms tick. This is needed to respond to reset commands and to
// send buffered scancodes.
void KeyboardTickMs(KeyboardState* keyboard);

#endif  // YAX86_KEYBOARD_PUBLIC_H


// ==============================================================================
// src/keyboard/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/keyboard/keyboard.c start
// ==============================================================================

#line 1 "./src/keyboard/keyboard.c"
#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

enum {
  // Value for clock_low_ms indicating that a reset has already been triggered.
  kKeyboardResetTriggered = 0xFF,
  // Scan code indicating successful self-test.
  kKeyboardSelfTestOK = 0xAA,
};

void KeyboardInit(KeyboardState* keyboard, KeyboardConfig* config) {
  static const KeyboardState zero_keyboard_state = {0};
  *keyboard = zero_keyboard_state;
  keyboard->config = config;

  // Default to keyboard enabled (enable_clear = false) with clock held low
  // (clock_low = true). This allows us to detect a falling edge on clock_low
  // which triggers the reset timer.
  keyboard->enable_clear = false;
  keyboard->clock_low = true;
  keyboard->clock_low_ms = 0;
  KeyboardBufferInit(&keyboard->buffer);
  keyboard->waiting_for_ack = false;
}

// Helper to send a scancode to the PPI and raise IRQ1 if needed.
static inline void KeyboardSendScancode(
    KeyboardState* keyboard, uint8_t scancode) {
  if (keyboard->config && keyboard->config->send_scancode) {
    keyboard->config->send_scancode(keyboard->config->context, scancode);
  }
  if (keyboard->config && keyboard->config->raise_irq1) {
    keyboard->config->raise_irq1(keyboard->config->context);
  }
  keyboard->waiting_for_ack = true;
}

// Helper to send the next scancode in the buffer if available.
static inline void KeyboardSendNextScancode(KeyboardState* keyboard) {
  // Can only send in state [0, 1], i.e. enable_clear = false clock_low = true
  if (!(keyboard->enable_clear == false && keyboard->clock_low == true)) {
    return;
  }
  // Can only send after previous scancode has been acked.
  if (keyboard->waiting_for_ack) {
    return;
  }
  // If buffer is empty, nothing to send.
  if (KeyboardBufferLength(&keyboard->buffer) == 0) {
    return;
  }

  // Send the next scancode in the buffer.
  uint8_t scancode = *KeyboardBufferGet(&keyboard->buffer, 0);
  KeyboardBufferRemove(&keyboard->buffer, 0);
  KeyboardSendScancode(keyboard, scancode);
}

void KeyboardHandleControl(
    KeyboardState* keyboard, bool enable_clear, bool clock_low) {
  // Save previous state.
  bool old_clock_low = keyboard->clock_low;
  bool old_enable_clear = keyboard->enable_clear;

  // Update state.
  keyboard->enable_clear = enable_clear;
  keyboard->clock_low = clock_low;

  // Falling edge of enable_clear bit indicates ack from BIOS. we clear the
  // waiting_for_ack bit, allowing the next queued scancode to be sent on the
  // next tick.
  if (old_enable_clear == true && keyboard->enable_clear == false &&
      keyboard->clock_low == true) {
    keyboard->waiting_for_ack = false;
  }

  // Falling edge of clock_low bit possibly indicates the start of a reset
  // command from BIOS. We reset the timer at 0ms.
  if (old_clock_low == true && keyboard->clock_low == false) {
    keyboard->clock_low_ms = 0;
  }
}

void KeyboardHandleKeyPress(KeyboardState* keyboard, uint8_t scancode) {
  // A keyboard running its self test is not scanning its matrix, so a key
  // pressed now never becomes a scan code. Queueing it would let it resurface
  // once the reset ends, which lands it in the middle of the BIOS's stuck key
  // test - the BIOS clears the interface, re-enables the keyboard, and reports
  // anything arriving over the next few hundred milliseconds as a stuck key.
  //
  // Only a reset drops keys. Holding the clock low briefly just stops the
  // keyboard talking, and it keeps scanning and buffering through that.
  if (!keyboard->clock_low &&
      keyboard->clock_low_ms == kKeyboardResetTriggered) {
    return;
  }
  KeyboardBufferAppend(&keyboard->buffer, &scancode);
}

void KeyboardTickMs(KeyboardState* keyboard) {
  // If clock_low line is being held low, update timer and trigger reset if
  // reached threshold.
  if (keyboard->clock_low == false) {
    if (keyboard->clock_low_ms == kKeyboardResetTriggered) {
      // Reset already triggered, nothing to do.
      return;
    }

    // Increment timer since clock line was held low.
    ++keyboard->clock_low_ms;

    // Haven't reached threshold yet, nothing to do.
    if (keyboard->clock_low_ms < kKeyboardResetThresholdMs) {
      return;
    }

    // Reached threshold, trigger reset.
    KeyboardBufferClear(&keyboard->buffer);
    keyboard->waiting_for_ack = false;
    // Set to special value indicating reset has been triggered.
    keyboard->clock_low_ms = kKeyboardResetTriggered;
    // Send self-test passed scancode on next falling edge of enable_clear.
    uint8_t scancode = kKeyboardSelfTestOK;
    KeyboardBufferAppend(&keyboard->buffer, &scancode);
    return;
  }

  // Normal operation.
  KeyboardSendNextScancode(keyboard);
}


// ==============================================================================
// src/keyboard/keyboard.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_KEYBOARD_BUNDLE_H

