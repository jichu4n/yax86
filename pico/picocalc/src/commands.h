// The commands a host sends over USB serial, answered whichever mode the
// board is in: ping, report, screen, screenshot and key. Reading and
// dispatching a command, and the commands every mode answers the same, are
// here; the report's groups and the guest's text screen are the mode's,
// which hands them in.
#ifndef YAX86_PICOCALC_COMMANDS_H
#define YAX86_PICOCALC_COMMANDS_H

#include <stdbool.h>
#include <stdint.h>

#include "command.h"

// What a mode answers itself.
typedef struct CommandsHandlers {
  // Reply with the report's groups; the reply is ended after.
  void (*report)(const Command* command);
  // Reply with the guest's text screen, ending the reply.
  void (*screen)(const Command* command);
} CommandsHandlers;

// Answer the command the host has sent, if a whole one is in, with
// handlers for what is the mode's. With run_us, the time spent answering it
// is added to *run_us. Returns whether there was one.
bool CommandsPoll(const CommandsHandlers* handlers, uint64_t* run_us);

#endif  // YAX86_PICOCALC_COMMANDS_H
