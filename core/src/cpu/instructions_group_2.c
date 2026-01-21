#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 2 - ROL, ROR, RCL, RCR, SHL, SHR, SAL, SAR
// ============================================================================

typedef ExecuteStatus (*Group2ExecuteInstructionFn)(
    const InstructionContext* ctx, Operand* op, uint8_t count);

// SHL r/m8, 1
// SHL r/m16, 1
// SHL r/m8, CL
// SHL r/m16, CL
static ExecuteStatus ExecuteGroup2Shl(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kExecuteSuccess;
  }
  uint32_t value = FromOperand(op);
  uint32_t result = (value << count) & kMaxValue[ctx->metadata->width];
  WriteOperand(ctx, op, result);
  bool last_msb =
      ((value << (count - 1)) & kSignBit[ctx->metadata->width]) != 0;
  CPUSetFlag(ctx->cpu, kCF, last_msb);
  if (count == 1) {
    bool current_msb = ((result & kSignBit[ctx->metadata->width]) != 0);
    CPUSetFlag(ctx->cpu, kOF, last_msb != current_msb);
  }
  SetCommonFlagsAfterInstruction(ctx, result);
  return kExecuteSuccess;
}

// SHR r/m8, 1
// SHR r/m16, 1
// SHR r/m8, CL
// SHR r/m16, CL
static ExecuteStatus ExecuteGroup2Shr(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kExecuteSuccess;
  }
  uint32_t value = FromOperand(op);
  uint32_t result = value >> count;
  WriteOperand(ctx, op, result);
  bool last_lsb = ((value >> (count - 1)) & 1) != 0;
  CPUSetFlag(ctx->cpu, kCF, last_lsb);
  if (count == 1) {
    bool original_msb = ((value & kSignBit[ctx->metadata->width]) != 0);
    CPUSetFlag(ctx->cpu, kOF, original_msb);
  }
  SetCommonFlagsAfterInstruction(ctx, result);
  return kExecuteSuccess;
}

// SAR r/m8, 1
// SAR r/m16, 1
// SAR r/m8, CL
// SAR r/m16, CL
static ExecuteStatus ExecuteGroup2Sar(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kExecuteSuccess;
  }
  int32_t value = FromSignedOperand(op);
  int32_t result = value >> count;
  WriteOperand(ctx, op, result);
  bool last_lsb = ((value >> (count - 1)) & 1) != 0;
  CPUSetFlag(ctx->cpu, kCF, last_lsb);
  if (count == 1) {
    CPUSetFlag(ctx->cpu, kOF, false);
  }
  SetCommonFlagsAfterInstruction(ctx, result);
  return kExecuteSuccess;
}

// ROL r/m8, 1
// ROL r/m16, 1
// ROL r/m8, CL
// ROL r/m16, CL
static ExecuteStatus ExecuteGroup2Rol(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kExecuteSuccess;
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
  if (count == 1) {
    bool current_msb = ((result & kSignBit[ctx->metadata->width]) != 0);
    CPUSetFlag(ctx->cpu, kOF, last_msb != current_msb);
  }
  return kExecuteSuccess;
}

// ROR r/m8, 1
// ROR r/m16, 1
// ROR r/m8, CL
// ROR r/m16, CL
static ExecuteStatus ExecuteGroup2Ror(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kExecuteSuccess;
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
  if (count == 1) {
    bool original_msb = ((value & kSignBit[ctx->metadata->width]) != 0);
    CPUSetFlag(ctx->cpu, kOF, last_lsb != original_msb);
  }
  return kExecuteSuccess;
}

// RCL r/m8, 1
// RCL r/m16, 1
// RCL r/m8, CL
// RCL r/m16, CL
static ExecuteStatus ExecuteGroup2Rcl(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  uint8_t effective_count = count % (kNumBits[ctx->metadata->width] + 1);
  if (effective_count == 0) {
    return kExecuteSuccess;
  }
  uint32_t cf_value = CPUGetFlag(ctx->cpu, kCF) ? (1 << (effective_count - 1)) : 0;
  uint32_t value = FromOperand(op);
  uint32_t result =
      (value << effective_count) | cf_value |
      (value >> (kNumBits[ctx->metadata->width] - (effective_count - 1)));
  WriteOperand(ctx, op, result);
  bool last_msb =
      ((value << (effective_count - 1)) & kSignBit[ctx->metadata->width]) != 0;
  CPUSetFlag(ctx->cpu, kCF, last_msb);
  if (count == 1) {
    bool current_msb = ((result & kSignBit[ctx->metadata->width]) != 0);
    CPUSetFlag(ctx->cpu, kOF, last_msb != current_msb);
  }
  return kExecuteSuccess;
}

// RCR r/m8, 1
// RCR r/m16, 1
// RCR r/m8, CL
// RCR r/m16, CL
static ExecuteStatus ExecuteGroup2Rcr(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  uint8_t effective_count = count % (kNumBits[ctx->metadata->width] + 1);
  if (effective_count == 0) {
    return kExecuteSuccess;
  }
  uint32_t cf_value =
      CPUGetFlag(ctx->cpu, kCF)
          ? (kSignBit[ctx->metadata->width] >> (effective_count - 1))
          : 0;
  uint32_t value = FromOperand(op);
  uint32_t result =
      (value >> effective_count) | cf_value |
      (value << (kNumBits[ctx->metadata->width] - (effective_count - 1)));
  WriteOperand(ctx, op, result);
  bool last_lsb = ((value >> (effective_count - 1)) & 1) != 0;
  CPUSetFlag(ctx->cpu, kCF, last_lsb);
  if (count == 1) {
    bool original_msb = ((value & kSignBit[ctx->metadata->width]) != 0);
    bool current_msb = ((result & kSignBit[ctx->metadata->width]) != 0);
    CPUSetFlag(ctx->cpu, kOF, current_msb != original_msb);
  }
  return kExecuteSuccess;
}

static const Group2ExecuteInstructionFn kGroup2ExecuteInstructionFns[] = {
    ExecuteGroup2Rol,  // 0 - ROL
    ExecuteGroup2Ror,  // 1 - ROR
    ExecuteGroup2Rcl,  // 2 - RCL
    ExecuteGroup2Rcr,  // 3 - RCR
    ExecuteGroup2Shl,  // 4 - SHL
    ExecuteGroup2Shr,  // 5 - SHR
    ExecuteGroup2Shl,  // 6 - SAL (same as SHL)
    ExecuteGroup2Sar,  // 7 - SAR
};

// Group 2 shift / rotate by 1.
YAX86_PRIVATE ExecuteStatus
ExecuteGroup2ShiftOrRotateBy1Instruction(const InstructionContext* ctx) {
  const Group2ExecuteInstructionFn fn =
      kGroup2ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand op = ReadRegisterOrMemoryOperand(ctx);
  return fn(ctx, &op, 1);
}

// Group 2 shift / rotate by CL.
YAX86_PRIVATE ExecuteStatus
ExecuteGroup2ShiftOrRotateByCLInstruction(const InstructionContext* ctx) {
  const Group2ExecuteInstructionFn fn =
      kGroup2ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand op = ReadRegisterOrMemoryOperand(ctx);
  return fn(ctx, &op, ctx->cpu->registers[kCX] & 0xFF);
}
