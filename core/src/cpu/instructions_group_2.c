#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "cycles.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 2 - ROL, ROR, RCL, RCR, SHL, SHR, SAL, SAR
// ============================================================================

typedef InstructionResult (*Group2ExecuteInstructionFn)(
    const InstructionContext* ctx, Operand* op, uint8_t count);

// The 8086/8088 shifts one bit at a time and recomputes the overflow flag on
// every pass, so a multi-bit shift leaves behind the flag from its last pass
// rather than an undefined value. Each rule below is that last pass written in
// closed form.

// Overflow after a left shift or rotate: the bit shifted out of the top
// differs from the sign bit left behind, which is to say the last pass changed
// the sign of the value.
static void SetOverflowFlagAfterLeftShift(
    const InstructionContext* ctx, uint32_t result, bool carry) {
  const bool result_sign = (result & kSignBit[ctx->metadata->width]) != 0;
  CPUSetFlag(ctx->cpu, kOF, carry != result_sign);
}

// Overflow after a right rotate: the top two bits of the result differ. The
// bit rotated into the top came from the bottom, so this again says the last
// pass changed the sign of the value.
static void SetOverflowFlagAfterRightRotate(
    const InstructionContext* ctx, uint32_t result) {
  const uint32_t sign_bit = kSignBit[ctx->metadata->width];
  const bool result_sign = (result & sign_bit) != 0;
  const bool below_sign = (result & (sign_bit >> 1)) != 0;
  CPUSetFlag(ctx->cpu, kOF, result_sign != below_sign);
}

// The 8086/8088 does not mask the shift count the way the 80186 and later do,
// so a count taken from CL can be as large as 255. Once an operand has been
// shifted past its own width there is nothing left in it and nothing left to
// fall out of it, so every larger count behaves alike. Clamping to just past
// the width keeps that true while holding the shifts below the width of the
// intermediate they are computed in, where C leaves them undefined.
static uint8_t ClampShiftCount(const InstructionContext* ctx, uint8_t count) {
  const uint8_t limit = kNumBits[ctx->metadata->width] + 1;
  return count > limit ? limit : count;
}

// SHL r/m8, 1
// SHL r/m16, 1
// SHL r/m8, CL
// SHL r/m16, CL
static InstructionResult ExecuteGroup2Shl(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kInstructionExecuted;
  }
  count = ClampShiftCount(ctx, count);
  uint32_t value = FromOperand(op);
  uint32_t result = (value << count) & kMaxValue[ctx->metadata->width];
  WriteOperand(ctx, op, result);
  bool last_msb =
      ((value << (count - 1)) & kSignBit[ctx->metadata->width]) != 0;
  CPUSetFlag(ctx->cpu, kCF, last_msb);
  SetOverflowFlagAfterLeftShift(ctx, result, last_msb);
  SetCommonFlagsAfterInstruction(ctx, result);
  return kInstructionExecuted;
}

// SHR r/m8, 1
// SHR r/m16, 1
// SHR r/m8, CL
// SHR r/m16, CL
YAX86_HOT static InstructionResult ExecuteGroup2Shr(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kInstructionExecuted;
  }
  count = ClampShiftCount(ctx, count);
  uint32_t value = FromOperand(op);
  uint32_t result = value >> count;
  WriteOperand(ctx, op, result);
  bool last_lsb = ((value >> (count - 1)) & 1) != 0;
  CPUSetFlag(ctx->cpu, kCF, last_lsb);
  // A right shift clears the sign bit on its first pass, so only a shift by
  // one can leave a sign change behind.
  bool original_msb = ((value & kSignBit[ctx->metadata->width]) != 0);
  CPUSetFlag(ctx->cpu, kOF, count == 1 && original_msb);
  SetCommonFlagsAfterInstruction(ctx, result);
  return kInstructionExecuted;
}

// SAR r/m8, 1
// SAR r/m16, 1
// SAR r/m8, CL
// SAR r/m16, CL
static InstructionResult ExecuteGroup2Sar(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kInstructionExecuted;
  }
  count = ClampShiftCount(ctx, count);
  int32_t value = FromSignedOperand(op);
  int32_t result = value >> count;
  WriteOperand(ctx, op, result);
  bool last_lsb = ((value >> (count - 1)) & 1) != 0;
  CPUSetFlag(ctx->cpu, kCF, last_lsb);
  // An arithmetic right shift preserves the sign bit, so it can never
  // overflow.
  CPUSetFlag(ctx->cpu, kOF, false);
  SetCommonFlagsAfterInstruction(ctx, result);
  return kInstructionExecuted;
}

// ROL r/m8, 1
// ROL r/m16, 1
// ROL r/m8, CL
// ROL r/m16, CL
static InstructionResult ExecuteGroup2Rol(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kInstructionExecuted;
  }
  // The 8086 computes the modulus of the count after the zero check, which is
  // different than the 80286 and later processors.
  uint8_t effective_count = count % kNumBits[ctx->metadata->width];
  uint32_t value = FromOperand(op);
  uint32_t result =
      (value << effective_count) |
      (value >> (kNumBits[ctx->metadata->width] - effective_count));
  WriteOperand(ctx, op, result);
  bool last_msb = (result & 1) != 0;
  CPUSetFlag(ctx->cpu, kCF, last_msb);
  SetOverflowFlagAfterLeftShift(ctx, result, last_msb);
  return kInstructionExecuted;
}

// ROR r/m8, 1
// ROR r/m16, 1
// ROR r/m8, CL
// ROR r/m16, CL
static InstructionResult ExecuteGroup2Ror(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kInstructionExecuted;
  }
  // The 8086 computes the modulus of the count after the zero check, which is
  // different than the 80286 and later processors.
  uint8_t effective_count = count % kNumBits[ctx->metadata->width];
  uint32_t value = FromOperand(op);
  uint32_t result =
      (value >> effective_count) |
      (value << (kNumBits[ctx->metadata->width] - effective_count));
  WriteOperand(ctx, op, result);
  bool last_lsb = (result & kSignBit[ctx->metadata->width]) != 0;
  CPUSetFlag(ctx->cpu, kCF, last_lsb);
  SetOverflowFlagAfterRightRotate(ctx, result);
  return kInstructionExecuted;
}

// RCL r/m8, 1
// RCL r/m16, 1
// RCL r/m8, CL
// RCL r/m16, CL
static InstructionResult ExecuteGroup2Rcl(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kInstructionExecuted;
  }
  const Width width = ctx->metadata->width;
  // Rotating through the carry flag cycles over one more bit than the operand
  // is wide. A whole number of cycles puts the value and the carry back the
  // way they were, but the passes still happened, so the overflow flag is
  // still left over from the last one.
  uint8_t effective_count = count % (kNumBits[width] + 1);
  uint32_t value = FromOperand(op);
  uint32_t result = value;
  bool last_msb = CPUGetFlag(ctx->cpu, kCF);
  if (effective_count > 0) {
    uint32_t cf_value = last_msb ? (1 << (effective_count - 1)) : 0;
    result = ((value << effective_count) | cf_value |
              (value >> (kNumBits[width] - (effective_count - 1)))) &
             kMaxValue[width];
    WriteOperand(ctx, op, result);
    last_msb = ((value << (effective_count - 1)) & kSignBit[width]) != 0;
  }
  CPUSetFlag(ctx->cpu, kCF, last_msb);
  SetOverflowFlagAfterLeftShift(ctx, result, last_msb);
  return kInstructionExecuted;
}

// RCR r/m8, 1
// RCR r/m16, 1
// RCR r/m8, CL
// RCR r/m16, CL
static InstructionResult ExecuteGroup2Rcr(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kInstructionExecuted;
  }
  const Width width = ctx->metadata->width;
  // As with RCL, a whole number of cycles restores the value and the carry but
  // still leaves the overflow flag from the last pass.
  uint8_t effective_count = count % (kNumBits[width] + 1);
  uint32_t value = FromOperand(op);
  uint32_t result = value;
  bool last_lsb = CPUGetFlag(ctx->cpu, kCF);
  if (effective_count > 0) {
    uint32_t cf_value =
        last_lsb ? (kSignBit[width] >> (effective_count - 1)) : 0;
    result = ((value >> effective_count) | cf_value |
              (value << (kNumBits[width] - (effective_count - 1)))) &
             kMaxValue[width];
    WriteOperand(ctx, op, result);
    last_lsb = ((value >> (effective_count - 1)) & 1) != 0;
  }
  CPUSetFlag(ctx->cpu, kCF, last_lsb);
  SetOverflowFlagAfterRightRotate(ctx, result);
  return kInstructionExecuted;
}

// SETMO r/m8, 1
// SETMO r/m16, 1
// SETMOC r/m8, CL
// SETMOC r/m16, CL
//
// Undocumented. REG 6 of the shift group is a distinct operation on the
// 8086/8088 rather than an alias of SAL: it sets every bit of the operand,
// hence "set minus one". The count still gates it, so the CL forms - SETMOC,
// "set minus one conditional" - do nothing at all when CL is zero.
//
// Nothing an IBM PC/XT runs uses this, but leaving REG 6 aliased to SAL would
// silently give a different answer than the hardware for the same encoding,
// and the operation is a single store.
static InstructionResult ExecuteGroup2Setmo(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kInstructionExecuted;
  }
  const uint32_t result = kMaxValue[ctx->metadata->width];
  WriteOperand(ctx, op, result);
  CPUSetFlag(ctx->cpu, kCF, false);
  CPUSetFlag(ctx->cpu, kAF, false);
  CPUSetFlag(ctx->cpu, kOF, false);
  SetCommonFlagsAfterInstruction(ctx, result);
  return kInstructionExecuted;
}

static const Group2ExecuteInstructionFn kGroup2ExecuteInstructionFns[] = {
    ExecuteGroup2Rol,    // 0 - ROL
    ExecuteGroup2Ror,    // 1 - ROR
    ExecuteGroup2Rcl,    // 2 - RCL
    ExecuteGroup2Rcr,    // 3 - RCR
    ExecuteGroup2Shl,    // 4 - SHL
    ExecuteGroup2Shr,    // 5 - SHR
    ExecuteGroup2Setmo,  // 6 - SETMO / SETMOC
    ExecuteGroup2Sar,    // 7 - SAR
};

// Group 2 shift / rotate by 1.
YAX86_PRIVATE InstructionResult
ExecuteGroup2ShiftOrRotateBy1Instruction(const InstructionContext* ctx) {
  const Group2ExecuteInstructionFn fn =
      kGroup2ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand op = ReadRegisterOrMemoryOperand(ctx);
  return fn(ctx, &op, 1);
}

// Group 2 shift / rotate by CL.
YAX86_PRIVATE InstructionResult
ExecuteGroup2ShiftOrRotateByCLInstruction(const InstructionContext* ctx) {
  const Group2ExecuteInstructionFn fn =
      kGroup2ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand op = ReadRegisterOrMemoryOperand(ctx);
  const uint8_t count = ctx->cpu->registers[kCX] & 0xFF;
  // A shift by CL works through the count a bit at a time.
  CPUAddCycles(ctx->cpu, (uint16_t)count * kShiftCyclesPerBit);
  return fn(ctx, &op, count);
}
