#include "runtime_log.h"

#include <stdio.h>

#include "pico/stdio_usb.h"

enum {
  // The runtime's modules take the IDs the core leaves for hosts.
  kSystemLogModuleID = Y_LOG_FIRST_HOST_MODULE_ID,
  kBootLogModuleID,
  kDisplayLogModuleID,
  kDiskLogModuleID,
  kKeyboardLogModuleID,
  kPacingLogModuleID,
};

const YLogModule kSystemLogModule = {kSystemLogModuleID, "system"};
const YLogModule kBootLogModule = {kBootLogModuleID, "boot"};
const YLogModule kDisplayLogModule = {kDisplayLogModuleID, "display"};
const YLogModule kDiskLogModule = {kDiskLogModuleID, "disk"};
const YLogModule kKeyboardLogModule = {kKeyboardLogModuleID, "keyboard"};
const YLogModule kPacingLogModule = {kPacingLogModuleID, "pacing"};

YLoggerConfig g_machine_log_config;
static YLoggerConfig g_runtime_log_config;
YLogger g_runtime_logger;
YLogger g_render_logger;

// The SDK's stdio takes a lock around each write, so the two cores' lines
// come out whole rather than interleaved.
static void WriteLine(
    Y_UNUSED void* context, Y_UNUSED YLogKind kind,
    Y_UNUSED const YLogModule* module, Y_UNUSED YLogLevel level,
    const char* line, Y_UNUSED size_t length) {
  if (stdio_usb_connected()) {
    printf("%s\n", line);
  }
}

void RuntimeLogInit(void) {
  g_machine_log_config.write_line = WriteLine;
  g_machine_log_config.enabled_modules = 0xFFFFFFFF;
  g_machine_log_config.min_level = Y_LOG_LEVEL_ERROR;
  g_runtime_log_config.write_line = WriteLine;
  g_runtime_log_config.enabled_modules = 0xFFFFFFFF;
  g_runtime_log_config.min_level = Y_LOG_LEVEL_INFO;
  YLoggerInit(&g_runtime_logger, &g_runtime_log_config);
  YLoggerInit(&g_render_logger, &g_machine_log_config);
}
