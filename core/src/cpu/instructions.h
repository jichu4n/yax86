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
extern InstructionResult ExecuteNoOp(const InstructionContext* ctx);

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
extern InstructionResult ExecuteMoveRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// MOV r8, r/m8
// MOV r16, r/m16
extern InstructionResult ExecuteMoveRegisterOrMemoryToRegister(
    const InstructionContext* ctx);
// MOV r/m16, sreg
extern InstructionResult ExecuteMoveSegmentRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// MOV sreg, r/m16
extern InstructionResult ExecuteMoveRegisterOrMemoryToSegmentRegister(
    const InstructionContext* ctx);
// MOV AX/CX/DX/BX/SP/BP/SI/DI, imm16
// MOV AH/AL/CH/CL/DH/DL/BH/BL, imm8
extern InstructionResult ExecuteMoveImmediateToRegister(
    const InstructionContext* ctx);
// MOV AL, moffs16
// MOV AX, moffs16
extern InstructionResult ExecuteMoveMemoryOffsetToALOrAX(
    const InstructionContext* ctx);
// MOV moffs16, AL
// MOV moffs16, AX
extern InstructionResult ExecuteMoveALOrAXToMemoryOffset(
    const InstructionContext* ctx);
// MOV r/m8, imm8
// MOV r/m16, imm16
extern InstructionResult ExecuteMoveImmediateToRegisterOrMemory(
    const InstructionContext* ctx);
// XCHG AX, AX/CX/DX/BX/SP/BP/SI/DI
extern InstructionResult ExecuteExchangeRegister(const InstructionContext* ctx);
// XCHG r/m8, r8
// XCHG r/m16, r16
extern InstructionResult ExecuteExchangeRegisterOrMemory(
    const InstructionContext* ctx);
// XLAT
extern InstructionResult ExecuteTranslateByte(const InstructionContext* ctx);

// ============================================================================
// LEA instructions - instructions_lea.h
// ============================================================================

// LEA r16, m
extern InstructionResult ExecuteLoadEffectiveAddress(
    const InstructionContext* ctx);
// LES r16, m
extern InstructionResult ExecuteLoadESWithPointer(
    const InstructionContext* ctx);
// LDS r16, m
extern InstructionResult ExecuteLoadDSWithPointer(
    const InstructionContext* ctx);

// ============================================================================
// Addition instructions - instructions_add.h
// ============================================================================

// Common logic for ADD instructions
extern InstructionResult ExecuteAdd(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);
// Common logic for INC instructions
extern InstructionResult ExecuteInc(
    const InstructionContext* ctx, Operand* dest);
// Common logic for ADC instructions
extern InstructionResult ExecuteAddWithCarry(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);

// ADD r/m8, r8
// ADD r/m16, r16
extern InstructionResult ExecuteAddRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// ADD r8, r/m8
// ADD r16, r/m16
extern InstructionResult ExecuteAddRegisterOrMemoryToRegister(
    const InstructionContext* ctx);
// ADD AL, imm8
// ADD AX, imm16
extern InstructionResult ExecuteAddImmediateToALOrAX(
    const InstructionContext* ctx);
// ADC r/m8, r8
// ADC r/m16, r16
extern InstructionResult ExecuteAddRegisterToRegisterOrMemoryWithCarry(
    const InstructionContext* ctx);
// ADC r8, r/m8
// ADC r16, r/m16
extern InstructionResult ExecuteAddRegisterOrMemoryToRegisterWithCarry(
    const InstructionContext* ctx);
// ADC AL, imm8
// ADC AX, imm16
extern InstructionResult ExecuteAddImmediateToALOrAXWithCarry(
    const InstructionContext* ctx);
// INC AX/CX/DX/BX/SP/BP/SI/DI
extern InstructionResult ExecuteIncRegister(const InstructionContext* ctx);

// ============================================================================
// Subtraction instructions - instructions_sub.c
// ============================================================================

// Set CPU flags after a SUB, SBB, CMP, or NEG instruction.
extern void SetFlagsAfterSub(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_borrow);

// Common logic for SUB instructions
extern InstructionResult ExecuteSub(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);
// Common logic for SBB instructions
extern InstructionResult ExecuteSubWithBorrow(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);
// Common logic for DEC instructions
extern InstructionResult ExecuteDec(
    const InstructionContext* ctx, Operand* dest);

// SUB r/m8, r8
// SUB r/m16, r16
extern InstructionResult ExecuteSubRegisterFromRegisterOrMemory(
    const InstructionContext* ctx);
// SUB r8, r/m8
// SUB r16, r/m16
extern InstructionResult ExecuteSubRegisterOrMemoryFromRegister(
    const InstructionContext* ctx);
// SUB AL, imm8
// SUB AX, imm16
extern InstructionResult ExecuteSubImmediateFromALOrAX(
    const InstructionContext* ctx);
// SBB r/m8, r8
// SBB r/m16, r16
extern InstructionResult ExecuteSubRegisterFromRegisterOrMemoryWithBorrow(
    const InstructionContext* ctx);
// SBB r8, r/m8
// SBB r16, r/m16
extern InstructionResult ExecuteSubRegisterOrMemoryFromRegisterWithBorrow(
    const InstructionContext* ctx);
// SBB AL, imm8
// SBB AX, imm16
extern InstructionResult ExecuteSubImmediateFromALOrAXWithBorrow(
    const InstructionContext* ctx);
// DEC AX/CX/DX/BX/SP/BP/SI/DI
extern InstructionResult ExecuteDecRegister(const InstructionContext* ctx);

// ============================================================================
// Sign extension instructions - instructions_sign_ext.c
// ============================================================================

// CBW
extern InstructionResult ExecuteCbw(const InstructionContext* ctx);
// CWD
extern InstructionResult ExecuteCwd(const InstructionContext* ctx);

// ============================================================================
// CMP instructions - instructions_cmp.c
// ============================================================================

// Common logic for CMP instructions. Computes dest - src and sets flags.
extern InstructionResult ExecuteCmp(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);

// CMP r/m8, r8
// CMP r/m16, r16
extern InstructionResult ExecuteCmpRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// CMP r8, r/m8
// CMP r16, r/m16
extern InstructionResult ExecuteCmpRegisterOrMemoryToRegister(
    const InstructionContext* ctx);
// CMP AL, imm8
// CMP AX, imm16
extern InstructionResult ExecuteCmpImmediateToALOrAX(
    const InstructionContext* ctx);

// ============================================================================
// Boolean instructions - instructions_bool.c
// ============================================================================

extern void SetFlagsAfterBooleanInstruction(
    const InstructionContext* ctx, uint32_t result);
// Common logic for AND instructions.
extern InstructionResult ExecuteBooleanAnd(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);
// Common logic for OR instructions.
extern InstructionResult ExecuteBooleanOr(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);
// Common logic for XOR instructions.
extern InstructionResult ExecuteBooleanXor(
    const InstructionContext* ctx, Operand* dest,
    const OperandValue* src_value);
// Common logic for TEST instructions.
extern InstructionResult ExecuteTest(
    const InstructionContext* ctx, Operand* dest, OperandValue* src_value);

// AND r/m8, r8
// AND r/m16, r16
extern InstructionResult ExecuteBooleanAndRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// AND r8, r/m8
// AND r16, r/m16
extern InstructionResult ExecuteBooleanAndRegisterOrMemoryToRegister(
    const InstructionContext* ctx);
// AND AL, imm8
// AND AX, imm16
extern InstructionResult ExecuteBooleanAndImmediateToALOrAX(
    const InstructionContext* ctx);
// OR r/m8, r8
// OR r/m16, r16
extern InstructionResult ExecuteBooleanOrRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// OR r8, r/m8
// OR r16, r/m16
extern InstructionResult ExecuteBooleanOrRegisterOrMemoryToRegister(
    const InstructionContext* ctx);
// OR AL, imm8
// OR AX, imm16
extern InstructionResult ExecuteBooleanOrImmediateToALOrAX(
    const InstructionContext* ctx);
// XOR r/m8, r8
// XOR r/m16, r16
extern InstructionResult ExecuteBooleanXorRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// XOR r8, r/m8
// XOR r16, r/m16
extern InstructionResult ExecuteBooleanXorRegisterOrMemoryToRegister(
    const InstructionContext* ctx);
// XOR AL, imm8
// XOR AX, imm16
extern InstructionResult ExecuteBooleanXorImmediateToALOrAX(
    const InstructionContext* ctx);
// TEST r/m8, r8
// TEST r/m16, r16
extern InstructionResult ExecuteTestRegisterToRegisterOrMemory(
    const InstructionContext* ctx);
// TEST AL, imm8
// TEST AX, imm16
extern InstructionResult ExecuteTestImmediateToALOrAX(
    const InstructionContext* ctx);

// ============================================================================
// Control flow instructions - instructions_ctrl_flow.c
// ============================================================================

// Common logic for far jumps.
extern InstructionResult ExecuteFarJump(
    const InstructionContext* ctx, const OperandValue* segment,
    const OperandValue* offset);
// Common logic for far calls.
extern InstructionResult ExecuteFarCall(
    const InstructionContext* ctx, const OperandValue* segment,
    const OperandValue* offset);
// Common logic for returning from an interrupt.
extern InstructionResult ExecuteReturnFromInterrupt(CPUState* cpu);

// JMP rel8
// JMP rel16
extern InstructionResult ExecuteShortOrNearJump(const InstructionContext* ctx);
// JMP ptr16:16
extern InstructionResult ExecuteDirectFarJump(const InstructionContext* ctx);
// Unsigned conditional jumps.
extern InstructionResult ExecuteUnsignedConditionalJump(
    const InstructionContext* ctx);
// JL/JGNE and JNL/JGE
extern InstructionResult ExecuteSignedConditionalJumpJLOrJNL(
    const InstructionContext* ctx);
// JLE/JG and JNLE/JG
extern InstructionResult ExecuteSignedConditionalJumpJLEOrJNLE(
    const InstructionContext* ctx);
// LOOP rel8
extern InstructionResult ExecuteLoop(const InstructionContext* ctx);
// LOOPZ rel8
// LOOPNZ rel8
extern InstructionResult ExecuteLoopZOrNZ(const InstructionContext* ctx);
// JCXZ rel8
extern InstructionResult ExecuteJumpIfCXIsZero(const InstructionContext* ctx);
// CALL rel16
extern InstructionResult ExecuteDirectNearCall(const InstructionContext* ctx);
// CALL ptr16:16
extern InstructionResult ExecuteDirectFarCall(const InstructionContext* ctx);
// RET
extern InstructionResult ExecuteNearReturn(const InstructionContext* ctx);
// RET imm16
extern InstructionResult ExecuteNearReturnAndPop(const InstructionContext* ctx);
// RETF
extern InstructionResult ExecuteFarReturn(const InstructionContext* ctx);
// RETF imm16
extern InstructionResult ExecuteFarReturnAndPop(const InstructionContext* ctx);
// IRET
extern InstructionResult ExecuteIret(const InstructionContext* ctx);
// INT 3
extern InstructionResult ExecuteInt3(const InstructionContext* ctx);
// INTO
extern InstructionResult ExecuteInto(const InstructionContext* ctx);
// INT n
extern InstructionResult ExecuteIntN(const InstructionContext* ctx);
// HLT
extern InstructionResult ExecuteHlt(const InstructionContext* ctx);

// ============================================================================
// Stack instructions - instructions_stack.c
// ============================================================================

// PUSH AX/CX/DX/BX/SP/BP/SI/DI
extern InstructionResult ExecutePushRegister(const InstructionContext* ctx);
// POP AX/CX/DX/BX/SP/BP/SI/DI
extern InstructionResult ExecutePopRegister(const InstructionContext* ctx);
// PUSH ES/CS/SS/DS
extern InstructionResult ExecutePushSegmentRegister(
    const InstructionContext* ctx);
// POP ES/CS/SS/DS
extern InstructionResult ExecutePopSegmentRegister(
    const InstructionContext* ctx);
// PUSHF
extern InstructionResult ExecutePushFlags(const InstructionContext* ctx);
// POPF
extern InstructionResult ExecutePopFlags(const InstructionContext* ctx);
// POP r/m16
extern InstructionResult ExecutePopRegisterOrMemory(
    const InstructionContext* ctx);
// LAHF
extern InstructionResult ExecuteLoadAHFromFlags(const InstructionContext* ctx);
// SAHF
extern InstructionResult ExecuteStoreAHToFlags(const InstructionContext* ctx);

// ============================================================================
// Flag manipulation instructions - instructions_flags.c
// ============================================================================

// CLC, STC, CLI, STI, CLD, STD
extern InstructionResult ExecuteClearOrSetFlag(const InstructionContext* ctx);
// CMC
extern InstructionResult ExecuteComplementCarryFlag(
    const InstructionContext* ctx);
// SALC
extern InstructionResult ExecuteSetALFromCarry(const InstructionContext* ctx);

// ============================================================================
// IN and OUT instructions - instructions_io.c
// ============================================================================

// IN AL, imm8
// IN AX, imm8
extern InstructionResult ExecuteInImmediate(const InstructionContext* ctx);
// IN AL, DX
// IN AX, DX
extern InstructionResult ExecuteInDX(const InstructionContext* ctx);
// OUT imm8, AL
// OUT imm8, AX
extern InstructionResult ExecuteOutImmediate(const InstructionContext* ctx);
// OUT DX, AL
// OUT DX, AX
extern InstructionResult ExecuteOutDX(const InstructionContext* ctx);

// ============================================================================
// String instructions - instructions_string.c
// ============================================================================

// MOVS
extern InstructionResult ExecuteMovs(const InstructionContext* ctx);
// STOS
extern InstructionResult ExecuteStos(const InstructionContext* ctx);
// LODS
extern InstructionResult ExecuteLods(const InstructionContext* ctx);
// SCAS
extern InstructionResult ExecuteScas(const InstructionContext* ctx);
// CMPS
extern InstructionResult ExecuteCmps(const InstructionContext* ctx);

// ============================================================================
// BCD and ASCII arithmetic instructions - instructions_bcd_ascii.c
// ============================================================================

// AAA
extern InstructionResult ExecuteAaa(const InstructionContext* ctx);
// AAS
extern InstructionResult ExecuteAas(const InstructionContext* ctx);
// AAM
extern InstructionResult ExecuteAam(const InstructionContext* ctx);
// AAD
extern InstructionResult ExecuteAad(const InstructionContext* ctx);
// DAA
extern InstructionResult ExecuteDaa(const InstructionContext* ctx);
// DAS
extern InstructionResult ExecuteDas(const InstructionContext* ctx);

// ============================================================================
// Group 1 instructions - instructions_group_1.c
// ============================================================================

// Group 1 instruction handler.
extern InstructionResult ExecuteGroup1Instruction(
    const InstructionContext* ctx);

// Group 1 instruction handler, but sign-extends the 8-bit immediate value.
extern InstructionResult ExecuteGroup1InstructionWithSignExtension(
    const InstructionContext* ctx);

// ============================================================================
// Group 2 instructions - instructions_group_2.c
// ============================================================================

// Group 2 shift / rotate by 1.
extern InstructionResult ExecuteGroup2ShiftOrRotateBy1Instruction(
    const InstructionContext* ctx);
// Group 2 shift / rotate by CL.
extern InstructionResult ExecuteGroup2ShiftOrRotateByCLInstruction(
    const InstructionContext* ctx);

// ============================================================================
// Group 3 instructions - instructions_group_3.c
// ============================================================================

// Group 3 instruction handler.
extern InstructionResult ExecuteGroup3Instruction(
    const InstructionContext* ctx);

// ============================================================================
// Group 4 instructions - instructions_group_4.c
// ============================================================================

// Group 4 instruction handler.
extern InstructionResult ExecuteGroup4Instruction(
    const InstructionContext* ctx);

// ============================================================================
// Group 5 instructions - instructions_group_5.c
// ============================================================================

// Group 5 instruction handler.
extern InstructionResult ExecuteGroup5Instruction(
    const InstructionContext* ctx);

#endif  // YAX86_IMPLEMENTATION

#endif  // YAX86_CPU_INSTRUCTIONS_H
