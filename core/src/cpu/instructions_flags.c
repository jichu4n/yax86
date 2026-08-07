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

YAX86_PRIVATE InstructionResult
ExecuteClearOrSetFlag(const InstructionContext* ctx) {
  uint8_t opcode_index = ctx->instruction->opcode - 0xF8;
  Flag flag = kFlagsForClearAndSetInstructions[opcode_index / 2];
  bool value = (opcode_index & 0x1) != 0;
  CPUSetFlag(ctx->cpu, flag, value);
  return kInstructionExecuted;
}

// ============================================================================
// CMC instruction
// ============================================================================

// CMC
YAX86_PRIVATE InstructionResult
ExecuteComplementCarryFlag(const InstructionContext* ctx) {
  CPUSetFlag(ctx->cpu, kCF, !CPUGetFlag(ctx->cpu, kCF));
  return kInstructionExecuted;
}

// ============================================================================
// SALC instruction
// ============================================================================

// SALC - Set AL from Carry
//
// Undocumented on every x86 generation, but consistently implemented: AL
// becomes 0xFF if CF is set and 0x00 otherwise. No flags are affected.
YAX86_PRIVATE InstructionResult
ExecuteSetALFromCarry(const InstructionContext* ctx) {
  const uint8_t value = CPUGetFlag(ctx->cpu, kCF) ? 0xFF : 0x00;
  ctx->cpu->registers[kAX] = (ctx->cpu->registers[kAX] & 0xFF00) | value;
  return kInstructionExecuted;
}
