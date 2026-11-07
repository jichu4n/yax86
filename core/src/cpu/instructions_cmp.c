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
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult ExecuteCmp(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  uint32_t raw_dest_value = FromOperand(dest);
  uint32_t raw_src_value = FromOperandValue(src_value);
  uint32_t result = raw_dest_value - raw_src_value;
  SetFlagsAfterSub(ctx, raw_dest_value, raw_src_value, result, false);
  return kInstructionExecuted;
}

// CMP r/m8, r8
// CMP r/m16, r16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteCmpRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  ReadRegisterOperand(ctx, &src);
  return ExecuteCmp(ctx, &dest, src.value);
}

// CMP r8, r/m8
// CMP r16, r/m16
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
ExecuteCmpRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  ReadRegisterOrMemoryOperand(ctx, &src);
  return ExecuteCmp(ctx, &dest, src.value);
}

// CMP AL, imm8
// CMP AX, imm16
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
ExecuteCmpImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);
  YCPUOperandValue src_value = ReadImmediate(ctx);
  return ExecuteCmp(ctx, &dest, src_value);
}
