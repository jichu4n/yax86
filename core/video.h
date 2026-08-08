// ==============================================================================
// YAX86 VIDEO MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_VIDEO_BUNDLE_H
#define YAX86_VIDEO_BUNDLE_H

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
// src/video/public.h start
// ==============================================================================

#line 1 "./src/video/public.h"
// Public interface for the Video module.
#ifndef YAX86_VIDEO_PUBLIC_H
#define YAX86_VIDEO_PUBLIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef YAX86_VIDEO_BUNDLE_H
#include "../util/log.h"
#include "../util/static_vector.h"
#endif  // YAX86_VIDEO_BUNDLE_H

enum {
  // Log module ID for the Video.
  kLogModuleIDVideo = 8,
};

// Log module for the Video.
static const LogModule kLogModuleVideo = {
    .id = kLogModuleIDVideo,
    .name = "VIDEO",
};

// ============================================================================
// General
// ============================================================================

// RGB pixel value.
typedef struct RGB {
  // Red component (0-255).
  uint8_t r;
  // Green component (0-255).
  uint8_t g;
  // Blue component (0-255).
  uint8_t b;
} RGB;

// Position in 2D space.
typedef struct Position {
  // X coordinate.
  uint16_t x;
  // Y coordinate.
  uint16_t y;
} Position;

// Text mode character position. We use a different structure to avoid confusion
// with Position, which is used for pixel coordinates.
typedef struct TextPosition {
  // Column (0-based).
  uint8_t col;
  // Row (0-based).
  uint8_t row;
} TextPosition;

// Video adapters supported by this module. Only one adapter is present in a
// machine at a time, selected before initialization.
typedef enum VideoAdapter {
  // Monochrome Display and Printer Adapter.
  kVideoAdapterMDA = 0,
  // Color Graphics Adapter.
  kVideoAdapterCGA = 1,

  // Number of adapters supported.
  kNumVideoAdapters = 2,
} VideoAdapter;

// Video modes, numbered as in the BIOS INT 10h mode numbers.
typedef enum VideoMode {
  // CGA text mode 0x00: Text, 40x25, color burst off, 8x8
  kVideoModeCGAText40x25Mono = 0x00,
  // CGA text mode 0x01: Text, 40x25, 16 colors, 8x8
  kVideoModeCGAText40x25Color = 0x01,
  // CGA text mode 0x02: Text, 80x25, color burst off, 8x8
  kVideoModeCGAText80x25Mono = 0x02,
  // CGA text mode 0x03: Text, 80x25, 16 colors, 8x8
  kVideoModeCGAText80x25Color = 0x03,
  // CGA graphics mode 0x04: 320x200, 4 colors
  kVideoModeCGAGraphics320x200 = 0x04,
  // CGA graphics mode 0x05: 320x200, 4 colors, alternate palette
  kVideoModeCGAGraphics320x200Alt = 0x05,
  // CGA graphics mode 0x06: 640x200, 2 colors
  kVideoModeCGAGraphics640x200 = 0x06,
  // MDA text mode 0x07: Text, 80×25, monochrome, 720x350, 9x14
  kVideoModeMDAText80x25 = 0x07,

  // Number of video modes supported.
  kNumVideoModes = 8,
} VideoMode;

// Text vs graphics modes.
typedef enum VideoModeType {
  // Invalid video mode. This is needed due to gap in the list of video mode
  // values.
  kVideoModeUnsupported = 0,
  // Text mode.
  kVideoModeText,
  // Graphics mode.
  kVideoModeGraphics,
} VideoModeType;

// Metadata for video modes.
typedef struct VideoModeMetadata {
  // The video mode.
  VideoMode mode;
  // Type of the video mode (text or graphics).
  VideoModeType type;
  // Mapped memory address of video RAM.
  uint32_t vram_address;
  // Video RAM size in bytes.
  uint32_t vram_size;
  // Resolution width in pixels.
  uint16_t width;
  // Resolution height in pixels.
  uint16_t height;
  // Number of pages in the video mode.
  uint8_t num_pages;

  // Text mode - number of columns.
  uint8_t columns;
  // Text mode - number of rows.
  uint8_t rows;
  // Text mode - character width in pixels.
  uint8_t char_width;
  // Text mode - character height in pixels.
  uint8_t char_height;
} VideoModeMetadata;

// ============================================================================
// Monochrome Display and Printer Adapter (MDA)
// ============================================================================

// MDA I/O ports
// ========================================
// I/O Register |
// Address      |  Function
// -------------|--------------------------
// 3B0          | Not Used
// 3B1          | Not Used
// 3B2          | Not Used
// 3B3          | Not Used
// 3B4          | 6845 Index Register
// 3B5          | 6845 Data Register
// 3B6          | Not Used
// 3B7          | Not Used
// 3B8          | CRT Control Port 1
// 3B9          | Reserved
// 3BA          | CRT Status Port
// 3BB          | Reserved
// 3BC          | Parallel Data Port
// 3BD          | Printer Status Port
// 3BE          | Printer Control Port
// 3BF          | Not Used
// ========================================

// CRT Control Port 1 (I/O port 3B8) - write only
// ========================================
// Bit Number | Function
//------------|-------------------------
// 0          | + High Resolution Mode
// 1          | Not Used
// 2          | Not Used
// 3          | + Video Enable
// 4          | Not Used
// 5          | + Enable Blink
// 6,7        | Not Used
// ========================================

// CRT Status Port (I/O port 3BA) - read only
// ========================================
// Bit Number | Function
//------------|-------------------------
// 0          | + Horizontal Drive
// 1          | Reserved
// 2          | Reserved
// 3          | + Black/White Video
// ========================================

// MDA I/O ports.
enum {
  kMDAPortRegisterIndex = 0x3B4,
  kMDAPortRegisterData = 0x3B5,
  kMDAPortControl = 0x3B8,
  kMDAPortStatus = 0x3BA,
  kMDAPortPrinterData = 0x3BC,
  kMDAPortPrinterStatus = 0x3BD,
  kMDAPortPrinterControl = 0x3BE,

  // Start of the MDA I/O port range.
  kMDAPortStart = 0x3B0,
  // Inclusive end of the MDA I/O port range.
  kMDAPortEnd = 0x3BF,
};

enum {
  // MDA VRAM address.
  kMDAVRAMAddress = 0xB0000,
  // MDA VRAM size.
  kMDAVRAMSize = 4 * 1024,  // 4K
};

// ============================================================================
// Color Graphics Adapter (CGA)
// ============================================================================

// CGA I/O ports
// ========================================
// I/O Register |
// Address      |  Function
// -------------|--------------------------
// 3D0          | 6845 Index Register (alias of 3D4)
// 3D1          | 6845 Data Register (alias of 3D5)
// 3D2          | 6845 Index Register (alias of 3D4)
// 3D3          | 6845 Data Register (alias of 3D5)
// 3D4          | 6845 Index Register
// 3D5          | 6845 Data Register
// 3D6          | 6845 Index Register (alias of 3D4)
// 3D7          | 6845 Data Register (alias of 3D5)
// 3D8          | Mode Control Register
// 3D9          | Color Select Register
// 3DA          | CRT Status Register
// 3DB          | Clear Light Pen Latch
// 3DC          | Preset Light Pen Latch
// 3DD - 3DF    | Not Used
// ========================================

// Mode Control Register (I/O port 3D8) - write only
// ========================================
// Bit Number | Function
//------------|-------------------------
// 0          | + 80x25 Text Mode (0 = 40x25)
// 1          | + Graphics Mode
// 2          | + Black/White Mode (disables the color burst)
// 3          | + Video Enable
// 4          | + 640x200 High Resolution Graphics Mode
// 5          | + Enable Blink
// 6,7        | Not Used
// ========================================

// Color Select Register (I/O port 3D9) - write only
// ========================================
// Bit Number | Function
//------------|-------------------------
// 0-2        | Background / border RGB. In 640x200 graphics, this is the
//            | foreground color instead.
// 3          | + Intensity of the above
// 4          | + Intensity of the 320x200 graphics palette
// 5          | 320x200 graphics palette (0 = green/red/brown,
//            | 1 = cyan/magenta/white)
// 6,7        | Not Used
// ========================================

// CRT Status Register (I/O port 3DA) - read only
// ========================================
// Bit Number | Function
//------------|-------------------------
// 0          | + Display disabled - VRAM access is safe
// 1          | + Light pen trigger set
// 2          | - Light pen switch is on
// 3          | + Vertical retrace in progress
// ========================================

// CGA I/O ports.
enum {
  kCGAPortRegisterIndex = 0x3D4,
  kCGAPortRegisterData = 0x3D5,
  kCGAPortControl = 0x3D8,
  kCGAPortColorSelect = 0x3D9,
  kCGAPortStatus = 0x3DA,
  kCGAPortClearLightPen = 0x3DB,
  kCGAPortPresetLightPen = 0x3DC,

  // Start of the CGA I/O port range.
  kCGAPortStart = 0x3D0,
  // Inclusive end of the CGA I/O port range.
  kCGAPortEnd = 0x3DF,
};

enum {
  // CGA VRAM address.
  kCGAVRAMAddress = 0xB8000,
  // CGA VRAM size.
  kCGAVRAMSize = 16 * 1024,  // 16K
  // Byte offset of the odd scan lines in CGA graphics modes. Graphics VRAM is
  // interleaved - even scan lines live in the first half and odd scan lines in
  // the second.
  kCGAGraphicsOddScanLineOffset = 0x2000,
  // Number of bytes per scan line in CGA graphics modes.
  kCGAGraphicsBytesPerScanLine = 80,
  // Number of colors in the CGA palette.
  kNumCGAColors = 16,
};

// ============================================================================
// Motorola 6845 CRT controller
// ============================================================================

// Both the MDA and the CGA are built around a Motorola 6845 CRT controller. The
// index port (3B4 on MDA, 3D4 on CGA) selects a register, and the data port
// (3B5 / 3D5) is used to read or write the data for that register. Below are
// the registers and their default values for the IBM Monochrome Display and for
// the CGA in 80x25 text mode, as programmed by GLaBIOS.
// =============================================================================
// Register | Register File              | Program Unit     | MDA  | CGA 80x25
// ---------|----------------------------|------------------|------|-----------
// R0       | Horizontal Total           | Characters       | 0x61 | 0x71
// R1       | Horizontal Displayed       | Characters       | 0x50 | 0x50
// R2       | Horizontal Sync Position   | Characters       | 0x52 | 0x5A
// R3       | Horizontal Sync Width      | Characters       | 0x0F | 0x0A
// R4       | Vertical Total             | Character Rows   | 0x19 | 0x1F
// R5       | Vertical Total Adjust      | Scan Line        | 0x06 | 0x06
// R6       | Vertical Displayed         | Character Row    | 0x19 | 0x19
// R7       | Vertical Sync Position     | Character Row    | 0x19 | 0x1C
// R8       | Interlace Mode             | --------         | 0x02 | 0x02
// R9       | Maximum Scan Line          | Scan Line        | 0x0D | 0x07
// R10      | Cursor Start               | Scan Line        | 0x0B | 0x06
// R11      | Cursor End                 | Scan Line        | 0x0C | 0x07
// R12      | Start Address (H)          | --------         | 0x00 | 0x00
// R13      | Start Address (L)          | --------         | 0x00 | 0x00
// R14      | Cursor (H)                 | --------         | 0x00 | 0x00
// R15      | Cursor (L)                 | --------         | 0x00 | 0x00
// R16      | Reserved                   | --------         | --   | --
// R17      | Reserved                   | --------         | --   | --
// =============================================================================

// 6845 CRT controller registers.
enum {
  kCRTCRegisterHorizontalTotal = 0,
  kCRTCRegisterHorizontalDisplayed,
  kCRTCRegisterHorizontalSyncPosition,
  kCRTCRegisterHorizontalSyncWidth,
  kCRTCRegisterVerticalTotal,
  kCRTCRegisterVerticalTotalAdjust,
  kCRTCRegisterVerticalDisplayed,
  kCRTCRegisterVerticalSyncPosition,
  kCRTCRegisterInterlaceMode,
  kCRTCRegisterMaximumScanLine,
  kCRTCRegisterCursorStart,
  kCRTCRegisterCursorEnd,
  kCRTCRegisterStartAddressH,
  kCRTCRegisterStartAddressL,
  kCRTCRegisterCursorH,
  kCRTCRegisterCursorL,
  kCRTCRegisterReserved16,
  kCRTCRegisterReserved17,

  // Total number of 6845 CRT controller registers.
  kNumCRTCRegisters,
};

// ============================================================================
// Register bits
// ============================================================================

// Bits in the mode control register - I/O port 3B8 on MDA, 3D8 on CGA.
enum {
  // CGA only - 80x25 text mode. MDA uses the same bit for high resolution mode,
  // which it always is.
  kVideoControlHighResolution = 1 << 0,
  // CGA only - graphics mode.
  kVideoControlGraphics = 1 << 1,
  // CGA only - black and white mode, i.e. the color burst is disabled. This has
  // no effect on an RGB monitor in text modes, but selects the third palette in
  // 320x200 graphics mode.
  kVideoControlBlackAndWhite = 1 << 2,
  // Video signal enabled. When clear, the display is blank.
  kVideoControlVideoEnable = 1 << 3,
  // CGA only - 640x200 high resolution graphics mode.
  kVideoControlHighResolutionGraphics = 1 << 4,
  // Attribute bit 7 means blinking rather than intense background.
  kVideoControlEnableBlink = 1 << 5,
};

// Bits in the CGA color select register - I/O port 3D9.
enum {
  // Background / border color, or the foreground color in 640x200 graphics.
  kCGAColorSelectColorMask = 0x07,
  // Intensity of the background / border color.
  kCGAColorSelectIntensity = 1 << 3,
  // Intensity of the 320x200 graphics palette.
  kCGAColorSelectPaletteIntensity = 1 << 4,
  // 320x200 graphics palette selection.
  kCGAColorSelectPalette = 1 << 5,
};

// Bits in the status register - I/O port 3BA on MDA, 3DA on CGA.
enum {
  // Display is disabled, i.e. a horizontal or vertical retrace is in progress,
  // so VRAM can be accessed without causing snow.
  kVideoStatusDisplayDisabled = 1 << 0,
  // Light pen trigger set. Always clear, as no light pen is emulated.
  kVideoStatusLightPenTrigger = 1 << 1,
  // Light pen switch is off. Always set, as no light pen is emulated.
  kVideoStatusLightPenSwitchOff = 1 << 2,
  // Vertical retrace in progress.
  kVideoStatusVerticalRetrace = 1 << 3,
};

// Bits in a text mode attribute byte.
enum {
  // Foreground color.
  kVideoAttributeForegroundMask = 0x07,
  // Intense foreground.
  kVideoAttributeIntenseForeground = 1 << 3,
  // Background color.
  kVideoAttributeBackgroundMask = 0x70,
  // Number of bits to shift right to get the background color.
  kVideoAttributeBackgroundShift = 4,
  // Blinking foreground, or intense background if blinking is disabled in the
  // mode control register.
  kVideoAttributeBlink = 1 << 7,
};

// ============================================================================
// Video mode and adapter metadata
// ============================================================================

// Metadata for each video mode, indexed by mode number.
static const VideoModeMetadata kVideoModeMetadata[kNumVideoModes] = {
    // 0x00: CGA text, 40x25, color burst off
    {
        .mode = kVideoModeCGAText40x25Mono,
        .type = kVideoModeText,
        .vram_address = kCGAVRAMAddress,
        .vram_size = kCGAVRAMSize,
        .width = 320,
        .height = 200,
        .num_pages = 8,
        .columns = 40,
        .rows = 25,
        .char_width = 8,
        .char_height = 8,
    },
    // 0x01: CGA text, 40x25, 16 colors
    {
        .mode = kVideoModeCGAText40x25Color,
        .type = kVideoModeText,
        .vram_address = kCGAVRAMAddress,
        .vram_size = kCGAVRAMSize,
        .width = 320,
        .height = 200,
        .num_pages = 8,
        .columns = 40,
        .rows = 25,
        .char_width = 8,
        .char_height = 8,
    },
    // 0x02: CGA text, 80x25, color burst off
    {
        .mode = kVideoModeCGAText80x25Mono,
        .type = kVideoModeText,
        .vram_address = kCGAVRAMAddress,
        .vram_size = kCGAVRAMSize,
        .width = 640,
        .height = 200,
        .num_pages = 4,
        .columns = 80,
        .rows = 25,
        .char_width = 8,
        .char_height = 8,
    },
    // 0x03: CGA text, 80x25, 16 colors
    {
        .mode = kVideoModeCGAText80x25Color,
        .type = kVideoModeText,
        .vram_address = kCGAVRAMAddress,
        .vram_size = kCGAVRAMSize,
        .width = 640,
        .height = 200,
        .num_pages = 4,
        .columns = 80,
        .rows = 25,
        .char_width = 8,
        .char_height = 8,
    },
    // 0x04: CGA graphics, 320x200, 4 colors
    {
        .mode = kVideoModeCGAGraphics320x200,
        .type = kVideoModeGraphics,
        .vram_address = kCGAVRAMAddress,
        .vram_size = kCGAVRAMSize,
        .width = 320,
        .height = 200,
        .num_pages = 1,
    },
    // 0x05: CGA graphics, 320x200, 4 colors, alternate palette
    {
        .mode = kVideoModeCGAGraphics320x200Alt,
        .type = kVideoModeGraphics,
        .vram_address = kCGAVRAMAddress,
        .vram_size = kCGAVRAMSize,
        .width = 320,
        .height = 200,
        .num_pages = 1,
    },
    // 0x06: CGA graphics, 640x200, 2 colors
    {
        .mode = kVideoModeCGAGraphics640x200,
        .type = kVideoModeGraphics,
        .vram_address = kCGAVRAMAddress,
        .vram_size = kCGAVRAMSize,
        .width = 640,
        .height = 200,
        .num_pages = 1,
    },
    // 0x07: MDA text, 80x25, monochrome
    {
        .mode = kVideoModeMDAText80x25,
        .type = kVideoModeText,
        .vram_address = kMDAVRAMAddress,
        .vram_size = kMDAVRAMSize,
        .width = 720,
        .height = 350,
        .num_pages = 1,
        .columns = 80,
        .rows = 25,
        .char_width = 9,
        .char_height = 14,
    },
};

// Metadata for a video adapter - the facts about an adapter that do not depend
// on the current video mode.
typedef struct VideoAdapterMetadata {
  // The adapter.
  VideoAdapter adapter;

  // Width of the frame buffer the host must provide, in pixels. This does not
  // change with the video mode: the CGA always scans the same number of dots
  // across the screen, and modes with a lower horizontal resolution are drawn
  // with each pixel doubled horizontally, just as on real hardware.
  uint16_t frame_buffer_width;
  // Height of the frame buffer the host must provide, in pixels.
  uint16_t frame_buffer_height;

  // Mapped memory address of video RAM.
  uint32_t vram_address;
  // Video RAM size in bytes.
  uint32_t vram_size;

  // Start of the adapter's I/O port range.
  uint16_t port_start;
  // Inclusive end of the adapter's I/O port range.
  uint16_t port_end;

  // Mode control register value at power-on. The power-on video mode follows
  // from it, so it is not stored separately.
  uint8_t default_control_register;

  // Number of CPU cycles per scan line.
  uint16_t cycles_per_scan_line;
  // Number of CPU cycles at the start of a scan line during which the display
  // is active. The remainder of the scan line is the horizontal retrace.
  uint16_t display_cycles_per_scan_line;
  // Number of scan lines per frame, including the vertical retrace.
  uint16_t scan_lines_per_frame;
  // Number of scan lines at the start of a frame that are displayed. The
  // remainder of the frame is the vertical retrace.
  uint16_t displayed_scan_lines;
} VideoAdapterMetadata;

// Metadata for each video adapter, indexed by VideoAdapter.
//
// The timing values are derived from the same 14.318MHz master crystal the CPU
// clock comes from, so they are exact in CPU cycles. The MDA scans 882 dots per
// line at 16.257MHz over 370 lines, which is 259 cycles per line and just under
// 50Hz. The CGA scans 912 dots per line at 14.318MHz over 262 lines, which is
// 304 cycles per line and just under 60Hz.
static const VideoAdapterMetadata kVideoAdapterMetadata[kNumVideoAdapters] = {
    // MDA
    {
        .adapter = kVideoAdapterMDA,
        .frame_buffer_width = 720,
        .frame_buffer_height = 350,
        .vram_address = kMDAVRAMAddress,
        .vram_size = kMDAVRAMSize,
        .port_start = kMDAPortStart,
        .port_end = kMDAPortEnd,
        // High resolution mode, video enable, blink enable.
        .default_control_register = 0x29,
        .cycles_per_scan_line = 259,
        .display_cycles_per_scan_line = 211,
        .scan_lines_per_frame = 370,
        .displayed_scan_lines = 350,
    },
    // CGA
    {
        .adapter = kVideoAdapterCGA,
        .frame_buffer_width = 640,
        .frame_buffer_height = 200,
        .vram_address = kCGAVRAMAddress,
        .vram_size = kCGAVRAMSize,
        .port_start = kCGAPortStart,
        .port_end = kCGAPortEnd,
        // 80x25 text mode, video enable, blink enable.
        .default_control_register = 0x29,
        .cycles_per_scan_line = 304,
        .display_cycles_per_scan_line = 213,
        .scan_lines_per_frame = 262,
        .displayed_scan_lines = 200,
    },
};

enum {
  // Number of frames between cursor blink phase changes. At roughly 50Hz this
  // gives a blink rate of about 3.1Hz, close to what the 6845 produces when it
  // is configured to blink at a sixteenth of the field rate.
  kVideoFramesPerCursorBlinkPhase = 8,
  // Number of frames between character blink phase changes. Characters blink
  // half as fast as the cursor: the 6845 generates the cursor blink itself,
  // while the blink attribute is decoded by the adapter from a separate
  // divider running at a thirty-secondth of the field rate.
  kVideoFramesPerTextBlinkPhase = 16,
};

enum {
  // Video memory map entry type.
  kMemoryMapEntryVRAM = 0x10,
  // Video port map entry type.
  kPortMapEntryVideo = 0x10,
};

// ============================================================================
// Video state
// ============================================================================

struct VideoState;

// Caller-provided configuration for video rendering.
typedef struct VideoConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger for this module. May be NULL.
  Logger* logger;

  // The video adapter to emulate.
  VideoAdapter adapter;

  // MDA - foreground color.
  RGB foreground;
  // MDA - intense foreground color.
  RGB intense_foreground;
  // MDA - background color.
  RGB background;

  // CGA - the 16 color RGBI palette.
  RGB cga_palette[kNumCGAColors];

  // Callback to read a byte from the emulated video RAM.
  uint8_t (*read_vram_byte)(struct VideoState* video, uint32_t address);
  // Callback to write a byte to the emulated video RAM.
  void (*write_vram_byte)(
      struct VideoState* video, uint32_t address, uint8_t value);

  // Callback to write an RGB pixel value to the real display, invoked from
  // VideoRender().
  void (*write_pixel)(struct VideoState* video, Position position, RGB rgb);
} VideoConfig;

// Default video config.
static const VideoConfig kDefaultVideoConfig = {
    .context = NULL,

    .adapter = kVideoAdapterMDA,

    .foreground = {.r = 0xAA, .g = 0xAA, .b = 0xAA},
    .intense_foreground = {.r = 0xFF, .g = 0xFF, .b = 0xFF},
    .background = {.r = 0x00, .g = 0x00, .b = 0x00},

    .cga_palette =
        {
            {.r = 0x00, .g = 0x00, .b = 0x00},  // 0  black
            {.r = 0x00, .g = 0x00, .b = 0xAA},  // 1  blue
            {.r = 0x00, .g = 0xAA, .b = 0x00},  // 2  green
            {.r = 0x00, .g = 0xAA, .b = 0xAA},  // 3  cyan
            {.r = 0xAA, .g = 0x00, .b = 0x00},  // 4  red
            {.r = 0xAA, .g = 0x00, .b = 0xAA},  // 5  magenta
            {.r = 0xAA, .g = 0x55, .b = 0x00},  // 6  brown
            {.r = 0xAA, .g = 0xAA, .b = 0xAA},  // 7  light gray
            {.r = 0x55, .g = 0x55, .b = 0x55},  // 8  dark gray
            {.r = 0x55, .g = 0x55, .b = 0xFF},  // 9  light blue
            {.r = 0x55, .g = 0xFF, .b = 0x55},  // 10 light green
            {.r = 0x55, .g = 0xFF, .b = 0xFF},  // 11 light cyan
            {.r = 0xFF, .g = 0x55, .b = 0x55},  // 12 light red
            {.r = 0xFF, .g = 0x55, .b = 0xFF},  // 13 light magenta
            {.r = 0xFF, .g = 0xFF, .b = 0x55},  // 14 yellow
            {.r = 0xFF, .g = 0xFF, .b = 0xFF},  // 15 white
        },

    .read_vram_byte = NULL,
    .write_vram_byte = NULL,
    .write_pixel = NULL,
};

// Video state.
typedef struct VideoState {
  // Caller-provided runtime configuration.
  VideoConfig* config;

  // The video adapter being emulated, copied from the config at init time.
  VideoAdapter adapter;

  // Motorola 6845 CRT controller registers.
  uint8_t registers[kNumCRTCRegisters];
  // Currently selected 6845 CRT controller register index (I/O port 3B4/3D4).
  uint8_t selected_register;
  // Mode control register value (I/O port 3B8/3D8).
  uint8_t control_register;
  // CGA color select register value (I/O port 3D9). Unused on MDA.
  uint8_t color_select_register;

  // Current scan line within the frame, including the vertical retrace.
  uint16_t scan_line;
  // CPU cycles elapsed within the current scan line.
  uint32_t scan_line_cycles;
  // Number of frames since initialization. Drives blinking.
  uint32_t frames;
} VideoState;

// Initialize video state with the provided configuration.
void VideoInit(VideoState* video, VideoConfig* config);

// Metadata for the adapter being emulated.
const VideoAdapterMetadata* VideoGetAdapterMetadata(const VideoState* video);

// The current video mode, derived from the mode control register.
VideoMode VideoGetMode(const VideoState* video);

// Metadata for the current video mode.
const VideoModeMetadata* VideoGetModeMetadata(const VideoState* video);

// Read a byte from a video I/O port.
uint8_t VideoReadPort(VideoState* video, uint16_t port);
// Write a byte to a video I/O port.
void VideoWritePort(VideoState* video, uint16_t port, uint8_t value);

// Read a byte from video RAM.
uint8_t VideoReadVRAM(VideoState* video, uint32_t address);
// Write a byte to video RAM.
void VideoWriteVRAM(VideoState* video, uint32_t address, uint8_t value);

// Advance the CRT beam by the given number of CPU cycles. This drives the
// retrace bits in the status register and the blink phase.
void VideoTick(VideoState* video, uint16_t cycles);

// Render the current display. Invokes the write_pixel callback to do the actual
// pixel rendering.
void VideoRender(VideoState* video);

#endif  // YAX86_VIDEO_PUBLIC_H


// ==============================================================================
// src/video/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/video/fonts.h start
// ==============================================================================

#line 1 "./src/video/fonts.h"
#ifndef YAX86_VIDEO_FONTS_H
#define YAX86_VIDEO_FONTS_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"

// MDA 9x14 font bitmaps.
extern const uint16_t kFontMDA9x14Bitmap[256][14];
// CGA 8x8 font bitmaps.
extern const uint8_t kFontCGA8x8Bitmap[256][8];

#endif  // YAX86_IMPLEMENTATION

#endif  // YAX86_VIDEO_FONTS_H


// ==============================================================================
// src/video/fonts.h end
// ==============================================================================

// ==============================================================================
// src/video/internal.h start
// ==============================================================================

#line 1 "./src/video/internal.h"
// Internal interface shared between the video module's source files.
#ifndef YAX86_VIDEO_INTERNAL_H
#define YAX86_VIDEO_INTERNAL_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Read a byte from the emulated video RAM, or 0xFF if no callback is installed.
// VRAM is aliased throughout the adapter's window, so an address past the end
// wraps around.
YAX86_PRIVATE uint8_t VideoReadVRAMByte(VideoState* video, uint32_t address);

// Write a byte to the emulated video RAM, ignored if no callback is installed.
// The address wraps in the same way as for reads.
YAX86_PRIVATE void VideoWriteVRAMByte(
    VideoState* video, uint32_t address, uint8_t value);

// Write an RGB pixel value to the real display, ignored if no callback is
// installed.
YAX86_PRIVATE void VideoWritePixel(
    VideoState* video, Position position, RGB rgb);

// Whether the text mode cursor is currently in the visible half of its blink
// cycle.
YAX86_PRIVATE bool VideoIsCursorBlinkOn(const VideoState* video);

// Whether characters carrying the blink attribute are currently visible. This
// runs at half the cursor's rate, so the two drift in and out of phase.
YAX86_PRIVATE bool VideoIsTextBlinkOn(const VideoState* video);

// Whether the text mode cursor is enabled in the 6845 registers.
YAX86_PRIVATE bool VideoIsCursorEnabled(const VideoState* video);

// The first scan line of the character cell covered by the text mode cursor,
// from the 6845 cursor start register. May be out of range for the current
// character height.
YAX86_PRIVATE uint8_t VideoGetCursorStartScanLine(const VideoState* video);

// The last scan line of the character cell covered by the text mode cursor,
// from the 6845 cursor end register. May be out of range for the current
// character height.
YAX86_PRIVATE uint8_t VideoGetCursorEndScanLine(const VideoState* video);

// The address of the first displayed character, in character units, from the
// 6845 start address registers.
YAX86_PRIVATE uint16_t VideoGetStartAddress(const VideoState* video);

// The address of the text mode cursor, in character units, from the 6845 cursor
// address registers.
YAX86_PRIVATE uint16_t VideoGetCursorAddress(const VideoState* video);

// Render the current display in MDA text mode.
YAX86_PRIVATE void MDARenderScreen(VideoState* video);

// Render the current display on the CGA.
YAX86_PRIVATE void CGARenderScreen(VideoState* video);

#endif  // YAX86_VIDEO_INTERNAL_H


// ==============================================================================
// src/video/internal.h end
// ==============================================================================

// ==============================================================================
// src/video/fonts.c start
// ==============================================================================

#line 1 "./src/video/fonts.c"
#ifndef YAX86_IMPLEMENTATION
#include "fonts.h"

#include "../util/common.h"
#endif  // YAX86_IMPLEMENTATION

// MDA 9x14 font bitmaps. Each character is actually 8x14, with the rightmost
// column left blank except for block characters.
//
// From pcface project:
// https://github.com/susam/pcface/blob/main/out/oldschool-mda-9x14/fontlist.js
YAX86_PRIVATE const uint16_t kFontMDA9x14Bitmap[256][14] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [] (0)
    {0x00, 0x00, 0xfc, 0x102, 0x14a, 0x102, 0x102, 0x17a, 0x132, 0x102, 0xfc,
     0x00, 0x00, 0x00},  // [☺] (1)
    {0x00, 0x00, 0xfc, 0x1fe, 0x1b6, 0x1fe, 0x1fe, 0x186, 0x1ce, 0x1fe, 0xfc,
     0x00, 0x00, 0x00},  // [☻] (2)
    {0x00, 0x00, 0x00, 0x6c, 0xfe, 0xfe, 0xfe, 0xfe, 0x7c, 0x38, 0x10, 0x00,
     0x00, 0x00},  // [♥] (3)
    {0x00, 0x00, 0x00, 0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x38, 0x10, 0x00, 0x00,
     0x00, 0x00},  // [♦] (4)
    {0x00, 0x00, 0x30, 0x78, 0x78, 0x1ce, 0x1ce, 0x1ce, 0x30, 0x30, 0x78, 0x00,
     0x00, 0x00},  // [♣] (5)
    {0x00, 0x00, 0x30, 0x78, 0xfc, 0x1fe, 0x1fe, 0xfc, 0x30, 0x30, 0x78, 0x00,
     0x00, 0x00},  // [♠] (6)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x78, 0x78, 0x30, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [•] (7)
    {0x1fe, 0x1fe, 0x1fe, 0x1fe, 0x1fe, 0x1ce, 0x186, 0x186, 0x1ce, 0x1fe,
     0x1fe, 0x1fe, 0x1fe, 0x1fe},  // [◘] (8)
    {0x00, 0x00, 0x00, 0x00, 0x78, 0xcc, 0x84, 0x84, 0xcc, 0x78, 0x00, 0x00,
     0x00, 0x00},  // [○] (9)
    {0x1fe, 0x1fe, 0x1fe, 0x1fe, 0x186, 0x132, 0x17a, 0x17a, 0x132, 0x186,
     0x1fe, 0x1fe, 0x1fe, 0x1fe},  // [◙] (10)
    {0x00, 0x00, 0x1e, 0x0e, 0x1a, 0x32, 0x78, 0xcc, 0xcc, 0xcc, 0x78, 0x00,
     0x00, 0x00},  // [♂] (11)
    {0x00, 0x00, 0x78, 0xcc, 0xcc, 0xcc, 0x78, 0x30, 0xfc, 0x30, 0x30, 0x00,
     0x00, 0x00},  // [♀] (12)
    {0x00, 0x00, 0x7e, 0x66, 0x7e, 0x60, 0x60, 0x60, 0xe0, 0x1e0, 0x1c0, 0x00,
     0x00, 0x00},  // [♪] (13)
    {0x00, 0x00, 0xfe, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0xce, 0x1ce, 0x1cc, 0x180,
     0x00, 0x00},  // [♫] (14)
    {0x00, 0x00, 0x30, 0x30, 0x1b6, 0x78, 0x1ce, 0x78, 0x1b6, 0x30, 0x30, 0x00,
     0x00, 0x00},  // [☼] (15)
    {0x00, 0x00, 0x80, 0xc0, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0xc0, 0x80, 0x00,
     0x00, 0x00},  // [►] (16)
    {0x00, 0x00, 0x02, 0x06, 0x0e, 0x3e, 0xfe, 0x3e, 0x0e, 0x06, 0x02, 0x00,
     0x00, 0x00},  // [◄] (17)
    {0x00, 0x00, 0x30, 0x78, 0xfc, 0x30, 0x30, 0x30, 0xfc, 0x78, 0x30, 0x00,
     0x00, 0x00},  // [↕] (18)
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x66, 0x66, 0x00,
     0x00, 0x00},  // [‼] (19)
    {0x00, 0x00, 0xfe, 0x1b6, 0x1b6, 0x1b6, 0xf6, 0x36, 0x36, 0x36, 0x36, 0x00,
     0x00, 0x00},  // [¶] (20)
    {0x00, 0x7c, 0xc6, 0x60, 0x38, 0x6c, 0xc6, 0xc6, 0x6c, 0x38, 0x0c, 0xc6,
     0x7c, 0x00},  // [§] (21)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xfe, 0xfe, 0x00,
     0x00, 0x00},  // [▬] (22)
    {0x00, 0x00, 0x30, 0x78, 0xfc, 0x30, 0x30, 0x30, 0xfc, 0x78, 0x30, 0xfc,
     0x00, 0x00},  // [↨] (23)
    {0x00, 0x00, 0x30, 0x78, 0xfc, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00,
     0x00, 0x00},  // [↑] (24)
    {0x00, 0x00, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0xfc, 0x78, 0x30, 0x00,
     0x00, 0x00},  // [↓] (25)
    {0x00, 0x00, 0x00, 0x00, 0x18, 0x0c, 0xfe, 0x0c, 0x18, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [→] (26)
    {0x00, 0x00, 0x00, 0x00, 0x30, 0x60, 0xfe, 0x60, 0x30, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [←] (27)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xc0, 0xc0, 0xfe, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [∟] (28)
    {0x00, 0x00, 0x00, 0x00, 0x48, 0xcc, 0x1fe, 0xcc, 0x48, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [↔] (29)
    {0x00, 0x00, 0x00, 0x10, 0x38, 0x38, 0x7c, 0x7c, 0xfe, 0xfe, 0x00, 0x00,
     0x00, 0x00},  // [▲] (30)
    {0x00, 0x00, 0x00, 0xfe, 0xfe, 0x7c, 0x7c, 0x38, 0x38, 0x10, 0x00, 0x00,
     0x00, 0x00},  // [▼] (31)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [ ] (32)
    {0x00, 0x00, 0x30, 0x78, 0x78, 0x78, 0x30, 0x30, 0x00, 0x30, 0x30, 0x00,
     0x00, 0x00},  // [!] (33)
    {0x00, 0xc6, 0xc6, 0xc6, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // ["] (34)
    {0x00, 0x00, 0x6c, 0x6c, 0xfe, 0x6c, 0x6c, 0x6c, 0xfe, 0x6c, 0x6c, 0x00,
     0x00, 0x00},  // [#] (35)
    {0x18, 0x18, 0x7c, 0xc6, 0xc2, 0xc0, 0x7c, 0x06, 0x86, 0xc6, 0x7c, 0x18,
     0x18, 0x00},  // [$] (36)
    {0x00, 0x00, 0x00, 0x00, 0xc2, 0xc6, 0x0c, 0x18, 0x30, 0x66, 0xc6, 0x00,
     0x00, 0x00},  // [%] (37)
    {0x00, 0x00, 0x38, 0x6c, 0x6c, 0x38, 0x76, 0xdc, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [&] (38)
    {0x00, 0x60, 0x60, 0x60, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // ['] (39)
    {0x00, 0x00, 0x18, 0x30, 0x60, 0x60, 0x60, 0x60, 0x60, 0x30, 0x18, 0x00,
     0x00, 0x00},  // [(] (40)
    {0x00, 0x00, 0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x18, 0x30, 0x00,
     0x00, 0x00},  // [)] (41)
    {0x00, 0x00, 0x00, 0x00, 0xcc, 0x78, 0x1fe, 0x78, 0xcc, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [*] (42)
    {0x00, 0x00, 0x00, 0x30, 0x30, 0x30, 0x1fe, 0x30, 0x30, 0x30, 0x00, 0x00,
     0x00, 0x00},  // [+] (43)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x30, 0x60,
     0x00, 0x00},  // [,] (44)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1fe, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [-] (45)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00,
     0x00, 0x00},  // [.] (46)
    {0x00, 0x00, 0x02, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x80, 0x00, 0x00,
     0x00, 0x00},  // [/] (47)
    {0x00, 0x00, 0x7c, 0xc6, 0xce, 0xde, 0xf6, 0xe6, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [0] (48)
    {0x00, 0x00, 0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00,
     0x00, 0x00},  // [1] (49)
    {0x00, 0x00, 0x7c, 0xc6, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc6, 0xfe, 0x00,
     0x00, 0x00},  // [2] (50)
    {0x00, 0x00, 0x7c, 0xc6, 0x06, 0x06, 0x3c, 0x06, 0x06, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [3] (51)
    {0x00, 0x00, 0x0c, 0x1c, 0x3c, 0x6c, 0xcc, 0xfe, 0x0c, 0x0c, 0x1e, 0x00,
     0x00, 0x00},  // [4] (52)
    {0x00, 0x00, 0xfe, 0xc0, 0xc0, 0xc0, 0xfc, 0x06, 0x06, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [5] (53)
    {0x00, 0x00, 0x38, 0x60, 0xc0, 0xc0, 0xfc, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [6] (54)
    {0x00, 0x00, 0xfe, 0xc6, 0x06, 0x0c, 0x18, 0x30, 0x30, 0x30, 0x30, 0x00,
     0x00, 0x00},  // [7] (55)
    {0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0x7c, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [8] (56)
    {0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x06, 0x0c, 0x78, 0x00,
     0x00, 0x00},  // [9] (57)
    {0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00,
     0x00, 0x00},  // [:] (58)
    {0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x30, 0x30, 0x60, 0x00,
     0x00, 0x00},  // [;] (59)
    {0x00, 0x00, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x60, 0x30, 0x18, 0x0c, 0x00,
     0x00, 0x00},  // [<] (60)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [=] (61)
    {0x00, 0x00, 0xc0, 0x60, 0x30, 0x18, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x00,
     0x00, 0x00},  // [>] (62)
    {0x00, 0x00, 0x7c, 0xc6, 0xc6, 0x0c, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00,
     0x00, 0x00},  // [?] (63)
    {0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xde, 0xde, 0xde, 0xdc, 0xc0, 0x7c, 0x00,
     0x00, 0x00},  // [@] (64)
    {0x00, 0x00, 0x10, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0x00,
     0x00, 0x00},  // [A] (65)
    {0x00, 0x00, 0xfc, 0x66, 0x66, 0x66, 0x7c, 0x66, 0x66, 0x66, 0xfc, 0x00,
     0x00, 0x00},  // [B] (66)
    {0x00, 0x00, 0x3c, 0x66, 0xc2, 0xc0, 0xc0, 0xc0, 0xc2, 0x66, 0x3c, 0x00,
     0x00, 0x00},  // [C] (67)
    {0x00, 0x00, 0xf8, 0x6c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x6c, 0xf8, 0x00,
     0x00, 0x00},  // [D] (68)
    {0x00, 0x00, 0xfe, 0x66, 0x62, 0x68, 0x78, 0x68, 0x62, 0x66, 0xfe, 0x00,
     0x00, 0x00},  // [E] (69)
    {0x00, 0x00, 0xfe, 0x66, 0x62, 0x68, 0x78, 0x68, 0x60, 0x60, 0xf0, 0x00,
     0x00, 0x00},  // [F] (70)
    {0x00, 0x00, 0x3c, 0x66, 0xc2, 0xc0, 0xc0, 0xde, 0xc6, 0x66, 0x3a, 0x00,
     0x00, 0x00},  // [G] (71)
    {0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0xc6, 0x00,
     0x00, 0x00},  // [H] (72)
    {0x00, 0x00, 0x78, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00,
     0x00, 0x00},  // [I] (73)
    {0x00, 0x00, 0x1e, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0xcc, 0xcc, 0x78, 0x00,
     0x00, 0x00},  // [J] (74)
    {0x00, 0x00, 0xe6, 0x66, 0x6c, 0x6c, 0x78, 0x6c, 0x6c, 0x66, 0xe6, 0x00,
     0x00, 0x00},  // [K] (75)
    {0x00, 0x00, 0xf0, 0x60, 0x60, 0x60, 0x60, 0x60, 0x62, 0x66, 0xfe, 0x00,
     0x00, 0x00},  // [L] (76)
    {0x00, 0x00, 0x186, 0x1ce, 0x1fe, 0x1b6, 0x186, 0x186, 0x186, 0x186, 0x186,
     0x00, 0x00, 0x00},  // [M] (77)
    {0x00, 0x00, 0xc6, 0xe6, 0xf6, 0xfe, 0xde, 0xce, 0xc6, 0xc6, 0xc6, 0x00,
     0x00, 0x00},  // [N] (78)
    {0x00, 0x00, 0x38, 0x6c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x00,
     0x00, 0x00},  // [O] (79)
    {0x00, 0x00, 0xfc, 0x66, 0x66, 0x66, 0x7c, 0x60, 0x60, 0x60, 0xf0, 0x00,
     0x00, 0x00},  // [P] (80)
    {0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xd6, 0xde, 0x7c, 0x0c, 0x0e,
     0x00, 0x00},  // [Q] (81)
    {0x00, 0x00, 0xfc, 0x66, 0x66, 0x66, 0x7c, 0x6c, 0x66, 0x66, 0xe6, 0x00,
     0x00, 0x00},  // [R] (82)
    {0x00, 0x00, 0x7c, 0xc6, 0xc6, 0x60, 0x38, 0x0c, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [S] (83)
    {0x00, 0x00, 0x1fe, 0x1b6, 0x132, 0x30, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00,
     0x00, 0x00},  // [T] (84)
    {0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [U] (85)
    {0x00, 0x00, 0x186, 0x186, 0x186, 0x186, 0x186, 0x186, 0xcc, 0x78, 0x30,
     0x00, 0x00, 0x00},  // [V] (86)
    {0x00, 0x00, 0x186, 0x186, 0x186, 0x186, 0x1b6, 0x1b6, 0x1fe, 0xcc, 0xcc,
     0x00, 0x00, 0x00},  // [W] (87)
    {0x00, 0x00, 0x186, 0x186, 0xcc, 0x78, 0x30, 0x78, 0xcc, 0x186, 0x186, 0x00,
     0x00, 0x00},  // [X] (88)
    {0x00, 0x00, 0x186, 0x186, 0x186, 0xcc, 0x78, 0x30, 0x30, 0x30, 0x78, 0x00,
     0x00, 0x00},  // [Y] (89)
    {0x00, 0x00, 0x1fe, 0x186, 0x10c, 0x18, 0x30, 0x60, 0xc2, 0x186, 0x1fe,
     0x00, 0x00, 0x00},  // [Z] (90)
    {0x00, 0x00, 0x78, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x78, 0x00,
     0x00, 0x00},  // [[] (91)
    {0x00, 0x00, 0x80, 0xc0, 0xe0, 0x70, 0x38, 0x1c, 0x0e, 0x06, 0x02, 0x00,
     0x00, 0x00},  // [\] (92)
    {0x00, 0x00, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x78, 0x00,
     0x00, 0x00},  // []] (93)
    {0x10, 0x38, 0x6c, 0xc6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [^] (94)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x1fe, 0x00},  // [_] (95)
    {0x30, 0x30, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [`] (96)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [a] (97)
    {0x00, 0x00, 0xe0, 0x60, 0x60, 0x78, 0x6c, 0x66, 0x66, 0x66, 0xdc, 0x00,
     0x00, 0x00},  // [b] (98)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc0, 0xc0, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [c] (99)
    {0x00, 0x00, 0x1c, 0x0c, 0x0c, 0x3c, 0x6c, 0xcc, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [d] (100)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [e] (101)
    {0x00, 0x00, 0x38, 0x6c, 0x64, 0x60, 0xf8, 0x60, 0x60, 0x60, 0xf0, 0x00,
     0x00, 0x00},  // [f] (102)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xcc, 0xcc, 0xcc, 0x7c, 0x0c, 0xcc,
     0x78, 0x00},  // [g] (103)
    {0x00, 0x00, 0xe0, 0x60, 0x60, 0x6c, 0x76, 0x66, 0x66, 0x66, 0xe6, 0x00,
     0x00, 0x00},  // [h] (104)
    {0x00, 0x00, 0x18, 0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00,
     0x00, 0x00},  // [i] (105)
    {0x00, 0x00, 0x0c, 0x0c, 0x00, 0x1c, 0x0c, 0x0c, 0x0c, 0x0c, 0xcc, 0xcc,
     0x78, 0x00},  // [j] (106)
    {0x00, 0x00, 0xe0, 0x60, 0x60, 0x66, 0x6c, 0x78, 0x6c, 0x66, 0xe6, 0x00,
     0x00, 0x00},  // [k] (107)
    {0x00, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00,
     0x00, 0x00},  // [l] (108)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1cc, 0x1fe, 0x1b6, 0x1b6, 0x1b6, 0x1b6,
     0x00, 0x00, 0x00},  // [m] (109)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xdc, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00,
     0x00, 0x00},  // [n] (110)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [o] (111)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xdc, 0x66, 0x66, 0x66, 0x7c, 0x60, 0x60,
     0xf0, 0x00},  // [p] (112)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xcc, 0xcc, 0xcc, 0x7c, 0x0c, 0x0c,
     0x1e, 0x00},  // [q] (113)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xdc, 0x76, 0x66, 0x60, 0x60, 0xf0, 0x00,
     0x00, 0x00},  // [r] (114)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0x70, 0x1c, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [s] (115)
    {0x00, 0x00, 0x10, 0x30, 0x30, 0xfc, 0x30, 0x30, 0x30, 0x36, 0x1c, 0x00,
     0x00, 0x00},  // [t] (116)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [u] (117)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x186, 0x186, 0x186, 0xcc, 0x78, 0x30, 0x00,
     0x00, 0x00},  // [v] (118)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x186, 0x186, 0x1b6, 0x1b6, 0x1fe, 0xcc,
     0x00, 0x00, 0x00},  // [w] (119)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xc6, 0x6c, 0x38, 0x38, 0x6c, 0xc6, 0x00,
     0x00, 0x00},  // [x] (120)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x0c,
     0x78, 0x00},  // [y] (121)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xcc, 0x18, 0x30, 0x66, 0xfe, 0x00,
     0x00, 0x00},  // [z] (122)
    {0x00, 0x00, 0x1c, 0x30, 0x30, 0x30, 0xe0, 0x30, 0x30, 0x30, 0x1c, 0x00,
     0x00, 0x00},  // [{] (123)
    {0x00, 0x00, 0x30, 0x30, 0x30, 0x30, 0x00, 0x30, 0x30, 0x30, 0x30, 0x00,
     0x00, 0x00},  // [|] (124)
    {0x00, 0x00, 0xe0, 0x30, 0x30, 0x30, 0x1c, 0x30, 0x30, 0x30, 0xe0, 0x00,
     0x00, 0x00},  // [}] (125)
    {0x00, 0x00, 0x76, 0xdc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [~] (126)
    {0x00, 0x00, 0x00, 0x00, 0x10, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0x00, 0x00,
     0x00, 0x00},  // [⌂] (127)
    {0x00, 0x00, 0x3c, 0x66, 0xc2, 0xc0, 0xc0, 0xc2, 0x66, 0x3c, 0x0c, 0x06,
     0x7c, 0x00},  // [Ç] (128)
    {0x00, 0x00, 0xcc, 0xcc, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [ü] (129)
    {0x00, 0x0c, 0x18, 0x30, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [é] (130)
    {0x00, 0x10, 0x38, 0x6c, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [â] (131)
    {0x00, 0x00, 0xcc, 0xcc, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [ä] (132)
    {0x00, 0x60, 0x30, 0x18, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [à] (133)
    {0x00, 0x38, 0x6c, 0x38, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [å] (134)
    {0x00, 0x00, 0x00, 0x00, 0x78, 0xcc, 0xc0, 0xcc, 0x78, 0x18, 0x0c, 0x78,
     0x00, 0x00},  // [ç] (135)
    {0x00, 0x10, 0x38, 0x6c, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [ê] (136)
    {0x00, 0x00, 0xcc, 0xcc, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [ë] (137)
    {0x00, 0x60, 0x30, 0x18, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [è] (138)
    {0x00, 0x00, 0xcc, 0xcc, 0x00, 0x70, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00,
     0x00, 0x00},  // [ï] (139)
    {0x00, 0x30, 0x78, 0xcc, 0x00, 0x70, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00,
     0x00, 0x00},  // [î] (140)
    {0x00, 0xc0, 0x60, 0x30, 0x00, 0x70, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00,
     0x00, 0x00},  // [ì] (141)
    {0x00, 0xc6, 0xc6, 0x10, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0x00,
     0x00, 0x00},  // [Ä] (142)
    {0x38, 0x6c, 0x38, 0x00, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0x00,
     0x00, 0x00},  // [Å] (143)
    {0x18, 0x30, 0x60, 0x00, 0xfe, 0x66, 0x60, 0x7c, 0x60, 0x66, 0xfe, 0x00,
     0x00, 0x00},  // [É] (144)
    {0x00, 0x00, 0x00, 0x00, 0xdc, 0x76, 0x36, 0xfc, 0x1b0, 0x1b8, 0xee, 0x00,
     0x00, 0x00},  // [æ] (145)
    {0x00, 0x00, 0x3e, 0x6c, 0xcc, 0xcc, 0xfe, 0xcc, 0xcc, 0xcc, 0xce, 0x00,
     0x00, 0x00},  // [Æ] (146)
    {0x00, 0x10, 0x38, 0x6c, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [ô] (147)
    {0x00, 0x00, 0xc6, 0xc6, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [ö] (148)
    {0x00, 0x60, 0x30, 0x18, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [ò] (149)
    {0x00, 0x30, 0x78, 0xcc, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [û] (150)
    {0x00, 0x60, 0x30, 0x18, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [ù] (151)
    {0x00, 0x00, 0xc6, 0xc6, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x0c,
     0x78, 0x00},  // [ÿ] (152)
    {0x00, 0xc6, 0xc6, 0x38, 0x6c, 0xc6, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x00,
     0x00, 0x00},  // [Ö] (153)
    {0x00, 0xc6, 0xc6, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [Ü] (154)
    {0x00, 0x30, 0x30, 0xfc, 0x186, 0x180, 0x180, 0x186, 0xfc, 0x30, 0x30, 0x00,
     0x00, 0x00},  // [¢] (155)
    {0x00, 0x38, 0x6c, 0x64, 0x60, 0xf0, 0x60, 0x60, 0x60, 0xe6, 0xfc, 0x00,
     0x00, 0x00},  // [£] (156)
    {0x00, 0x00, 0x186, 0xcc, 0x78, 0x30, 0x1fe, 0x30, 0x1fe, 0x30, 0x30, 0x00,
     0x00, 0x00},  // [¥] (157)
    {0x00, 0x1f8, 0xcc, 0xcc, 0xf8, 0xc4, 0xcc, 0xde, 0xcc, 0xcc, 0x1e6, 0x00,
     0x00, 0x00},  // [₧] (158)
    {0x00, 0x1c, 0x36, 0x30, 0x30, 0x30, 0xfc, 0x30, 0x30, 0x30, 0x30, 0x1b0,
     0xe0, 0x00},  // [ƒ] (159)
    {0x00, 0x18, 0x30, 0x60, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [á] (160)
    {0x00, 0x18, 0x30, 0x60, 0x00, 0x70, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00,
     0x00, 0x00},  // [í] (161)
    {0x00, 0x18, 0x30, 0x60, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [ó] (162)
    {0x00, 0x18, 0x30, 0x60, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00,
     0x00, 0x00},  // [ú] (163)
    {0x00, 0x00, 0x76, 0xdc, 0x00, 0xdc, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00,
     0x00, 0x00},  // [ñ] (164)
    {0x76, 0xdc, 0x00, 0xc6, 0xe6, 0xf6, 0xfe, 0xde, 0xce, 0xc6, 0xc6, 0x00,
     0x00, 0x00},  // [Ñ] (165)
    {0x00, 0x78, 0xd8, 0xd8, 0x7c, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [ª] (166)
    {0x00, 0x70, 0xd8, 0xd8, 0x70, 0x00, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [º] (167)
    {0x00, 0x00, 0x30, 0x30, 0x00, 0x30, 0x30, 0x60, 0xc6, 0xc6, 0x7c, 0x00,
     0x00, 0x00},  // [¿] (168)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xc0, 0xc0, 0xc0, 0x00, 0x00,
     0x00, 0x00},  // [⌐] (169)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x06, 0x06, 0x06, 0x00, 0x00,
     0x00, 0x00},  // [¬] (170)
    {0x00, 0xc0, 0x1c0, 0xc6, 0xcc, 0xd8, 0x30, 0x60, 0xdc, 0x186, 0x0c, 0x18,
     0x3e, 0x00},  // [½] (171)
    {0x00, 0xc0, 0x1c0, 0xc6, 0xcc, 0xd8, 0x30, 0x66, 0xce, 0x19e, 0x3e, 0x06,
     0x06, 0x00},  // [¼] (172)
    {0x00, 0x00, 0x30, 0x30, 0x00, 0x30, 0x30, 0x78, 0x78, 0x78, 0x30, 0x00,
     0x00, 0x00},  // [¡] (173)
    {0x00, 0x00, 0x00, 0x00, 0x36, 0x6c, 0xd8, 0x6c, 0x36, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [«] (174)
    {0x00, 0x00, 0x00, 0x00, 0xd8, 0x6c, 0x36, 0x6c, 0xd8, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [»] (175)
    {0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88,
     0x22, 0x88},  // [░] (176)
    {0xaa, 0x154, 0xaa, 0x154, 0xaa, 0x154, 0xaa, 0x154, 0xaa, 0x154, 0xaa,
     0x154, 0xaa, 0x154},  // [▒] (177)
    {0x1ba, 0xee, 0x1ba, 0xee, 0x1ba, 0xee, 0x1ba, 0xee, 0x1ba, 0xee, 0x1ba,
     0xee, 0x1ba, 0xee},  // [▓] (178)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [│] (179)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x1f0, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [┤] (180)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x1f0, 0x30, 0x1f0, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [╡] (181)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x1ec, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╢] (182)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1fc, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╖] (183)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1f0, 0x30, 0x1f0, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [╕] (184)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x1ec, 0x0c, 0x1ec, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╣] (185)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [║] (186)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1fc, 0x0c, 0x1ec, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╗] (187)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x1ec, 0x0c, 0x1fc, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [╝] (188)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x1fc, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [╜] (189)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x1f0, 0x30, 0x1f0, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [╛] (190)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f0, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [┐] (191)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3f, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [└] (192)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x1ff, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [┴] (193)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1ff, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [┬] (194)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3f, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [├] (195)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1ff, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [─] (196)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x1ff, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [┼] (197)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x3f, 0x30, 0x3f, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [╞] (198)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6f, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╟] (199)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6f, 0x60, 0x7f, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [╚] (200)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x60, 0x6f, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╔] (201)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x1ef, 0x00, 0x1ff, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [╩] (202)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1ff, 0x00, 0x1ef, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╦] (203)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6f, 0x60, 0x6f, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╠] (204)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1ff, 0x00, 0x1ff, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [═] (205)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x1ef, 0x00, 0x1ef, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╬] (206)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x1ff, 0x00, 0x1ff, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [╧] (207)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x1ff, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [╨] (208)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x1ff, 0x00, 0x1ff, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [╤] (209)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1ff, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╥] (210)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x7f, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [╙] (211)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x3f, 0x30, 0x3f, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [╘] (212)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x30, 0x3f, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [╒] (213)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╓] (214)
    {0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x1ff, 0x6c, 0x6c, 0x6c, 0x6c,
     0x6c, 0x6c},  // [╫] (215)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x1ff, 0x30, 0x1ff, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [╪] (216)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x1f0, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [┘] (217)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [┌] (218)
    {0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x1ff,
     0x1ff, 0x1ff, 0x1ff, 0x1ff},  // [█] (219)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1ff, 0x1ff, 0x1ff, 0x1ff,
     0x1ff, 0x1ff, 0x1ff},  // [▄] (220)
    {0x1e0, 0x1e0, 0x1e0, 0x1e0, 0x1e0, 0x1e0, 0x1e0, 0x1e0, 0x1e0, 0x1e0,
     0x1e0, 0x1e0, 0x1e0, 0x1e0},  // [▌] (221)
    {0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
     0x1f, 0x1f},  // [▐] (222)
    {0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x1ff, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00},  // [▀] (223)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xdc, 0xd8, 0xd8, 0xdc, 0x76, 0x00,
     0x00, 0x00},  // [α] (224)
    {0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xfc, 0xc6, 0xc6, 0xfc, 0xc0, 0xc0,
     0x40, 0x00},  // [ß] (225)
    {0x00, 0x00, 0xfe, 0xc6, 0xc6, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0x00,
     0x00, 0x00},  // [Γ] (226)
    {0x00, 0x00, 0x00, 0x00, 0xfe, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x00,
     0x00, 0x00},  // [π] (227)
    {0x00, 0x00, 0xfe, 0xc6, 0x60, 0x30, 0x18, 0x30, 0x60, 0xc6, 0xfe, 0x00,
     0x00, 0x00},  // [Σ] (228)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0xd8, 0xd8, 0xd8, 0xd8, 0x70, 0x00,
     0x00, 0x00},  // [σ] (229)
    {0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x60, 0x60, 0xc0,
     0x00, 0x00},  // [µ] (230)
    {0x00, 0x00, 0x00, 0x00, 0x76, 0xdc, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00,
     0x00, 0x00},  // [τ] (231)
    {0x00, 0x00, 0xfc, 0x30, 0x78, 0xcc, 0xcc, 0xcc, 0x78, 0x30, 0xfc, 0x00,
     0x00, 0x00},  // [Φ] (232)
    {0x00, 0x00, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0x6c, 0x38, 0x00,
     0x00, 0x00},  // [Θ] (233)
    {0x00, 0x00, 0x38, 0x6c, 0xc6, 0xc6, 0xc6, 0x6c, 0x6c, 0x6c, 0xee, 0x00,
     0x00, 0x00},  // [Ω] (234)
    {0x00, 0x00, 0x3c, 0x60, 0x30, 0x18, 0x7c, 0xcc, 0xcc, 0xcc, 0x78, 0x00,
     0x00, 0x00},  // [δ] (235)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x1b6, 0x1b6, 0xfc, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [∞] (236)
    {0x00, 0x00, 0x06, 0x0c, 0xfc, 0x1b6, 0x1b6, 0x1e6, 0xfc, 0xc0, 0x180, 0x00,
     0x00, 0x00},  // [φ] (237)
    {0x00, 0x00, 0x38, 0x60, 0xc0, 0xc0, 0xf8, 0xc0, 0xc0, 0x60, 0x38, 0x00,
     0x00, 0x00},  // [ε] (238)
    {0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x00,
     0x00, 0x00},  // [∩] (239)
    {0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0xfe, 0x00, 0x00, 0xfe, 0x00, 0x00,
     0x00, 0x00},  // [≡] (240)
    {0x00, 0x00, 0x30, 0x30, 0x30, 0x1fe, 0x30, 0x30, 0x30, 0x00, 0x1fe, 0x00,
     0x00, 0x00},  // [±] (241)
    {0x00, 0x00, 0x60, 0x30, 0x18, 0x0c, 0x18, 0x30, 0x60, 0x00, 0xfc, 0x00,
     0x00, 0x00},  // [≥] (242)
    {0x00, 0x00, 0x18, 0x30, 0x60, 0xc0, 0x60, 0x30, 0x18, 0x00, 0xfc, 0x00,
     0x00, 0x00},  // [≤] (243)
    {0x00, 0x00, 0x1c, 0x36, 0x36, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
     0x30, 0x30},  // [⌠] (244)
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x1b0, 0x1b0, 0xe0, 0x00,
     0x00, 0x00},  // [⌡] (245)
    {0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x1fe, 0x00, 0x00, 0x30, 0x30, 0x00,
     0x00, 0x00},  // [÷] (246)
    {0x00, 0x00, 0x00, 0x00, 0x76, 0xdc, 0x00, 0x76, 0xdc, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [≈] (247)
    {0x00, 0x70, 0xd8, 0xd8, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [°] (248)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [∙] (249)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [·] (250)
    {0x00, 0x1e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1d8, 0xd8, 0x78, 0x38, 0x00,
     0x00, 0x00},  // [√] (251)
    {0x00, 0x1b0, 0xd8, 0xd8, 0xd8, 0xd8, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [ⁿ] (252)
    {0x00, 0xe0, 0x1b0, 0x60, 0xc0, 0x190, 0x1f0, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [²] (253)
    {0x00, 0x00, 0x00, 0x00, 0x7c, 0x7c, 0x7c, 0x7c, 0x7c, 0x7c, 0x00, 0x00,
     0x00, 0x00},  // [■] (254)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00},  // [ ] (255)
};

// CGA 8x8 font bitmaps.
//
// From pcface project:
// https://github.com/susam/pcface/blob/main/out/oldschool-cga-8x8/fontlist.js
YAX86_PRIVATE const uint8_t kFontCGA8x8Bitmap[256][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // [] (0)
    {0x7e, 0x81, 0xa5, 0x81, 0xbd, 0x99, 0x81, 0x7e},  // [☺] (1)
    {0x7e, 0xff, 0xdb, 0xff, 0xc3, 0xe7, 0xff, 0x7e},  // [☻] (2)
    {0x6c, 0xfe, 0xfe, 0xfe, 0x7c, 0x38, 0x10, 0x00},  // [♥] (3)
    {0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x38, 0x10, 0x00},  // [♦] (4)
    {0x38, 0x7c, 0x38, 0xfe, 0xfe, 0xd6, 0x10, 0x38},  // [♣] (5)
    {0x10, 0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x10, 0x38},  // [♠] (6)
    {0x00, 0x00, 0x18, 0x3c, 0x3c, 0x18, 0x00, 0x00},  // [•] (7)
    {0xff, 0xff, 0xe7, 0xc3, 0xc3, 0xe7, 0xff, 0xff},  // [◘] (8)
    {0x00, 0x3c, 0x66, 0x42, 0x42, 0x66, 0x3c, 0x00},  // [○] (9)
    {0xff, 0xc3, 0x99, 0xbd, 0xbd, 0x99, 0xc3, 0xff},  // [◙] (10)
    {0x0f, 0x07, 0x0f, 0x7d, 0xcc, 0xcc, 0xcc, 0x78},  // [♂] (11)
    {0x3c, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x7e, 0x18},  // [♀] (12)
    {0x3f, 0x33, 0x3f, 0x30, 0x30, 0x70, 0xf0, 0xe0},  // [♪] (13)
    {0x7f, 0x63, 0x7f, 0x63, 0x63, 0x67, 0xe6, 0xc0},  // [♫] (14)
    {0x18, 0xdb, 0x3c, 0xe7, 0xe7, 0x3c, 0xdb, 0x18},  // [☼] (15)
    {0x80, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0x80, 0x00},  // [►] (16)
    {0x02, 0x0e, 0x3e, 0xfe, 0x3e, 0x0e, 0x02, 0x00},  // [◄] (17)
    {0x18, 0x3c, 0x7e, 0x18, 0x18, 0x7e, 0x3c, 0x18},  // [↕] (18)
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x66, 0x00},  // [‼] (19)
    {0x7f, 0xdb, 0xdb, 0x7b, 0x1b, 0x1b, 0x1b, 0x00},  // [¶] (20)
    {0x3e, 0x63, 0x38, 0x6c, 0x6c, 0x38, 0xcc, 0x78},  // [§] (21)
    {0x00, 0x00, 0x00, 0x00, 0x7e, 0x7e, 0x7e, 0x00},  // [▬] (22)
    {0x18, 0x3c, 0x7e, 0x18, 0x7e, 0x3c, 0x18, 0xff},  // [↨] (23)
    {0x18, 0x3c, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x00},  // [↑] (24)
    {0x18, 0x18, 0x18, 0x18, 0x7e, 0x3c, 0x18, 0x00},  // [↓] (25)
    {0x00, 0x18, 0x0c, 0xfe, 0x0c, 0x18, 0x00, 0x00},  // [→] (26)
    {0x00, 0x30, 0x60, 0xfe, 0x60, 0x30, 0x00, 0x00},  // [←] (27)
    {0x00, 0x00, 0xc0, 0xc0, 0xc0, 0xfe, 0x00, 0x00},  // [∟] (28)
    {0x00, 0x24, 0x66, 0xff, 0x66, 0x24, 0x00, 0x00},  // [↔] (29)
    {0x00, 0x18, 0x3c, 0x7e, 0xff, 0xff, 0x00, 0x00},  // [▲] (30)
    {0x00, 0xff, 0xff, 0x7e, 0x3c, 0x18, 0x00, 0x00},  // [▼] (31)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // [ ] (32)
    {0x30, 0x78, 0x78, 0x30, 0x30, 0x00, 0x30, 0x00},  // [!] (33)
    {0x6c, 0x6c, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00},  // ["] (34)
    {0x6c, 0x6c, 0xfe, 0x6c, 0xfe, 0x6c, 0x6c, 0x00},  // [#] (35)
    {0x30, 0x7c, 0xc0, 0x78, 0x0c, 0xf8, 0x30, 0x00},  // [$] (36)
    {0x00, 0xc6, 0xcc, 0x18, 0x30, 0x66, 0xc6, 0x00},  // [%] (37)
    {0x38, 0x6c, 0x38, 0x76, 0xdc, 0xcc, 0x76, 0x00},  // [&] (38)
    {0x60, 0x60, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00},  // ['] (39)
    {0x18, 0x30, 0x60, 0x60, 0x60, 0x30, 0x18, 0x00},  // [(] (40)
    {0x60, 0x30, 0x18, 0x18, 0x18, 0x30, 0x60, 0x00},  // [)] (41)
    {0x00, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x00, 0x00},  // [*] (42)
    {0x00, 0x30, 0x30, 0xfc, 0x30, 0x30, 0x00, 0x00},  // [+] (43)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x60},  // [,] (44)
    {0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00},  // [-] (45)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00},  // [.] (46)
    {0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x80, 0x00},  // [/] (47)
    {0x7c, 0xc6, 0xce, 0xde, 0xf6, 0xe6, 0x7c, 0x00},  // [0] (48)
    {0x30, 0x70, 0x30, 0x30, 0x30, 0x30, 0xfc, 0x00},  // [1] (49)
    {0x78, 0xcc, 0x0c, 0x38, 0x60, 0xcc, 0xfc, 0x00},  // [2] (50)
    {0x78, 0xcc, 0x0c, 0x38, 0x0c, 0xcc, 0x78, 0x00},  // [3] (51)
    {0x1c, 0x3c, 0x6c, 0xcc, 0xfe, 0x0c, 0x1e, 0x00},  // [4] (52)
    {0xfc, 0xc0, 0xf8, 0x0c, 0x0c, 0xcc, 0x78, 0x00},  // [5] (53)
    {0x38, 0x60, 0xc0, 0xf8, 0xcc, 0xcc, 0x78, 0x00},  // [6] (54)
    {0xfc, 0xcc, 0x0c, 0x18, 0x30, 0x30, 0x30, 0x00},  // [7] (55)
    {0x78, 0xcc, 0xcc, 0x78, 0xcc, 0xcc, 0x78, 0x00},  // [8] (56)
    {0x78, 0xcc, 0xcc, 0x7c, 0x0c, 0x18, 0x70, 0x00},  // [9] (57)
    {0x00, 0x30, 0x30, 0x00, 0x00, 0x30, 0x30, 0x00},  // [:] (58)
    {0x00, 0x30, 0x30, 0x00, 0x00, 0x30, 0x30, 0x60},  // [;] (59)
    {0x18, 0x30, 0x60, 0xc0, 0x60, 0x30, 0x18, 0x00},  // [<] (60)
    {0x00, 0x00, 0xfc, 0x00, 0x00, 0xfc, 0x00, 0x00},  // [=] (61)
    {0x60, 0x30, 0x18, 0x0c, 0x18, 0x30, 0x60, 0x00},  // [>] (62)
    {0x78, 0xcc, 0x0c, 0x18, 0x30, 0x00, 0x30, 0x00},  // [?] (63)
    {0x7c, 0xc6, 0xde, 0xde, 0xde, 0xc0, 0x78, 0x00},  // [@] (64)
    {0x30, 0x78, 0xcc, 0xcc, 0xfc, 0xcc, 0xcc, 0x00},  // [A] (65)
    {0xfc, 0x66, 0x66, 0x7c, 0x66, 0x66, 0xfc, 0x00},  // [B] (66)
    {0x3c, 0x66, 0xc0, 0xc0, 0xc0, 0x66, 0x3c, 0x00},  // [C] (67)
    {0xf8, 0x6c, 0x66, 0x66, 0x66, 0x6c, 0xf8, 0x00},  // [D] (68)
    {0xfe, 0x62, 0x68, 0x78, 0x68, 0x62, 0xfe, 0x00},  // [E] (69)
    {0xfe, 0x62, 0x68, 0x78, 0x68, 0x60, 0xf0, 0x00},  // [F] (70)
    {0x3c, 0x66, 0xc0, 0xc0, 0xce, 0x66, 0x3e, 0x00},  // [G] (71)
    {0xcc, 0xcc, 0xcc, 0xfc, 0xcc, 0xcc, 0xcc, 0x00},  // [H] (72)
    {0x78, 0x30, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00},  // [I] (73)
    {0x1e, 0x0c, 0x0c, 0x0c, 0xcc, 0xcc, 0x78, 0x00},  // [J] (74)
    {0xe6, 0x66, 0x6c, 0x78, 0x6c, 0x66, 0xe6, 0x00},  // [K] (75)
    {0xf0, 0x60, 0x60, 0x60, 0x62, 0x66, 0xfe, 0x00},  // [L] (76)
    {0xc6, 0xee, 0xfe, 0xfe, 0xd6, 0xc6, 0xc6, 0x00},  // [M] (77)
    {0xc6, 0xe6, 0xf6, 0xde, 0xce, 0xc6, 0xc6, 0x00},  // [N] (78)
    {0x38, 0x6c, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x00},  // [O] (79)
    {0xfc, 0x66, 0x66, 0x7c, 0x60, 0x60, 0xf0, 0x00},  // [P] (80)
    {0x78, 0xcc, 0xcc, 0xcc, 0xdc, 0x78, 0x1c, 0x00},  // [Q] (81)
    {0xfc, 0x66, 0x66, 0x7c, 0x6c, 0x66, 0xe6, 0x00},  // [R] (82)
    {0x78, 0xcc, 0x60, 0x30, 0x18, 0xcc, 0x78, 0x00},  // [S] (83)
    {0xfc, 0xb4, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00},  // [T] (84)
    {0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xfc, 0x00},  // [U] (85)
    {0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x78, 0x30, 0x00},  // [V] (86)
    {0xc6, 0xc6, 0xc6, 0xd6, 0xfe, 0xee, 0xc6, 0x00},  // [W] (87)
    {0xc6, 0xc6, 0x6c, 0x38, 0x38, 0x6c, 0xc6, 0x00},  // [X] (88)
    {0xcc, 0xcc, 0xcc, 0x78, 0x30, 0x30, 0x78, 0x00},  // [Y] (89)
    {0xfe, 0xc6, 0x8c, 0x18, 0x32, 0x66, 0xfe, 0x00},  // [Z] (90)
    {0x78, 0x60, 0x60, 0x60, 0x60, 0x60, 0x78, 0x00},  // [[] (91)
    {0xc0, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x02, 0x00},  // [\] (92)
    {0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x78, 0x00},  // []] (93)
    {0x10, 0x38, 0x6c, 0xc6, 0x00, 0x00, 0x00, 0x00},  // [^] (94)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff},  // [_] (95)
    {0x30, 0x30, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00},  // [`] (96)
    {0x00, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0x76, 0x00},  // [a] (97)
    {0xe0, 0x60, 0x60, 0x7c, 0x66, 0x66, 0xdc, 0x00},  // [b] (98)
    {0x00, 0x00, 0x78, 0xcc, 0xc0, 0xcc, 0x78, 0x00},  // [c] (99)
    {0x1c, 0x0c, 0x0c, 0x7c, 0xcc, 0xcc, 0x76, 0x00},  // [d] (100)
    {0x00, 0x00, 0x78, 0xcc, 0xfc, 0xc0, 0x78, 0x00},  // [e] (101)
    {0x38, 0x6c, 0x60, 0xf0, 0x60, 0x60, 0xf0, 0x00},  // [f] (102)
    {0x00, 0x00, 0x76, 0xcc, 0xcc, 0x7c, 0x0c, 0xf8},  // [g] (103)
    {0xe0, 0x60, 0x6c, 0x76, 0x66, 0x66, 0xe6, 0x00},  // [h] (104)
    {0x30, 0x00, 0x70, 0x30, 0x30, 0x30, 0x78, 0x00},  // [i] (105)
    {0x0c, 0x00, 0x0c, 0x0c, 0x0c, 0xcc, 0xcc, 0x78},  // [j] (106)
    {0xe0, 0x60, 0x66, 0x6c, 0x78, 0x6c, 0xe6, 0x00},  // [k] (107)
    {0x70, 0x30, 0x30, 0x30, 0x30, 0x30, 0x78, 0x00},  // [l] (108)
    {0x00, 0x00, 0xcc, 0xfe, 0xfe, 0xd6, 0xc6, 0x00},  // [m] (109)
    {0x00, 0x00, 0xf8, 0xcc, 0xcc, 0xcc, 0xcc, 0x00},  // [n] (110)
    {0x00, 0x00, 0x78, 0xcc, 0xcc, 0xcc, 0x78, 0x00},  // [o] (111)
    {0x00, 0x00, 0xdc, 0x66, 0x66, 0x7c, 0x60, 0xf0},  // [p] (112)
    {0x00, 0x00, 0x76, 0xcc, 0xcc, 0x7c, 0x0c, 0x1e},  // [q] (113)
    {0x00, 0x00, 0xdc, 0x76, 0x66, 0x60, 0xf0, 0x00},  // [r] (114)
    {0x00, 0x00, 0x7c, 0xc0, 0x78, 0x0c, 0xf8, 0x00},  // [s] (115)
    {0x10, 0x30, 0x7c, 0x30, 0x30, 0x34, 0x18, 0x00},  // [t] (116)
    {0x00, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00},  // [u] (117)
    {0x00, 0x00, 0xcc, 0xcc, 0xcc, 0x78, 0x30, 0x00},  // [v] (118)
    {0x00, 0x00, 0xc6, 0xd6, 0xfe, 0xfe, 0x6c, 0x00},  // [w] (119)
    {0x00, 0x00, 0xc6, 0x6c, 0x38, 0x6c, 0xc6, 0x00},  // [x] (120)
    {0x00, 0x00, 0xcc, 0xcc, 0xcc, 0x7c, 0x0c, 0xf8},  // [y] (121)
    {0x00, 0x00, 0xfc, 0x98, 0x30, 0x64, 0xfc, 0x00},  // [z] (122)
    {0x1c, 0x30, 0x30, 0xe0, 0x30, 0x30, 0x1c, 0x00},  // [{] (123)
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00},  // [|] (124)
    {0xe0, 0x30, 0x30, 0x1c, 0x30, 0x30, 0xe0, 0x00},  // [}] (125)
    {0x76, 0xdc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // [~] (126)
    {0x00, 0x10, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0x00},  // [⌂] (127)
    {0x78, 0xcc, 0xc0, 0xcc, 0x78, 0x18, 0x0c, 0x78},  // [Ç] (128)
    {0x00, 0xcc, 0x00, 0xcc, 0xcc, 0xcc, 0x7e, 0x00},  // [ü] (129)
    {0x1c, 0x00, 0x78, 0xcc, 0xfc, 0xc0, 0x78, 0x00},  // [é] (130)
    {0x7e, 0xc3, 0x3c, 0x06, 0x3e, 0x66, 0x3f, 0x00},  // [â] (131)
    {0xcc, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0x7e, 0x00},  // [ä] (132)
    {0xe0, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0x7e, 0x00},  // [à] (133)
    {0x30, 0x30, 0x78, 0x0c, 0x7c, 0xcc, 0x7e, 0x00},  // [å] (134)
    {0x00, 0x00, 0x78, 0xc0, 0xc0, 0x78, 0x0c, 0x38},  // [ç] (135)
    {0x7e, 0xc3, 0x3c, 0x66, 0x7e, 0x60, 0x3c, 0x00},  // [ê] (136)
    {0xcc, 0x00, 0x78, 0xcc, 0xfc, 0xc0, 0x78, 0x00},  // [ë] (137)
    {0xe0, 0x00, 0x78, 0xcc, 0xfc, 0xc0, 0x78, 0x00},  // [è] (138)
    {0xcc, 0x00, 0x70, 0x30, 0x30, 0x30, 0x78, 0x00},  // [ï] (139)
    {0x7c, 0xc6, 0x38, 0x18, 0x18, 0x18, 0x3c, 0x00},  // [î] (140)
    {0xe0, 0x00, 0x70, 0x30, 0x30, 0x30, 0x78, 0x00},  // [ì] (141)
    {0xc6, 0x38, 0x6c, 0xc6, 0xfe, 0xc6, 0xc6, 0x00},  // [Ä] (142)
    {0x30, 0x30, 0x00, 0x78, 0xcc, 0xfc, 0xcc, 0x00},  // [Å] (143)
    {0x1c, 0x00, 0xfc, 0x60, 0x78, 0x60, 0xfc, 0x00},  // [É] (144)
    {0x00, 0x00, 0x7f, 0x0c, 0x7f, 0xcc, 0x7f, 0x00},  // [æ] (145)
    {0x3e, 0x6c, 0xcc, 0xfe, 0xcc, 0xcc, 0xce, 0x00},  // [Æ] (146)
    {0x78, 0xcc, 0x00, 0x78, 0xcc, 0xcc, 0x78, 0x00},  // [ô] (147)
    {0x00, 0xcc, 0x00, 0x78, 0xcc, 0xcc, 0x78, 0x00},  // [ö] (148)
    {0x00, 0xe0, 0x00, 0x78, 0xcc, 0xcc, 0x78, 0x00},  // [ò] (149)
    {0x78, 0xcc, 0x00, 0xcc, 0xcc, 0xcc, 0x7e, 0x00},  // [û] (150)
    {0x00, 0xe0, 0x00, 0xcc, 0xcc, 0xcc, 0x7e, 0x00},  // [ù] (151)
    {0x00, 0xcc, 0x00, 0xcc, 0xcc, 0x7c, 0x0c, 0xf8},  // [ÿ] (152)
    {0xc3, 0x18, 0x3c, 0x66, 0x66, 0x3c, 0x18, 0x00},  // [Ö] (153)
    {0xcc, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0x78, 0x00},  // [Ü] (154)
    {0x18, 0x18, 0x7e, 0xc0, 0xc0, 0x7e, 0x18, 0x18},  // [¢] (155)
    {0x38, 0x6c, 0x64, 0xf0, 0x60, 0xe6, 0xfc, 0x00},  // [£] (156)
    {0xcc, 0xcc, 0x78, 0xfc, 0x30, 0xfc, 0x30, 0x30},  // [¥] (157)
    {0xf8, 0xcc, 0xcc, 0xfa, 0xc6, 0xcf, 0xc6, 0xc7},  // [₧] (158)
    {0x0e, 0x1b, 0x18, 0x3c, 0x18, 0x18, 0xd8, 0x70},  // [ƒ] (159)
    {0x1c, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0x7e, 0x00},  // [á] (160)
    {0x38, 0x00, 0x70, 0x30, 0x30, 0x30, 0x78, 0x00},  // [í] (161)
    {0x00, 0x1c, 0x00, 0x78, 0xcc, 0xcc, 0x78, 0x00},  // [ó] (162)
    {0x00, 0x1c, 0x00, 0xcc, 0xcc, 0xcc, 0x7e, 0x00},  // [ú] (163)
    {0x00, 0xf8, 0x00, 0xf8, 0xcc, 0xcc, 0xcc, 0x00},  // [ñ] (164)
    {0xfc, 0x00, 0xcc, 0xec, 0xfc, 0xdc, 0xcc, 0x00},  // [Ñ] (165)
    {0x3c, 0x6c, 0x6c, 0x3e, 0x00, 0x7e, 0x00, 0x00},  // [ª] (166)
    {0x38, 0x6c, 0x6c, 0x38, 0x00, 0x7c, 0x00, 0x00},  // [º] (167)
    {0x30, 0x00, 0x30, 0x60, 0xc0, 0xcc, 0x78, 0x00},  // [¿] (168)
    {0x00, 0x00, 0x00, 0xfc, 0xc0, 0xc0, 0x00, 0x00},  // [⌐] (169)
    {0x00, 0x00, 0x00, 0xfc, 0x0c, 0x0c, 0x00, 0x00},  // [¬] (170)
    {0xc3, 0xc6, 0xcc, 0xde, 0x33, 0x66, 0xcc, 0x0f},  // [½] (171)
    {0xc3, 0xc6, 0xcc, 0xdb, 0x37, 0x6f, 0xcf, 0x03},  // [¼] (172)
    {0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00},  // [¡] (173)
    {0x00, 0x33, 0x66, 0xcc, 0x66, 0x33, 0x00, 0x00},  // [«] (174)
    {0x00, 0xcc, 0x66, 0x33, 0x66, 0xcc, 0x00, 0x00},  // [»] (175)
    {0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88},  // [░] (176)
    {0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa},  // [▒] (177)
    {0xdb, 0x77, 0xdb, 0xee, 0xdb, 0x77, 0xdb, 0xee},  // [▓] (178)
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18},  // [│] (179)
    {0x18, 0x18, 0x18, 0x18, 0xf8, 0x18, 0x18, 0x18},  // [┤] (180)
    {0x18, 0x18, 0xf8, 0x18, 0xf8, 0x18, 0x18, 0x18},  // [╡] (181)
    {0x36, 0x36, 0x36, 0x36, 0xf6, 0x36, 0x36, 0x36},  // [╢] (182)
    {0x00, 0x00, 0x00, 0x00, 0xfe, 0x36, 0x36, 0x36},  // [╖] (183)
    {0x00, 0x00, 0xf8, 0x18, 0xf8, 0x18, 0x18, 0x18},  // [╕] (184)
    {0x36, 0x36, 0xf6, 0x06, 0xf6, 0x36, 0x36, 0x36},  // [╣] (185)
    {0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36},  // [║] (186)
    {0x00, 0x00, 0xfe, 0x06, 0xf6, 0x36, 0x36, 0x36},  // [╗] (187)
    {0x36, 0x36, 0xf6, 0x06, 0xfe, 0x00, 0x00, 0x00},  // [╝] (188)
    {0x36, 0x36, 0x36, 0x36, 0xfe, 0x00, 0x00, 0x00},  // [╜] (189)
    {0x18, 0x18, 0xf8, 0x18, 0xf8, 0x00, 0x00, 0x00},  // [╛] (190)
    {0x00, 0x00, 0x00, 0x00, 0xf8, 0x18, 0x18, 0x18},  // [┐] (191)
    {0x18, 0x18, 0x18, 0x18, 0x1f, 0x00, 0x00, 0x00},  // [└] (192)
    {0x18, 0x18, 0x18, 0x18, 0xff, 0x00, 0x00, 0x00},  // [┴] (193)
    {0x00, 0x00, 0x00, 0x00, 0xff, 0x18, 0x18, 0x18},  // [┬] (194)
    {0x18, 0x18, 0x18, 0x18, 0x1f, 0x18, 0x18, 0x18},  // [├] (195)
    {0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00},  // [─] (196)
    {0x18, 0x18, 0x18, 0x18, 0xff, 0x18, 0x18, 0x18},  // [┼] (197)
    {0x18, 0x18, 0x1f, 0x18, 0x1f, 0x18, 0x18, 0x18},  // [╞] (198)
    {0x36, 0x36, 0x36, 0x36, 0x37, 0x36, 0x36, 0x36},  // [╟] (199)
    {0x36, 0x36, 0x37, 0x30, 0x3f, 0x00, 0x00, 0x00},  // [╚] (200)
    {0x00, 0x00, 0x3f, 0x30, 0x37, 0x36, 0x36, 0x36},  // [╔] (201)
    {0x36, 0x36, 0xf7, 0x00, 0xff, 0x00, 0x00, 0x00},  // [╩] (202)
    {0x00, 0x00, 0xff, 0x00, 0xf7, 0x36, 0x36, 0x36},  // [╦] (203)
    {0x36, 0x36, 0x37, 0x30, 0x37, 0x36, 0x36, 0x36},  // [╠] (204)
    {0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00},  // [═] (205)
    {0x36, 0x36, 0xf7, 0x00, 0xf7, 0x36, 0x36, 0x36},  // [╬] (206)
    {0x18, 0x18, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00},  // [╧] (207)
    {0x36, 0x36, 0x36, 0x36, 0xff, 0x00, 0x00, 0x00},  // [╨] (208)
    {0x00, 0x00, 0xff, 0x00, 0xff, 0x18, 0x18, 0x18},  // [╤] (209)
    {0x00, 0x00, 0x00, 0x00, 0xff, 0x36, 0x36, 0x36},  // [╥] (210)
    {0x36, 0x36, 0x36, 0x36, 0x3f, 0x00, 0x00, 0x00},  // [╙] (211)
    {0x18, 0x18, 0x1f, 0x18, 0x1f, 0x00, 0x00, 0x00},  // [╘] (212)
    {0x00, 0x00, 0x1f, 0x18, 0x1f, 0x18, 0x18, 0x18},  // [╒] (213)
    {0x00, 0x00, 0x00, 0x00, 0x3f, 0x36, 0x36, 0x36},  // [╓] (214)
    {0x36, 0x36, 0x36, 0x36, 0xff, 0x36, 0x36, 0x36},  // [╫] (215)
    {0x18, 0x18, 0xff, 0x18, 0xff, 0x18, 0x18, 0x18},  // [╪] (216)
    {0x18, 0x18, 0x18, 0x18, 0xf8, 0x00, 0x00, 0x00},  // [┘] (217)
    {0x00, 0x00, 0x00, 0x00, 0x1f, 0x18, 0x18, 0x18},  // [┌] (218)
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},  // [█] (219)
    {0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff},  // [▄] (220)
    {0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0},  // [▌] (221)
    {0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f},  // [▐] (222)
    {0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00},  // [▀] (223)
    {0x00, 0x00, 0x76, 0xdc, 0xc8, 0xdc, 0x76, 0x00},  // [α] (224)
    {0x00, 0x78, 0xcc, 0xf8, 0xcc, 0xf8, 0xc0, 0xc0},  // [ß] (225)
    {0x00, 0xfc, 0xcc, 0xc0, 0xc0, 0xc0, 0xc0, 0x00},  // [Γ] (226)
    {0x00, 0xfe, 0x6c, 0x6c, 0x6c, 0x6c, 0x6c, 0x00},  // [π] (227)
    {0xfc, 0xcc, 0x60, 0x30, 0x60, 0xcc, 0xfc, 0x00},  // [Σ] (228)
    {0x00, 0x00, 0x7e, 0xd8, 0xd8, 0xd8, 0x70, 0x00},  // [σ] (229)
    {0x00, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x60, 0xc0},  // [µ] (230)
    {0x00, 0x76, 0xdc, 0x18, 0x18, 0x18, 0x18, 0x00},  // [τ] (231)
    {0xfc, 0x30, 0x78, 0xcc, 0xcc, 0x78, 0x30, 0xfc},  // [Φ] (232)
    {0x38, 0x6c, 0xc6, 0xfe, 0xc6, 0x6c, 0x38, 0x00},  // [Θ] (233)
    {0x38, 0x6c, 0xc6, 0xc6, 0x6c, 0x6c, 0xee, 0x00},  // [Ω] (234)
    {0x1c, 0x30, 0x18, 0x7c, 0xcc, 0xcc, 0x78, 0x00},  // [δ] (235)
    {0x00, 0x00, 0x7e, 0xdb, 0xdb, 0x7e, 0x00, 0x00},  // [∞] (236)
    {0x06, 0x0c, 0x7e, 0xdb, 0xdb, 0x7e, 0x60, 0xc0},  // [φ] (237)
    {0x38, 0x60, 0xc0, 0xf8, 0xc0, 0x60, 0x38, 0x00},  // [ε] (238)
    {0x78, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x00},  // [∩] (239)
    {0x00, 0xfc, 0x00, 0xfc, 0x00, 0xfc, 0x00, 0x00},  // [≡] (240)
    {0x30, 0x30, 0xfc, 0x30, 0x30, 0x00, 0xfc, 0x00},  // [±] (241)
    {0x60, 0x30, 0x18, 0x30, 0x60, 0x00, 0xfc, 0x00},  // [≥] (242)
    {0x18, 0x30, 0x60, 0x30, 0x18, 0x00, 0xfc, 0x00},  // [≤] (243)
    {0x0e, 0x1b, 0x1b, 0x18, 0x18, 0x18, 0x18, 0x18},  // [⌠] (244)
    {0x18, 0x18, 0x18, 0x18, 0x18, 0xd8, 0xd8, 0x70},  // [⌡] (245)
    {0x30, 0x30, 0x00, 0xfc, 0x00, 0x30, 0x30, 0x00},  // [÷] (246)
    {0x00, 0x76, 0xdc, 0x00, 0x76, 0xdc, 0x00, 0x00},  // [≈] (247)
    {0x38, 0x6c, 0x6c, 0x38, 0x00, 0x00, 0x00, 0x00},  // [°] (248)
    {0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00},  // [∙] (249)
    {0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00},  // [·] (250)
    {0x0f, 0x0c, 0x0c, 0x0c, 0xec, 0x6c, 0x3c, 0x1c},  // [√] (251)
    {0x78, 0x6c, 0x6c, 0x6c, 0x6c, 0x00, 0x00, 0x00},  // [ⁿ] (252)
    {0x70, 0x18, 0x30, 0x60, 0x78, 0x00, 0x00, 0x00},  // [²] (253)
    {0x00, 0x00, 0x3c, 0x3c, 0x3c, 0x3c, 0x00, 0x00},  // [■] (254)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // [ ] (255)
};


// ==============================================================================
// src/video/fonts.c end
// ==============================================================================

// ==============================================================================
// src/video/mda.c start
// ==============================================================================

#line 1 "./src/video/mda.c"
#ifndef YAX86_IMPLEMENTATION
#include "fonts.h"
#include "internal.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

enum {
  // Position of the underline within an MDA character cell.
  kMDAUnderlinePosition = 12,
  // Attribute values are only meaningful in these three-bit fields, so the
  // documented combinations are compared against them directly.
  kMDAAttributeNormal = 0x07,
  kMDAAttributeInverse = 0x00,
  kMDAAttributeUnderline = 0x01,
};

// Colors to draw a character cell with.
typedef struct MDACellColors {
  const RGB* foreground;
  const RGB* background;
  bool underline;
} MDACellColors;

// Decode an MDA attribute byte. We only support the officially documented
// combinations of values.
//
// Attribute byte structure:
//   - Bit 7: blink (0 = normal, 1 = blink)
//   - Bits 6-4: background
//   - Bit 3: intense foreground (0 = normal, 1 = intense)
//   - Bits 2-0: foreground
//
// Valid MDA character background and foreground attribute combinations:
//   - Normal: background = 000, foreground = 111
//   - Inverse video: background = 111, foreground = 000
//   - Invisible: background = 000, foreground = 000
//   - Underline: background = 000, foreground = 001
//
// Other combinations are undefined, but we will treat them as normal.
static MDACellColors MDADecodeAttribute(VideoState* video, uint8_t attr_value) {
  const VideoConfig* config = video->config;
  MDACellColors colors = {
      .foreground = &config->foreground,
      .background = &config->background,
      .underline = false,
  };

  bool intense = (attr_value & kVideoAttributeIntenseForeground) != 0;
  const RGB* intense_aware_foreground =
      intense ? &config->intense_foreground : &config->foreground;
  uint8_t background_attr = (attr_value & kVideoAttributeBackgroundMask) >>
                            kVideoAttributeBackgroundShift;
  uint8_t foreground_attr = attr_value & kVideoAttributeForegroundMask;

  if (background_attr == kMDAAttributeInverse &&
      foreground_attr == kMDAAttributeNormal) {
    // Normal video mode.
    colors.foreground = intense_aware_foreground;
  } else if (
      background_attr == kMDAAttributeNormal &&
      foreground_attr == kMDAAttributeInverse) {
    // Inverse video mode.
    colors.foreground = &config->background;
    colors.background = &config->foreground;
  } else if (
      background_attr == kMDAAttributeInverse &&
      foreground_attr == kMDAAttributeInverse) {
    // Invisible mode.
    colors.foreground = &config->background;
  } else if (
      background_attr == kMDAAttributeInverse &&
      foreground_attr == kMDAAttributeUnderline) {
    // Underline mode.
    colors.underline = true;
    colors.foreground = intense_aware_foreground;
  } else {
    // Other combinations are treated as normal.
    colors.foreground = intense_aware_foreground;
  }

  // Blinking characters alternate between the character and blank. The blink
  // attribute only applies if blinking is enabled in the mode control register.
  if ((attr_value & kVideoAttributeBlink) &&
      (video->control_register & kVideoControlEnableBlink) &&
      !VideoIsTextBlinkOn(video)) {
    colors.foreground = colors.background;
    colors.underline = false;
  }

  return colors;
}

// Write a character to display in MDA text mode. char_address is the address of
// the character's first byte in VRAM.
static void MDAWriteChar(
    VideoState* video, TextPosition char_pos, uint32_t char_address) {
  const VideoModeMetadata* metadata =
      &kVideoModeMetadata[kVideoModeMDAText80x25];
  uint8_t char_value = VideoReadVRAMByte(video, char_address);
  uint8_t attr_value = VideoReadVRAMByte(video, char_address + 1);
  const uint16_t* char_bitmap = kFontMDA9x14Bitmap[char_value];
  MDACellColors colors = MDADecodeAttribute(video, attr_value);

  Position origin_pixel_pos = {
      .x = char_pos.col * metadata->char_width,
      .y = char_pos.row * metadata->char_height,
  };
  for (uint8_t y = 0; y < metadata->char_height; ++y) {
    uint16_t row_bitmap;
    // If underline, set entire underline row to foreground color.
    if (y == kMDAUnderlinePosition && colors.underline) {
      row_bitmap = 0xFFFF;
    } else {
      row_bitmap = char_bitmap[y];
    }
    for (uint8_t x = 0; x < metadata->char_width; ++x) {
      Position pixel_pos = {
          .x = origin_pixel_pos.x + x,
          .y = origin_pixel_pos.y + y,
      };
      bool is_foreground =
          (row_bitmap & (1 << (metadata->char_width - 1 - x))) != 0;
      const RGB* pixel_rgb =
          is_foreground ? colors.foreground : colors.background;
      VideoWritePixel(video, pixel_pos, *pixel_rgb);
    }
  }
}

// Draw the text mode cursor over the character cell it occupies.
static void MDADrawCursor(VideoState* video, uint16_t start_address) {
  const VideoModeMetadata* metadata =
      &kVideoModeMetadata[kVideoModeMDAText80x25];
  // VideoIsCursorEnabled only distinguishes the 6845's "off" cursor mode from
  // the other three; it does not model the 1/16 and 1/32 field rate modes
  // separately. The actual toggling comes from VideoIsCursorBlinkOn, which
  // flips every 8 frames (see VideoTick), so the cursor is skipped entirely
  // during the off half of each blink cycle.
  if (!VideoIsCursorEnabled(video) || !VideoIsCursorBlinkOn(video)) {
    return;
  }
  // The cursor address (R14/R15) and start address (R12/R13) are both
  // character-unit VRAM addresses, so subtracting them gives the cursor's
  // position relative to the top-left of whatever is currently scrolled into
  // view. Both are independently wrapping 16-bit values, so the difference
  // is masked into the 2048-character VRAM space (4KB VRAM / 2 bytes per
  // character) rather than allowed to underflow or run past it.
  uint16_t cursor_offset = (VideoGetCursorAddress(video) - start_address) &
                           (metadata->vram_size / 2 - 1);
  uint16_t num_cells = (uint16_t)metadata->columns * metadata->rows;
  // The 80x25 grid only covers 2000 of VRAM's 2048 addressable characters;
  // if the cursor lands outside the visible grid it is legitimately
  // off-screen, so nothing is drawn.
  if (cursor_offset >= num_cells) {
    return;
  }

  // R10/R11 select the first and last scan line of the cell to highlight
  // (e.g. 11-12 of 0-13 for the default underline cursor). An out-of-range
  // start leaves nothing valid to draw; an out-of-range end is clamped to the
  // last scan line instead of discarding the whole cursor.
  uint8_t cursor_start = VideoGetCursorStartScanLine(video);
  uint8_t cursor_end = VideoGetCursorEndScanLine(video);
  if (cursor_start >= metadata->char_height) {
    return;
  }
  if (cursor_end >= metadata->char_height) {
    cursor_end = metadata->char_height - 1;
  }

  // Recover the (col, row) the linear cursor_offset represents, and scale up
  // to the pixel origin of that cell.
  uint16_t origin_x =
      (cursor_offset % metadata->columns) * metadata->char_width;
  uint16_t origin_y =
      (cursor_offset / metadata->columns) * metadata->char_height;
  // Paint a full-width band across the selected scan lines in the plain
  // foreground color - the cursor ignores the character's own attribute
  // byte.
  for (uint8_t y = cursor_start; y <= cursor_end; ++y) {
    for (uint8_t x = 0; x < metadata->char_width; ++x) {
      Position pixel_pos = {.x = origin_x + x, .y = origin_y + y};
      VideoWritePixel(video, pixel_pos, video->config->foreground);
    }
  }
}

// Render the current display in MDA text mode.
YAX86_PRIVATE void MDARenderScreen(VideoState* video) {
  const VideoModeMetadata* metadata =
      &kVideoModeMetadata[kVideoModeMDAText80x25];
  uint16_t start_address = VideoGetStartAddress(video);
  for (uint8_t row = 0; row < metadata->rows; ++row) {
    for (uint8_t col = 0; col < metadata->columns; ++col) {
      TextPosition char_pos = {.col = col, .row = row};
      // Each character takes 2 bytes (char + attr).
      uint32_t char_address =
          ((uint32_t)start_address + row * metadata->columns + col) * 2;
      char_address &= metadata->vram_size - 1;
      MDAWriteChar(video, char_pos, char_address);
    }
  }
  MDADrawCursor(video, start_address);
}


// ==============================================================================
// src/video/mda.c end
// ==============================================================================

// ==============================================================================
// src/video/cga.c start
// ==============================================================================

#line 1 "./src/video/cga.c"
#ifndef YAX86_IMPLEMENTATION
#include "fonts.h"
#include "internal.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

enum {
  // Number of pixels per byte in 320x200 graphics mode.
  kCGAPixelsPerByte320x200 = 4,
  // Number of bits per pixel in 320x200 graphics mode.
  kCGABitsPerPixel320x200 = 2,
  // Mask of a single pixel in 320x200 graphics mode.
  kCGAPixelMask320x200 = 0x03,
  // Number of pixels per byte in 640x200 graphics mode.
  kCGAPixelsPerByte640x200 = 8,
  // Mask of a graphics mode address within the half of VRAM holding either the
  // even or the odd scan lines.
  kCGAGraphicsScanLineAddressMask = kCGAGraphicsOddScanLineOffset - 1,
};

// Look up a color in the configured CGA palette.
static inline RGB CGAGetColor(const VideoState* video, uint8_t color) {
  return video->config->cga_palette[color & (kNumCGAColors - 1)];
}

// Write a pixel, expanding it horizontally to fill the frame buffer when the
// mode's horizontal resolution is lower than the frame buffer's. The CGA scans
// the same number of dots across the screen in every mode, so a 320 pixel wide
// mode is drawn with each pixel twice as wide.
static void CGAWritePixels(
    VideoState* video, uint16_t x, uint16_t y, uint8_t scale, RGB rgb) {
  for (uint8_t i = 0; i < scale; ++i) {
    Position pixel_pos = {.x = x + i, .y = y};
    VideoWritePixel(video, pixel_pos, rgb);
  }
}

// ============================================================================
// Text modes 0x00 - 0x03
// ============================================================================

// Write a character to display in a CGA text mode. char_address is the address
// of the character's first byte in VRAM, and scale is the horizontal pixel
// scaling factor for the mode.
static void CGAWriteChar(
    VideoState* video, const VideoModeMetadata* metadata, TextPosition char_pos,
    uint32_t char_address, uint8_t scale) {
  uint8_t char_value = VideoReadVRAMByte(video, char_address);
  uint8_t attr_value = VideoReadVRAMByte(video, char_address + 1);
  const uint8_t* char_bitmap = kFontCGA8x8Bitmap[char_value];

  uint8_t foreground_color = attr_value & (kVideoAttributeForegroundMask |
                                           kVideoAttributeIntenseForeground);
  uint8_t background_color = (attr_value & kVideoAttributeBackgroundMask) >>
                             kVideoAttributeBackgroundShift;
  bool blink_enabled =
      (video->control_register & kVideoControlEnableBlink) != 0;
  if (blink_enabled) {
    // Attribute bit 7 means blinking, so only eight background colors are
    // available.
    if ((attr_value & kVideoAttributeBlink) && !VideoIsTextBlinkOn(video)) {
      foreground_color = background_color;
    }
  } else {
    // Attribute bit 7 is the background intensity instead, giving all sixteen
    // background colors.
    if (attr_value & kVideoAttributeBlink) {
      background_color |= kNumCGAColors / 2;
    }
  }

  RGB foreground = CGAGetColor(video, foreground_color);
  RGB background = CGAGetColor(video, background_color);
  Position origin_pixel_pos = {
      .x = char_pos.col * metadata->char_width * scale,
      .y = char_pos.row * metadata->char_height,
  };
  for (uint8_t y = 0; y < metadata->char_height; ++y) {
    uint8_t row_bitmap = char_bitmap[y];
    for (uint8_t x = 0; x < metadata->char_width; ++x) {
      bool is_foreground =
          (row_bitmap & (1 << (metadata->char_width - 1 - x))) != 0;
      CGAWritePixels(
          video, origin_pixel_pos.x + x * scale, origin_pixel_pos.y + y, scale,
          is_foreground ? foreground : background);
    }
  }
}

// Draw the text mode cursor over the character cell it occupies.
static void CGADrawCursor(
    VideoState* video, const VideoModeMetadata* metadata,
    uint16_t start_address, uint8_t scale) {
  if (!VideoIsCursorEnabled(video) || !VideoIsCursorBlinkOn(video)) {
    return;
  }
  uint16_t cursor_offset = (VideoGetCursorAddress(video) - start_address) &
                           (metadata->vram_size / 2 - 1);
  uint16_t num_cells = (uint16_t)metadata->columns * metadata->rows;
  if (cursor_offset >= num_cells) {
    return;
  }

  uint8_t cursor_start = VideoGetCursorStartScanLine(video);
  uint8_t cursor_end = VideoGetCursorEndScanLine(video);
  if (cursor_start >= metadata->char_height) {
    return;
  }
  if (cursor_end >= metadata->char_height) {
    cursor_end = metadata->char_height - 1;
  }

  // The cursor is drawn in the foreground color of the cell it occupies.
  uint32_t char_address = ((uint32_t)start_address + cursor_offset) * 2;
  uint8_t attr_value = VideoReadVRAMByte(video, char_address + 1);
  RGB foreground = CGAGetColor(
      video, attr_value & (kVideoAttributeForegroundMask |
                           kVideoAttributeIntenseForeground));

  uint16_t origin_x =
      (cursor_offset % metadata->columns) * metadata->char_width * scale;
  uint16_t origin_y =
      (cursor_offset / metadata->columns) * metadata->char_height;
  for (uint8_t y = cursor_start; y <= cursor_end; ++y) {
    for (uint8_t x = 0; x < metadata->char_width; ++x) {
      CGAWritePixels(
          video, origin_x + x * scale, origin_y + y, scale, foreground);
    }
  }
}

// Render the current display in a CGA text mode.
static void CGARenderText(
    VideoState* video, const VideoModeMetadata* metadata) {
  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(video);
  uint8_t scale = (uint8_t)(adapter->frame_buffer_width / metadata->width);
  uint16_t start_address = VideoGetStartAddress(video);

  for (uint8_t row = 0; row < metadata->rows; ++row) {
    for (uint8_t col = 0; col < metadata->columns; ++col) {
      TextPosition char_pos = {.col = col, .row = row};
      // Each character takes 2 bytes (char + attr).
      uint32_t char_address =
          ((uint32_t)start_address + row * metadata->columns + col) * 2;
      CGAWriteChar(video, metadata, char_pos, char_address, scale);
    }
  }
  CGADrawCursor(video, metadata, start_address, scale);
}

// ============================================================================
// Graphics modes 0x04 - 0x06
// ============================================================================

// The three 320x200 graphics palettes, each holding colors 1 to 3. Color 0
// comes from the color select register instead. The palette is chosen by the
// palette bit of the color select register, except that the black and white bit
// of the mode control register overrides both.
static const uint8_t kCGAGraphicsPalettes[3][3] = {
    // Palette 0: green, red, brown
    {2, 4, 6},
    // Palette 1: cyan, magenta, light gray
    {3, 5, 7},
    // Black and white: cyan, red, light gray
    {3, 4, 7},
};

// Address of the first byte of a graphics mode scan line. Graphics VRAM is
// interleaved: even scan lines live in the first half of VRAM and odd scan
// lines in the second.
//
// The 6845 counts in character units, and a graphics mode fetches two bytes per
// unit, so the start address it holds is doubled to get a byte address. It also
// only addresses one half of VRAM - the scan line's parity picks the half - so
// it wraps within that half rather than across the whole of VRAM. The BIOS
// zeroes the start address on a mode set, so this only matters to software that
// page flips or scrolls by writing it directly.
static inline uint32_t CGAGetScanLineAddress(
    const VideoState* video, uint16_t y) {
  uint32_t address = ((uint32_t)VideoGetStartAddress(video) * 2 +
                      (uint32_t)(y / 2) * kCGAGraphicsBytesPerScanLine) &
                     kCGAGraphicsScanLineAddressMask;
  return address + (y & 1 ? kCGAGraphicsOddScanLineOffset : 0);
}

// Render the current display in 320x200 graphics mode.
static void CGARenderGraphics320x200(
    VideoState* video, const VideoModeMetadata* metadata) {
  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(video);
  uint8_t scale = (uint8_t)(adapter->frame_buffer_width / metadata->width);

  // Resolve the four palette entries once up front.
  const uint8_t* palette_colors;
  if (video->control_register & kVideoControlBlackAndWhite) {
    palette_colors = kCGAGraphicsPalettes[2];
  } else if (video->color_select_register & kCGAColorSelectPalette) {
    palette_colors = kCGAGraphicsPalettes[1];
  } else {
    palette_colors = kCGAGraphicsPalettes[0];
  }
  uint8_t intensity =
      (video->color_select_register & kCGAColorSelectPaletteIntensity)
          ? kNumCGAColors / 2
          : 0;
  RGB palette[4];
  palette[0] = CGAGetColor(
      video, video->color_select_register &
                 (kCGAColorSelectColorMask | kCGAColorSelectIntensity));
  for (uint8_t i = 0; i < 3; ++i) {
    palette[i + 1] = CGAGetColor(video, palette_colors[i] | intensity);
  }

  // Each VRAM byte holds several pixels, so the loop runs over bytes and
  // unpacks each one, rather than re-reading the same byte once per pixel.
  uint16_t bytes_per_scan_line = metadata->width / kCGAPixelsPerByte320x200;
  for (uint16_t y = 0; y < metadata->height; ++y) {
    uint32_t scan_line_address = CGAGetScanLineAddress(video, y);
    for (uint16_t byte_index = 0; byte_index < bytes_per_scan_line;
         ++byte_index) {
      uint8_t byte_value =
          VideoReadVRAMByte(video, scan_line_address + byte_index);
      uint16_t first_x = byte_index * kCGAPixelsPerByte320x200;
      for (uint8_t i = 0; i < kCGAPixelsPerByte320x200; ++i) {
        // The leftmost pixel is in the most significant bits.
        uint8_t shift = (uint8_t)((kCGAPixelsPerByte320x200 - 1 - i) *
                                  kCGABitsPerPixel320x200);
        uint8_t color = (byte_value >> shift) & kCGAPixelMask320x200;
        CGAWritePixels(video, (first_x + i) * scale, y, scale, palette[color]);
      }
    }
  }
}

// Render the current display in 640x200 graphics mode.
static void CGARenderGraphics640x200(
    VideoState* video, const VideoModeMetadata* metadata) {
  // The foreground color comes from the color select register, and the
  // background is always black.
  RGB foreground = CGAGetColor(
      video, video->color_select_register &
                 (kCGAColorSelectColorMask | kCGAColorSelectIntensity));
  RGB background = CGAGetColor(video, 0);

  // As in 320x200 mode, each VRAM byte is fetched once and unpacked into the
  // eight pixels it holds.
  uint16_t bytes_per_scan_line = metadata->width / kCGAPixelsPerByte640x200;
  for (uint16_t y = 0; y < metadata->height; ++y) {
    uint32_t scan_line_address = CGAGetScanLineAddress(video, y);
    for (uint16_t byte_index = 0; byte_index < bytes_per_scan_line;
         ++byte_index) {
      uint8_t byte_value =
          VideoReadVRAMByte(video, scan_line_address + byte_index);
      uint16_t first_x = byte_index * kCGAPixelsPerByte640x200;
      for (uint8_t i = 0; i < kCGAPixelsPerByte640x200; ++i) {
        // The leftmost pixel is in the most significant bit.
        uint8_t shift = (uint8_t)(kCGAPixelsPerByte640x200 - 1 - i);
        bool is_foreground = (byte_value >> shift) & 1;
        Position pixel_pos = {.x = first_x + i, .y = y};
        VideoWritePixel(
            video, pixel_pos, is_foreground ? foreground : background);
      }
    }
  }
}

// Render the current display on the CGA.
YAX86_PRIVATE void CGARenderScreen(VideoState* video) {
  const VideoModeMetadata* metadata = VideoGetModeMetadata(video);
  switch (metadata->mode) {
    case kVideoModeCGAText40x25Mono:
    case kVideoModeCGAText40x25Color:
    case kVideoModeCGAText80x25Mono:
    case kVideoModeCGAText80x25Color:
      CGARenderText(video, metadata);
      break;
    case kVideoModeCGAGraphics320x200:
    case kVideoModeCGAGraphics320x200Alt:
      CGARenderGraphics320x200(video, metadata);
      break;
    case kVideoModeCGAGraphics640x200:
      CGARenderGraphics640x200(video, metadata);
      break;
    default:
      // Not reachable - VideoGetMode() only ever returns a CGA mode on the CGA.
      // The branch exists so that the switch covers the enum.
      break;
  }
}


// ==============================================================================
// src/video/cga.c end
// ==============================================================================

// ==============================================================================
// src/video/video.c start
// ==============================================================================

#line 1 "./src/video/video.c"
#ifndef YAX86_IMPLEMENTATION
#include "internal.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Power-on 6845 register values for the IBM Monochrome Display.
static const uint8_t kDefaultMDARegisters[kNumCRTCRegisters] = {
    0x61, 0x50, 0x52, 0x0F, 0x19, 0x06, 0x19, 0x19, 0x02,
    0x0D, 0x0B, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// Power-on 6845 register values for the CGA in 80x25 text mode, matching the
// values GLaBIOS programs for that mode.
static const uint8_t kDefaultCGARegisters[kNumCRTCRegisters] = {
    0x71, 0x50, 0x5A, 0x0A, 0x1F, 0x06, 0x19, 0x1C, 0x02,
    0x07, 0x06, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

enum {
  // Value of bits 6-5 of the cursor start register that disables the cursor.
  kCRTCCursorDisabled = 0x20,
  // Mask of bits 6-5 of the cursor start register.
  kCRTCCursorModeMask = 0x60,
  // Mask of the scan line select bits in the cursor start and end registers.
  kCRTCCursorScanLineMask = 0x1F,

  // The 6845 latches only the low five bits of the register index.
  kCRTCRegisterIndexMask = 0x1F,

  // Value returned for reads that decode to nothing.
  kVideoUnmappedPortValue = 0xFF,
};

const VideoAdapterMetadata* VideoGetAdapterMetadata(const VideoState* video) {
  return &kVideoAdapterMetadata[video->adapter];
}

// ============================================================================
// Video RAM
// ============================================================================

YAX86_PRIVATE uint8_t VideoReadVRAMByte(VideoState* video, uint32_t address) {
  if (!video->config || !video->config->read_vram_byte) {
    return kVideoUnmappedPortValue;
  }
  // VRAM is aliased throughout the adapter's window, so an address past the end
  // wraps around rather than reading nothing. The renderer relies on this when
  // the 6845 start address pushes it past the end of VRAM.
  //
  // Every adapter's VRAM size is a power of two, so the wrap is a mask rather
  // than a remainder. The size is only known at run time, so a remainder would
  // compile to a hardware divide in the middle of the render loop - and the
  // Cortex-M0+ this targets has no divide instruction at all.
  uint32_t vram_size = VideoGetAdapterMetadata(video)->vram_size;
  return video->config->read_vram_byte(video, address & (vram_size - 1));
}

YAX86_PRIVATE void VideoWriteVRAMByte(
    VideoState* video, uint32_t address, uint8_t value) {
  if (!video->config || !video->config->write_vram_byte) {
    return;
  }
  uint32_t vram_size = VideoGetAdapterMetadata(video)->vram_size;
  video->config->write_vram_byte(video, address & (vram_size - 1), value);
}

YAX86_PRIVATE void VideoWritePixel(
    VideoState* video, Position position, RGB rgb) {
  if (!video->config || !video->config->write_pixel) {
    return;
  }
  video->config->write_pixel(video, position, rgb);
}

uint8_t VideoReadVRAM(VideoState* video, uint32_t address) {
  if (address >= VideoGetAdapterMetadata(video)->vram_size) {
    return kVideoUnmappedPortValue;
  }
  return VideoReadVRAMByte(video, address);
}

void VideoWriteVRAM(VideoState* video, uint32_t address, uint8_t value) {
  if (address >= VideoGetAdapterMetadata(video)->vram_size) {
    return;
  }
  VideoWriteVRAMByte(video, address, value);
}

// ============================================================================
// Initialization
// ============================================================================

void VideoInit(VideoState* video, VideoConfig* config) {
  static const VideoState kEmptyVideoState = {0};
  *video = kEmptyVideoState;
  video->config = config;
  video->adapter = config && config->adapter == kVideoAdapterCGA
                       ? kVideoAdapterCGA
                       : kVideoAdapterMDA;

  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(video);
  const uint8_t* default_registers = video->adapter == kVideoAdapterCGA
                                         ? kDefaultCGARegisters
                                         : kDefaultMDARegisters;
  for (uint8_t i = 0; i < kNumCRTCRegisters; ++i) {
    video->registers[i] = default_registers[i];
  }
  video->control_register = adapter->default_control_register;

  for (uint32_t i = 0; i < adapter->vram_size; i += 2) {
    VideoWriteVRAMByte(video, i, ' ');
    VideoWriteVRAMByte(video, i + 1, 0x07 /* default attr */);
  }
}

// ============================================================================
// Video mode
// ============================================================================

VideoMode VideoGetMode(const VideoState* video) {
  if (video->adapter == kVideoAdapterMDA) {
    // The MDA has only one mode.
    return kVideoModeMDAText80x25;
  }
  // Derive the CGA mode from the mode control register, matching the values
  // the BIOS writes for each of its INT 10h modes.
  //
  // The bits are tested in the order a real card resolves them: the high
  // resolution bit wins over the graphics bits, and the graphics bit over the
  // high resolution graphics bit. The BIOS never writes a combination where
  // the order matters, but software that ORs a bit into a saved mode byte can,
  // and this is the order 86Box's renderer decodes them in.
  if (video->control_register & kVideoControlHighResolution) {
    return video->control_register & kVideoControlBlackAndWhite
               ? kVideoModeCGAText80x25Mono
               : kVideoModeCGAText80x25Color;
  }
  if (!(video->control_register & kVideoControlGraphics)) {
    return video->control_register & kVideoControlBlackAndWhite
               ? kVideoModeCGAText40x25Mono
               : kVideoModeCGAText40x25Color;
  }
  if (video->control_register & kVideoControlHighResolutionGraphics) {
    return kVideoModeCGAGraphics640x200;
  }
  return video->control_register & kVideoControlBlackAndWhite
             ? kVideoModeCGAGraphics320x200Alt
             : kVideoModeCGAGraphics320x200;
}

const VideoModeMetadata* VideoGetModeMetadata(const VideoState* video) {
  return &kVideoModeMetadata[VideoGetMode(video)];
}

// ============================================================================
// 6845 CRT controller
// ============================================================================

YAX86_PRIVATE uint16_t VideoGetStartAddress(const VideoState* video) {
  return (uint16_t)(video->registers[kCRTCRegisterStartAddressH] << 8) |
         video->registers[kCRTCRegisterStartAddressL];
}

YAX86_PRIVATE uint16_t VideoGetCursorAddress(const VideoState* video) {
  return (uint16_t)(video->registers[kCRTCRegisterCursorH] << 8) |
         video->registers[kCRTCRegisterCursorL];
}

YAX86_PRIVATE bool VideoIsCursorEnabled(const VideoState* video) {
  return (video->registers[kCRTCRegisterCursorStart] & kCRTCCursorModeMask) !=
         kCRTCCursorDisabled;
}

YAX86_PRIVATE uint8_t VideoGetCursorStartScanLine(const VideoState* video) {
  return video->registers[kCRTCRegisterCursorStart] & kCRTCCursorScanLineMask;
}

YAX86_PRIVATE uint8_t VideoGetCursorEndScanLine(const VideoState* video) {
  return video->registers[kCRTCRegisterCursorEnd] & kCRTCCursorScanLineMask;
}

YAX86_PRIVATE bool VideoIsCursorBlinkOn(const VideoState* video) {
  return (video->frames / kVideoFramesPerCursorBlinkPhase) % 2 == 0;
}

YAX86_PRIVATE bool VideoIsTextBlinkOn(const VideoState* video) {
  return (video->frames / kVideoFramesPerTextBlinkPhase) % 2 == 0;
}

// ============================================================================
// Retrace timing
// ============================================================================

// Advances a software model of the CRT beam position by the given number of
// CPU cycles. This is what status port reads (below) derive retrace timing
// from.
//
// The beam advances one scan line every kMDACyclesPerScanLine cycles, but
// callers report cycles in whatever amount the CPU just consumed, which
// rarely divides evenly. scan_line_cycles banks the remainder as credit
// toward the next scan line; the loop pays it off a scan line at a time,
// looping rather than branching once since a single call can be worth more
// than one scan line. scan_line wraps at the adapter's scan_lines_per_frame,
// incrementing frames, which VideoIsCursorBlinkOn() and VideoIsTextBlinkOn()
// use to derive their blink phases.
void VideoTick(VideoState* video, uint16_t cycles) {
  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(video);
  video->scan_line_cycles += cycles;
  while (video->scan_line_cycles >= adapter->cycles_per_scan_line) {
    video->scan_line_cycles -= adapter->cycles_per_scan_line;
    if (++video->scan_line >= adapter->scan_lines_per_frame) {
      video->scan_line = 0;
      ++video->frames;
    }
  }
}

// The value the status port reads back, computed from where the CRT beam
// currently is.
static uint8_t VideoGetStatus(const VideoState* video) {
  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(video);
  bool in_vertical_retrace = video->scan_line >= adapter->displayed_scan_lines;
  bool in_horizontal_retrace =
      video->scan_line_cycles >= adapter->display_cycles_per_scan_line;

  // No light pen is emulated, so its switch always reads as off.
  uint8_t status = kVideoStatusLightPenSwitchOff;
  if (in_vertical_retrace || in_horizontal_retrace) {
    status |= kVideoStatusDisplayDisabled;
  }
  if (in_vertical_retrace) {
    status |= kVideoStatusVerticalRetrace;
  }
  // Note that the unused high bits are deliberately left clear rather than
  // floating high as they do on a real card: GLaBIOS probes bit 7 of the MDA
  // status port to detect a Hercules adapter.
  return status;
}

// ============================================================================
// I/O ports
// ============================================================================

// What an I/O port in the adapter's range does.
typedef enum VideoPortFunction {
  // Port is not decoded by the adapter.
  kVideoPortNone = 0,
  // 6845 register index.
  kVideoPortRegisterIndex,
  // 6845 register data.
  kVideoPortRegisterData,
  // Mode control register.
  kVideoPortControl,
  // CGA color select register.
  kVideoPortColorSelect,
  // Status register.
  kVideoPortStatus,
} VideoPortFunction;

// Decode an I/O port within the adapter's range.
static VideoPortFunction VideoDecodePort(
    const VideoState* video, uint16_t port) {
  if (video->adapter == kVideoAdapterMDA) {
    switch (port) {
      case kMDAPortRegisterIndex:
        return kVideoPortRegisterIndex;
      case kMDAPortRegisterData:
        return kVideoPortRegisterData;
      case kMDAPortControl:
        return kVideoPortControl;
      case kMDAPortStatus:
        return kVideoPortStatus;
      default:
        return kVideoPortNone;
    }
  }

  // The CGA only decodes the low four address bits, so the 6845 index and data
  // registers are aliased across 3D0 to 3D7.
  uint16_t offset = port - kCGAPortStart;
  if (offset < 8) {
    return offset & 1 ? kVideoPortRegisterData : kVideoPortRegisterIndex;
  }
  switch (port) {
    case kCGAPortControl:
      return kVideoPortControl;
    case kCGAPortColorSelect:
      return kVideoPortColorSelect;
    case kCGAPortStatus:
      return kVideoPortStatus;
    default:
      // The light pen strobe ports respond but do nothing, as no light pen is
      // emulated.
      return kVideoPortNone;
  }
}

uint8_t VideoReadPort(VideoState* video, uint16_t port) {
  switch (VideoDecodePort(video, port)) {
    case kVideoPortRegisterIndex:
      return video->selected_register;
    case kVideoPortRegisterData:
      if (video->selected_register < kNumCRTCRegisters) {
        return video->registers[video->selected_register];
      }
      return kVideoUnmappedPortValue;
    case kVideoPortControl:
      return video->control_register;
    case kVideoPortColorSelect:
      return video->color_select_register;
    case kVideoPortStatus:
      return VideoGetStatus(video);
    default:
      return kVideoUnmappedPortValue;
  }
}

void VideoWritePort(VideoState* video, uint16_t port, uint8_t value) {
  switch (VideoDecodePort(video, port)) {
    case kVideoPortRegisterIndex:
      video->selected_register = value & kCRTCRegisterIndexMask;
      break;
    case kVideoPortRegisterData:
      if (video->selected_register < kNumCRTCRegisters) {
        video->registers[video->selected_register] = value;
      }
      break;
    case kVideoPortControl:
      video->control_register = value;
      break;
    case kVideoPortColorSelect:
      video->color_select_register = value;
      break;
    default:
      // The status port is read only.
      break;
  }
}

// ============================================================================
// Rendering
// ============================================================================

void VideoRender(VideoState* video) {
  if (!video->config || !video->config->write_pixel) {
    return;
  }

  if (!(video->control_register & kVideoControlVideoEnable)) {
    // The video signal is disabled, so the display is blank. The BIOS leaves it
    // this way while it reprograms the 6845.
    const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(video);
    RGB blank = video->adapter == kVideoAdapterCGA
                    ? video->config->cga_palette[0]
                    : video->config->background;
    for (uint16_t y = 0; y < adapter->frame_buffer_height; ++y) {
      for (uint16_t x = 0; x < adapter->frame_buffer_width; ++x) {
        Position pixel_pos = {.x = x, .y = y};
        VideoWritePixel(video, pixel_pos, blank);
      }
    }
    return;
  }

  if (video->adapter == kVideoAdapterCGA) {
    CGARenderScreen(video);
  } else {
    MDARenderScreen(video);
  }
}


// ==============================================================================
// src/video/video.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_VIDEO_BUNDLE_H

