#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Boolean AND, OR and XOR instructions
// ============================================================================

YAX86_MODULE_PRIVATE void SetFlagsAfterBooleanInstruction(
    const YCPUInstructionContext* ctx, uint32_t result) {
  SetCommonFlagsAfterInstruction(ctx, result);
  // Carry Flag (CF) should be cleared
  CPUSetFlag(ctx->cpu, kCF, false);
  // Overflow Flag (OF) should be cleared
  CPUSetFlag(ctx->cpu, kOF, false);
}

// Common logic for AND instructions.
YAX86_MODULE_PRIVATE YCPUInstructionResult ExecuteBooleanAnd(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  uint32_t result = FromOperand(dest) & FromOperandValue(src_value);
  WriteOperand(ctx, dest, result);
  SetFlagsAfterBooleanInstruction(ctx, result);
  return kInstructionExecuted;
}

// AND r/m8, r8
// AND r/m16, r16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteBooleanAndRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  ReadRegisterOperand(ctx, &src);
  return ExecuteBooleanAnd(ctx, &dest, src.value);
}

// AND r8, r/m8
// AND r16, r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteBooleanAndRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  ReadRegisterOrMemoryOperand(ctx, &src);
  return ExecuteBooleanAnd(ctx, &dest, src.value);
}

// AND AL, imm8
// AND AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteBooleanAndImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);
  YCPUOperandValue src_value = ReadImmediate(ctx);
  return ExecuteBooleanAnd(ctx, &dest, src_value);
}

// Common logic for OR instructions.
YAX86_MODULE_PRIVATE YCPUInstructionResult ExecuteBooleanOr(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  uint32_t result = FromOperand(dest) | FromOperandValue(src_value);
  WriteOperand(ctx, dest, result);
  SetFlagsAfterBooleanInstruction(ctx, result);
  return kInstructionExecuted;
}

// OR r/m8, r8
// OR r/m16, r16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteBooleanOrRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  ReadRegisterOperand(ctx, &src);
  return ExecuteBooleanOr(ctx, &dest, src.value);
}

// OR r8, r/m8
// OR r16, r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteBooleanOrRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  ReadRegisterOrMemoryOperand(ctx, &src);
  return ExecuteBooleanOr(ctx, &dest, src.value);
}

// OR AL, imm8
// OR AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteBooleanOrImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);
  YCPUOperandValue src_value = ReadImmediate(ctx);
  return ExecuteBooleanOr(ctx, &dest, src_value);
}

// Common logic for XOR instructions.
YAX86_MODULE_PRIVATE YCPUInstructionResult ExecuteBooleanXor(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  uint32_t result = FromOperand(dest) ^ FromOperandValue(src_value);
  WriteOperand(ctx, dest, result);
  SetFlagsAfterBooleanInstruction(ctx, result);
  return kInstructionExecuted;
}

// XOR r/m8, r8
// XOR r/m16, r16
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
ExecuteBooleanXorRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  ReadRegisterOperand(ctx, &src);
  return ExecuteBooleanXor(ctx, &dest, src.value);
}

// XOR r8, r/m8
// XOR r16, r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteBooleanXorRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  ReadRegisterOrMemoryOperand(ctx, &src);
  return ExecuteBooleanXor(ctx, &dest, src.value);
}

// XOR AL, imm8
// XOR AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteBooleanXorImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);
  YCPUOperandValue src_value = ReadImmediate(ctx);
  return ExecuteBooleanXor(ctx, &dest, src_value);
}

// ============================================================================
// TEST instructions
// ============================================================================

// Common logic for TEST instructions.
YAX86_MODULE_PRIVATE YCPUInstructionResult ExecuteTest(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  uint32_t result = FromOperand(dest) & FromOperandValue(src_value);
  SetFlagsAfterBooleanInstruction(ctx, result);
  return kInstructionExecuted;
}

// TEST r/m8, r8
// TEST r/m16, r16
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
ExecuteTestRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  ReadRegisterOperand(ctx, &src);
  return ExecuteTest(ctx, &dest, src.value);
}

// TEST AL, imm8
// TEST AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteTestImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);
  YCPUOperandValue src_value = ReadImmediate(ctx);
  return ExecuteTest(ctx, &dest, src_value);
}
