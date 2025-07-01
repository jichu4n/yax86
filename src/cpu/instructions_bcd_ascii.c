#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// BCD and ASCII arithmetic instructions
// ============================================================================

// AAA
YAX86_PRIVATE ExecuteStatus ExecuteAaa(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  uint8_t al_low = al & 0x0F;
  if (al_low > 9 || GetFlag(ctx->cpu, kAF)) {
    al += 6;
    ++ah;
    SetFlag(ctx->cpu, kAF, true);
    SetFlag(ctx->cpu, kCF, true);
  } else {
    SetFlag(ctx->cpu, kAF, false);
    SetFlag(ctx->cpu, kCF, false);
  }
  al &= 0x0F;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  return kExecuteSuccess;
}

// AAS
YAX86_PRIVATE ExecuteStatus ExecuteAas(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  uint8_t al_low = al & 0x0F;
  if (al_low > 9 || GetFlag(ctx->cpu, kAF)) {
    al -= 6;
    --ah;
    SetFlag(ctx->cpu, kAF, true);
    SetFlag(ctx->cpu, kCF, true);
  } else {
    SetFlag(ctx->cpu, kAF, false);
    SetFlag(ctx->cpu, kCF, false);
  }
  al &= 0x0F;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  return kExecuteSuccess;
}

// AAM
YAX86_PRIVATE ExecuteStatus ExecuteAam(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  OperandValue base = ReadImmediate(ctx);
  uint16_t base_value = FromOperandValue(&base);
  if (base_value == 0) {
    return kExecuteInvalidInstruction;
  }
  uint8_t ah = al / base_value;
  al %= base_value;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  SetCommonFlagsAfterInstruction(ctx, al);
  return kExecuteSuccess;
}

// AAD
YAX86_PRIVATE ExecuteStatus ExecuteAad(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  OperandValue base = ReadImmediate(ctx);
  uint8_t base_value = FromOperandValue(&base);
  al += ah * base_value;
  ah = 0;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  SetCommonFlagsAfterInstruction(ctx, al);
  return kExecuteSuccess;
}

// DAA
YAX86_PRIVATE ExecuteStatus ExecuteDaa(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  uint8_t al_low = al & 0x0F;
  if (al_low > 9 || GetFlag(ctx->cpu, kAF)) {
    al += 6;
    SetFlag(ctx->cpu, kAF, true);
  } else {
    SetFlag(ctx->cpu, kAF, false);
  }
  uint8_t al_high = (al >> 4) & 0x0F;
  if (al_high > 9 || GetFlag(ctx->cpu, kCF)) {
    al += 0x60;
    SetFlag(ctx->cpu, kCF, true);
  } else {
    SetFlag(ctx->cpu, kCF, false);
  }
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  SetCommonFlagsAfterInstruction(ctx, al);
  return kExecuteSuccess;
}

// DAS
YAX86_PRIVATE ExecuteStatus ExecuteDas(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  uint8_t al_low = al & 0x0F;
  if (al_low > 9 || GetFlag(ctx->cpu, kAF)) {
    al -= 6;
    SetFlag(ctx->cpu, kAF, true);
  } else {
    SetFlag(ctx->cpu, kAF, false);
  }
  uint8_t al_high = (al >> 4) & 0x0F;
  if (al_high > 9 || GetFlag(ctx->cpu, kCF)) {
    al -= 0x60;
    SetFlag(ctx->cpu, kCF, true);
  } else {
    SetFlag(ctx->cpu, kCF, false);
  }
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  SetCommonFlagsAfterInstruction(ctx, al);
  return kExecuteSuccess;
}
