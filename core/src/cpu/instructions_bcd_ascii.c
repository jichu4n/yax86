#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// BCD and ASCII arithmetic instructions
// ============================================================================

// The value DAA and DAS compare AL against to decide whether the high digit
// needs adjusting. Both test AL as it was on entry, before the low digit was
// adjusted, so a carry out of the low digit does not drag the high one along
// with it.
//
// The limit is one higher when the auxiliary carry flag was already set on
// entry. That is not what Intel's published pseudocode says, which uses 0x99
// throughout, but it is what the 8086/8088 does - and with AL between 0x9A and
// 0x9F it is the difference between adjusting and not.
static uint8_t GetBCDHighDigitLimit(bool auxiliary_carry) {
  return auxiliary_carry ? 0x9F : 0x99;
}

// AAA
YAX86_PRIVATE InstructionResult ExecuteAaa(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  uint8_t al_low = al & 0x0F;
  if (al_low > 9 || CPUGetFlag(ctx->cpu, kAF)) {
    al += 6;
    ++ah;
    CPUSetFlag(ctx->cpu, kAF, true);
    CPUSetFlag(ctx->cpu, kCF, true);
  } else {
    CPUSetFlag(ctx->cpu, kAF, false);
    CPUSetFlag(ctx->cpu, kCF, false);
  }
  al &= 0x0F;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  return kInstructionExecuted;
}

// AAS
YAX86_PRIVATE InstructionResult ExecuteAas(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  uint8_t al_low = al & 0x0F;
  if (al_low > 9 || CPUGetFlag(ctx->cpu, kAF)) {
    al -= 6;
    --ah;
    CPUSetFlag(ctx->cpu, kAF, true);
    CPUSetFlag(ctx->cpu, kCF, true);
  } else {
    CPUSetFlag(ctx->cpu, kAF, false);
    CPUSetFlag(ctx->cpu, kCF, false);
  }
  al &= 0x0F;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  return kInstructionExecuted;
}

// AAM
YAX86_PRIVATE InstructionResult ExecuteAam(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  OperandValue base = ReadImmediate(ctx);
  uint16_t base_value = FromOperandValue(&base);
  if (base_value == 0) {
    // AAM divides by its immediate operand, so a base of 0 raises a divide
    // error just like DIV by zero does, rather than being an invalid encoding.
    // The division never produces a result, and the flags are left set as
    // though it had produced zero.
    SetCommonFlagsAfterInstruction(ctx, 0);
    CPUSetFlag(ctx->cpu, kCF, false);
    CPUSetFlag(ctx->cpu, kAF, false);
    CPUSetFlag(ctx->cpu, kOF, false);
    CPURaiseInternalInterrupt(ctx->cpu, kInterruptDivideError);
    return kInstructionExecuted;
  }
  uint8_t ah = al / base_value;
  al %= base_value;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  SetCommonFlagsAfterInstruction(ctx, al);
  return kInstructionExecuted;
}

// AAD
YAX86_PRIVATE InstructionResult ExecuteAad(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  OperandValue base = ReadImmediate(ctx);
  uint8_t base_value = FromOperandValue(&base);
  al += ah * base_value;
  ah = 0;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  SetCommonFlagsAfterInstruction(ctx, al);
  return kInstructionExecuted;
}

// DAA
YAX86_PRIVATE InstructionResult ExecuteDaa(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  const uint8_t original_al = al;
  const bool original_carry = CPUGetFlag(ctx->cpu, kCF);
  const uint8_t high_digit_limit =
      GetBCDHighDigitLimit(CPUGetFlag(ctx->cpu, kAF));

  if ((al & 0x0F) > 9 || CPUGetFlag(ctx->cpu, kAF)) {
    al += 6;
    CPUSetFlag(ctx->cpu, kAF, true);
  } else {
    CPUSetFlag(ctx->cpu, kAF, false);
  }
  if (original_al > high_digit_limit || original_carry) {
    al += 0x60;
    CPUSetFlag(ctx->cpu, kCF, true);
  } else {
    CPUSetFlag(ctx->cpu, kCF, false);
  }
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  SetCommonFlagsAfterInstruction(ctx, al);
  return kInstructionExecuted;
}

// DAS
YAX86_PRIVATE InstructionResult ExecuteDas(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  const uint8_t original_al = al;
  const bool original_carry = CPUGetFlag(ctx->cpu, kCF);
  const uint8_t high_digit_limit =
      GetBCDHighDigitLimit(CPUGetFlag(ctx->cpu, kAF));

  if ((al & 0x0F) > 9 || CPUGetFlag(ctx->cpu, kAF)) {
    al -= 6;
    CPUSetFlag(ctx->cpu, kAF, true);
  } else {
    CPUSetFlag(ctx->cpu, kAF, false);
  }
  if (original_al > high_digit_limit || original_carry) {
    al -= 0x60;
    CPUSetFlag(ctx->cpu, kCF, true);
  } else {
    CPUSetFlag(ctx->cpu, kCF, false);
  }
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  SetCommonFlagsAfterInstruction(ctx, al);
  return kInstructionExecuted;
}
