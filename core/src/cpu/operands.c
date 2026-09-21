#ifndef YAX86_IMPLEMENTATION
#include "operands.h"

#include "../util/common.h"
#include "cycles.h"
#endif  // YAX86_IMPLEMENTATION

// What the unreachable arm of a width dispatch returns.
//
// Width names two values and every switch below handles both, so nothing
// reaches these arms. They exist because C does not promise an enum object
// holds only the values its enumerators name, and a function with a return
// type has to return something.
//
// All ones in the low byte is what an 8086 reads from an address nothing
// drives, and it is a legal value at either width. 0xFFFF is not: at kByte it
// would break the zero high byte a byte-wide value is promised to have, which
// is the invariant FromOperandValue() widens by doing nothing.
enum { kUnreachableOperandValue = 0xFF };

// Narrow a computed result to what an operand of the given width holds. A
// byte keeps a zero high byte, which is what makes widening it again free.
YAX86_MODULE_PRIVATE OperandValue ToOperandValue(Width width, uint32_t raw_value) {
  return (OperandValue)(raw_value & kMaxValue[width]);
}

// Widen an operand value for arithmetic, which is done in 32 bits so that
// nothing overflows on the way. A byte value already has a zero high byte, so
// this is where that invariant is spent rather than a width being consulted.
YAX86_MODULE_PRIVATE uint32_t FromOperandValue(OperandValue value) { return value; }

// The same, sign-extended. This one does need the width, since which bit is
// the sign bit is exactly what the value no longer says.
YAX86_MODULE_PRIVATE int32_t FromSignedOperandValue(Width width, OperandValue value) {
  switch (width) {
    case kByte:
      return (int32_t)((int8_t)value);
    case kWord:
      return (int32_t)((int16_t)value);
  }
  // Should never reach here, but return a default value to avoid warnings.
  return kUnreachableOperandValue;
}

// Helper function to extract a zero-extended value from an operand.
YAX86_MODULE_PRIVATE uint32_t FromOperand(const Operand* operand) {
  return FromOperandValue(operand->value);
}

// Helper function to extract a sign-extended value from an operand.
YAX86_MODULE_PRIVATE int32_t FromSignedOperand(Width width, const Operand* operand) {
  return FromSignedOperandValue(width, operand->value);
}

enum {
  // The address bus is 20 bits wide. A segment base plus an offset can sum to
  // as much as 0x10FFEF, and the carry out of bit 19 goes nowhere, so an
  // address past the top of memory wraps around to the bottom.
  kPhysicalAddressMask = 0xFFFFF,
};

// Computes the raw address a segment register and an offset address.
YAX86_MODULE_PRIVATE uint32_t ToRawAddress(
    const CPUState* cpu, uint8_t segment_register_index, uint16_t offset) {
  uint16_t segment = cpu->registers[segment_register_index];
  return ((((uint32_t)segment) << 4) + (uint32_t)offset) & kPhysicalAddressMask;
}

// Read a byte from memory as a uint8_t.
YAX86_MODULE_PRIVATE uint8_t ReadRawMemoryByte(CPUState* cpu, uint32_t raw_address) {
  // Guest RAM, where nearly every operand lands, is reached by indexing. The
  // call below ends up indexing an array too, having gone through a function
  // pointer, the host's context and a memory map lookup to arrive at it.
  if (raw_address < cpu->direct_data_window.end) {
    return cpu->direct_data_window.data[raw_address];
  }
  return cpu->config.read_memory_byte
             ? cpu->config.read_memory_byte(cpu, raw_address)
             : 0xFF;
}

// Read a word from memory as a uint16_t.
YAX86_MODULE_PRIVATE uint16_t ReadRawMemoryWord(CPUState* cpu, uint32_t raw_address) {
  uint8_t low_byte_value = ReadRawMemoryByte(cpu, raw_address);
  uint8_t high_byte_value = ReadRawMemoryByte(cpu, raw_address + 1);
  return (((uint16_t)high_byte_value) << 8) | (uint16_t)low_byte_value;
}

// Read a byte from memory to an OperandValue.
YAX86_MODULE_PRIVATE YAX86_HOT OperandValue
ReadMemoryOperandByte(CPUState* cpu, const OperandAddress* address) {
  AddBusCycles(cpu, 1);
  return ReadRawMemoryByte(
      cpu, ToRawAddress(cpu, address->register_index, address->offset));
}

// Read a word from memory to an OperandValue.
YAX86_MODULE_PRIVATE YAX86_HOT OperandValue
ReadMemoryOperandWord(CPUState* cpu, const OperandAddress* address) {
  AddBusCycles(cpu, 2);
  // The offset is 16 bits wide and wraps within the segment, so the high byte
  // of a word at offset 0xFFFF comes from offset 0 of the same segment rather
  // than from the paragraph above it.
  const uint8_t segment = address->register_index;
  uint8_t low_byte_value =
      ReadRawMemoryByte(cpu, ToRawAddress(cpu, segment, address->offset));
  uint8_t high_byte_value = ReadRawMemoryByte(
      cpu, ToRawAddress(cpu, segment, (uint16_t)(address->offset + 1)));
  return (OperandValue)((((uint16_t)high_byte_value) << 8) |
                        (uint16_t)low_byte_value);
}

// Read a memory operand of the given width to an OperandValue.
YAX86_MODULE_PRIVATE OperandValue ReadMemoryOperandValue(
    CPUState* cpu, const OperandAddress* address, Width width) {
  switch (width) {
    case kByte:
      return ReadMemoryOperandByte(cpu, address);
    case kWord:
      return ReadMemoryOperandWord(cpu, address);
  }
  // Should never reach here, but return a default value to avoid warnings.
  return kUnreachableOperandValue;
}

// Read a byte from a register to an OperandValue.
YAX86_MODULE_PRIVATE YAX86_HOT OperandValue
ReadRegisterOperandByte(CPUState* cpu, const OperandAddress* address) {
  // Truncated to a byte, which is the invariant every consumer widens by
  // doing nothing: AH and the high half of a word register live in the bits
  // this drops.
  return (uint8_t)(cpu->registers[address->register_index] >> address->offset);
}

// Read a word from a register to an OperandValue.
YAX86_MODULE_PRIVATE YAX86_HOT OperandValue
ReadRegisterOperandWord(CPUState* cpu, const OperandAddress* address) {
  return cpu->registers[address->register_index];
}

// Read a register operand of the given width to an OperandValue.
YAX86_MODULE_PRIVATE OperandValue ReadRegisterOperandValue(
    CPUState* cpu, const OperandAddress* address, Width width) {
  switch (width) {
    case kByte:
      return ReadRegisterOperandByte(cpu, address);
    case kWord:
      return ReadRegisterOperandWord(cpu, address);
  }
  // Should never reach here, but return a default value to avoid warnings.
  return kUnreachableOperandValue;
}

// Write a byte as uint8_t to memory.
YAX86_MODULE_PRIVATE void WriteRawMemoryByte(
    CPUState* cpu, uint32_t address, uint8_t value) {
  // Every write the CPU makes comes through here - operands, stack pushes and
  // the interrupt vector alike - which is what lets a host report only the
  // writes it makes some other way.
  CPUNotifyMemoryWrite(cpu, address);
  if (address < cpu->direct_data_window.end) {
    cpu->direct_data_window.data[address] = value;
    return;
  }
  if (!cpu->config.write_memory_byte) {
    return;
  }
  cpu->config.write_memory_byte(cpu, address, value);
}

// Write a byte to memory.
YAX86_MODULE_PRIVATE YAX86_HOT void WriteMemoryOperandByte(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  AddBusCycles(cpu, 1);
  WriteRawMemoryByte(
      cpu, ToRawAddress(cpu, address->register_index, address->offset),
      (uint8_t)value);
}

// Write a word to memory.
YAX86_MODULE_PRIVATE YAX86_HOT void WriteMemoryOperandWord(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  AddBusCycles(cpu, 2);
  // See ReadMemoryOperandWord() for why the high byte's offset wraps within
  // the segment.
  const uint8_t segment = address->register_index;
  WriteRawMemoryByte(
      cpu, ToRawAddress(cpu, segment, address->offset), (uint8_t)value);
  WriteRawMemoryByte(
      cpu, ToRawAddress(cpu, segment, (uint16_t)(address->offset + 1)),
      (uint8_t)(value >> 8));
}

// Write a memory operand of the given width.
YAX86_MODULE_PRIVATE void WriteMemoryOperand(
    CPUState* cpu, const OperandAddress* address, OperandValue value,
    Width width) {
  switch (width) {
    case kByte:
      WriteMemoryOperandByte(cpu, address, value);
      return;
    case kWord:
      WriteMemoryOperandWord(cpu, address, value);
      return;
  }
  // Should never reach here. Writing nothing is the safe default.
}

// Write a byte to a register.
YAX86_MODULE_PRIVATE YAX86_HOT void WriteRegisterOperandByte(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  const uint16_t updated_byte = ((uint16_t)(uint8_t)value) << address->offset;
  const uint16_t other_byte = cpu->registers[address->register_index] &
                              (((uint16_t)0xFF) << (8 - address->offset));
  cpu->registers[address->register_index] = other_byte | updated_byte;
}

// Write a word to a register.
YAX86_MODULE_PRIVATE YAX86_HOT void WriteRegisterOperandWord(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  cpu->registers[address->register_index] = value;
}

// Write a register operand of the given width.
YAX86_MODULE_PRIVATE void WriteRegisterOperand(
    CPUState* cpu, const OperandAddress* address, OperandValue value,
    Width width) {
  switch (width) {
    case kByte:
      WriteRegisterOperandByte(cpu, address, value);
      return;
    case kWord:
      WriteRegisterOperandWord(cpu, address, value);
      return;
  }
  // Should never reach here. Writing nothing is the safe default.
}

// Add an 8-bit signed relative offset to a 16-bit unsigned base address.
YAX86_MODULE_PRIVATE uint16_t AddSignedOffsetByte(uint16_t base, uint8_t raw_offset) {
  // Sign-extend the offset to 32 bits
  int32_t signed_offset = (int32_t)((int8_t)raw_offset);
  // Zero-extend base to 32 bits
  int32_t signed_base = (int32_t)base;
  // Add the two 32-bit signed values then truncate back down to 16-bit unsigned
  return (uint16_t)(signed_base + signed_offset);
}

// Add a 16-bit signed relative offset to a 16-bit unsigned base address.
YAX86_MODULE_PRIVATE uint16_t AddSignedOffsetWord(uint16_t base, uint16_t raw_offset) {
  // Sign-extend the offset to 32 bits
  int32_t signed_offset = (int32_t)((int16_t)raw_offset);
  // Zero-extend base to 32 bits
  int32_t signed_base = (int32_t)base;
  // Add the two 32-bit signed values then truncate back down to 16-bit unsigned
  return (uint16_t)(signed_base + signed_offset);
}

// Get the register operand for a byte instruction based on the ModR/M byte's
// reg or R/M field.
YAX86_MODULE_PRIVATE YAX86_HOT RegisterAddress
GetRegisterAddressByte(YAX86_UNUSED CPUState* cpu, uint8_t reg_or_rm) {
  RegisterAddress address;
  if (reg_or_rm < 4) {
    // AL, CL, DL, BL
    address.register_index = reg_or_rm;
    address.byte_offset = 0;
  } else {
    // AH, CH, DH, BH
    address.register_index = reg_or_rm - 4;
    address.byte_offset = 8;
  }
  return address;
}

// Get the register operand for a word instruction based on the ModR/M byte's
// reg or R/M field.
YAX86_MODULE_PRIVATE RegisterAddress
GetRegisterAddressWord(YAX86_UNUSED CPUState* cpu, uint8_t reg_or_rm) {
  const RegisterAddress address = {
      .register_index = reg_or_rm, .byte_offset = 0};
  return address;
}

// Get the register operand of the given width from the ModR/M byte's reg or
// R/M field.
YAX86_MODULE_PRIVATE RegisterAddress
GetRegisterAddress(CPUState* cpu, uint8_t reg_or_rm, Width width) {
  switch (width) {
    case kByte:
      return GetRegisterAddressByte(cpu, reg_or_rm);
    case kWord:
      return GetRegisterAddressWord(cpu, reg_or_rm);
  }
  // Should never reach here. AL is in range for both widths, so a caller that
  // somehow got here names a real register rather than running off the array.
  const RegisterAddress fallback = {.register_index = kAX, .byte_offset = 0};
  return fallback;
}

// Replace a segment register index with whatever the instruction's segment
// override prefix names, if it carries one.
YAX86_MODULE_PRIVATE void ApplySegmentOverride(
    const Instruction* instruction, uint8_t* segment_register_index) {
  if (instruction->segment_override != kNoSegmentOverride) {
    *segment_register_index = instruction->segment_override;
  }
}

// Compute the memory address for an instruction.
YAX86_MODULE_PRIVATE YAX86_HOT MemoryAddress
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
  ApplySegmentOverride(instruction, &address.segment_register_index);

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
YAX86_MODULE_PRIVATE YAX86_ALWAYS_INLINE OperandAddress
GetRegisterOrMemoryOperandAddress(const InstructionContext* ctx) {
  CPUState* cpu = ctx->cpu;
  const Instruction* instruction = ctx->instruction;
  OperandAddress address;
  uint8_t mod = instruction->mod_rm.mod;
  uint8_t rm = instruction->mod_rm.rm;
  if (mod == 3) {
    // Register operand
    const RegisterAddress register_address =
        GetRegisterAddress(cpu, rm, ctx->metadata->width);
    address.type = kOperandAddressTypeRegister;
    address.register_index = register_address.register_index;
    address.offset = register_address.byte_offset;
  } else {
    // Memory operand
    const MemoryAddress memory_address =
        GetMemoryOperandAddress(cpu, instruction);
    address.type = kOperandAddressTypeMemory;
    address.register_index = (uint8_t)memory_address.segment_register_index;
    address.offset = memory_address.offset;
  }
  return address;
}

// Read an 8-bit immediate value.
YAX86_MODULE_PRIVATE YAX86_HOT OperandValue
ReadImmediateOperandByte(const Instruction* instruction) {
  return instruction->immediate[0];
}

// Read a 16-bit immediate value.
YAX86_MODULE_PRIVATE YAX86_HOT OperandValue
ReadImmediateOperandWord(const Instruction* instruction) {
  return (OperandValue)(((uint16_t)instruction->immediate[0]) |
                        (((uint16_t)instruction->immediate[1]) << 8));
}

// Read an immediate value of the given width.
YAX86_MODULE_PRIVATE OperandValue
ReadImmediateOperand(const Instruction* instruction, Width width) {
  switch (width) {
    case kByte:
      return ReadImmediateOperandByte(instruction);
    case kWord:
      return ReadImmediateOperandWord(instruction);
  }
  // Should never reach here, but return a default value to avoid warnings.
  return kUnreachableOperandValue;
}

// Read a value from an operand address.
//
// Always inlined. Left to itself GCC emits this out of line, in flash, and
// puts a veneer and an XIP fetch on every operand read - 5.4% at -O3.
YAX86_MODULE_PRIVATE YAX86_ALWAYS_INLINE OperandValue
ReadOperandValue(const InstructionContext* ctx, const OperandAddress* address) {
  // Not a switch, unlike the width dispatch it calls into. OperandAddressType
  // is a plain enum field rather than a bitfield, so most of the values it can
  // hold are ones the enum does not name and the compiler cannot prove a
  // switch's default arm unreachable: the memory path, which is the common
  // one, ends up paying an extra compare and branch for a case that cannot
  // happen. Width is a one-bit bitfield, which has no unnamed value to
  // represent, so there the default arm disappears from the output entirely.
  // Switching this one too costs 1.06% at -O3.
  //
  // Narrowing type to a one-bit bitfield to make the switch free was measured
  // and is worse still - GetRegisterOrMemoryOperandAddress() writes this field
  // for every operand, and a write to a bitfield is a read-modify-write.
  // Widening OpcodeMetadata.width to a byte would likewise put the width
  // switches in this same position; see AGENTS.md before doing either.
  const Width width = ctx->metadata->width;
  if (address->type == kOperandAddressTypeRegister) {
    return ReadRegisterOperandValue(ctx->cpu, address, width);
  }
  return ReadMemoryOperandValue(ctx->cpu, address, width);
}

// Get a register or memory operand for an instruction based on the ModR/M
// byte and displacement.
YAX86_MODULE_PRIVATE YAX86_HOT void ReadRegisterOrMemoryOperand(
    const InstructionContext* ctx, Operand* operand) {
  operand->address = GetRegisterOrMemoryOperandAddress(ctx);
  operand->value = ReadOperandValue(ctx, &operand->address);
}

// Get a register operand for an instruction.
YAX86_MODULE_PRIVATE YAX86_HOT void ReadRegisterOperandForRegisterIndex(
    const InstructionContext* ctx, RegisterIndex register_index,
    Operand* operand) {
  const Width width = ctx->metadata->width;
  // Do not fold these into an initializer: C would zero the whole struct
  // before a single field of it is written.
  operand->address.type = kOperandAddressTypeRegister;
  const RegisterAddress register_address =
      GetRegisterAddress(ctx->cpu, register_index, width);
  operand->address.register_index = register_address.register_index;
  operand->address.offset = register_address.byte_offset;
  operand->value = ReadOperandValue(ctx, &operand->address);
}

// Get a register operand for an instruction from the REG field of the Mod/RM
// byte.
YAX86_MODULE_PRIVATE void ReadRegisterOperand(
    const InstructionContext* ctx, Operand* operand) {
  ReadRegisterOperandForRegisterIndex(
      ctx, (RegisterIndex)ctx->instruction->mod_rm.reg, operand);
}

// Get a segment register operand for an instruction from the REG field of the
// Mod/RM byte.
YAX86_MODULE_PRIVATE void ReadSegmentRegisterOperand(
    const InstructionContext* ctx, Operand* operand) {
  // The segment register field is only two bits wide. The 8086/8088 does not
  // decode the third bit at all, so REG 4 through 7 name the same four
  // registers over again - which is what makes 0x8C and 0x8E accept every REG
  // value. Masking it also keeps the index inside the register array, which
  // REG 4 and above would otherwise run past the end of.
  ReadRegisterOperandForRegisterIndex(
      ctx, (RegisterIndex)(kES + (ctx->instruction->mod_rm.reg & 0x03)),
      operand);
}

// Write a value to a register or memory operand address.
YAX86_MODULE_PRIVATE YAX86_HOT void WriteOperandAddress(
    const InstructionContext* ctx, const OperandAddress* address,
    uint32_t raw_value) {
  const Width width = ctx->metadata->width;
  const OperandValue value = ToOperandValue(width, raw_value);
  // See ReadOperandValue() for why this one is not a switch.
  if (address->type == kOperandAddressTypeRegister) {
    WriteRegisterOperand(ctx->cpu, address, value, width);
  } else {
    WriteMemoryOperand(ctx->cpu, address, value, width);
  }
}

// Write a value to a register or memory operand.
YAX86_MODULE_PRIVATE void WriteOperand(
    const InstructionContext* ctx, const Operand* operand, uint32_t raw_value) {
  WriteOperandAddress(ctx, &operand->address, raw_value);
}

// Read an immediate value from the instruction.
YAX86_MODULE_PRIVATE OperandValue ReadImmediate(const InstructionContext* ctx) {
  return ReadImmediateOperand(ctx->instruction, ctx->metadata->width);
}
