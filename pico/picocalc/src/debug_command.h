// Debug commands: what a host, such as pico/picocalc/debug.js, sends over USB
// serial to look at the board and drive it. Each is a line of logfmt pairs,
// such as
//
//   id=7 cmd=screen
//
// and the answer is reply lines carrying the same id, the last with
// status=ok, or status=error and what went wrong. See "util - logging" in
// core/AGENTS.md.
//
// This module reads the commands, without waiting, and answers the ones that
// are common to every mode: ping, key and screenshot. It also gives the
// system's groups of a report, the facts that hold whatever mode the board is
// in. Each mode answers its own commands, report and screen, and passes any
// other to DebugCommandRunCommon().
#ifndef YAX86_PICOCALC_DEBUG_COMMAND_H
#define YAX86_PICOCALC_DEBUG_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "core/platform.h"
#include "init.h"
#include "runtime_log.h"

enum {
  // The most pairs a command's line may have, its id and name included.
  kDebugCommandMaxNumPairs = 8,
};

// A command read from a line, its pairs pointing into the line. Valid until
// the next DebugCommandPoll().
typedef struct DebugCommand {
  // The id its replies carry, or 0 if the line gave none.
  uint32_t id;
  // The command's name, cmd's value: ping, report, screen and so on.
  const char* name;
  // Every pair on the line, the id and name included, for its arguments.
  YLogfmtPair pairs[kDebugCommandMaxNumPairs];
  uint8_t num_pairs;
} DebugCommand;

// Keep init_status, what initializing the system found, for the report's
// system groups. It must outlive the commands.
void DebugCommandInit(const InitStatus* init_status);

// Read what the host has sent since the last call, a few bytes at most so
// that a call costs next to nothing. Returns true with a command once a
// whole line of one is in. A line that is not a command is answered with an
// error here.
bool DebugCommandPoll(DebugCommand* command);

// Whether command is the one named name.
bool DebugCommandIs(const DebugCommand* command, const char* name);

// The value of the argument named key, or NULL if command has none.
const char* DebugCommandArgument(const DebugCommand* command, const char* key);

// End a command's reply: status=ok, or status=error with what went wrong.
void DebugCommandReplyOk(const DebugCommand* command);
void DebugCommandReplyError(const DebugCommand* command, const char* error);

// Reply with one group of a report: a line with group= the group's name, and
// the log fields after it.
#define DEBUG_COMMAND_REPLY_GROUP(command, group, ...)   \
  Y_LOG_REPLY(                                           \
      &g_runtime_logger, (command)->id, (command)->name, \
      Y_LOG_STRING("group", (group)), __VA_ARGS__)

// Reply with the system's groups of a report: the system, PSRAM, stacks, USB
// storage, keyboard and speaker. A mode's report gives these, then its own,
// and ends the reply.
void DebugCommandReportSystem(const DebugCommand* command);

// Answer a command common to every mode, ping, key or screenshot, or reply
// that the command is unknown.
void DebugCommandRunCommon(const DebugCommand* command);

#endif  // YAX86_PICOCALC_DEBUG_COMMAND_H
