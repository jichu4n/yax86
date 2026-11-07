#ifndef Y_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // Y_IMPLEMENTATION

// ============================================================================
// MOV instructions
// ============================================================================

// MOV r/m8, r8
// MOV r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperandAddress dest = YCPUGetRegisterOrMemoryOperandAddress(ctx);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  YCPUWriteOperandAddress(ctx, &dest, YCPUFromOperand(&src));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV r8, r/m8
// MOV r16, r/m16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteMoveRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperand(&src));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV r/m16, sreg
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveSegmentRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx) {
  YCPUOperandAddress dest = YCPUGetRegisterOrMemoryOperandAddress(ctx);
  YCPUOperand src;
  YCPUReadSegmentRegisterOperand(ctx, &src);
  YCPUWriteOperandAddress(ctx, &dest, YCPUFromOperand(&src));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV sreg, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveRegisterOrMemoryToSegmentRegister(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadSegmentRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperand(&src));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV AX/CX/DX/BX/SP/BP/SI/DI, imm16
// MOV AH/AL/CH/CL/DH/DL/BH/BL, imm8
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteMoveImmediateToRegister(const YCPUInstructionContext* ctx) {
  static const uint8_t register_index_opcode_base[Y_CPU_NUM_WIDTHS] = {
      0xB0,  // Y_CPU_BYTE
      0xB8,  // Y_CPU_WORD
  };
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode -
                          register_index_opcode_base[ctx->metadata->width]);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperandValue(src_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV AL, moffs16
// MOV AX, moffs16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteMoveMemoryOffsetToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  // Offset is always 16 bits, even though the data width of the operation may
  // be 8 bits.
  YCPUOperandValue src_offset_value =
      YCPUReadImmediateOperandWord(ctx->instruction);
  // The source is DS:offset by default, but can be overridden by a segment
  // override prefix.
  YCPUOperandAddress src_address = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
      .register_index = Y_CPU_DS,
      .offset = (uint16_t)YCPUFromOperandValue(src_offset_value),
  };
  YCPUApplySegmentOverride(ctx->instruction, &src_address.register_index);
  YCPUOperandValue src_value = YCPUReadOperandValue(ctx, &src_address);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperandValue(src_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV moffs16, AL
// MOV moffs16, AX
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteMoveALOrAXToMemoryOffset(const YCPUInstructionContext* ctx) {
  YCPUOperand src;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &src);
  // Offset is always 16 bits, even though the data width of the operation may
  // be 8 bits.
  YCPUOperandValue dest_offset_value =
      YCPUReadImmediateOperandWord(ctx->instruction);
  // The destination is DS:offset by default, but can be overridden by a segment
  // override prefix.
  YCPUOperandAddress dest_address = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
      .register_index = Y_CPU_DS,
      .offset = (uint16_t)YCPUFromOperandValue(dest_offset_value),
  };
  YCPUApplySegmentOverride(ctx->instruction, &dest_address.register_index);
  YCPUWriteOperandAddress(ctx, &dest_address, YCPUFromOperand(&src));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV r/m8, imm8
// MOV r/m16, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveImmediateToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperandAddress dest = YCPUGetRegisterOrMemoryOperandAddress(ctx);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  YCPUWriteOperandAddress(ctx, &dest, YCPUFromOperandValue(src_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ============================================================================
// XCHG instructions
// ============================================================================

// XCHG AX, AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteExchangeRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode - 0x90);
  if (register_index == Y_CPU_AX) {
    // No-op
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  YCPUOperand src;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &src);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  uint32_t temp = YCPUFromOperand(&dest);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperand(&src));
  YCPUWriteOperand(ctx, &src, temp);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// XCHG r/m8, r8
// XCHG r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteExchangeRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  uint32_t temp = YCPUFromOperand(&dest);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperand(&src));
  YCPUWriteOperand(ctx, &src, temp);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ============================================================================
// XLAT
// ============================================================================

// XLAT
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteTranslateByte(const YCPUInstructionContext* ctx) {
  // Read the AL register
  YCPUOperand al;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &al);
  // The translation table is at DS:BX by default, but can be overridden by a
  // segment override prefix.
  YCPUOperandAddress src_address = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
      .register_index = Y_CPU_DS,
      .offset =
          (uint16_t)(ctx->cpu->registers[Y_CPU_BX] + YCPUFromOperand(&al)),
  };
  YCPUApplySegmentOverride(ctx->instruction, &src_address.register_index);
  YCPUOperandValue src_value =
      YCPUReadMemoryOperandByte(ctx->cpu, &src_address);
  YCPUWriteOperandAddress(ctx, &al.address, YCPUFromOperandValue(src_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}
