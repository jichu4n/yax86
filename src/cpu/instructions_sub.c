#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
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
static void SetFlagsAfterDec(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_borrow) {
  SetCommonFlagsAfterInstruction(ctx, result);

  uint32_t sign_bit = kSignBit[ctx->metadata->width];
  uint32_t max_val = kMaxValue[ctx->metadata->width];

  // Overflow Flag (OF)
  // OF is set if op1_sign != val_being_subtracted_sign AND result_sign ==
  // val_being_subtracted_sign where val_being_subtracted = (op2 + did_borrow)
  bool op1_sign = (op1 & sign_bit) != 0;
  bool result_sign = (result & sign_bit) != 0;
  uint32_t val_being_subtracted = (op2 & max_val) + (did_borrow ? 1 : 0);
  // Mask val_being_subtracted to current width before checking its sign,
  // in case (op2 & max_val) + 1 caused a temporary overflow beyond max_val if
  // op2 was max_val.
  bool val_being_subtracted_sign =
      ((val_being_subtracted & max_val) & sign_bit) != 0;
  SetFlag(
      ctx->cpu, kOF,
      (op1_sign != val_being_subtracted_sign) &&
          (result_sign == val_being_subtracted_sign));

  // Auxiliary Carry Flag (AF) - borrow from bit 3 to bit 4
  SetFlag(ctx->cpu, kAF, (op1 & 0xF) < ((op2 & 0xF) + (did_borrow ? 1 : 0)));
}

// Set CPU flags after a SUB, SBB, CMP or NEG instruction.
// This calls SetFlagsAfterDec and then sets the Carry Flag (CF).
YAX86_PRIVATE void SetFlagsAfterSub(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_borrow) {
  SetFlagsAfterDec(ctx, op1, op2, result, did_borrow);

  // Carry Flag (CF) - Set when a borrow is generated
  // CF is set if op1 < (op2 + did_borrow) (unsigned comparison)
  uint32_t val_being_subtracted =
      (op2 & kMaxValue[ctx->metadata->width]) + (did_borrow ? 1 : 0);
  SetFlag(ctx->cpu, kCF, op1 < val_being_subtracted);
}

// Common signature of SetFlagsAfterSub and SetFlagsAfterDec.
typedef void (*SetFlagsAfterSubFn)(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_borrow);

// Common logic for SUB, SBB, and DEC instructions.
static ExecuteStatus ExecuteSubCommon(
    const InstructionContext* ctx, Operand* dest, const OperandValue* src_value,
    bool borrow, SetFlagsAfterSubFn set_flags_after_fn) {
  uint32_t raw_dest_value = FromOperand(dest);
  uint32_t raw_src_value = FromOperandValue(src_value);
  bool should_borrow = borrow && GetFlag(ctx->cpu, kCF);
  uint32_t result = raw_dest_value - raw_src_value - (should_borrow ? 1 : 0);
  WriteOperand(ctx, dest, result);
  (*set_flags_after_fn)(
      ctx, raw_dest_value, raw_src_value, result, should_borrow);
  return kExecuteSuccess;
}

// Common logic for SUB instructions
YAX86_PRIVATE ExecuteStatus ExecuteSub(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  return ExecuteSubCommon(
      ctx, dest, src_value, /* borrow */ false, SetFlagsAfterSub);
}

// SUB r/m8, r8
// SUB r/m16, r16
YAX86_PRIVATE ExecuteStatus
ExecuteSubRegisterFromRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteSub(ctx, &dest, &src.value);
}

// SUB r8, r/m8
// SUB r16, r/m16
YAX86_PRIVATE ExecuteStatus
ExecuteSubRegisterOrMemoryFromRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteSub(ctx, &dest, &src.value);
}

// SUB AL, imm8
// SUB AX, imm16
YAX86_PRIVATE ExecuteStatus
ExecuteSubImmediateFromALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteSub(ctx, &dest, &src_value);
}

// Common logic for SBB instructions
YAX86_PRIVATE ExecuteStatus ExecuteSubWithBorrow(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  return ExecuteSubCommon(
      ctx, dest, src_value, /* borrow */ true, SetFlagsAfterSub);
}

// SBB r/m8, r8
// SBB r/m16, r16
YAX86_PRIVATE ExecuteStatus ExecuteSubRegisterFromRegisterOrMemoryWithBorrow(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteSubWithBorrow(ctx, &dest, &src.value);
}

// SBB r8, r/m8
// SBB r16, r/m16
YAX86_PRIVATE ExecuteStatus ExecuteSubRegisterOrMemoryFromRegisterWithBorrow(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteSubWithBorrow(ctx, &dest, &src.value);
}

// SBB AL, imm8
// SBB AX, imm16
YAX86_PRIVATE ExecuteStatus
ExecuteSubImmediateFromALOrAXWithBorrow(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteSubWithBorrow(ctx, &dest, &src_value);
}

// Common logic for DEC instructions
YAX86_PRIVATE ExecuteStatus
ExecuteDec(const InstructionContext* ctx, Operand* dest) {
  OperandValue src_value = WordValue(1);
  return ExecuteSubCommon(
      ctx, dest, &src_value, /* borrow */ false, SetFlagsAfterDec);
}

// DEC AX/CX/DX/BX/SP/BP/SI/DI
YAX86_PRIVATE ExecuteStatus ExecuteDecRegister(const InstructionContext* ctx) {
  RegisterIndex register_index =
      (RegisterIndex)(ctx->instruction->opcode - 0x48);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  return ExecuteDec(ctx, &dest);
}
