#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 3 - TEST, NOT, NEG, MUL, IMUL, DIV, IDIV
// ============================================================================

typedef YCPUInstructionResult (*YCPUGroup3ExecuteInstructionFn)(
    const YCPUInstructionContext* ctx, YCPUOperand* op);

// TEST r/m8, imm8
// TEST r/m16, imm16
YAX86_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup3Test(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteTest(ctx, op, src_value);
}

// NOT r/m8
// NOT r/m16
YAX86_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteNot(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  YCPUWriteOperand(ctx, op, ~YCPUFromOperand(op));
  return kInstructionExecuted;
}

// NEG r/m8
// NEG r/m16
YAX86_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteNeg(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  int32_t op_value = YCPUFromSignedOperand(ctx->metadata->width, op);
  int32_t result_value = -op_value;
  YCPUWriteOperand(ctx, op, result_value);
  YCPUSetFlagsAfterSub(ctx, 0, op_value, result_value, false);
  return kInstructionExecuted;
}

// Table of where to store the higher half of the result for
// MUL, IMUL, DIV, and IDIV instructions, indexed by the data width.
YAX86_FILE_PRIVATE const YCPUOperandAddress
    kMulDivResultHighHalfAddress[kNumWidths] = {
        {.type = kOperandAddressTypeRegister,
         .register_index = kAX,
         .offset = 8},
        {.type = kOperandAddressTypeRegister,
         .register_index = kDX,
         .offset = 0},
};

// Number of bits to shift to extract the high part of the result of MUL, IMUL,
// DIV, and IDIV instructions, indexed by the data width.
YAX86_FILE_PRIVATE const uint8_t kMulDivResultHighHalfShiftWidth[kNumWidths] = {
    8,   // kByte
    16,  // kWord
};

// What the multiply and divide instructions cost. These dominate their own
// timing by an order of magnitude - the 8086/8088 works through them a bit at
// a time in microcode - so unlike everything else in the cycle table they are
// worth charging individually. The published figures are ranges that depend on
// the operands; these are the low end of each.
YAX86_FILE_PRIVATE const uint16_t kMulDivCycles[kNumWidths][2] = {
    // kByte: unsigned, signed
    {70, 80},
    // kWord: unsigned, signed
    {118, 128},
};

// Common logic for MUL and IMUL instructions.
YAX86_FILE_PRIVATE YCPUInstructionResult YCPUExecuteMulCommon(
    const YCPUInstructionContext* ctx, YCPUOperand* dest, uint32_t result,
    bool overflow) {
  YCPUWidth width = ctx->metadata->width;

  uint32_t result_low_half = result & kMaxValue[width];
  YCPUWriteOperand(ctx, dest, result_low_half);

  uint32_t result_high_half =
      (result >> kMulDivResultHighHalfShiftWidth[width]) & kMaxValue[width];
  YCPUWriteOperandAddress(
      ctx, &kMulDivResultHighHalfAddress[width], result_high_half);

  YCPUSetFlag(ctx->cpu, kCF, overflow);
  YCPUSetFlag(ctx->cpu, kOF, overflow);

  return kInstructionExecuted;
}

// MUL r/m8
// MUL r/m16
YAX86_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteMul(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  YCPUAddCycles(ctx->cpu, kMulDivCycles[ctx->metadata->width][0]);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);
  uint32_t result = YCPUFromOperand(&dest) * YCPUFromOperand(op);
  return YCPUExecuteMulCommon(
      ctx, &dest, result, result > kMaxValue[ctx->metadata->width]);
}

// IMUL r/m8
// IMUL r/m16
YAX86_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteImul(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  YCPUAddCycles(ctx->cpu, kMulDivCycles[ctx->metadata->width][1]);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);
  const YCPUWidth width = ctx->metadata->width;
  int32_t result =
      YCPUFromSignedOperand(width, &dest) * YCPUFromSignedOperand(width, op);
  return YCPUExecuteMulCommon(
      ctx, &dest, result,
      result > kMaxSignedValue[ctx->metadata->width] ||
          result < kMinSignedValue[ctx->metadata->width]);
}

YAX86_FILE_PRIVATE YCPUInstructionResult YCPUWriteDivResult(
    const YCPUInstructionContext* ctx, YCPUOperand* dest, uint32_t quotient,
    uint32_t remainder) {
  YCPUWriteOperand(ctx, dest, quotient);
  YCPUWriteOperandAddress(
      ctx, &kMulDivResultHighHalfAddress[ctx->metadata->width], remainder);
  return kInstructionExecuted;
}

// DIV r/m8
// DIV r/m16
YAX86_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteDiv(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  YCPUAddCycles(ctx->cpu, kMulDivCycles[ctx->metadata->width][0]);
  uint32_t divisor = YCPUFromOperand(op);
  if (divisor == 0) {
    YCPURaiseInternalInterrupt(ctx->cpu, kInterruptDivideError);
    return kInstructionExecuted;
  }

  YCPUWidth width = ctx->metadata->width;
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);

  YCPUOperandValue dest_high_half =
      YCPUReadOperandValue(ctx, &kMulDivResultHighHalfAddress[width]);
  uint32_t dividend =
      YCPUFromOperand(&dest) | (YCPUFromOperandValue(dest_high_half)
                                << kMulDivResultHighHalfShiftWidth[width]);
  uint32_t quotient = dividend / divisor;
  if (quotient > kMaxValue[ctx->metadata->width]) {
    YCPURaiseInternalInterrupt(ctx->cpu, kInterruptDivideError);
    return kInstructionExecuted;
  }
  return YCPUWriteDivResult(ctx, &dest, quotient, dividend % divisor);
}

// IDIV r/m8
// IDIV r/m16
YAX86_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteIdiv(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  YCPUAddCycles(ctx->cpu, kMulDivCycles[ctx->metadata->width][1]);
  int32_t divisor = YCPUFromSignedOperand(ctx->metadata->width, op);
  if (divisor == 0) {
    YCPURaiseInternalInterrupt(ctx->cpu, kInterruptDivideError);
    return kInstructionExecuted;
  }

  YCPUWidth width = ctx->metadata->width;
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);

  YCPUOperandValue dest_high_half =
      YCPUReadOperandValue(ctx, &kMulDivResultHighHalfAddress[width]);
  int32_t dividend = YCPUFromOperand(&dest) |
                     (YCPUFromSignedOperandValue(width, dest_high_half)
                      << kMulDivResultHighHalfShiftWidth[width]);
  int32_t quotient = dividend / divisor;
  // The 8086/8088 divides the magnitudes and checks the result against the
  // largest quotient that fits, so it rejects a quotient of -128 or -32768
  // even though those are representable. The valid range is symmetric, one
  // narrower at the bottom than the operand's own range.
  if (quotient > kMaxSignedValue[ctx->metadata->width] ||
      quotient < -kMaxSignedValue[ctx->metadata->width]) {
    YCPURaiseInternalInterrupt(ctx->cpu, kInterruptDivideError);
    return kInstructionExecuted;
  }
  return YCPUWriteDivResult(ctx, &dest, quotient, dividend % divisor);
}

// Group 3 instruction implementations, indexed by the corresponding REG field
// value in the YCPUModRM byte and data width.
YAX86_FILE_PRIVATE const YCPUGroup3ExecuteInstructionFn
    kGroup3ExecuteInstructionFns[] = {
        YCPUExecuteGroup3Test,  // 0 - TEST
        // REG 1 is an undocumented alias of REG 0: the 8086/8088 does not
        // decode bit 0 of the REG field for this group.
        YCPUExecuteGroup3Test,  // 1 - TEST
        YCPUExecuteNot,         // 2 - NOT
        YCPUExecuteNeg,         // 3 - NEG
        YCPUExecuteMul,         // 4 - MUL
        YCPUExecuteImul,        // 5 - IMUL
        YCPUExecuteDiv,         // 6 - DIV
        YCPUExecuteIdiv,        // 7 - IDIV
};

// Group 3 instruction handler.
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup3Instruction(const YCPUInstructionContext* ctx) {
  const YCPUGroup3ExecuteInstructionFn fn =
      kGroup3ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  return fn(ctx, &dest);
}
