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
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteMoveRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperandAddress dest = GetRegisterOrMemoryOperandAddress(ctx);
  YCPUOperand src;
  ReadRegisterOperand(ctx, &src);
  WriteOperandAddress(ctx, &dest, FromOperand(&src));
  return kInstructionExecuted;
}

// MOV r8, r/m8
// MOV r16, r/m16
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
ExecuteMoveRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  ReadRegisterOrMemoryOperand(ctx, &src);
  WriteOperand(ctx, &dest, FromOperand(&src));
  return kInstructionExecuted;
}

// MOV r/m16, sreg
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteMoveSegmentRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx) {
  YCPUOperandAddress dest = GetRegisterOrMemoryOperandAddress(ctx);
  YCPUOperand src;
  ReadSegmentRegisterOperand(ctx, &src);
  WriteOperandAddress(ctx, &dest, FromOperand(&src));
  return kInstructionExecuted;
}

// MOV sreg, r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteMoveRegisterOrMemoryToSegmentRegister(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadSegmentRegisterOperand(ctx, &dest);
  YCPUOperand src;
  ReadRegisterOrMemoryOperand(ctx, &src);
  WriteOperand(ctx, &dest, FromOperand(&src));
  return kInstructionExecuted;
}

// MOV AX/CX/DX/BX/SP/BP/SI/DI, imm16
// MOV AH/AL/CH/CL/DH/DL/BH/BL, imm8
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
ExecuteMoveImmediateToRegister(const YCPUInstructionContext* ctx) {
  static const uint8_t register_index_opcode_base[kNumWidths] = {
      0xB0,  // kByte
      0xB8,  // kWord
  };
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode -
                          register_index_opcode_base[ctx->metadata->width]);
  YCPUOperand dest;
  ReadRegisterOperandForRegisterIndex(ctx, register_index, &dest);
  YCPUOperandValue src_value = ReadImmediate(ctx);
  WriteOperand(ctx, &dest, FromOperandValue(src_value));
  return kInstructionExecuted;
}

// MOV AL, moffs16
// MOV AX, moffs16
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
ExecuteMoveMemoryOffsetToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);
  // Offset is always 16 bits, even though the data width of the operation may
  // be 8 bits.
  YCPUOperandValue src_offset_value =
      ReadImmediateOperandWord(ctx->instruction);
  // The source is DS:offset by default, but can be overridden by a segment
  // override prefix.
  YCPUOperandAddress src_address = {
      .type = kOperandAddressTypeMemory,
      .register_index = kDS,
      .offset = (uint16_t)FromOperandValue(src_offset_value),
  };
  ApplySegmentOverride(ctx->instruction, &src_address.register_index);
  YCPUOperandValue src_value = ReadOperandValue(ctx, &src_address);
  WriteOperand(ctx, &dest, FromOperandValue(src_value));
  return kInstructionExecuted;
}

// MOV moffs16, AL
// MOV moffs16, AX
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
ExecuteMoveALOrAXToMemoryOffset(const YCPUInstructionContext* ctx) {
  YCPUOperand src;
  ReadRegisterOperandForRegisterIndex(ctx, kAX, &src);
  // Offset is always 16 bits, even though the data width of the operation may
  // be 8 bits.
  YCPUOperandValue dest_offset_value =
      ReadImmediateOperandWord(ctx->instruction);
  // The destination is DS:offset by default, but can be overridden by a segment
  // override prefix.
  YCPUOperandAddress dest_address = {
      .type = kOperandAddressTypeMemory,
      .register_index = kDS,
      .offset = (uint16_t)FromOperandValue(dest_offset_value),
  };
  ApplySegmentOverride(ctx->instruction, &dest_address.register_index);
  WriteOperandAddress(ctx, &dest_address, FromOperand(&src));
  return kInstructionExecuted;
}

// MOV r/m8, imm8
// MOV r/m16, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteMoveImmediateToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperandAddress dest = GetRegisterOrMemoryOperandAddress(ctx);
  YCPUOperandValue src_value = ReadImmediate(ctx);
  WriteOperandAddress(ctx, &dest, FromOperandValue(src_value));
  return kInstructionExecuted;
}

// ============================================================================
// XCHG instructions
// ============================================================================

// XCHG AX, AX/CX/DX/BX/SP/BP/SI/DI
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteExchangeRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode - 0x90);
  if (register_index == kAX) {
    // No-op
    return kInstructionExecuted;
  }
  YCPUOperand src;
  ReadRegisterOperandForRegisterIndex(ctx, register_index, &src);
  YCPUOperand dest;
  ReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);
  uint32_t temp = FromOperand(&dest);
  WriteOperand(ctx, &dest, FromOperand(&src));
  WriteOperand(ctx, &src, temp);
  return kInstructionExecuted;
}

// XCHG r/m8, r8
// XCHG r/m16, r16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteExchangeRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  ReadRegisterOperand(ctx, &src);
  uint32_t temp = FromOperand(&dest);
  WriteOperand(ctx, &dest, FromOperand(&src));
  WriteOperand(ctx, &src, temp);
  return kInstructionExecuted;
}

// ============================================================================
// XLAT
// ============================================================================

// XLAT
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteTranslateByte(const YCPUInstructionContext* ctx) {
  // Read the AL register
  YCPUOperand al;
  ReadRegisterOperandForRegisterIndex(ctx, kAX, &al);
  // The translation table is at DS:BX by default, but can be overridden by a
  // segment override prefix.
  YCPUOperandAddress src_address = {
      .type = kOperandAddressTypeMemory,
      .register_index = kDS,
      .offset = (uint16_t)(ctx->cpu->registers[kBX] + FromOperand(&al)),
  };
  ApplySegmentOverride(ctx->instruction, &src_address.register_index);
  YCPUOperandValue src_value = ReadMemoryOperandByte(ctx->cpu, &src_address);
  WriteOperandAddress(ctx, &al.address, FromOperandValue(src_value));
  return kInstructionExecuted;
}
