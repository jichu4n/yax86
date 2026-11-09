// Commands from a host over USB serial: a logfmt line each, such as
//
//   id=7 cmd=screen
//
// answered by reply lines carrying the same id - see "util — logging" in
// AGENTS.md. Bytes are read without waiting, a few at a time, so that polling
// costs a batch next to nothing when a host has sent nothing.
#ifndef YAX86_PICOCALC_COMMAND_H
#define YAX86_PICOCALC_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "core/platform.h"

enum {
  // The most pairs a command line may have, its id and name included.
  kCommandMaxPairs = 8,
};

// A command read from a line, its pairs pointing into the line. Valid until
// the next CommandPoll().
typedef struct Command {
  // 0 if the line gave none.
  uint32_t id;
  const char* name;
  YLogfmtPair pairs[kCommandMaxPairs];
  uint8_t num_pairs;
} Command;

// Read what the host has sent since the last call, and return true with a
// command once a whole line of one is in. A line that is not a command is
// answered with an error here.
bool CommandPoll(Command* command);

// Whether a command is the one with this name.
bool CommandIs(const Command* command, const char* name);

// An argument's value, or NULL if the command does not have it.
const char* CommandArgument(const Command* command, const char* key);

// End a command's reply: status=ok, or status=error with what went wrong.
void CommandReplyOk(const Command* command);
void CommandReplyError(const Command* command, const char* error);

#endif  // YAX86_PICOCALC_COMMAND_H
