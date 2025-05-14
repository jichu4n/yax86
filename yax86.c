#include "yax86.h"

// Opcode metadata definitions.
static const OpcodeMetadata opcodes[] = {
    // ADD r/m8, r8
    {.opcode = 0x00, .has_modrm = 0, .immediate_size = 0},
    // ADD r/m16, r16
    {.opcode = 0x01, .has_modrm = 1, .immediate_size = 0},
    // ADD r8, r/m8
    {.opcode = 0x02, .has_modrm = 0, .immediate_size = 0},
    // ADD r16, r/m16
    {.opcode = 0x03, .has_modrm = 1, .immediate_size = 0},
    // ... (other opcodes)
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

  for (uint16_t i = 0; i < sizeof(opcodes) / sizeof(OpcodeMetadata); ++i) {
    opcode_table[opcodes[i].opcode] = opcodes[i];
  }
}

// Reads a byte from memory at the specified segment:offset
static uint8_t ReadByte(CPUState* cpu, uint16_t segment, uint16_t offset) {
  // Calculate physical address (segment * 16 + offset)
  uint16_t address = (segment << 4) + offset;
  return cpu->config->read_memory(address, kByte);
}

// Helper to check if a byte is a valid prefix
static bool IsPrefixByte(uint8_t byte) {
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
static uint8_t ReadNextInstructionByte(CPUState* cpu, uint16_t* ip) {
  return ReadByte(cpu, cpu->registers.CS, (*ip)++);
}

// Fetch the next instruction from memory at CS:IP
EncodedInstruction FetchNextInstruction(CPUState* cpu) {
  EncodedInstruction instruction = {0};
  uint8_t current_byte;
  const uint16_t original_ip = cpu->registers.IP;
  uint16_t ip = cpu->registers.IP;

  // Step 1: Check for prefix bytes
  current_byte = ReadNextInstructionByte(cpu, &ip);
  // Segment override prefixes or REP/LOCK prefixes
  while (IsPrefixByte(current_byte)) {
    if (instruction.prefix_size >= MAX_NUM_PREFIX_BYTES) {
      // Too many prefix bytes, stop reading
      cpu->config->handle_interrupt(kInterruptInvalidOpcode);
      return instruction;
    }
    instruction.prefix[instruction.prefix_size++] = current_byte;
    current_byte = ReadNextInstructionByte(cpu, &ip);
  }

  // Step 2: Read opcode
  instruction.opcode = current_byte;
  const OpcodeMetadata* metadata = &opcode_table[instruction.opcode];

  // Step 3: Read ModR/M byte if needed
  if (metadata->has_modrm) {
    instruction.mod_rm = ReadNextInstructionByte(cpu, &ip);
    instruction.has_mod_rm = true;

    // Extract mod and r/m fields to determine if displacement follows
    uint8_t mod = (instruction.mod_rm >> 6) & 0x03;
    uint8_t rm = instruction.mod_rm & 0x07;

    // Step 4: Handle displacement based on mod and r/m
    if (mod == 0 && rm == 6) {
      // Special case: 16-bit displacement
      instruction.displacement[0] = ReadNextInstructionByte(cpu, &ip);
      instruction.displacement[1] = ReadNextInstructionByte(cpu, &ip);
      instruction.displacement_size = 2;
    } else if (mod == 1) {
      // 8-bit displacement
      instruction.displacement[0] = ReadNextInstructionByte(cpu, &ip);
      instruction.displacement_size = 1;
    } else if (mod == 2) {
      // 16-bit displacement
      instruction.displacement[0] = ReadNextInstructionByte(cpu, &ip);
      instruction.displacement[1] = ReadNextInstructionByte(cpu, &ip);
      instruction.displacement_size = 2;
    }
  }

  // Step 5: Handle immediate data based on metadata
  if (metadata->immediate_size == 1) {
    instruction.immediate[0] = ReadNextInstructionByte(cpu, &ip);
    instruction.immediate_size = 1;
  } else if (metadata->immediate_size == 2) {
    instruction.immediate[0] = ReadNextInstructionByte(cpu, &ip);
    instruction.immediate[1] = ReadNextInstructionByte(cpu, &ip);
    instruction.immediate_size = 2;
  }

  instruction.size = ip - original_ip;

  return instruction;
}

// Initialize the CPU state
void InitCPU(CPUState* cpu) {
  // Global setup
  InitOpcodeTable();

  // Initialize CPU registers
  cpu->registers.IP = 0;
  cpu->registers.CS = 0;
  cpu->flags.value = 0;
  cpu->flags.flags.reserved_1 = 1;
}