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
static OperandValue ReadByteFromPort(CPUState* cpu, uint16_t port) {
  return ByteValue(
      cpu->config->read_port ? cpu->config->read_port(cpu, port) : 0xFF);
}

// Read a word from an I/O port as a uint16_t. The 8088 has an 8-bit data bus,
// so a word access is two byte accesses to consecutive ports. Peripherals rely
// on this: writing a 6845 register index and its data in one OUT DX, AX is the
// standard idiom, and it is what the BIOS uses.
static OperandValue ReadWordFromPort(CPUState* cpu, uint16_t port) {
  uint8_t low = ReadByteFromPort(cpu, port).value.byte_value;
  uint8_t high = ReadByteFromPort(cpu, (uint16_t)(port + 1)).value.byte_value;
  return WordValue((high << 8) | low);
}

// Table of functions to read from an I/O port, indexed by data width.
static OperandValue (*const kReadFromPortFns[])(CPUState*, uint16_t) = {
    ReadByteFromPort,  // kByte
    ReadWordFromPort,  // kWord
};

// Common logic for IN instructions.
static InstructionResult ExecuteIn(
    const InstructionContext* ctx, uint16_t port) {
  OperandValue value = kReadFromPortFns[ctx->metadata->width](ctx->cpu, port);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  WriteOperand(ctx, &dest, FromOperandValue(&value));
  return kInstructionExecuted;
}

// IN AL, imm8
// IN AX, imm8
YAX86_PRIVATE InstructionResult
ExecuteInImmediate(const InstructionContext* ctx) {
  OperandValue port = ReadImmediateOperandByte(ctx->instruction);
  return ExecuteIn(ctx, FromOperandValue(&port));
}

// IN AL, DX
// IN AX, DX
YAX86_PRIVATE InstructionResult ExecuteInDX(const InstructionContext* ctx) {
  return ExecuteIn(ctx, ctx->cpu->registers[kDX]);
}

// Write a byte to an I/O port.
static void WriteByteToPort(CPUState* cpu, uint16_t port, OperandValue value) {
  if (!cpu->config->write_port) {
    return;
  }
  cpu->config->write_port(cpu, port, FromOperandValue(&value));
}

// Write a word to an I/O port. As with reads, this is two byte accesses to
// consecutive ports.
static void WriteWordToPort(CPUState* cpu, uint16_t port, OperandValue value) {
  uint32_t raw_value = FromOperandValue(&value);
  WriteByteToPort(cpu, port, ByteValue(raw_value & 0xFF));
  WriteByteToPort(
      cpu, (uint16_t)(port + 1), ByteValue((raw_value >> 8) & 0xFF));
}

// Table of functions to write to an I/O port, indexed by data width.
static void (*const kWriteToPortFns[])(CPUState*, uint16_t, OperandValue) = {
    WriteByteToPort,  // kByte
    WriteWordToPort,  // kWord
};

// Common logic for OUT instructions.
static InstructionResult ExecuteOut(
    const InstructionContext* ctx, uint16_t port) {
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  kWriteToPortFns[ctx->metadata->width](ctx->cpu, port, src.value);
  return kInstructionExecuted;
}

// OUT imm8, AL
// OUT imm8, AX
YAX86_PRIVATE InstructionResult
ExecuteOutImmediate(const InstructionContext* ctx) {
  OperandValue port = ReadImmediateOperandByte(ctx->instruction);
  return ExecuteOut(ctx, FromOperandValue(&port));
}

// OUT DX, AL
// OUT DX, AX
YAX86_PRIVATE InstructionResult ExecuteOutDX(const InstructionContext* ctx) {
  return ExecuteOut(ctx, ctx->cpu->registers[kDX]);
}
