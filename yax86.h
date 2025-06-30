// 8086 CPU emulator.
#ifndef YAX86_H
#define YAX86_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

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

#define kNumRegisters (kIP + 1)

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

// CPU flags value on reset.
#define kInitialFlags (1 << 1)  // Reserved_1 is always 1.

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

// Set pending interrupt.
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

// Maximum number of prefix bytes supported. On the 8086 and 80186, the length
// of prefix bytes was actually unlimited. But well-formed code generated by
// compilers would only have 1 or 2 bytes.
#define kMaxPrefixBytes 2
// Maximum number of displacement bytes in an 8086 instruction.
#define kMaxDisplacementBytes 2
// Maximum number of immediate data bytes in an 8086 instruction.
#define kMaxImmediateBytes 4

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

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // YAX86_H
