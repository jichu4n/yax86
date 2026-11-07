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

typedef YCPUInstructionResult (*YCPUGroup2ExecuteInstructionFn)(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count);

// The 8086/8088 shifts one bit at a time and recomputes the overflow flag on
// every pass, so a multi-bit shift leaves behind the flag from its last pass
// rather than an undefined value. Each rule below is that last pass written in
// closed form.

// Overflow after a left shift or rotate: the bit shifted out of the top
// differs from the sign bit left behind, which is to say the last pass changed
// the sign of the value.
YAX86_FILE_PRIVATE void YCPUSetOverflowFlagAfterLeftShift(
    const YCPUInstructionContext* ctx, uint32_t result, bool carry) {
  const bool result_sign = (result & YCPUSignBit[ctx->metadata->width]) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_OF, carry != result_sign);
}

// Overflow after a right rotate: the top two bits of the result differ. The
// bit rotated into the top came from the bottom, so this again says the last
// pass changed the sign of the value.
YAX86_FILE_PRIVATE void YCPUSetOverflowFlagAfterRightRotate(
    const YCPUInstructionContext* ctx, uint32_t result) {
  const uint32_t sign_bit = YCPUSignBit[ctx->metadata->width];
  const bool result_sign = (result & sign_bit) != 0;
  const bool below_sign = (result & (sign_bit >> 1)) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_OF, result_sign != below_sign);
}

// The 8086/8088 does not mask the shift count the way the 80186 and later do,
// so a count taken from CL can be as large as 255. Once an operand has been
// shifted past its own width there is nothing left in it and nothing left to
// fall out of it, so every larger count behaves alike. Clamping to just past
// the width keeps that true while holding the shifts below the width of the
// intermediate they are computed in, where C leaves them undefined.
YAX86_FILE_PRIVATE uint8_t
YCPUClampShiftCount(const YCPUInstructionContext* ctx, uint8_t count) {
  const uint8_t limit = YCPUNumBits[ctx->metadata->width] + 1;
  return count > limit ? limit : count;
}

// SHL r/m8, 1
// SHL r/m16, 1
// SHL r/m8, CL
// SHL r/m16, CL
YAX86_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Shl(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  count = YCPUClampShiftCount(ctx, count);
  uint32_t value = YCPUFromOperand(op);
  uint32_t result = (value << count) & YCPUMaxValue[ctx->metadata->width];
  YCPUWriteOperand(ctx, op, result);
  bool last_msb =
      ((value << (count - 1)) & YCPUSignBit[ctx->metadata->width]) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_msb);
  YCPUSetOverflowFlagAfterLeftShift(ctx, result, last_msb);
  YCPUSetCommonFlagsAfterInstruction(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// SHR r/m8, 1
// SHR r/m16, 1
// SHR r/m8, CL
// SHR r/m16, CL
YAX86_FILE_PRIVATE YAX86_HOT YCPUInstructionResult YCPUExecuteGroup2Shr(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  count = YCPUClampShiftCount(ctx, count);
  uint32_t value = YCPUFromOperand(op);
  uint32_t result = value >> count;
  YCPUWriteOperand(ctx, op, result);
  bool last_lsb = ((value >> (count - 1)) & 1) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_lsb);
  // A right shift clears the sign bit on its first pass, so only a shift by
  // one can leave a sign change behind.
  bool original_msb = ((value & YCPUSignBit[ctx->metadata->width]) != 0);
  YCPUSetFlag(ctx->cpu, Y_CPU_OF, count == 1 && original_msb);
  YCPUSetCommonFlagsAfterInstruction(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// SAR r/m8, 1
// SAR r/m16, 1
// SAR r/m8, CL
// SAR r/m16, CL
YAX86_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Sar(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  count = YCPUClampShiftCount(ctx, count);
  int32_t value = YCPUFromSignedOperand(ctx->metadata->width, op);
  int32_t result = value >> count;
  YCPUWriteOperand(ctx, op, result);
  bool last_lsb = ((value >> (count - 1)) & 1) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_lsb);
  // An arithmetic right shift preserves the sign bit, so it can never
  // overflow.
  YCPUSetFlag(ctx->cpu, Y_CPU_OF, false);
  YCPUSetCommonFlagsAfterInstruction(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ROL r/m8, 1
// ROL r/m16, 1
// ROL r/m8, CL
// ROL r/m16, CL
YAX86_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Rol(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  // The 8086 computes the modulus of the count after the zero check, which is
  // different than the 80286 and later processors.
  uint8_t effective_count = count % YCPUNumBits[ctx->metadata->width];
  uint32_t value = YCPUFromOperand(op);
  uint32_t result =
      (value << effective_count) |
      (value >> (YCPUNumBits[ctx->metadata->width] - effective_count));
  YCPUWriteOperand(ctx, op, result);
  bool last_msb = (result & 1) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_msb);
  YCPUSetOverflowFlagAfterLeftShift(ctx, result, last_msb);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ROR r/m8, 1
// ROR r/m16, 1
// ROR r/m8, CL
// ROR r/m16, CL
YAX86_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Ror(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  // The 8086 computes the modulus of the count after the zero check, which is
  // different than the 80286 and later processors.
  uint8_t effective_count = count % YCPUNumBits[ctx->metadata->width];
  uint32_t value = YCPUFromOperand(op);
  uint32_t result =
      (value >> effective_count) |
      (value << (YCPUNumBits[ctx->metadata->width] - effective_count));
  YCPUWriteOperand(ctx, op, result);
  bool last_lsb = (result & YCPUSignBit[ctx->metadata->width]) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_lsb);
  YCPUSetOverflowFlagAfterRightRotate(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// RCL r/m8, 1
// RCL r/m16, 1
// RCL r/m8, CL
// RCL r/m16, CL
YAX86_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Rcl(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  const YCPUWidth width = ctx->metadata->width;
  // Rotating through the carry flag cycles over one more bit than the operand
  // is wide. A whole number of cycles puts the value and the carry back the
  // way they were, but the passes still happened, so the overflow flag is
  // still left over from the last one.
  uint8_t effective_count = count % (YCPUNumBits[width] + 1);
  uint32_t value = YCPUFromOperand(op);
  uint32_t result = value;
  bool last_msb = YCPUGetFlag(ctx->cpu, Y_CPU_CF);
  if (effective_count > 0) {
    uint32_t cf_value = last_msb ? (1 << (effective_count - 1)) : 0;
    result = ((value << effective_count) | cf_value |
              (value >> (YCPUNumBits[width] - (effective_count - 1)))) &
             YCPUMaxValue[width];
    YCPUWriteOperand(ctx, op, result);
    last_msb = ((value << (effective_count - 1)) & YCPUSignBit[width]) != 0;
  }
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_msb);
  YCPUSetOverflowFlagAfterLeftShift(ctx, result, last_msb);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// RCR r/m8, 1
// RCR r/m16, 1
// RCR r/m8, CL
// RCR r/m16, CL
YAX86_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Rcr(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  const YCPUWidth width = ctx->metadata->width;
  // As with RCL, a whole number of cycles restores the value and the carry but
  // still leaves the overflow flag from the last pass.
  uint8_t effective_count = count % (YCPUNumBits[width] + 1);
  uint32_t value = YCPUFromOperand(op);
  uint32_t result = value;
  bool last_lsb = YCPUGetFlag(ctx->cpu, Y_CPU_CF);
  if (effective_count > 0) {
    uint32_t cf_value =
        last_lsb ? (YCPUSignBit[width] >> (effective_count - 1)) : 0;
    result = ((value >> effective_count) | cf_value |
              (value << (YCPUNumBits[width] - (effective_count - 1)))) &
             YCPUMaxValue[width];
    YCPUWriteOperand(ctx, op, result);
    last_lsb = ((value >> (effective_count - 1)) & 1) != 0;
  }
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_lsb);
  YCPUSetOverflowFlagAfterRightRotate(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
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
YAX86_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Setmo(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  const uint32_t result = YCPUMaxValue[ctx->metadata->width];
  YCPUWriteOperand(ctx, op, result);
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, false);
  YCPUSetFlag(ctx->cpu, Y_CPU_AF, false);
  YCPUSetFlag(ctx->cpu, Y_CPU_OF, false);
  YCPUSetCommonFlagsAfterInstruction(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

YAX86_FILE_PRIVATE const YCPUGroup2ExecuteInstructionFn
    YCPUGroup2ExecuteInstructionFns[] = {
        YCPUExecuteGroup2Rol,    // 0 - ROL
        YCPUExecuteGroup2Ror,    // 1 - ROR
        YCPUExecuteGroup2Rcl,    // 2 - RCL
        YCPUExecuteGroup2Rcr,    // 3 - RCR
        YCPUExecuteGroup2Shl,    // 4 - SHL
        YCPUExecuteGroup2Shr,    // 5 - SHR
        YCPUExecuteGroup2Setmo,  // 6 - SETMO / SETMOC
        YCPUExecuteGroup2Sar,    // 7 - SAR
};

// Group 2 shift / rotate by 1.
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup2ShiftOrRotateBy1Instruction(
    const YCPUInstructionContext* ctx) {
  const YCPUGroup2ExecuteInstructionFn fn =
      YCPUGroup2ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand op;
  YCPUReadRegisterOrMemoryOperand(ctx, &op);
  return fn(ctx, &op, 1);
}

// Group 2 shift / rotate by CL.
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup2ShiftOrRotateByCLInstruction(
    const YCPUInstructionContext* ctx) {
  const YCPUGroup2ExecuteInstructionFn fn =
      YCPUGroup2ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand op;
  YCPUReadRegisterOrMemoryOperand(ctx, &op);
  const uint8_t count = ctx->cpu->registers[Y_CPU_CX] & 0xFF;
  // A shift by CL works through the count a bit at a time.
  YCPUAddCycles(ctx->cpu, (uint16_t)count * Y_CPU_SHIFT_CYCLES_PER_BIT);
  return fn(ctx, &op, count);
}
