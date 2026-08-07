#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// BCD and ASCII arithmetic instructions
// ============================================================================

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
  uint8_t al_low = al & 0x0F;
  if (al_low > 9 || CPUGetFlag(ctx->cpu, kAF)) {
    al += 6;
    CPUSetFlag(ctx->cpu, kAF, true);
  } else {
    CPUSetFlag(ctx->cpu, kAF, false);
  }
  uint8_t al_high = (al >> 4) & 0x0F;
  if (al_high > 9 || CPUGetFlag(ctx->cpu, kCF)) {
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
  uint8_t al_low = al & 0x0F;
  if (al_low > 9 || CPUGetFlag(ctx->cpu, kAF)) {
    al -= 6;
    CPUSetFlag(ctx->cpu, kAF, true);
  } else {
    CPUSetFlag(ctx->cpu, kAF, false);
  }
  uint8_t al_high = (al >> 4) & 0x0F;
  if (al_high > 9 || CPUGetFlag(ctx->cpu, kCF)) {
    al -= 0x60;
    CPUSetFlag(ctx->cpu, kCF, true);
  } else {
    CPUSetFlag(ctx->cpu, kCF, false);
  }
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  SetCommonFlagsAfterInstruction(ctx, al);
  return kInstructionExecuted;
}
