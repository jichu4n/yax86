#include "command.h"

#include "pico/stdio.h"
#include "runtime_log.h"

enum {
  // The longest line taken, its terminating null byte included. Longer ones
  // are refused whole.
  kCommandMaxLineLength = 256,
  // The most bytes taken in one call, so that a host sending a lot at once
  // cannot hold a batch up for long.
  kCommandMaxBytesPerPoll = 64,
};

// The line being read, and whether it has outgrown the buffer.
static char g_line[kCommandMaxLineLength];
static uint16_t g_length;
static bool g_too_long;

static bool StringsEqual(const char* a, const char* b) {
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
static bool ParseLine(Command* command) {
  if (!YLogfmtParse(
          g_line, command->pairs, kCommandMaxPairs, &command->num_pairs)) {
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
    CommandReplyError(command, "missing_cmd");
    return false;
  }
  return true;
}

bool CommandPoll(Command* command) {
  for (uint8_t i = 0; i < kCommandMaxBytesPerPoll; ++i) {
    const int c = getchar_timeout_us(0);
    if (c < 0) {
      return false;
    }
    if (c == '\r') {
      continue;
    }
    if (c != '\n') {
      if (g_length + 1 < kCommandMaxLineLength) {
        g_line[g_length++] = (char)c;
      } else {
        g_too_long = true;
      }
      continue;
    }
    g_line[g_length] = '\0';
    const bool too_long = g_too_long;
    g_length = 0;
    g_too_long = false;
    if (too_long) {
      ReplyToBadLine("line_too_long");
      continue;
    }
    if (ParseLine(command)) {
      return true;
    }
  }
  return false;
}

bool CommandIs(const Command* command, const char* name) {
  return StringsEqual(command->name, name);
}

const char* CommandArgument(const Command* command, const char* key) {
  return YLogfmtFind(command->pairs, command->num_pairs, key);
}

void CommandReplyOk(const Command* command) {
  Y_LOG_REPLY(
      &g_runtime_logger, command->id, command->name,
      Y_LOG_STRING("status", "ok"));
}

void CommandReplyError(const Command* command, const char* error) {
  Y_LOG_REPLY(
      &g_runtime_logger, command->id, command->name,
      Y_LOG_STRING("status", "error"), Y_LOG_STRING("error", error));
}
