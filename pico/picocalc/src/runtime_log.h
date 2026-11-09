// The runtime's logging: the configuration it shares with the machine, a
// logger for each core, the runtime's own log modules, and a macro for each
// module and level.
//
// Everything the runtime and the machine say goes out over USB serial as one
// logfmt line each - see "util — logging" in AGENTS.md. A line written while
// no host is attached is dropped, as USB CDC would drop it anyway.
#ifndef YAX86_PICOCALC_RUNTIME_LOG_H
#define YAX86_PICOCALC_RUNTIME_LOG_H

#include "core/platform.h"

// What the machine logs through, on both cores: errors only. The core's
// warnings - a read from an unmapped port, for one - come in numbers during a
// boot, and writing them out would put serial I/O in the time being measured.
extern YLoggerConfig g_machine_log_config;

// What the runtime logs through on core 0, down to info, which is where its
// reports and the boot's progress are. It has no source, so its lines carry
// no tick or CS:IP: what the runtime says is about the runtime, not about an
// instruction the guest was running.
extern YLogger g_runtime_logger;

// What core 1 logs through, with the machine's configuration: the copy of the
// guest's screen it renders, and the screenshot it sends, whose replies are
// never filtered. A logger builds each line in a buffer of its own, so the
// two cores cannot share one.
extern YLogger g_render_logger;

extern const YLogModule kSystemLogModule;
extern const YLogModule kBootLogModule;
extern const YLogModule kDisplayLogModule;
extern const YLogModule kDiskLogModule;
extern const YLogModule kKeyboardLogModule;
extern const YLogModule kPacingLogModule;

// Fill in both configurations and bring up both loggers. Call before
// anything logs.
void RuntimeLogInit(void);

#define RUNTIME_LOG(module, level, ...) \
  Y_LOG(&g_runtime_logger, &(module), (level), __VA_ARGS__)

#define SYSTEM_ERROR(...) \
  RUNTIME_LOG(kSystemLogModule, Y_LOG_LEVEL_ERROR, __VA_ARGS__)
#define SYSTEM_WARN(...) \
  RUNTIME_LOG(kSystemLogModule, Y_LOG_LEVEL_WARN, __VA_ARGS__)
#define SYSTEM_INFO(...) \
  RUNTIME_LOG(kSystemLogModule, Y_LOG_LEVEL_INFO, __VA_ARGS__)
#define SYSTEM_DEBUG(...) \
  RUNTIME_LOG(kSystemLogModule, Y_LOG_LEVEL_DEBUG, __VA_ARGS__)

#define BOOT_ERROR(...) \
  RUNTIME_LOG(kBootLogModule, Y_LOG_LEVEL_ERROR, __VA_ARGS__)
#define BOOT_WARN(...) \
  RUNTIME_LOG(kBootLogModule, Y_LOG_LEVEL_WARN, __VA_ARGS__)
#define BOOT_INFO(...) \
  RUNTIME_LOG(kBootLogModule, Y_LOG_LEVEL_INFO, __VA_ARGS__)
#define BOOT_DEBUG(...) \
  RUNTIME_LOG(kBootLogModule, Y_LOG_LEVEL_DEBUG, __VA_ARGS__)

#define DISPLAY_ERROR(...) \
  RUNTIME_LOG(kDisplayLogModule, Y_LOG_LEVEL_ERROR, __VA_ARGS__)
#define DISPLAY_WARN(...) \
  RUNTIME_LOG(kDisplayLogModule, Y_LOG_LEVEL_WARN, __VA_ARGS__)
#define DISPLAY_INFO(...) \
  RUNTIME_LOG(kDisplayLogModule, Y_LOG_LEVEL_INFO, __VA_ARGS__)
#define DISPLAY_DEBUG(...) \
  RUNTIME_LOG(kDisplayLogModule, Y_LOG_LEVEL_DEBUG, __VA_ARGS__)

#define DISK_ERROR(...) \
  RUNTIME_LOG(kDiskLogModule, Y_LOG_LEVEL_ERROR, __VA_ARGS__)
#define DISK_WARN(...) \
  RUNTIME_LOG(kDiskLogModule, Y_LOG_LEVEL_WARN, __VA_ARGS__)
#define DISK_INFO(...) \
  RUNTIME_LOG(kDiskLogModule, Y_LOG_LEVEL_INFO, __VA_ARGS__)
#define DISK_DEBUG(...) \
  RUNTIME_LOG(kDiskLogModule, Y_LOG_LEVEL_DEBUG, __VA_ARGS__)

#define KEYBOARD_ERROR(...) \
  RUNTIME_LOG(kKeyboardLogModule, Y_LOG_LEVEL_ERROR, __VA_ARGS__)
#define KEYBOARD_WARN(...) \
  RUNTIME_LOG(kKeyboardLogModule, Y_LOG_LEVEL_WARN, __VA_ARGS__)
#define KEYBOARD_INFO(...) \
  RUNTIME_LOG(kKeyboardLogModule, Y_LOG_LEVEL_INFO, __VA_ARGS__)
#define KEYBOARD_DEBUG(...) \
  RUNTIME_LOG(kKeyboardLogModule, Y_LOG_LEVEL_DEBUG, __VA_ARGS__)

#define PACING_ERROR(...) \
  RUNTIME_LOG(kPacingLogModule, Y_LOG_LEVEL_ERROR, __VA_ARGS__)
#define PACING_WARN(...) \
  RUNTIME_LOG(kPacingLogModule, Y_LOG_LEVEL_WARN, __VA_ARGS__)
#define PACING_INFO(...) \
  RUNTIME_LOG(kPacingLogModule, Y_LOG_LEVEL_INFO, __VA_ARGS__)
#define PACING_DEBUG(...) \
  RUNTIME_LOG(kPacingLogModule, Y_LOG_LEVEL_DEBUG, __VA_ARGS__)

#endif  // YAX86_PICOCALC_RUNTIME_LOG_H
