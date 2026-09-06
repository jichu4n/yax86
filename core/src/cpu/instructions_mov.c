#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// MOV instructions
// ============================================================================

// MOV r/m8, r8
// MOV r/m16, r16
YAX86_PRIVATE InstructionResult
ExecuteMoveRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  OperandAddress dest = GetRegisterOrMemoryOperandAddress(ctx);
  Operand src = ReadRegisterOperand(ctx);
  WriteOperandAddress(ctx, &dest, FromOperand(&src));
  return kInstructionExecuted;
}

// MOV r8, r/m8
// MOV r16, r/m16
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteMoveRegisterOrMemoryToRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  WriteOperand(ctx, &dest, FromOperand(&src));
  return kInstructionExecuted;
}

// MOV r/m16, sreg
YAX86_PRIVATE InstructionResult
ExecuteMoveSegmentRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  OperandAddress dest = GetRegisterOrMemoryOperandAddress(ctx);
  Operand src = ReadSegmentRegisterOperand(ctx);
  WriteOperandAddress(ctx, &dest, FromOperand(&src));
  return kInstructionExecuted;
}

// MOV sreg, r/m16
YAX86_PRIVATE InstructionResult
ExecuteMoveRegisterOrMemoryToSegmentRegister(const InstructionContext* ctx) {
  Operand dest = ReadSegmentRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  WriteOperand(ctx, &dest, FromOperand(&src));
  return kInstructionExecuted;
}

// MOV AX/CX/DX/BX/SP/BP/SI/DI, imm16
// MOV AH/AL/CH/CL/DH/DL/BH/BL, imm8
YAX86_HOT YAX86_PRIVATE InstructionResult
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
  return kInstructionExecuted;
}

// MOV AL, moffs16
// MOV AX, moffs16
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteMoveMemoryOffsetToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  // Offset is always 16 bits, even though the data width of the operation may
  // be 8 bits.
  OperandValue src_offset_value = ReadImmediateOperandWord(ctx->instruction);
  // The source is DS:offset by default, but can be overridden by a segment
  // override prefix.
  OperandAddress src_address = {
      .type = kOperandAddressTypeMemory,
      .value = {
          .memory_address = {
              .segment_register_index = kDS,
              .offset = (uint16_t)FromOperandValue(&src_offset_value),
          }}};
  ApplySegmentOverride(ctx->instruction, &src_address.value.memory_address);
  OperandValue src_value = ReadOperandValue(ctx, &src_address);
  WriteOperand(ctx, &dest, FromOperandValue(&src_value));
  return kInstructionExecuted;
}

// MOV moffs16, AL
// MOV moffs16, AX
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteMoveALOrAXToMemoryOffset(const InstructionContext* ctx) {
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  // Offset is always 16 bits, even though the data width of the operation may
  // be 8 bits.
  OperandValue dest_offset_value = ReadImmediateOperandWord(ctx->instruction);
  // The destination is DS:offset by default, but can be overridden by a segment
  // override prefix.
  OperandAddress dest_address = {
      .type = kOperandAddressTypeMemory,
      .value = {
          .memory_address = {
              .segment_register_index = kDS,
              .offset = (uint16_t)FromOperandValue(&dest_offset_value),
          }}};
  ApplySegmentOverride(ctx->instruction, &dest_address.value.memory_address);
  WriteOperandAddress(ctx, &dest_address, FromOperand(&src));
  return kInstructionExecuted;
}

// MOV r/m8, imm8
// MOV r/m16, imm16
YAX86_PRIVATE InstructionResult
ExecuteMoveImmediateToRegisterOrMemory(const InstructionContext* ctx) {
  OperandAddress dest = GetRegisterOrMemoryOperandAddress(ctx);
  OperandValue src_value = ReadImmediate(ctx);
  WriteOperandAddress(ctx, &dest, FromOperandValue(&src_value));
  return kInstructionExecuted;
}

// ============================================================================
// XCHG instructions
// ============================================================================

// XCHG AX, AX/CX/DX/BX/SP/BP/SI/DI
YAX86_PRIVATE InstructionResult
ExecuteExchangeRegister(const InstructionContext* ctx) {
  RegisterIndex register_index =
      (RegisterIndex)(ctx->instruction->opcode - 0x90);
  if (register_index == kAX) {
    // No-op
    return kInstructionExecuted;
  }
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  uint32_t temp = FromOperand(&dest);
  WriteOperand(ctx, &dest, FromOperand(&src));
  WriteOperand(ctx, &src, temp);
  return kInstructionExecuted;
}

// XCHG r/m8, r8
// XCHG r/m16, r16
YAX86_PRIVATE InstructionResult
ExecuteExchangeRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  uint32_t temp = FromOperand(&dest);
  WriteOperand(ctx, &dest, FromOperand(&src));
  WriteOperand(ctx, &src, temp);
  return kInstructionExecuted;
}

// ============================================================================
// XLAT
// ============================================================================

// XLAT
YAX86_PRIVATE InstructionResult
ExecuteTranslateByte(const InstructionContext* ctx) {
  // Read the AL register
  Operand al = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  // The translation table is at DS:BX by default, but can be overridden by a
  // segment override prefix.
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
  ApplySegmentOverride(ctx->instruction, &src_address.value.memory_address);
  OperandValue src_value = ReadMemoryOperandByte(ctx->cpu, &src_address);
  WriteOperandAddress(ctx, &al.address, FromOperandValue(&src_value));
  return kInstructionExecuted;
}
