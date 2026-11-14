#include "debug_command.h"

#include <stddef.h>

#include "display.h"
#include "hardware/clocks.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "psram.h"
#include "sdcard.h"
#include "southbridge.h"
#include "speaker.h"
#include "stack_usage.h"
#include "typing.h"
#include "usb_storage.h"

enum {
  kHzPerKHz = 1000,
  // The longest line taken, its terminating null byte included. Longer ones
  // are refused whole.
  kMaxLineLength = 256,
  // The most bytes taken in one call, so that a host sending a lot at once
  // cannot hold a batch up for long.
  kMaxBytesPerPoll = 64,
};

// What initializing the system found, main.c's.
static const InitStatus* g_init_status;
// The line being read, and whether it has outgrown the buffer.
static char g_line[kMaxLineLength];
static uint16_t g_length;
static bool g_is_too_long;

static bool AreStringsEqual(const char* a, const char* b) {
  while (*a != '\0' && *a == *b) {
    ++a;
    ++b;
  }
  return *a == *b;
}

// Answer a line that is not a command. Its id is unknown, so the reply has
// none.
static void ReplyToBadLine(const char* error) {
  Y_LOG_REPLY(
      &g_runtime_logger, 0, "", Y_LOG_STRING("status", "error"),
      Y_LOG_STRING("error", error));
}

// Make a command of a whole line, answering it here if it is not one.
static bool ParseLine(DebugCommand* command) {
  if (!YLogfmtParse(
          g_line, command->pairs, kDebugCommandMaxPairs, &command->num_pairs)) {
    ReplyToBadLine("malformed_line");
    return false;
  }
  if (command->num_pairs == 0) {
    return false;
  }
  command->id = 0;
  const char* id = YLogfmtFind(command->pairs, command->num_pairs, "id");
  uint64_t number = 0;
  if (id != NULL) {
    if (!YLogfmtParseUInt(id, &number) || number > UINT32_MAX) {
      ReplyToBadLine("bad_id");
      return false;
    }
    command->id = (uint32_t)number;
  }
  command->name = YLogfmtFind(command->pairs, command->num_pairs, "cmd");
  if (command->name == NULL) {
    command->name = "";
    DebugCommandReplyError(command, "missing_cmd");
    return false;
  }
  return true;
}

bool DebugCommandPoll(DebugCommand* command) {
  for (uint8_t i = 0; i < kMaxBytesPerPoll; ++i) {
    const int c = getchar_timeout_us(0);
    if (c < 0) {
      return false;
    }
    if (c == '\r') {
      continue;
    }
    if (c != '\n') {
      if (g_length + 1 < kMaxLineLength) {
        g_line[g_length++] = (char)c;
      } else {
        g_is_too_long = true;
      }
      continue;
    }
    g_line[g_length] = '\0';
    const bool is_too_long = g_is_too_long;
    g_length = 0;
    g_is_too_long = false;
    if (is_too_long) {
      ReplyToBadLine("line_too_long");
      continue;
    }
    if (ParseLine(command)) {
      return true;
    }
  }
  return false;
}

bool DebugCommandIs(const DebugCommand* command, const char* name) {
  return AreStringsEqual(command->name, name);
}

const char* DebugCommandArgument(const DebugCommand* command, const char* key) {
  return YLogfmtFind(command->pairs, command->num_pairs, key);
}

void DebugCommandReplyOk(const DebugCommand* command) {
  Y_LOG_REPLY(
      &g_runtime_logger, command->id, command->name,
      Y_LOG_STRING("status", "ok"));
}

void DebugCommandReplyError(const DebugCommand* command, const char* error) {
  Y_LOG_REPLY(
      &g_runtime_logger, command->id, command->name,
      Y_LOG_STRING("status", "error"), Y_LOG_STRING("error", error));
}

void DebugCommandInit(const InitStatus* init_status) {
  g_init_status = init_status;
}

// The system's groups, in two parts, neither inlined into the other: each
// group's fields take stack until the function they are in returns, and a
// report whole is among the deepest calls on core 0's stack. See
// StackUsagePaint().
//
// What the system is, how initializing it went, and its stacks.
static __noinline void ReportSystem(const DebugCommand* command) {
  char version[16];
  InitFormatKeyboardVersion(g_init_status, version, sizeof(version));
  DEBUG_COMMAND_REPLY_GROUP(
      command, "system", Y_LOG_STRING("runtime", "yax86 picocalc"),
      Y_LOG_UINT("sys_clk_khz", clock_get_hz(clk_sys) / kHzPerKHz),
      Y_LOG_STRING("overclock", InitOverclockName(g_init_status->overclock)),
      Y_LOG_UINT("lcd_spi_hz", g_init_status->lcd_spi_hz),
      Y_LOG_UINT("sd_clock_hz", SdcardGetClockHz()),
      Y_LOG_UINT("sd_blocks", SdcardGetNumBlocks()),
      Y_LOG_STRING("keyboard_fw", version));
  const PsramStats psram = PsramGetStats();
  DEBUG_COMMAND_REPLY_GROUP(
      command, "psram", Y_LOG_STRING("state", PsramStateName(psram.state)),
      Y_LOG_UINT("serial_hz", psram.serial_hz), Y_LOG_HEX8("kgd", psram.kgd),
      Y_LOG_UINT("read_512_us", psram.read_512_us),
      Y_LOG_UINT("write_512_us", psram.write_512_us),
      Y_LOG_UINT("bytes_read", psram.bytes_read),
      Y_LOG_UINT("bytes_written", psram.bytes_written),
      Y_LOG_UINT("timeouts", psram.num_timeouts));
  DEBUG_COMMAND_REPLY_GROUP(
      command, "stack", Y_LOG_UINT("size", StackUsageSize()),
      Y_LOG_UINT("core0_used", StackUsageCore0()),
      Y_LOG_UINT("core1_used", StackUsageCore1()));
}

// What has been done with USB storage, the keyboard and the speaker.
static __noinline void ReportDevices(const DebugCommand* command) {
  const UsbStorageStats usb = UsbStorageGetStats();
  DEBUG_COMMAND_REPLY_GROUP(
      command, "usb", Y_LOG_BOOL("exporting", usb.is_exporting),
      Y_LOG_STRING("image", usb.image_path != NULL ? usb.image_path : ""),
      Y_LOG_BOOL("connected", usb.is_connected),
      Y_LOG_BOOL("host_seen", usb.was_seen),
      Y_LOG_BOOL("ejected", usb.was_ejected),
      Y_LOG_UINT("blocks_read", usb.num_blocks_read),
      Y_LOG_UINT("blocks_written", usb.num_blocks_written),
      Y_LOG_UINT("read_us", usb.read_us), Y_LOG_UINT("write_us", usb.write_us),
      Y_LOG_UINT("errors", usb.num_errors));
  const SouthbridgePollStats keyboard = SouthbridgeGetPollStats();
  DEBUG_COMMAND_REPLY_GROUP(
      command, "keyboard", Y_LOG_UINT("reads", keyboard.num_reads),
      Y_LOG_UINT("keys", keyboard.num_keys),
      Y_LOG_UINT("stale_keys", keyboard.num_stale_keys),
      Y_LOG_UINT("aborts", keyboard.num_aborts),
      Y_LOG_UINT("timeouts", keyboard.num_timeouts));
  const SpeakerStats speaker = SpeakerGetStats();
  DEBUG_COMMAND_REPLY_GROUP(
      command, "speaker", Y_LOG_UINT("frequency_hz", speaker.frequency_hz),
      Y_LOG_UINT("output_hz", speaker.output_hz),
      Y_LOG_UINT("changes", speaker.num_changes));
}

void DebugCommandReportSystem(const DebugCommand* command) {
  ReportSystem(command);
  ReportDevices(command);
}

// Press, release or, by default, tap a key, by name. The key goes through the
// keymap as a key pressed on the PicoCalc does, one key event each pass of
// the loop, and the reply comes once it is queued. A host sending keys faster
// than they go is told the queue is full, and nothing is queued, so the key can
// be sent again.
static void RunKey(const DebugCommand* command) {
  const char* name = DebugCommandArgument(command, "name");
  const char* state = DebugCommandArgument(command, "state");
  TypingAction action = kTypingTap;
  if (state != NULL && state[0] == 'p') {
    action = kTypingPress;
  } else if (state != NULL && state[0] == 'r') {
    action = kTypingRelease;
  }
  uint8_t code = 0;
  if (name == NULL) {
    DebugCommandReplyError(command, "missing_name");
  } else if (!TypingFindKey(name, &code)) {
    DebugCommandReplyError(command, "unknown_key");
  } else if (!TypingQueueKey(code, action)) {
    DebugCommandReplyError(command, "queue_full");
  } else {
    DebugCommandReplyOk(command);
  }
}

void DebugCommandRunCommon(const DebugCommand* command) {
  if (DebugCommandIs(command, "ping")) {
    DebugCommandReplyOk(command);
  } else if (DebugCommandIs(command, "screenshot")) {
    // Core 1 owns the panel, so it reads it back and sends the reply.
    if (!DisplaySendScreenshot(command->id)) {
      DebugCommandReplyError(command, "busy");
    }
  } else if (DebugCommandIs(command, "key")) {
    RunKey(command);
  } else {
    DebugCommandReplyError(command, "unknown_command");
  }
}
