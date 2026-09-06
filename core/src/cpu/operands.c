#ifndef YAX86_IMPLEMENTATION
#include "operands.h"

#include "../util/common.h"
#include "cycles.h"
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

enum {
  // The address bus is 20 bits wide. A segment base plus an offset can sum to
  // as much as 0x10FFEF, and the carry out of bit 19 goes nowhere, so an
  // address past the top of memory wraps around to the bottom.
  kPhysicalAddressMask = 0xFFFFF,
};

// Computes the raw effective address corresponding to a MemoryAddress.
YAX86_PRIVATE uint32_t
ToRawAddress(const CPUState* cpu, const MemoryAddress* address) {
  uint16_t segment = cpu->registers[address->segment_register_index];
  return ((((uint32_t)segment) << 4) + (uint32_t)(address->offset)) &
         kPhysicalAddressMask;
}

// The address of the byte following a memory operand. The offset is 16 bits
// wide and wraps within the segment, so the high byte of a word at offset
// 0xFFFF comes from offset 0 of the same segment rather than from the
// paragraph above it.
static MemoryAddress NextMemoryAddress(const MemoryAddress* address) {
  MemoryAddress next_address = *address;
  ++next_address.offset;
  return next_address;
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
YAX86_HOT YAX86_PRIVATE OperandValue
ReadMemoryOperandByte(CPUState* cpu, const OperandAddress* address) {
  AddBusCycles(cpu, 1);
  uint8_t byte_value =
      ReadRawMemoryByte(cpu, ToRawAddress(cpu, &address->value.memory_address));
  return ByteValue(byte_value);
}

// Read a word from memory to an OperandValue.
YAX86_HOT YAX86_PRIVATE OperandValue
ReadMemoryOperandWord(CPUState* cpu, const OperandAddress* address) {
  AddBusCycles(cpu, 2);
  const MemoryAddress* low_byte_address = &address->value.memory_address;
  const MemoryAddress high_byte_address = NextMemoryAddress(low_byte_address);
  uint8_t low_byte_value =
      ReadRawMemoryByte(cpu, ToRawAddress(cpu, low_byte_address));
  uint8_t high_byte_value =
      ReadRawMemoryByte(cpu, ToRawAddress(cpu, &high_byte_address));
  return WordValue(
      (((uint16_t)high_byte_value) << 8) | (uint16_t)low_byte_value);
}

// Read a byte from a register to an OperandValue.
YAX86_HOT YAX86_PRIVATE OperandValue
ReadRegisterOperandByte(CPUState* cpu, const OperandAddress* address) {
  const RegisterAddress* register_address = &address->value.register_address;
  uint8_t byte_value = cpu->registers[register_address->register_index] >>
                       register_address->byte_offset;
  return ByteValue(byte_value);
}

// Read a word from a register to an OperandValue.
YAX86_HOT YAX86_PRIVATE OperandValue
ReadRegisterOperandWord(CPUState* cpu, const OperandAddress* address) {
  const RegisterAddress* register_address = &address->value.register_address;
  uint16_t word_value = cpu->registers[register_address->register_index];
  return WordValue(word_value);
}

// The four functions above, and the read, write and immediate variants further
// down, used to be selected through tables of function pointers indexed by
// OperandAddressType and Width. Every operand of every instruction went through
// one, and an indirect call is a pair of loads and a branch the compiler cannot
// see through - on a Cortex-M0+, which has no branch predictor and no
// speculation, it also cannot be overlapped with anything. Selecting with an
// explicit test on a one-bit width instead lets the compiler inline both sides,
// and each of these is a handful of instructions once inlined.
//
// Taking their addresses was not free either. A function whose address is
// stored in a table has to exist out of line whether or not anything reaches it
// that way, so the table kept eight copies alive that are now inlined away.

// Write a byte as uint8_t to memory.
YAX86_PRIVATE void WriteRawMemoryByte(
    CPUState* cpu, uint32_t address, uint8_t value) {
  if (!cpu->config->write_memory_byte) {
    return;
  }
  cpu->config->write_memory_byte(cpu, address, value);
}

// Write a byte to memory.
YAX86_HOT YAX86_PRIVATE void WriteMemoryOperandByte(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  AddBusCycles(cpu, 1);
  WriteRawMemoryByte(
      cpu, ToRawAddress(cpu, &address->value.memory_address),
      value.value.byte_value);
}

// Write a word to memory.
YAX86_HOT YAX86_PRIVATE void WriteMemoryOperandWord(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  AddBusCycles(cpu, 2);
  const MemoryAddress* low_byte_address = &address->value.memory_address;
  const MemoryAddress high_byte_address = NextMemoryAddress(low_byte_address);
  WriteRawMemoryByte(
      cpu, ToRawAddress(cpu, low_byte_address), value.value.word_value & 0xFF);
  WriteRawMemoryByte(
      cpu, ToRawAddress(cpu, &high_byte_address),
      (value.value.word_value >> 8) & 0xFF);
}

// Write a byte to a register.
YAX86_HOT YAX86_PRIVATE void WriteRegisterOperandByte(
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
YAX86_HOT YAX86_PRIVATE void WriteRegisterOperandWord(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  const RegisterAddress* register_address = &address->value.register_address;
  cpu->registers[register_address->register_index] = value.value.word_value;
}

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
YAX86_HOT YAX86_PRIVATE RegisterAddress
GetRegisterAddressByte(YAX86_UNUSED CPUState* cpu, uint8_t reg_or_rm) {
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
GetRegisterAddressWord(YAX86_UNUSED CPUState* cpu, uint8_t reg_or_rm) {
  const RegisterAddress address = {
      .register_index = (RegisterIndex)reg_or_rm, .byte_offset = 0};
  return address;
}

// Apply segment override prefixes to a MemoryAddress.
YAX86_PRIVATE void ApplySegmentOverride(
    const Instruction* instruction, MemoryAddress* address) {
  if (instruction->segment_override != kNoSegmentOverride) {
    address->segment_register_index =
        (RegisterIndex)instruction->segment_override;
  }
}

// Compute the memory address for an instruction.
YAX86_HOT YAX86_PRIVATE MemoryAddress
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
// displacement, without reading the value currently there.
//
// An instruction that overwrites its destination completely - as opposed to a
// read-modify-write - resolves the address with this and stores through
// WriteOperandAddress(). Reading the destination on the way is not free: it
// charges the bus cycles of a memory access the 8088 never performs.
//
// Always inlined. With more than one caller, -Os and -O2 emit it out of line,
// which puts a call and its register shuffling on the hottest path in the
// emulator - 3.6% at -O2.
YAX86_ALWAYS_INLINE YAX86_PRIVATE OperandAddress
GetRegisterOrMemoryOperandAddress(const InstructionContext* ctx) {
  CPUState* cpu = ctx->cpu;
  const Instruction* instruction = ctx->instruction;
  OperandAddress address;
  uint8_t mod = instruction->mod_rm.mod;
  uint8_t rm = instruction->mod_rm.rm;
  if (mod == 3) {
    // Register operand
    address.type = kOperandAddressTypeRegister;
    address.value.register_address = ctx->metadata->width == kByte
                                         ? GetRegisterAddressByte(cpu, rm)
                                         : GetRegisterAddressWord(cpu, rm);
  } else {
    // Memory operand
    address.type = kOperandAddressTypeMemory;
    address.value.memory_address = GetMemoryOperandAddress(cpu, instruction);
  }
  return address;
}

// Read an 8-bit immediate value.
YAX86_HOT YAX86_PRIVATE OperandValue
ReadImmediateOperandByte(const Instruction* instruction) {
  return ByteValue(instruction->immediate[0]);
}

// Read a 16-bit immediate value.
YAX86_HOT YAX86_PRIVATE OperandValue
ReadImmediateOperandWord(const Instruction* instruction) {
  return WordValue(
      ((uint16_t)instruction->immediate[0]) |
      (((uint16_t)instruction->immediate[1]) << 8));
}

// Read a value from an operand address.
YAX86_PRIVATE OperandValue
ReadOperandValue(const InstructionContext* ctx, const OperandAddress* address) {
  const Width width = ctx->metadata->width;
  if (address->type == kOperandAddressTypeRegister) {
    return width == kByte ? ReadRegisterOperandByte(ctx->cpu, address)
                          : ReadRegisterOperandWord(ctx->cpu, address);
  }
  return width == kByte ? ReadMemoryOperandByte(ctx->cpu, address)
                        : ReadMemoryOperandWord(ctx->cpu, address);
}

// Get a register or memory operand for an instruction based on the ModR/M
// byte and displacement.
YAX86_HOT YAX86_PRIVATE Operand
ReadRegisterOrMemoryOperand(const InstructionContext* ctx) {
  Operand operand;
  operand.address = GetRegisterOrMemoryOperandAddress(ctx);
  operand.value = ReadOperandValue(ctx, &operand.address);
  return operand;
}

// Get a register operand for an instruction.
YAX86_HOT YAX86_PRIVATE Operand ReadRegisterOperandForRegisterIndex(
    const InstructionContext* ctx, RegisterIndex register_index) {
  Width width = ctx->metadata->width;
  Operand operand = {
      .address = {
          .type = kOperandAddressTypeRegister,
          .value = {
              .register_address =
                  (width == kByte
                       ? GetRegisterAddressByte(ctx->cpu, register_index)
                       : GetRegisterAddressWord(ctx->cpu, register_index)),
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
  // The segment register field is only two bits wide. The 8086/8088 does not
  // decode the third bit at all, so REG 4 through 7 name the same four
  // registers over again - which is what makes 0x8C and 0x8E accept every REG
  // value. Masking it also keeps the index inside the register array, which
  // REG 4 and above would otherwise run past the end of.
  return ReadRegisterOperandForRegisterIndex(
      ctx, (RegisterIndex)(kES + (ctx->instruction->mod_rm.reg & 0x03)));
}

// Write a value to a register or memory operand address.
YAX86_HOT YAX86_PRIVATE void WriteOperandAddress(
    const InstructionContext* ctx, const OperandAddress* address,
    uint32_t raw_value) {
  const Width width = ctx->metadata->width;
  const OperandValue value = ToOperandValue(width, raw_value);
  if (address->type == kOperandAddressTypeRegister) {
    if (width == kByte) {
      WriteRegisterOperandByte(ctx->cpu, address, value);
    } else {
      WriteRegisterOperandWord(ctx->cpu, address, value);
    }
  } else if (width == kByte) {
    WriteMemoryOperandByte(ctx->cpu, address, value);
  } else {
    WriteMemoryOperandWord(ctx->cpu, address, value);
  }
}

// Write a value to a register or memory operand.
YAX86_PRIVATE void WriteOperand(
    const InstructionContext* ctx, const Operand* operand, uint32_t raw_value) {
  WriteOperandAddress(ctx, &operand->address, raw_value);
}

// Read an immediate value from the instruction.
YAX86_PRIVATE OperandValue ReadImmediate(const InstructionContext* ctx) {
  return ctx->metadata->width == kByte
             ? ReadImmediateOperandByte(ctx->instruction)
             : ReadImmediateOperandWord(ctx->instruction);
}
