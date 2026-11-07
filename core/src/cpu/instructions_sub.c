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
YAX86_FILE_PRIVATE void SetFlagsAfterDec(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_borrow) {
  SetCommonFlagsAfterInstruction(ctx, result);

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
  CPUSetFlag(
      ctx->cpu, kOF, (op1_sign != op2_sign) && (result_sign != op1_sign));

  // Auxiliary Carry Flag (AF) - borrow from bit 3 to bit 4
  CPUSetFlag(ctx->cpu, kAF, (op1 & 0xF) < ((op2 & 0xF) + (did_borrow ? 1 : 0)));
}

// Set CPU flags after a SUB, SBB, CMP or NEG instruction.
// This calls SetFlagsAfterDec and then sets the Carry Flag (CF).
YAX86_MODULE_PRIVATE void SetFlagsAfterSub(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_borrow) {
  SetFlagsAfterDec(ctx, op1, op2, result, did_borrow);

  // Carry Flag (CF) - Set when a borrow is generated
  // CF is set if op1 < (op2 + did_borrow) (unsigned comparison)
  uint32_t val_being_subtracted =
      (op2 & kMaxValue[ctx->metadata->width]) + (did_borrow ? 1 : 0);
  CPUSetFlag(ctx->cpu, kCF, op1 < val_being_subtracted);
}

// Common signature of SetFlagsAfterSub and SetFlagsAfterDec.
typedef void (*YCPUSetFlagsAfterSubFn)(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_borrow);

// Common logic for SUB, SBB, and DEC instructions.
YAX86_FILE_PRIVATE YAX86_HOT YCPUInstructionResult ExecuteSubCommon(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value, bool borrow,
    YCPUSetFlagsAfterSubFn set_flags_after_fn) {
  uint32_t raw_dest_value = FromOperand(dest);
  uint32_t raw_src_value = FromOperandValue(src_value);
  bool should_borrow = borrow && CPUGetFlag(ctx->cpu, kCF);
  uint32_t result = raw_dest_value - raw_src_value - (should_borrow ? 1 : 0);
  WriteOperand(ctx, dest, result);
  (*set_flags_after_fn)(
      ctx, raw_dest_value, raw_src_value, result, should_borrow);
  return kInstructionExecuted;
}

// Common logic for SUB instructions
YAX86_MODULE_PRIVATE YCPUInstructionResult ExecuteSub(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  return ExecuteSubCommon(
      ctx, dest, src_value, /* borrow */ false, SetFlagsAfterSub);
}

// SUB r/m8, r8
// SUB r/m16, r16
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
ExecuteSubRegisterFromRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  ReadRegisterOperand(ctx, &src);
  return ExecuteSub(ctx, &dest, src.value);
}

// SUB r8, r/m8
// SUB r16, r/m16
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
ExecuteSubRegisterOrMemoryFromRegister(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  ReadRegisterOrMemoryOperand(ctx, &src);
  return ExecuteSub(ctx, &dest, src.value);
}

// SUB AL, imm8
// SUB AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteSubImmediateFromALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);
  YCPUOperandValue src_value = ReadImmediate(ctx);
  return ExecuteSub(ctx, &dest, src_value);
}

// Common logic for SBB instructions
YAX86_MODULE_PRIVATE YCPUInstructionResult ExecuteSubWithBorrow(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  return ExecuteSubCommon(
      ctx, dest, src_value, /* borrow */ true, SetFlagsAfterSub);
}

// SBB r/m8, r8
// SBB r/m16, r16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteSubRegisterFromRegisterOrMemoryWithBorrow(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  ReadRegisterOperand(ctx, &src);
  return ExecuteSubWithBorrow(ctx, &dest, src.value);
}

// SBB r8, r/m8
// SBB r16, r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteSubRegisterOrMemoryFromRegisterWithBorrow(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  ReadRegisterOrMemoryOperand(ctx, &src);
  return ExecuteSubWithBorrow(ctx, &dest, src.value);
}

// SBB AL, imm8
// SBB AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteSubImmediateFromALOrAXWithBorrow(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);
  YCPUOperandValue src_value = ReadImmediate(ctx);
  return ExecuteSubWithBorrow(ctx, &dest, src_value);
}

// Common logic for DEC instructions
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteDec(const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUOperandValue src_value = 1;
  return ExecuteSubCommon(
      ctx, dest, src_value, /* borrow */ false, SetFlagsAfterDec);
}

// DEC AX/CX/DX/BX/SP/BP/SI/DI
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteDecRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode - 0x48);
  YCPUOperand dest;
  ReadRegisterOperandForRegisterIndex(ctx, register_index, &dest);
  return ExecuteDec(ctx, &dest);
}
