#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// PUSH and POP instructions
// ============================================================================

// PUSH AX/CX/DX/BX/SP/BP/SI/DI
YAX86_PRIVATE ExecuteStatus ExecutePushRegister(const InstructionContext* ctx) {
  RegisterIndex register_index = ctx->instruction->opcode - 0x50;
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  Push(ctx->cpu, src.value);
  return kExecuteSuccess;
}

// POP AX/CX/DX/BX/SP/BP/SI/DI
YAX86_PRIVATE ExecuteStatus ExecutePopRegister(const InstructionContext* ctx) {
  RegisterIndex register_index = ctx->instruction->opcode - 0x58;
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  OperandValue value = Pop(ctx->cpu);
  WriteOperandAddress(ctx, &dest.address, FromOperandValue(&value));
  return kExecuteSuccess;
}

// PUSH ES/CS/SS/DS
YAX86_PRIVATE ExecuteStatus ExecutePushSegmentRegister(const InstructionContext* ctx) {
  RegisterIndex register_index = ((ctx->instruction->opcode >> 3) & 0x03) + 8;
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  Push(ctx->cpu, src.value);
  return kExecuteSuccess;
}

// POP ES/CS/SS/DS
YAX86_PRIVATE ExecuteStatus ExecutePopSegmentRegister(const InstructionContext* ctx) {
  RegisterIndex register_index = ((ctx->instruction->opcode >> 3) & 0x03) + 8;
  // Special case - disallow POP CS
  if (register_index == kCS) {
    return kExecuteInvalidInstruction;
  }
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  OperandValue value = Pop(ctx->cpu);
  WriteOperandAddress(ctx, &dest.address, FromOperandValue(&value));
  return kExecuteSuccess;
}

// PUSHF
YAX86_PRIVATE ExecuteStatus ExecutePushFlags(const InstructionContext* ctx) {
  Push(ctx->cpu, WordValue(ctx->cpu->flags));
  return kExecuteSuccess;
}

// POPF
YAX86_PRIVATE ExecuteStatus ExecutePopFlags(const InstructionContext* ctx) {
  OperandValue value = Pop(ctx->cpu);
  ctx->cpu->flags = FromOperandValue(&value);
  return kExecuteSuccess;
}

// POP r/m16
YAX86_PRIVATE ExecuteStatus ExecutePopRegisterOrMemory(const InstructionContext* ctx) {
  if (ctx->instruction->mod_rm.reg != 0) {
    return kExecuteInvalidInstruction;
  }
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  OperandValue value = Pop(ctx->cpu);
  WriteOperandAddress(ctx, &dest.address, FromOperandValue(&value));
  return kExecuteSuccess;
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
YAX86_PRIVATE ExecuteStatus ExecuteLoadAHFromFlags(const InstructionContext* ctx) {
  WriteRegisterByte(
      ctx->cpu, GetAHRegisterAddress(), ByteValue(ctx->cpu->flags & 0x00FF));
  return kExecuteSuccess;
}

// SAHF
YAX86_PRIVATE ExecuteStatus ExecuteStoreAHToFlags(const InstructionContext* ctx) {
  OperandValue value = ReadRegisterByte(ctx->cpu, GetAHRegisterAddress());
  // Clear the lower byte of flags and set it to the value in AH
  ctx->cpu->flags = (ctx->cpu->flags & 0xFF00) | value.value.byte_value;
  return kExecuteSuccess;
}
