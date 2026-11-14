#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
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
Y_FILE_PRIVATE void YCPUSetFlagsAfterInc(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_carry) {
  YCPUSetCommonFlagsAfterInstruction(ctx, result);

  // Overflow Flag (OF) Set when result has wrong sign (both operands have same
  // sign but result has different sign)
  uint32_t sign_bit = YCPUSignBit[ctx->metadata->width];
  bool op1_sign = (op1 & sign_bit) != 0;
  bool op2_sign = (op2 & sign_bit) != 0;
  bool result_sign = (result & sign_bit) != 0;
  YCPUSetFlag(
      ctx->cpu, Y_CPU_OF, (op1_sign == op2_sign) && (result_sign != op1_sign));

  // Auxiliary Carry Flag (AF) - carry from bit 3 to bit 4
  YCPUSetFlag(
      ctx->cpu, Y_CPU_AF,
      ((op1 & 0xF) + (op2 & 0xF) + (did_carry ? 1 : 0)) > 0xF);
}

// Set CPU flags after an ADD or ADC instruction.
// Other than the flags set by the INC instruction, the ADD instruction sets the
// following flags:
// - Carry Flag (CF) - Set when result overflows the maximum width
Y_FILE_PRIVATE void YCPUSetFlagsAfterAdd(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_carry) {
  YCPUSetFlagsAfterInc(ctx, op1, op2, result, did_carry);
  // Carry Flag (CF)
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, result > YCPUMaxValue[ctx->metadata->width]);
}

// Common signature of YCPUSetFlagsAfterAdd and YCPUSetFlagsAfterInc.
typedef void (*YCPUSetFlagsAfterAddFn)(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_carry);

// Common logic for ADD, ADC, and INC instructions.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteAddCommon(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value, bool use_carry,
    YCPUSetFlagsAfterAddFn set_flags_after_fn) {
  uint32_t raw_dest_value = YCPUFromOperand(dest);
  uint32_t raw_src_value = YCPUFromOperandValue(src_value);
  bool should_carry = use_carry && YCPUGetFlag(ctx->cpu, Y_CPU_CF);
  uint32_t result = raw_dest_value + raw_src_value + (should_carry ? 1 : 0);
  YCPUWriteOperand(ctx, dest, result);
  (*set_flags_after_fn)(
      ctx, raw_dest_value, raw_src_value, result, should_carry);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Common logic for ADD instructions
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteAdd(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  return YCPUExecuteAddCommon(
      ctx, dest, src_value, /* use_carry */ false, YCPUSetFlagsAfterAdd);
}

// ADD r/m8, r8
// ADD r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteAdd(ctx, &dest, src.value);
}

// ADD r8, r/m8
// ADD r16, r/m16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteAddRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteAdd(ctx, &dest, src.value);
}

// ADD AL, imm8
// ADD AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteAdd(ctx, &dest, src_value);
}

// Common logic for ADC instructions
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult YCPUExecuteAddWithCarry(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  return YCPUExecuteAddCommon(
      ctx, dest, src_value, /* use_carry */ true, YCPUSetFlagsAfterAdd);
}

// ADC r/m8, r8
// ADC r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddRegisterToRegisterOrMemoryWithCarry(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteAddWithCarry(ctx, &dest, src.value);
}
// ADC r8, r/m8
// ADC r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddRegisterOrMemoryToRegisterWithCarry(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteAddWithCarry(ctx, &dest, src.value);
}

// ADC AL, imm8
// ADC AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddImmediateToALOrAXWithCarry(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteAddWithCarry(ctx, &dest, src_value);
}

// Common logic for INC instructions
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInc(const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUOperandValue src_value = 1;
  return YCPUExecuteAddCommon(
      ctx, dest, src_value, /* use_carry */ false, YCPUSetFlagsAfterInc);
}

// INC AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteIncRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode - 0x40);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &dest);
  return YCPUExecuteInc(ctx, &dest);
}
