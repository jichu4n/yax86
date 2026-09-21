#ifndef YAX86_CPU_OPERANDS_H
#define YAX86_CPU_OPERANDS_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// Narrow a computed result to what an operand of the given width holds.
YAX86_MODULE_PRIVATE OperandValue
ToOperandValue(Width width, uint32_t raw_value);

// Helper function to zero-extend OperandValue to a 32-bit value. This makes it
// simpler to do direct arithmetic without worrying about overflow.
YAX86_MODULE_PRIVATE uint32_t FromOperandValue(OperandValue value);

// Helper function to sign-extend OperandValue to a 32-bit value. This makes it
// simpler to do direct arithmetic without worrying about overflow.
YAX86_MODULE_PRIVATE int32_t
FromSignedOperandValue(Width width, OperandValue value);

// Helper function to extract a zero-extended value from an operand.
YAX86_MODULE_PRIVATE uint32_t FromOperand(const Operand* operand);

// Helper function to extract a sign-extended value from an operand.
YAX86_MODULE_PRIVATE int32_t
FromSignedOperand(Width width, const Operand* operand);

// Computes the raw address a segment register and an offset address.
YAX86_MODULE_PRIVATE uint32_t ToRawAddress(
    const CPUState* cpu, uint8_t segment_register_index, uint16_t offset);

// Read a byte from memory as a uint8_t.
YAX86_MODULE_PRIVATE uint8_t
ReadRawMemoryByte(CPUState* cpu, uint32_t raw_address);

// Read a word from memory as a uint16_t.
YAX86_MODULE_PRIVATE uint16_t
ReadRawMemoryWord(CPUState* cpu, uint32_t raw_address);

// Read a byte from memory as an OperandValue.
YAX86_MODULE_PRIVATE OperandValue
ReadMemoryOperandByte(CPUState* cpu, const OperandAddress* address);

// Read a word from memory as an OperandValue.
YAX86_MODULE_PRIVATE OperandValue
ReadMemoryOperandWord(CPUState* cpu, const OperandAddress* address);

// Read a byte from a register as an OperandValue.
YAX86_MODULE_PRIVATE OperandValue
ReadRegisterOperandByte(CPUState* cpu, const OperandAddress* address);

// Read a word from a register as an OperandValue.
YAX86_MODULE_PRIVATE OperandValue
ReadRegisterOperandWord(CPUState* cpu, const OperandAddress* address);

// Write a byte as uint8_t to memory.
YAX86_MODULE_PRIVATE void WriteRawMemoryByte(
    CPUState* cpu, uint32_t address, uint8_t value);

// Write a byte to memory.
YAX86_MODULE_PRIVATE void WriteMemoryOperandByte(
    CPUState* cpu, const OperandAddress* address, OperandValue value);

// Write a word to memory.
YAX86_MODULE_PRIVATE void WriteMemoryOperandWord(
    CPUState* cpu, const OperandAddress* address, OperandValue value);

// Write a byte to a register.
YAX86_MODULE_PRIVATE void WriteRegisterOperandByte(
    CPUState* cpu, const OperandAddress* address, OperandValue value);

// Write a word to a register.
YAX86_MODULE_PRIVATE void WriteRegisterOperandWord(
    CPUState* cpu, const OperandAddress* address, OperandValue value);

// Add an 8-bit signed relative offset to a 16-bit unsigned base address.
YAX86_MODULE_PRIVATE uint16_t
AddSignedOffsetByte(uint16_t base, uint8_t raw_offset);

// Add a 16-bit signed relative offset to a 16-bit unsigned base address.
YAX86_MODULE_PRIVATE uint16_t
AddSignedOffsetWord(uint16_t base, uint16_t raw_offset);

// Get the register operand for a byte instruction based on the ModR/M byte's
// reg or R/M field.
YAX86_MODULE_PRIVATE RegisterAddress
GetRegisterAddressByte(CPUState* cpu, uint8_t reg_or_rm);

// Get the register operand for a word instruction based on the ModR/M byte's
// reg or R/M field.
YAX86_MODULE_PRIVATE RegisterAddress
GetRegisterAddressWord(CPUState* cpu, uint8_t reg_or_rm);

// Replace a segment register index with whatever the instruction's segment
// override prefix names, if it carries one.
YAX86_MODULE_PRIVATE void ApplySegmentOverride(
    const Instruction* instruction, uint8_t* segment_register_index);

// Compute the memory address for an instruction.
YAX86_MODULE_PRIVATE MemoryAddress
GetMemoryOperandAddress(CPUState* cpu, const Instruction* instruction);

// Get a register or memory operand address based on the ModR/M byte and
// displacement, without reading the value currently there.
YAX86_MODULE_PRIVATE OperandAddress
GetRegisterOrMemoryOperandAddress(const InstructionContext* ctx);

// Read an 8-bit immediate value.
YAX86_MODULE_PRIVATE OperandValue
ReadImmediateOperandByte(const Instruction* instruction);

// Read a 16-bit immediate value.
YAX86_MODULE_PRIVATE OperandValue
ReadImmediateOperandWord(const Instruction* instruction);

// Read a value from an operand address.
YAX86_MODULE_PRIVATE OperandValue
ReadOperandValue(const InstructionContext* ctx, const OperandAddress* address);

// Get a register or memory operand for an instruction based on the ModR/M
// byte and displacement.
YAX86_MODULE_PRIVATE void ReadRegisterOrMemoryOperand(
    const InstructionContext* ctx, Operand* operand);

// Get a register operand for an instruction.
YAX86_MODULE_PRIVATE void ReadRegisterOperandForRegisterIndex(
    const InstructionContext* ctx, RegisterIndex register_index,
    Operand* operand);

// Get a register operand for an instruction from the REG field of the Mod/RM
// byte.
YAX86_MODULE_PRIVATE void ReadRegisterOperand(
    const InstructionContext* ctx, Operand* operand);

// Get a segment register operand for an instruction from the REG field of the
// Mod/RM byte.
YAX86_MODULE_PRIVATE void ReadSegmentRegisterOperand(
    const InstructionContext* ctx, Operand* operand);

// Write a value to a register or memory operand address.
YAX86_MODULE_PRIVATE void WriteOperandAddress(
    const InstructionContext* ctx, const OperandAddress* address,
    uint32_t raw_value);

// Write a value to a register or memory operand.
YAX86_MODULE_PRIVATE void WriteOperand(
    const InstructionContext* ctx, const Operand* operand, uint32_t raw_value);

// Read an immediate value from the instruction.
YAX86_MODULE_PRIVATE OperandValue ReadImmediate(const InstructionContext* ctx);

#endif  // YAX86_CPU_OPERANDS_H
