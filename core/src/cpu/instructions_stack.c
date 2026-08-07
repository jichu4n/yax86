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
YAX86_PRIVATE InstructionResult
ExecutePushRegister(const InstructionContext* ctx) {
  RegisterIndex register_index =
      (RegisterIndex)(ctx->instruction->opcode - 0x50);
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  Push(ctx->cpu, src.value);
  return kInstructionExecuted;
}

// POP AX/CX/DX/BX/SP/BP/SI/DI
YAX86_PRIVATE InstructionResult
ExecutePopRegister(const InstructionContext* ctx) {
  RegisterIndex register_index =
      (RegisterIndex)(ctx->instruction->opcode - 0x58);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  OperandValue value = Pop(ctx->cpu);
  WriteOperandAddress(ctx, &dest.address, FromOperandValue(&value));
  return kInstructionExecuted;
}

// PUSH ES/CS/SS/DS
YAX86_PRIVATE InstructionResult
ExecutePushSegmentRegister(const InstructionContext* ctx) {
  RegisterIndex register_index =
      (RegisterIndex)(((ctx->instruction->opcode >> 3) & 0x03) + 8);
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  Push(ctx->cpu, src.value);
  return kInstructionExecuted;
}

// POP ES/CS/SS/DS
YAX86_PRIVATE InstructionResult
ExecutePopSegmentRegister(const InstructionContext* ctx) {
  // The segment register field is only two bits wide, which is what makes
  // 0x0F decode as POP CS on the 8086/8088. Popping into CS is legal there -
  // it just makes the next instruction fetch come from the new segment.
  RegisterIndex register_index =
      (RegisterIndex)(((ctx->instruction->opcode >> 3) & 0x03) + 8);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  OperandValue value = Pop(ctx->cpu);
  WriteOperandAddress(ctx, &dest.address, FromOperandValue(&value));
  return kInstructionExecuted;
}

// PUSHF
YAX86_PRIVATE InstructionResult
ExecutePushFlags(const InstructionContext* ctx) {
  Push(ctx->cpu, WordValue(ctx->cpu->flags));
  return kInstructionExecuted;
}

// POPF
YAX86_PRIVATE InstructionResult ExecutePopFlags(const InstructionContext* ctx) {
  OperandValue value = Pop(ctx->cpu);
  ctx->cpu->flags = ToFlagsRegisterValue(FromOperandValue(&value));
  return kInstructionExecuted;
}

// POP r/m16
YAX86_PRIVATE InstructionResult
ExecutePopRegisterOrMemory(const InstructionContext* ctx) {
  // The 8086/8088 does not decode the REG field of 0x8F at all, so every value
  // pops. Only REG 0 is documented.
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  OperandValue value = Pop(ctx->cpu);
  WriteOperandAddress(ctx, &dest.address, FromOperandValue(&value));
  return kInstructionExecuted;
}

// ============================================================================
// LAHF and SAHF
// ============================================================================

// Returns the AH register address.
static const OperandAddress* GetAHRegisterAddress(void) {
  static OperandAddress ah = {
      .type = kOperandAddressTypeRegister,
      .value = {
          .register_address = {
              .register_index = kAX,
              .byte_offset = 8,
          }}};
  return &ah;
}

// LAHF
YAX86_PRIVATE InstructionResult
ExecuteLoadAHFromFlags(const InstructionContext* ctx) {
  WriteRegisterOperandByte(
      ctx->cpu, GetAHRegisterAddress(), ByteValue(ctx->cpu->flags & 0x00FF));
  return kInstructionExecuted;
}

// SAHF
YAX86_PRIVATE InstructionResult
ExecuteStoreAHToFlags(const InstructionContext* ctx) {
  OperandValue value =
      ReadRegisterOperandByte(ctx->cpu, GetAHRegisterAddress());
  // Clear the lower byte of flags and set it to the value in AH
  ctx->cpu->flags =
      ToFlagsRegisterValue((ctx->cpu->flags & 0xFF00) | value.value.byte_value);
  return kInstructionExecuted;
}
