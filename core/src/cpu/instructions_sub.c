#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// SUB, SBB, and DEC instructions
// ============================================================================

// Set CPU flags after a DEC or SUB/SBB operation (base function).
// This function sets ZF, SF, PF, OF, AF. It does NOT affect CF.
// - OF is for the full operation op1 - (op2 + did_borrow).
// - AF is for the full operation op1 - (op2 + did_borrow).
YAX86_FILE_PRIVATE void YCPUSetFlagsAfterDec(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_borrow) {
  YCPUSetCommonFlagsAfterInstruction(ctx, result);

  uint32_t sign_bit = kSignBit[ctx->metadata->width];
  uint32_t max_val = kMaxValue[ctx->metadata->width];

  // Overflow Flag (OF)
  // A subtraction overflows when the operands have different signs and the
  // result takes the sign of the subtrahend rather than the minuend. The
  // borrow does not enter into it: it shifts the result by one, and the result
  // is already what the test looks at. Folding the borrow into the subtrahend
  // instead would change that operand's sign whenever it is 0x7F or 0x7FFF,
  // and give the wrong answer for exactly those values.
  bool op1_sign = (op1 & sign_bit) != 0;
  bool op2_sign = ((op2 & max_val) & sign_bit) != 0;
  bool result_sign = (result & sign_bit) != 0;
  YCPUSetFlag(
      ctx->cpu, kOF, (op1_sign != op2_sign) && (result_sign != op1_sign));

  // Auxiliary Carry Flag (AF) - borrow from bit 3 to bit 4
  YCPUSetFlag(
      ctx->cpu, kAF, (op1 & 0xF) < ((op2 & 0xF) + (did_borrow ? 1 : 0)));
}

// Set CPU flags after a SUB, SBB, CMP or NEG instruction.
// This calls YCPUSetFlagsAfterDec and then sets the Carry Flag (CF).
YAX86_MODULE_PRIVATE void YCPUSetFlagsAfterSub(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_borrow) {
  YCPUSetFlagsAfterDec(ctx, op1, op2, result, did_borrow);

  // Carry Flag (CF) - Set when a borrow is generated
  // CF is set if op1 < (op2 + did_borrow) (unsigned comparison)
  uint32_t val_being_subtracted =
      (op2 & kMaxValue[ctx->metadata->width]) + (did_borrow ? 1 : 0);
  YCPUSetFlag(ctx->cpu, kCF, op1 < val_being_subtracted);
}

// Common signature of YCPUSetFlagsAfterSub and YCPUSetFlagsAfterDec.
typedef void (*YCPUSetFlagsAfterSubFn)(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_borrow);

// Common logic for SUB, SBB, and DEC instructions.
YAX86_FILE_PRIVATE YAX86_HOT YCPUInstructionResult YCPUExecuteSubCommon(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value, bool borrow,
    YCPUSetFlagsAfterSubFn set_flags_after_fn) {
  uint32_t raw_dest_value = YCPUFromOperand(dest);
  uint32_t raw_src_value = YCPUFromOperandValue(src_value);
  bool should_borrow = borrow && YCPUGetFlag(ctx->cpu, kCF);
  uint32_t result = raw_dest_value - raw_src_value - (should_borrow ? 1 : 0);
  YCPUWriteOperand(ctx, dest, result);
  (*set_flags_after_fn)(
      ctx, raw_dest_value, raw_src_value, result, should_borrow);
  return kInstructionExecuted;
}

// Common logic for SUB instructions
YAX86_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteSub(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  return YCPUExecuteSubCommon(
      ctx, dest, src_value, /* borrow */ false, YCPUSetFlagsAfterSub);
}

// SUB r/m8, r8
// SUB r/m16, r16
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
YCPUExecuteSubRegisterFromRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteSub(ctx, &dest, src.value);
}

// SUB r8, r/m8
// SUB r16, r/m16
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
YCPUExecuteSubRegisterOrMemoryFromRegister(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteSub(ctx, &dest, src.value);
}

// SUB AL, imm8
// SUB AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubImmediateFromALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteSub(ctx, &dest, src_value);
}

// Common logic for SBB instructions
YAX86_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteSubWithBorrow(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  return YCPUExecuteSubCommon(
      ctx, dest, src_value, /* borrow */ true, YCPUSetFlagsAfterSub);
}

// SBB r/m8, r8
// SBB r/m16, r16
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubRegisterFromRegisterOrMemoryWithBorrow(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteSubWithBorrow(ctx, &dest, src.value);
}

// SBB r8, r/m8
// SBB r16, r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubRegisterOrMemoryFromRegisterWithBorrow(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteSubWithBorrow(ctx, &dest, src.value);
}

// SBB AL, imm8
// SBB AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubImmediateFromALOrAXWithBorrow(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteSubWithBorrow(ctx, &dest, src_value);
}

// Common logic for DEC instructions
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDec(const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUOperandValue src_value = 1;
  return YCPUExecuteSubCommon(
      ctx, dest, src_value, /* borrow */ false, YCPUSetFlagsAfterDec);
}

// DEC AX/CX/DX/BX/SP/BP/SI/DI
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDecRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode - 0x48);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &dest);
  return YCPUExecuteDec(ctx, &dest);
}
