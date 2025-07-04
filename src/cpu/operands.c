#ifndef YAX86_IMPLEMENTATION
#include "operands.h"

#include "../util/common.h"
#endif  // YAX86_IMPLEMENTATION

// Helper functions to construct OperandValue.
YAX86_PRIVATE OperandValue ByteValue(uint8_t byte_value) {
  OperandValue value = {
      .width = kByte,
      .value = {.byte_value = byte_value},
  };
  return value;
}

// Helper function to construct OperandValue for a word.
YAX86_PRIVATE OperandValue WordValue(uint16_t word_value) {
  OperandValue value = {
      .width = kWord,
      .value = {.word_value = word_value},
  };
  return value;
}

// Helper function to construct OperandValue given a Width and a value.
YAX86_PRIVATE OperandValue ToOperandValue(Width width, uint32_t raw_value) {
  switch (width) {
    case kByte:
      return ByteValue(raw_value & kMaxValue[width]);
    case kWord:
      return WordValue(raw_value & kMaxValue[width]);
  }
  // Should never reach here, but return a default value to avoid warnings.
  return ByteValue(0xFF);
}

// Helper function to zero-extend OperandValue to a 32-bit value. This makes it
// simpler to do direct arithmetic without worrying about overflow.
YAX86_PRIVATE uint32_t FromOperandValue(const OperandValue* value) {
  switch (value->width) {
    case kByte:
      return value->value.byte_value;
    case kWord:
      return value->value.word_value;
  }
  // Should never reach here, but return a default value to avoid warnings.
  return 0xFFFF;
}

// Helper function to sign-extend OperandValue to a 32-bit value. This makes it
// simpler to do direct arithmetic without worrying about overflow.
YAX86_PRIVATE int32_t FromSignedOperandValue(const OperandValue* value) {
  switch (value->width) {
    case kByte:
      return (int32_t)((int8_t)value->value.byte_value);
    case kWord:
      return (int32_t)((int16_t)value->value.word_value);
  }
  // Should never reach here, but return a default value to avoid warnings.
  return 0xFFFF;
}

// Helper function to extract a zero-extended value from an operand.
YAX86_PRIVATE uint32_t FromOperand(const Operand* operand) {
  return FromOperandValue(&operand->value);
}

// Helper function to extract a sign-extended value from an operand.
YAX86_PRIVATE int32_t FromSignedOperand(const Operand* operand) {
  return FromSignedOperandValue(&operand->value);
}

// Computes the raw effective address corresponding to a MemoryAddress.
YAX86_PRIVATE uint32_t
ToRawAddress(const CPUState* cpu, const MemoryAddress* address) {
  uint16_t segment = cpu->registers[address->segment_register_index];
  return (((uint32_t)segment) << 4) + (uint32_t)(address->offset);
}

// Read a byte from memory as a uint8_t.
YAX86_PRIVATE uint8_t ReadRawMemoryByte(CPUState* cpu, uint32_t raw_address) {
  return cpu->config->read_memory_byte
             ? cpu->config->read_memory_byte(cpu, raw_address)
             : 0xFF;
}

// Read a word from memory as a uint16_t.
YAX86_PRIVATE uint16_t ReadRawMemoryWord(CPUState* cpu, uint32_t raw_address) {
  uint8_t low_byte_value = ReadRawMemoryByte(cpu, raw_address);
  uint8_t high_byte_value = ReadRawMemoryByte(cpu, raw_address + 1);
  return (((uint16_t)high_byte_value) << 8) | (uint16_t)low_byte_value;
}

// Read a byte from memory to an OperandValue.
YAX86_PRIVATE OperandValue
ReadMemoryByte(CPUState* cpu, const OperandAddress* address) {
  uint8_t byte_value =
      ReadRawMemoryByte(cpu, ToRawAddress(cpu, &address->value.memory_address));
  return ByteValue(byte_value);
}

// Read a word from memory to an OperandValue.
YAX86_PRIVATE OperandValue
ReadMemoryWord(CPUState* cpu, const OperandAddress* address) {
  uint16_t word_value =
      ReadRawMemoryWord(cpu, ToRawAddress(cpu, &address->value.memory_address));
  return WordValue(word_value);
}

// Read a byte from a register to an OperandValue.
YAX86_PRIVATE OperandValue
ReadRegisterByte(CPUState* cpu, const OperandAddress* address) {
  const RegisterAddress* register_address = &address->value.register_address;
  uint8_t byte_value = cpu->registers[register_address->register_index] >>
                       register_address->byte_offset;
  return ByteValue(byte_value);
}

// Read a word from a register to an OperandValue.
YAX86_PRIVATE OperandValue
ReadRegisterWord(CPUState* cpu, const OperandAddress* address) {
  const RegisterAddress* register_address = &address->value.register_address;
  uint16_t word_value = cpu->registers[register_address->register_index];
  return WordValue(word_value);
}

// Table of Read* functions, indexed by OperandAddressType and Width.
YAX86_PRIVATE
OperandValue (*const kReadOperandValueFn[kNumOperandAddressTypes][kNumWidths])(
    CPUState* cpu, const OperandAddress* address) = {
    // kOperandTypeRegister
    {ReadRegisterByte, ReadRegisterWord},
    // kOperandTypeMemory
    {ReadMemoryByte, ReadMemoryWord},
};

// Write a byte as uint8_t to memory.
YAX86_PRIVATE void WriteRawMemoryByte(
    CPUState* cpu, uint32_t address, uint8_t value) {
  if (!cpu->config->write_memory_byte) {
    return;
  }
  cpu->config->write_memory_byte(cpu, address, value);
}

// Write a word as uint16_t to memory.
YAX86_PRIVATE void WriteRawMemoryWord(
    CPUState* cpu, uint32_t address, uint16_t value) {
  WriteRawMemoryByte(cpu, address, value & 0xFF);
  WriteRawMemoryByte(cpu, address + 1, (value >> 8) & 0xFF);
}

// Write a byte to memory.
YAX86_PRIVATE void WriteMemoryByte(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  WriteRawMemoryByte(
      cpu, ToRawAddress(cpu, &address->value.memory_address),
      value.value.byte_value);
}

// Write a word to memory.
YAX86_PRIVATE void WriteMemoryWord(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  WriteRawMemoryWord(
      cpu, ToRawAddress(cpu, &address->value.memory_address),
      value.value.word_value);
}

// Write a byte to a register.
YAX86_PRIVATE void WriteRegisterByte(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  const RegisterAddress* register_address = &address->value.register_address;
  const uint16_t updated_byte = ((uint16_t)value.value.byte_value)
                                << register_address->byte_offset;
  const uint16_t other_byte =
      cpu->registers[register_address->register_index] &
      (((uint16_t)0xFF) << (8 - register_address->byte_offset));
  cpu->registers[register_address->register_index] = other_byte | updated_byte;
}

// Write a word to a register.
YAX86_PRIVATE void WriteRegisterWord(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  const RegisterAddress* register_address = &address->value.register_address;
  cpu->registers[register_address->register_index] = value.value.word_value;
}

// Table of Write* functions, indexed by OperandAddressType and Width.
YAX86_PRIVATE void (*const kWriteOperandFn[kNumOperandAddressTypes]
                                          [kNumWidths])(
    CPUState* cpu, const OperandAddress* address, OperandValue value) = {
    // kOperandTypeRegister
    {WriteRegisterByte, WriteRegisterWord},
    // kOperandTypeMemory
    {WriteMemoryByte, WriteMemoryWord},
};

// Add an 8-bit signed relative offset to a 16-bit unsigned base address.
YAX86_PRIVATE uint16_t AddSignedOffsetByte(uint16_t base, uint8_t raw_offset) {
  // Sign-extend the offset to 32 bits
  int32_t signed_offset = (int32_t)((int8_t)raw_offset);
  // Zero-extend base to 32 bits
  int32_t signed_base = (int32_t)base;
  // Add the two 32-bit signed values then truncate back down to 16-bit unsigned
  return (uint16_t)(signed_base + signed_offset);
}

// Add a 16-bit signed relative offset to a 16-bit unsigned base address.
YAX86_PRIVATE uint16_t AddSignedOffsetWord(uint16_t base, uint16_t raw_offset) {
  // Sign-extend the offset to 32 bits
  int32_t signed_offset = (int32_t)((int16_t)raw_offset);
  // Zero-extend base to 32 bits
  int32_t signed_base = (int32_t)base;
  // Add the two 32-bit signed values then truncate back down to 16-bit unsigned
  return (uint16_t)(signed_base + signed_offset);
}

// Get the register operand for a byte instruction based on the ModR/M byte's
// reg or R/M field.
YAX86_PRIVATE RegisterAddress
GetRegisterAddressByte(CPUState* cpu, uint8_t reg_or_rm) {
  (void)cpu;
  RegisterAddress address;
  if (reg_or_rm < 4) {
    // AL, CL, DL, BL
    address.register_index = (RegisterIndex)reg_or_rm;
    address.byte_offset = 0;
  } else {
    // AH, CH, DH, BH
    address.register_index = (RegisterIndex)(reg_or_rm - 4);
    address.byte_offset = 8;
  }
  return address;
}

// Get the register operand for a word instruction based on the ModR/M byte's
// reg or R/M field.
YAX86_PRIVATE RegisterAddress
GetRegisterAddressWord(CPUState* cpu, uint8_t reg_or_rm) {
  (void)cpu;
  const RegisterAddress address = {
      .register_index = (RegisterIndex)reg_or_rm, .byte_offset = 0};
  return address;
}

// Table of GetRegisterAddress functions, indexed by Width.
YAX86_PRIVATE RegisterAddress (*const kGetRegisterAddressFn[kNumWidths])(
    CPUState* cpu, uint8_t reg_or_rm) = {
  GetRegisterAddressByte,  // kByte
  GetRegisterAddressWord   // kWord
};

// Apply segment override prefixes to a MemoryAddress.
YAX86_PRIVATE void ApplySegmentOverride(
    const Instruction* instruction, MemoryAddress* address) {
  for (int i = 0; i < instruction->prefix_size; ++i) {
    switch (instruction->prefix[i]) {
      case kPrefixES:
        address->segment_register_index = kES;
        break;
      case kPrefixCS:
        address->segment_register_index = kCS;
        break;
      case kPrefixSS:
        address->segment_register_index = kSS;
        break;
      case kPrefixDS:
        address->segment_register_index = kDS;
        break;
      default:
        // Ignore other prefixes
        break;
    }
  }
}

// Compute the memory address for an instruction.
YAX86_PRIVATE MemoryAddress
GetMemoryOperandAddress(CPUState* cpu, const Instruction* instruction) {
  MemoryAddress address;
  uint8_t mod = instruction->mod_rm.mod;
  uint8_t rm = instruction->mod_rm.rm;
  switch (rm) {
    case 0:  // [BX + SI]
      address.offset = cpu->registers[kBX] + cpu->registers[kSI];
      address.segment_register_index = kDS;
      break;
    case 1:  // [BX + DI]
      address.offset = cpu->registers[kBX] + cpu->registers[kDI];
      address.segment_register_index = kDS;
      break;
    case 2:  // [BP + SI]
      address.offset = cpu->registers[kBP] + cpu->registers[kSI];
      address.segment_register_index = kSS;
      break;
    case 3:  // [BP + DI]
      address.offset = cpu->registers[kBP] + cpu->registers[kDI];
      address.segment_register_index = kSS;
      break;
    case 4:  // [SI]
      address.offset = cpu->registers[kSI];
      address.segment_register_index = kDS;
      break;
    case 5:  // [DI]
      address.offset = cpu->registers[kDI];
      address.segment_register_index = kDS;
      break;
    case 6:
      if (mod == 0) {
        // Direct memory address with 16-bit displacement
        address.offset = 0;
        address.segment_register_index = kDS;
      } else {
        // [BP]
        address.offset = cpu->registers[kBP];
        address.segment_register_index = kSS;
      }
      break;
    case 7:  // [BX]
      address.offset = cpu->registers[kBX];
      address.segment_register_index = kDS;
      break;
    default:
      // Not possible as RM field is 3 bits (0-7).
      address.offset = 0xFFFF;
      address.segment_register_index = kDS;  // Invalid RM field
      break;
  }

  // Apply segment override prefixes if present
  ApplySegmentOverride(instruction, &address);

  // Add displacement if present
  switch (instruction->displacement_size) {
    case 1: {
      uint8_t raw_displacement = instruction->displacement[0];
      address.offset = AddSignedOffsetByte(address.offset, raw_displacement);
      break;
    }
    case 2: {
      // Concatenate the two displacement bytes as an unsigned 16-bit integer
      uint16_t raw_displacement =
          ((uint16_t)instruction->displacement[0]) |
          (((uint16_t)instruction->displacement[1]) << 8);
      address.offset = AddSignedOffsetWord(address.offset, raw_displacement);
      break;
    }
    default:
      // No displacement
      break;
  }

  return address;
}

// Get a register or memory operand address based on the ModR/M byte and
// displacement.
YAX86_PRIVATE OperandAddress GetRegisterOrMemoryOperandAddress(
    CPUState* cpu, const Instruction* instruction, Width width) {
  OperandAddress address;
  uint8_t mod = instruction->mod_rm.mod;
  uint8_t rm = instruction->mod_rm.rm;
  if (mod == 3) {
    // Register operand
    address.type = kOperandAddressTypeRegister;
    address.value.register_address = kGetRegisterAddressFn[width](cpu, rm);
  } else {
    // Memory operand
    address.type = kOperandAddressTypeMemory;
    address.value.memory_address = GetMemoryOperandAddress(cpu, instruction);
  }
  return address;
}

// Read an 8-bit immediate value.
YAX86_PRIVATE OperandValue ReadImmediateByte(const Instruction* instruction) {
  return ByteValue(instruction->immediate[0]);
}

// Read a 16-bit immediate value.
YAX86_PRIVATE OperandValue ReadImmediateWord(const Instruction* instruction) {
  return WordValue(
      ((uint16_t)instruction->immediate[0]) |
      (((uint16_t)instruction->immediate[1]) << 8));
}

// Table of ReadImmediate* functions, indexed by Width.
YAX86_PRIVATE OperandValue (*const kReadImmediateValueFn[kNumWidths])(
    const Instruction* instruction) = {
  ReadImmediateByte,  // kByte
  ReadImmediateWord   // kWord
};

// Read a value from an operand address.
YAX86_PRIVATE OperandValue
ReadOperandValue(const InstructionContext* ctx, const OperandAddress* address) {
  return kReadOperandValueFn[address->type][ctx->metadata->width](
      ctx->cpu, address);
}

// Get a register or memory operand for an instruction based on the ModR/M
// byte and displacement.
YAX86_PRIVATE Operand
ReadRegisterOrMemoryOperand(const InstructionContext* ctx) {
  Width width = ctx->metadata->width;
  Operand operand;
  operand.address =
      GetRegisterOrMemoryOperandAddress(ctx->cpu, ctx->instruction, width);
  operand.value = ReadOperandValue(ctx, &operand.address);
  return operand;
}

// Get a register operand for an instruction.
YAX86_PRIVATE Operand ReadRegisterOperandForRegisterIndex(
    const InstructionContext* ctx, RegisterIndex register_index) {
  Width width = ctx->metadata->width;
  Operand operand = {
      .address = {
          .type = kOperandAddressTypeRegister,
          .value = {
              .register_address =
                  kGetRegisterAddressFn[width](ctx->cpu, register_index),
          }}};
  operand.value = ReadOperandValue(ctx, &operand.address);
  return operand;
}

// Get a register operand for an instruction from the REG field of the Mod/RM
// byte.
YAX86_PRIVATE Operand ReadRegisterOperand(const InstructionContext* ctx) {
  return ReadRegisterOperandForRegisterIndex(
      ctx, (RegisterIndex)ctx->instruction->mod_rm.reg);
}

// Get a segment register operand for an instruction from the REG field of the
// Mod/RM byte.
YAX86_PRIVATE Operand
ReadSegmentRegisterOperand(const InstructionContext* ctx) {
  return ReadRegisterOperandForRegisterIndex(
      ctx, (RegisterIndex)(ctx->instruction->mod_rm.reg + 8));
}

// Write a value to a register or memory operand address.
YAX86_PRIVATE void WriteOperandAddress(
    const InstructionContext* ctx, const OperandAddress* address,
    uint32_t raw_value) {
  Width width = ctx->metadata->width;
  kWriteOperandFn[address->type][width](
      ctx->cpu, address, ToOperandValue(width, raw_value));
}

// Write a value to a register or memory operand.
YAX86_PRIVATE void WriteOperand(
    const InstructionContext* ctx, const Operand* operand, uint32_t raw_value) {
  WriteOperandAddress(ctx, &operand->address, raw_value);
}

// Read an immediate value from the instruction.
YAX86_PRIVATE OperandValue ReadImmediate(const InstructionContext* ctx) {
  Width width = ctx->metadata->width;
  return kReadImmediateValueFn[width](ctx->instruction);
}
