#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// PUSH and POP instructions
// ============================================================================

// PUSH AX/CX/DX/BX/SP/BP/SI/DI
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
ExecutePushRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode - 0x50);
  YCPUOperand src;
  ReadRegisterOperandForRegisterIndex(ctx, register_index, &src);
  PushSourceOperand(ctx->cpu, &src);
  return kInstructionExecuted;
}

// POP AX/CX/DX/BX/SP/BP/SI/DI
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
ExecutePopRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode - 0x58);
  YCPUOperand dest;
  ReadRegisterOperandForRegisterIndex(ctx, register_index, &dest);
  YCPUOperandValue value = Pop(ctx->cpu);
  WriteOperandAddress(ctx, &dest.address, FromOperandValue(value));
  return kInstructionExecuted;
}

// PUSH ES/CS/SS/DS
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
ExecutePushSegmentRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(((ctx->instruction->opcode >> 3) & 0x03) + 8);
  YCPUOperand src;
  ReadRegisterOperandForRegisterIndex(ctx, register_index, &src);
  PushValue(ctx->cpu, src.value);
  return kInstructionExecuted;
}

// POP ES/CS/SS/DS
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
ExecutePopSegmentRegister(const YCPUInstructionContext* ctx) {
  // The segment register field is only two bits wide, which is what makes
  // 0x0F decode as POP CS on the 8086/8088. Popping into CS is legal there -
  // it just makes the next instruction fetch come from the new segment.
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(((ctx->instruction->opcode >> 3) & 0x03) + 8);
  YCPUOperand dest;
  ReadRegisterOperandForRegisterIndex(ctx, register_index, &dest);
  YCPUOperandValue value = Pop(ctx->cpu);
  WriteOperandAddress(ctx, &dest.address, FromOperandValue(value));
  return kInstructionExecuted;
}

// PUSHF
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecutePushFlags(const YCPUInstructionContext* ctx) {
  PushValue(ctx->cpu, ctx->cpu->flags);
  return kInstructionExecuted;
}

// POPF
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecutePopFlags(const YCPUInstructionContext* ctx) {
  YCPUOperandValue value = Pop(ctx->cpu);
  ctx->cpu->flags = ToFlagsRegisterValue(FromOperandValue(value));
  return kInstructionExecuted;
}

// POP r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecutePopRegisterOrMemory(const YCPUInstructionContext* ctx) {
  // The 8086/8088 does not decode the REG field of 0x8F at all, so every value
  // pops. Only REG 0 is documented.
  //
  // The destination address is resolved before the pop, because SP and BP can
  // both address it and the 8086 computes the effective address first.
  YCPUOperandAddress dest = GetRegisterOrMemoryOperandAddress(ctx);
  YCPUOperandValue value = Pop(ctx->cpu);
  WriteOperandAddress(ctx, &dest, FromOperandValue(value));
  return kInstructionExecuted;
}

// ============================================================================
// LAHF and SAHF
// ============================================================================

// Returns the AH register address.
YAX86_FILE_PRIVATE const YCPUOperandAddress* GetAHRegisterAddress(void) {
  static YCPUOperandAddress ah = {
      .type = kOperandAddressTypeRegister,
      .register_index = kAX,
      .offset = 8,
  };
  return &ah;
}

// LAHF
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteLoadAHFromFlags(const YCPUInstructionContext* ctx) {
  WriteRegisterOperandByte(
      ctx->cpu, GetAHRegisterAddress(),
      (YCPUOperandValue)(ctx->cpu->flags & 0x00FF));
  return kInstructionExecuted;
}

// SAHF
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteStoreAHToFlags(const YCPUInstructionContext* ctx) {
  YCPUOperandValue value =
      ReadRegisterOperandByte(ctx->cpu, GetAHRegisterAddress());
  // Clear the lower byte of flags and set it to the value in AH
  ctx->cpu->flags = ToFlagsRegisterValue((ctx->cpu->flags & 0xFF00) | value);
  return kInstructionExecuted;
}
