#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 3 - TEST, NOT, NEG, MUL, IMUL, DIV, IDIV
// ============================================================================

typedef InstructionResult (*Group3ExecuteInstructionFn)(
    const InstructionContext* ctx, Operand* op);

// TEST r/m8, imm8
// TEST r/m16, imm16
static InstructionResult ExecuteGroup3Test(
    const InstructionContext* ctx, Operand* op) {
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteTest(ctx, op, &src_value);
}

// NOT r/m8
// NOT r/m16
static InstructionResult ExecuteNot(
    const InstructionContext* ctx, Operand* op) {
  WriteOperand(ctx, op, ~FromOperand(op));
  return kInstructionExecuted;
}

// NEG r/m8
// NEG r/m16
static InstructionResult ExecuteNeg(
    const InstructionContext* ctx, Operand* op) {
  int32_t op_value = FromSignedOperand(op);
  int32_t result_value = -op_value;
  WriteOperand(ctx, op, result_value);
  SetFlagsAfterSub(ctx, 0, op_value, result_value, false);
  return kInstructionExecuted;
}

// Table of where to store the higher half of the result for
// MUL, IMUL, DIV, and IDIV instructions, indexed by the data width.
static const OperandAddress kMulDivResultHighHalfAddress[kNumWidths] = {
    {.type = kOperandAddressTypeRegister,
     .value =
         {
             .register_address =
                 {
                     .register_index = kAX,
                     .byte_offset = 8,
                 },
         }},
    {.type = kOperandAddressTypeRegister,
     .value = {
         .register_address =
             {
                 .register_index = kDX,
             },
     }}};

// Number of bits to shift to extract the high part of the result of MUL, IMUL,
// DIV, and IDIV instructions, indexed by the data width.
static const uint8_t kMulDivResultHighHalfShiftWidth[kNumWidths] = {
    8,   // kByte
    16,  // kWord
};

// Common logic for MUL and IMUL instructions.
static InstructionResult ExecuteMulCommon(
    const InstructionContext* ctx, Operand* dest, uint32_t result,
    bool overflow) {
  Width width = ctx->metadata->width;

  uint32_t result_low_half = result & kMaxValue[width];
  WriteOperand(ctx, dest, result_low_half);

  uint32_t result_high_half =
      (result >> kMulDivResultHighHalfShiftWidth[width]) & kMaxValue[width];
  WriteOperandAddress(
      ctx, &kMulDivResultHighHalfAddress[width], result_high_half);

  CPUSetFlag(ctx->cpu, kCF, overflow);
  CPUSetFlag(ctx->cpu, kOF, overflow);

  return kInstructionExecuted;
}

// MUL r/m8
// MUL r/m16
static InstructionResult ExecuteMul(
    const InstructionContext* ctx, Operand* op) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  uint32_t result = FromOperand(&dest) * FromOperand(op);
  return ExecuteMulCommon(
      ctx, &dest, result, result > kMaxValue[ctx->metadata->width]);
}

// IMUL r/m8
// IMUL r/m16
static InstructionResult ExecuteImul(
    const InstructionContext* ctx, Operand* op) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  int32_t result = FromSignedOperand(&dest) * FromSignedOperand(op);
  return ExecuteMulCommon(
      ctx, &dest, result,
      result > kMaxSignedValue[ctx->metadata->width] ||
          result < kMinSignedValue[ctx->metadata->width]);
}

static InstructionResult WriteDivResult(
    const InstructionContext* ctx, Operand* dest, uint32_t quotient,
    uint32_t remainder) {
  WriteOperand(ctx, dest, quotient);
  WriteOperandAddress(
      ctx, &kMulDivResultHighHalfAddress[ctx->metadata->width], remainder);
  return kInstructionExecuted;
}

// DIV r/m8
// DIV r/m16
static InstructionResult ExecuteDiv(
    const InstructionContext* ctx, Operand* op) {
  uint32_t divisor = FromOperand(op);
  if (divisor == 0) {
    CPURaiseInternalInterrupt(ctx->cpu, kInterruptDivideError);
    return kInstructionExecuted;
  }

  Width width = ctx->metadata->width;
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);

  OperandValue dest_high_half =
      ReadOperandValue(ctx, &kMulDivResultHighHalfAddress[width]);
  uint32_t dividend =
      FromOperand(&dest) | (FromOperandValue(&dest_high_half)
                            << kMulDivResultHighHalfShiftWidth[width]);
  uint32_t quotient = dividend / divisor;
  if (quotient > kMaxValue[ctx->metadata->width]) {
    CPURaiseInternalInterrupt(ctx->cpu, kInterruptDivideError);
    return kInstructionExecuted;
  }
  return WriteDivResult(ctx, &dest, quotient, dividend % divisor);
}

// IDIV r/m8
// IDIV r/m16
static InstructionResult ExecuteIdiv(
    const InstructionContext* ctx, Operand* op) {
  int32_t divisor = FromSignedOperand(op);
  if (divisor == 0) {
    CPURaiseInternalInterrupt(ctx->cpu, kInterruptDivideError);
    return kInstructionExecuted;
  }

  Width width = ctx->metadata->width;
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);

  OperandValue dest_high_half =
      ReadOperandValue(ctx, &kMulDivResultHighHalfAddress[width]);
  int32_t dividend =
      FromOperand(&dest) | (FromSignedOperandValue(&dest_high_half)
                            << kMulDivResultHighHalfShiftWidth[width]);
  int32_t quotient = dividend / divisor;
  if (quotient > kMaxSignedValue[ctx->metadata->width] ||
      quotient < kMinSignedValue[ctx->metadata->width]) {
    CPURaiseInternalInterrupt(ctx->cpu, kInterruptDivideError);
    return kInstructionExecuted;
  }
  return WriteDivResult(ctx, &dest, quotient, dividend % divisor);
}

// Group 3 instruction implementations, indexed by the corresponding REG field
// value in the ModRM byte and data width.
static const Group3ExecuteInstructionFn kGroup3ExecuteInstructionFns[] = {
    ExecuteGroup3Test,  // 0 - TEST
    // REG 1 is an undocumented alias of REG 0: the 8086/8088 does not decode
    // bit 0 of the REG field for this group.
    ExecuteGroup3Test,  // 1 - TEST
    ExecuteNot,         // 2 - NOT
    ExecuteNeg,         // 3 - NEG
    ExecuteMul,         // 4 - MUL
    ExecuteImul,        // 5 - IMUL
    ExecuteDiv,         // 6 - DIV
    ExecuteIdiv,        // 7 - IDIV
};

// Group 3 instruction handler.
YAX86_PRIVATE InstructionResult
ExecuteGroup3Instruction(const InstructionContext* ctx) {
  const Group3ExecuteInstructionFn fn =
      kGroup3ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  return fn(ctx, &dest);
}
