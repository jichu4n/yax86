#ifndef YAX86_CPU_OPERANDS_H
#define YAX86_CPU_OPERANDS_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// Narrow a computed result to what an operand of the given width holds.
YAX86_MODULE_PRIVATE YCPUOperandValue
ToOperandValue(YCPUWidth width, uint32_t raw_value);

// Helper function to zero-extend YCPUOperandValue to a 32-bit value. This makes
// it simpler to do direct arithmetic without worrying about overflow.
YAX86_MODULE_PRIVATE uint32_t FromOperandValue(YCPUOperandValue value);

// Helper function to sign-extend YCPUOperandValue to a 32-bit value. This makes
// it simpler to do direct arithmetic without worrying about overflow.
YAX86_MODULE_PRIVATE int32_t
FromSignedOperandValue(YCPUWidth width, YCPUOperandValue value);

// Helper function to extract a zero-extended value from an operand.
YAX86_MODULE_PRIVATE uint32_t FromOperand(const YCPUOperand* operand);

// Helper function to extract a sign-extended value from an operand.
YAX86_MODULE_PRIVATE int32_t
FromSignedOperand(YCPUWidth width, const YCPUOperand* operand);

// Computes the raw address a segment register and an offset address.
YAX86_MODULE_PRIVATE uint32_t ToRawAddress(
    const YCPUState* cpu, uint8_t segment_register_index, uint16_t offset);

// Read a byte from memory as a uint8_t.
YAX86_MODULE_PRIVATE uint8_t
ReadRawMemoryByte(YCPUState* cpu, uint32_t raw_address);

// Read a word from memory as a uint16_t.
YAX86_MODULE_PRIVATE uint16_t
ReadRawMemoryWord(YCPUState* cpu, uint32_t raw_address);

// Read a byte from memory as a YCPUOperandValue.
YAX86_MODULE_PRIVATE YCPUOperandValue
ReadMemoryOperandByte(YCPUState* cpu, const YCPUOperandAddress* address);

// Read a word from memory as a YCPUOperandValue.
YAX86_MODULE_PRIVATE YCPUOperandValue
ReadMemoryOperandWord(YCPUState* cpu, const YCPUOperandAddress* address);

// Read a byte from a register as a YCPUOperandValue.
YAX86_MODULE_PRIVATE YCPUOperandValue
ReadRegisterOperandByte(YCPUState* cpu, const YCPUOperandAddress* address);

// Read a word from a register as a YCPUOperandValue.
YAX86_MODULE_PRIVATE YCPUOperandValue
ReadRegisterOperandWord(YCPUState* cpu, const YCPUOperandAddress* address);

// Write a byte as uint8_t to memory.
YAX86_MODULE_PRIVATE void WriteRawMemoryByte(
    YCPUState* cpu, uint32_t address, uint8_t value);

// Write a byte to memory.
YAX86_MODULE_PRIVATE void WriteMemoryOperandByte(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value);

// Write a word to memory.
YAX86_MODULE_PRIVATE void WriteMemoryOperandWord(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value);

// Write a byte to a register.
YAX86_MODULE_PRIVATE void WriteRegisterOperandByte(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value);

// Write a word to a register.
YAX86_MODULE_PRIVATE void WriteRegisterOperandWord(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value);

// Add an 8-bit signed relative offset to a 16-bit unsigned base address.
YAX86_MODULE_PRIVATE uint16_t
AddSignedOffsetByte(uint16_t base, uint8_t raw_offset);

// Add a 16-bit signed relative offset to a 16-bit unsigned base address.
YAX86_MODULE_PRIVATE uint16_t
AddSignedOffsetWord(uint16_t base, uint16_t raw_offset);

// Get the register operand for a byte instruction based on the ModR/M byte's
// reg or R/M field.
YAX86_MODULE_PRIVATE YCPURegisterAddress
GetRegisterAddressByte(YCPUState* cpu, uint8_t reg_or_rm);

// Get the register operand for a word instruction based on the ModR/M byte's
// reg or R/M field.
YAX86_MODULE_PRIVATE YCPURegisterAddress
GetRegisterAddressWord(YCPUState* cpu, uint8_t reg_or_rm);

// Replace a segment register index with whatever the instruction's segment
// override prefix names, if it carries one.
YAX86_MODULE_PRIVATE void ApplySegmentOverride(
    const YCPUInstruction* instruction, uint8_t* segment_register_index);

// Compute the memory address for an instruction.
YAX86_MODULE_PRIVATE YCPUMemoryAddress
GetMemoryOperandAddress(YCPUState* cpu, const YCPUInstruction* instruction);

// Get a register or memory operand address based on the ModR/M byte and
// displacement, without reading the value currently there.
YAX86_MODULE_PRIVATE YCPUOperandAddress
GetRegisterOrMemoryOperandAddress(const YCPUInstructionContext* ctx);

// Read an 8-bit immediate value.
YAX86_MODULE_PRIVATE YCPUOperandValue
ReadImmediateOperandByte(const YCPUInstruction* instruction);

// Read a 16-bit immediate value.
YAX86_MODULE_PRIVATE YCPUOperandValue
ReadImmediateOperandWord(const YCPUInstruction* instruction);

// Read a value from an operand address.
YAX86_MODULE_PRIVATE YCPUOperandValue ReadOperandValue(
    const YCPUInstructionContext* ctx, const YCPUOperandAddress* address);

// Get a register or memory operand for an instruction based on the ModR/M
// byte and displacement.
YAX86_MODULE_PRIVATE void ReadRegisterOrMemoryOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand);

// Get a register operand for an instruction.
YAX86_MODULE_PRIVATE void ReadRegisterOperandForRegisterIndex(
    const YCPUInstructionContext* ctx, YCPURegisterIndex register_index,
    YCPUOperand* operand);

// Get a register operand for an instruction from the REG field of the Mod/RM
// byte.
YAX86_MODULE_PRIVATE void ReadRegisterOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand);

// Get a segment register operand for an instruction from the REG field of the
// Mod/RM byte.
YAX86_MODULE_PRIVATE void ReadSegmentRegisterOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand);

// Write a value to a register or memory operand address.
YAX86_MODULE_PRIVATE void WriteOperandAddress(
    const YCPUInstructionContext* ctx, const YCPUOperandAddress* address,
    uint32_t raw_value);

// Write a value to a register or memory operand.
YAX86_MODULE_PRIVATE void WriteOperand(
    const YCPUInstructionContext* ctx, const YCPUOperand* operand,
    uint32_t raw_value);

// Read an immediate value from the instruction.
YAX86_MODULE_PRIVATE YCPUOperandValue
ReadImmediate(const YCPUInstructionContext* ctx);

#endif  // YAX86_CPU_OPERANDS_H
