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
YAX86_FILE_PRIVATE YAX86_HOT YCPUOperandValue
ReadByteFromPort(YCPUState* cpu, uint16_t port) {
  return (YCPUOperandValue)(cpu->config.read_port
                                ? cpu->config.read_port(cpu, port)
                                : 0xFF);
}

// Read a word from an I/O port as a uint16_t. The 8088 has an 8-bit data bus,
// so a word access is two byte accesses to consecutive ports. Peripherals rely
// on this: writing a 6845 register index and its data in one OUT DX, AX is the
// standard idiom, and it is what the BIOS uses.
YAX86_FILE_PRIVATE YCPUOperandValue
ReadWordFromPort(YCPUState* cpu, uint16_t port) {
  uint8_t low = (uint8_t)ReadByteFromPort(cpu, port);
  uint8_t high = (uint8_t)ReadByteFromPort(cpu, (uint16_t)(port + 1));
  return (YCPUOperandValue)((high << 8) | low);
}

// Table of functions to read from an I/O port, indexed by data width.
YAX86_FILE_PRIVATE YCPUOperandValue (*const kReadFromPortFns[])(
    YCPUState*, uint16_t) = {
  ReadByteFromPort,  // kByte
  ReadWordFromPort,  // kWord
};

// Common logic for IN instructions.
YAX86_FILE_PRIVATE YCPUInstructionResult
ExecuteIn(const YCPUInstructionContext* ctx, uint16_t port) {
  YCPUOperandValue value =
      kReadFromPortFns[ctx->metadata->width](ctx->cpu, port);
  YCPUOperand dest;
  ReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);
  WriteOperand(ctx, &dest, FromOperandValue(value));
  return kInstructionExecuted;
}

// IN AL, imm8
// IN AX, imm8
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
ExecuteInImmediate(const YCPUInstructionContext* ctx) {
  YCPUOperandValue port = ReadImmediateOperandByte(ctx->instruction);
  return ExecuteIn(ctx, FromOperandValue(port));
}

// IN AL, DX
// IN AX, DX
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
ExecuteInDX(const YCPUInstructionContext* ctx) {
  return ExecuteIn(ctx, ctx->cpu->registers[kDX]);
}

// Write a byte to an I/O port.
YAX86_FILE_PRIVATE void WriteByteToPort(
    YCPUState* cpu, uint16_t port, YCPUOperandValue value) {
  if (!cpu->config.write_port) {
    return;
  }
  cpu->config.write_port(cpu, port, FromOperandValue(value));
}

// Write a word to an I/O port. As with reads, this is two byte accesses to
// consecutive ports.
YAX86_FILE_PRIVATE void WriteWordToPort(
    YCPUState* cpu, uint16_t port, YCPUOperandValue value) {
  uint32_t raw_value = FromOperandValue(value);
  WriteByteToPort(cpu, port, (YCPUOperandValue)(raw_value & 0xFF));
  WriteByteToPort(
      cpu, (uint16_t)(port + 1), (YCPUOperandValue)((raw_value >> 8) & 0xFF));
}

// Table of functions to write to an I/O port, indexed by data width.
YAX86_FILE_PRIVATE void (*const kWriteToPortFns[])(
    YCPUState*, uint16_t, YCPUOperandValue) = {
    WriteByteToPort,  // kByte
    WriteWordToPort,  // kWord
};

// Common logic for OUT instructions.
YAX86_FILE_PRIVATE YCPUInstructionResult
ExecuteOut(const YCPUInstructionContext* ctx, uint16_t port) {
  YCPUOperand src;
  ReadRegisterOperandForRegisterIndex(ctx, kAX, &src);
  kWriteToPortFns[ctx->metadata->width](ctx->cpu, port, src.value);
  return kInstructionExecuted;
}

// OUT imm8, AL
// OUT imm8, AX
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteOutImmediate(const YCPUInstructionContext* ctx) {
  YCPUOperandValue port = ReadImmediateOperandByte(ctx->instruction);
  return ExecuteOut(ctx, FromOperandValue(port));
}

// OUT DX, AL
// OUT DX, AX
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
ExecuteOutDX(const YCPUInstructionContext* ctx) {
  return ExecuteOut(ctx, ctx->cpu->registers[kDX]);
}
