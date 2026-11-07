#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// IN and OUT instructions
// ============================================================================

// Read a byte from an I/O port.
Y_FILE_PRIVATE Y_HOT YCPUOperandValue
YCPUReadByteFromPort(YCPUState* cpu, uint16_t port) {
  return (YCPUOperandValue)(cpu->config.read_port
                                ? cpu->config.read_port(cpu, port)
                                : 0xFF);
}

// Read a word from an I/O port as a uint16_t. The 8088 has an 8-bit data bus,
// so a word access is two byte accesses to consecutive ports. Peripherals rely
// on this: writing a 6845 register index and its data in one OUT DX, AX is the
// standard idiom, and it is what the BIOS uses.
Y_FILE_PRIVATE YCPUOperandValue
YCPUReadWordFromPort(YCPUState* cpu, uint16_t port) {
  uint8_t low = (uint8_t)YCPUReadByteFromPort(cpu, port);
  uint8_t high = (uint8_t)YCPUReadByteFromPort(cpu, (uint16_t)(port + 1));
  return (YCPUOperandValue)((high << 8) | low);
}

// Table of functions to read from an I/O port, indexed by data width.
Y_FILE_PRIVATE YCPUOperandValue (*const YCPUReadFromPortFns[])(
    YCPUState*, uint16_t) = {
  YCPUReadByteFromPort,  // Y_CPU_BYTE
  YCPUReadWordFromPort,  // Y_CPU_WORD
};

// Common logic for IN instructions.
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteIn(const YCPUInstructionContext* ctx, uint16_t port) {
  YCPUOperandValue value =
      YCPUReadFromPortFns[ctx->metadata->width](ctx->cpu, port);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperandValue(value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// IN AL, imm8
// IN AX, imm8
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteInImmediate(const YCPUInstructionContext* ctx) {
  YCPUOperandValue port = YCPUReadImmediateOperandByte(ctx->instruction);
  return YCPUExecuteIn(ctx, YCPUFromOperandValue(port));
}

// IN AL, DX
// IN AX, DX
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteInDX(const YCPUInstructionContext* ctx) {
  return YCPUExecuteIn(ctx, ctx->cpu->registers[Y_CPU_DX]);
}

// Write a byte to an I/O port.
Y_FILE_PRIVATE void YCPUWriteByteToPort(
    YCPUState* cpu, uint16_t port, YCPUOperandValue value) {
  if (!cpu->config.write_port) {
    return;
  }
  cpu->config.write_port(cpu, port, YCPUFromOperandValue(value));
}

// Write a word to an I/O port. As with reads, this is two byte accesses to
// consecutive ports.
Y_FILE_PRIVATE void YCPUWriteWordToPort(
    YCPUState* cpu, uint16_t port, YCPUOperandValue value) {
  uint32_t raw_value = YCPUFromOperandValue(value);
  YCPUWriteByteToPort(cpu, port, (YCPUOperandValue)(raw_value & 0xFF));
  YCPUWriteByteToPort(
      cpu, (uint16_t)(port + 1), (YCPUOperandValue)((raw_value >> 8) & 0xFF));
}

// Table of functions to write to an I/O port, indexed by data width.
Y_FILE_PRIVATE void (*const YCPUWriteToPortFns[])(
    YCPUState*, uint16_t, YCPUOperandValue) = {
    YCPUWriteByteToPort,  // Y_CPU_BYTE
    YCPUWriteWordToPort,  // Y_CPU_WORD
};

// Common logic for OUT instructions.
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteOut(const YCPUInstructionContext* ctx, uint16_t port) {
  YCPUOperand src;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &src);
  YCPUWriteToPortFns[ctx->metadata->width](ctx->cpu, port, src.value);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// OUT imm8, AL
// OUT imm8, AX
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteOutImmediate(const YCPUInstructionContext* ctx) {
  YCPUOperandValue port = YCPUReadImmediateOperandByte(ctx->instruction);
  return YCPUExecuteOut(ctx, YCPUFromOperandValue(port));
}

// OUT DX, AL
// OUT DX, AX
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteOutDX(const YCPUInstructionContext* ctx) {
  return YCPUExecuteOut(ctx, ctx->cpu->registers[Y_CPU_DX]);
}
