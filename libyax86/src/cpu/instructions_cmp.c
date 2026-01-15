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
YAX86_PRIVATE ExecuteStatus ExecuteCmp(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  uint32_t raw_dest_value = FromOperand(dest);
  uint32_t raw_src_value = FromOperandValue(src_value);
  uint32_t result = raw_dest_value - raw_src_value;
  SetFlagsAfterSub(ctx, raw_dest_value, raw_src_value, result, false);
  return kExecuteSuccess;
}

// CMP r/m8, r8
// CMP r/m16, r16
YAX86_PRIVATE ExecuteStatus
ExecuteCmpRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteCmp(ctx, &dest, &src.value);
}

// CMP r8, r/m8
// CMP r16, r/m16
YAX86_PRIVATE ExecuteStatus
ExecuteCmpRegisterOrMemoryToRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteCmp(ctx, &dest, &src.value);
}

// CMP AL, imm8
// CMP AX, imm16
YAX86_PRIVATE ExecuteStatus
ExecuteCmpImmediateToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteCmp(ctx, &dest, &src_value);
}
