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
YAX86_FILE_PRIVATE const YCPUFlag kFlagsForClearAndSetInstructions[] = {
    kCF,  // CLC, STC
    kIF,  // CLI, STI
    kDF,  // CLD, STD
};

YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteClearOrSetFlag(const YCPUInstructionContext* ctx) {
  uint8_t opcode_index = ctx->instruction->opcode - 0xF8;
  YCPUFlag flag = kFlagsForClearAndSetInstructions[opcode_index / 2];
  bool value = (opcode_index & 0x1) != 0;
  YCPUSetFlag(ctx->cpu, flag, value);
  return kInstructionExecuted;
}

// ============================================================================
// CMC instruction
// ============================================================================

// CMC
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteComplementCarryFlag(const YCPUInstructionContext* ctx) {
  YCPUSetFlag(ctx->cpu, kCF, !YCPUGetFlag(ctx->cpu, kCF));
  return kInstructionExecuted;
}

// ============================================================================
// SALC instruction
// ============================================================================

// SALC - Set AL from Carry
//
// Undocumented on every x86 generation, but consistently implemented: AL
// becomes 0xFF if CF is set and 0x00 otherwise. No flags are affected.
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSetALFromCarry(const YCPUInstructionContext* ctx) {
  const uint8_t value = YCPUGetFlag(ctx->cpu, kCF) ? 0xFF : 0x00;
  ctx->cpu->registers[kAX] = (ctx->cpu->registers[kAX] & 0xFF00) | value;
  return kInstructionExecuted;
}
