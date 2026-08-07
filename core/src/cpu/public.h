// Public interface for the CPU emulator module.
#ifndef YAX86_CPU_PUBLIC_H
#define YAX86_CPU_PUBLIC_H

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_CPU_BUNDLE_H
#include "../util/log.h"
#endif  // YAX86_CPU_BUNDLE_H

enum {
  // Log module ID for the CPU.
  kLogModuleIDCPU = 1,
};

// Log module for the CPU.
static const LogModule kLogModuleCPU = {
    .id = kLogModuleIDCPU,
    .name = "CPU",
};

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

// Result of executing a single instruction.
typedef enum InstructionResult {
  // The instruction was executed.
  kInstructionExecuted = 0,
  // The instruction could not be executed, because its opcode has no handler or
  // because its encoding does not match the expected format for its opcode.
  kInstructionInvalid,
} InstructionResult;

// Result of a single CPU tick.
typedef enum CPUTickResult {
  // An instruction was executed. This includes HLT: the tick that halts the
  // CPU still ran an instruction.
  kCPUTickExecuted = 0,
  // The instruction at CS:IP could not be fetched or executed. The CPU is left
  // pointing past the offending instruction; it is up to the caller to decide
  // whether to continue.
  kCPUTickInvalid,
  // The CPU was already halted and executed no instruction this tick. It stays
  // halted until an interrupt wakes it, so the caller must keep ticking the
  // rest of the machine.
  kCPUTickHalted,
  // Execution was stopped part way through the tick via CPURequestStop().
  kCPUTickStopped,
} CPUTickResult;

// Result of the handle_interrupt callback, directing how the CPU should
// proceed with an interrupt.
typedef enum InterruptHandlerResult {
  // The callback serviced the interrupt itself. The CPU restores the state
  // saved on entry and resumes at the interrupted instruction.
  kInterruptHandlerHandled = 0,
  // The callback did not service the interrupt. The CPU dispatches it through
  // the Interrupt Vector Table instead.
  kInterruptHandlerUnhandled,
} InterruptHandlerResult;

struct CPUState;
struct Instruction;

// Caller-provided runtime configuration.
typedef struct CPUConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger for this module. May be NULL.
  Logger* logger;

  // Callback to read a byte from memory.
  //
  // On the 8086, accessing an invalid memory address will yield garbage data
  // rather than causing a page fault. This callback interface mirrors that
  // behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  uint8_t (*read_memory_byte)(struct CPUState* cpu, uint32_t address);

  // Callback to write a byte to memory.
  //
  // On the 8086, accessing an invalid memory address will yield garbage data
  // rather than causing a page fault. This callback interface mirrors that
  // behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  void (*write_memory_byte)(
      struct CPUState* cpu, uint32_t address, uint8_t value);

  // Callback to handle an interrupt. If NULL, every interrupt is dispatched
  // through the Interrupt Vector Table.
  InterruptHandlerResult (*handle_interrupt)(
      struct CPUState* cpu, uint8_t interrupt_number);

  // Callback invoked before executing an instruction. This can be used to
  // inspect or modify the instruction before it is executed, or to inject a
  // pending interrupt. To stop execution, call CPURequestStop().
  void (*on_before_execute_instruction)(
      struct CPUState* cpu, struct Instruction* instruction);

  // Callback invoked after executing an instruction. This can be used to
  // inspect the instruction after it is executed, or to inject a pending
  // interrupt. To stop execution, call CPURequestStop().
  void (*on_after_execute_instruction)(
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
  // Pointer to caller-provided runtime configuration
  CPUConfig* config;

  // Register values
  uint16_t registers[kNumRegisters];
  // Flag values
  uint16_t flags;

  // An internal interrupt, raised by the CPU itself as a result of the
  // instruction it just executed: INT n, INT 3, INTO, a divide error, or a
  // single-step trap. IF does not gate these - INT 21h works with interrupts
  // disabled, which is how DOS calls inside critical sections behave.
  bool has_pending_internal_interrupt;
  // Interrupt number of the pending internal interrupt.
  uint8_t pending_internal_interrupt_number;

  // The INTR pin: a maskable interrupt request from an external interrupt
  // controller, which supplies the vector number during the acknowledge cycle.
  //
  // This is held separately from an internal interrupt because the two have
  // genuinely independent sources. On real hardware INTR is a line the
  // controller holds asserted until the CPU acknowledges it, so an instruction
  // that raises an interrupt of its own cannot make the request go away.
  bool is_intr_asserted;
  // Vector number the interrupt controller supplied along with the request.
  uint8_t intr_vector;

  // Whether the CPU is in halted state. When true, CPUTick() will not fetch
  // or execute any instructions until an external event (e.g., an interrupt)
  // clears this state.
  bool is_halted;

  // Whether a stop has been requested during the current tick. See
  // CPURequestStop().
  bool stop_requested;
} CPUState;

// Initialize CPU state.
void CPUInit(CPUState* cpu, CPUConfig* config);

// Get the value of a CPU flag.
static inline bool CPUGetFlag(const CPUState* cpu, Flag flag) {
  return (cpu->flags & flag) != 0;
}
// Set a CPU flag.
static inline void CPUSetFlag(CPUState* cpu, Flag flag, bool value) {
  if (value) {
    cpu->flags |= flag;
  } else {
    cpu->flags &= ~flag;
  }
}

// Raise an interrupt from within the CPU, to be taken at the end of the
// instruction currently executing. This is for the sources the 8086 calls
// internal - INT n, INT 3, INTO, a divide error, a single-step trap - which
// are not maskable by IF. External requests arrive via CPUAssertINTR().
static inline void CPURaiseInternalInterrupt(
    CPUState* cpu, uint8_t interrupt_number) {
  cpu->has_pending_internal_interrupt = true;
  cpu->pending_internal_interrupt_number = interrupt_number;
}

// Discard a pending internal interrupt without taking it.
static inline void CPUClearInternalInterrupt(CPUState* cpu) {
  cpu->has_pending_internal_interrupt = false;
  cpu->pending_internal_interrupt_number = 0;
}

// Assert an external interrupt request on the INTR line, as an interrupt
// controller does. The CPU takes it at the end of the next instruction at
// which interrupts are enabled, so unlike CPURaiseInternalInterrupt() this does
// not disturb an internal interrupt that is already pending.
//
// Callers should check is_intr_asserted first: the CPU has a single INTR slot,
// so asserting a second request before the first is taken would drop it,
// leaving the controller waiting for an end-of-interrupt that never comes.
static inline void CPUAssertINTR(CPUState* cpu, uint8_t vector) {
  cpu->is_intr_asserted = true;
  cpu->intr_vector = vector;
}

// Request that the current tick stop as soon as the instruction in progress
// finishes, causing CPUTick() to return kCPUTickStopped.
//
// This is intended to be called from within a CPU callback - a memory or I/O
// port access, an interrupt handler, or an instruction hook - which is why
// stopping is signalled out of band rather than through a return value: a
// watchpoint fires inside read_memory_byte, which returns a uint8_t and has no
// way to carry a status.
//
// The request applies only to the tick during which it was made. CPUTick()
// clears it on entry, so a request made outside a tick has no effect.
static inline void CPURequestStop(CPUState* cpu) { cpu->stop_requested = true; }

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
  kPrefixES = 0x26,    // ES segment override
  kPrefixCS = 0x2E,    // CS segment override
  kPrefixSS = 0x36,    // SS segment override
  kPrefixDS = 0x3E,    // DS segment override
  kPrefixLOCK = 0xF0,  // LOCK
  // Undocumented alias of LOCK on the 8086/8088, which does not decode bit 0
  // of this opcode.
  kPrefixLOCKAlt = 0xF1,
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
typedef enum CPUFetchNextInstructionStatus {
  kFetchSuccess = 0,
  // Prefix exceeds maximum allowed size.
  kFetchPrefixTooLong = -1,
} CPUFetchNextInstructionStatus;

// Fetch the next instruction from CS:IP.
CPUFetchNextInstructionStatus CPUFetchNextInstruction(
    CPUState* cpu, Instruction* instruction);

// Execute a single fetched instruction.
InstructionResult CPUExecuteInstruction(
    CPUState* cpu, Instruction* instruction);

// Run a single instruction cycle, including fetching and executing the next
// instruction at CS:IP, and handling interrupts.
CPUTickResult CPUTick(CPUState* cpu);

#endif  // YAX86_CPU_PUBLIC_H
