#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// ADD, ADC, and INC instructions
// ============================================================================

// Set CPU flags after an INC instruction.
// Other than common flags, the INC instruction sets the following flags:
// - Overflow Flag (OF) - Set when result has wrong sign
// - Auxiliary Carry Flag (AF) - carry from bit 3 to bit 4
static void SetFlagsAfterInc(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_carry) {
  SetCommonFlagsAfterInstruction(ctx, result);

  // Overflow Flag (OF) Set when result has wrong sign (both operands have same
  // sign but result has different sign)
  uint32_t sign_bit = kSignBit[ctx->metadata->width];
  bool op1_sign = (op1 & sign_bit) != 0;
  bool op2_sign = (op2 & sign_bit) != 0;
  bool result_sign = (result & sign_bit) != 0;
  SetFlag(ctx->cpu, kOF, (op1_sign == op2_sign) && (result_sign != op1_sign));

  // Auxiliary Carry Flag (AF) - carry from bit 3 to bit 4
  SetFlag(
      ctx->cpu, kAF, ((op1 & 0xF) + (op2 & 0xF) + (did_carry ? 1 : 0)) > 0xF);
}

// Set CPU flags after an ADD or ADC instruction.
// Other than the flags set by the INC instruction, the ADD instruction sets the
// following flags:
// - Carry Flag (CF) - Set when result overflows the maximum width
static void SetFlagsAfterAdd(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_carry) {
  SetFlagsAfterInc(ctx, op1, op2, result, did_carry);
  // Carry Flag (CF)
  SetFlag(ctx->cpu, kCF, result > kMaxValue[ctx->metadata->width]);
}

// Common signature of SetFlagsAfterAdd and SetFlagsAfterInc.
typedef void (*SetFlagsAfterAddFn)(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_carry);

// Common logic for ADD, ADC, and INC instructions.
static ExecuteStatus ExecuteAddCommon(
    const InstructionContext* ctx, Operand* dest, const OperandValue* src_value,
    bool carry, SetFlagsAfterAddFn set_flags_after_fn) {
  uint32_t raw_dest_value = FromOperand(dest);
  uint32_t raw_src_value = FromOperandValue(src_value);
  bool should_carry = carry && GetFlag(ctx->cpu, kCF);
  uint32_t result = raw_dest_value + raw_src_value + (should_carry ? 1 : 0);
  WriteOperand(ctx, dest, result);
  (*set_flags_after_fn)(
      ctx, raw_dest_value, raw_src_value, result, should_carry);
  return kExecuteSuccess;
}

// Common logic for ADD instructions
YAX86_PRIVATE ExecuteStatus ExecuteAdd(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  return ExecuteAddCommon(
      ctx, dest, src_value, /* carry */ false, SetFlagsAfterAdd);
}

// ADD r/m8, r8
// ADD r/m16, r16
YAX86_PRIVATE ExecuteStatus
ExecuteAddRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteAdd(ctx, &dest, &src.value);
}

// ADD r8, r/m8
// ADD r16, r/m16
YAX86_PRIVATE ExecuteStatus
ExecuteAddRegisterOrMemoryToRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteAdd(ctx, &dest, &src.value);
}

// ADD AL, imm8
// ADD AX, imm16
YAX86_PRIVATE ExecuteStatus
ExecuteAddImmediateToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteAdd(ctx, &dest, &src_value);
}

// Common logic for ADC instructions
YAX86_PRIVATE ExecuteStatus ExecuteAddWithCarry(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  return ExecuteAddCommon(
      ctx, dest, src_value, /* carry */ true, SetFlagsAfterAdd);
}

// ADC r/m8, r8
// ADC r/m16, r16
YAX86_PRIVATE ExecuteStatus
ExecuteAddRegisterToRegisterOrMemoryWithCarry(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteAddWithCarry(ctx, &dest, &src.value);
}
// ADC r8, r/m8
// ADC r16, r/m16
YAX86_PRIVATE ExecuteStatus
ExecuteAddRegisterOrMemoryToRegisterWithCarry(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteAddWithCarry(ctx, &dest, &src.value);
}

// ADC AL, imm8
// ADC AX, imm16
YAX86_PRIVATE ExecuteStatus
ExecuteAddImmediateToALOrAXWithCarry(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteAddWithCarry(ctx, &dest, &src_value);
}

// Common logic for INC instructions
YAX86_PRIVATE ExecuteStatus
ExecuteInc(const InstructionContext* ctx, Operand* dest) {
  OperandValue src_value = WordValue(1);
  return ExecuteAddCommon(
      ctx, dest, &src_value, /* carry */ false, SetFlagsAfterInc);
}

// INC AX/CX/DX/BX/SP/BP/SI/DI
YAX86_PRIVATE ExecuteStatus ExecuteIncRegister(const InstructionContext* ctx) {
  RegisterIndex register_index =
      (RegisterIndex)(ctx->instruction->opcode - 0x40);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  return ExecuteInc(ctx, &dest);
}
