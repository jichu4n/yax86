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
YAX86_FILE_PRIVATE uint8_t YCPUGetBCDHighDigitLimit(bool auxiliary_carry) {
  return auxiliary_carry ? 0x9F : 0x99;
}

// AAA
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAaa(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  uint8_t al_low = al & 0x0F;
  if (al_low > 9 || YCPUGetFlag(ctx->cpu, kAF)) {
    al += 6;
    ++ah;
    YCPUSetFlag(ctx->cpu, kAF, true);
    YCPUSetFlag(ctx->cpu, kCF, true);
  } else {
    YCPUSetFlag(ctx->cpu, kAF, false);
    YCPUSetFlag(ctx->cpu, kCF, false);
  }
  al &= 0x0F;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  return kInstructionExecuted;
}

// AAS
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAas(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  uint8_t al_low = al & 0x0F;
  if (al_low > 9 || YCPUGetFlag(ctx->cpu, kAF)) {
    al -= 6;
    --ah;
    YCPUSetFlag(ctx->cpu, kAF, true);
    YCPUSetFlag(ctx->cpu, kCF, true);
  } else {
    YCPUSetFlag(ctx->cpu, kAF, false);
    YCPUSetFlag(ctx->cpu, kCF, false);
  }
  al &= 0x0F;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  return kInstructionExecuted;
}

// AAM
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAam(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  YCPUOperandValue base = YCPUReadImmediate(ctx);
  uint16_t base_value = YCPUFromOperandValue(base);
  if (base_value == 0) {
    // AAM divides by its immediate operand, so a base of 0 raises a divide
    // error just like DIV by zero does, rather than being an invalid encoding.
    // The division never produces a result, and the flags are left set as
    // though it had produced zero.
    YCPUSetCommonFlagsAfterInstruction(ctx, 0);
    YCPUSetFlag(ctx->cpu, kCF, false);
    YCPUSetFlag(ctx->cpu, kAF, false);
    YCPUSetFlag(ctx->cpu, kOF, false);
    YCPURaiseInternalInterrupt(ctx->cpu, kInterruptDivideError);
    return kInstructionExecuted;
  }
  uint8_t ah = al / base_value;
  al %= base_value;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  YCPUSetCommonFlagsAfterInstruction(ctx, al);
  return kInstructionExecuted;
}

// AAD
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAad(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  YCPUOperandValue base = YCPUReadImmediate(ctx);
  uint8_t base_value = YCPUFromOperandValue(base);
  al += ah * base_value;
  ah = 0;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  YCPUSetCommonFlagsAfterInstruction(ctx, al);
  return kInstructionExecuted;
}

// DAA
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDaa(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  const uint8_t original_al = al;
  const bool original_carry = YCPUGetFlag(ctx->cpu, kCF);
  const uint8_t high_digit_limit =
      YCPUGetBCDHighDigitLimit(YCPUGetFlag(ctx->cpu, kAF));

  if ((al & 0x0F) > 9 || YCPUGetFlag(ctx->cpu, kAF)) {
    al += 6;
    YCPUSetFlag(ctx->cpu, kAF, true);
  } else {
    YCPUSetFlag(ctx->cpu, kAF, false);
  }
  if (original_al > high_digit_limit || original_carry) {
    al += 0x60;
    YCPUSetFlag(ctx->cpu, kCF, true);
  } else {
    YCPUSetFlag(ctx->cpu, kCF, false);
  }
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  YCPUSetCommonFlagsAfterInstruction(ctx, al);
  return kInstructionExecuted;
}

// DAS
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDas(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  const uint8_t original_al = al;
  const bool original_carry = YCPUGetFlag(ctx->cpu, kCF);
  const uint8_t high_digit_limit =
      YCPUGetBCDHighDigitLimit(YCPUGetFlag(ctx->cpu, kAF));

  if ((al & 0x0F) > 9 || YCPUGetFlag(ctx->cpu, kAF)) {
    al -= 6;
    YCPUSetFlag(ctx->cpu, kAF, true);
  } else {
    YCPUSetFlag(ctx->cpu, kAF, false);
  }
  if (original_al > high_digit_limit || original_carry) {
    al -= 0x60;
    YCPUSetFlag(ctx->cpu, kCF, true);
  } else {
    YCPUSetFlag(ctx->cpu, kCF, false);
  }
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  YCPUSetCommonFlagsAfterInstruction(ctx, al);
  return kInstructionExecuted;
}
