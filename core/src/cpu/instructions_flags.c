#ifndef Y_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // Y_IMPLEMENTATION

// ============================================================================
// CLC, STC, CLI, STI, CLD, STD instructions
// ============================================================================

// Table of flags corresponding to the CLC, STC, CLI, STI, CLD, and STD
// instructions, indexed by (opcode - 0xF8) / 2.
Y_FILE_PRIVATE const YCPUFlag YCPUFlagsForClearAndSetInstructions[] = {
    Y_CPU_CF,  // CLC, STC
    Y_CPU_IF,  // CLI, STI
    Y_CPU_DF,  // CLD, STD
};

Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteClearOrSetFlag(const YCPUInstructionContext* ctx) {
  uint8_t opcode_index = ctx->instruction->opcode - 0xF8;
  YCPUFlag flag = YCPUFlagsForClearAndSetInstructions[opcode_index / 2];
  bool value = (opcode_index & 0x1) != 0;
  YCPUSetFlag(ctx->cpu, flag, value);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ============================================================================
// CMC instruction
// ============================================================================

// CMC
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteComplementCarryFlag(const YCPUInstructionContext* ctx) {
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, !YCPUGetFlag(ctx->cpu, Y_CPU_CF));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ============================================================================
// SALC instruction
// ============================================================================

// SALC - Set AL from Carry
//
// Undocumented on every x86 generation, but consistently implemented: AL
// becomes 0xFF if CF is set and 0x00 otherwise. No flags are affected.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSetALFromCarry(const YCPUInstructionContext* ctx) {
  const uint8_t value = YCPUGetFlag(ctx->cpu, Y_CPU_CF) ? 0xFF : 0x00;
  ctx->cpu->registers[Y_CPU_AX] =
      (ctx->cpu->registers[Y_CPU_AX] & 0xFF00) | value;
  return Y_CPU_INSTRUCTION_EXECUTED;
}
