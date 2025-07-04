#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// CLC, STC, CLI, STI, CLD, STD instructions
// ============================================================================

// Table of flags corresponding to the CLC, STC, CLI, STI, CLD, and STD
// instructions, indexed by (opcode - 0xF8) / 2.
static const Flag kFlagsForClearAndSetInstructions[] = {
    kCF,  // CLC, STC
    kIF,  // CLI, STI
    kDF,  // CLD, STD
};

YAX86_PRIVATE ExecuteStatus
ExecuteClearOrSetFlag(const InstructionContext* ctx) {
  uint8_t opcode_index = ctx->instruction->opcode - 0xF8;
  Flag flag = kFlagsForClearAndSetInstructions[opcode_index / 2];
  bool value = (opcode_index & 0x1) != 0;
  SetFlag(ctx->cpu, flag, value);
  return kExecuteSuccess;
}

// ============================================================================
// CMC instruction
// ============================================================================

// CMC
YAX86_PRIVATE ExecuteStatus
ExecuteComplementCarryFlag(const InstructionContext* ctx) {
  SetFlag(ctx->cpu, kCF, !GetFlag(ctx->cpu, kCF));
  return kExecuteSuccess;
}
