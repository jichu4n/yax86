#include "commands.h"

#include <stddef.h>

#include "display.h"
#include "pico/stdlib.h"
#include "typing.h"

// Press, release or, by default, tap a key, by name. The key goes through the
// keymap as a key pressed on the PicoCalc does, a key event a batch, and the
// reply comes once it is queued. A host sending keys faster than they go is
// told the queue is full, and nothing is queued, so the key can be sent
// again.
static void RunKey(const Command* command) {
  const char* name = CommandArgument(command, "name");
  const char* state = CommandArgument(command, "state");
  TypingAction action = kTypingTap;
  if (state != NULL && state[0] == 'p') {
    action = kTypingPress;
  } else if (state != NULL && state[0] == 'r') {
    action = kTypingRelease;
  }
  uint8_t code = 0;
  if (name == NULL) {
    CommandReplyError(command, "missing_name");
  } else if (!TypingFindKey(name, &code)) {
    CommandReplyError(command, "unknown_key");
  } else if (!TypingQueueKey(code, action)) {
    CommandReplyError(command, "queue_full");
  } else {
    CommandReplyOk(command);
  }
}

// Kept out of line, so that the report's groups are on core 0's stack only
// while a command runs.
static __noinline void Run(
    const CommandsHandlers* handlers, const Command* command) {
  if (CommandIs(command, "ping")) {
    CommandReplyOk(command);
  } else if (CommandIs(command, "report")) {
    handlers->report(command);
    CommandReplyOk(command);
  } else if (CommandIs(command, "screen")) {
    handlers->screen(command);
  } else if (CommandIs(command, "screenshot")) {
    // Core 1 owns the panel, so it reads it back and sends the reply.
    if (!DisplaySendScreenshot(command->id)) {
      CommandReplyError(command, "busy");
    }
  } else if (CommandIs(command, "key")) {
    RunKey(command);
  } else {
    CommandReplyError(command, "unknown_command");
  }
}

bool CommandsPoll(const CommandsHandlers* handlers, uint64_t* run_us) {
  Command command;
  if (!CommandPoll(&command)) {
    return false;
  }
  const uint64_t start_us = time_us_64();
  Run(handlers, &command);
  if (run_us != NULL) {
    *run_us += time_us_64() - start_us;
  }
  return true;
}
