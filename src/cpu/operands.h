#ifndef YAX86_CPU_OPERANDS_H
#define YAX86_CPU_OPERANDS_H

#ifndef YAX86_IMPLEMENTATION
#include "cpu_public.h"
#include "widths.h"
#endif  // YAX86_IMPLEMENTATION

// The address of a register operand.
typedef struct RegisterAddress {
  // Register index.
  RegisterIndex register_index;
  // Byte offset within the register; only relevant for byte-sized operands.
  // 0 for low byte (AL, CL, DL, BL), 8 for high byte (AH, CH, DH, BH).
  uint8_t byte_offset;
} RegisterAddress;

// The address of a memory operand.
typedef struct MemoryAddress {
  // Segment register.
  RegisterIndex segment_register_index;
  // Effective address offset.
  uint16_t offset;
} MemoryAddress;

// Whether the operand is a register or memory operand.
typedef enum OperandAddressType {
  kOperandAddressTypeRegister = 0,
  kOperandAddressTypeMemory,
} OperandAddressType;

enum {
  // Number of operand address types.
  kNumOperandAddressTypes = kOperandAddressTypeMemory + 1,
};

// Operand address.
typedef struct OperandAddress {
  // Type of operand (register or memory).
  OperandAddressType type;
  // Address of the operand.
  union {
    RegisterAddress register_address;  // For register operands
    MemoryAddress memory_address;      // For memory operands
  } value;
} OperandAddress;

// Operand value.
typedef struct OperandValue {
  // Data width.
  Width width;
  // The value of the operand.
  union {
    uint8_t byte_value;   // For byte operands
    uint16_t word_value;  // For word operands
  } value;
} OperandValue;

// An operand.
typedef struct Operand {
  // Address of the operand.
  OperandAddress address;
  // Value of the operand.
  OperandValue value;
} Operand;

#ifndef YAX86_IMPLEMENTATION

// Helper functions to construct OperandValue.
extern OperandValue ByteValue(uint8_t byte_value);

// Helper function to construct OperandValue for a word.
extern OperandValue WordValue(uint16_t word_value);

// Helper function to construct OperandValue given a Width and a value.
extern OperandValue ToOperandValue(Width width, uint32_t raw_value);

// Helper function to zero-extend OperandValue to a 32-bit value. This makes it
// simpler to do direct arithmetic without worrying about overflow.
extern uint32_t FromOperandValue(const OperandValue* value);

// Helper function to sign-extend OperandValue to a 32-bit value. This makes it
// simpler to do direct arithmetic without worrying about overflow.
extern int32_t FromSignedOperandValue(const OperandValue* value);

// Helper function to extract a zero-extended value from an operand.
extern uint32_t FromOperand(const Operand* operand);

// Helper function to extract a sign-extended value from an operand.
extern int32_t FromSignedOperand(const Operand* operand);

// Computes the raw effective address corresponding to a MemoryAddress.
extern uint16_t ToPhysicalAddress(
    const CPUState* cpu, const MemoryAddress* address);

// Read a byte from memory as a uint8_t.
extern uint8_t ReadRawMemoryByte(CPUState* cpu, uint16_t physical_address);

// Read a word from memory as a uint16_t.
extern uint16_t ReadRawMemoryWord(CPUState* cpu, uint16_t physical_address);

// Read a byte from memory to an OperandValue.
extern OperandValue ReadMemoryByte(
    CPUState* cpu, const OperandAddress* address);

// Read a word from memory to an OperandValue.
extern OperandValue ReadMemoryWord(
    CPUState* cpu, const OperandAddress* address);

// Read a byte from a register to an OperandValue.
extern OperandValue ReadRegisterByte(
    CPUState* cpu, const OperandAddress* address);

// Read a word from a register to an OperandValue.
extern OperandValue ReadRegisterWord(
    CPUState* cpu, const OperandAddress* address);

// Write a byte as uint8_t to memory.
extern void WriteRawMemoryByte(CPUState* cpu, uint16_t address, uint8_t value);

// Write a word as uint16_t to memory.
extern void WriteRawMemoryWord(CPUState* cpu, uint16_t address, uint16_t value);

// Write a byte to memory.
extern void WriteMemoryByte(
    CPUState* cpu, const OperandAddress* address, OperandValue value);

// Write a word to memory.
extern void WriteMemoryWord(
    CPUState* cpu, const OperandAddress* address, OperandValue value);

// Write a byte to a register.
extern void WriteRegisterByte(
    CPUState* cpu, const OperandAddress* address, OperandValue value);

// Write a word to a register.
extern void WriteRegisterWord(
    CPUState* cpu, const OperandAddress* address, OperandValue value);

// Add an 8-bit signed relative offset to a 16-bit unsigned base address.
extern uint16_t AddSignedOffsetByte(uint16_t base, uint8_t raw_offset);

// Add a 16-bit signed relative offset to a 16-bit unsigned base address.
extern uint16_t AddSignedOffsetWord(uint16_t base, uint16_t raw_offset);

// Get the register operand for a byte instruction based on the ModR/M byte's
// reg or R/M field.
extern RegisterAddress GetRegisterAddressByte(CPUState* cpu, uint8_t reg_or_rm);

// Get the register operand for a word instruction based on the ModR/M byte's
// reg or R/M field.
extern RegisterAddress GetRegisterAddressWord(CPUState* cpu, uint8_t reg_or_rm);

// Apply segment override prefixes to a MemoryAddress.
extern void ApplySegmentOverride(
    const Instruction* instruction, MemoryAddress* address);

// Compute the memory address for an instruction.
extern MemoryAddress GetMemoryOperandAddress(
    CPUState* cpu, const Instruction* instruction);

// Get a register or memory operand address based on the ModR/M byte and
// displacement.
extern OperandAddress GetRegisterOrMemoryOperandAddress(
    CPUState* cpu, const Instruction* instruction, Width width);

// Read an 8-bit immediate value.
extern OperandValue ReadImmediateByte(const Instruction* instruction);

// Read a 16-bit immediate value.
extern OperandValue ReadImmediateWord(const Instruction* instruction);

// Table of GetRegisterAddress functions, indexed by Width.
extern RegisterAddress (*const kGetRegisterAddressFn[kNumWidths])(
    CPUState* cpu, uint8_t reg_or_rm);

// Table of Read* functions, indexed by OperandAddressType and Width.
extern OperandValue (*const kReadOperandValueFn[kNumOperandAddressTypes][kNumWidths])(
    CPUState* cpu, const OperandAddress* address);

// Table of Write* functions, indexed by OperandAddressType and Width.
extern void (*const kWriteOperandFn[kNumOperandAddressTypes][kNumWidths])(
    CPUState* cpu, const OperandAddress* address, OperandValue value);

// Table of ReadImmediate* functions, indexed by Width.
extern OperandValue (*const kReadImmediateValueFn[kNumWidths])(
    const Instruction* instruction);

#endif  // YAX86_IMPLEMENTATION

#endif  // YAX86_CPU_OPERANDS_H
