#ifndef YAX86_CPU_OPERANDS_H
#define YAX86_CPU_OPERANDS_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// Narrow a computed result to what an operand of the given width holds.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUToOperandValue(YCPUWidth width, uint32_t raw_value);

// Helper function to zero-extend YCPUOperandValue to a 32-bit value. This makes
// it simpler to do direct arithmetic without worrying about overflow.
Y_MODULE_PRIVATE uint32_t YCPUFromOperandValue(YCPUOperandValue value);

// Helper function to sign-extend YCPUOperandValue to a 32-bit value. This makes
// it simpler to do direct arithmetic without worrying about overflow.
Y_MODULE_PRIVATE int32_t
YCPUFromSignedOperandValue(YCPUWidth width, YCPUOperandValue value);

// Helper function to extract a zero-extended value from an operand.
Y_MODULE_PRIVATE uint32_t YCPUFromOperand(const YCPUOperand* operand);

// Helper function to extract a sign-extended value from an operand.
Y_MODULE_PRIVATE int32_t
YCPUFromSignedOperand(YCPUWidth width, const YCPUOperand* operand);

// Computes the raw address a segment register and an offset address.
Y_MODULE_PRIVATE uint32_t YCPUToRawAddress(
    const YCPUState* cpu, uint8_t segment_register_index, uint16_t offset);

// Read a byte from memory as a uint8_t.
Y_MODULE_PRIVATE uint8_t
YCPUReadRawMemoryByte(YCPUState* cpu, uint32_t raw_address);

// Read a word from memory as a uint16_t.
Y_MODULE_PRIVATE uint16_t
YCPUReadRawMemoryWord(YCPUState* cpu, uint32_t raw_address);

// Read a byte from memory as a YCPUOperandValue.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadMemoryOperandByte(YCPUState* cpu, const YCPUOperandAddress* address);

// Read a word from memory as a YCPUOperandValue.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadMemoryOperandWord(YCPUState* cpu, const YCPUOperandAddress* address);

// Read a byte from a register as a YCPUOperandValue.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadRegisterOperandByte(YCPUState* cpu, const YCPUOperandAddress* address);

// Read a word from a register as a YCPUOperandValue.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadRegisterOperandWord(YCPUState* cpu, const YCPUOperandAddress* address);

// Write a byte as uint8_t to memory.
Y_MODULE_PRIVATE void YCPUWriteRawMemoryByte(
    YCPUState* cpu, uint32_t address, uint8_t value);

// Write a byte to memory.
Y_MODULE_PRIVATE void YCPUWriteMemoryOperandByte(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value);

// Write a word to memory.
Y_MODULE_PRIVATE void YCPUWriteMemoryOperandWord(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value);

// Write a byte to a register.
Y_MODULE_PRIVATE void YCPUWriteRegisterOperandByte(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value);

// Write a word to a register.
Y_MODULE_PRIVATE void YCPUWriteRegisterOperandWord(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value);

// Add an 8-bit signed relative offset to a 16-bit unsigned base address.
Y_MODULE_PRIVATE uint16_t
YCPUAddSignedOffsetByte(uint16_t base, uint8_t raw_offset);

// Add a 16-bit signed relative offset to a 16-bit unsigned base address.
Y_MODULE_PRIVATE uint16_t
YCPUAddSignedOffsetWord(uint16_t base, uint16_t raw_offset);

// Get the register operand for a byte instruction based on the ModR/M byte's
// reg or R/M field.
Y_MODULE_PRIVATE YCPURegisterAddress
YCPUGetRegisterAddressByte(YCPUState* cpu, uint8_t reg_or_rm);

// Get the register operand for a word instruction based on the ModR/M byte's
// reg or R/M field.
Y_MODULE_PRIVATE YCPURegisterAddress
YCPUGetRegisterAddressWord(YCPUState* cpu, uint8_t reg_or_rm);

// Replace a segment register index with whatever the instruction's segment
// override prefix names, if it carries one.
Y_MODULE_PRIVATE void YCPUApplySegmentOverride(
    const YCPUInstruction* instruction, uint8_t* segment_register_index);

// Compute the memory address for an instruction.
Y_MODULE_PRIVATE YCPUMemoryAddress
YCPUGetMemoryOperandAddress(YCPUState* cpu, const YCPUInstruction* instruction);

// Get a register or memory operand address based on the ModR/M byte and
// displacement, without reading the value currently there.
Y_MODULE_PRIVATE YCPUOperandAddress
YCPUGetRegisterOrMemoryOperandAddress(const YCPUInstructionContext* ctx);

// Read an 8-bit immediate value.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadImmediateOperandByte(const YCPUInstruction* instruction);

// Read a 16-bit immediate value.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadImmediateOperandWord(const YCPUInstruction* instruction);

// Read a value from an operand address.
Y_MODULE_PRIVATE YCPUOperandValue YCPUReadOperandValue(
    const YCPUInstructionContext* ctx, const YCPUOperandAddress* address);

// Get a register or memory operand for an instruction based on the ModR/M
// byte and displacement.
Y_MODULE_PRIVATE void YCPUReadRegisterOrMemoryOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand);

// Get a register operand for an instruction.
Y_MODULE_PRIVATE void YCPUReadRegisterOperandForRegisterIndex(
    const YCPUInstructionContext* ctx, YCPURegisterIndex register_index,
    YCPUOperand* operand);

// Get a register operand for an instruction from the REG field of the Mod/RM
// byte.
Y_MODULE_PRIVATE void YCPUReadRegisterOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand);

// Get a segment register operand for an instruction from the REG field of the
// Mod/RM byte.
Y_MODULE_PRIVATE void YCPUReadSegmentRegisterOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand);

// Write a value to a register or memory operand address.
Y_MODULE_PRIVATE void YCPUWriteOperandAddress(
    const YCPUInstructionContext* ctx, const YCPUOperandAddress* address,
    uint32_t raw_value);

// Write a value to a register or memory operand.
Y_MODULE_PRIVATE void YCPUWriteOperand(
    const YCPUInstructionContext* ctx, const YCPUOperand* operand,
    uint32_t raw_value);

// Read an immediate value from the instruction.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadImmediate(const YCPUInstructionContext* ctx);

#endif  // YAX86_CPU_OPERANDS_H
