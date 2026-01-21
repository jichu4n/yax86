#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Boolean AND, OR and XOR instructions
// ============================================================================

YAX86_PRIVATE void SetFlagsAfterBooleanInstruction(
    const InstructionContext* ctx, uint32_t result) {
  SetCommonFlagsAfterInstruction(ctx, result);
  // Carry Flag (CF) should be cleared
  CPUSetFlag(ctx->cpu, kCF, false);
  // Overflow Flag (OF) should be cleared
  CPUSetFlag(ctx->cpu, kOF, false);
}

// Common logic for AND instructions.
YAX86_PRIVATE ExecuteStatus ExecuteBooleanAnd(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  uint32_t result = FromOperand(dest) & FromOperandValue(src_value);
  WriteOperand(ctx, dest, result);
  SetFlagsAfterBooleanInstruction(ctx, result);
  return kExecuteSuccess;
}

// AND r/m8, r8
// AND r/m16, r16
YAX86_PRIVATE ExecuteStatus
ExecuteBooleanAndRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteBooleanAnd(ctx, &dest, &src.value);
}

// AND r8, r/m8
// AND r16, r/m16
YAX86_PRIVATE ExecuteStatus
ExecuteBooleanAndRegisterOrMemoryToRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteBooleanAnd(ctx, &dest, &src.value);
}

// AND AL, imm8
// AND AX, imm16
YAX86_PRIVATE ExecuteStatus
ExecuteBooleanAndImmediateToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteBooleanAnd(ctx, &dest, &src_value);
}

// Common logic for OR instructions.
YAX86_PRIVATE ExecuteStatus ExecuteBooleanOr(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  uint32_t result = FromOperand(dest) | FromOperandValue(src_value);
  WriteOperand(ctx, dest, result);
  SetFlagsAfterBooleanInstruction(ctx, result);
  return kExecuteSuccess;
}

// OR r/m8, r8
// OR r/m16, r16
YAX86_PRIVATE ExecuteStatus
ExecuteBooleanOrRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteBooleanOr(ctx, &dest, &src.value);
}

// OR r8, r/m8
// OR r16, r/m16
YAX86_PRIVATE ExecuteStatus
ExecuteBooleanOrRegisterOrMemoryToRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteBooleanOr(ctx, &dest, &src.value);
}

// OR AL, imm8
// OR AX, imm16
YAX86_PRIVATE ExecuteStatus
ExecuteBooleanOrImmediateToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteBooleanOr(ctx, &dest, &src_value);
}

// Common logic for XOR instructions.
YAX86_PRIVATE ExecuteStatus ExecuteBooleanXor(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  uint32_t result = FromOperand(dest) ^ FromOperandValue(src_value);
  WriteOperand(ctx, dest, result);
  SetFlagsAfterBooleanInstruction(ctx, result);
  return kExecuteSuccess;
}

// XOR r/m8, r8
// XOR r/m16, r16
YAX86_PRIVATE ExecuteStatus
ExecuteBooleanXorRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteBooleanXor(ctx, &dest, &src.value);
}

// XOR r8, r/m8
// XOR r16, r/m16
YAX86_PRIVATE ExecuteStatus
ExecuteBooleanXorRegisterOrMemoryToRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteBooleanXor(ctx, &dest, &src.value);
}

// XOR AL, imm8
// XOR AX, imm16
YAX86_PRIVATE ExecuteStatus
ExecuteBooleanXorImmediateToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteBooleanXor(ctx, &dest, &src_value);
}

// ============================================================================
// TEST instructions
// ============================================================================

// Common logic for TEST instructions.
YAX86_PRIVATE ExecuteStatus ExecuteTest(
    const InstructionContext* ctx, Operand* dest, OperandValue* src_value) {
  uint32_t result = FromOperand(dest) & FromOperandValue(src_value);
  SetFlagsAfterBooleanInstruction(ctx, result);
  return kExecuteSuccess;
}

// TEST r/m8, r8
// TEST r/m16, r16
YAX86_PRIVATE ExecuteStatus
ExecuteTestRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteTest(ctx, &dest, &src.value);
}

// TEST AL, imm8
// TEST AX, imm16
YAX86_PRIVATE ExecuteStatus
ExecuteTestImmediateToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteTest(ctx, &dest, &src_value);
}
