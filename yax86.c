#include "yax86.h"

// ============================================================================
// Types and helpers
// ============================================================================

// The address of a register operand.
typedef struct {
  // Register index.
  RegisterIndex register_index;
  // Byte offset within the register; only relevant for byte-sized operands.
  // 0 for low byte (AL, CL, DL, BL), 8 for high byte (AH, CH, DH, BH).
  uint8_t byte_offset;
} RegisterAddress;

// The address of a memory operand.
typedef struct {
  // Segment register.
  RegisterIndex segment_register_index;
  // Effective address offset.
  uint16_t offset;
} MemoryAddress;

// Whether the operand is a register or memory operand.
typedef enum {
  kOperandAddressTypeRegister = 0,
  kOperandAddressTypeMemory
} OperandAddressType;

// Number of operand address types.
#define kNumOperandAddressTypes (kOperandAddressTypeMemory + 1)

// Operand address.
typedef struct {
  // Type of operand (register or memory).
  OperandAddressType type;
  // Address of the operand.
  union {
    RegisterAddress register_address;  // For register operands
    MemoryAddress memory_address;      // For memory operands
  } value;
} OperandAddress;

// Data width.
typedef enum {
  kByte = 0,  // 8-bit operand
  kWord       // 16-bit operand
} Width;

// Number of data width types.
#define kNumWidths (kWord + 1)

// Operand value.
typedef struct {
  // Data width.
  Width width;
  // The value of the operand.
  union {
    uint8_t byte_value;   // For byte operands
    uint16_t word_value;  // For word operands
  } value;
} OperandValue;

// Helper functions to construct OperandValue.
static inline OperandValue ByteValue(uint8_t byte_value) {
  OperandValue value = {
      .width = kByte,
      .value = {.byte_value = byte_value},
  };
  return value;
}

// Helper function to construct OperandValue for a word.
static inline OperandValue WordValue(uint16_t word_value) {
  OperandValue value = {
      .width = kWord,
      .value = {.word_value = word_value},
  };
  return value;
}

// Helper function to construct OperandValue given a Width and a value.
static inline OperandValue ToOperandValue(Width width, uint32_t raw_value) {
  switch (width) {
    case kByte:
      return ByteValue(raw_value & 0xFF);
    case kWord:
      return WordValue(raw_value & 0xFFFF);
  }
  // Should never reach here, but return a default value to avoid warnings.
  OperandValue value = {
      .width = kByte,
      .value = {.byte_value = 0xFF},
  };
  return value;
}

// Helper function to zero-extend OperandValue to a 32-bit value. This makes it
// simpler to do direct arithmetic without worrying about overflow.
static inline uint32_t FromOperandValue(const OperandValue* value) {
  switch (value->width) {
    case kByte:
      return value->value.byte_value;
    case kWord:
      return value->value.word_value;
  }
  // Should never reach here, but return a default value to avoid warnings.
  return 0xFFFF;
}

// An operand.
typedef struct {
  // Address of the operand.
  OperandAddress address;
  // Value of the operand.
  OperandValue value;
} Operand;

// Helper function to extract a 16-bit value from an operand.
static inline uint32_t FromOperand(const Operand* operand) {
  return FromOperandValue(&operand->value);
}

// Computes the raw effective address corresponding to a MemoryAddress.
static inline uint16_t ToPhysicalAddress(
    const CPUState* cpu, const MemoryAddress* address) {
  uint16_t segment = cpu->registers[address->segment_register_index];
  return (segment << 4) + address->offset;
}

// Read a byte from memory.
static OperandValue ReadMemoryByte(
    CPUState* cpu, const OperandAddress* address) {
  uint8_t byte_value = cpu->config->read_memory_byte(
      cpu->config->context,
      ToPhysicalAddress(cpu, &address->value.memory_address));
  return ByteValue(byte_value);
}

// Read a word from memory.
static OperandValue ReadMemoryWord(
    CPUState* cpu, const OperandAddress* address) {
  const uint16_t physical_address =
      ToPhysicalAddress(cpu, &address->value.memory_address);
  uint8_t low_byte_value =
      cpu->config->read_memory_byte(cpu->config->context, physical_address);
  uint8_t high_byte_value =
      cpu->config->read_memory_byte(cpu->config->context, physical_address + 1);
  uint16_t word_value = (high_byte_value << 8) | low_byte_value;
  return WordValue(word_value);
}

// Read a byte from a register.
static OperandValue ReadRegisterByte(
    CPUState* cpu, const OperandAddress* address) {
  const RegisterAddress* register_address = &address->value.register_address;
  uint8_t byte_value = cpu->registers[register_address->register_index] >>
                       register_address->byte_offset;
  return ByteValue(byte_value);
}

// Read a word from a register.
static OperandValue ReadRegisterWord(
    CPUState* cpu, const OperandAddress* address) {
  const RegisterAddress* register_address = &address->value.register_address;
  uint16_t word_value = cpu->registers[register_address->register_index];
  return WordValue(word_value);
}

// Table of Read* functions, indexed by OperandAddressType and Width.
static OperandValue (*const kReadOperandValueFn[kNumOperandAddressTypes]
                                               [kNumWidths])(
    CPUState* cpu, const OperandAddress* address) = {
    // kOperandTypeRegister
    {ReadRegisterByte, ReadRegisterWord},
    // kOperandTypeMemory
    {ReadMemoryByte, ReadMemoryWord},
};

// Write a byte to memory.
static void WriteMemoryByte(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  const MemoryAddress* memory_address = &address->value.memory_address;
  uint16_t segment = cpu->registers[memory_address->segment_register_index];
  cpu->config->write_memory_byte(
      cpu->config->context, (segment << 4) + memory_address->offset,
      value.value.byte_value);
}

// Write a word to memory.
static void WriteMemoryWord(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  const MemoryAddress* memory_address = &address->value.memory_address;
  uint16_t segment = cpu->registers[memory_address->segment_register_index];
  uint16_t address_value = (segment << 4) + memory_address->offset;
  uint16_t word_value = value.value.word_value;
  uint8_t low_byte_value = word_value & 0xFF;
  uint8_t high_byte_value = (word_value >> 8) & 0xFF;
  cpu->config->write_memory_byte(
      cpu->config->context, address_value, low_byte_value);
  cpu->config->write_memory_byte(
      cpu->config->context, address_value + 1, high_byte_value);
}
// Write a byte to a register.
static void WriteRegisterByte(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  const RegisterAddress* register_address = &address->value.register_address;
  const uint16_t updated_byte = value.value.byte_value
                                << register_address->byte_offset;
  const uint16_t other_byte = cpu->registers[register_address->register_index] &
                              (0xFF << (8 - register_address->byte_offset));
  cpu->registers[register_address->register_index] = other_byte | updated_byte;
}

// Write a word to a register.
static void WriteRegisterWord(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  const RegisterAddress* register_address = &address->value.register_address;
  cpu->registers[register_address->register_index] = value.value.word_value;
}

// Table of Write* functions, indexed by OperandAddressType and Width.
static void (*const kWriteOperandFn[kNumOperandAddressTypes][kNumWidths])(
    CPUState* cpu, const OperandAddress* address, OperandValue value) = {
    // kOperandTypeRegister
    {WriteRegisterByte, WriteRegisterWord},
    // kOperandTypeMemory
    {WriteMemoryByte, WriteMemoryWord},
};

// Bitmask to extract the sign bit of a value.
static const uint32_t kSignBit[kNumWidths] = {
    1 << 7,   // kByte
    1 << 15,  // kWord
};

// Maximum value of each data width.
static const uint32_t kMaxValue[kNumWidths] = {
    0xFF,   // kByte
    0xFFFF  // kWord
};

// Add an 8-bit signed relative offset to a 16-bit unsigned base address.
static inline uint16_t AddSignedOffsetByte(uint16_t base, uint8_t raw_offset) {
  // Sign-extend the offset to 32 bits
  int32_t signed_offset = (int32_t)((int8_t)raw_offset);
  // Zero-extend base to 32 bits
  int32_t signed_base = (int32_t)base;
  // Add the two 32-bit signed values then truncate back down to 16-bit unsigned
  return (uint16_t)(signed_base + signed_offset);
}

// Add a 16-bit signed relative offset to a 16-bit unsigned base address.
static inline uint16_t AddSignedOffsetWord(uint16_t base, uint16_t raw_offset) {
  // Sign-extend the offset to 32 bits
  int32_t signed_offset = (int32_t)((int16_t)raw_offset);
  // Zero-extend base to 32 bits
  int32_t signed_base = (int32_t)base;
  // Add the two 32-bit signed values then truncate back down to 16-bit unsigned
  return (uint16_t)(signed_base + signed_offset);
}

// Get the register operand for a byte instruction based on the ModR/M byte's
// reg or R/M field.
static inline RegisterAddress GetRegisterAddressByte(
    CPUState* cpu, uint8_t reg_or_rm) {
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
static inline RegisterAddress GetRegisterAddressWord(
    CPUState* cpu, uint8_t reg_or_rm) {
  const RegisterAddress address = {
      .register_index = reg_or_rm, .byte_offset = 0};
  return address;
}

// Table of GetRegisterAddress functions, indexed by Width.
static RegisterAddress (*const kGetRegisterAddressFn[kNumWidths])(
    CPUState* cpu, uint8_t reg_or_rm) = {
    GetRegisterAddressByte,  // kByte
    GetRegisterAddressWord   // kWord
};

// Compute the memory address for an instruction.
static inline MemoryAddress GetMemoryOperandAddress(
    CPUState* cpu, const EncodedInstruction* instruction) {
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

  // Apply segment override if present
  for (int i = 0; i < instruction->prefix_size; ++i) {
    switch (instruction->prefix[i]) {
      case 0x26:  // ES
        address.segment_register_index = kES;
        break;
      case 0x2E:  // CS
        address.segment_register_index = kCS;
        break;
      case 0x36:  // SS
        address.segment_register_index = kSS;
        break;
      case 0x3E:  // DS
        address.segment_register_index = kDS;
        break;
      default:
        // Ignore other prefixes
        break;
    }
  }

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
static inline OperandAddress GetRegisterOrMemoryOperandAddress(
    CPUState* cpu, const EncodedInstruction* instruction, Width width) {
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
static inline OperandValue ReadImmediateByte(
    const EncodedInstruction* instruction) {
  return ByteValue(instruction->immediate[0]);
}

// Read a 16-bit immediate value.
static inline OperandValue ReadImmediateWord(
    const EncodedInstruction* instruction) {
  return WordValue(
      ((uint16_t)instruction->immediate[0]) |
      (((uint16_t)instruction->immediate[1]) << 8));
}

// Table of ReadImmediate* functions, indexed by Width.
static OperandValue (*const kReadImmediateValueFn[kNumWidths])(
    const EncodedInstruction* instruction) = {
    ReadImmediateByte,  // kByte
    ReadImmediateWord   // kWord
};

struct OpcodeMetadata;

// Instruction execution context.
typedef struct {
  CPUState* cpu;
  const EncodedInstruction* instruction;
  const struct OpcodeMetadata* metadata;
} InstructionContext;

typedef ExecuteInstructionStatus (*OpcodeHandler)(
    const InstructionContext* context);

// Opcode lookup table entry.
typedef struct OpcodeMetadata {
  // Opcode.
  uint8_t opcode;

  // Instruction has ModR/M byte
  bool has_modrm : 1;
  // Number of immediate data bytes: 0, 1, 2, or 4
  uint8_t immediate_size : 3;

  // Width of the instruction's operands. Passed to handler.
  Width width : 1;

  // Handler function.
  OpcodeHandler handler;
} OpcodeMetadata;

// Get a register or memory operand for an instruction based on the ModR/M
// byte and displacement.
static inline Operand ReadRegisterOrMemoryOperand(
    const InstructionContext* ctx) {
  Width width = ctx->metadata->width;
  Operand operand;
  operand.address =
      GetRegisterOrMemoryOperandAddress(ctx->cpu, ctx->instruction, width);
  operand.value = kReadOperandValueFn[operand.address.type][width](
      ctx->cpu, &operand.address);
  return operand;
}

// Get a register operand for an instruction.
static inline Operand ReadRegisterOperandForRegisterIndex(
    const InstructionContext* ctx, RegisterIndex register_index) {
  Width width = ctx->metadata->width;
  Operand operand;
  operand.address.type = kOperandAddressTypeRegister;
  operand.address.value.register_address =
      kGetRegisterAddressFn[width](ctx->cpu, register_index);
  operand.value = kReadOperandValueFn[kOperandAddressTypeRegister][width](
      ctx->cpu, &operand.address);
  return operand;
}

// Get a register operand for an instruction from the REG field of the Mod/RM
// byte.
static inline Operand ReadRegisterOperand(const InstructionContext* ctx) {
  return ReadRegisterOperandForRegisterIndex(ctx, ctx->instruction->mod_rm.reg);
}

// Get a segment register operand for an instruction from the REG field of the
// Mod/RM byte.
static inline Operand ReadSegmentRegisterOperand(
    const InstructionContext* ctx) {
  return ReadRegisterOperandForRegisterIndex(
      ctx, ctx->instruction->mod_rm.reg + 8);
}

// Write a value to a register or memory operand address.
static inline void WriteOperandAddress(
    const InstructionContext* ctx, const OperandAddress* address,
    uint32_t raw_value) {
  Width width = ctx->metadata->width;
  kWriteOperandFn[address->type][width](
      ctx->cpu, address, ToOperandValue(width, raw_value));
}

// Write a value to a register or memory operand.
static inline void WriteOperand(
    const InstructionContext* ctx, const Operand* operand, uint32_t raw_value) {
  WriteOperandAddress(ctx, &operand->address, raw_value);
}

// Read an immediate value from the instruction.
static inline OperandValue ReadImmediate(const InstructionContext* ctx) {
  Width width = ctx->metadata->width;
  return kReadImmediateValueFn[width](ctx->instruction);
}

// Set common CPU flags after an instruction. This includes:
// - Zero flag (ZF)
// - Sign flag (SF)
// - Parity Flag (PF)
static inline void SetCommonFlagsAfterInstruction(
    const InstructionContext* ctx, uint32_t result) {
  Width width = ctx->metadata->width;
  result &= kMaxValue[width];
  // Zero flag (ZF)
  SetFlag(ctx->cpu, kZF, result == 0);
  // Sign flag (SF)
  SetFlag(ctx->cpu, kSF, result & kSignBit[width]);
  // Parity flag (PF)
  // Set if the number of set bits in the least significant byte is even
  uint8_t parity = result & 0xFF;  // Check only the low byte for parity
  parity ^= parity >> 4;
  parity ^= parity >> 2;
  parity ^= parity >> 1;
  SetFlag(ctx->cpu, kPF, (parity & 1) == 0);
}

// ============================================================================
// MOV instructions
// ============================================================================

// MOV r/m8, r8
// MOV r/m16, r16
static ExecuteInstructionStatus ExecuteMoveRegisterToRegisterOrMemory(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  WriteOperand(ctx, &dest, FromOperand(&src));
  return kExecuteSuccess;
}

// MOV r8, r/m8
// MOV r16, r/m16
static ExecuteInstructionStatus ExecuteMoveRegisterOrMemoryToRegister(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  WriteOperand(ctx, &dest, FromOperand(&src));
  return kExecuteSuccess;
}

// MOV r/m16, sreg
static ExecuteInstructionStatus ExecuteMoveSegmentRegisterToRegisterOrMemory(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadSegmentRegisterOperand(ctx);
  WriteOperand(ctx, &dest, FromOperand(&src));
  return kExecuteSuccess;
}

// MOV sreg, r/m16
static ExecuteInstructionStatus ExecuteMoveRegisterOrMemoryToSegmentRegister(
    const InstructionContext* ctx) {
  Operand dest = ReadSegmentRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  WriteOperand(ctx, &dest, FromOperand(&src));
  return kExecuteSuccess;
}

// MOV AX/CX/DX/BX/SP/BP/SI/DI, imm16
// MOV AH/AL/CH/CL/DH/DL/BH/BL, imm8
static ExecuteInstructionStatus ExecuteMoveImmediateToRegister(
    const InstructionContext* ctx) {
  static const uint8_t register_index_opcode_base[kNumWidths] = {
      0xB0,  // kByte
      0xB8,  // kWord
  };
  RegisterIndex register_index =
      ctx->instruction->opcode -
      register_index_opcode_base[ctx->metadata->width];
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  OperandValue src_value = ReadImmediate(ctx);
  WriteOperand(ctx, &dest, FromOperandValue(&src_value));
  return kExecuteSuccess;
}

// MOV AL, moffs16
// MOV AX, moffs16
static ExecuteInstructionStatus ExecuteMoveMemoryOffsetToALOrAX(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  // Offset is always 16 bits, even though the data width of the operation may
  // be 8 bits.
  OperandValue src_offset_value =
      kReadImmediateValueFn[kWord](ctx->instruction);
  OperandAddress src_address = {
      .type = kOperandAddressTypeMemory,
      .value = {
          .memory_address.segment_register_index = kDS,
          .memory_address.offset = FromOperandValue(&src_offset_value),
      }};
  OperandValue src_value =
      kReadOperandValueFn[kOperandAddressTypeMemory][ctx->metadata->width](
          ctx->cpu, &src_address);
  WriteOperand(ctx, &dest, FromOperandValue(&src_value));
  return kExecuteSuccess;
}

// MOV moffs16, AL
// MOV moffs16, AX
static ExecuteInstructionStatus ExecuteMoveALOrAXToMemoryOffset(
    const InstructionContext* ctx) {
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  // Offset is always 16 bits, even though the data width of the operation may
  // be 8 bits.
  OperandValue dest_offset_value =
      kReadImmediateValueFn[kWord](ctx->instruction);
  OperandAddress dest_address = {
      .type = kOperandAddressTypeMemory,
      .value = {
          .memory_address = {
              .segment_register_index = kDS,
              .offset = FromOperandValue(&dest_offset_value),
          }}};
  WriteOperandAddress(ctx, &dest_address, FromOperand(&src));
  return kExecuteSuccess;
}

// MOV r/m8, imm8
// MOV r/m16, imm16
static ExecuteInstructionStatus ExecuteMoveImmediateToRegisterOrMemory(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  OperandValue src_value = ReadImmediate(ctx);
  WriteOperand(ctx, &dest, FromOperandValue(&src_value));
  return kExecuteSuccess;
}

// ============================================================================
// XCHG instructions
// ============================================================================

// XCHG AX, AX/CX/DX/BX/SP/BP/SI/DI
static ExecuteInstructionStatus ExecuteExchangeRegister(
    const InstructionContext* ctx) {
  RegisterIndex register_index = ctx->instruction->opcode - 0x90;
  if (register_index == kAX) {
    // No-op
    return kExecuteSuccess;
  }
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  uint32_t temp = FromOperand(&dest);
  WriteOperand(ctx, &dest, FromOperand(&src));
  WriteOperand(ctx, &src, temp);
  return kExecuteSuccess;
}

// XCHG r/m8, r8
// XCHG r/m16, r16
static ExecuteInstructionStatus ExecuteExchangeRegisterOrMemory(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  uint32_t temp = FromOperand(&dest);
  WriteOperand(ctx, &dest, FromOperand(&src));
  WriteOperand(ctx, &src, temp);
  return kExecuteSuccess;
}

// ============================================================================
// LEA instruction
// ============================================================================

// LEA r16, m
static ExecuteInstructionStatus ExecuteLoadEffectiveAddress(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  MemoryAddress memory_address =
      GetMemoryOperandAddress(ctx->cpu, ctx->instruction);
  uint32_t physical_address = ToPhysicalAddress(ctx->cpu, &memory_address);
  WriteOperandAddress(ctx, &dest.address, physical_address);
  return kExecuteSuccess;
}

// ============================================================================
// PUSH and POP instructions
// ============================================================================

static inline void Push(const InstructionContext* ctx, OperandValue value) {
  ctx->cpu->registers[kSP] -= 2;
  OperandAddress address = {
      .type = kOperandAddressTypeMemory,
      .value.memory_address = {
          .segment_register_index = kSS,
          .offset = ctx->cpu->registers[kSP],
      }};
  WriteMemoryWord(ctx->cpu, &address, value);
}

static inline OperandValue Pop(const InstructionContext* ctx) {
  OperandAddress address = {
      .type = kOperandAddressTypeMemory,
      .value.memory_address = {
          .segment_register_index = kSS,
          .offset = ctx->cpu->registers[kSP],
      }};
  OperandValue value = ReadMemoryWord(ctx->cpu, &address);
  ctx->cpu->registers[kSP] += 2;
  return value;
}

// PUSH AX/CX/DX/BX/SP/BP/SI/DI
static ExecuteInstructionStatus ExecutePushRegister(
    const InstructionContext* ctx) {
  RegisterIndex register_index = ctx->instruction->opcode - 0x50;
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  Push(ctx, src.value);
  return kExecuteSuccess;
}

// POP AX/CX/DX/BX/SP/BP/SI/DI
static ExecuteInstructionStatus ExecutePopRegister(
    const InstructionContext* ctx) {
  RegisterIndex register_index = ctx->instruction->opcode - 0x58;
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  OperandValue value = Pop(ctx);
  WriteOperandAddress(ctx, &dest.address, FromOperandValue(&value));
  return kExecuteSuccess;
}

// PUSH ES/CS/SS/DS
static ExecuteInstructionStatus ExecutePushSegmentRegister(
    const InstructionContext* ctx) {
  RegisterIndex register_index = ((ctx->instruction->opcode >> 3) & 0x03) + 8;
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  Push(ctx, src.value);
  return kExecuteSuccess;
}

// POP ES/CS/SS/DS
static ExecuteInstructionStatus ExecutePopSegmentRegister(
    const InstructionContext* ctx) {
  RegisterIndex register_index = ((ctx->instruction->opcode >> 3) & 0x03) + 8;
  // Special case - disallow POP CS
  if (register_index == kCS) {
    return kExecuteInvalidInstruction;
  }
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  OperandValue value = Pop(ctx);
  WriteOperandAddress(ctx, &dest.address, FromOperandValue(&value));
  return kExecuteSuccess;
}

// PUSHF
static ExecuteInstructionStatus ExecutePushFlags(
    const InstructionContext* ctx) {
  Push(ctx, WordValue(ctx->cpu->flags));
  return kExecuteSuccess;
}

// POPF
static ExecuteInstructionStatus ExecutePopFlags(const InstructionContext* ctx) {
  OperandValue value = Pop(ctx);
  ctx->cpu->flags = FromOperandValue(&value);
  return kExecuteSuccess;
}

// ============================================================================
// LAHF and SAHF
// ============================================================================

// Returns the AH register address.
static inline const OperandAddress* GetAHRegisterAddress() {
  static OperandAddress ah = {
      .type = kOperandAddressTypeRegister,
      .value = {
          .register_address = {
              .register_index = kAX,
              .byte_offset = 8,
          }}};
  return &ah;
}

// LAHF
static ExecuteInstructionStatus ExecuteLoadAHFromFlags(
    const InstructionContext* ctx) {
  WriteRegisterByte(
      ctx->cpu, GetAHRegisterAddress(), ByteValue(ctx->cpu->flags & 0x00FF));
  return kExecuteSuccess;
}

// SAHF
static ExecuteInstructionStatus ExecuteStoreAHToFlags(
    const InstructionContext* ctx) {
  OperandValue value = ReadRegisterByte(ctx->cpu, GetAHRegisterAddress());
  // Clear the lower byte of flags and set it to the value in AH
  ctx->cpu->flags = (ctx->cpu->flags & 0xFF00) | value.value.byte_value;
  return kExecuteSuccess;
}

// ============================================================================
// ADD, ADC, and INC instructions
// ============================================================================

// Set CPU flags after an INC instruction.
// Other than common flags, the INC instruction sets the following flags:
// - Overflow Flag (OF) - Set when result has wrong sign
// - Auxiliary Carry Flag (AF) - carry from bit 3 to bit 4
static void SetFlagsAfterInc(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_carry) {
  SetCommonFlagsAfterInstruction(ctx, result);

  // Overflow Flag (OF) Set when result has wrong sign (both operands have same
  // sign but result has different sign)
  uint32_t sign_bit = kSignBit[ctx->metadata->width];
  bool op1_sign = (op1 & sign_bit) != 0;
  bool op2_sign = (op2 & sign_bit) != 0;
  bool result_sign = (result & sign_bit) != 0;
  SetFlag(ctx->cpu, kOF, (op1_sign == op2_sign) && (result_sign != op1_sign));

  // Auxiliary Carry Flag (AF) - carry from bit 3 to bit 4
  SetFlag(
      ctx->cpu, kAF, ((op1 & 0xF) + (op2 & 0xF) + (did_carry ? 1 : 0)) > 0xF);
}

// Set CPU flags after an ADD or ADC instruction.
// Other than the flags set by the INC instruction, the ADD instruction sets the
// following flags:
// - Carry Flag (CF) - Set when result overflows the maximum width
static void SetFlagsAfterAdd(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_carry) {
  SetFlagsAfterInc(ctx, op1, op2, result, did_carry);
  // Carry Flag (CF)
  SetFlag(ctx->cpu, kCF, result > kMaxValue[ctx->metadata->width]);
}

// Common signature of SetFlagsAfterAdd and SetFlagsAfterInc.
typedef void (*SetFlagsAfterAddFn)(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_carry);

// Common logic for ADD, ADC, and INC instructions.
static inline ExecuteInstructionStatus ExecuteAddCommon(
    const InstructionContext* ctx, Operand* dest, const OperandValue* src_value,
    bool carry, SetFlagsAfterAddFn set_flags_after_fn) {
  uint32_t raw_dest_value = FromOperand(dest);
  uint32_t raw_src_value = FromOperandValue(src_value);
  bool should_carry = carry && GetFlag(ctx->cpu, kCF);
  uint32_t result = raw_dest_value + raw_src_value + (should_carry ? 1 : 0);
  WriteOperand(ctx, dest, result);
  (*set_flags_after_fn)(
      ctx, raw_dest_value, raw_src_value, result, should_carry);
  return kExecuteSuccess;
}

// Common logic for ADD instructions
static inline ExecuteInstructionStatus ExecuteAdd(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  return ExecuteAddCommon(
      ctx, dest, src_value, /* carry */ false, SetFlagsAfterAdd);
}

// ADD r/m8, r8
// ADD r/m16, r16
static ExecuteInstructionStatus ExecuteAddRegisterToRegisterOrMemory(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteAdd(ctx, &dest, &src.value);
}

// ADD r8, r/m8
// ADD r16, r/m16
static ExecuteInstructionStatus ExecuteAddRegisterOrMemoryToRegister(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteAdd(ctx, &dest, &src.value);
}

// ADD AL, imm8
// ADD AX, imm16
static ExecuteInstructionStatus ExecuteAddImmediateToALOrAX(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteAdd(ctx, &dest, &src_value);
}

// Common logic for ADC instructions
static inline ExecuteInstructionStatus ExecuteAddWithCarry(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  return ExecuteAddCommon(
      ctx, dest, src_value, /* carry */ true, SetFlagsAfterAdd);
}

// ADC r/m8, r8
// ADC r/m16, r16
static ExecuteInstructionStatus ExecuteAddRegisterToRegisterOrMemoryWithCarry(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteAddWithCarry(ctx, &dest, &src.value);
}
// ADC r8, r/m8
// ADC r16, r/m16
static ExecuteInstructionStatus ExecuteAddRegisterOrMemoryToRegisterWithCarry(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteAddWithCarry(ctx, &dest, &src.value);
}

// ADC AL, imm8
// ADC AX, imm16
static ExecuteInstructionStatus ExecuteAddImmediateToALOrAXWithCarry(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteAddWithCarry(ctx, &dest, &src_value);
}

// Common logic for INC instructions
static inline ExecuteInstructionStatus ExecuteInc(
    const InstructionContext* ctx, Operand* dest) {
  OperandValue src_value = WordValue(1);
  return ExecuteAddCommon(
      ctx, dest, &src_value, /* carry */ false, SetFlagsAfterInc);
}

// INC AX/CX/DX/BX/SP/BP/SI/DI
static ExecuteInstructionStatus ExecuteIncRegister(
    const InstructionContext* ctx) {
  RegisterIndex register_index = ctx->instruction->opcode - 0x40;
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  return ExecuteInc(ctx, &dest);
}

// ============================================================================
// SUB, SBB, and DEC instructions
// ============================================================================

// Set CPU flags after a DEC or SUB/SBB operation (base function).
// This function sets ZF, SF, PF, OF, AF. It does NOT affect CF.
// - OF is for the full operation op1 - (op2 + did_borrow).
// - AF is for the full operation op1 - (op2 + did_borrow).
static void SetFlagsAfterDec(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_borrow) {
  SetCommonFlagsAfterInstruction(ctx, result);

  uint32_t sign_bit = kSignBit[ctx->metadata->width];
  uint32_t max_val = kMaxValue[ctx->metadata->width];

  // Overflow Flag (OF)
  // OF is set if op1_sign != val_being_subtracted_sign AND result_sign ==
  // val_being_subtracted_sign where val_being_subtracted = (op2 + did_borrow)
  bool op1_sign = (op1 & sign_bit) != 0;
  bool result_sign = (result & sign_bit) != 0;
  uint32_t val_being_subtracted = (op2 & max_val) + (did_borrow ? 1 : 0);
  // Mask val_being_subtracted to current width before checking its sign,
  // in case (op2 & max_val) + 1 caused a temporary overflow beyond max_val if
  // op2 was max_val.
  bool val_being_subtracted_sign =
      ((val_being_subtracted & max_val) & sign_bit) != 0;
  SetFlag(
      ctx->cpu, kOF,
      (op1_sign != val_being_subtracted_sign) &&
          (result_sign == val_being_subtracted_sign));

  // Auxiliary Carry Flag (AF) - borrow from bit 3 to bit 4
  SetFlag(ctx->cpu, kAF, (op1 & 0xF) < ((op2 & 0xF) + (did_borrow ? 1 : 0)));
}

// Set CPU flags after a SUB or SBB instruction.
// This calls SetFlagsAfterDec and then sets the Carry Flag (CF).
static void SetFlagsAfterSub(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_borrow) {
  SetFlagsAfterDec(ctx, op1, op2, result, did_borrow);

  // Carry Flag (CF) - Set when a borrow is generated
  // CF is set if op1 < (op2 + did_borrow) (unsigned comparison)
  uint32_t val_being_subtracted =
      (op2 & kMaxValue[ctx->metadata->width]) + (did_borrow ? 1 : 0);
  SetFlag(ctx->cpu, kCF, op1 < val_being_subtracted);
}

// Common signature of SetFlagsAfterSub and SetFlagsAfterDec.
typedef void (*SetFlagsAfterSubFn)(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_borrow);

// Common logic for SUB, SBB, and DEC instructions.
static inline ExecuteInstructionStatus ExecuteSubCommon(
    const InstructionContext* ctx, Operand* dest, const OperandValue* src_value,
    bool borrow, SetFlagsAfterSubFn set_flags_after_fn) {
  uint32_t raw_dest_value = FromOperand(dest);
  uint32_t raw_src_value = FromOperandValue(src_value);
  bool should_borrow = borrow && GetFlag(ctx->cpu, kCF);
  uint32_t result = raw_dest_value - raw_src_value - (should_borrow ? 1 : 0);
  WriteOperand(ctx, dest, result);
  (*set_flags_after_fn)(
      ctx, raw_dest_value, raw_src_value, result, should_borrow);
  return kExecuteSuccess;
}

// Common logic for SUB instructions
static inline ExecuteInstructionStatus ExecuteSub(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  return ExecuteSubCommon(
      ctx, dest, src_value, /* borrow */ false, SetFlagsAfterSub);
}

// SUB r/m8, r8
// SUB r/m16, r16
static ExecuteInstructionStatus ExecuteSubRegisterFromRegisterOrMemory(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteSub(ctx, &dest, &src.value);
}

// SUB r8, r/m8
// SUB r16, r/m16
static ExecuteInstructionStatus ExecuteSubRegisterOrMemoryFromRegister(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteSub(ctx, &dest, &src.value);
}

// SUB AL, imm8
// SUB AX, imm16
static ExecuteInstructionStatus ExecuteSubImmediateFromALOrAX(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteSub(ctx, &dest, &src_value);
}

// Common logic for SBB instructions
static inline ExecuteInstructionStatus ExecuteSubWithBorrow(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  return ExecuteSubCommon(
      ctx, dest, src_value, /* borrow */ true, SetFlagsAfterSub);
}

// SBB r/m8, r8
// SBB r/m16, r16
static ExecuteInstructionStatus
ExecuteSubRegisterFromRegisterOrMemoryWithBorrow(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteSubWithBorrow(ctx, &dest, &src.value);
}

// SBB r8, r/m8
// SBB r16, r/m16
static ExecuteInstructionStatus
ExecuteSubRegisterOrMemoryFromRegisterWithBorrow(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteSubWithBorrow(ctx, &dest, &src.value);
}

// SBB AL, imm8
// SBB AX, imm16
static ExecuteInstructionStatus ExecuteSubImmediateFromALOrAXWithBorrow(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteSubWithBorrow(ctx, &dest, &src_value);
}

// Common logic for DEC instructions
static inline ExecuteInstructionStatus ExecuteDec(
    const InstructionContext* ctx, Operand* dest) {
  OperandValue src_value = WordValue(1);
  return ExecuteSubCommon(
      ctx, dest, &src_value, /* borrow */ false, SetFlagsAfterDec);
}

// DEC AX/CX/DX/BX/SP/BP/SI/DI
static ExecuteInstructionStatus ExecuteDecRegister(
    const InstructionContext* ctx) {
  RegisterIndex register_index = ctx->instruction->opcode - 0x48;
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  return ExecuteDec(ctx, &dest);
}

// ============================================================================
// CMP instructions
// ============================================================================

// Common logic for CMP instructions.
static inline ExecuteInstructionStatus ExecuteCmp(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  uint32_t raw_dest_value = FromOperand(dest);
  uint32_t raw_src_value = FromOperandValue(src_value);
  uint32_t result = raw_dest_value - raw_src_value;
  SetFlagsAfterSub(ctx, raw_dest_value, raw_src_value, result, false);
  return kExecuteSuccess;
}

// CMP r/m8, r8
// CMP r/m16, r16
static ExecuteInstructionStatus ExecuteCmpRegisterToRegisterOrMemory(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteCmp(ctx, &dest, &src.value);
}

// CMP r8, r/m8
// CMP r16, r/m16
static ExecuteInstructionStatus ExecuteCmpRegisterOrMemoryToRegister(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteCmp(ctx, &dest, &src.value);
}

// CMP AL, imm8
// CMP AX, imm16
static ExecuteInstructionStatus ExecuteCmpImmediateToALOrAX(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteCmp(ctx, &dest, &src_value);
}

// ============================================================================
// Boolean AND, OR and XOR instructions
// ============================================================================

static inline void SetFlagsAfterBooleanInstruction(
    const InstructionContext* ctx, uint32_t result) {
  SetCommonFlagsAfterInstruction(ctx, result);
  // Carry Flag (CF) should be cleared
  SetFlag(ctx->cpu, kCF, false);
  // Overflow Flag (OF) should be cleared
  SetFlag(ctx->cpu, kOF, false);
}

// Common logic for AND instructions.
static inline ExecuteInstructionStatus ExecuteBooleanAnd(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  uint32_t result = FromOperand(dest) & FromOperandValue(src_value);
  WriteOperand(ctx, dest, result);
  SetFlagsAfterBooleanInstruction(ctx, result);
  return kExecuteSuccess;
}

// AND r/m8, r8
// AND r/m16, r16
static ExecuteInstructionStatus ExecuteBooleanAndRegisterToRegisterOrMemory(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteBooleanAnd(ctx, &dest, &src.value);
}

// AND r8, r/m8
// AND r16, r/m16
static ExecuteInstructionStatus ExecuteBooleanAndRegisterOrMemoryToRegister(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteBooleanAnd(ctx, &dest, &src.value);
}

// AND AL, imm8
// AND AX, imm16
static ExecuteInstructionStatus ExecuteBooleanAndImmediateToALOrAX(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteBooleanAnd(ctx, &dest, &src_value);
}

// Common logic for OR instructions.
static inline ExecuteInstructionStatus ExecuteBooleanOr(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  uint32_t result = FromOperand(dest) | FromOperandValue(src_value);
  WriteOperand(ctx, dest, result);
  SetFlagsAfterBooleanInstruction(ctx, result);
  return kExecuteSuccess;
}

// OR r/m8, r8
// OR r/m16, r16
static ExecuteInstructionStatus ExecuteBooleanOrRegisterToRegisterOrMemory(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteBooleanOr(ctx, &dest, &src.value);
}

// OR r8, r/m8
// OR r16, r/m16
static ExecuteInstructionStatus ExecuteBooleanOrRegisterOrMemoryToRegister(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteBooleanOr(ctx, &dest, &src.value);
}

// OR AL, imm8
// OR AX, imm16
static ExecuteInstructionStatus ExecuteBooleanOrImmediateToALOrAX(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteBooleanOr(ctx, &dest, &src_value);
}

// Common logic for XOR instructions.
static inline ExecuteInstructionStatus ExecuteBooleanXor(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  uint32_t result = FromOperand(dest) ^ FromOperandValue(src_value);
  WriteOperand(ctx, dest, result);
  SetFlagsAfterBooleanInstruction(ctx, result);
  return kExecuteSuccess;
}

// XOR r/m8, r8
// XOR r/m16, r16
static ExecuteInstructionStatus ExecuteBooleanXorRegisterToRegisterOrMemory(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteBooleanXor(ctx, &dest, &src.value);
}

// XOR r8, r/m8
// XOR r16, r/m16
static ExecuteInstructionStatus ExecuteBooleanXorRegisterOrMemoryToRegister(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteBooleanXor(ctx, &dest, &src.value);
}

// XOR AL, imm8
// XOR AX, imm16
static ExecuteInstructionStatus ExecuteBooleanXorImmediateToALOrAX(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteBooleanXor(ctx, &dest, &src_value);
}

// ============================================================================
// TEST instructions
// ============================================================================

// Common logic for TEST instructions.
static inline ExecuteInstructionStatus ExecuteTest(
    const InstructionContext* ctx, Operand* dest, OperandValue* src_value) {
  uint32_t result = FromOperand(dest) & FromOperandValue(src_value);
  SetFlagsAfterBooleanInstruction(ctx, result);
  return kExecuteSuccess;
}

// TEST r/m8, r8
// TEST r/m16, r16
static ExecuteInstructionStatus ExecuteTestRegisterToRegisterOrMemory(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteTest(ctx, &dest, &src.value);
}

// TEST AL, imm8
// TEST AX, imm16
static ExecuteInstructionStatus ExecuteTestImmediateToALOrAX(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteTest(ctx, &dest, &src_value);
}

// ============================================================================
// Conditional jumps
// ============================================================================

// Common logic for conditional jumps.
static inline ExecuteInstructionStatus ExecuteConditionalJump(
    const InstructionContext* ctx, bool value, bool success_value) {
  if (value == success_value) {
    OperandValue offset_value = ReadImmediate(ctx);
    ctx->cpu->registers[kIP] = AddSignedOffsetByte(
        ctx->cpu->registers[kIP], FromOperandValue(&offset_value));
  }
  return kExecuteSuccess;
}

// Table of flag register bitmasks for conditional jumps. The index corresponds
// to (opcode - 0x70) / 2.
static const uint16_t kUnsignedConditionalJumpFlagBitmasks[] = {
    kOF,        // 0x70 - JO, 0x71 - JNO
    kCF,        // 0x72 - JC, 0x73 - JNC
    kZF,        // 0x74 - JE, 0x75 - JNE
    kCF | kZF,  // 0x76 - JBE, 0x77 - JNBE
    kSF,        // 0x78 - JS, 0x79 - JNS
    kPF,        // 0x7A - JP, 0x7B - JNP
};

// Unsigned conditional jumps.
static ExecuteInstructionStatus ExecuteUnsignedConditionalJump(
    const InstructionContext* ctx) {
  uint16_t flag_mask = kUnsignedConditionalJumpFlagBitmasks
      [(ctx->instruction->opcode - 0x70) / 2];
  bool flag_value = (ctx->cpu->flags & flag_mask) != 0;
  // Even opcode => jump if the flag is set
  // Odd opcode => jump if the flag is not set
  bool success_value = ((ctx->instruction->opcode & 0x1) == 0);
  return ExecuteConditionalJump(ctx, flag_value, success_value);
}

// JL/JGNE and JNL/JGE
static ExecuteInstructionStatus ExecuteSignedConditionalJumpJLOrJNL(
    const InstructionContext* ctx) {
  const bool is_greater_or_equal =
      GetFlag(ctx->cpu, kSF) == GetFlag(ctx->cpu, kOF);
  const bool success_value = (ctx->instruction->opcode & 0x1);
  return ExecuteConditionalJump(ctx, is_greater_or_equal, success_value);
}

// JLE/JG and JNLE/JG
static ExecuteInstructionStatus ExecuteSignedConditionalJumpJLEOrJNLE(
    const InstructionContext* ctx) {
  const bool is_greater = !GetFlag(ctx->cpu, kZF) &&
                          (GetFlag(ctx->cpu, kSF) == GetFlag(ctx->cpu, kOF));
  const bool success_value = (ctx->instruction->opcode & 0x1);
  return ExecuteConditionalJump(ctx, is_greater, success_value);
}

// ============================================================================
// Loop instructions
// ============================================================================

// LOOP rel8
static ExecuteInstructionStatus ExecuteLoop(const InstructionContext* ctx) {
  return ExecuteConditionalJump(ctx, --(ctx->cpu->registers[kCX]) != 0, true);
}

// LOOPZ rel8
// LOOPNZ rel8
static ExecuteInstructionStatus ExecuteLoopZOrNZ(
    const InstructionContext* ctx) {
  bool condition1 = --(ctx->cpu->registers[kCX]) != 0;
  bool condition2 =
      GetFlag(ctx->cpu, kZF) == (bool)(ctx->instruction->opcode - 0xE0);
  return ExecuteConditionalJump(ctx, condition1 && condition2, true);
}

// JCXZ rel8
static ExecuteInstructionStatus ExecuteJumpIfCXIsZero(
    const InstructionContext* ctx) {
  return ExecuteConditionalJump(ctx, ctx->cpu->registers[kCX] == 0, true);
}

// ============================================================================
// CALL and RET instructions
// ============================================================================

// CALL rel16
static ExecuteInstructionStatus ExecuteDirectNearCall(
    const InstructionContext* ctx) {
  OperandValue offset = ReadImmediate(ctx);
  Push(ctx, WordValue(ctx->cpu->registers[kIP]));
  ctx->cpu->registers[kIP] =
      AddSignedOffsetWord(ctx->cpu->registers[kIP], FromOperandValue(&offset));
  return kExecuteSuccess;
}

// CALL ptr16:16
static ExecuteInstructionStatus ExecuteDirectFarCall(
    const InstructionContext* ctx) {
  Push(ctx, WordValue(ctx->cpu->registers[kCS]));
  Push(ctx, WordValue(ctx->cpu->registers[kIP]));
  OperandValue new_cs = WordValue(
      ((uint16_t)ctx->instruction->immediate[2]) |
      (((uint16_t)ctx->instruction->immediate[3]) << 8));
  ctx->cpu->registers[kCS] = FromOperandValue(&new_cs);
  OperandValue new_ip = WordValue(
      ((uint16_t)ctx->instruction->immediate[0]) |
      (((uint16_t)ctx->instruction->immediate[1]) << 8));
  ctx->cpu->registers[kIP] = FromOperandValue(&new_ip);
  return kExecuteSuccess;
}

// Common logic for RET instructions.
static ExecuteInstructionStatus ExecuteNearReturnCommon(
    const InstructionContext* ctx, uint16_t arg_size) {
  OperandValue new_ip = Pop(ctx);
  ctx->cpu->registers[kIP] = FromOperandValue(&new_ip);
  ctx->cpu->registers[kSP] += arg_size;
  return kExecuteSuccess;
}

// RET
static ExecuteInstructionStatus ExecuteNearReturn(
    const InstructionContext* ctx) {
  return ExecuteNearReturnCommon(ctx, 0);
}

// RET imm16
static ExecuteInstructionStatus ExecuteNearReturnAndPop(
    const InstructionContext* ctx) {
  OperandValue arg_size_value = ReadImmediate(ctx);
  return ExecuteNearReturnCommon(ctx, FromOperandValue(&arg_size_value));
}

// Common logic for RETF instructions.
static ExecuteInstructionStatus ExecuteFarReturnCommon(
    const InstructionContext* ctx, uint16_t arg_size) {
  OperandValue new_ip = Pop(ctx);
  OperandValue new_cs = Pop(ctx);
  ctx->cpu->registers[kIP] = FromOperandValue(&new_ip);
  ctx->cpu->registers[kCS] = FromOperandValue(&new_cs);
  ctx->cpu->registers[kSP] += arg_size;
  return kExecuteSuccess;
}

// RETF
static ExecuteInstructionStatus ExecuteFarReturn(
    const InstructionContext* ctx) {
  return ExecuteFarReturnCommon(ctx, 0);
}

// RETF imm16
static ExecuteInstructionStatus ExecuteFarReturnAndPop(
    const InstructionContext* ctx) {
  OperandValue arg_size_value = ReadImmediate(ctx);
  return ExecuteFarReturnCommon(ctx, FromOperandValue(&arg_size_value));
}

// ============================================================================
// Group 1 - ADD, OR, ADC, SBB, AND, SUB, XOR, CMP
// ============================================================================

typedef ExecuteInstructionStatus (*Group1ExecuteInstructionFn)(
    const InstructionContext* ctx, Operand* dest, const OperandValue* src);

// Group 1 instruction implementations, indexed by the corresponding REG field
// value in the ModRM byte.
static const Group1ExecuteInstructionFn kGroup1ExecuteInstructionFns[] = {
    ExecuteAdd,            // 0 - ADD
    ExecuteBooleanOr,      // 1 - OR
    ExecuteAddWithCarry,   // 2 - ADC
    ExecuteSubWithBorrow,  // 3 - SBB
    ExecuteBooleanAnd,     // 4 - AND
    ExecuteSub,            // 5 - SUB
    ExecuteBooleanXor,     // 6 - XOR
    ExecuteCmp,            // 7 - CMP
};

// Group 1 instruction handler.
static ExecuteInstructionStatus ExecuteGroup1Instruction(
    const InstructionContext* ctx) {
  const Group1ExecuteInstructionFn fn =
      kGroup1ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  OperandValue src_value = ReadImmediate(ctx);
  return fn(ctx, &dest, &src_value);
}

// Group 1 instruction handler, but sign-extends the 8-bit immediate value.
static ExecuteInstructionStatus ExecuteGroup1InstructionWithSignExtension(
    const InstructionContext* ctx) {
  const Group1ExecuteInstructionFn fn =
      kGroup1ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  OperandValue src_value =
      ReadImmediateByte(ctx->instruction);  // immediate is always 8-bit
  OperandValue src_value_extended =
      WordValue((uint16_t)((int16_t)((int8_t)src_value.value.byte_value)));
  // Sign-extend the immediate value to the destination width.
  return fn(ctx, &dest, &src_value_extended);
}

// ============================================================================
// Group 4 - INC, DEC
// ============================================================================

typedef ExecuteInstructionStatus (*Group4ExecuteInstructionFn)(
    const InstructionContext* ctx, Operand* dest);

// Group 4 instruction implementations, indexed by the corresponding REG field
// value in the ModRM byte.
static const Group4ExecuteInstructionFn kGroup4ExecuteInstructionFns[] = {
    ExecuteInc,  // 0 - INC
    ExecuteDec,  // 1 - DEC
};

// Group 4 instruction handler.
static ExecuteInstructionStatus ExecuteGroup4Instruction(
    const InstructionContext* ctx) {
  const Group4ExecuteInstructionFn fn =
      kGroup4ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  return fn(ctx, &dest);
}

// ============================================================================
// Opcode table
// ============================================================================

// Opcode metadata definitions.
static const OpcodeMetadata opcodes[] = {
    // ADD r/m8, r8
    {.opcode = 0x00,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteAddRegisterToRegisterOrMemory},
    // ADD r/m16, r16
    {.opcode = 0x01,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteAddRegisterToRegisterOrMemory},
    // ADD r8, r/m8
    {.opcode = 0x02,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteAddRegisterOrMemoryToRegister},
    // ADD r16, r/m16
    {.opcode = 0x03,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteAddRegisterOrMemoryToRegister},
    // ADD AL, imm8
    {.opcode = 0x04,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteAddImmediateToALOrAX},
    // ADD AX, imm16
    {.opcode = 0x05,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteAddImmediateToALOrAX},
    // PUSH ES
    {.opcode = 0x06,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushSegmentRegister},
    // POP ES
    {.opcode = 0x07,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopSegmentRegister},
    // OR r/m8, r8
    {.opcode = 0x08,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteBooleanOrRegisterToRegisterOrMemory},
    // OR r/m16, r16
    {.opcode = 0x09,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteBooleanOrRegisterToRegisterOrMemory},
    // OR r8, r/m8
    {.opcode = 0x0A,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteBooleanOrRegisterOrMemoryToRegister},
    // OR r16, r/m16
    {.opcode = 0x0B,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteBooleanOrRegisterOrMemoryToRegister},
    // OR AL, imm8
    {.opcode = 0x0C,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteBooleanOrImmediateToALOrAX},
    // OR AX, imm16
    {.opcode = 0x0D,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteBooleanOrImmediateToALOrAX},
    // PUSH CS
    {.opcode = 0x0E,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushSegmentRegister},
    // ADC r/m8, r8
    {.opcode = 0x10,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteAddRegisterToRegisterOrMemoryWithCarry},
    // ADC r/m16, r16
    {.opcode = 0x11,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteAddRegisterToRegisterOrMemoryWithCarry},
    // ADC r8, r/m8
    {.opcode = 0x12,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteAddRegisterOrMemoryToRegisterWithCarry},
    // ADC r16, r/m16
    {.opcode = 0x13,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteAddRegisterOrMemoryToRegisterWithCarry},
    // ADC AL, imm8
    {.opcode = 0x14,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteAddImmediateToALOrAXWithCarry},
    // ADC AX, imm16
    {.opcode = 0x15,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteAddImmediateToALOrAXWithCarry},
    // PUSH SS
    {.opcode = 0x16,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushSegmentRegister},
    // POP SS
    {.opcode = 0x17,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopSegmentRegister},
    // SBB r/m8, r8
    {.opcode = 0x18,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteSubRegisterFromRegisterOrMemoryWithBorrow},
    // SBB r/m16, r16
    {.opcode = 0x19,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteSubRegisterFromRegisterOrMemoryWithBorrow},
    // SBB r8, r/m8
    {.opcode = 0x1A,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteSubRegisterOrMemoryFromRegisterWithBorrow},
    // SBB r16, r/m16
    {.opcode = 0x1B,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteSubRegisterOrMemoryFromRegisterWithBorrow},
    // SBB AL, imm8
    {.opcode = 0x1C,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteSubImmediateFromALOrAXWithBorrow},
    // SBB AX, imm16
    {.opcode = 0x1D,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteSubImmediateFromALOrAXWithBorrow},
    // PUSH DS
    {.opcode = 0x1E,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushSegmentRegister},
    // POP DS
    {.opcode = 0x1F,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopSegmentRegister},
    // AND r/m8, r8
    {.opcode = 0x20,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteBooleanAndRegisterToRegisterOrMemory},
    // AND r/m16, r16
    {.opcode = 0x21,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteBooleanAndRegisterToRegisterOrMemory},
    // AND r8, r/m8
    {.opcode = 0x22,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteBooleanAndRegisterOrMemoryToRegister},
    // AND r16, r/m16
    {.opcode = 0x23,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteBooleanAndRegisterOrMemoryToRegister},
    // AND AL, imm8
    {.opcode = 0x24,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteBooleanAndImmediateToALOrAX},
    // AND AX, imm16
    {.opcode = 0x25,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteBooleanAndImmediateToALOrAX},
    // DAA
    {.opcode = 0x27, .has_modrm = false, .immediate_size = 0},
    // SUB r/m8, r8
    {.opcode = 0x28,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteSubRegisterFromRegisterOrMemory},
    // SUB r/m16, r16
    {.opcode = 0x29,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteSubRegisterFromRegisterOrMemory},
    // SUB r8, r/m8
    {.opcode = 0x2A,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteSubRegisterOrMemoryFromRegister},
    // SUB r16, r/m16
    {.opcode = 0x2B,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteSubRegisterOrMemoryFromRegister},
    // SUB AL, imm8
    {.opcode = 0x2C,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteSubImmediateFromALOrAX},
    // SUB AX, imm16
    {.opcode = 0x2D,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteSubImmediateFromALOrAX},
    // DAS
    {.opcode = 0x2F, .has_modrm = false, .immediate_size = 0},
    // XOR r/m8, r8
    {.opcode = 0x30,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteBooleanXorRegisterToRegisterOrMemory},
    // XOR r/m16, r16
    {.opcode = 0x31,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteBooleanXorRegisterToRegisterOrMemory},
    // XOR r8, r/m8
    {.opcode = 0x32,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteBooleanXorRegisterOrMemoryToRegister},
    // XOR r16, r/m16
    {.opcode = 0x33,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteBooleanXorRegisterOrMemoryToRegister},
    // XOR AL, imm8
    {.opcode = 0x34,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteBooleanXorImmediateToALOrAX},
    // XOR AX, imm16
    {.opcode = 0x35,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteBooleanXorImmediateToALOrAX},
    // AAA
    {.opcode = 0x37, .has_modrm = false, .immediate_size = 0},
    // CMP r/m8, r8
    {.opcode = 0x38,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteCmpRegisterToRegisterOrMemory},
    // CMP r/m16, r16
    {.opcode = 0x39,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteCmpRegisterToRegisterOrMemory},
    // CMP r8, r/m8
    {.opcode = 0x3A,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteCmpRegisterOrMemoryToRegister},
    // CMP r16, r/m16
    {.opcode = 0x3B,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteCmpRegisterOrMemoryToRegister},
    // CMP AL, imm8
    {.opcode = 0x3C,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteCmpImmediateToALOrAX},
    // CMP AX, imm16
    {.opcode = 0x3D,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteCmpImmediateToALOrAX},
    // AAS
    {.opcode = 0x3F, .has_modrm = false, .immediate_size = 0},
    // INC AX
    {.opcode = 0x40,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteIncRegister},
    // INC CX
    {.opcode = 0x41,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteIncRegister},
    // INC DX
    {.opcode = 0x42,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteIncRegister},
    // INC BX
    {.opcode = 0x43,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteIncRegister},
    // INC SP
    {.opcode = 0x44,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteIncRegister},
    // INC BP
    {.opcode = 0x45,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteIncRegister},
    // INC SI
    {.opcode = 0x46,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteIncRegister},
    // INC DI
    {.opcode = 0x47,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteIncRegister},
    // DEC AX
    {.opcode = 0x48,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteDecRegister},
    // DEC CX
    {.opcode = 0x49,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteDecRegister},
    // DEC DX
    {.opcode = 0x4A,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteDecRegister},
    // DEC BX
    {.opcode = 0x4B,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteDecRegister},
    // DEC SP
    {.opcode = 0x4C,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteDecRegister},
    // DEC BP
    {.opcode = 0x4D,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteDecRegister},
    // DEC SI
    {.opcode = 0x4E,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteDecRegister},
    // DEC DI
    {.opcode = 0x4F,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteDecRegister},
    // PUSH AX
    {.opcode = 0x50,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushRegister},
    // PUSH CX
    {.opcode = 0x51,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushRegister},
    // PUSH DX
    {.opcode = 0x52,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushRegister},
    // PUSH BX
    {.opcode = 0x53,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushRegister},
    // PUSH SP
    {.opcode = 0x54,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushRegister},
    // PUSH BP
    {.opcode = 0x55,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushRegister},
    // PUSH SI
    {.opcode = 0x56,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushRegister},
    // PUSH DI
    {.opcode = 0x57,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushRegister},
    // POP AX
    {.opcode = 0x58,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopRegister},
    // POP CX
    {.opcode = 0x59,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopRegister},
    // POP DX
    {.opcode = 0x5A,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopRegister},
    // POP BX
    {.opcode = 0x5B,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopRegister},
    // POP SP
    {.opcode = 0x5C,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopRegister},
    // POP BP
    {.opcode = 0x5D,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopRegister},
    // POP SI
    {.opcode = 0x5E,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopRegister},
    // POP DI
    {.opcode = 0x5F,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopRegister},
    // JO rel8
    {.opcode = 0x70,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JNO rel8
    {.opcode = 0x71,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JB/JNAE/JC rel8
    {.opcode = 0x72,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JNB/JAE/JNC rel8
    {.opcode = 0x73,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JE/JZ rel8
    {.opcode = 0x74,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JNE/JNZ rel8
    {.opcode = 0x75,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JBE/JNA rel8
    {.opcode = 0x76,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JNBE/JA rel8
    {.opcode = 0x77,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JS rel8
    {.opcode = 0x78,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JNS rel8
    {.opcode = 0x79,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JP/JPE rel8
    {.opcode = 0x7A,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JNP/JPO rel8
    {.opcode = 0x7B,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteUnsignedConditionalJump},
    // JL/JNGE rel8
    {.opcode = 0x7C,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteSignedConditionalJumpJLOrJNL},
    // JNL/JGE rel8
    {.opcode = 0x7D,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteSignedConditionalJumpJLOrJNL},
    // JLE/JNG rel8
    {.opcode = 0x7E,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteSignedConditionalJumpJLEOrJNLE},
    // JNLE/JG rel8
    {.opcode = 0x7F,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteSignedConditionalJumpJLEOrJNLE},
    // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m8, imm8 (Group 1)
    {.opcode = 0x80,
     .has_modrm = true,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteGroup1Instruction},
    // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m16, imm16 (Group 1)
    {.opcode = 0x81,
     .has_modrm = true,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteGroup1Instruction},
    // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m8, imm8 (Group 1)
    {.opcode = 0x82,
     .has_modrm = true,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteGroup1Instruction},
    // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m16, imm8 (Group 1)
    {.opcode = 0x83,
     .has_modrm = true,
     // This is a special case - the immediate is 8 bits but the destination is
     // 16 bits.
     .immediate_size = 1,
     .width = kWord,
     .handler = ExecuteGroup1InstructionWithSignExtension},
    // TEST r/m8, r8
    {.opcode = 0x84,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteTestRegisterToRegisterOrMemory},
    // TEST r/m16, r16
    {.opcode = 0x85,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteTestRegisterToRegisterOrMemory},
    // XCHG r/m8, r8
    {.opcode = 0x86,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteExchangeRegisterOrMemory},
    // XCHG r/m16, r16
    {.opcode = 0x87,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteExchangeRegisterOrMemory},
    // MOV r/m8, r8
    {.opcode = 0x88,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteMoveRegisterToRegisterOrMemory},
    // MOV r/m16, r16
    {.opcode = 0x89,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteMoveRegisterToRegisterOrMemory},
    // MOV r8, r/m8
    {.opcode = 0x8A,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteMoveRegisterOrMemoryToRegister},
    // MOV r16, r/m16
    {.opcode = 0x8B,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteMoveRegisterOrMemoryToRegister},
    // MOV r/m16, sreg
    {.opcode = 0x8C,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteMoveSegmentRegisterToRegisterOrMemory},
    // LEA r16, m
    {.opcode = 0x8D,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteLoadEffectiveAddress},
    // MOV sreg, r/m16
    {.opcode = 0x8E,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteMoveRegisterOrMemoryToSegmentRegister},
    // POP r/m16 (Group 1A)
    {.opcode = 0x8F, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // XCHG AX, AX (NOP)
    {.opcode = 0x90,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteExchangeRegister},
    // XCHG AX, CX
    {.opcode = 0x91,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteExchangeRegister},
    // XCHG AX, DX
    {.opcode = 0x92,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteExchangeRegister},
    // XCHG AX, BX
    {.opcode = 0x93,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteExchangeRegister},
    // XCHG AX, SP
    {.opcode = 0x94,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteExchangeRegister},
    // XCHG AX, BP
    {.opcode = 0x95,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteExchangeRegister},
    // XCHG AX, SI
    {.opcode = 0x96,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteExchangeRegister},
    // XCHG AX, DI
    {.opcode = 0x97,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteExchangeRegister},
    // CBW
    {.opcode = 0x98, .has_modrm = false, .immediate_size = 0},
    // CWD
    {.opcode = 0x99, .has_modrm = false, .immediate_size = 0},
    // CALL ptr16:16 (4 bytes: 2 for offset, 2 for segment)
    {.opcode = 0x9A,
     .has_modrm = false,
     .immediate_size = 4,
     .width = kWord,
     .handler = ExecuteDirectFarCall},
    // WAIT
    {.opcode = 0x9B, .has_modrm = false, .immediate_size = 0},
    // PUSHF
    {.opcode = 0x9C,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePushFlags},
    // POPF
    {.opcode = 0x9D,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopFlags},
    // SAHF
    {.opcode = 0x9E,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteStoreAHToFlags},
    // LAHF
    {.opcode = 0x9F,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteLoadAHFromFlags},
    // MOV AL, moffs16
    {.opcode = 0xA0,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kByte,
     .handler = ExecuteMoveMemoryOffsetToALOrAX},
    // MOV AX, moffs16
    {.opcode = 0xA1,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveMemoryOffsetToALOrAX},
    // MOV moffs16, AL
    {.opcode = 0xA2,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kByte,
     .handler = ExecuteMoveALOrAXToMemoryOffset},
    // MOV moffs16, AX
    {.opcode = 0xA3,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveALOrAXToMemoryOffset},
    // MOVSB
    {.opcode = 0xA4, .has_modrm = false, .immediate_size = 0},
    // MOVSW
    {.opcode = 0xA5, .has_modrm = false, .immediate_size = 0},
    // CMPSB
    {.opcode = 0xA6, .has_modrm = false, .immediate_size = 0},
    // CMPSW
    {.opcode = 0xA7, .has_modrm = false, .immediate_size = 0},
    // TEST AL, imm8
    {.opcode = 0xA8,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteTestImmediateToALOrAX},
    // TEST AX, imm16
    {.opcode = 0xA9,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteTestImmediateToALOrAX},
    // STOSB
    {.opcode = 0xAA, .has_modrm = false, .immediate_size = 0},
    // STOSW
    {.opcode = 0xAB, .has_modrm = false, .immediate_size = 0},
    // LODSB
    {.opcode = 0xAC, .has_modrm = false, .immediate_size = 0},
    // LODSW
    {.opcode = 0xAD, .has_modrm = false, .immediate_size = 0},
    // SCASB
    {.opcode = 0xAE, .has_modrm = false, .immediate_size = 0},
    // SCASW
    {.opcode = 0xAF, .has_modrm = false, .immediate_size = 0},
    // MOV AL, imm8
    {.opcode = 0xB0,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV CL, imm8
    {.opcode = 0xB1,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV DL, imm8
    {.opcode = 0xB2,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV BL, imm8
    {.opcode = 0xB3,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV AH, imm8
    {.opcode = 0xB4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV CH, imm8
    {.opcode = 0xB5,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV DH, imm8
    {.opcode = 0xB6,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV BH, imm8
    {.opcode = 0xB7,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV AX, imm16
    {.opcode = 0xB8,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV CX, imm16
    {.opcode = 0xB9,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV DX, imm16
    {.opcode = 0xBA,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV BX, imm16
    {.opcode = 0xBB,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV SP, imm16
    {.opcode = 0xBC,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV BP, imm16
    {.opcode = 0xBD,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV SI, imm16
    {.opcode = 0xBE,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveImmediateToRegister},
    // MOV DI, imm16
    {.opcode = 0xBF,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveImmediateToRegister},
    // RET imm16
    {.opcode = 0xC2,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteNearReturnAndPop},
    // RET
    {.opcode = 0xC3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteNearReturn},
    // LES r16, m32
    {.opcode = 0xC4, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // LDS r16, m32
    {.opcode = 0xC5, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // MOV r/m8, imm8
    {.opcode = 0xC6,
     .has_modrm = true,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteMoveImmediateToRegisterOrMemory},
    // MOV r/m16, imm16
    {.opcode = 0xC7,
     .has_modrm = true,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteMoveImmediateToRegisterOrMemory},
    // RETF imm16
    {.opcode = 0xCA,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteFarReturnAndPop},
    // RETF
    {.opcode = 0xCB,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteFarReturn},
    // INT 3
    {.opcode = 0xCC, .has_modrm = false, .immediate_size = 0},
    // INT imm8
    {.opcode = 0xCD, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // INTO
    {.opcode = 0xCE, .has_modrm = false, .immediate_size = 0},
    // IRET
    {.opcode = 0xCF, .has_modrm = false, .immediate_size = 0},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m8, 1 (Group 2)
    {.opcode = 0xD0, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m16, 1 (Group 2)
    {.opcode = 0xD1, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m8, CL (Group 2)
    {.opcode = 0xD2, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m16, CL (Group 2)
    {.opcode = 0xD3, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // AAM
    {.opcode = 0xD4, .has_modrm = false, .immediate_size = 1},
    // AAD
    {.opcode = 0xD5, .has_modrm = false, .immediate_size = 1},
    // XLAT/XLATB
    {.opcode = 0xD7, .has_modrm = false, .immediate_size = 0},
    // ESC instruction 0xD8 for 8087 numeric coprocessor
    {.opcode = 0xD8, .has_modrm = true, .immediate_size = 0},
    // ESC instruction 0xD9 for 8087 numeric coprocessor
    {.opcode = 0xD9, .has_modrm = true, .immediate_size = 0},
    // ESC instruction 0xDA for 8087 numeric coprocessor
    {.opcode = 0xDA, .has_modrm = true, .immediate_size = 0},
    // ESC instruction 0xDB for 8087 numeric coprocessor
    {.opcode = 0xDB, .has_modrm = true, .immediate_size = 0},
    // ESC instruction 0xDC for 8087 numeric coprocessor
    {.opcode = 0xDC, .has_modrm = true, .immediate_size = 0},
    // ESC instruction 0xDD for 8087 numeric coprocessor
    {.opcode = 0xDD, .has_modrm = true, .immediate_size = 0},
    // ESC instruction 0xDE for 8087 numeric coprocessor
    {.opcode = 0xDE, .has_modrm = true, .immediate_size = 0},
    // ESC instruction 0xDF for 8087 numeric coprocessor
    {.opcode = 0xDF, .has_modrm = true, .immediate_size = 0},
    // LOOPNE/LOOPNZ rel8
    {.opcode = 0xE0,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteLoopZOrNZ},
    // LOOPE/LOOPZ rel8
    {.opcode = 0xE1,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteLoopZOrNZ},
    // LOOP rel8
    {.opcode = 0xE2,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteLoop},
    // JCXZ rel8
    {.opcode = 0xE3,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteJumpIfCXIsZero},
    // IN AL, imm8
    {.opcode = 0xE4, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // IN AX, imm8
    {.opcode = 0xE5, .has_modrm = false, .immediate_size = 1, .width = kWord},
    // OUT imm8, AL
    {.opcode = 0xE6, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // OUT imm8, AX
    {.opcode = 0xE7, .has_modrm = false, .immediate_size = 1, .width = kWord},
    // CALL rel16
    {.opcode = 0xE8,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteDirectNearCall},
    // JMP rel16
    {.opcode = 0xE9, .has_modrm = false, .immediate_size = 2, .width = kWord},
    // JMP ptr16:16 (4 bytes: 2 for offset, 2 for segment)
    {.opcode = 0xEA, .has_modrm = false, .immediate_size = 4, .width = kWord},
    // JMP rel8
    {.opcode = 0xEB, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // IN AL, DX
    {.opcode = 0xEC, .has_modrm = false, .immediate_size = 0, .width = kByte},
    // IN AX, DX
    {.opcode = 0xED, .has_modrm = false, .immediate_size = 0, .width = kWord},
    // OUT DX, AL
    {.opcode = 0xEE, .has_modrm = false, .immediate_size = 0, .width = kByte},
    // OUT DX, AX
    {.opcode = 0xEF, .has_modrm = false, .immediate_size = 0, .width = kWord},
    // LOCK
    {.opcode = 0xF0, .has_modrm = false, .immediate_size = 0},
    // REPNE/REPNZ
    {.opcode = 0xF2, .has_modrm = false, .immediate_size = 0},
    // REP/REPE/REPZ
    {.opcode = 0xF3, .has_modrm = false, .immediate_size = 0},
    // HLT
    {.opcode = 0xF4, .has_modrm = false, .immediate_size = 0},
    // CMC
    {.opcode = 0xF5, .has_modrm = false, .immediate_size = 0},
    // TEST/NOT/NEG/MUL/IMUL/DIV/IDIV r/m8 (Group 3)
    // The immediate size depends on the ModR/M byte.
    {.opcode = 0xF6, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // TEST/NOT/NEG/MUL/IMUL/DIV/IDIV r/m16 (Group 3)
    // The immediate size depends on the ModR/M byte.
    {.opcode = 0xF7, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // CLC
    {.opcode = 0xF8, .has_modrm = false, .immediate_size = 0},
    // STC
    {.opcode = 0xF9, .has_modrm = false, .immediate_size = 0},
    // CLI
    {.opcode = 0xFA, .has_modrm = false, .immediate_size = 0},
    // STI
    {.opcode = 0xFB, .has_modrm = false, .immediate_size = 0},
    // CLD
    {.opcode = 0xFC, .has_modrm = false, .immediate_size = 0},
    // STD
    {.opcode = 0xFD, .has_modrm = false, .immediate_size = 0},
    // INC/DEC r/m8 (Group 4)
    {.opcode = 0xFE,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteGroup4Instruction},
    // INC/DEC/CALL/JMP/PUSH r/m16 (Group 5)
    {.opcode = 0xFF, .has_modrm = true, .immediate_size = 0, .width = kWord},
};

// Opcode metadata lookup table.
static OpcodeMetadata opcode_table[256] = {};

// Populate opcode_table based on opcodes array.
static void InitOpcodeTable() {
  static bool has_run = false;
  if (has_run) {
    return;
  }
  has_run = true;

  for (int i = 0; i < sizeof(opcodes) / sizeof(OpcodeMetadata); ++i) {
    opcode_table[opcodes[i].opcode] = opcodes[i];
  }
}

// ============================================================================
// CPU state
// ============================================================================

void InitCPU(CPUState* cpu) {
  // Global setup
  InitOpcodeTable();

  // Zero out the CPU state
  const CPUState zero_cpu_state = {0};
  *cpu = zero_cpu_state;
  cpu->flags = kInitialFlags;
}

// ============================================================================
// Execution
// ============================================================================

// Helper to check if a byte is a valid prefix
static inline bool IsPrefixByte(uint8_t byte) {
  static const uint8_t kPrefixBytes[] = {
      // Segment overrides
      0x26,  // ES
      0x2E,  // CS
      0x36,  // SS
      0x3E,  // DS
      // Repetition prefixes and LOCK
      0xF0,  // LOCK
      0xF2,  // REPNE
      0xF3,  // REP
  };
  for (uint8_t i = 0; i < sizeof(kPrefixBytes); ++i) {
    if (byte == kPrefixBytes[i]) {
      return true;
    }
  }
  return false;
}

// Helper to read the next instruction byte.
static inline uint8_t ReadNextInstructionByte(CPUState* cpu, uint16_t* ip) {
  OperandAddress address = {
      .type = kOperandAddressTypeMemory,
      .value = {
          .memory_address = {
              .segment_register_index = kCS,
              .offset = (*ip)++,
          }}};
  return ReadMemoryByte(cpu, &address).value.byte_value;
}

// Returns the number of displacement bytes based on the ModR/M byte.
static inline uint8_t GetDisplacementSize(uint8_t mod, uint8_t rm) {
  switch (mod) {
    case 0:
      // Special case: 16-bit displacement
      return rm == 6 ? 2 : 0;
    case 1:
    case 2:
      // 8 or 16-bit displacement
      return mod;
    default:
      // No displacement
      return 0;
  }
}

// Returns the number of immediate bytes in an instruction.
static inline uint8_t GetImmediateSize(
    const OpcodeMetadata* metadata, uint8_t reg) {
  switch (metadata->opcode) {
    // TEST r/m8, imm8
    case 0xF6:
      return reg == 0 ? 1 : 0;
    // TEST r/m16, imm16
    case 0xF7:
      return reg == 0 ? 2 : 0;
    default:
      return metadata->immediate_size;
  }
}

FetchNextInstructionStatus FetchNextInstruction(
    CPUState* cpu, EncodedInstruction* dest_instruction) {
  EncodedInstruction instruction = {0};
  uint8_t current_byte;
  const uint16_t original_ip = cpu->registers[kIP];
  uint16_t ip = cpu->registers[kIP];

  // Prefix
  current_byte = ReadNextInstructionByte(cpu, &ip);
  while (IsPrefixByte(current_byte)) {
    if (instruction.prefix_size >= kMaxPrefixBytes) {
      return kFetchPrefixTooLong;
    }
    instruction.prefix[instruction.prefix_size++] = current_byte;
    current_byte = ReadNextInstructionByte(cpu, &ip);
  }

  // Opcode
  instruction.opcode = current_byte;
  const OpcodeMetadata* metadata = &opcode_table[instruction.opcode];

  // ModR/M
  if (metadata->has_modrm) {
    uint8_t mod_rm_byte = ReadNextInstructionByte(cpu, &ip);
    instruction.has_mod_rm = true;
    instruction.mod_rm.mod = (mod_rm_byte >> 6) & 0x03;  // Bits 6-7
    instruction.mod_rm.reg = (mod_rm_byte >> 3) & 0x07;  // Bits 3-5
    instruction.mod_rm.rm = mod_rm_byte & 0x07;          // Bits 0-2

    // Displacement
    instruction.displacement_size =
        GetDisplacementSize(instruction.mod_rm.mod, instruction.mod_rm.rm);
    for (int i = 0; i < instruction.displacement_size; ++i) {
      instruction.displacement[i] = ReadNextInstructionByte(cpu, &ip);
    }
  }

  // Immediate operand
  instruction.immediate_size =
      GetImmediateSize(metadata, instruction.mod_rm.reg);
  for (int i = 0; i < instruction.immediate_size; ++i) {
    instruction.immediate[i] = ReadNextInstructionByte(cpu, &ip);
  }

  instruction.size = ip - original_ip;

  *dest_instruction = instruction;
  return kFetchSuccess;
}

ExecuteInstructionStatus ExecuteInstruction(
    CPUState* cpu, const EncodedInstruction* instruction) {
  const OpcodeMetadata* metadata = &opcode_table[instruction->opcode];

  if (!metadata->handler) {
    return kExecuteInvalidOpcode;
  }

  // Check encoded instruction against expected instruction format.
  if (instruction->has_mod_rm != metadata->has_modrm) {
    return kExecuteInvalidInstruction;
  }
  if (instruction->immediate_size !=
      (metadata->has_modrm ? GetImmediateSize(metadata, instruction->mod_rm.reg)
                           : metadata->immediate_size)) {
    return kExecuteInvalidInstruction;
  }

  InstructionContext context = {
      .cpu = cpu,
      .instruction = instruction,
      .metadata = metadata,
  };
  return metadata->handler(&context);
}