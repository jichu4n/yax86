#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "cycles.h"
#include "instructions.h"
#include "operands.h"
#include "public.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

#define YAX86_CPU_LOG(level, ...) \
  YAX86_LOG(cpu->config->logger, &kLogModuleCPU, level, __VA_ARGS__)

// ============================================================================
// CPU state
// ============================================================================

YAX86_HOT void CPUInit(CPUState* cpu, CPUConfig* config) {
  // Zero out the CPU state
  const CPUState zero_cpu_state = {0};
  *cpu = zero_cpu_state;
  cpu->flags = kInitialFlags;
  cpu->config = config;
}

// ============================================================================
// Instruction decoding
// ============================================================================

// The two prefix groups are each a contiguous encoding family, so a masked
// compare identifies a whole group and the bits the mask leaves free say which
// member it is.
enum {
  // Segment overrides encode as 001ss110, where ss selects the segment.
  kSegmentOverridePrefixMask = 0xE7,
  kSegmentOverridePrefixValue = 0x26,
  // Position of the ss field within a segment override prefix.
  kSegmentOverridePrefixShift = 3,
  kSegmentOverridePrefixSegmentMask = 0x03,

  // Within the LOCK and repetition prefix group, bit 1 separates the
  // repetition prefixes (REPNZ, REP) from LOCK and its undocumented 0xF1
  // alias.
  kRepetitionPrefixBit = 0x02,
};

// Whether a byte is a segment override prefix. The mask pins every bit but the
// two that select the segment, so it matches those four bytes and nothing else.
static inline bool IsSegmentOverridePrefix(uint8_t byte) {
  return (byte & kSegmentOverridePrefixMask) == kSegmentOverridePrefixValue;
}

// Whether a byte is a LOCK or repetition prefix. These four are consecutive,
// so this is a range check rather than a mask - which is both clearer and one
// instruction cheaper, since a compiler folds it into a single subtract and
// compare.
static inline bool IsLockOrRepetitionPrefix(uint8_t byte) {
  return byte >= kPrefixLOCK && byte <= kPrefixREP;
}

// Record what a prefix byte selects, and report whether it was a prefix at
// all. Deciding which group a byte belongs to is the same test as deciding
// whether it is a prefix, so both happen here rather than the caller asking
// first and this asking again.
//
// The cheaper test goes first. A byte that is not a prefix runs both, and that
// is the common case by far - every instruction ends the loop with one.
static bool ApplyPrefixByte(Instruction* instruction, uint8_t byte) {
  if (IsLockOrRepetitionPrefix(byte)) {
    // LOCK and its 0xF1 alias advance IP, but nothing acts on them, so only a
    // repetition prefix is worth recording.
    if (byte & kRepetitionPrefixBit) {
      instruction->repetition_prefix = byte;
    }
    return true;
  }
  if (IsSegmentOverridePrefix(byte)) {
    // The segment field is in the 8086's sreg encoding order, which is the
    // order kES through kDS are numbered in, so the register index is an
    // offset from kES.
    instruction->segment_override =
        (uint8_t)(kES + ((byte >> kSegmentOverridePrefixShift) &
                         kSegmentOverridePrefixSegmentMask));
    return true;
  }
  return false;
}

// Helper to read the next instruction byte.
//
// This goes straight to memory rather than through ReadMemoryOperandByte, so
// that the fetch is not charged for time on the data bus. The 8088 fetches
// ahead into a queue while the previous instruction executes, so most of that
// time is already paid for by the instruction being executed - and the
// published per-instruction figures the cycle table is built from assume the
// queue is full.
static uint8_t ReadNextInstructionByte(CPUState* cpu, uint16_t* ip) {
  const MemoryAddress address = {
      .segment_register_index = kCS,
      .offset = (*ip)++,
  };
  return ReadRawMemoryByte(cpu, ToRawAddress(cpu, &address));
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
      // REG 0 and REG 1 are both TEST, which carries an immediate; the other
      // REG values do not. The 8086/8088 does not decode bit 0 of the REG
      // field here, which is what makes REG 1 an alias of REG 0.
      return reg <= 1 ? metadata->opcode - 0xF5 : 0;
    default:
      return metadata->immediate_size;
  }
}

YAX86_HOT CPUFetchNextInstructionStatus
CPUFetchNextInstruction(CPUState* cpu, Instruction* instruction) {
  const Instruction zero_instruction = {0};
  *instruction = zero_instruction;

  uint8_t current_byte;
  const uint16_t original_ip = cpu->registers[kIP];
  uint16_t ip = cpu->registers[kIP];

  // Prefix
  //
  // The count is local because nothing outside this loop wants it: what the
  // prefixes selected is in the instruction, and the bytes they occupied are
  // in its size. It exists only to stop a run of prefix bytes from fetching
  // forever - see kMaxPrefixBytes.
  uint8_t prefix_size = 0;
  current_byte = ReadNextInstructionByte(cpu, &ip);
  while (ApplyPrefixByte(instruction, current_byte)) {
    if (++prefix_size > kMaxPrefixBytes) {
      return kFetchPrefixTooLong;
    }
    current_byte = ReadNextInstructionByte(cpu, &ip);
  }

  // Opcode
  instruction->opcode = current_byte;
  const OpcodeMetadata* metadata = &opcode_table[instruction->opcode];

  // ModR/M
  if (metadata->has_modrm) {
    uint8_t mod_rm_byte = ReadNextInstructionByte(cpu, &ip);
    instruction->has_mod_rm = true;
    instruction->mod_rm.mod = (mod_rm_byte >> 6) & 0x03;  // Bits 6-7
    instruction->mod_rm.reg = (mod_rm_byte >> 3) & 0x07;  // Bits 3-5
    instruction->mod_rm.rm = mod_rm_byte & 0x07;          // Bits 0-2

    // Displacement
    const uint8_t displacement_size =
        GetDisplacementSize(instruction->mod_rm.mod, instruction->mod_rm.rm);
    instruction->displacement_size = displacement_size;
    for (uint8_t i = 0; i < displacement_size; ++i) {
      instruction->displacement[i] = ReadNextInstructionByte(cpu, &ip);
    }
  }

  // Immediate operand
  //
  // immediate_size is a three bit field, so it can express more bytes than
  // immediate[] holds. No entry in the opcode table does - the widest is the 4
  // of a far pointer - but nothing in the type says so, and a compiler that
  // cannot see it is right to warn about the writes below. Bounding it by the
  // array is what makes the invariant explicit.
  uint8_t immediate_size = GetImmediateSize(metadata, instruction->mod_rm.reg);
  if (immediate_size > kMaxImmediateBytes) {
    immediate_size = kMaxImmediateBytes;
  }
  instruction->immediate_size = immediate_size;
  for (uint8_t i = 0; i < immediate_size; ++i) {
    instruction->immediate[i] = ReadNextInstructionByte(cpu, &ip);
  }

  instruction->size = ip - original_ip;

  return kFetchSuccess;
}

// ============================================================================
// Execution
// ============================================================================

// Runs an instruction whose opcode table entry the caller already has, and
// which the caller has already established the entry agrees with.
//
// Kept out of line. Inlined into CPUTick() this measured 31% slower on a
// Cortex-M0+: the execute path wants registers, the core has few, and folding
// the two together makes both spill. It only shows up once the hot path is in
// SRAM - from flash the XIP cache dominates and hides it.
YAX86_HOT YAX86_NOINLINE YAX86_PRIVATE InstructionResult
CPUExecuteDecodedInstruction(
    CPUState* cpu, Instruction* instruction, const OpcodeMetadata* metadata) {
  // Run the on_before_execute_instruction callback if provided.
  if (cpu->config->on_before_execute_instruction) {
    cpu->config->on_before_execute_instruction(cpu, instruction);
  }

  // Run the instruction handler.
  InstructionContext context = {
      .cpu = cpu,
      .instruction = instruction,
      .metadata = metadata,
  };
  InstructionResult result = metadata->handler(&context);
  if (result != kInstructionExecuted) {
    return result;
  }

  // Run the on_after_execute_instruction callback if provided.
  if (cpu->config->on_after_execute_instruction) {
    cpu->config->on_after_execute_instruction(cpu, instruction);
  }

  return kInstructionExecuted;
}

// Checks an instruction against the opcode table before running it.
//
// For a caller that built the Instruction itself rather than decoding one -
// CPUTick() goes straight to CPUExecuteDecodedInstruction(), because its own
// decode is what produced the encoding these checks would be re-examining.
YAX86_HOT InstructionResult
CPUExecuteInstruction(CPUState* cpu, Instruction* instruction) {
  const OpcodeMetadata* metadata = &opcode_table[instruction->opcode];

  // Check the encoded instruction against the expected format for its opcode.
  // Nothing checks that the opcode has a handler, because every entry in the
  // table has one: the eight bytes that are prefixes rather than instructions
  // are handled by ExecuteInvalidOpcode(), which returns what a missing handler
  // used to.
  if (instruction->has_mod_rm != metadata->has_modrm) {
    return kInstructionInvalid;
  }
  if (instruction->immediate_size !=
      (metadata->has_modrm ? GetImmediateSize(metadata, instruction->mod_rm.reg)
                           : metadata->immediate_size)) {
    return kInstructionInvalid;
  }

  return CPUExecuteDecodedInstruction(cpu, instruction, metadata);
}

// Save state and vector to the handler for an interrupt.
static void DispatchInterrupt(CPUState* cpu, uint8_t interrupt_number) {
  // Prepare for interrupt processing.
  cpu->is_halted = false;
  PushValue(cpu, WordValue(cpu->flags));
  CPUSetFlag(cpu, kIF, false);
  CPUSetFlag(cpu, kTF, false);
  PushValue(cpu, WordValue(cpu->registers[kCS]));
  PushValue(cpu, WordValue(cpu->registers[kIP]));

  // Invoke the interrupt handler callback first. If the caller did not provide
  // an interrupt handler callback, handle the interrupt within the VM using the
  // Interrupt Vector Table.
  InterruptHandlerResult interrupt_handler_result =
      cpu->config->handle_interrupt
          ? cpu->config->handle_interrupt(cpu, interrupt_number)
          : kInterruptHandlerUnhandled;

  if (interrupt_handler_result == kInterruptHandlerHandled) {
    // If the interrupt was handled by the caller-provided interrupt handler
    // callback, restore state and continue execution.
    ExecuteReturnFromInterrupt(cpu);
    return;
  }

  // If the interrupt was not handled by the caller-provided interrupt handler
  // callback, handle it within the VM using the Interrupt Vector Table.
  uint16_t ivt_entry_offset = interrupt_number << 2;
  cpu->registers[kIP] = ReadRawMemoryWord(cpu, ivt_entry_offset);
  cpu->registers[kCS] = ReadRawMemoryWord(cpu, ivt_entry_offset + 2);
}

// Take a pending interrupt, if any. Returns whether one was dispatched.
static bool ExecutePendingInterrupt(CPUState* cpu) {
  // An internal interrupt goes first. It was raised by the instruction that
  // just executed, and taking it clears IF, which correctly holds off any
  // external request until the handler re-enables interrupts.
  if (cpu->has_pending_internal_interrupt) {
    const uint8_t interrupt_number = cpu->pending_internal_interrupt_number;
    CPUClearInternalInterrupt(cpu);
    DispatchInterrupt(cpu, interrupt_number);
    return true;
  }

  // An external request on the INTR pin is only taken while interrupts are
  // enabled. Acknowledging it is what produces its vector - there is nothing to
  // latch beforehand, and the controller keeps requesting until acknowledged.
  uint8_t intr_vector;
  if (CPUGetFlag(cpu, kIF) && cpu->config->acknowledge_interrupt &&
      cpu->config->acknowledge_interrupt(cpu, &intr_vector)) {
    DispatchInterrupt(cpu, intr_vector);
    return true;
  }

  return false;
}

YAX86_HOT CPUTickResult CPUTick(CPUState* cpu) {
  // A stop request only applies to the tick during which it was made.
  cpu->stop_requested = false;

  // Whether this tick ran an instruction. A halted CPU runs none until an
  // interrupt wakes it.
  bool executed_instruction = false;

  // A halted CPU still consumes time - it is sitting in a wait state, not
  // stopped - so a tick that runs no instruction still has to advance the
  // clock, or the timer that is meant to wake it would never tick either.
  cpu->pending_cycles = 0;
  cpu->cycles_this_tick = kHaltedCycles;

  // The trap flag is sampled before the instruction runs, not after. An
  // instruction that sets TF - POPF or IRET - must not trap on itself, and one
  // that clears TF still traps once for the instruction it was set during.
  const bool trap_flag_was_set = CPUGetFlag(cpu, kTF);

  // Execute next CPU instruction if not halted.
  if (!cpu->is_halted) {
    // Step 1: Fetch the next instruction, and increment IP.
    Instruction instruction;
    uint16_t instruction_cs = cpu->registers[kCS];
    uint16_t instruction_ip = cpu->registers[kIP];
    CPUFetchNextInstructionStatus fetch_status =
        CPUFetchNextInstruction(cpu, &instruction);
    if (fetch_status != kFetchSuccess) {
      YAX86_CPU_LOG(
          kLogLevelError, "%04X:%04X failed to fetch instruction, status %d",
          instruction_cs, instruction_ip, (int)fetch_status);
      return kCPUTickInvalid;
    }
    cpu->registers[kIP] += instruction.size;

    // The cost of the instruction is its base cost plus the address it had to
    // compute, and then whatever it charges itself as it runs - its traffic on
    // the data bus, and any part of its cost that depends on its operands.
    CPUAddCycles(
        cpu, kOpcodeBaseCycles[instruction.opcode] +
                 GetEffectiveAddressCycles(&instruction));

    // Step 2: Execute the instruction. The fetch above derived has_mod_rm and
    // immediate_size from this same table entry, so the checks
    // CPUExecuteInstruction() makes cannot fail here.
    const OpcodeMetadata* const metadata = &opcode_table[instruction.opcode];
    if (CPUExecuteDecodedInstruction(cpu, &instruction, metadata) !=
        kInstructionExecuted) {
      YAX86_CPU_LOG(
          kLogLevelError, "%04X:%04X invalid instruction, opcode %02X",
          instruction_cs, instruction_ip, instruction.opcode);
      return kCPUTickInvalid;
    }
    executed_instruction = true;
    ++cpu->instructions_retired;
    cpu->cycles_this_tick = cpu->pending_cycles;
  }

  // Step 3: Handle a pending interrupt. This runs even while halted, because
  // an interrupt is the only thing that can clear the halted state -
  // ExecutePendingInterrupt() resets is_halted when it dispatches one.
  const bool dispatched_interrupt = ExecutePendingInterrupt(cpu);

  // Step 4: The trap flag raises a single-step interrupt after an instruction
  // executes, so a halted CPU must not trap - otherwise the trap would wake it
  // and then fire again on every subsequent tick.
  //
  // Single-stepping is the lowest priority of the interrupt sources recognized
  // at an instruction boundary, so an interrupt dispatched above takes its
  // place rather than both firing.
  if (executed_instruction && trap_flag_was_set && !dispatched_interrupt) {
    CPURaiseInternalInterrupt(cpu, kInterruptSingleStep);
    ExecutePendingInterrupt(cpu);
  }

  // A stop requested from within a callback takes precedence over everything
  // else: the caller asked to be handed control back at this exact point.
  if (cpu->stop_requested) {
    return kCPUTickStopped;
  }
  // This reports what the tick did, not what state the CPU ended up in. A tick
  // that executes HLT ran an instruction, so it reports kCPUTickExecuted even
  // though the CPU is now halted; the ticks that follow report kCPUTickHalted.
  return executed_instruction ? kCPUTickExecuted : kCPUTickHalted;
}
