#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// MOV instructions
// ============================================================================

// MOV r/m8, r8
// MOV r/m16, r16
YAX86_PRIVATE ExecuteStatus
ExecuteMoveRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  WriteOperand(ctx, &dest, FromOperand(&src));
  return kExecuteSuccess;
}

// MOV r8, r/m8
// MOV r16, r/m16
YAX86_PRIVATE ExecuteStatus
ExecuteMoveRegisterOrMemoryToRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  WriteOperand(ctx, &dest, FromOperand(&src));
  return kExecuteSuccess;
}

// MOV r/m16, sreg
YAX86_PRIVATE ExecuteStatus
ExecuteMoveSegmentRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadSegmentRegisterOperand(ctx);
  WriteOperand(ctx, &dest, FromOperand(&src));
  return kExecuteSuccess;
}

// MOV sreg, r/m16
YAX86_PRIVATE ExecuteStatus
ExecuteMoveRegisterOrMemoryToSegmentRegister(const InstructionContext* ctx) {
  Operand dest = ReadSegmentRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  WriteOperand(ctx, &dest, FromOperand(&src));
  return kExecuteSuccess;
}

// MOV AX/CX/DX/BX/SP/BP/SI/DI, imm16
// MOV AH/AL/CH/CL/DH/DL/BH/BL, imm8
YAX86_PRIVATE ExecuteStatus
ExecuteMoveImmediateToRegister(const InstructionContext* ctx) {
  static const uint8_t register_index_opcode_base[kNumWidths] = {
      0xB0,  // kByte
      0xB8,  // kWord
  };
  RegisterIndex register_index =
      (RegisterIndex)(ctx->instruction->opcode -
                      register_index_opcode_base[ctx->metadata->width]);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  OperandValue src_value = ReadImmediate(ctx);
  WriteOperand(ctx, &dest, FromOperandValue(&src_value));
  return kExecuteSuccess;
}

// MOV AL, moffs16
// MOV AX, moffs16
YAX86_PRIVATE ExecuteStatus
ExecuteMoveMemoryOffsetToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  // Offset is always 16 bits, even though the data width of the operation may
  // be 8 bits.
  OperandValue src_offset_value =
      kReadImmediateValueFn[kWord](ctx->instruction);
  OperandAddress src_address = {
      .type = kOperandAddressTypeMemory,
      .value = {
          .memory_address = {
              .segment_register_index = kDS,
              .offset = (uint16_t)FromOperandValue(&src_offset_value),
          }}};
  OperandValue src_value = ReadOperandValue(ctx, &src_address);
  WriteOperand(ctx, &dest, FromOperandValue(&src_value));
  return kExecuteSuccess;
}

// MOV moffs16, AL
// MOV moffs16, AX
YAX86_PRIVATE ExecuteStatus
ExecuteMoveALOrAXToMemoryOffset(const InstructionContext* ctx) {
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  // Offset is always 16 bits, even though the data width of the operation may
  // be 8 bits.
  OperandValue dest_offset_value =
      kReadImmediateValueFn[kWord](ctx->instruction);
  OperandAddress dest_address = {
      .type = kOperandAddressTypeMemory,
      .value = {
          .memory_address = {
              .segment_register_index = kDS,
              .offset = (uint16_t)FromOperandValue(&dest_offset_value),
          }}};
  WriteOperandAddress(ctx, &dest_address, FromOperand(&src));
  return kExecuteSuccess;
}

// MOV r/m8, imm8
// MOV r/m16, imm16
YAX86_PRIVATE ExecuteStatus
ExecuteMoveImmediateToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  OperandValue src_value = ReadImmediate(ctx);
  WriteOperand(ctx, &dest, FromOperandValue(&src_value));
  return kExecuteSuccess;
}

// ============================================================================
// XCHG instructions
// ============================================================================

// XCHG AX, AX/CX/DX/BX/SP/BP/SI/DI
YAX86_PRIVATE ExecuteStatus
ExecuteExchangeRegister(const InstructionContext* ctx) {
  RegisterIndex register_index =
      (RegisterIndex)(ctx->instruction->opcode - 0x90);
  if (register_index == kAX) {
    // No-op
    return kExecuteSuccess;
  }
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  uint32_t temp = FromOperand(&dest);
  WriteOperand(ctx, &dest, FromOperand(&src));
  WriteOperand(ctx, &src, temp);
  return kExecuteSuccess;
}

// XCHG r/m8, r8
// XCHG r/m16, r16
YAX86_PRIVATE ExecuteStatus
ExecuteExchangeRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  uint32_t temp = FromOperand(&dest);
  WriteOperand(ctx, &dest, FromOperand(&src));
  WriteOperand(ctx, &src, temp);
  return kExecuteSuccess;
}

// ============================================================================
// XLAT
// ============================================================================

// XLAT
YAX86_PRIVATE ExecuteStatus
ExecuteTranslateByte(const InstructionContext* ctx) {
  // Read the AL register
  Operand al = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandAddress src_address = {
      .type = kOperandAddressTypeMemory,
      .value =
          {.memory_address =
               {
                   .segment_register_index = kDS,
                   .offset =
                       (uint16_t)(ctx->cpu->registers[kBX] + FromOperand(&al)),
               }},
  };
  OperandValue src_value = ReadMemoryByte(ctx->cpu, &src_address);
  WriteOperandAddress(ctx, &al.address, FromOperandValue(&src_value));
  return kExecuteSuccess;
}
