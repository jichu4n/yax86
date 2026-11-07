#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Sign extension instructions
// ============================================================================

// CBW
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCbw(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[Y_CPU_AX] & 0xFF;
  uint8_t ah = (al & YCPUSignBit[Y_CPU_BYTE]) ? 0xFF : 0x00;
  ctx->cpu->registers[Y_CPU_AX] = (ah << 8) | al;
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// CWD
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
YCPUExecuteCwd(const YCPUInstructionContext* ctx) {
  ctx->cpu->registers[Y_CPU_DX] =
      (ctx->cpu->registers[Y_CPU_AX] & YCPUSignBit[Y_CPU_WORD]) ? 0xFFFF
                                                                : 0x0000;
  return Y_CPU_INSTRUCTION_EXECUTED;
}
