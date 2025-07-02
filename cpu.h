// ==============================================================================
// YAX86 CPU MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_CPU_BUNDLE_H
#define YAX86_CPU_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/cpu/public.h start
// ==============================================================================

#line 1 "./src/cpu/public.h"
// Public interface for the CPU emulator module.
#ifndef YAX86_CPU_PUBLIC_H
#define YAX86_CPU_PUBLIC_H

#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// CPU state
// ============================================================================

// CPU registers.
// Note that the order / numeric values of these constants are important here as
// they must match how the registers are encoded in the ModR/M byte.
typedef enum RegisterIndex {
  // General-purpose and index registers.

  // Accumulator Register
  kAX = 0,
  // Counter Register
  kCX = 1,
  // Data Register
  kDX = 2,
  // Base Register
  kBX = 3,
  // Stack Pointer Register
  kSP = 4,
  // Base Pointer Register
  kBP = 5,
  // Source Index Register
  kSI = 6,
  // Destination Index Register
  kDI = 7,

  // Segment registers.

  // Extra Segment Register
  kES = 8,
  // Code Segment Register
  kCS = 9,
  // Stack Segment Register
  kSS = 10,
  // Data Segment Register
  kDS = 11,

  // Instruction Pointer Register
  kIP,
} RegisterIndex;

enum {
  // Number of registers.
  kNumRegisters = kIP + 1,
};

// CPU flag masks.
typedef enum Flag {
  // Carry Flag
  kCF = (1 << 0),
  // Parity Flag
  kPF = (1 << 2),
  // Auxiliary Carry Flag
  kAF = (1 << 4),
  // Zero Flag
  kZF = (1 << 6),
  // Sign Flag
  kSF = (1 << 7),
  // Trap Flag
  kTF = (1 << 8),
  // Interrupt Enable Flag
  kIF = (1 << 9),
  // Direction Flag
  kDF = (1 << 10),
  // Overflow Flag
  kOF = (1 << 11),
} Flag;

enum {
  // CPU flags value on reset.
  kInitialFlags = (1 << 1),  // Reserved_1 is always 1.
};

// Standard interrupts.
typedef enum InterruptNumber {
  kInterruptDivideError = 0,
  kInterruptSingleStep = 1,
  kInterruptNMI = 2,
  kInterruptBreakpoint = 3,
  kInterruptOverflow = 4,
} InterruptNumer;

// Result status from executing an instruction or opcode.
typedef enum ExecuteStatus {
  // Successfully executed the instruction or opcode.
  kExecuteSuccess = 0,
  // Invalid instruction opcode.
  kExecuteInvalidOpcode,
  // Invalid instruction operands.
  kExecuteInvalidInstruction,
  // The interrupt was not handled by the interrupt handler callback, and should
  // be handled by the VM instead.
  kExecuteUnhandledInterrupt,
  // The VM should stop execution.
  kExecuteHalt,
} ExecuteStatus;

struct CPUState;
struct Instruction;

// Caller-provided runtime configuration.
typedef struct CPUConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Callback to read a byte from memory.
  //
  // On the 8086, accessing an invalid memory address will yield garbage data
  // rather than causing a page fault. This callback interface mirrors that
  // behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  uint8_t (*read_memory_byte)(struct CPUState* cpu, uint16_t address);

  // Callback to write a byte to memory.
  //
  // On the 8086, accessing an invalid memory address will yield garbage data
  // rather than causing a page fault. This callback interface mirrors that
  // behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  void (*write_memory_byte)(
      struct CPUState* cpu, uint16_t address, uint8_t value);

  // Callback to handle an interrupt.
  //   - Return kExecuteSuccess if the interrupt was handled and execution
  //     should continue.
  //   - Return kExecuteUnhandledInterrupt if the interrupt was not handled and
  //     should be handled by the VM instead.
  //   - Return any other value to terminate the execution loop.
  ExecuteStatus (*handle_interrupt)(
      struct CPUState* cpu, uint8_t interrupt_number);

  // Callback invoked before executing an instruction. This can be used to
  // inspect or modify the instruction before it is executed, inject pending
  // interrupt or delay, or terminate the execution loop.
  //   - Return kExecuteSuccess to continue execution.
  //   - Return any other value to terminate the execution loop.
  ExecuteStatus (*on_before_execute_instruction)(
      struct CPUState* cpu, struct Instruction* instruction);

  // Callback invoked after executing an instruction. This can be used to
  // inspect the instruction after it is executed, inject pending interrupt or
  // delay, or terminate the execution loop.
  //   - Return kExecuteSuccess to continue execution.
  //   - Return any other value to terminate the execution loop.
  ExecuteStatus (*on_after_execute_instruction)(
      struct CPUState* cpu, const struct Instruction* instruction);

  // Callback to read a byte from an I/O port.
  //
  // On the 8086, accessing an invalid I/O port will most likely yield garbage
  // data. This callback interface mirrors that behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  uint8_t (*read_port)(struct CPUState* cpu, uint16_t port);

  // Callback to write a byte to an I/O port.
  //
  // On the 8086, accessing an invalid I/O port will most likely yield garbage
  // data. This callback interface mirrors that behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  void (*write_port)(struct CPUState* cpu, uint16_t port, uint8_t value);
} CPUConfig;

// State of the emulated CPU.
typedef struct CPUState {
  // Pointer to the configuration
  CPUConfig* config;

  // Register values
  uint16_t registers[kNumRegisters];
  // Flag values
  uint16_t flags;

  // Whether there is an active interrupt.
  bool has_pending_interrupt;
  // The interrupt number of the pending interrupt.
  uint8_t pending_interrupt_number;
} CPUState;

// ============================================================================
// CPU state helper functions.
// ============================================================================

// Initialize CPU state.
void InitCPU(CPUState* cpu);

// Get the value of a CPU flag.
static inline bool GetFlag(const CPUState* cpu, Flag flag) {
  return (cpu->flags & flag) != 0;
}
// Set a CPU flag.
static inline void SetFlag(CPUState* cpu, Flag flag, bool value) {
  if (value) {
    cpu->flags |= flag;
  } else {
    cpu->flags &= ~flag;
  }
}

// Set pending interrupt to be executed at the end of the current instruction.
static inline void SetPendingInterrupt(
    CPUState* cpu, uint8_t interrupt_number) {
  cpu->has_pending_interrupt = true;
  cpu->pending_interrupt_number = interrupt_number;
}

// Clear pending interrupt.
static inline void ClearPendingInterrupt(CPUState* cpu) {
  cpu->has_pending_interrupt = false;
  cpu->pending_interrupt_number = 0;
}

// ============================================================================
// Instructions
// ============================================================================

enum {
  // Maximum number of prefix bytes supported. On the 8086 and 80186, the length
  // of prefix bytes was actually unlimited. But well-formed code generated by
  // compilers would only have 1 or 2 bytes.
  kMaxPrefixBytes = 2,
  // Maximum number of displacement bytes in an 8086 instruction.
  kMaxDisplacementBytes = 2,
  // Maximum number of immediate data bytes in an 8086 instruction.
  kMaxImmediateBytes = 4,
};

// Instruction prefixes.
typedef enum {
  kPrefixES = 0x26,     // ES segment override
  kPrefixCS = 0x2E,     // CS segment override
  kPrefixSS = 0x36,     // SS segment override
  kPrefixDS = 0x3E,     // DS segment override
  kPrefixLOCK = 0xF0,   // LOCK
  kPrefixREPNZ = 0xF2,  // REPNE/REPNZ
  kPrefixREP = 0xF3,    // REP/REPE/REPZ
} InstructionPrefix;

// The Mod R/M byte.
typedef struct ModRM {
  // Mod field - bits 6 and 7
  uint8_t mod : 2;
  // REG field - bits 3 to 5
  uint8_t reg : 3;
  // R/M field - bits 0 to 2
  uint8_t rm : 3;
} ModRM;

// An encoded instruction.
typedef struct Instruction {
  // Prefix bytes.
  uint8_t prefix[kMaxPrefixBytes];

  // The primary opcode byte.
  uint8_t opcode;

  // The ModR/M byte, which specifies addressing modes. For some instructions,
  // the REG field within this byte acts as an opcode extension.
  ModRM mod_rm;

  // Raw displacement bytes. If displacement_size is 1, only disp_bytes[0] is
  // valid (value is typically sign-extended). If displacement_size is 2,
  // disp_bytes[0] is the low byte, disp_bytes[1] is the high byte.
  uint8_t displacement[kMaxDisplacementBytes];

  // Raw immediate data bytes.
  uint8_t immediate[kMaxImmediateBytes];

  // Flags

  // Whether prefix byte is part of this instruction.
  uint8_t prefix_size : 2;
  // Flag indicating if a ModR/M byte is part of this instruction.
  bool has_mod_rm : 1;
  // Number of displacement bytes present: 0, 1, or 2.
  uint8_t displacement_size : 2;
  // Number of immediate data bytes present: 0, 1, 2, or 4.
  uint8_t immediate_size : 3;

  // Total length of the original encoded instruction in bytes.
  uint8_t size;
} Instruction;

// ============================================================================
// Execution
// ============================================================================

// Result status from fetching the next instruction.
typedef enum FetchNextInstructionStatus {
  kFetchSuccess = 0,
  // Prefix exceeds maximum allowed size.
  kFetchPrefixTooLong = -1,
} FetchNextInstructionStatus;

// Fetch the next instruction from CS:IP.
FetchNextInstructionStatus FetchNextInstruction(
    CPUState* cpu, Instruction* instruction);

// Execute a single fetched instruction.
ExecuteStatus ExecuteInstruction(CPUState* cpu, Instruction* instruction);

// Run a single instruction cycle, including fetching and executing the next
// instruction at CS:IP, and handling interrupts.
ExecuteStatus RunInstructionCycle(CPUState* cpu);

// Run instruction execution loop.
//
// Terminates when an instruction execution or handler returns a non-success
// status.
ExecuteStatus RunMainLoop(CPUState* cpu);

#endif  // YAX86_CPU_PUBLIC_H


// ==============================================================================
// src/cpu/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/common.h start
// ==============================================================================

#line 1 "./src/common.h"
#ifndef YAX86_COMMON_H
#define YAX86_COMMON_H

// Macro that expands to `static` when bundled. Use for variables and functions
// that need to be visible to other files within the same module, but not
// publicly to users of the bundled library.
//
// This enables better IDE integration as it allows each source file to be
// compiled independently in unbundled form, but still keeps the symbols private
// when bundled.
#ifdef YAX86_IMPLEMENTATION
// When bundled, static linkage so that the symbol is only visible within the
// implementation file.
#define YAX86_PRIVATE static
#else
// When unbundled, use default linkage.
#define YAX86_PRIVATE
#endif  // YAX86_IMPLEMENTATION

#endif  // YAX86_COMMON_H


// ==============================================================================
// src/common.h end
// ==============================================================================

// ==============================================================================
// src/cpu/types.h start
// ==============================================================================

#line 1 "./src/cpu/types.h"
#ifndef YAX86_CPU_TYPES_H
#define YAX86_CPU_TYPES_H

#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Data width helpers.

// Data widths supported by the 8086 CPU.
typedef enum Width {
  kByte = 0,
  kWord,
} Width;

enum {
  // Number of data width types.
  kNumWidths = kWord + 1,
};

// Bitmask to extract the sign bit of a value.
static const uint32_t kSignBit[kNumWidths] = {
    1 << 7,   // kByte
    1 << 15,  // kWord
};

// Maximum unsigned value for each data width.
static const uint32_t kMaxValue[kNumWidths] = {
    0xFF,   // kByte
    0xFFFF  // kWord
};

// Maximum signed value for each data width.
static const int32_t kMaxSignedValue[kNumWidths] = {
    0x7F,   // kByte
    0x7FFF  // kWord
};

// Minimum signed value for each data width.
static const int32_t kMinSignedValue[kNumWidths] = {
    -0x80,   // kByte
    -0x8000  // kWord
};

// Number of bytes in each data width.
static const uint8_t kNumBytes[kNumWidths] = {
    1,  // kByte
    2,  // kWord
};

// Number of bits in each data width.
static const uint8_t kNumBits[kNumWidths] = {
    8,   // kByte
    16,  // kWord
};

// Operand types.

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

// Instruction types.

struct OpcodeMetadata;

// Context during instruction execution.
typedef struct {
  CPUState* cpu;
  const Instruction* instruction;
  const struct OpcodeMetadata* metadata;
} InstructionContext;

// Handler function for an opcode.
typedef ExecuteStatus (*OpcodeHandler)(const InstructionContext* context);

// An entry in the opcode lookup table.
typedef struct OpcodeMetadata {
  // Opcode.
  uint8_t opcode;

  // Instruction has ModR/M byte
  bool has_modrm : 1;
  // Number of immediate data bytes: 0, 1, 2, or 4
  uint8_t immediate_size : 3;

  // Width of the instruction's operands.
  Width width : 1;

  // Handler function.
  OpcodeHandler handler;
} OpcodeMetadata;

#endif  // YAX86_CPU_TYPES_H


// ==============================================================================
// src/cpu/types.h end
// ==============================================================================

// ==============================================================================
// src/cpu/operands.h start
// ==============================================================================

#line 1 "./src/cpu/operands.h"
#ifndef YAX86_CPU_OPERANDS_H
#define YAX86_CPU_OPERANDS_H

#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#include "types.h"

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

// Read a value from an operand address.
extern OperandValue ReadOperandValue(
    const InstructionContext* ctx, const OperandAddress* address);

// Get a register or memory operand for an instruction based on the ModR/M
// byte and displacement.
extern Operand ReadRegisterOrMemoryOperand(const InstructionContext* ctx);

// Get a register operand for an instruction.
extern Operand ReadRegisterOperandForRegisterIndex(
    const InstructionContext* ctx, RegisterIndex register_index);

// Get a register operand for an instruction from the REG field of the Mod/RM
// byte.
extern Operand ReadRegisterOperand(const InstructionContext* ctx);

// Get a segment register operand for an instruction from the REG field of the
// Mod/RM byte.
extern Operand ReadSegmentRegisterOperand(const InstructionContext* ctx);

// Write a value to a register or memory operand address.
extern void WriteOperandAddress(
    const InstructionContext* ctx, const OperandAddress* address,
    uint32_t raw_value);

// Write a value to a register or memory operand.
extern void WriteOperand(
    const InstructionContext* ctx, const Operand* operand, uint32_t raw_value);

// Read an immediate value from the instruction.
extern OperandValue ReadImmediate(const InstructionContext* ctx);

#endif  // YAX86_IMPLEMENTATION

#endif  // YAX86_CPU_OPERANDS_H


// ==============================================================================
// src/cpu/operands.h end
// ==============================================================================

// ==============================================================================
// src/cpu/operands.c start
// ==============================================================================

#line 1 "./src/cpu/operands.c"
#ifndef YAX86_IMPLEMENTATION
#include "operands.h"

#include "../common.h"
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
YAX86_PRIVATE uint16_t
ToPhysicalAddress(const CPUState* cpu, const MemoryAddress* address) {
  uint16_t segment = cpu->registers[address->segment_register_index];
  return (segment << 4) + address->offset;
}

// Read a byte from memory as a uint8_t.
YAX86_PRIVATE uint8_t
ReadRawMemoryByte(CPUState* cpu, uint16_t physical_address) {
  return cpu->config->read_memory_byte
             ? cpu->config->read_memory_byte(cpu, physical_address)
             : 0xFF;
}

// Read a word from memory as a uint16_t.
YAX86_PRIVATE uint16_t
ReadRawMemoryWord(CPUState* cpu, uint16_t physical_address) {
  uint8_t low_byte_value = ReadRawMemoryByte(cpu, physical_address);
  uint8_t high_byte_value = ReadRawMemoryByte(cpu, physical_address + 1);
  return (((uint16_t)high_byte_value) << 8) | (uint16_t)low_byte_value;
}

// Read a byte from memory to an OperandValue.
YAX86_PRIVATE OperandValue
ReadMemoryByte(CPUState* cpu, const OperandAddress* address) {
  uint8_t byte_value = ReadRawMemoryByte(
      cpu, ToPhysicalAddress(cpu, &address->value.memory_address));
  return ByteValue(byte_value);
}

// Read a word from memory to an OperandValue.
YAX86_PRIVATE OperandValue
ReadMemoryWord(CPUState* cpu, const OperandAddress* address) {
  uint16_t word_value = ReadRawMemoryWord(
      cpu, ToPhysicalAddress(cpu, &address->value.memory_address));
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
    CPUState* cpu, uint16_t address, uint8_t value) {
  if (!cpu->config->write_memory_byte) {
    return;
  }
  cpu->config->write_memory_byte(cpu, address, value);
}

// Write a word as uint16_t to memory.
YAX86_PRIVATE void WriteRawMemoryWord(
    CPUState* cpu, uint16_t address, uint16_t value) {
  WriteRawMemoryByte(cpu, address, value & 0xFF);
  WriteRawMemoryByte(cpu, address + 1, (value >> 8) & 0xFF);
}

// Write a byte to memory.
YAX86_PRIVATE void WriteMemoryByte(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  WriteRawMemoryByte(
      cpu, ToPhysicalAddress(cpu, &address->value.memory_address),
      value.value.byte_value);
}

// Write a word to memory.
YAX86_PRIVATE void WriteMemoryWord(
    CPUState* cpu, const OperandAddress* address, OperandValue value) {
  WriteRawMemoryWord(
      cpu, ToPhysicalAddress(cpu, &address->value.memory_address),
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


// ==============================================================================
// src/cpu/operands.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions.h start
// ==============================================================================

#line 1 "./src/cpu/instructions.h"
#ifndef YAX86_CPU_INSTRUCTIONS_H
#define YAX86_CPU_INSTRUCTIONS_H

#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#include "types.h"

// ============================================================================
// Helpers - instructions_helpers.h
// ============================================================================

// Set common CPU flags after an instruction. This includes:
// - Zero flag (ZF)
// - Sign flag (SF)
// - Parity Flag (PF)
extern void SetCommonFlagsAfterInstruction(
    const InstructionContext* ctx, uint32_t result);

// Push a value onto the stack.
extern void Push(CPUState* cpu, OperandValue value);
// Pop a value from the stack.
extern OperandValue Pop(CPUState* cpu);

// Dummy instruction for unsupported opcodes.
extern ExecuteStatus ExecuteNoOp(const InstructionContext* ctx);

// ============================================================================
// Opcode table - opcode_table.h
// ============================================================================

// Global opcode metadata lookup table.
extern OpcodeMetadata opcode_table[256];

// ============================================================================
// Move instructions - instructions_mov.h
// ============================================================================

// MOV r/m8, r8
// MOV r/m16, r16
extern ExecuteStatus ExecuteMoveRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// MOV r8, r/m8
// MOV r16, r/m16
extern ExecuteStatus ExecuteMoveRegisterOrMemoryToRegister(
    const InstructionContext* ctx);
// MOV r/m16, sreg
extern ExecuteStatus ExecuteMoveSegmentRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// MOV sreg, r/m16
extern ExecuteStatus ExecuteMoveRegisterOrMemoryToSegmentRegister(
    const InstructionContext* ctx);
// MOV AX/CX/DX/BX/SP/BP/SI/DI, imm16
// MOV AH/AL/CH/CL/DH/DL/BH/BL, imm8
extern ExecuteStatus ExecuteMoveImmediateToRegister(
    const InstructionContext* ctx);
// MOV AL, moffs16
// MOV AX, moffs16
extern ExecuteStatus ExecuteMoveMemoryOffsetToALOrAX(
    const InstructionContext* ctx);
// MOV moffs16, AL
// MOV moffs16, AX
extern ExecuteStatus ExecuteMoveALOrAXToMemoryOffset(
    const InstructionContext* ctx);
// MOV r/m8, imm8
// MOV r/m16, imm16
extern ExecuteStatus ExecuteMoveImmediateToRegisterOrMemory(
    const InstructionContext* ctx);
// XCHG AX, AX/CX/DX/BX/SP/BP/SI/DI
extern ExecuteStatus ExecuteExchangeRegister(const InstructionContext* ctx);
// XCHG r/m8, r8
// XCHG r/m16, r16
extern ExecuteStatus ExecuteExchangeRegisterOrMemory(
    const InstructionContext* ctx);
// XLAT
extern ExecuteStatus ExecuteTranslateByte(const InstructionContext* ctx);

// ============================================================================
// LEA instructions - instructions_lea.h
// ============================================================================

// LEA r16, m
extern ExecuteStatus ExecuteLoadEffectiveAddress(const InstructionContext* ctx);
// LES r16, m
extern ExecuteStatus ExecuteLoadESWithPointer(const InstructionContext* ctx);
// LDS r16, m
extern ExecuteStatus ExecuteLoadDSWithPointer(const InstructionContext* ctx);

// ============================================================================
// Addition instructions - instructions_add.h
// ============================================================================

// Common logic for ADD instructions
extern ExecuteStatus ExecuteAdd(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);
// Common logic for INC instructions
extern ExecuteStatus ExecuteInc(const InstructionContext* ctx, Operand* dest);
// Common logic for ADC instructions
extern ExecuteStatus ExecuteAddWithCarry(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);

// ADD r/m8, r8
// ADD r/m16, r16
extern ExecuteStatus ExecuteAddRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// ADD r8, r/m8
// ADD r16, r/m16
extern ExecuteStatus ExecuteAddRegisterOrMemoryToRegister(
    const InstructionContext* ctx);
// ADD AL, imm8
// ADD AX, imm16
extern ExecuteStatus ExecuteAddImmediateToALOrAX(const InstructionContext* ctx);
// ADC r/m8, r8
// ADC r/m16, r16
extern ExecuteStatus ExecuteAddRegisterToRegisterOrMemoryWithCarry(
    const InstructionContext* ctx);
// ADC r8, r/m8
// ADC r16, r/m16
extern ExecuteStatus ExecuteAddRegisterOrMemoryToRegisterWithCarry(
    const InstructionContext* ctx);
// ADC AL, imm8
// ADC AX, imm16
extern ExecuteStatus ExecuteAddImmediateToALOrAXWithCarry(
    const InstructionContext* ctx);
// INC AX/CX/DX/BX/SP/BP/SI/DI
extern ExecuteStatus ExecuteIncRegister(const InstructionContext* ctx);

// ============================================================================
// Subtraction instructions - instructions_sub.c
// ============================================================================

// Set CPU flags after a SUB, SBB, CMP, or NEG instruction.
extern void SetFlagsAfterSub(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_borrow);

// Common logic for SUB instructions
extern ExecuteStatus ExecuteSub(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);
// Common logic for SBB instructions
extern ExecuteStatus ExecuteSubWithBorrow(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);
// Common logic for DEC instructions
extern ExecuteStatus ExecuteDec(const InstructionContext* ctx, Operand* dest);

// SUB r/m8, r8
// SUB r/m16, r16
extern ExecuteStatus ExecuteSubRegisterFromRegisterOrMemory(
    const InstructionContext* ctx);
// SUB r8, r/m8
// SUB r16, r/m16
extern ExecuteStatus ExecuteSubRegisterOrMemoryFromRegister(
    const InstructionContext* ctx);
// SUB AL, imm8
// SUB AX, imm16
extern ExecuteStatus ExecuteSubImmediateFromALOrAX(
    const InstructionContext* ctx);
// SBB r/m8, r8
// SBB r/m16, r16
extern ExecuteStatus ExecuteSubRegisterFromRegisterOrMemoryWithBorrow(
    const InstructionContext* ctx);
// SBB r8, r/m8
// SBB r16, r/m16
extern ExecuteStatus ExecuteSubRegisterOrMemoryFromRegisterWithBorrow(
    const InstructionContext* ctx);
// SBB AL, imm8
// SBB AX, imm16
extern ExecuteStatus ExecuteSubImmediateFromALOrAXWithBorrow(
    const InstructionContext* ctx);
// DEC AX/CX/DX/BX/SP/BP/SI/DI
extern ExecuteStatus ExecuteDecRegister(const InstructionContext* ctx);

// ============================================================================
// Sign extension instructions - instructions_sign_ext.c
// ============================================================================

// CBW
extern ExecuteStatus ExecuteCbw(const InstructionContext* ctx);
// CWD
extern ExecuteStatus ExecuteCwd(const InstructionContext* ctx);

// ============================================================================
// CMP instructions - instructions_cmp.c
// ============================================================================

// Common logic for CMP instructions. Computes dest - src and sets flags.
extern ExecuteStatus ExecuteCmp(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);

// CMP r/m8, r8
// CMP r/m16, r16
extern ExecuteStatus ExecuteCmpRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// CMP r8, r/m8
// CMP r16, r/m16
extern ExecuteStatus ExecuteCmpRegisterOrMemoryToRegister(
    const InstructionContext* ctx);
// CMP AL, imm8
// CMP AX, imm16
extern ExecuteStatus ExecuteCmpImmediateToALOrAX(const InstructionContext* ctx);

// ============================================================================
// Boolean instructions - instructions_bool.c
// ============================================================================

extern void SetFlagsAfterBooleanInstruction(
    const InstructionContext* ctx, uint32_t result);
// Common logic for AND instructions.
extern ExecuteStatus ExecuteBooleanAnd(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);
// Common logic for OR instructions.
extern ExecuteStatus ExecuteBooleanOr(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);
// Common logic for XOR instructions.
extern ExecuteStatus ExecuteBooleanXor(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);
// Common logic for TEST instructions.
extern ExecuteStatus ExecuteTest(
    const InstructionContext* ctx, Operand* dest, OperandValue* src_value);

// AND r/m8, r8
// AND r/m16, r16
extern ExecuteStatus ExecuteBooleanAndRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// AND r8, r/m8
// AND r16, r/m16
extern ExecuteStatus ExecuteBooleanAndRegisterOrMemoryToRegister(
    const InstructionContext* ctx);
// AND AL, imm8
// AND AX, imm16
extern ExecuteStatus ExecuteBooleanAndImmediateToALOrAX(
    const InstructionContext* ctx);
// OR r/m8, r8
// OR r/m16, r16
extern ExecuteStatus ExecuteBooleanOrRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// OR r8, r/m8
// OR r16, r/m16
extern ExecuteStatus ExecuteBooleanOrRegisterOrMemoryToRegister(
    const InstructionContext* ctx);
// OR AL, imm8
// OR AX, imm16
extern ExecuteStatus ExecuteBooleanOrImmediateToALOrAX(
    const InstructionContext* ctx);
// XOR r/m8, r8
// XOR r/m16, r16
extern ExecuteStatus ExecuteBooleanXorRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// XOR r8, r/m8
// XOR r16, r/m16
extern ExecuteStatus ExecuteBooleanXorRegisterOrMemoryToRegister(
    const InstructionContext* ctx);
// XOR AL, imm8
// XOR AX, imm16
extern ExecuteStatus ExecuteBooleanXorImmediateToALOrAX(
    const InstructionContext* ctx);
// TEST r/m8, r8
// TEST r/m16, r16
extern ExecuteStatus ExecuteTestRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// TEST AL, imm8
// TEST AX, imm16
extern ExecuteStatus ExecuteTestImmediateToALOrAX(
    const InstructionContext* ctx);

// ============================================================================
// Control flow instructions - instructions_ctrl_flow.c
// ============================================================================

// Common logic for far jumps.
extern ExecuteStatus ExecuteFarJump(
    const InstructionContext* ctx, const OperandValue* segment,
    const OperandValue* offset);
// Common logic for far calls.
extern ExecuteStatus ExecuteFarCall(
    const InstructionContext* ctx, const OperandValue* segment,
    const OperandValue* offset);
// Common logic for returning from an interrupt.
extern ExecuteStatus ExecuteReturnFromInterrupt(CPUState* cpu);

// JMP rel8
// JMP rel16
extern ExecuteStatus ExecuteShortOrNearJump(const InstructionContext* ctx);
// JMP ptr16:16
extern ExecuteStatus ExecuteDirectFarJump(const InstructionContext* ctx);
// Unsigned conditional jumps.
extern ExecuteStatus ExecuteUnsignedConditionalJump(
    const InstructionContext* ctx);
// JL/JGNE and JNL/JGE
extern ExecuteStatus ExecuteSignedConditionalJumpJLOrJNL(
    const InstructionContext* ctx);
// JLE/JG and JNLE/JG
extern ExecuteStatus ExecuteSignedConditionalJumpJLEOrJNLE(
    const InstructionContext* ctx);
// LOOP rel8
extern ExecuteStatus ExecuteLoop(const InstructionContext* ctx);
// LOOPZ rel8
// LOOPNZ rel8
extern ExecuteStatus ExecuteLoopZOrNZ(const InstructionContext* ctx);
// JCXZ rel8
extern ExecuteStatus ExecuteJumpIfCXIsZero(const InstructionContext* ctx);
// CALL rel16
extern ExecuteStatus ExecuteDirectNearCall(const InstructionContext* ctx);
// CALL ptr16:16
extern ExecuteStatus ExecuteDirectFarCall(const InstructionContext* ctx);
// RET
extern ExecuteStatus ExecuteNearReturn(const InstructionContext* ctx);
// RET imm16
extern ExecuteStatus ExecuteNearReturnAndPop(const InstructionContext* ctx);
// RETF
extern ExecuteStatus ExecuteFarReturn(const InstructionContext* ctx);
// RETF imm16
extern ExecuteStatus ExecuteFarReturnAndPop(const InstructionContext* ctx);
// IRET
extern ExecuteStatus ExecuteIret(const InstructionContext* ctx);
// INT 3
extern ExecuteStatus ExecuteInt3(const InstructionContext* ctx);
// INTO
extern ExecuteStatus ExecuteInto(const InstructionContext* ctx);
// INT n
extern ExecuteStatus ExecuteIntN(const InstructionContext* ctx);
// HLT
extern ExecuteStatus ExecuteHlt(const InstructionContext* ctx);

// ============================================================================
// Stack instructions - instructions_stack.c
// ============================================================================

// PUSH AX/CX/DX/BX/SP/BP/SI/DI
extern ExecuteStatus ExecutePushRegister(const InstructionContext* ctx);
// POP AX/CX/DX/BX/SP/BP/SI/DI
extern ExecuteStatus ExecutePopRegister(const InstructionContext* ctx);
// PUSH ES/CS/SS/DS
extern ExecuteStatus ExecutePushSegmentRegister(const InstructionContext* ctx);
// POP ES/CS/SS/DS
extern ExecuteStatus ExecutePopSegmentRegister(const InstructionContext* ctx);
// PUSHF
extern ExecuteStatus ExecutePushFlags(const InstructionContext* ctx);
// POPF
extern ExecuteStatus ExecutePopFlags(const InstructionContext* ctx);
// POP r/m16
extern ExecuteStatus ExecutePopRegisterOrMemory(const InstructionContext* ctx);
// LAHF
extern ExecuteStatus ExecuteLoadAHFromFlags(const InstructionContext* ctx);
// SAHF
extern ExecuteStatus ExecuteStoreAHToFlags(const InstructionContext* ctx);

// ============================================================================
// Flag manipulation instructions - instructions_flags.c
// ============================================================================

// CLC, STC, CLI, STI, CLD, STD
extern ExecuteStatus ExecuteClearOrSetFlag(const InstructionContext* ctx);
// CMC
extern ExecuteStatus ExecuteComplementCarryFlag(const InstructionContext* ctx);

// ============================================================================
// IN and OUT instructions - instructions_io.c
// ============================================================================

// IN AL, imm8
// IN AX, imm8
extern ExecuteStatus ExecuteInImmediate(const InstructionContext* ctx);
// IN AL, DX
// IN AX, DX
extern ExecuteStatus ExecuteInDX(const InstructionContext* ctx);
// OUT imm8, AL
// OUT imm8, AX
extern ExecuteStatus ExecuteOutImmediate(const InstructionContext* ctx);
// OUT DX, AL
// OUT DX, AX
extern ExecuteStatus ExecuteOutDX(const InstructionContext* ctx);

// ============================================================================
// String instructions - instructions_string.c
// ============================================================================

// MOVS
extern ExecuteStatus ExecuteMovs(const InstructionContext* ctx);
// STOS
extern ExecuteStatus ExecuteStos(const InstructionContext* ctx);
// LODS
extern ExecuteStatus ExecuteLods(const InstructionContext* ctx);
// SCAS
extern ExecuteStatus ExecuteScas(const InstructionContext* ctx);
// CMPS
extern ExecuteStatus ExecuteCmps(const InstructionContext* ctx);

// ============================================================================
// BCD and ASCII arithmetic instructions - instructions_bcd_ascii.c
// ============================================================================

// AAA
extern ExecuteStatus ExecuteAaa(const InstructionContext* ctx);
// AAS
extern ExecuteStatus ExecuteAas(const InstructionContext* ctx);
// AAM
extern ExecuteStatus ExecuteAam(const InstructionContext* ctx);
// AAD
extern ExecuteStatus ExecuteAad(const InstructionContext* ctx);
// DAA
extern ExecuteStatus ExecuteDaa(const InstructionContext* ctx);
// DAS
extern ExecuteStatus ExecuteDas(const InstructionContext* ctx);

// ============================================================================
// Group 1 instructions - instructions_group_1.c
// ============================================================================

// Group 1 instruction handler.
extern ExecuteStatus ExecuteGroup1Instruction(const InstructionContext* ctx);

// Group 1 instruction handler, but sign-extends the 8-bit immediate value.
extern ExecuteStatus ExecuteGroup1InstructionWithSignExtension(
    const InstructionContext* ctx);

// ============================================================================
// Group 2 instructions - instructions_group_2.c
// ============================================================================

// Group 2 shift / rotate by 1.
extern ExecuteStatus ExecuteGroup2ShiftOrRotateBy1Instruction(
    const InstructionContext* ctx);
// Group 2 shift / rotate by CL.
extern ExecuteStatus ExecuteGroup2ShiftOrRotateByCLInstruction(
    const InstructionContext* ctx);

// ============================================================================
// Group 3 instructions - instructions_group_3.c
// ============================================================================

// Group 3 instruction handler.
extern ExecuteStatus ExecuteGroup3Instruction(const InstructionContext* ctx);

// ============================================================================
// Group 4 instructions - instructions_group_4.c
// ============================================================================

// Group 4 instruction handler.
extern ExecuteStatus ExecuteGroup4Instruction(const InstructionContext* ctx);

// ============================================================================
// Group 5 instructions - instructions_group_5.c
// ============================================================================

// Group 5 instruction handler.
extern ExecuteStatus ExecuteGroup5Instruction(const InstructionContext* ctx);

#endif  // YAX86_IMPLEMENTATION

#endif  // YAX86_CPU_INSTRUCTIONS_H


// ==============================================================================
// src/cpu/instructions.h end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_helpers.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_helpers.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// Set common CPU flags after an instruction. This includes:
// - Zero flag (ZF)
// - Sign flag (SF)
// - Parity Flag (PF)
YAX86_PRIVATE void SetCommonFlagsAfterInstruction(
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

YAX86_PRIVATE void Push(CPUState* cpu, OperandValue value) {
  cpu->registers[kSP] -= 2;
  OperandAddress address = {
      .type = kOperandAddressTypeMemory,
      .value = {
          .memory_address = {
              .segment_register_index = kSS,
              .offset = cpu->registers[kSP],
          }}};
  WriteMemoryWord(cpu, &address, value);
}

YAX86_PRIVATE OperandValue Pop(CPUState* cpu) {
  OperandAddress address = {
      .type = kOperandAddressTypeMemory,
      .value = {
          .memory_address = {
              .segment_register_index = kSS,
              .offset = cpu->registers[kSP],
          }}};
  OperandValue value = ReadMemoryWord(cpu, &address);
  cpu->registers[kSP] += 2;
  return value;
}

// Dummy instruction for unsupported opcodes.
YAX86_PRIVATE ExecuteStatus ExecuteNoOp(const InstructionContext* ctx) {
  (void)ctx;
  return kExecuteSuccess;
}


// ==============================================================================
// src/cpu/instructions_helpers.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_mov.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_mov.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// MOV instructions
// ============================================================================

// MOV r/m8, r8
// MOV r/m16, r16
YAX86_PRIVATE ExecuteStatus
ExecuteMoveRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  WriteOperand(ctx, &dest, FromOperand(&src));
  return kExecuteSuccess;
}

// MOV r8, r/m8
// MOV r16, r/m16
YAX86_PRIVATE ExecuteStatus
ExecuteMoveRegisterOrMemoryToRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  WriteOperand(ctx, &dest, FromOperand(&src));
  return kExecuteSuccess;
}

// MOV r/m16, sreg
YAX86_PRIVATE ExecuteStatus
ExecuteMoveSegmentRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadSegmentRegisterOperand(ctx);
  WriteOperand(ctx, &dest, FromOperand(&src));
  return kExecuteSuccess;
}

// MOV sreg, r/m16
YAX86_PRIVATE ExecuteStatus
ExecuteMoveRegisterOrMemoryToSegmentRegister(const InstructionContext* ctx) {
  Operand dest = ReadSegmentRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  WriteOperand(ctx, &dest, FromOperand(&src));
  return kExecuteSuccess;
}

// MOV AX/CX/DX/BX/SP/BP/SI/DI, imm16
// MOV AH/AL/CH/CL/DH/DL/BH/BL, imm8
YAX86_PRIVATE ExecuteStatus
ExecuteMoveImmediateToRegister(const InstructionContext* ctx) {
  static const uint8_t register_index_opcode_base[kNumWidths] = {
      0xB0,  // kByte
      0xB8,  // kWord
  };
  RegisterIndex register_index =
      (RegisterIndex)(ctx->instruction->opcode -
                      register_index_opcode_base[ctx->metadata->width]);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  OperandValue src_value = ReadImmediate(ctx);
  WriteOperand(ctx, &dest, FromOperandValue(&src_value));
  return kExecuteSuccess;
}

// MOV AL, moffs16
// MOV AX, moffs16
YAX86_PRIVATE ExecuteStatus
ExecuteMoveMemoryOffsetToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  // Offset is always 16 bits, even though the data width of the operation may
  // be 8 bits.
  OperandValue src_offset_value =
      kReadImmediateValueFn[kWord](ctx->instruction);
  OperandAddress src_address = {
      .type = kOperandAddressTypeMemory,
      .value = {
          .memory_address = {
              .segment_register_index = kDS,
              .offset = (uint16_t)FromOperandValue(&src_offset_value),
          }}};
  OperandValue src_value = ReadOperandValue(ctx, &src_address);
  WriteOperand(ctx, &dest, FromOperandValue(&src_value));
  return kExecuteSuccess;
}

// MOV moffs16, AL
// MOV moffs16, AX
YAX86_PRIVATE ExecuteStatus
ExecuteMoveALOrAXToMemoryOffset(const InstructionContext* ctx) {
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
              .offset = (uint16_t)FromOperandValue(&dest_offset_value),
          }}};
  WriteOperandAddress(ctx, &dest_address, FromOperand(&src));
  return kExecuteSuccess;
}

// MOV r/m8, imm8
// MOV r/m16, imm16
YAX86_PRIVATE ExecuteStatus
ExecuteMoveImmediateToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  OperandValue src_value = ReadImmediate(ctx);
  WriteOperand(ctx, &dest, FromOperandValue(&src_value));
  return kExecuteSuccess;
}

// ============================================================================
// XCHG instructions
// ============================================================================

// XCHG AX, AX/CX/DX/BX/SP/BP/SI/DI
YAX86_PRIVATE ExecuteStatus
ExecuteExchangeRegister(const InstructionContext* ctx) {
  RegisterIndex register_index =
      (RegisterIndex)(ctx->instruction->opcode - 0x90);
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
YAX86_PRIVATE ExecuteStatus
ExecuteExchangeRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  uint32_t temp = FromOperand(&dest);
  WriteOperand(ctx, &dest, FromOperand(&src));
  WriteOperand(ctx, &src, temp);
  return kExecuteSuccess;
}

// ============================================================================
// XLAT
// ============================================================================

// XLAT
YAX86_PRIVATE ExecuteStatus
ExecuteTranslateByte(const InstructionContext* ctx) {
  // Read the AL register
  Operand al = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandAddress src_address = {
      .type = kOperandAddressTypeMemory,
      .value =
          {.memory_address =
               {
                   .segment_register_index = kDS,
                   .offset =
                       (uint16_t)(ctx->cpu->registers[kBX] + FromOperand(&al)),
               }},
  };
  OperandValue src_value = ReadMemoryByte(ctx->cpu, &src_address);
  WriteOperandAddress(ctx, &al.address, FromOperandValue(&src_value));
  return kExecuteSuccess;
}


// ==============================================================================
// src/cpu/instructions_mov.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_lea.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_lea.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// LEA instruction
// ============================================================================

// LEA r16, m
YAX86_PRIVATE ExecuteStatus
ExecuteLoadEffectiveAddress(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  MemoryAddress memory_address =
      GetMemoryOperandAddress(ctx->cpu, ctx->instruction);
  uint32_t physical_address = ToPhysicalAddress(ctx->cpu, &memory_address);
  WriteOperandAddress(ctx, &dest.address, physical_address);
  return kExecuteSuccess;
}

// ============================================================================
// LES and LDS instructions
// ============================================================================

// Common logic for LES and LDS instructions.
static ExecuteStatus ExecuteLoadSegmentWithPointer(
    const InstructionContext* ctx, RegisterIndex segment_register_index) {
  Operand destRegister = ReadRegisterOperand(ctx);
  Operand destSegmentRegister =
      ReadRegisterOperandForRegisterIndex(ctx, segment_register_index);

  OperandAddress src_address = {
      .type = kOperandAddressTypeMemory,
      .value = {
          .memory_address = GetMemoryOperandAddress(ctx->cpu, ctx->instruction),
      }};
  OperandValue src_offset_value = ReadMemoryWord(ctx->cpu, &src_address);
  src_address.value.memory_address.offset += 2;
  OperandValue src_segment_value = ReadMemoryWord(ctx->cpu, &src_address);

  WriteOperand(ctx, &destRegister, FromOperandValue(&src_offset_value));
  WriteOperand(ctx, &destSegmentRegister, FromOperandValue(&src_segment_value));
  return kExecuteSuccess;
}

// LES r16, m
YAX86_PRIVATE ExecuteStatus
ExecuteLoadESWithPointer(const InstructionContext* ctx) {
  return ExecuteLoadSegmentWithPointer(ctx, kES);
}

// LDS r16, m
YAX86_PRIVATE ExecuteStatus
ExecuteLoadDSWithPointer(const InstructionContext* ctx) {
  return ExecuteLoadSegmentWithPointer(ctx, kDS);
}


// ==============================================================================
// src/cpu/instructions_lea.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_add.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_add.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

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
static ExecuteStatus ExecuteAddCommon(
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
YAX86_PRIVATE ExecuteStatus ExecuteAdd(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  return ExecuteAddCommon(
      ctx, dest, src_value, /* carry */ false, SetFlagsAfterAdd);
}

// ADD r/m8, r8
// ADD r/m16, r16
YAX86_PRIVATE ExecuteStatus
ExecuteAddRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteAdd(ctx, &dest, &src.value);
}

// ADD r8, r/m8
// ADD r16, r/m16
YAX86_PRIVATE ExecuteStatus
ExecuteAddRegisterOrMemoryToRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteAdd(ctx, &dest, &src.value);
}

// ADD AL, imm8
// ADD AX, imm16
YAX86_PRIVATE ExecuteStatus
ExecuteAddImmediateToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteAdd(ctx, &dest, &src_value);
}

// Common logic for ADC instructions
YAX86_PRIVATE ExecuteStatus ExecuteAddWithCarry(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  return ExecuteAddCommon(
      ctx, dest, src_value, /* carry */ true, SetFlagsAfterAdd);
}

// ADC r/m8, r8
// ADC r/m16, r16
YAX86_PRIVATE ExecuteStatus
ExecuteAddRegisterToRegisterOrMemoryWithCarry(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteAddWithCarry(ctx, &dest, &src.value);
}
// ADC r8, r/m8
// ADC r16, r/m16
YAX86_PRIVATE ExecuteStatus
ExecuteAddRegisterOrMemoryToRegisterWithCarry(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteAddWithCarry(ctx, &dest, &src.value);
}

// ADC AL, imm8
// ADC AX, imm16
YAX86_PRIVATE ExecuteStatus
ExecuteAddImmediateToALOrAXWithCarry(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteAddWithCarry(ctx, &dest, &src_value);
}

// Common logic for INC instructions
YAX86_PRIVATE ExecuteStatus
ExecuteInc(const InstructionContext* ctx, Operand* dest) {
  OperandValue src_value = WordValue(1);
  return ExecuteAddCommon(
      ctx, dest, &src_value, /* carry */ false, SetFlagsAfterInc);
}

// INC AX/CX/DX/BX/SP/BP/SI/DI
YAX86_PRIVATE ExecuteStatus ExecuteIncRegister(const InstructionContext* ctx) {
  RegisterIndex register_index =
      (RegisterIndex)(ctx->instruction->opcode - 0x40);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  return ExecuteInc(ctx, &dest);
}


// ==============================================================================
// src/cpu/instructions_add.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_sub.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_sub.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

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

// Set CPU flags after a SUB, SBB, CMP or NEG instruction.
// This calls SetFlagsAfterDec and then sets the Carry Flag (CF).
YAX86_PRIVATE void SetFlagsAfterSub(
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
static ExecuteStatus ExecuteSubCommon(
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
YAX86_PRIVATE ExecuteStatus ExecuteSub(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  return ExecuteSubCommon(
      ctx, dest, src_value, /* borrow */ false, SetFlagsAfterSub);
}

// SUB r/m8, r8
// SUB r/m16, r16
YAX86_PRIVATE ExecuteStatus
ExecuteSubRegisterFromRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteSub(ctx, &dest, &src.value);
}

// SUB r8, r/m8
// SUB r16, r/m16
YAX86_PRIVATE ExecuteStatus
ExecuteSubRegisterOrMemoryFromRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteSub(ctx, &dest, &src.value);
}

// SUB AL, imm8
// SUB AX, imm16
YAX86_PRIVATE ExecuteStatus
ExecuteSubImmediateFromALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteSub(ctx, &dest, &src_value);
}

// Common logic for SBB instructions
YAX86_PRIVATE ExecuteStatus ExecuteSubWithBorrow(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  return ExecuteSubCommon(
      ctx, dest, src_value, /* borrow */ true, SetFlagsAfterSub);
}

// SBB r/m8, r8
// SBB r/m16, r16
YAX86_PRIVATE ExecuteStatus ExecuteSubRegisterFromRegisterOrMemoryWithBorrow(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteSubWithBorrow(ctx, &dest, &src.value);
}

// SBB r8, r/m8
// SBB r16, r/m16
YAX86_PRIVATE ExecuteStatus ExecuteSubRegisterOrMemoryFromRegisterWithBorrow(
    const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteSubWithBorrow(ctx, &dest, &src.value);
}

// SBB AL, imm8
// SBB AX, imm16
YAX86_PRIVATE ExecuteStatus
ExecuteSubImmediateFromALOrAXWithBorrow(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteSubWithBorrow(ctx, &dest, &src_value);
}

// Common logic for DEC instructions
YAX86_PRIVATE ExecuteStatus
ExecuteDec(const InstructionContext* ctx, Operand* dest) {
  OperandValue src_value = WordValue(1);
  return ExecuteSubCommon(
      ctx, dest, &src_value, /* borrow */ false, SetFlagsAfterDec);
}

// DEC AX/CX/DX/BX/SP/BP/SI/DI
YAX86_PRIVATE ExecuteStatus ExecuteDecRegister(const InstructionContext* ctx) {
  RegisterIndex register_index =
      (RegisterIndex)(ctx->instruction->opcode - 0x48);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  return ExecuteDec(ctx, &dest);
}


// ==============================================================================
// src/cpu/instructions_sub.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_sign_ext.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_sign_ext.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Sign extension instructions
// ============================================================================

// CBW
YAX86_PRIVATE ExecuteStatus ExecuteCbw(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (al & kSignBit[kByte]) ? 0xFF : 0x00;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  return kExecuteSuccess;
}

// CWD
YAX86_PRIVATE ExecuteStatus ExecuteCwd(const InstructionContext* ctx) {
  ctx->cpu->registers[kDX] =
      (ctx->cpu->registers[kAX] & kSignBit[kWord]) ? 0xFFFF : 0x0000;
  return kExecuteSuccess;
}


// ==============================================================================
// src/cpu/instructions_sign_ext.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_cmp.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_cmp.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// CMP instructions
// ============================================================================

// Common logic for CMP instructions. Computes dest - src and sets flags.
YAX86_PRIVATE ExecuteStatus ExecuteCmp(
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
YAX86_PRIVATE ExecuteStatus
ExecuteCmpRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteCmp(ctx, &dest, &src.value);
}

// CMP r8, r/m8
// CMP r16, r/m16
YAX86_PRIVATE ExecuteStatus
ExecuteCmpRegisterOrMemoryToRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteCmp(ctx, &dest, &src.value);
}

// CMP AL, imm8
// CMP AX, imm16
YAX86_PRIVATE ExecuteStatus
ExecuteCmpImmediateToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteCmp(ctx, &dest, &src_value);
}


// ==============================================================================
// src/cpu/instructions_cmp.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_bool.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_bool.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Boolean AND, OR and XOR instructions
// ============================================================================

YAX86_PRIVATE void SetFlagsAfterBooleanInstruction(
    const InstructionContext* ctx, uint32_t result) {
  SetCommonFlagsAfterInstruction(ctx, result);
  // Carry Flag (CF) should be cleared
  SetFlag(ctx->cpu, kCF, false);
  // Overflow Flag (OF) should be cleared
  SetFlag(ctx->cpu, kOF, false);
}

// Common logic for AND instructions.
YAX86_PRIVATE ExecuteStatus ExecuteBooleanAnd(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  uint32_t result = FromOperand(dest) & FromOperandValue(src_value);
  WriteOperand(ctx, dest, result);
  SetFlagsAfterBooleanInstruction(ctx, result);
  return kExecuteSuccess;
}

// AND r/m8, r8
// AND r/m16, r16
YAX86_PRIVATE ExecuteStatus
ExecuteBooleanAndRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteBooleanAnd(ctx, &dest, &src.value);
}

// AND r8, r/m8
// AND r16, r/m16
YAX86_PRIVATE ExecuteStatus
ExecuteBooleanAndRegisterOrMemoryToRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteBooleanAnd(ctx, &dest, &src.value);
}

// AND AL, imm8
// AND AX, imm16
YAX86_PRIVATE ExecuteStatus
ExecuteBooleanAndImmediateToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteBooleanAnd(ctx, &dest, &src_value);
}

// Common logic for OR instructions.
YAX86_PRIVATE ExecuteStatus ExecuteBooleanOr(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  uint32_t result = FromOperand(dest) | FromOperandValue(src_value);
  WriteOperand(ctx, dest, result);
  SetFlagsAfterBooleanInstruction(ctx, result);
  return kExecuteSuccess;
}

// OR r/m8, r8
// OR r/m16, r16
YAX86_PRIVATE ExecuteStatus
ExecuteBooleanOrRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteBooleanOr(ctx, &dest, &src.value);
}

// OR r8, r/m8
// OR r16, r/m16
YAX86_PRIVATE ExecuteStatus
ExecuteBooleanOrRegisterOrMemoryToRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteBooleanOr(ctx, &dest, &src.value);
}

// OR AL, imm8
// OR AX, imm16
YAX86_PRIVATE ExecuteStatus
ExecuteBooleanOrImmediateToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteBooleanOr(ctx, &dest, &src_value);
}

// Common logic for XOR instructions.
YAX86_PRIVATE ExecuteStatus ExecuteBooleanXor(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value) {
  uint32_t result = FromOperand(dest) ^ FromOperandValue(src_value);
  WriteOperand(ctx, dest, result);
  SetFlagsAfterBooleanInstruction(ctx, result);
  return kExecuteSuccess;
}

// XOR r/m8, r8
// XOR r/m16, r16
YAX86_PRIVATE ExecuteStatus
ExecuteBooleanXorRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteBooleanXor(ctx, &dest, &src.value);
}

// XOR r8, r/m8
// XOR r16, r/m16
YAX86_PRIVATE ExecuteStatus
ExecuteBooleanXorRegisterOrMemoryToRegister(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  Operand src = ReadRegisterOrMemoryOperand(ctx);
  return ExecuteBooleanXor(ctx, &dest, &src.value);
}

// XOR AL, imm8
// XOR AX, imm16
YAX86_PRIVATE ExecuteStatus
ExecuteBooleanXorImmediateToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteBooleanXor(ctx, &dest, &src_value);
}

// ============================================================================
// TEST instructions
// ============================================================================

// Common logic for TEST instructions.
YAX86_PRIVATE ExecuteStatus ExecuteTest(
    const InstructionContext* ctx, Operand* dest, OperandValue* src_value) {
  uint32_t result = FromOperand(dest) & FromOperandValue(src_value);
  SetFlagsAfterBooleanInstruction(ctx, result);
  return kExecuteSuccess;
}

// TEST r/m8, r8
// TEST r/m16, r16
YAX86_PRIVATE ExecuteStatus
ExecuteTestRegisterToRegisterOrMemory(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  Operand src = ReadRegisterOperand(ctx);
  return ExecuteTest(ctx, &dest, &src.value);
}

// TEST AL, imm8
// TEST AX, imm16
YAX86_PRIVATE ExecuteStatus
ExecuteTestImmediateToALOrAX(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteTest(ctx, &dest, &src_value);
}


// ==============================================================================
// src/cpu/instructions_bool.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_ctrl_flow.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_ctrl_flow.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// JMP instructions
// ============================================================================

// Jump to a relative signed byte offset.
static ExecuteStatus ExecuteRelativeJumpByte(
    const InstructionContext* ctx, const OperandValue* offset_value) {
  ctx->cpu->registers[kIP] = AddSignedOffsetByte(
      ctx->cpu->registers[kIP], FromOperandValue(offset_value));
  return kExecuteSuccess;
}

// Jump to a relative signed word offset.
static ExecuteStatus ExecuteRelativeJumpWord(
    const InstructionContext* ctx, const OperandValue* offset_value) {
  ctx->cpu->registers[kIP] = AddSignedOffsetWord(
      ctx->cpu->registers[kIP], FromOperandValue(offset_value));
  return kExecuteSuccess;
}

// Table of relative jump instructions, indexed by width.
static ExecuteStatus (*const kRelativeJumpFn[kNumWidths])(
    const InstructionContext* ctx, const OperandValue* offset_value) = {
    ExecuteRelativeJumpByte,  // kByte
    ExecuteRelativeJumpWord,  // kWord
};

// Common logic for JMP instructions.
static ExecuteStatus ExecuteRelativeJump(
    const InstructionContext* ctx, const OperandValue* offset_value) {
  return kRelativeJumpFn[ctx->metadata->width](ctx, offset_value);
}

// JMP rel8
// JMP rel16
YAX86_PRIVATE ExecuteStatus
ExecuteShortOrNearJump(const InstructionContext* ctx) {
  OperandValue offset_value = ReadImmediate(ctx);
  return ExecuteRelativeJump(ctx, &offset_value);
}

// Common logic for far jumps.
YAX86_PRIVATE ExecuteStatus ExecuteFarJump(
    const InstructionContext* ctx, const OperandValue* segment,
    const OperandValue* offset) {
  ctx->cpu->registers[kCS] = FromOperandValue(segment);
  ctx->cpu->registers[kIP] = FromOperandValue(offset);
  return kExecuteSuccess;
}

// JMP ptr16:16
YAX86_PRIVATE ExecuteStatus
ExecuteDirectFarJump(const InstructionContext* ctx) {
  OperandValue new_cs = WordValue(
      ((uint16_t)ctx->instruction->immediate[2]) |
      (((uint16_t)ctx->instruction->immediate[3]) << 8));
  OperandValue new_ip = WordValue(
      ((uint16_t)ctx->instruction->immediate[0]) |
      (((uint16_t)ctx->instruction->immediate[1]) << 8));
  return ExecuteFarJump(ctx, &new_cs, &new_ip);
}

// ============================================================================
// Conditional jumps
// ============================================================================

// Common logic for conditional jumps.
static ExecuteStatus ExecuteConditionalJump(
    const InstructionContext* ctx, bool value, bool success_value) {
  if (value == success_value) {
    OperandValue offset_value = ReadImmediate(ctx);
    return ExecuteRelativeJump(ctx, &offset_value);
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
YAX86_PRIVATE ExecuteStatus
ExecuteUnsignedConditionalJump(const InstructionContext* ctx) {
  uint16_t flag_mask = kUnsignedConditionalJumpFlagBitmasks
      [(ctx->instruction->opcode - 0x70) / 2];
  bool flag_value = (ctx->cpu->flags & flag_mask) != 0;
  // Even opcode => jump if the flag is set
  // Odd opcode => jump if the flag is not set
  bool success_value = ((ctx->instruction->opcode & 0x1) == 0);
  return ExecuteConditionalJump(ctx, flag_value, success_value);
}

// JL/JGNE and JNL/JGE
YAX86_PRIVATE ExecuteStatus
ExecuteSignedConditionalJumpJLOrJNL(const InstructionContext* ctx) {
  const bool is_greater_or_equal =
      GetFlag(ctx->cpu, kSF) == GetFlag(ctx->cpu, kOF);
  const bool success_value = (ctx->instruction->opcode & 0x1);
  return ExecuteConditionalJump(ctx, is_greater_or_equal, success_value);
}

// JLE/JG and JNLE/JG
YAX86_PRIVATE ExecuteStatus
ExecuteSignedConditionalJumpJLEOrJNLE(const InstructionContext* ctx) {
  const bool is_greater = !GetFlag(ctx->cpu, kZF) &&
                          (GetFlag(ctx->cpu, kSF) == GetFlag(ctx->cpu, kOF));
  const bool success_value = (ctx->instruction->opcode & 0x1);
  return ExecuteConditionalJump(ctx, is_greater, success_value);
}

// ============================================================================
// Loop instructions
// ============================================================================

// LOOP rel8
YAX86_PRIVATE ExecuteStatus ExecuteLoop(const InstructionContext* ctx) {
  return ExecuteConditionalJump(ctx, --(ctx->cpu->registers[kCX]) != 0, true);
}

// LOOPZ rel8
// LOOPNZ rel8
YAX86_PRIVATE ExecuteStatus ExecuteLoopZOrNZ(const InstructionContext* ctx) {
  bool condition1 = --(ctx->cpu->registers[kCX]) != 0;
  bool condition2 =
      GetFlag(ctx->cpu, kZF) == (bool)(ctx->instruction->opcode - 0xE0);
  return ExecuteConditionalJump(ctx, condition1 && condition2, true);
}

// JCXZ rel8
YAX86_PRIVATE ExecuteStatus
ExecuteJumpIfCXIsZero(const InstructionContext* ctx) {
  return ExecuteConditionalJump(ctx, ctx->cpu->registers[kCX] == 0, true);
}

// ============================================================================
// CALL and RET instructions
// ============================================================================

// Common logic for near calls.
static ExecuteStatus ExecuteNearCall(
    const InstructionContext* ctx, const OperandValue* offset) {
  Push(ctx->cpu, WordValue(ctx->cpu->registers[kIP]));
  return ExecuteRelativeJump(ctx, offset);
}

// CALL rel16
YAX86_PRIVATE ExecuteStatus
ExecuteDirectNearCall(const InstructionContext* ctx) {
  OperandValue offset = ReadImmediate(ctx);
  return ExecuteNearCall(ctx, &offset);
}

// Common logic for far calls.
YAX86_PRIVATE ExecuteStatus ExecuteFarCall(
    const InstructionContext* ctx, const OperandValue* segment,
    const OperandValue* offset) {
  // Push the current CS and IP onto the stack.
  Push(ctx->cpu, WordValue(ctx->cpu->registers[kCS]));
  Push(ctx->cpu, WordValue(ctx->cpu->registers[kIP]));
  return ExecuteFarJump(ctx, segment, offset);
}

// CALL ptr16:16
YAX86_PRIVATE ExecuteStatus
ExecuteDirectFarCall(const InstructionContext* ctx) {
  Push(ctx->cpu, WordValue(ctx->cpu->registers[kCS]));
  Push(ctx->cpu, WordValue(ctx->cpu->registers[kIP]));
  return ExecuteDirectFarJump(ctx);
}

// Common logic for RET instructions.
static ExecuteStatus ExecuteNearReturnCommon(
    const InstructionContext* ctx, uint16_t arg_size) {
  OperandValue new_ip = Pop(ctx->cpu);
  ctx->cpu->registers[kIP] = FromOperandValue(&new_ip);
  ctx->cpu->registers[kSP] += arg_size;
  return kExecuteSuccess;
}

// RET
YAX86_PRIVATE ExecuteStatus ExecuteNearReturn(const InstructionContext* ctx) {
  return ExecuteNearReturnCommon(ctx, 0);
}

// RET imm16
YAX86_PRIVATE ExecuteStatus
ExecuteNearReturnAndPop(const InstructionContext* ctx) {
  OperandValue arg_size_value = ReadImmediate(ctx);
  return ExecuteNearReturnCommon(ctx, FromOperandValue(&arg_size_value));
}

// Common logic for RETF instructions.
static ExecuteStatus ExecuteFarReturnCommon(
    const InstructionContext* ctx, uint16_t arg_size) {
  OperandValue new_ip = Pop(ctx->cpu);
  OperandValue new_cs = Pop(ctx->cpu);
  ctx->cpu->registers[kIP] = FromOperandValue(&new_ip);
  ctx->cpu->registers[kCS] = FromOperandValue(&new_cs);
  ctx->cpu->registers[kSP] += arg_size;
  return kExecuteSuccess;
}

// RETF
YAX86_PRIVATE ExecuteStatus ExecuteFarReturn(const InstructionContext* ctx) {
  return ExecuteFarReturnCommon(ctx, 0);
}

// RETF imm16
YAX86_PRIVATE ExecuteStatus
ExecuteFarReturnAndPop(const InstructionContext* ctx) {
  OperandValue arg_size_value = ReadImmediate(ctx);
  return ExecuteFarReturnCommon(ctx, FromOperandValue(&arg_size_value));
}

// ============================================================================
// Interrupt instructions
// ============================================================================

// Common logic for returning from an interrupt.
YAX86_PRIVATE ExecuteStatus ExecuteReturnFromInterrupt(CPUState* cpu) {
  OperandValue ip_value = Pop(cpu);
  cpu->registers[kIP] = FromOperandValue(&ip_value);
  OperandValue cs_value = Pop(cpu);
  cpu->registers[kCS] = FromOperandValue(&cs_value);
  OperandValue flags_value = Pop(cpu);
  cpu->flags = FromOperandValue(&flags_value);
  return kExecuteSuccess;
}

// IRET
YAX86_PRIVATE ExecuteStatus ExecuteIret(const InstructionContext* ctx) {
  return ExecuteReturnFromInterrupt(ctx->cpu);
}

// INT 3
YAX86_PRIVATE ExecuteStatus ExecuteInt3(const InstructionContext* ctx) {
  SetPendingInterrupt(ctx->cpu, kInterruptBreakpoint);
  return kExecuteSuccess;
}

// INTO
YAX86_PRIVATE ExecuteStatus ExecuteInto(const InstructionContext* ctx) {
  if (GetFlag(ctx->cpu, kOF)) {
    SetPendingInterrupt(ctx->cpu, kInterruptOverflow);
  }
  return kExecuteSuccess;
}

// INT n
YAX86_PRIVATE ExecuteStatus ExecuteIntN(const InstructionContext* ctx) {
  OperandValue interrupt_number_value = ReadImmediate(ctx);
  SetPendingInterrupt(ctx->cpu, FromOperandValue(&interrupt_number_value));
  return kExecuteSuccess;
}

// HLT
YAX86_PRIVATE ExecuteStatus ExecuteHlt(const InstructionContext* ctx) {
  (void)ctx;
  return kExecuteHalt;
}


// ==============================================================================
// src/cpu/instructions_ctrl_flow.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_stack.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_stack.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// PUSH and POP instructions
// ============================================================================

// PUSH AX/CX/DX/BX/SP/BP/SI/DI
YAX86_PRIVATE ExecuteStatus ExecutePushRegister(const InstructionContext* ctx) {
  RegisterIndex register_index =
      (RegisterIndex)(ctx->instruction->opcode - 0x50);
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  Push(ctx->cpu, src.value);
  return kExecuteSuccess;
}

// POP AX/CX/DX/BX/SP/BP/SI/DI
YAX86_PRIVATE ExecuteStatus ExecutePopRegister(const InstructionContext* ctx) {
  RegisterIndex register_index =
      (RegisterIndex)(ctx->instruction->opcode - 0x58);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  OperandValue value = Pop(ctx->cpu);
  WriteOperandAddress(ctx, &dest.address, FromOperandValue(&value));
  return kExecuteSuccess;
}

// PUSH ES/CS/SS/DS
YAX86_PRIVATE ExecuteStatus
ExecutePushSegmentRegister(const InstructionContext* ctx) {
  RegisterIndex register_index =
      (RegisterIndex)(((ctx->instruction->opcode >> 3) & 0x03) + 8);
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  Push(ctx->cpu, src.value);
  return kExecuteSuccess;
}

// POP ES/CS/SS/DS
YAX86_PRIVATE ExecuteStatus
ExecutePopSegmentRegister(const InstructionContext* ctx) {
  RegisterIndex register_index =
      (RegisterIndex)(((ctx->instruction->opcode >> 3) & 0x03) + 8);
  // Special case - disallow POP CS
  if (register_index == kCS) {
    return kExecuteInvalidInstruction;
  }
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, register_index);
  OperandValue value = Pop(ctx->cpu);
  WriteOperandAddress(ctx, &dest.address, FromOperandValue(&value));
  return kExecuteSuccess;
}

// PUSHF
YAX86_PRIVATE ExecuteStatus ExecutePushFlags(const InstructionContext* ctx) {
  Push(ctx->cpu, WordValue(ctx->cpu->flags));
  return kExecuteSuccess;
}

// POPF
YAX86_PRIVATE ExecuteStatus ExecutePopFlags(const InstructionContext* ctx) {
  OperandValue value = Pop(ctx->cpu);
  ctx->cpu->flags = FromOperandValue(&value);
  return kExecuteSuccess;
}

// POP r/m16
YAX86_PRIVATE ExecuteStatus
ExecutePopRegisterOrMemory(const InstructionContext* ctx) {
  if (ctx->instruction->mod_rm.reg != 0) {
    return kExecuteInvalidInstruction;
  }
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  OperandValue value = Pop(ctx->cpu);
  WriteOperandAddress(ctx, &dest.address, FromOperandValue(&value));
  return kExecuteSuccess;
}

// ============================================================================
// LAHF and SAHF
// ============================================================================

// Returns the AH register address.
static const OperandAddress* GetAHRegisterAddress(void) {
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
YAX86_PRIVATE ExecuteStatus
ExecuteLoadAHFromFlags(const InstructionContext* ctx) {
  WriteRegisterByte(
      ctx->cpu, GetAHRegisterAddress(), ByteValue(ctx->cpu->flags & 0x00FF));
  return kExecuteSuccess;
}

// SAHF
YAX86_PRIVATE ExecuteStatus
ExecuteStoreAHToFlags(const InstructionContext* ctx) {
  OperandValue value = ReadRegisterByte(ctx->cpu, GetAHRegisterAddress());
  // Clear the lower byte of flags and set it to the value in AH
  ctx->cpu->flags = (ctx->cpu->flags & 0xFF00) | value.value.byte_value;
  return kExecuteSuccess;
}


// ==============================================================================
// src/cpu/instructions_stack.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_flags.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_flags.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// CLC, STC, CLI, STI, CLD, STD instructions
// ============================================================================

// Table of flags corresponding to the CLC, STC, CLI, STI, CLD, and STD
// instructions, indexed by (opcode - 0xF8) / 2.
static const Flag kFlagsForClearAndSetInstructions[] = {
    kCF,  // CLC, STC
    kIF,  // CLI, STI
    kDF,  // CLD, STD
};

YAX86_PRIVATE ExecuteStatus ExecuteClearOrSetFlag(const InstructionContext* ctx) {
  uint8_t opcode_index = ctx->instruction->opcode - 0xF8;
  Flag flag = kFlagsForClearAndSetInstructions[opcode_index / 2];
  bool value = (opcode_index & 0x1) != 0;
  SetFlag(ctx->cpu, flag, value);
  return kExecuteSuccess;
}

// ============================================================================
// CMC instruction
// ============================================================================

// CMC
YAX86_PRIVATE ExecuteStatus ExecuteComplementCarryFlag(const InstructionContext* ctx) {
  SetFlag(ctx->cpu, kCF, !GetFlag(ctx->cpu, kCF));
  return kExecuteSuccess;
}


// ==============================================================================
// src/cpu/instructions_flags.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_io.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_io.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
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

// Read a word from an I/O port as a uint16_t.
static OperandValue ReadWordFromPort(CPUState* cpu, uint16_t port) {
  uint8_t low = ReadByteFromPort(cpu, port).value.byte_value;
  uint8_t high = ReadByteFromPort(cpu, port).value.byte_value;
  return WordValue((high << 8) | low);
}

// Table of functions to read from an I/O port, indexed by data width.
static OperandValue (*const kReadFromPortFns[])(CPUState*, uint16_t) = {
    ReadByteFromPort,  // kByte
    ReadWordFromPort,  // kWord
};

// Common logic for IN instructions.
static ExecuteStatus ExecuteIn(const InstructionContext* ctx, uint16_t port) {
  OperandValue value = kReadFromPortFns[ctx->metadata->width](ctx->cpu, port);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  WriteOperand(ctx, &dest, FromOperandValue(&value));
  return kExecuteSuccess;
}

// IN AL, imm8
// IN AX, imm8
YAX86_PRIVATE ExecuteStatus ExecuteInImmediate(const InstructionContext* ctx) {
  OperandValue port = ReadImmediateByte(ctx->instruction);
  return ExecuteIn(ctx, FromOperandValue(&port));
}

// IN AL, DX
// IN AX, DX
YAX86_PRIVATE ExecuteStatus ExecuteInDX(const InstructionContext* ctx) {
  return ExecuteIn(ctx, ctx->cpu->registers[kDX]);
}

// Write a byte to an I/O port.
static void WriteByteToPort(CPUState* cpu, uint16_t port, OperandValue value) {
  if (!cpu->config->write_port) {
    return;
  }
  cpu->config->write_port(cpu, port, FromOperandValue(&value));
}

// Write a word to an I/O port.
static void WriteWordToPort(CPUState* cpu, uint16_t port, OperandValue value) {
  uint32_t raw_value = FromOperandValue(&value);
  WriteByteToPort(cpu, port, ByteValue(raw_value & 0xFF));
  WriteByteToPort(cpu, port, ByteValue((raw_value >> 8) & 0xFF));
}

// Table of functions to write to an I/O port, indexed by data width.
static void (*const kWriteToPortFns[])(CPUState*, uint16_t, OperandValue) = {
    WriteByteToPort,  // kByte
    WriteWordToPort,  // kWord
};

// Common logic for OUT instructions.
static ExecuteStatus ExecuteOut(const InstructionContext* ctx, uint16_t port) {
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  kWriteToPortFns[ctx->metadata->width](ctx->cpu, port, src.value);
  return kExecuteSuccess;
}

// OUT imm8, AL
// OUT imm8, AX
YAX86_PRIVATE ExecuteStatus ExecuteOutImmediate(const InstructionContext* ctx) {
  OperandValue port = ReadImmediateByte(ctx->instruction);
  return ExecuteOut(ctx, FromOperandValue(&port));
}

// OUT DX, AL
// OUT DX, AX
YAX86_PRIVATE ExecuteStatus ExecuteOutDX(const InstructionContext* ctx) {
  return ExecuteOut(ctx, ctx->cpu->registers[kDX]);
}


// ==============================================================================
// src/cpu/instructions_io.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_string.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_string.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// String instructions
// ============================================================================

// Get the repetition prefix of a string instruction, if any.
static uint8_t GetRepetitionPrefix(const InstructionContext* ctx) {
  uint8_t prefix = 0;
  for (int i = 0; i < ctx->instruction->prefix_size; ++i) {
    switch (ctx->instruction->prefix[i]) {
      case kPrefixREP:
      case kPrefixREPNZ:
        prefix = ctx->instruction->prefix[i];
        break;
      default:
        continue;
    }
  }
  return prefix;
}

// Get the source operand for string instructions. Typically DS:SI but can be
// overridden by a segment override prefix.
static Operand GetStringSourceOperand(const InstructionContext* ctx) {
  OperandAddress address = {
      .type = kOperandAddressTypeMemory,
      .value =
          {
              .memory_address =
                  {
                      .segment_register_index = kDS,
                      .offset = ctx->cpu->registers[kSI],
                  },
          },
  };
  ApplySegmentOverride(ctx->instruction, &address.value.memory_address);
  Operand operand = {
      .address = address,
      .value = ReadOperandValue(ctx, &address),
  };
  return operand;
}

// Get the destination operand address for string instructions. Always ES:DI.
static OperandAddress GetStringDestinationOperandAddress(
    const InstructionContext* ctx) {
  OperandAddress address = {
      .type = kOperandAddressTypeMemory,
      .value =
          {
              .memory_address =
                  {
                      .segment_register_index = kES,
                      .offset = ctx->cpu->registers[kDI],
                  },
          },
  };
  return address;
}

// Get the destination operand for string instructions. Always ES:DI.
static Operand GetStringDestinationOperand(const InstructionContext* ctx) {
  OperandAddress address = GetStringDestinationOperandAddress(ctx);
  Operand operand = {
      .address = address,
      .value = ReadOperandValue(ctx, &address),
  };
  return operand;
}

// Update the source address register (SI) after a string operation.
static void UpdateStringSourceAddress(const InstructionContext* ctx) {
  if (GetFlag(ctx->cpu, kDF)) {
    ctx->cpu->registers[kSI] -= kNumBytes[ctx->metadata->width];
  } else {
    ctx->cpu->registers[kSI] += kNumBytes[ctx->metadata->width];
  }
}

// Update the destination address register (DI) after a string operation.
static void UpdateStringDestinationAddress(const InstructionContext* ctx) {
  if (GetFlag(ctx->cpu, kDF)) {
    ctx->cpu->registers[kDI] -= kNumBytes[ctx->metadata->width];
  } else {
    ctx->cpu->registers[kDI] += kNumBytes[ctx->metadata->width];
  }
}

// Execute a string instruction with optional REP prefix.
static ExecuteStatus ExecuteStringInstructionWithREPPrefix(
    const InstructionContext* ctx,
    ExecuteStatus (*fn)(const InstructionContext*)) {
  uint8_t prefix = GetRepetitionPrefix(ctx);
  if (prefix != kPrefixREP) {
    return fn(ctx);
  }
  while (ctx->cpu->registers[kCX]) {
    ExecuteStatus status = fn(ctx);
    if (status != kExecuteSuccess) {
      return status;
    }
    --ctx->cpu->registers[kCX];
  }
  return kExecuteSuccess;
}

// Single MOVS iteration.
static ExecuteStatus ExecuteMovsIteration(const InstructionContext* ctx) {
  Operand src = GetStringSourceOperand(ctx);
  OperandAddress dest_address = GetStringDestinationOperandAddress(ctx);
  WriteOperandAddress(ctx, &dest_address, FromOperand(&src));
  UpdateStringSourceAddress(ctx);
  UpdateStringDestinationAddress(ctx);
  return kExecuteSuccess;
}

// MOVS
YAX86_PRIVATE ExecuteStatus ExecuteMovs(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPPrefix(ctx, ExecuteMovsIteration);
}

// Single STOS iteration.
static ExecuteStatus ExecuteStosIteration(const InstructionContext* ctx) {
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandAddress dest_address = GetStringDestinationOperandAddress(ctx);
  WriteOperandAddress(ctx, &dest_address, FromOperand(&src));
  UpdateStringDestinationAddress(ctx);
  return kExecuteSuccess;
}

// STOS
YAX86_PRIVATE ExecuteStatus ExecuteStos(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPPrefix(ctx, ExecuteStosIteration);
}

// Single LODS iteration.
static ExecuteStatus ExecuteLodsIteration(const InstructionContext* ctx) {
  Operand src = GetStringSourceOperand(ctx);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  WriteOperand(ctx, &dest, FromOperand(&src));
  UpdateStringSourceAddress(ctx);
  return kExecuteSuccess;
}

// LODS
YAX86_PRIVATE ExecuteStatus ExecuteLods(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPPrefix(ctx, ExecuteLodsIteration);
}

// Execute a string instruction with optional REPZ/REPE or REPNZ/REPNE prefix.
static ExecuteStatus ExecuteStringInstructionWithREPZOrRepNZPrefix(
    const InstructionContext* ctx,
    ExecuteStatus (*fn)(const InstructionContext*)) {
  uint8_t prefix = GetRepetitionPrefix(ctx);
  if (prefix != kPrefixREP && prefix != kPrefixREPNZ) {
    return fn(ctx);
  }
  bool terminate_zf_value = prefix == kPrefixREPNZ;
  while (ctx->cpu->registers[kCX]) {
    ExecuteStatus status = fn(ctx);
    if (status != kExecuteSuccess) {
      return status;
    }
    --ctx->cpu->registers[kCX];
    if (GetFlag(ctx->cpu, kZF) == terminate_zf_value) {
      break;
    }
  }
  return kExecuteSuccess;
}

// Single SCAS iteration.
static ExecuteStatus ExecuteScasIteration(const InstructionContext* ctx) {
  Operand src = GetStringDestinationOperand(ctx);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  ExecuteCmp(ctx, &dest, &src.value);
  UpdateStringDestinationAddress(ctx);
  return kExecuteSuccess;
}

// SCAS
YAX86_PRIVATE ExecuteStatus ExecuteScas(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPZOrRepNZPrefix(
      ctx, ExecuteScasIteration);
}

// Single CMPS iteration.
static ExecuteStatus ExecuteCmpsIteration(const InstructionContext* ctx) {
  Operand dest = GetStringSourceOperand(ctx);
  Operand src = GetStringDestinationOperand(ctx);
  ExecuteCmp(ctx, &dest, &src.value);
  UpdateStringSourceAddress(ctx);
  UpdateStringDestinationAddress(ctx);
  return kExecuteSuccess;
}

// CMPS
YAX86_PRIVATE ExecuteStatus ExecuteCmps(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPZOrRepNZPrefix(
      ctx, ExecuteCmpsIteration);
}


// ==============================================================================
// src/cpu/instructions_string.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_bcd_ascii.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_bcd_ascii.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// BCD and ASCII arithmetic instructions
// ============================================================================

// AAA
YAX86_PRIVATE ExecuteStatus ExecuteAaa(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  uint8_t al_low = al & 0x0F;
  if (al_low > 9 || GetFlag(ctx->cpu, kAF)) {
    al += 6;
    ++ah;
    SetFlag(ctx->cpu, kAF, true);
    SetFlag(ctx->cpu, kCF, true);
  } else {
    SetFlag(ctx->cpu, kAF, false);
    SetFlag(ctx->cpu, kCF, false);
  }
  al &= 0x0F;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  return kExecuteSuccess;
}

// AAS
YAX86_PRIVATE ExecuteStatus ExecuteAas(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  uint8_t al_low = al & 0x0F;
  if (al_low > 9 || GetFlag(ctx->cpu, kAF)) {
    al -= 6;
    --ah;
    SetFlag(ctx->cpu, kAF, true);
    SetFlag(ctx->cpu, kCF, true);
  } else {
    SetFlag(ctx->cpu, kAF, false);
    SetFlag(ctx->cpu, kCF, false);
  }
  al &= 0x0F;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  return kExecuteSuccess;
}

// AAM
YAX86_PRIVATE ExecuteStatus ExecuteAam(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  OperandValue base = ReadImmediate(ctx);
  uint16_t base_value = FromOperandValue(&base);
  if (base_value == 0) {
    return kExecuteInvalidInstruction;
  }
  uint8_t ah = al / base_value;
  al %= base_value;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  SetCommonFlagsAfterInstruction(ctx, al);
  return kExecuteSuccess;
}

// AAD
YAX86_PRIVATE ExecuteStatus ExecuteAad(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  OperandValue base = ReadImmediate(ctx);
  uint8_t base_value = FromOperandValue(&base);
  al += ah * base_value;
  ah = 0;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  SetCommonFlagsAfterInstruction(ctx, al);
  return kExecuteSuccess;
}

// DAA
YAX86_PRIVATE ExecuteStatus ExecuteDaa(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  uint8_t al_low = al & 0x0F;
  if (al_low > 9 || GetFlag(ctx->cpu, kAF)) {
    al += 6;
    SetFlag(ctx->cpu, kAF, true);
  } else {
    SetFlag(ctx->cpu, kAF, false);
  }
  uint8_t al_high = (al >> 4) & 0x0F;
  if (al_high > 9 || GetFlag(ctx->cpu, kCF)) {
    al += 0x60;
    SetFlag(ctx->cpu, kCF, true);
  } else {
    SetFlag(ctx->cpu, kCF, false);
  }
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  SetCommonFlagsAfterInstruction(ctx, al);
  return kExecuteSuccess;
}

// DAS
YAX86_PRIVATE ExecuteStatus ExecuteDas(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[kAX] >> 8) & 0xFF;
  uint8_t al_low = al & 0x0F;
  if (al_low > 9 || GetFlag(ctx->cpu, kAF)) {
    al -= 6;
    SetFlag(ctx->cpu, kAF, true);
  } else {
    SetFlag(ctx->cpu, kAF, false);
  }
  uint8_t al_high = (al >> 4) & 0x0F;
  if (al_high > 9 || GetFlag(ctx->cpu, kCF)) {
    al -= 0x60;
    SetFlag(ctx->cpu, kCF, true);
  } else {
    SetFlag(ctx->cpu, kCF, false);
  }
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  SetCommonFlagsAfterInstruction(ctx, al);
  return kExecuteSuccess;
}


// ==============================================================================
// src/cpu/instructions_bcd_ascii.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_group_1.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_group_1.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 1 - ADD, OR, ADC, SBB, AND, SUB, XOR, CMP
// ============================================================================

typedef ExecuteStatus (*Group1ExecuteInstructionFn)(
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
YAX86_PRIVATE ExecuteStatus ExecuteGroup1Instruction(const InstructionContext* ctx) {
  const Group1ExecuteInstructionFn fn =
      kGroup1ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  OperandValue src_value = ReadImmediate(ctx);
  return fn(ctx, &dest, &src_value);
}

// Group 1 instruction handler, but sign-extends the 8-bit immediate value.
YAX86_PRIVATE ExecuteStatus ExecuteGroup1InstructionWithSignExtension(
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


// ==============================================================================
// src/cpu/instructions_group_1.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_group_2.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_group_2.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 2 - ROL, ROR, RCL, RCR, SHL, SHR, SAL, SAR
// ============================================================================

typedef ExecuteStatus (*Group2ExecuteInstructionFn)(
    const InstructionContext* ctx, Operand* op, uint8_t count);

// SHL r/m8, 1
// SHL r/m16, 1
// SHL r/m8, CL
// SHL r/m16, CL
static ExecuteStatus ExecuteGroup2Shl(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kExecuteSuccess;
  }
  uint32_t value = FromOperand(op);
  uint32_t result = (value << count) & kMaxValue[ctx->metadata->width];
  WriteOperand(ctx, op, result);
  bool last_msb =
      ((value << (count - 1)) & kSignBit[ctx->metadata->width]) != 0;
  SetFlag(ctx->cpu, kCF, last_msb);
  if (count == 1) {
    bool current_msb = ((result & kSignBit[ctx->metadata->width]) != 0);
    SetFlag(ctx->cpu, kOF, last_msb != current_msb);
  }
  SetCommonFlagsAfterInstruction(ctx, result);
  return kExecuteSuccess;
}

// SHR r/m8, 1
// SHR r/m16, 1
// SHR r/m8, CL
// SHR r/m16, CL
static ExecuteStatus ExecuteGroup2Shr(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kExecuteSuccess;
  }
  uint32_t value = FromOperand(op);
  uint32_t result = value >> count;
  WriteOperand(ctx, op, result);
  bool last_lsb = ((value >> (count - 1)) & 1) != 0;
  SetFlag(ctx->cpu, kCF, last_lsb);
  if (count == 1) {
    bool original_msb = ((value & kSignBit[ctx->metadata->width]) != 0);
    SetFlag(ctx->cpu, kOF, original_msb);
  }
  SetCommonFlagsAfterInstruction(ctx, result);
  return kExecuteSuccess;
}

// SAR r/m8, 1
// SAR r/m16, 1
// SAR r/m8, CL
// SAR r/m16, CL
static ExecuteStatus ExecuteGroup2Sar(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kExecuteSuccess;
  }
  int32_t value = FromSignedOperand(op);
  int32_t result = value >> count;
  WriteOperand(ctx, op, result);
  bool last_lsb = ((value >> (count - 1)) & 1) != 0;
  SetFlag(ctx->cpu, kCF, last_lsb);
  if (count == 1) {
    SetFlag(ctx->cpu, kOF, false);
  }
  SetCommonFlagsAfterInstruction(ctx, result);
  return kExecuteSuccess;
}

// ROL r/m8, 1
// ROL r/m16, 1
// ROL r/m8, CL
// ROL r/m16, CL
static ExecuteStatus ExecuteGroup2Rol(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kExecuteSuccess;
  }
  // The 8086 computes the modulus of the count after the zero check, which is
  // different than the 80286 and later processors.
  uint8_t effective_count = count % kNumBits[ctx->metadata->width];
  uint32_t value = FromOperand(op);
  uint32_t result =
      (value << effective_count) |
      (value >> (kNumBits[ctx->metadata->width] - effective_count));
  WriteOperand(ctx, op, result);
  bool last_msb = (result & 1) != 0;
  SetFlag(ctx->cpu, kCF, last_msb);
  if (count == 1) {
    bool current_msb = ((result & kSignBit[ctx->metadata->width]) != 0);
    SetFlag(ctx->cpu, kOF, last_msb != current_msb);
  }
  return kExecuteSuccess;
}

// ROR r/m8, 1
// ROR r/m16, 1
// ROR r/m8, CL
// ROR r/m16, CL
static ExecuteStatus ExecuteGroup2Ror(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return kExecuteSuccess;
  }
  // The 8086 computes the modulus of the count after the zero check, which is
  // different than the 80286 and later processors.
  uint8_t effective_count = count % kNumBits[ctx->metadata->width];
  uint32_t value = FromOperand(op);
  uint32_t result =
      (value >> effective_count) |
      (value << (kNumBits[ctx->metadata->width] - effective_count));
  WriteOperand(ctx, op, result);
  bool last_lsb = (result & kSignBit[ctx->metadata->width]) != 0;
  SetFlag(ctx->cpu, kCF, last_lsb);
  if (count == 1) {
    bool original_msb = ((value & kSignBit[ctx->metadata->width]) != 0);
    SetFlag(ctx->cpu, kOF, last_lsb != original_msb);
  }
  return kExecuteSuccess;
}

// RCL r/m8, 1
// RCL r/m16, 1
// RCL r/m8, CL
// RCL r/m16, CL
static ExecuteStatus ExecuteGroup2Rcl(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  uint8_t effective_count = count % (kNumBits[ctx->metadata->width] + 1);
  if (effective_count == 0) {
    return kExecuteSuccess;
  }
  uint32_t cf_value = GetFlag(ctx->cpu, kCF) ? (1 << (effective_count - 1)) : 0;
  uint32_t value = FromOperand(op);
  uint32_t result =
      (value << effective_count) | cf_value |
      (value >> (kNumBits[ctx->metadata->width] - (effective_count - 1)));
  WriteOperand(ctx, op, result);
  bool last_msb =
      ((value << (effective_count - 1)) & kSignBit[ctx->metadata->width]) != 0;
  SetFlag(ctx->cpu, kCF, last_msb);
  if (count == 1) {
    bool current_msb = ((result & kSignBit[ctx->metadata->width]) != 0);
    SetFlag(ctx->cpu, kOF, last_msb != current_msb);
  }
  return kExecuteSuccess;
}

// RCR r/m8, 1
// RCR r/m16, 1
// RCR r/m8, CL
// RCR r/m16, CL
static ExecuteStatus ExecuteGroup2Rcr(
    const InstructionContext* ctx, Operand* op, uint8_t count) {
  uint8_t effective_count = count % (kNumBits[ctx->metadata->width] + 1);
  if (effective_count == 0) {
    return kExecuteSuccess;
  }
  uint32_t cf_value =
      GetFlag(ctx->cpu, kCF)
          ? (kSignBit[ctx->metadata->width] >> (effective_count - 1))
          : 0;
  uint32_t value = FromOperand(op);
  uint32_t result =
      (value >> effective_count) | cf_value |
      (value << (kNumBits[ctx->metadata->width] - (effective_count - 1)));
  WriteOperand(ctx, op, result);
  bool last_lsb = ((value >> (effective_count - 1)) & 1) != 0;
  SetFlag(ctx->cpu, kCF, last_lsb);
  if (count == 1) {
    bool original_msb = ((value & kSignBit[ctx->metadata->width]) != 0);
    bool current_msb = ((result & kSignBit[ctx->metadata->width]) != 0);
    SetFlag(ctx->cpu, kOF, current_msb != original_msb);
  }
  return kExecuteSuccess;
}

static const Group2ExecuteInstructionFn kGroup2ExecuteInstructionFns[] = {
    ExecuteGroup2Rol,  // 0 - ROL
    ExecuteGroup2Ror,  // 1 - ROR
    ExecuteGroup2Rcl,  // 2 - RCL
    ExecuteGroup2Rcr,  // 3 - RCR
    ExecuteGroup2Shl,  // 4 - SHL
    ExecuteGroup2Shr,  // 5 - SHR
    ExecuteGroup2Shl,  // 6 - SAL (same as SHL)
    ExecuteGroup2Sar,  // 7 - SAR
};

// Group 2 shift / rotate by 1.
YAX86_PRIVATE ExecuteStatus
ExecuteGroup2ShiftOrRotateBy1Instruction(const InstructionContext* ctx) {
  const Group2ExecuteInstructionFn fn =
      kGroup2ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand op = ReadRegisterOrMemoryOperand(ctx);
  return fn(ctx, &op, 1);
}

// Group 2 shift / rotate by CL.
YAX86_PRIVATE ExecuteStatus
ExecuteGroup2ShiftOrRotateByCLInstruction(const InstructionContext* ctx) {
  const Group2ExecuteInstructionFn fn =
      kGroup2ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand op = ReadRegisterOrMemoryOperand(ctx);
  return fn(ctx, &op, ctx->cpu->registers[kCX] & 0xFF);
}


// ==============================================================================
// src/cpu/instructions_group_2.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_group_3.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_group_3.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 3 - TEST, NOT, NEG, MUL, IMUL, DIV, IDIV
// ============================================================================

typedef ExecuteStatus (*Group3ExecuteInstructionFn)(
    const InstructionContext* ctx, Operand* op);

// TEST r/m8, imm8
// TEST r/m16, imm16
static ExecuteStatus ExecuteGroup3Test(
    const InstructionContext* ctx, Operand* op) {
  OperandValue src_value = ReadImmediate(ctx);
  return ExecuteTest(ctx, op, &src_value);
}

// NOT r/m8
// NOT r/m16
static ExecuteStatus ExecuteNot(const InstructionContext* ctx, Operand* op) {
  WriteOperand(ctx, op, ~FromOperand(op));
  return kExecuteSuccess;
}

// NEG r/m8
// NEG r/m16
static ExecuteStatus ExecuteNeg(const InstructionContext* ctx, Operand* op) {
  int32_t op_value = FromSignedOperand(op);
  int32_t result_value = -op_value;
  WriteOperand(ctx, op, result_value);
  SetFlagsAfterSub(ctx, 0, op_value, result_value, false);
  return kExecuteSuccess;
}

// Table of where to store the higher half of the result for
// MUL, IMUL, DIV, and IDIV instructions, indexed by the data width.
static const OperandAddress kMulDivResultHighHalfAddress[kNumWidths] = {
    {.type = kOperandAddressTypeRegister,
     .value =
         {
             .register_address =
                 {
                     .register_index = kAX,
                     .byte_offset = 8,
                 },
         }},
    {.type = kOperandAddressTypeRegister,
     .value = {
         .register_address =
             {
                 .register_index = kDX,
             },
     }}};

// Number of bits to shift to extract the high part of the result of MUL, IMUL,
// DIV, and IDIV instructions, indexed by the data width.
static const uint8_t kMulDivResultHighHalfShiftWidth[kNumWidths] = {
    8,   // kByte
    16,  // kWord
};

// Common logic for MUL and IMUL instructions.
static ExecuteStatus ExecuteMulCommon(
    const InstructionContext* ctx, Operand* dest, uint32_t result,
    bool overflow) {
  Width width = ctx->metadata->width;

  uint32_t result_low_half = result & kMaxValue[width];
  WriteOperand(ctx, dest, result_low_half);

  uint32_t result_high_half =
      (result >> kMulDivResultHighHalfShiftWidth[width]) & kMaxValue[width];
  WriteOperandAddress(
      ctx, &kMulDivResultHighHalfAddress[width], result_high_half);

  SetFlag(ctx->cpu, kCF, overflow);
  SetFlag(ctx->cpu, kOF, overflow);

  return kExecuteSuccess;
}

// MUL r/m8
// MUL r/m16
static ExecuteStatus ExecuteMul(const InstructionContext* ctx, Operand* op) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  uint32_t result = FromOperand(&dest) * FromOperand(op);
  return ExecuteMulCommon(
      ctx, &dest, result, result > kMaxValue[ctx->metadata->width]);
}

// IMUL r/m8
// IMUL r/m16
static ExecuteStatus ExecuteImul(const InstructionContext* ctx, Operand* op) {
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  int32_t result = FromSignedOperand(&dest) * FromSignedOperand(op);
  return ExecuteMulCommon(
      ctx, &dest, result,
      result > kMaxSignedValue[ctx->metadata->width] ||
          result < kMinSignedValue[ctx->metadata->width]);
}

static ExecuteStatus WriteDivResult(
    const InstructionContext* ctx, Operand* dest, uint32_t quotient,
    uint32_t remainder) {
  WriteOperand(ctx, dest, quotient);
  WriteOperandAddress(
      ctx, &kMulDivResultHighHalfAddress[ctx->metadata->width], remainder);
  return kExecuteSuccess;
}

// DIV r/m8
// DIV r/m16
static ExecuteStatus ExecuteDiv(const InstructionContext* ctx, Operand* op) {
  uint32_t divisor = FromOperand(op);
  if (divisor == 0) {
    SetPendingInterrupt(ctx->cpu, kInterruptDivideError);
    return kExecuteSuccess;
  }

  Width width = ctx->metadata->width;
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);

  OperandValue dest_high_half =
      ReadOperandValue(ctx, &kMulDivResultHighHalfAddress[width]);
  uint32_t dividend =
      FromOperand(&dest) | (FromOperandValue(&dest_high_half)
                            << kMulDivResultHighHalfShiftWidth[width]);
  uint32_t quotient = dividend / divisor;
  if (quotient > kMaxValue[ctx->metadata->width]) {
    SetPendingInterrupt(ctx->cpu, kInterruptDivideError);
    return kExecuteSuccess;
  }
  return WriteDivResult(ctx, &dest, quotient, dividend % divisor);
}

// IDIV r/m8
// IDIV r/m16
static ExecuteStatus ExecuteIdiv(const InstructionContext* ctx, Operand* op) {
  int32_t divisor = FromSignedOperand(op);
  if (divisor == 0) {
    SetPendingInterrupt(ctx->cpu, kInterruptDivideError);
    return kExecuteSuccess;
  }

  Width width = ctx->metadata->width;
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);

  OperandValue dest_high_half =
      ReadOperandValue(ctx, &kMulDivResultHighHalfAddress[width]);
  int32_t dividend =
      FromOperand(&dest) | (FromSignedOperandValue(&dest_high_half)
                            << kMulDivResultHighHalfShiftWidth[width]);
  int32_t quotient = dividend / divisor;
  if (quotient > kMaxSignedValue[ctx->metadata->width] ||
      quotient < kMinSignedValue[ctx->metadata->width]) {
    SetPendingInterrupt(ctx->cpu, kInterruptDivideError);
    return kExecuteSuccess;
  }
  return WriteDivResult(ctx, &dest, quotient, dividend % divisor);
}

// Group 3 instruction implementations, indexed by the corresponding REG field
// value in the ModRM byte and data width.
static const Group3ExecuteInstructionFn kGroup3ExecuteInstructionFns[] = {
    ExecuteGroup3Test,  // 0 - TEST
    0,                  // 1 - Reserved
    ExecuteNot,         // 2 - NOT
    ExecuteNeg,         // 3 - NEG
    ExecuteMul,         // 4 - MUL
    ExecuteImul,        // 5 - IMUL
    ExecuteDiv,         // 6 - DIV
    ExecuteIdiv,        // 7 - IDIV
};

// Group 3 instruction handler.
YAX86_PRIVATE ExecuteStatus
ExecuteGroup3Instruction(const InstructionContext* ctx) {
  const Group3ExecuteInstructionFn fn =
      kGroup3ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  if (fn == 0) {
    return kExecuteInvalidOpcode;
  }
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  return fn(ctx, &dest);
}


// ==============================================================================
// src/cpu/instructions_group_3.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_group_4.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_group_4.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 4 - INC, DEC
// ============================================================================

typedef ExecuteStatus (*Group4ExecuteInstructionFn)(
    const InstructionContext* ctx, Operand* dest);

// Group 4 instruction implementations, indexed by the corresponding REG field
// value in the ModRM byte.
static const Group4ExecuteInstructionFn kGroup4ExecuteInstructionFns[] = {
    ExecuteInc,  // 0 - INC
    ExecuteDec,  // 1 - DEC
};

// Group 4 instruction handler.
YAX86_PRIVATE ExecuteStatus ExecuteGroup4Instruction(const InstructionContext* ctx) {
  const Group4ExecuteInstructionFn fn =
      kGroup4ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  return fn(ctx, &dest);
}


// ==============================================================================
// src/cpu/instructions_group_4.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_group_5.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_group_5.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 5 - INC, DEC, CALL, JMP, PUSH
// ============================================================================

// Helper to get the segment register value for far JMP and CALL instructions.
static Operand GetSegmentRegisterOperandForIndirectFarJumpOrCall(
    const InstructionContext* ctx, const Operand* offset) {
  OperandAddress segment_address = offset->address;
  segment_address.value.memory_address.offset += 2;  // Skip the offset
  OperandValue segment_value = ReadMemoryWord(ctx->cpu, &segment_address);
  Operand operand = {
      .address = segment_address,
      .value = segment_value,
  };
  return operand;
}

// JMP ptr16
static ExecuteStatus ExecuteIndirectNearJump(
    const InstructionContext* ctx, Operand* dest) {
  ctx->cpu->registers[kIP] = FromOperandValue(&dest->value);
  return kExecuteSuccess;
}

// CALL ptr16
static ExecuteStatus ExecuteIndirectNearCall(
    const InstructionContext* ctx, Operand* dest) {
  Push(ctx->cpu, WordValue(ctx->cpu->registers[kIP]));
  return ExecuteIndirectNearJump(ctx, dest);
}

// CALL ptr16:16
static ExecuteStatus ExecuteIndirectFarCall(
    const InstructionContext* ctx, Operand* dest) {
  Operand segment =
      GetSegmentRegisterOperandForIndirectFarJumpOrCall(ctx, dest);
  return ExecuteFarCall(ctx, &segment.value, &dest->value);
}

// JMP ptr16:16
static ExecuteStatus ExecuteIndirectFarJump(
    const InstructionContext* ctx, Operand* dest) {
  Operand segment =
      GetSegmentRegisterOperandForIndirectFarJumpOrCall(ctx, dest);
  return ExecuteFarJump(ctx, &segment.value, &dest->value);
}

// PUSH r/m16
static ExecuteStatus ExecuteIndirectPush(
    const InstructionContext* ctx, Operand* dest) {
  Push(ctx->cpu, dest->value);
  return kExecuteSuccess;
}

typedef ExecuteStatus (*Group5ExecuteInstructionFn)(
    const InstructionContext* ctx, Operand* dest);

// Group 5 instruction implementations, indexed by the corresponding REG
// field value in the ModRM byte.
static const Group5ExecuteInstructionFn kGroup5ExecuteInstructionFns[] = {
    ExecuteInc,               // 0 - INC r/m8/r/m16
    ExecuteDec,               // 1 - DEC r/m8/r/m16
    ExecuteIndirectNearCall,  // 2 - CALL rel16
    ExecuteIndirectFarCall,   // 3 - CALL ptr16:16
    ExecuteIndirectNearJump,  // 4 - JMP ptr16
    ExecuteIndirectFarJump,   // 5 - JMP ptr16:16
    ExecuteIndirectPush,      // 6 - PUSH r/m16
                              // 7 - Reserved
};

// Group 5 instruction handler.
YAX86_PRIVATE ExecuteStatus
ExecuteGroup5Instruction(const InstructionContext* ctx) {
  const Group5ExecuteInstructionFn fn =
      kGroup5ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  return fn(ctx, &dest);
}


// ==============================================================================
// src/cpu/instructions_group_5.c end
// ==============================================================================

// ==============================================================================
// src/cpu/opcode_table.c start
// ==============================================================================

#line 1 "./src/cpu/opcode_table.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Opcode table
// ============================================================================

// Global opcode metadata lookup table.
YAX86_PRIVATE OpcodeMetadata opcode_table[256] = {
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
    // 0x0F - UNSUPPORTED
    {.opcode = 0x0F, .handler = 0},
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
    // ES prefix - 0x26
    {.opcode = 0x26, .handler = 0},
    // DAA
    {.opcode = 0x27,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteDaa},
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
    // CS prefix - 0x2E
    {.opcode = 0x2E, .handler = 0},
    // DAS
    {.opcode = 0x2F,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteDas},
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
    // SS prefix - 0x36
    {.opcode = 0x36, .handler = 0},
    // AAA
    {.opcode = 0x37,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteAaa},
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
    // DS prefix - 0x3E
    {.opcode = 0x3E, .handler = 0},
    // AAS
    {.opcode = 0x3F,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteAas},
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
    // 0x60 - 0x6F - UNSUPPORTED
    {.opcode = 0x60, .handler = 0},
    {.opcode = 0x61, .handler = 0},
    {.opcode = 0x62, .handler = 0},
    {.opcode = 0x63, .handler = 0},
    {.opcode = 0x64, .handler = 0},
    {.opcode = 0x65, .handler = 0},
    {.opcode = 0x66, .handler = 0},
    {.opcode = 0x67, .handler = 0},
    {.opcode = 0x68, .handler = 0},
    {.opcode = 0x69, .handler = 0},
    {.opcode = 0x6A, .handler = 0},
    {.opcode = 0x6B, .handler = 0},
    {.opcode = 0x6C, .handler = 0},
    {.opcode = 0x6D, .handler = 0},
    {.opcode = 0x6E, .handler = 0},
    {.opcode = 0x6F, .handler = 0},
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
    // POP r/m16
    {.opcode = 0x8F,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecutePopRegisterOrMemory},
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
    {.opcode = 0x98,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteCbw},
    // CWD
    {.opcode = 0x99,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteCwd},
    // CALL ptr16:16 (4 bytes: 2 for offset, 2 for segment)
    {.opcode = 0x9A,
     .has_modrm = false,
     .immediate_size = 4,
     .width = kWord,
     .handler = ExecuteDirectFarCall},
    // WAIT
    {.opcode = 0x9B,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteNoOp},
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
    {.opcode = 0xA4,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteMovs},
    // MOVSW
    {.opcode = 0xA5,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteMovs},
    // CMPSB
    {.opcode = 0xA6,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteCmps},
    // CMPSW
    {.opcode = 0xA7,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteCmps},
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
    {.opcode = 0xAA,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteStos},
    // STOSW
    {.opcode = 0xAB,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteStos},
    // LODSB
    {.opcode = 0xAC,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteLods},
    // LODSW
    {.opcode = 0xAD,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteLods},
    // SCASB
    {.opcode = 0xAE,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteScas},
    // SCASW
    {.opcode = 0xAF,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteScas},
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
    // 0xC0 - UNSUPPORTED
    {.opcode = 0xC0, .handler = 0},
    // 0xC1 - UNSUPPORTED
    {.opcode = 0xC1, .handler = 0},
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
    {.opcode = 0xC4,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteLoadESWithPointer},
    // LDS r16, m32
    {.opcode = 0xC5,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteLoadDSWithPointer},
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
    // 0xC8 - UNSUPPORTED
    {.opcode = 0xC8, .handler = 0},
    // 0xC9 - UNSUPPORTED
    {.opcode = 0xC9, .handler = 0},
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
    {.opcode = 0xCC,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteInt3},
    // INT imm8
    {.opcode = 0xCD,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteIntN},
    // INTO
    {.opcode = 0xCE,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteInto},
    // IRET
    {.opcode = 0xCF,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteIret},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m8, 1 (Group 2)
    {.opcode = 0xD0,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteGroup2ShiftOrRotateBy1Instruction},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m16, 1 (Group 2)
    {.opcode = 0xD1,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteGroup2ShiftOrRotateBy1Instruction},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m8, CL (Group 2)
    {.opcode = 0xD2,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteGroup2ShiftOrRotateByCLInstruction},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m16, CL (Group 2)
    {.opcode = 0xD3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteGroup2ShiftOrRotateByCLInstruction},
    // AAM
    {.opcode = 0xD4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteAam},
    // AAD
    {.opcode = 0xD5,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteAad},
    // 0xD6 - UNSUPPORTED
    {.opcode = 0xD6, .handler = 0},
    // XLAT/XLATB
    {.opcode = 0xD7,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteTranslateByte},
    // ESC instruction 0xD8 for 8087 numeric coprocessor
    {.opcode = 0xD8,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = ExecuteNoOp},
    // ESC instruction 0xD9 for 8087 numeric coprocessor
    {.opcode = 0xD9,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = ExecuteNoOp},
    // ESC instruction 0xDA for 8087 numeric coprocessor
    {.opcode = 0xDA,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = ExecuteNoOp},
    // ESC instruction 0xDB for 8087 numeric coprocessor
    {.opcode = 0xDB,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = ExecuteNoOp},
    // ESC instruction 0xDC for 8087 numeric coprocessor
    {.opcode = 0xDC,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = ExecuteNoOp},
    // ESC instruction 0xDD for 8087 numeric coprocessor
    {.opcode = 0xDD,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = ExecuteNoOp},
    // ESC instruction 0xDE for 8087 numeric coprocessor
    {.opcode = 0xDE,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = ExecuteNoOp},
    // ESC instruction 0xDF for 8087 numeric coprocessor
    {.opcode = 0xDF,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = ExecuteNoOp},
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
    {.opcode = 0xE4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteInImmediate},
    // IN AX, imm8
    {.opcode = 0xE5,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kWord,
     .handler = ExecuteInImmediate},
    // OUT imm8, AL
    {.opcode = 0xE6,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteOutImmediate},
    // OUT imm8, AX
    {.opcode = 0xE7,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kWord,
     .handler = ExecuteOutImmediate},
    // CALL rel16
    {.opcode = 0xE8,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteDirectNearCall},
    // JMP rel16
    {.opcode = 0xE9,
     .has_modrm = false,
     .immediate_size = 2,
     .width = kWord,
     .handler = ExecuteShortOrNearJump},
    // JMP ptr16:16 (4 bytes: 2 for offset, 2 for segment)
    {.opcode = 0xEA,
     .has_modrm = false,
     .immediate_size = 4,
     .width = kWord,
     .handler = ExecuteDirectFarJump},
    // JMP rel8
    {.opcode = 0xEB,
     .has_modrm = false,
     .immediate_size = 1,
     .width = kByte,
     .handler = ExecuteShortOrNearJump},
    // IN AL, DX
    {.opcode = 0xEC,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteInDX},
    // IN AX, DX
    {.opcode = 0xED,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteInDX},
    // OUT DX, AL
    {.opcode = 0xEE,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteOutDX},
    // OUT DX, AX
    {.opcode = 0xEF,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteOutDX},
    // 0xF0 - LOCK prefix
    {.opcode = 0xF0, .handler = 0},
    // 0xF1 - UNSUPPORTED
    {.opcode = 0xF1, .handler = 0},
    // 0xF2 - REPNE prefix
    {.opcode = 0xF2, .handler = 0},
    // 0xF3 - REP/REPE prefix
    {.opcode = 0xF3, .handler = 0},
    // HLT
    {.opcode = 0xF4,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteHlt},
    // CMC
    {.opcode = 0xF5,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = ExecuteComplementCarryFlag},
    // TEST/NOT/NEG/MUL/IMUL/DIV/IDIV r/m8 (Group 3)
    // The immediate size depends on the ModR/M byte.
    {.opcode = 0xF6,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteGroup3Instruction},
    // TEST/NOT/NEG/MUL/IMUL/DIV/IDIV r/m16 (Group 3)
    // The immediate size depends on the ModR/M byte.
    {.opcode = 0xF7,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteGroup3Instruction},
    // CLC
    {.opcode = 0xF8,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteClearOrSetFlag},
    // STC
    {.opcode = 0xF9,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteClearOrSetFlag},
    // CLI
    {.opcode = 0xFA,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteClearOrSetFlag},
    // STI
    {.opcode = 0xFB,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteClearOrSetFlag},
    // CLD
    {.opcode = 0xFC,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteClearOrSetFlag},
    // STD
    {.opcode = 0xFD,
     .has_modrm = false,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteClearOrSetFlag},
    // INC/DEC r/m8 (Group 4)
    {.opcode = 0xFE,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kByte,
     .handler = ExecuteGroup4Instruction},
    // INC/DEC/CALL/JMP/PUSH r/m16 (Group 5)
    {.opcode = 0xFF,
     .has_modrm = true,
     .immediate_size = 0,
     .width = kWord,
     .handler = ExecuteGroup5Instruction},
};


// ==============================================================================
// src/cpu/opcode_table.c end
// ==============================================================================

// ==============================================================================
// src/cpu/cpu.c start
// ==============================================================================

#line 1 "./src/cpu/cpu.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "public.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// CPU state
// ============================================================================

void InitCPU(CPUState* cpu) {
  // Zero out the CPU state
  const CPUState zero_cpu_state = {0};
  *cpu = zero_cpu_state;
  cpu->flags = kInitialFlags;
}

// ============================================================================
// Instruction decoding
// ============================================================================

// Helper to check if a byte is a valid prefix
static bool IsPrefixByte(uint8_t byte) {
  static const uint8_t kPrefixBytes[] = {
      kPrefixES,   kPrefixCS,    kPrefixSS,  kPrefixDS,
      kPrefixLOCK, kPrefixREPNZ, kPrefixREP,
  };
  for (uint8_t i = 0; i < sizeof(kPrefixBytes); ++i) {
    if (byte == kPrefixBytes[i]) {
      return true;
    }
  }
  return false;
}

// Helper to read the next instruction byte.
static uint8_t ReadNextInstructionByte(CPUState* cpu, uint16_t* ip) {
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
static uint8_t GetDisplacementSize(uint8_t mod, uint8_t rm) {
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
static uint8_t GetImmediateSize(const OpcodeMetadata* metadata, uint8_t reg) {
  switch (metadata->opcode) {
    // TEST r/m8, imm8
    case 0xF6:
    // TEST r/m16, imm16
    case 0xF7:
      return reg == 0 ? metadata->opcode - 0xF5 : 0;
    default:
      return metadata->immediate_size;
  }
}

FetchNextInstructionStatus FetchNextInstruction(
    CPUState* cpu, Instruction* dest_instruction) {
  Instruction instruction = {0};
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

// ============================================================================
// Execution
// ============================================================================

ExecuteStatus ExecuteInstruction(CPUState* cpu, Instruction* instruction) {
  ExecuteStatus status;

  // Run the on_before_execute_instruction callback if provided.
  if (cpu->config->on_before_execute_instruction &&
      (status = cpu->config->on_before_execute_instruction(cpu, instruction)) !=
          kExecuteSuccess) {
    return status;
  }

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

  // Run the instruction handler.
  InstructionContext context = {
      .cpu = cpu,
      .instruction = instruction,
      .metadata = metadata,
  };
  if ((status = metadata->handler(&context)) != kExecuteSuccess) {
    return status;
  }

  // Run the on_after_execute_instruction callback if provided.
  if (cpu->config->on_after_execute_instruction &&
      (status = cpu->config->on_after_execute_instruction(cpu, instruction)) !=
          kExecuteSuccess) {
    return status;
  }

  return kExecuteSuccess;
}

// Process pending interrupt, if any.
static ExecuteStatus ExecutePendingInterrupt(CPUState* cpu) {
  if (!cpu->has_pending_interrupt) {
    return kExecuteSuccess;
  }
  uint8_t interrupt_number = cpu->pending_interrupt_number;
  ClearPendingInterrupt(cpu);

  // Prepare for interrupt processing.
  Push(cpu, WordValue(cpu->flags));
  SetFlag(cpu, kIF, false);
  SetFlag(cpu, kTF, false);
  Push(cpu, WordValue(cpu->registers[kCS]));
  Push(cpu, WordValue(cpu->registers[kIP]));

  // Invoke the interrupt handler callback first. If the caller did not provide
  // an interrupt handler callback, handle the interrupt within the VM using the
  // Interrupt Vector Table.
  ExecuteStatus interrupt_handler_status =
      cpu->config->handle_interrupt
          ? cpu->config->handle_interrupt(cpu, interrupt_number)
          : kExecuteUnhandledInterrupt;

  switch (interrupt_handler_status) {
    case kExecuteSuccess: {
      // If the interrupt was handled by the caller-provided interrupt handler
      // callback, restore state and continue execution.
      return ExecuteReturnFromInterrupt(cpu);
    }
    case kExecuteUnhandledInterrupt: {
      // If the interrupt was not handled by the caller-provided interrupt
      // handler callback, handle it within the VM using the Interrupt Vector
      // Table.
      uint16_t ivt_entry_offset = interrupt_number << 2;
      cpu->registers[kIP] = ReadRawMemoryWord(cpu, ivt_entry_offset);
      cpu->registers[kCS] = ReadRawMemoryWord(cpu, ivt_entry_offset + 2);
      return kExecuteSuccess;
    }
    default:
      // If the interrupt handler returned an error, return it.
      return interrupt_handler_status;
  }
}

ExecuteStatus RunInstructionCycle(CPUState* cpu) {
  // Step 1: Fetch the next instruction, and increment IP.
  Instruction instruction;
  FetchNextInstructionStatus fetch_status =
      FetchNextInstruction(cpu, &instruction);
  if (fetch_status != kFetchSuccess) {
    return kExecuteInvalidInstruction;
  }
  cpu->registers[kIP] += instruction.size;

  // Step 2: Execute the instruction.
  ExecuteStatus status;
  if ((status = ExecuteInstruction(cpu, &instruction)) != kExecuteSuccess) {
    return status;
  }

  // Step 3: Handle pending interrupts.
  if ((status = ExecutePendingInterrupt(cpu)) != kExecuteSuccess) {
    return status;
  }

  // Step 4: If trap flag is set, handle single-step execution.
  if (GetFlag(cpu, kTF)) {
    SetPendingInterrupt(cpu, kInterruptSingleStep);
    if ((status = ExecutePendingInterrupt(cpu)) != kExecuteSuccess) {
      return status;
    }
  }

  return kExecuteSuccess;
}

ExecuteStatus RunMainLoop(CPUState* cpu) {
  ExecuteStatus status;
  for (;;) {
    if ((status = RunInstructionCycle(cpu)) != kExecuteSuccess) {
      return status;
    }
  }
}


// ==============================================================================
// src/cpu/cpu.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_CPU_BUNDLE_H

