#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Boolean AND, OR and XOR instructions
// ============================================================================

YAX86_MODULE_PRIVATE void YCPUSetFlagsAfterBooleanInstruction(
    const YCPUInstructionContext* ctx, uint32_t result) {
  YCPUSetCommonFlagsAfterInstruction(ctx, result);
  // Carry Flag (CF) should be cleared
  YCPUSetFlag(ctx->cpu, kCF, false);
  // Overflow Flag (OF) should be cleared
  YCPUSetFlag(ctx->cpu, kOF, false);
}

// Common logic for AND instructions.
YAX86_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteBooleanAnd(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  uint32_t result = YCPUFromOperand(dest) & YCPUFromOperandValue(src_value);
  YCPUWriteOperand(ctx, dest, result);
  YCPUSetFlagsAfterBooleanInstruction(ctx, result);
  return kInstructionExecuted;
}

// AND r/m8, r8
// AND r/m16, r16
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanAndRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteBooleanAnd(ctx, &dest, src.value);
}

// AND r8, r/m8
// AND r16, r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanAndRegisterOrMemoryToRegister(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteBooleanAnd(ctx, &dest, src.value);
}

// AND AL, imm8
// AND AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanAndImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteBooleanAnd(ctx, &dest, src_value);
}

// Common logic for OR instructions.
YAX86_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteBooleanOr(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  uint32_t result = YCPUFromOperand(dest) | YCPUFromOperandValue(src_value);
  YCPUWriteOperand(ctx, dest, result);
  YCPUSetFlagsAfterBooleanInstruction(ctx, result);
  return kInstructionExecuted;
}

// OR r/m8, r8
// OR r/m16, r16
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanOrRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteBooleanOr(ctx, &dest, src.value);
}

// OR r8, r/m8
// OR r16, r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanOrRegisterOrMemoryToRegister(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteBooleanOr(ctx, &dest, src.value);
}

// OR AL, imm8
// OR AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanOrImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteBooleanOr(ctx, &dest, src_value);
}

// Common logic for XOR instructions.
YAX86_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteBooleanXor(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  uint32_t result = YCPUFromOperand(dest) ^ YCPUFromOperandValue(src_value);
  YCPUWriteOperand(ctx, dest, result);
  YCPUSetFlagsAfterBooleanInstruction(ctx, result);
  return kInstructionExecuted;
}

// XOR r/m8, r8
// XOR r/m16, r16
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
YCPUExecuteBooleanXorRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteBooleanXor(ctx, &dest, src.value);
}

// XOR r8, r/m8
// XOR r16, r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanXorRegisterOrMemoryToRegister(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteBooleanXor(ctx, &dest, src.value);
}

// XOR AL, imm8
// XOR AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanXorImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteBooleanXor(ctx, &dest, src_value);
}

// ============================================================================
// TEST instructions
// ============================================================================

// Common logic for TEST instructions.
YAX86_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteTest(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  uint32_t result = YCPUFromOperand(dest) & YCPUFromOperandValue(src_value);
  YCPUSetFlagsAfterBooleanInstruction(ctx, result);
  return kInstructionExecuted;
}

// TEST r/m8, r8
// TEST r/m16, r16
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
YCPUExecuteTestRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteTest(ctx, &dest, src.value);
}

// TEST AL, imm8
// TEST AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteTestImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteTest(ctx, &dest, src_value);
}
