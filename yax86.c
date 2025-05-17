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
    // ADC r/m8, r8
    {.opcode = 0x10, .has_modrm = true, .immediate_size = 0},
    // ADC r/m16, r16
    {.opcode = 0x11, .has_modrm = true, .immediate_size = 0},
    // ADC r8, r/m8
    {.opcode = 0x12, .has_modrm = true, .immediate_size = 0},
    // ADC r16, r/m16
    {.opcode = 0x13, .has_modrm = true, .immediate_size = 0},
    // ADC AL, imm8
    {.opcode = 0x14, .has_modrm = false, .immediate_size = 1},
    // ADC AX, imm16
    {.opcode = 0x15, .has_modrm = false, .immediate_size = 2},
    // PUSH SS
    {.opcode = 0x16, .has_modrm = false, .immediate_size = 0},
    // POP SS
    {.opcode = 0x17, .has_modrm = false, .immediate_size = 0},
    // SBB r/m8, r8
    {.opcode = 0x18, .has_modrm = true, .immediate_size = 0},
    // SBB r/m16, r16
    {.opcode = 0x19, .has_modrm = true, .immediate_size = 0},
    // SBB r8, r/m8
    {.opcode = 0x1A, .has_modrm = true, .immediate_size = 0},
    // SBB r16, r/m16
    {.opcode = 0x1B, .has_modrm = true, .immediate_size = 0},
    // SBB AL, imm8
    {.opcode = 0x1C, .has_modrm = false, .immediate_size = 1},
    // SBB AX, imm16
    {.opcode = 0x1D, .has_modrm = false, .immediate_size = 2},
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
    // SEG ES
    {.opcode = 0x26, .has_modrm = false, .immediate_size = 0},
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
    // SEG CS
    {.opcode = 0x2E, .has_modrm = false, .immediate_size = 0},
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
    // SEG SS
    {.opcode = 0x36, .has_modrm = false, .immediate_size = 0},
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
    // SEG DS
    {.opcode = 0x3E, .has_modrm = false, .immediate_size = 0},
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
    // ADD/ADC/SBB/SUB/CMP r/m8, imm8 (Group 1)
    {.opcode = 0x80, .has_modrm = true, .immediate_size = 1},
    // ADD/ADC/SBB/SUB/CMP r/m16, imm16 (Group 1)
    {.opcode = 0x81, .has_modrm = true, .immediate_size = 2},
    // ADC/SBB/SUB/CMP r/m8, imm8 (Group 1)
    {.opcode = 0x82, .has_modrm = true, .immediate_size = 1},
    // ADD/ADC/SBB/SUB/CMP r/m16, imm8 (Group 1)
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
    {.opcode = 0xE0, .has_modrm = false, .immediate_size = 1},
    // LOOPE/LOOPZ rel8
    {.opcode = 0xE1, .has_modrm = false, .immediate_size = 1},
    // LOOP rel8
    {.opcode = 0xE2, .has_modrm = false, .immediate_size = 1},
    // JCXZ rel8
    {.opcode = 0xE3, .has_modrm = false, .immediate_size = 1},
    // IN AL, imm8
    {.opcode = 0xE4, .has_modrm = false, .immediate_size = 1},
    // IN AX, imm8
    {.opcode = 0xE5, .has_modrm = false, .immediate_size = 1},
    // OUT imm8, AL
    {.opcode = 0xE6, .has_modrm = false, .immediate_size = 1},
    // OUT imm8, AX
    {.opcode = 0xE7, .has_modrm = false, .immediate_size = 1},
    // CALL rel16
    {.opcode = 0xE8, .has_modrm = false, .immediate_size = 2},
    // JMP rel16
    {.opcode = 0xE9, .has_modrm = false, .immediate_size = 2},
    // JMP ptr16:16 (4 bytes: 2 for offset, 2 for segment)
    {.opcode = 0xEA, .has_modrm = false, .immediate_size = 4},
    // JMP rel8
    {.opcode = 0xEB, .has_modrm = false, .immediate_size = 1},
    // IN AL, DX
    {.opcode = 0xEC, .has_modrm = false, .immediate_size = 0},
    // IN AX, DX
    {.opcode = 0xED, .has_modrm = false, .immediate_size = 0},
    // OUT DX, AL
    {.opcode = 0xEE, .has_modrm = false, .immediate_size = 0},
    // OUT DX, AX
    {.opcode = 0xEF, .has_modrm = false, .immediate_size = 0},
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
    {.opcode = 0xF6, .has_modrm = true, .immediate_size = 0},
    // TEST/NOT/NEG/MUL/IMUL/DIV/IDIV r/m16 (Group 3)
    // The immediate size depends on the ModR/M byte.
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
static inline uint8_t ReadByte(
    CPUState* cpu, uint16_t segment, uint16_t offset) {
  // Calculate physical address (segment * 16 + offset)
  uint16_t address = (segment << 4) + offset;
  return cpu->config->read_memory_byte(cpu->config->context, address);
}

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
  return ReadByte(cpu, cpu->registers[kCS], (*ip)++);
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

// Fetch the next instruction from memory at CS:IP
EncodedInstruction FetchNextInstruction(CPUState* cpu) {
  EncodedInstruction instruction = {0};
  uint8_t current_byte;
  const uint16_t original_ip = cpu->registers[kIP];
  uint16_t ip = cpu->registers[kIP];

  // Prefix
  current_byte = ReadNextInstructionByte(cpu, &ip);
  while (IsPrefixByte(current_byte)) {
    if (instruction.prefix_size >= kMaxPrefixBytes) {
      cpu->config->handle_interrupt(
          cpu->config->context, kInterruptInvalidOpcode);
      return instruction;
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

  return instruction;
}

// Initialize the CPU state
void InitCPU(CPUState* cpu) {
  // Global setup
  InitOpcodeTable();

  // Zero out the CPU state
  const CPUState zero_cpu_state = {0};
  *cpu = zero_cpu_state;
  cpu->flags.reserved_1 = 1;
}