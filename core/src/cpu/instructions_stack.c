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
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecutePushRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode - 0x50);
  YCPUOperand src;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &src);
  YCPUPushSourceOperand(ctx->cpu, &src);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// POP AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecutePopRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode - 0x58);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &dest);
  YCPUOperandValue value = YCPUPop(ctx->cpu);
  YCPUWriteOperandAddress(ctx, &dest.address, YCPUFromOperandValue(value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// PUSH ES/CS/SS/DS
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecutePushSegmentRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(((ctx->instruction->opcode >> 3) & 0x03) + 8);
  YCPUOperand src;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &src);
  YCPUPushValue(ctx->cpu, src.value);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// POP ES/CS/SS/DS
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecutePopSegmentRegister(const YCPUInstructionContext* ctx) {
  // The segment register field is only two bits wide, which is what makes
  // 0x0F decode as POP CS on the 8086/8088. Popping into CS is legal there -
  // it just makes the next instruction fetch come from the new segment.
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(((ctx->instruction->opcode >> 3) & 0x03) + 8);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &dest);
  YCPUOperandValue value = YCPUPop(ctx->cpu);
  YCPUWriteOperandAddress(ctx, &dest.address, YCPUFromOperandValue(value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// PUSHF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePushFlags(const YCPUInstructionContext* ctx) {
  YCPUPushValue(ctx->cpu, ctx->cpu->flags);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// POPF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePopFlags(const YCPUInstructionContext* ctx) {
  YCPUOperandValue value = YCPUPop(ctx->cpu);
  ctx->cpu->flags = YCPUToFlagsRegisterValue(YCPUFromOperandValue(value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// POP r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePopRegisterOrMemory(const YCPUInstructionContext* ctx) {
  // The 8086/8088 does not decode the REG field of 0x8F at all, so every value
  // pops. Only REG 0 is documented.
  //
  // The destination address is resolved before the pop, because SP and BP can
  // both address it and the 8086 computes the effective address first.
  YCPUOperandAddress dest = YCPUGetRegisterOrMemoryOperandAddress(ctx);
  YCPUOperandValue value = YCPUPop(ctx->cpu);
  YCPUWriteOperandAddress(ctx, &dest, YCPUFromOperandValue(value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ============================================================================
// LAHF and SAHF
// ============================================================================

// Returns the AH register address.
Y_FILE_PRIVATE const YCPUOperandAddress* YCPUGetAHRegisterAddress(void) {
  static YCPUOperandAddress ah = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER,
      .register_index = Y_CPU_AX,
      .offset = 8,
  };
  return &ah;
}

// LAHF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoadAHFromFlags(const YCPUInstructionContext* ctx) {
  YCPUWriteRegisterOperandByte(
      ctx->cpu, YCPUGetAHRegisterAddress(),
      (YCPUOperandValue)(ctx->cpu->flags & 0x00FF));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// SAHF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteStoreAHToFlags(const YCPUInstructionContext* ctx) {
  YCPUOperandValue value =
      YCPUReadRegisterOperandByte(ctx->cpu, YCPUGetAHRegisterAddress());
  // Clear the lower byte of flags and set it to the value in AH
  ctx->cpu->flags =
      YCPUToFlagsRegisterValue((ctx->cpu->flags & 0xFF00) | value);
  return Y_CPU_INSTRUCTION_EXECUTED;
}
