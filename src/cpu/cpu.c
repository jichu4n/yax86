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
  // Global setup
  InitOpcodeTable();

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
      (status = cpu->config->on_before_execute_instruction(cpu, instruction) !=
                kExecuteSuccess)) {
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
      (status = cpu->config->on_after_execute_instruction(cpu, instruction) !=
                kExecuteSuccess)) {
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
