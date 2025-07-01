#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// IN and OUT instructions
// ============================================================================

// Read a byte from an I/O port.
static OperandValue ReadByteFromPort(CPUState* cpu, uint16_t port) {
  return ByteValue(
      cpu->config->read_port ? cpu->config->read_port(cpu, port) : 0xFF);
}

// Read a word from an I/O port as a uint16_t.
static OperandValue ReadWordFromPort(CPUState* cpu, uint16_t port) {
  uint8_t low = ReadByteFromPort(cpu, port).value.byte_value;
  uint8_t high = ReadByteFromPort(cpu, port).value.byte_value;
  return WordValue((high << 8) | low);
}

// Table of functions to read from an I/O port, indexed by data width.
static OperandValue (*const kReadFromPortFns[])(CPUState*, uint16_t) = {
    ReadByteFromPort,  // kByte
    ReadWordFromPort,  // kWord
};

// Common logic for IN instructions.
static ExecuteStatus ExecuteIn(const InstructionContext* ctx, uint16_t port) {
  OperandValue value = kReadFromPortFns[ctx->metadata->width](ctx->cpu, port);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  WriteOperand(ctx, &dest, FromOperandValue(&value));
  return kExecuteSuccess;
}

// IN AL, imm8
// IN AX, imm8
YAX86_PRIVATE ExecuteStatus ExecuteInImmediate(const InstructionContext* ctx) {
  OperandValue port = ReadImmediateByte(ctx->instruction);
  return ExecuteIn(ctx, FromOperandValue(&port));
}

// IN AL, DX
// IN AX, DX
YAX86_PRIVATE ExecuteStatus ExecuteInDX(const InstructionContext* ctx) {
  return ExecuteIn(ctx, ctx->cpu->registers[kDX]);
}

// Write a byte to an I/O port.
static void WriteByteToPort(CPUState* cpu, uint16_t port, OperandValue value) {
  if (!cpu->config->write_port) {
    return;
  }
  cpu->config->write_port(cpu, port, FromOperandValue(&value));
}

// Write a word to an I/O port.
static void WriteWordToPort(CPUState* cpu, uint16_t port, OperandValue value) {
  uint32_t raw_value = FromOperandValue(&value);
  WriteByteToPort(cpu, port, ByteValue(raw_value & 0xFF));
  WriteByteToPort(cpu, port, ByteValue((raw_value >> 8) & 0xFF));
}

// Table of functions to write to an I/O port, indexed by data width.
static void (*const kWriteToPortFns[])(CPUState*, uint16_t, OperandValue) = {
    WriteByteToPort,  // kByte
    WriteWordToPort,  // kWord
};

// Common logic for OUT instructions.
static ExecuteStatus ExecuteOut(const InstructionContext* ctx, uint16_t port) {
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  kWriteToPortFns[ctx->metadata->width](ctx->cpu, port, src.value);
  return kExecuteSuccess;
}

// OUT imm8, AL
// OUT imm8, AX
YAX86_PRIVATE ExecuteStatus ExecuteOutImmediate(const InstructionContext* ctx) {
  OperandValue port = ReadImmediateByte(ctx->instruction);
  return ExecuteOut(ctx, FromOperandValue(&port));
}

// OUT DX, AL
// OUT DX, AX
YAX86_PRIVATE ExecuteStatus ExecuteOutDX(const InstructionContext* ctx) {
  return ExecuteOut(ctx, ctx->cpu->registers[kDX]);
}
