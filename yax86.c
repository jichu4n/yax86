#include "yax86.h"

// ============================================================================
// Opcodes
// ============================================================================

// Opcode lookup table entry.
typedef struct {
  // Opcode.
  uint8_t opcode;

  // Instruction has ModR/M byte
  bool has_modrm : 1;
  // Number of immediate data bytes: 0, 1, 2, or 4
  uint8_t immediate_size : 3;
} OpcodeMetadata;

// Opcode metadata definitions.
static const OpcodeMetadata opcodes[] = {
    // ADD r/m8, r8
    {.opcode = 0x00, .has_modrm = true, .immediate_size = 0},
    // ADD r/m16, r16
    {.opcode = 0x01, .has_modrm = true, .immediate_size = 0},
    // ADD r8, r/m8
    {.opcode = 0x02, .has_modrm = true, .immediate_size = 0},
    // ADD r16, r/m16
    {.opcode = 0x03, .has_modrm = true, .immediate_size = 0},
    // ADD AL, imm8
    {.opcode = 0x04, .has_modrm = false, .immediate_size = 1},
    // ADD AX, imm16
    {.opcode = 0x05, .has_modrm = false, .immediate_size = 2},
    // PUSH ES
    {.opcode = 0x06, .has_modrm = false, .immediate_size = 0},
    // POP ES
    {.opcode = 0x07, .has_modrm = false, .immediate_size = 0},
    // OR r/m8, r8
    {.opcode = 0x08, .has_modrm = true, .immediate_size = 0},
    // OR r/m16, r16
    {.opcode = 0x09, .has_modrm = true, .immediate_size = 0},
    // OR r8, r/m8
    {.opcode = 0x0A, .has_modrm = true, .immediate_size = 0},
    // OR r16, r/m16
    {.opcode = 0x0B, .has_modrm = true, .immediate_size = 0},
    // OR AL, imm8
    {.opcode = 0x0C, .has_modrm = false, .immediate_size = 1},
    // OR AX, imm16
    {.opcode = 0x0D, .has_modrm = false, .immediate_size = 2},
    // PUSH CS
    {.opcode = 0x0E, .has_modrm = false, .immediate_size = 0},
    // PUSH SS
    {.opcode = 0x16, .has_modrm = false, .immediate_size = 0},
    // POP SS
    {.opcode = 0x17, .has_modrm = false, .immediate_size = 0},
    // PUSH DS
    {.opcode = 0x1E, .has_modrm = false, .immediate_size = 0},
    // POP DS
    {.opcode = 0x1F, .has_modrm = false, .immediate_size = 0},
    // AND r/m8, r8
    {.opcode = 0x20, .has_modrm = true, .immediate_size = 0},
    // AND r/m16, r16
    {.opcode = 0x21, .has_modrm = true, .immediate_size = 0},
    // AND r8, r/m8
    {.opcode = 0x22, .has_modrm = true, .immediate_size = 0},
    // AND r16, r/m16
    {.opcode = 0x23, .has_modrm = true, .immediate_size = 0},
    // AND AL, imm8
    {.opcode = 0x24, .has_modrm = false, .immediate_size = 1},
    // AND AX, imm16
    {.opcode = 0x25, .has_modrm = false, .immediate_size = 2},
    // DAA
    {.opcode = 0x27, .has_modrm = false, .immediate_size = 0},
    // SUB r/m8, r8
    {.opcode = 0x28, .has_modrm = true, .immediate_size = 0},
    // SUB r/m16, r16
    {.opcode = 0x29, .has_modrm = true, .immediate_size = 0},
    // SUB r8, r/m8
    {.opcode = 0x2A, .has_modrm = true, .immediate_size = 0},
    // SUB r16, r/m16
    {.opcode = 0x2B, .has_modrm = true, .immediate_size = 0},
    // SUB AL, imm8
    {.opcode = 0x2C, .has_modrm = false, .immediate_size = 1},
    // SUB AX, imm16
    {.opcode = 0x2D, .has_modrm = false, .immediate_size = 2},
    // DAS
    {.opcode = 0x2F, .has_modrm = false, .immediate_size = 0},
    // XOR r/m8, r8
    {.opcode = 0x30, .has_modrm = true, .immediate_size = 0},
    // XOR r/m16, r16
    {.opcode = 0x31, .has_modrm = true, .immediate_size = 0},
    // XOR r8, r/m8
    {.opcode = 0x32, .has_modrm = true, .immediate_size = 0},
    // XOR r16, r/m16
    {.opcode = 0x33, .has_modrm = true, .immediate_size = 0},
    // XOR AL, imm8
    {.opcode = 0x34, .has_modrm = false, .immediate_size = 1},
    // XOR AX, imm16
    {.opcode = 0x35, .has_modrm = false, .immediate_size = 2},
    // AAA
    {.opcode = 0x37, .has_modrm = false, .immediate_size = 0},
    // CMP r/m8, r8
    {.opcode = 0x38, .has_modrm = true, .immediate_size = 0},
    // CMP r/m16, r16
    {.opcode = 0x39, .has_modrm = true, .immediate_size = 0},
    // CMP r8, r/m8
    {.opcode = 0x3A, .has_modrm = true, .immediate_size = 0},
    // CMP r16, r/m16
    {.opcode = 0x3B, .has_modrm = true, .immediate_size = 0},
    // CMP AL, imm8
    {.opcode = 0x3C, .has_modrm = false, .immediate_size = 1},
    // CMP AX, imm16
    {.opcode = 0x3D, .has_modrm = false, .immediate_size = 2},
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
    // PUSH imm16
    {.opcode = 0x68, .has_modrm = false, .immediate_size = 2},
    // PUSH imm8
    {.opcode = 0x6A, .has_modrm = false, .immediate_size = 1},
    // JO rel8
    {.opcode = 0x70, .has_modrm = false, .immediate_size = 1},
    // JNO rel8
    {.opcode = 0x71, .has_modrm = false, .immediate_size = 1},
    // JB/JNAE/JC rel8
    {.opcode = 0x72, .has_modrm = false, .immediate_size = 1},
    // JNB/JAE/JNC rel8
    {.opcode = 0x73, .has_modrm = false, .immediate_size = 1},
    // JE/JZ rel8
    {.opcode = 0x74, .has_modrm = false, .immediate_size = 1},
    // JNE/JNZ rel8
    {.opcode = 0x75, .has_modrm = false, .immediate_size = 1},
    // JBE/JNA rel8
    {.opcode = 0x76, .has_modrm = false, .immediate_size = 1},
    // JNBE/JA rel8
    {.opcode = 0x77, .has_modrm = false, .immediate_size = 1},
    // JS rel8
    {.opcode = 0x78, .has_modrm = false, .immediate_size = 1},
    // JNS rel8
    {.opcode = 0x79, .has_modrm = false, .immediate_size = 1},
    // JP/JPE rel8
    {.opcode = 0x7A, .has_modrm = false, .immediate_size = 1},
    // JNP/JPO rel8
    {.opcode = 0x7B, .has_modrm = false, .immediate_size = 1},
    // JL/JNGE rel8
    {.opcode = 0x7C, .has_modrm = false, .immediate_size = 1},
    // JNL/JGE rel8
    {.opcode = 0x7D, .has_modrm = false, .immediate_size = 1},
    // JLE/JNG rel8
    {.opcode = 0x7E, .has_modrm = false, .immediate_size = 1},
    // JNLE/JG rel8
    {.opcode = 0x7F, .has_modrm = false, .immediate_size = 1},
    // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m8, imm8 (Group 1)
    {.opcode = 0x80, .has_modrm = true, .immediate_size = 1},
    // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m16, imm16 (Group 1)
    {.opcode = 0x81, .has_modrm = true, .immediate_size = 2},
    // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m16, imm8 (Group 1)
    {.opcode = 0x83, .has_modrm = true, .immediate_size = 1},
    // TEST r/m8, r8
    {.opcode = 0x84, .has_modrm = true, .immediate_size = 0},
    // TEST r/m16, r16
    {.opcode = 0x85, .has_modrm = true, .immediate_size = 0},
    // XCHG r/m8, r8
    {.opcode = 0x86, .has_modrm = true, .immediate_size = 0},
    // XCHG r/m16, r16
    {.opcode = 0x87, .has_modrm = true, .immediate_size = 0},
    // MOV r/m8, r8
    {.opcode = 0x88, .has_modrm = true, .immediate_size = 0},
    // MOV r/m16, r16
    {.opcode = 0x89, .has_modrm = true, .immediate_size = 0},
    // MOV r8, r/m8
    {.opcode = 0x8A, .has_modrm = true, .immediate_size = 0},
    // MOV r16, r/m16
    {.opcode = 0x8B, .has_modrm = true, .immediate_size = 0},
    // MOV r/m16, sreg
    {.opcode = 0x8C, .has_modrm = true, .immediate_size = 0},
    // LEA r16, m
    {.opcode = 0x8D, .has_modrm = true, .immediate_size = 0},
    // MOV sreg, r/m16
    {.opcode = 0x8E, .has_modrm = true, .immediate_size = 0},
    // POP r/m16 (Group 1A)
    {.opcode = 0x8F, .has_modrm = true, .immediate_size = 0},
    // XCHG AX, AX (NOP)
    {.opcode = 0x90, .has_modrm = false, .immediate_size = 0},
    // XCHG AX, CX
    {.opcode = 0x91, .has_modrm = false, .immediate_size = 0},
    // XCHG AX, DX
    {.opcode = 0x92, .has_modrm = false, .immediate_size = 0},
    // XCHG AX, BX
    {.opcode = 0x93, .has_modrm = false, .immediate_size = 0},
    // XCHG AX, SP
    {.opcode = 0x94, .has_modrm = false, .immediate_size = 0},
    // XCHG AX, BP
    {.opcode = 0x95, .has_modrm = false, .immediate_size = 0},
    // XCHG AX, SI
    {.opcode = 0x96, .has_modrm = false, .immediate_size = 0},
    // XCHG AX, DI
    {.opcode = 0x97, .has_modrm = false, .immediate_size = 0},
    // CBW
    {.opcode = 0x98, .has_modrm = false, .immediate_size = 0},
    // CWD
    {.opcode = 0x99, .has_modrm = false, .immediate_size = 0},
    // CALL ptr16:16 (4 bytes: 2 for offset, 2 for segment)
    {.opcode = 0x9A, .has_modrm = false, .immediate_size = 4},
    // PUSHF
    {.opcode = 0x9C, .has_modrm = false, .immediate_size = 0},
    // POPF
    {.opcode = 0x9D, .has_modrm = false, .immediate_size = 0},
    // SAHF
    {.opcode = 0x9E, .has_modrm = false, .immediate_size = 0},
    // LAHF
    {.opcode = 0x9F, .has_modrm = false, .immediate_size = 0},
    // MOV AL, moffs8
    {.opcode = 0xA0, .has_modrm = false, .immediate_size = 2},
    // MOV AX, moffs16
    {.opcode = 0xA1, .has_modrm = false, .immediate_size = 2},
    // MOV moffs8, AL
    {.opcode = 0xA2, .has_modrm = false, .immediate_size = 2},
    // MOV moffs16, AX
    {.opcode = 0xA3, .has_modrm = false, .immediate_size = 2},
    // MOVSB
    {.opcode = 0xA4, .has_modrm = false, .immediate_size = 0},
    // MOVSW
    {.opcode = 0xA5, .has_modrm = false, .immediate_size = 0},
    // CMPSB
    {.opcode = 0xA6, .has_modrm = false, .immediate_size = 0},
    // CMPSW
    {.opcode = 0xA7, .has_modrm = false, .immediate_size = 0},
    // TEST AL, imm8
    {.opcode = 0xA8, .has_modrm = false, .immediate_size = 1},
    // TEST AX, imm16
    {.opcode = 0xA9, .has_modrm = false, .immediate_size = 2},
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
    {.opcode = 0xB0, .has_modrm = false, .immediate_size = 1},
    // MOV CL, imm8
    {.opcode = 0xB1, .has_modrm = false, .immediate_size = 1},
    // MOV DL, imm8
    {.opcode = 0xB2, .has_modrm = false, .immediate_size = 1},
    // MOV BL, imm8
    {.opcode = 0xB3, .has_modrm = false, .immediate_size = 1},
    // MOV AH, imm8
    {.opcode = 0xB4, .has_modrm = false, .immediate_size = 1},
    // MOV CH, imm8
    {.opcode = 0xB5, .has_modrm = false, .immediate_size = 1},
    // MOV DH, imm8
    {.opcode = 0xB6, .has_modrm = false, .immediate_size = 1},
    // MOV BH, imm8
    {.opcode = 0xB7, .has_modrm = false, .immediate_size = 1},
    // MOV AX, imm16
    {.opcode = 0xB8, .has_modrm = false, .immediate_size = 2},
    // MOV CX, imm16
    {.opcode = 0xB9, .has_modrm = false, .immediate_size = 2},
    // MOV DX, imm16
    {.opcode = 0xBA, .has_modrm = false, .immediate_size = 2},
    // MOV BX, imm16
    {.opcode = 0xBB, .has_modrm = false, .immediate_size = 2},
    // MOV SP, imm16
    {.opcode = 0xBC, .has_modrm = false, .immediate_size = 2},
    // MOV BP, imm16
    {.opcode = 0xBD, .has_modrm = false, .immediate_size = 2},
    // MOV SI, imm16
    {.opcode = 0xBE, .has_modrm = false, .immediate_size = 2},
    // MOV DI, imm16
    {.opcode = 0xBF, .has_modrm = false, .immediate_size = 2},
    // RET imm16
    {.opcode = 0xC2, .has_modrm = false, .immediate_size = 2},
    // RET
    {.opcode = 0xC3, .has_modrm = false, .immediate_size = 0},
    // LES r16, m32
    {.opcode = 0xC4, .has_modrm = true, .immediate_size = 0},
    // LDS r16, m32
    {.opcode = 0xC5, .has_modrm = true, .immediate_size = 0},
    // MOV r/m8, imm8 (Group 11)
    {.opcode = 0xC6, .has_modrm = true, .immediate_size = 1},
    // MOV r/m16, imm16 (Group 11)
    {.opcode = 0xC7, .has_modrm = true, .immediate_size = 2},
    // RETF imm16
    {.opcode = 0xCA, .has_modrm = false, .immediate_size = 2},
    // RETF
    {.opcode = 0xCB, .has_modrm = false, .immediate_size = 0},
    // INT 3
    {.opcode = 0xCC, .has_modrm = false, .immediate_size = 0},
    // INT imm8
    {.opcode = 0xCD, .has_modrm = false, .immediate_size = 1},
    // INTO
    {.opcode = 0xCE, .has_modrm = false, .immediate_size = 0},
    // IRET
    {.opcode = 0xCF, .has_modrm = false, .immediate_size = 0},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m8, 1 (Group 2)
    {.opcode = 0xD0, .has_modrm = true, .immediate_size = 0},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m16, 1 (Group 2)
    {.opcode = 0xD1, .has_modrm = true, .immediate_size = 0},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m8, CL (Group 2)
    {.opcode = 0xD2, .has_modrm = true, .immediate_size = 0},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m16, CL (Group 2)
    {.opcode = 0xD3, .has_modrm = true, .immediate_size = 0},
    // AAM
    {.opcode = 0xD4, .has_modrm = false, .immediate_size = 1},
    // AAD
    {.opcode = 0xD5, .has_modrm = false, .immediate_size = 1},
    // XLAT/XLATB
    {.opcode = 0xD7, .has_modrm = false, .immediate_size = 0},
    // CALL rel16
    {.opcode = 0xE8, .has_modrm = false, .immediate_size = 2},
    // JMP rel16
    {.opcode = 0xE9, .has_modrm = false, .immediate_size = 2},
    // JMP ptr16:16 (4 bytes: 2 for offset, 2 for segment)
    {.opcode = 0xEA, .has_modrm = false, .immediate_size = 4},
    // JMP rel8
    {.opcode = 0xEB, .has_modrm = false, .immediate_size = 1},
    // HLT
    {.opcode = 0xF4, .has_modrm = false, .immediate_size = 0},
    // CMC
    {.opcode = 0xF5, .has_modrm = false, .immediate_size = 0},
    // TEST/NOT/NEG/MUL/IMUL/DIV/IDIV r/m8 (Group 3)
    {.opcode = 0xF6, .has_modrm = true, .immediate_size = 0},
    // TEST/NOT/NEG/MUL/IMUL/DIV/IDIV r/m16 (Group 3)
    {.opcode = 0xF7, .has_modrm = true, .immediate_size = 0},
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
    {.opcode = 0xFE, .has_modrm = true, .immediate_size = 0},
    // INC/DEC/CALL/JMP/PUSH r/m16 (Group 5)
    {.opcode = 0xFF, .has_modrm = true, .immediate_size = 0},
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
  return ReadByte(cpu, cpu->registers[kCS], (*ip)++);
}

// Fetch the next instruction from memory at CS:IP
EncodedInstruction FetchNextInstruction(CPUState* cpu) {
  EncodedInstruction instruction = {0};
  uint8_t current_byte;
  const uint16_t original_ip = cpu->registers[kIP];
  uint16_t ip = cpu->registers[kIP];

  // Step 1: Check for prefix bytes
  current_byte = ReadNextInstructionByte(cpu, &ip);
  // Segment override prefixes or REP/LOCK prefixes
  while (IsPrefixByte(current_byte)) {
    if (instruction.prefix_size >= kMaxPrefixBytes) {
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
    instruction.mod_rm.value = ReadNextInstructionByte(cpu, &ip);
    instruction.has_mod_rm = true;
    const uint8_t mod = instruction.mod_rm.fields.mod;
    const uint8_t rm = instruction.mod_rm.fields.rm;

    // Step 4: Handle displacement based on mod and r/m
    if (mod == 0 && rm == 6) {
      // Special case: 16-bit displacement
      instruction.displacement_size = 2;
    } else if (mod == 1) {
      // 8-bit displacement
      instruction.displacement_size = 1;
    } else if (mod == 2) {
      // 16-bit displacement
      instruction.displacement_size = 2;
    }
    for (int i = 0; i < instruction.displacement_size; ++i) {
      instruction.displacement[i] = ReadNextInstructionByte(cpu, &ip);
    }
  }

  // Step 5: Handle immediate data based on metadata
  for (int i = 0; i < metadata->immediate_size; ++i) {
    instruction.immediate[i] = ReadNextInstructionByte(cpu, &ip);
  }
  instruction.immediate_size = metadata->immediate_size;

  instruction.size = ip - original_ip;

  return instruction;
}

// Initialize the CPU state
void InitCPU(CPUState* cpu) {
  // Global setup
  InitOpcodeTable();

  // Initialize CPU registers
  for (int i = 0; i < sizeof(cpu->registers) / sizeof(uint16_t); ++i) {
    ((uint16_t*)&cpu->registers)[i] = 0;
  }
  cpu->flags.value = 0;
  cpu->flags.fields.reserved_1 = 1;
}