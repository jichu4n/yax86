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

void CPUInit(CPUState* cpu, CPUConfig* config) {
  // Zero out the CPU state
  const CPUState zero_cpu_state = {0};
  *cpu = zero_cpu_state;
  cpu->flags = kInitialFlags;
  cpu->config = config;
}

// ============================================================================
// Instruction decoding
// ============================================================================

// Helper to check if a byte is a valid prefix
static bool IsPrefixByte(uint8_t byte) {
  static const uint8_t kPrefixBytes[] = {
      kPrefixES,   kPrefixCS,    kPrefixSS,  kPrefixDS,
      kPrefixLOCK, kPrefixREPNZ, kPrefixREP, kPrefixLOCKAlt,
  };
  for (uint8_t i = 0; i < sizeof(kPrefixBytes); ++i) {
    if (byte == kPrefixBytes[i]) {
      return true;
    }
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

// Number of displacement bytes carried by each combination of the ModR/M MOD
// and RM fields, indexed by (mod << 3) | rm. MOD 0 carries none except RM 6,
// which is the direct address form; MOD 1 and MOD 2 carry their own count; MOD
// 3 is register direct and addresses no memory at all.
//
// The whole input is five bits, so the chain of comparisons this replaces is a
// single load. Measured on x86-64 that is a small loss - the comparisons are
// predicted and the load is not free - so this is here for a host with no
// branch predictor, which is where a table is unambiguously cheaper.
static const uint8_t kDisplacementSizes[32] = {
    0, 0, 0, 0, 0, 0, 2, 0,  // MOD 0
    1, 1, 1, 1, 1, 1, 1, 1,  // MOD 1
    2, 2, 2, 2, 2, 2, 2, 2,  // MOD 2
    0, 0, 0, 0, 0, 0, 0, 0,  // MOD 3
};

// Returns the number of immediate bytes in an instruction.
static uint8_t GetImmediateSize(const OpcodeMetadata* metadata, uint8_t reg) {
  // Group 3 is the only place where the REG field decides whether there is an
  // immediate at all: it packs eight instructions into one opcode and only TEST
  // carries one. REG 1 is an alias of REG 0 because the 8086/8088 does not
  // decode bit 0 of the REG field here. Row 0 is the byte form (0xF6) and row 1
  // the word form (0xF7), which is bit 0 of the opcode.
  if ((metadata->opcode | 1) == 0xF7) {
    static const uint8_t kGroup3ImmediateSizes[2][8] = {
        {1, 1, 0, 0, 0, 0, 0, 0},
        {2, 2, 0, 0, 0, 0, 0, 0},
    };
    return kGroup3ImmediateSizes[metadata->opcode & 1][reg];
  }
  // Every other opcode carries what the opcode table says, whatever REG holds.
  return metadata->immediate_size;
}

CPUFetchNextInstructionStatus CPUFetchNextInstruction(
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
    const uint8_t addressing_mode =
        (uint8_t)((instruction.mod_rm.mod << 3) | instruction.mod_rm.rm);
    instruction.displacement_size = kDisplacementSizes[addressing_mode];
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

InstructionResult CPUExecuteInstruction(
    CPUState* cpu, Instruction* instruction) {
  // Run the on_before_execute_instruction callback if provided.
  if (cpu->config->on_before_execute_instruction) {
    cpu->config->on_before_execute_instruction(cpu, instruction);
  }

  const OpcodeMetadata* metadata = &opcode_table[instruction->opcode];
  if (!metadata->handler) {
    return kInstructionInvalid;
  }

  // Check encoded instruction against expected instruction format.
  if (instruction->has_mod_rm != metadata->has_modrm) {
    return kInstructionInvalid;
  }
  if (instruction->immediate_size !=
      (metadata->has_modrm ? GetImmediateSize(metadata, instruction->mod_rm.reg)
                           : metadata->immediate_size)) {
    return kInstructionInvalid;
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

CPUTickResult CPUTick(CPUState* cpu) {
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

    // Step 2: Execute the instruction.
    if (CPUExecuteInstruction(cpu, &instruction) != kInstructionExecuted) {
      YAX86_CPU_LOG(
          kLogLevelError, "%04X:%04X invalid instruction, opcode %02X",
          instruction_cs, instruction_ip, instruction.opcode);
      return kCPUTickInvalid;
    }
    executed_instruction = true;
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
