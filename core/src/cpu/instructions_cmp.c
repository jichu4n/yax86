#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// CMP instructions
// ============================================================================

// Common logic for CMP instructions. Computes dest - src and sets flags.
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult YCPUExecuteCmp(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  uint32_t raw_dest_value = YCPUFromOperand(dest);
  uint32_t raw_src_value = YCPUFromOperandValue(src_value);
  uint32_t result = raw_dest_value - raw_src_value;
  YCPUSetFlagsAfterSub(ctx, raw_dest_value, raw_src_value, result, false);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// CMP r/m8, r8
// CMP r/m16, r16
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCmpRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteCmp(ctx, &dest, src.value);
}

// CMP r8, r/m8
// CMP r16, r/m16
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
YCPUExecuteCmpRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteCmp(ctx, &dest, src.value);
}

// CMP AL, imm8
// CMP AX, imm16
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
YCPUExecuteCmpImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteCmp(ctx, &dest, src_value);
}
