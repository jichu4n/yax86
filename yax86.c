#include "yax86.h"

// ============================================================================
// General helpers
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

// Helper function to convert OperandValue to a 32-bit value. This makes it
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

// Read a byte from memory.
static OperandValue ReadMemoryByte(
    CPUState* cpu, const OperandAddress* address) {
  const MemoryAddress* memory_address = &address->value.memory_address;
  uint16_t segment = cpu->registers[memory_address->segment_register_index];
  uint8_t byte_value = cpu->config->read_memory_byte(
      cpu->config->context, (segment << 4) + memory_address->offset);
  OperandValue value = {
      .width = kByte,
      .value = {.byte_value = byte_value},
  };
  return value;
}

// Read a word from memory.
static OperandValue ReadMemoryWord(
    CPUState* cpu, const OperandAddress* address) {
  const MemoryAddress* memory_address = &address->value.memory_address;
  uint16_t segment = cpu->registers[memory_address->segment_register_index];
  const uint16_t address_value = (segment << 4) + memory_address->offset;
  uint8_t low_byte_value =
      cpu->config->read_memory_byte(cpu->config->context, address_value);
  uint8_t high_byte_value =
      cpu->config->read_memory_byte(cpu->config->context, address_value + 1);
  uint16_t word_value = (high_byte_value << 8) | low_byte_value;
  OperandValue value = {
      .width = kWord,
      .value = {.word_value = word_value},
  };
  return value;
}

// Read a byte from a register.
static OperandValue ReadRegisterByte(
    CPUState* cpu, const OperandAddress* address) {
  const RegisterAddress* register_address = &address->value.register_address;
  uint8_t byte_value = cpu->registers[register_address->register_index] >>
                       register_address->byte_offset;
  OperandValue value = {
      .width = kByte,
      .value = {.byte_value = byte_value},
  };
  return value;
}

// Read a word from a register.
static OperandValue ReadRegisterWord(
    CPUState* cpu, const OperandAddress* address) {
  const RegisterAddress* register_address = &address->value.register_address;
  uint16_t word_value = cpu->registers[register_address->register_index];
  OperandValue value = {
      .width = kWord,
      .value = {.word_value = word_value},
  };
  return value;
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

// ============================================================================
// Instructions
// ============================================================================

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
      // Sign-extend 8-bit displacement
      address.offset += (int8_t)instruction->displacement[0];
      break;
    }
    case 2: {
      // Signed 16-bit displacement
      address.offset += (int16_t)(instruction->displacement[0] |
                                  (instruction->displacement[1] << 8));
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

void SetFlag(CPUState* cpu, Flag flag, bool value) {
  if (value) {
    cpu->flags |= flag;
  } else {
    cpu->flags &= ~flag;
  }
}

bool GetFlag(const CPUState* cpu, Flag flag) {
  return (cpu->flags & flag) != 0;
}

// Set common CPU flags after an 8-bit operation. This includes:
// - Zero flag (ZF)
// - Sign flag (SF)
// - Parity Flag (PF)
static void SetCommonFlagsAfterInstructionByte(CPUState* cpu, uint32_t result) {
  uint8_t result8 = result & 0xFF;
  // Zero flag (ZF)
  SetFlag(cpu, kZF, result8 == 0);
  // Sign flag (SF)
  SetFlag(cpu, kSF, result8 & 0x80);
  // Parity flag (PF)
  // Set if the number of set bits in the least significant byte is even
  uint8_t parity = result8;
  parity ^= parity >> 4;
  parity ^= parity >> 2;
  parity ^= parity >> 1;
  SetFlag(cpu, kPF, (parity & 1) == 0);
}

// Set common CPU flags after a 16-bit operation.
static void SetCommonFlagsAfterInstructionWord(CPUState* cpu, uint32_t result) {
  uint16_t result16 = result & 0xFFFF;
  // Zero flag (ZF)
  SetFlag(cpu, kZF, result16 == 0);
  // Sign flag (SF)
  SetFlag(cpu, kSF, result16 & 0x8000);
  // Parity flag (PF)
  // Set if the number of set bits in the least significant byte is even
  uint8_t parity = result16 & 0xFF;  // Check only the low byte for parity
  parity ^= parity >> 4;
  parity ^= parity >> 2;
  parity ^= parity >> 1;
  SetFlag(cpu, kPF, (parity & 1) == 0);
}

// Table of common CPU flag setting functions, indexed by Width.
static void (*const kSetCommonFlagsFn[kNumWidths])(
    CPUState* cpu,
    uint32_t result) = {
    SetCommonFlagsAfterInstructionByte,  // kByte
    SetCommonFlagsAfterInstructionWord   // kWord
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

static const OpcodeMetadata opcodes[];

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
static inline Operand ReadRegisterOperand(const InstructionContext* ctx) {
  Width width = ctx->metadata->width;
  Operand operand;
  operand.address.type = kOperandAddressTypeRegister;
  operand.address.value.register_address =
      kGetRegisterAddressFn[width](ctx->cpu, ctx->instruction->mod_rm.reg);
  operand.value = kReadOperandValueFn[kOperandAddressTypeRegister][width](
      ctx->cpu, &operand.address);
  return operand;
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

// Set common CPU flags after an instruction.
static inline void SetCommonFlagsAfterInstruction(
    const InstructionContext* ctx, uint32_t result) {
  Width width = ctx->metadata->width;
  kSetCommonFlagsFn[width](ctx->cpu, result);
}

// Set CPU flags after an instruction.
static inline void SetFlagsAfterAddInstruction(
    const InstructionContext* ctx, const Operand* op1, const Operand* op2,
    uint32_t result) {
  Width width = ctx->metadata->width;
  SetCommonFlagsAfterInstruction(ctx, result);

  // Get raw values from operands
  uint32_t operand1 = FromOperand(op1);
  uint32_t operand2 = FromOperand(op2);

  switch (width) {
    case kByte: {
      // Carry Flag (CF)
      SetFlag(ctx->cpu, kCF, result > 0xFF);
      // Auxiliary Carry Flag (AF) - carry from bit 3 to bit 4
      SetFlag(ctx->cpu, kAF, ((operand1 & 0xF) + (operand2 & 0xF)) > 0xF);
      // Overflow Flag (OF)
      // Set when result has wrong sign (both operands have same sign but result
      // has different sign)
      bool op1_sign = (operand1 & 0x80) != 0;
      bool op2_sign = (operand2 & 0x80) != 0;
      bool result_sign = (result & 0x80) != 0;
      SetFlag(
          ctx->cpu, kOF, (op1_sign == op2_sign) && (result_sign != op1_sign));
      break;
    }
    case kWord: {
      // Carry Flag (CF)
      SetFlag(ctx->cpu, kCF, result > 0xFFFF);
      // Auxiliary Carry Flag (AF) - carry from bit 3 to bit 4
      SetFlag(ctx->cpu, kAF, ((operand1 & 0xF) + (operand2 & 0xF)) > 0xF);
      // Overflow Flag (OF)
      // Set when result has wrong sign (both operands have same sign but result
      // has different sign)
      bool op1_sign = (operand1 & 0x8000) != 0;
      bool op2_sign = (operand2 & 0x8000) != 0;
      bool result_sign = (result & 0x8000) != 0;
      SetFlag(
          ctx->cpu, kOF, (op1_sign == op2_sign) && (result_sign != op1_sign));
      break;
    }
  }
}

// ADD r/m8, r8
// ADD r/m16, r16
static ExecuteInstructionStatus HandleAddRegisterToRegisterOrMemory(
    const InstructionContext* ctx) {
  Operand src = ReadRegisterOperand(ctx);
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  uint32_t result = FromOperand(&src) + FromOperand(&dest);
  WriteOperand(ctx, &dest, result);
  SetFlagsAfterAddInstruction(ctx, &src, &dest, result);
  return kExecuteSuccess;
}

// ADD r8, r/m8
// ADD r16, r/m16
static ExecuteInstructionStatus HandleAddRegisterOrMemoryToRegister(
    const InstructionContext* ctx) {
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  Operand dest = ReadRegisterOperand(ctx);
  uint32_t result = FromOperand(&src) + FromOperand(&dest);
  WriteOperand(ctx, &dest, result);
  SetFlagsAfterAddInstruction(ctx, &src, &dest, result);
  return kExecuteSuccess;
}

// Opcode metadata definitions.
static const OpcodeMetadata opcodes[] = {
    // ADD r/m8, r8
    {.opcode = 0x00,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = HandleAddRegisterToRegisterOrMemory},
    // ADD r/m16, r16
    {.opcode = 0x01,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     HandleAddRegisterToRegisterOrMemory},
    // ADD r8, r/m8
    {.opcode = 0x02,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = HandleAddRegisterOrMemoryToRegister},
    // ADD r16, r/m16
    {.opcode = 0x03,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = HandleAddRegisterOrMemoryToRegister},
    // ADD AL, imm8
    {.opcode = 0x04, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // ADD AX, imm16
    {.opcode = 0x04, .has_modrm = false, .immediate_size = 1, .width = kWord},
    // PUSH ES
    {.opcode = 0x06, .has_modrm = false, .immediate_size = 0, .width = kWord},
    // POP ES
    {.opcode = 0x07, .has_modrm = false, .immediate_size = 0, .width = kWord},
    // OR r/m8, r8
    {.opcode = 0x08, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // OR r/m16, r16
    {.opcode = 0x09, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // OR r8, r/m8
    {.opcode = 0x0A, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // OR r16, r/m16
    {.opcode = 0x0B, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // OR AL, imm8
    {.opcode = 0x0C, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // OR AX, imm16
    {.opcode = 0x0D, .has_modrm = false, .immediate_size = 2, .width = kWord},
    // PUSH CS
    {.opcode = 0x0E, .has_modrm = false, .immediate_size = 0, .width = kWord},
    // ADC r/m8, r8
    {.opcode = 0x10, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // ADC r/m16, r16
    {.opcode = 0x11, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // ADC r8, r/m8
    {.opcode = 0x12, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // ADC r16, r/m16
    {.opcode = 0x13, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // ADC AL, imm8
    {.opcode = 0x14, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // ADC AX, imm16
    {.opcode = 0x15, .has_modrm = false, .immediate_size = 2, .width = kWord},
    // PUSH SS
    {.opcode = 0x16, .has_modrm = false, .immediate_size = 0, .width = kWord},
    // POP SS
    {.opcode = 0x17, .has_modrm = false, .immediate_size = 0, .width = kWord},
    // SBB r/m8, r8
    {.opcode = 0x18, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // SBB r/m16, r16
    {.opcode = 0x19, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // SBB r8, r/m8
    {.opcode = 0x1A, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // SBB r16, r/m16
    {.opcode = 0x1B, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // SBB AL, imm8
    {.opcode = 0x1C, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // SBB AX, imm16
    {.opcode = 0x1D, .has_modrm = false, .immediate_size = 2, .width = kWord},
    // PUSH DS
    {.opcode = 0x1E, .has_modrm = false, .immediate_size = 0, .width = kWord},
    // POP DS
    {.opcode = 0x1E, .has_modrm = false, .immediate_size = 0, .width = kWord},
    // AND r/m8, r8
    {.opcode = 0x20, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // AND r/m16, r16
    {.opcode = 0x21, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // AND r8, r/m8
    {.opcode = 0x22, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // AND r16, r/m16
    {.opcode = 0x23, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // AND AL, imm8
    {.opcode = 0x24, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // AND AX, imm16
    {.opcode = 0x25, .has_modrm = false, .immediate_size = 2, .width = kWord},
    // DAA
    {.opcode = 0x27, .has_modrm = false, .immediate_size = 0},
    // SUB r/m8, r8
    {.opcode = 0x28, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // SUB r/m16, r16
    {.opcode = 0x29, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // SUB r8, r/m8
    {.opcode = 0x2A, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // SUB r16, r/m16
    {.opcode = 0x2B, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // SUB AL, imm8
    {.opcode = 0x2C, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // SUB AX, imm16
    {.opcode = 0x2D, .has_modrm = false, .immediate_size = 2, .width = kWord},
    // DAS
    {.opcode = 0x2F, .has_modrm = false, .immediate_size = 0},
    // XOR r/m8, r8
    {.opcode = 0x30, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // XOR r/m16, r16
    {.opcode = 0x31, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // XOR r8, r/m8
    {.opcode = 0x32, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // XOR r16, r/m16
    {.opcode = 0x33, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // XOR AL, imm8
    {.opcode = 0x34, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // XOR AX, imm16
    {.opcode = 0x35, .has_modrm = false, .immediate_size = 2, .width = kWord},
    // AAA
    {.opcode = 0x37, .has_modrm = false, .immediate_size = 0},
    // CMP r/m8, r8
    {.opcode = 0x38, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // CMP r/m16, r16
    {.opcode = 0x39, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // CMP r8, r/m8
    {.opcode = 0x3A, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // CMP r16, r/m16
    {.opcode = 0x3B, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // CMP AL, imm8
    {.opcode = 0x3C, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // CMP AX, imm16
    {.opcode = 0x3D, .has_modrm = false, .immediate_size = 2, .width = kWord},
    // AAS
    {.opcode = 0x3F, .has_modrm = false, .immediate_size = 0},
    // INC AX
    {.opcode = 0x40, .has_modrm = false, .immediate_size = 0},
    // INC CX
    {.opcode = 0x41, .has_modrm = false, .immediate_size = 0},
    // INC DX
    {.opcode = 0x42, .has_modrm = false, .immediate_size = 0},
    // INC BX
    {.opcode = 0x43, .has_modrm = false, .immediate_size = 0},
    // INC SP
    {.opcode = 0x44, .has_modrm = false, .immediate_size = 0},
    // INC BP
    {.opcode = 0x45, .has_modrm = false, .immediate_size = 0},
    // INC SI
    {.opcode = 0x46, .has_modrm = false, .immediate_size = 0},
    // INC DI
    {.opcode = 0x47, .has_modrm = false, .immediate_size = 0},
    // DEC AX
    {.opcode = 0x48, .has_modrm = false, .immediate_size = 0},
    // DEC CX
    {.opcode = 0x49, .has_modrm = false, .immediate_size = 0},
    // DEC DX
    {.opcode = 0x4A, .has_modrm = false, .immediate_size = 0},
    // DEC BX
    {.opcode = 0x4B, .has_modrm = false, .immediate_size = 0},
    // DEC SP
    {.opcode = 0x4C, .has_modrm = false, .immediate_size = 0},
    // DEC BP
    {.opcode = 0x4D, .has_modrm = false, .immediate_size = 0},
    // DEC SI
    {.opcode = 0x4E, .has_modrm = false, .immediate_size = 0},
    // DEC DI
    {.opcode = 0x4F, .has_modrm = false, .immediate_size = 0},
    // PUSH AX
    {.opcode = 0x50, .has_modrm = false, .immediate_size = 0},
    // PUSH CX
    {.opcode = 0x51, .has_modrm = false, .immediate_size = 0},
    // PUSH DX
    {.opcode = 0x52, .has_modrm = false, .immediate_size = 0},
    // PUSH BX
    {.opcode = 0x53, .has_modrm = false, .immediate_size = 0},
    // PUSH SP
    {.opcode = 0x54, .has_modrm = false, .immediate_size = 0},
    // PUSH BP
    {.opcode = 0x55, .has_modrm = false, .immediate_size = 0},
    // PUSH SI
    {.opcode = 0x56, .has_modrm = false, .immediate_size = 0},
    // PUSH DI
    {.opcode = 0x57, .has_modrm = false, .immediate_size = 0},
    // POP AX
    {.opcode = 0x58, .has_modrm = false, .immediate_size = 0},
    // POP CX
    {.opcode = 0x59, .has_modrm = false, .immediate_size = 0},
    // POP DX
    {.opcode = 0x5A, .has_modrm = false, .immediate_size = 0},
    // POP BX
    {.opcode = 0x5B, .has_modrm = false, .immediate_size = 0},
    // POP SP
    {.opcode = 0x5C, .has_modrm = false, .immediate_size = 0},
    // POP BP
    {.opcode = 0x5D, .has_modrm = false, .immediate_size = 0},
    // POP SI
    {.opcode = 0x5E, .has_modrm = false, .immediate_size = 0},
    // POP DI
    {.opcode = 0x5F, .has_modrm = false, .immediate_size = 0},
    // JO rel8
    {.opcode = 0x70, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // JNO rel8
    {.opcode = 0x71, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // JB/JNAE/JC rel8
    {.opcode = 0x72, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // JNB/JAE/JNC rel8
    {.opcode = 0x73, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // JE/JZ rel8
    {.opcode = 0x74, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // JNE/JNZ rel8
    {.opcode = 0x75, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // JBE/JNA rel8
    {.opcode = 0x76, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // JNBE/JA rel8
    {.opcode = 0x77, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // JS rel8
    {.opcode = 0x78, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // JNS rel8
    {.opcode = 0x79, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // JP/JPE rel8
    {.opcode = 0x7A, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // JNP/JPO rel8
    {.opcode = 0x7B, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // JL/JNGE rel8
    {.opcode = 0x7C, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // JNL/JGE rel8
    {.opcode = 0x7D, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // JLE/JNG rel8
    {.opcode = 0x7E, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // JNLE/JG rel8
    {.opcode = 0x7F, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // ADD/ADC/SBB/SUB/CMP r/m8, imm8 (Group 1)
    {.opcode = 0x80, .has_modrm = true, .immediate_size = 1, .width = kByte},
    // ADD/ADC/SBB/SUB/CMP r/m16, imm16 (Group 1)
    {.opcode = 0x81, .has_modrm = true, .immediate_size = 2, .width = kWord},
    // ADC/SBB/SUB/CMP r/m8, imm8 (Group 1)
    {.opcode = 0x82, .has_modrm = true, .immediate_size = 1, .width = kByte},
    // ADD/ADC/SBB/SUB/CMP r/m16, imm8 (Group 1)
    {.opcode = 0x83, .has_modrm = true, .immediate_size = 1, .width = kWord},
    // TEST r/m8, r8
    {.opcode = 0x84, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // TEST r/m16, r16
    {.opcode = 0x85, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // XCHG r/m8, r8
    {.opcode = 0x86, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // XCHG r/m16, r16
    {.opcode = 0x87, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // MOV r/m8, r8
    {.opcode = 0x88, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // MOV r/m16, r16
    {.opcode = 0x89, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // MOV r8, r/m8
    {.opcode = 0x8A, .has_modrm = true, .immediate_size = 0, .width = kByte},
    // MOV r16, r/m16
    {.opcode = 0x8B, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // MOV r/m16, sreg
    {.opcode = 0x8C, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // LEA r16, m
    {.opcode = 0x8D, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // MOV sreg, r/m16
    {.opcode = 0x8E, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // POP r/m16 (Group 1A)
    {.opcode = 0x8F, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // XCHG AX, AX (NOP)
    {.opcode = 0x90, .has_modrm = false, .immediate_size = 0},
    // XCHG AX, CX
    {.opcode = 0x91, .has_modrm = false, .immediate_size = 0, .width = kWord},
    // XCHG AX, DX
    {.opcode = 0x92, .has_modrm = false, .immediate_size = 0, .width = kWord},
    // XCHG AX, BX
    {.opcode = 0x93, .has_modrm = false, .immediate_size = 0, .width = kWord},
    // XCHG AX, SP
    {.opcode = 0x94, .has_modrm = false, .immediate_size = 0, .width = kWord},
    // XCHG AX, BP
    {.opcode = 0x95, .has_modrm = false, .immediate_size = 0, .width = kWord},
    // XCHG AX, SI
    {.opcode = 0x96, .has_modrm = false, .immediate_size = 0, .width = kWord},
    // XCHG AX, DI
    {.opcode = 0x97, .has_modrm = false, .immediate_size = 0, .width = kWord},
    // CBW
    {.opcode = 0x98, .has_modrm = false, .immediate_size = 0},
    // CWD
    {.opcode = 0x99, .has_modrm = false, .immediate_size = 0},
    // CALL ptr16:16 (4 bytes: 2 for offset, 2 for segment)
    {.opcode = 0x9A, .has_modrm = false, .immediate_size = 4, .width = kWord},
    // WAIT
    {.opcode = 0x9B, .has_modrm = false, .immediate_size = 0},
    // PUSHF
    {.opcode = 0x9C, .has_modrm = false, .immediate_size = 0},
    // POPF
    {.opcode = 0x9D, .has_modrm = false, .immediate_size = 0},
    // SAHF
    {.opcode = 0x9E, .has_modrm = false, .immediate_size = 0},
    // LAHF
    {.opcode = 0x9F, .has_modrm = false, .immediate_size = 0},
    // MOV AL, moffs8
    {.opcode = 0xA0, .has_modrm = false, .immediate_size = 2, .width = kByte},
    // MOV AX, moffs16
    {.opcode = 0xA1, .has_modrm = false, .immediate_size = 2, .width = kWord},
    // MOV moffs8, AL
    {.opcode = 0xA2, .has_modrm = false, .immediate_size = 2, .width = kByte},
    // MOV moffs16, AX
    {.opcode = 0xA3, .has_modrm = false, .immediate_size = 2, .width = kWord},
    // MOVSB
    {.opcode = 0xA4, .has_modrm = false, .immediate_size = 0},
    // MOVSW
    {.opcode = 0xA5, .has_modrm = false, .immediate_size = 0},
    // CMPSB
    {.opcode = 0xA6, .has_modrm = false, .immediate_size = 0},
    // CMPSW
    {.opcode = 0xA7, .has_modrm = false, .immediate_size = 0},
    // TEST AL, imm8
    {.opcode = 0xA8, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // TEST AX, imm16
    {.opcode = 0xA9, .has_modrm = false, .immediate_size = 2, .width = kWord},
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
    {.opcode = 0xB0, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // MOV CL, imm8
    {.opcode = 0xB1, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // MOV DL, imm8
    {.opcode = 0xB2, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // MOV BL, imm8
    {.opcode = 0xB3, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // MOV AH, imm8
    {.opcode = 0xB4, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // MOV CH, imm8
    {.opcode = 0xB5, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // MOV DH, imm8
    {.opcode = 0xB6, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // MOV BH, imm8
    {.opcode = 0xB7, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // MOV AX, imm16
    {.opcode = 0xB8, .has_modrm = false, .immediate_size = 2, .width = kWord},
    // MOV CX, imm16
    {.opcode = 0xB9, .has_modrm = false, .immediate_size = 2, .width = kWord},
    // MOV DX, imm16
    {.opcode = 0xBA, .has_modrm = false, .immediate_size = 2, .width = kWord},
    // MOV BX, imm16
    {.opcode = 0xBB, .has_modrm = false, .immediate_size = 2, .width = kWord},
    // MOV SP, imm16
    {.opcode = 0xBC, .has_modrm = false, .immediate_size = 2, .width = kWord},
    // MOV BP, imm16
    {.opcode = 0xBD, .has_modrm = false, .immediate_size = 2, .width = kWord},
    // MOV SI, imm16
    {.opcode = 0xBE, .has_modrm = false, .immediate_size = 2, .width = kWord},
    // MOV DI, imm16
    {.opcode = 0xBF, .has_modrm = false, .immediate_size = 2, .width = kWord},
    // RET imm16
    {.opcode = 0xC2, .has_modrm = false, .immediate_size = 2, .width = kWord},
    // RET
    {.opcode = 0xC3, .has_modrm = false, .immediate_size = 0},
    // LES r16, m32
    {.opcode = 0xC4, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // LDS r16, m32
    {.opcode = 0xC5, .has_modrm = true, .immediate_size = 0, .width = kWord},
    // MOV r/m8, imm8 (Group 11)
    {.opcode = 0xC6, .has_modrm = true, .immediate_size = 1, .width = kByte},
    // MOV r/m16, imm16 (Group 11)
    {.opcode = 0xC7, .has_modrm = true, .immediate_size = 2, .width = kWord},
    // RETF imm16
    {.opcode = 0xCA, .has_modrm = false, .immediate_size = 2, .width = kWord},
    // RETF
    {.opcode = 0xCB, .has_modrm = false, .immediate_size = 0},
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
    {.opcode = 0xE0, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // LOOPE/LOOPZ rel8
    {.opcode = 0xE1, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // LOOP rel8
    {.opcode = 0xE2, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // JCXZ rel8
    {.opcode = 0xE3, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // IN AL, imm8
    {.opcode = 0xE4, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // IN AX, imm8
    {.opcode = 0xE5, .has_modrm = false, .immediate_size = 1, .width = kWord},
    // OUT imm8, AL
    {.opcode = 0xE6, .has_modrm = false, .immediate_size = 1, .width = kByte},
    // OUT imm8, AX
    {.opcode = 0xE7, .has_modrm = false, .immediate_size = 1, .width = kWord},
    // CALL rel16
    {.opcode = 0xE8, .has_modrm = false, .immediate_size = 2, .width = kWord},
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
    {.opcode = 0xFE, .has_modrm = true, .immediate_size = 0, .width = kByte},
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