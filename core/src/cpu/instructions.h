#ifndef YAX86_CPU_INSTRUCTIONS_H
#define YAX86_CPU_INSTRUCTIONS_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Helpers - instructions_helpers.h
// ============================================================================

// Set common CPU flags after an instruction. This includes:
// - Zero flag (ZF)
// - Sign flag (SF)
// - Parity Flag (PF)
YAX86_MODULE_PRIVATE void SetCommonFlagsAfterInstruction(
    const InstructionContext* ctx, uint32_t result);

// Apply the bits that are not flags to a value on its way into the flags
// register. POPF, IRET and SAHF all load flags from somewhere the guest
// controls, and none of them can change these bits.
YAX86_MODULE_PRIVATE uint16_t ToFlagsRegisterValue(uint16_t value);

// Push a value the caller has already worked out onto the stack - a return
// address, the flags, a segment register. Use this wherever the value does not
// depend on the stack pointer.
YAX86_MODULE_PRIVATE void PushValue(CPUState* cpu, OperandValue value);
// Push a PUSH instruction's source operand onto the stack. The operand is
// taken as of after the stack pointer has moved, rather than as of the start
// of the instruction, which is what makes PUSH SP store the decremented value.
YAX86_MODULE_PRIVATE void PushSourceOperand(CPUState* cpu, const Operand* src);
// Pop a value from the stack.
YAX86_MODULE_PRIVATE OperandValue Pop(CPUState* cpu);

// Dummy instruction for unsupported opcodes.
YAX86_MODULE_PRIVATE InstructionResult
ExecuteNoOp(const InstructionContext* ctx);
// Handler for the opcode bytes that are prefixes rather than instructions.
YAX86_MODULE_PRIVATE InstructionResult
ExecuteInvalidOpcode(const InstructionContext* ctx);

// ============================================================================
// Opcode table - opcode_table.h
// ============================================================================

// Global opcode metadata lookup table.
#ifndef YAX86_IMPLEMENTATION
extern const OpcodeMetadata opcode_table[256];
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Move instructions - instructions_mov.h
// ============================================================================

// MOV r/m8, r8
// MOV r/m16, r16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteMoveRegisterToRegisterOrMemory(const InstructionContext* ctx);
// MOV r8, r/m8
// MOV r16, r/m16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteMoveRegisterOrMemoryToRegister(const InstructionContext* ctx);
// MOV r/m16, sreg
YAX86_MODULE_PRIVATE InstructionResult
ExecuteMoveSegmentRegisterToRegisterOrMemory(const InstructionContext* ctx);
// MOV sreg, r/m16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteMoveRegisterOrMemoryToSegmentRegister(const InstructionContext* ctx);
// MOV AX/CX/DX/BX/SP/BP/SI/DI, imm16
// MOV AH/AL/CH/CL/DH/DL/BH/BL, imm8
YAX86_MODULE_PRIVATE InstructionResult
ExecuteMoveImmediateToRegister(const InstructionContext* ctx);
// MOV AL, moffs16
// MOV AX, moffs16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteMoveMemoryOffsetToALOrAX(const InstructionContext* ctx);
// MOV moffs16, AL
// MOV moffs16, AX
YAX86_MODULE_PRIVATE InstructionResult
ExecuteMoveALOrAXToMemoryOffset(const InstructionContext* ctx);
// MOV r/m8, imm8
// MOV r/m16, imm16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteMoveImmediateToRegisterOrMemory(const InstructionContext* ctx);
// XCHG AX, AX/CX/DX/BX/SP/BP/SI/DI
YAX86_MODULE_PRIVATE InstructionResult
ExecuteExchangeRegister(const InstructionContext* ctx);
// XCHG r/m8, r8
// XCHG r/m16, r16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteExchangeRegisterOrMemory(const InstructionContext* ctx);
// XLAT
YAX86_MODULE_PRIVATE InstructionResult
ExecuteTranslateByte(const InstructionContext* ctx);

// ============================================================================
// LEA instructions - instructions_lea.h
// ============================================================================

// LEA r16, m
YAX86_MODULE_PRIVATE InstructionResult
ExecuteLoadEffectiveAddress(const InstructionContext* ctx);
// LES r16, m
YAX86_MODULE_PRIVATE InstructionResult
ExecuteLoadESWithPointer(const InstructionContext* ctx);
// LDS r16, m
YAX86_MODULE_PRIVATE InstructionResult
ExecuteLoadDSWithPointer(const InstructionContext* ctx);

// ============================================================================
// Addition instructions - instructions_add.h
// ============================================================================

// Common logic for ADD instructions
YAX86_MODULE_PRIVATE InstructionResult ExecuteAdd(
    const InstructionContext* ctx, Operand* dest, OperandValue src_value);
// Common logic for INC instructions
YAX86_MODULE_PRIVATE InstructionResult
ExecuteInc(const InstructionContext* ctx, Operand* dest);
// Common logic for ADC instructions
YAX86_MODULE_PRIVATE InstructionResult ExecuteAddWithCarry(
    const InstructionContext* ctx, Operand* dest, OperandValue src_value);

// ADD r/m8, r8
// ADD r/m16, r16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteAddRegisterToRegisterOrMemory(const InstructionContext* ctx);
// ADD r8, r/m8
// ADD r16, r/m16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteAddRegisterOrMemoryToRegister(const InstructionContext* ctx);
// ADD AL, imm8
// ADD AX, imm16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteAddImmediateToALOrAX(const InstructionContext* ctx);
// ADC r/m8, r8
// ADC r/m16, r16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteAddRegisterToRegisterOrMemoryWithCarry(const InstructionContext* ctx);
// ADC r8, r/m8
// ADC r16, r/m16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteAddRegisterOrMemoryToRegisterWithCarry(const InstructionContext* ctx);
// ADC AL, imm8
// ADC AX, imm16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteAddImmediateToALOrAXWithCarry(const InstructionContext* ctx);
// INC AX/CX/DX/BX/SP/BP/SI/DI
YAX86_MODULE_PRIVATE InstructionResult
ExecuteIncRegister(const InstructionContext* ctx);

// ============================================================================
// Subtraction instructions - instructions_sub.c
// ============================================================================

// Set CPU flags after a SUB, SBB, CMP, or NEG instruction.
YAX86_MODULE_PRIVATE void SetFlagsAfterSub(
    const InstructionContext* ctx, uint32_t op1, uint32_t op2, uint32_t result,
    bool did_borrow);

// Common logic for SUB instructions
YAX86_MODULE_PRIVATE InstructionResult ExecuteSub(
    const InstructionContext* ctx, Operand* dest, OperandValue src_value);
// Common logic for SBB instructions
YAX86_MODULE_PRIVATE InstructionResult ExecuteSubWithBorrow(
    const InstructionContext* ctx, Operand* dest, OperandValue src_value);
// Common logic for DEC instructions
YAX86_MODULE_PRIVATE InstructionResult
ExecuteDec(const InstructionContext* ctx, Operand* dest);

// SUB r/m8, r8
// SUB r/m16, r16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteSubRegisterFromRegisterOrMemory(const InstructionContext* ctx);
// SUB r8, r/m8
// SUB r16, r/m16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteSubRegisterOrMemoryFromRegister(const InstructionContext* ctx);
// SUB AL, imm8
// SUB AX, imm16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteSubImmediateFromALOrAX(const InstructionContext* ctx);
// SBB r/m8, r8
// SBB r/m16, r16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteSubRegisterFromRegisterOrMemoryWithBorrow(const InstructionContext* ctx);
// SBB r8, r/m8
// SBB r16, r/m16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteSubRegisterOrMemoryFromRegisterWithBorrow(const InstructionContext* ctx);
// SBB AL, imm8
// SBB AX, imm16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteSubImmediateFromALOrAXWithBorrow(const InstructionContext* ctx);
// DEC AX/CX/DX/BX/SP/BP/SI/DI
YAX86_MODULE_PRIVATE InstructionResult
ExecuteDecRegister(const InstructionContext* ctx);

// ============================================================================
// Sign extension instructions - instructions_sign_ext.c
// ============================================================================

// CBW
YAX86_MODULE_PRIVATE InstructionResult
ExecuteCbw(const InstructionContext* ctx);
// CWD
YAX86_MODULE_PRIVATE InstructionResult
ExecuteCwd(const InstructionContext* ctx);

// ============================================================================
// CMP instructions - instructions_cmp.c
// ============================================================================

// Common logic for CMP instructions. Computes dest - src and sets flags.
YAX86_MODULE_PRIVATE InstructionResult ExecuteCmp(
    const InstructionContext* ctx, Operand* dest, OperandValue src_value);

// CMP r/m8, r8
// CMP r/m16, r16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteCmpRegisterToRegisterOrMemory(const InstructionContext* ctx);
// CMP r8, r/m8
// CMP r16, r/m16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteCmpRegisterOrMemoryToRegister(const InstructionContext* ctx);
// CMP AL, imm8
// CMP AX, imm16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteCmpImmediateToALOrAX(const InstructionContext* ctx);

// ============================================================================
// Boolean instructions - instructions_bool.c
// ============================================================================

YAX86_MODULE_PRIVATE void SetFlagsAfterBooleanInstruction(
    const InstructionContext* ctx, uint32_t result);
// Common logic for AND instructions.
YAX86_MODULE_PRIVATE InstructionResult ExecuteBooleanAnd(
    const InstructionContext* ctx, Operand* dest, OperandValue src_value);
// Common logic for OR instructions.
YAX86_MODULE_PRIVATE InstructionResult ExecuteBooleanOr(
    const InstructionContext* ctx, Operand* dest, OperandValue src_value);
// Common logic for XOR instructions.
YAX86_MODULE_PRIVATE InstructionResult ExecuteBooleanXor(
    const InstructionContext* ctx, Operand* dest, OperandValue src_value);
// Common logic for TEST instructions.
YAX86_MODULE_PRIVATE InstructionResult ExecuteTest(
    const InstructionContext* ctx, Operand* dest, OperandValue src_value);

// AND r/m8, r8
// AND r/m16, r16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteBooleanAndRegisterToRegisterOrMemory(const InstructionContext* ctx);
// AND r8, r/m8
// AND r16, r/m16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteBooleanAndRegisterOrMemoryToRegister(const InstructionContext* ctx);
// AND AL, imm8
// AND AX, imm16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteBooleanAndImmediateToALOrAX(const InstructionContext* ctx);
// OR r/m8, r8
// OR r/m16, r16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteBooleanOrRegisterToRegisterOrMemory(const InstructionContext* ctx);
// OR r8, r/m8
// OR r16, r/m16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteBooleanOrRegisterOrMemoryToRegister(const InstructionContext* ctx);
// OR AL, imm8
// OR AX, imm16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteBooleanOrImmediateToALOrAX(const InstructionContext* ctx);
// XOR r/m8, r8
// XOR r/m16, r16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteBooleanXorRegisterToRegisterOrMemory(const InstructionContext* ctx);
// XOR r8, r/m8
// XOR r16, r/m16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteBooleanXorRegisterOrMemoryToRegister(const InstructionContext* ctx);
// XOR AL, imm8
// XOR AX, imm16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteBooleanXorImmediateToALOrAX(const InstructionContext* ctx);
// TEST r/m8, r8
// TEST r/m16, r16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteTestRegisterToRegisterOrMemory(const InstructionContext* ctx);
// TEST AL, imm8
// TEST AX, imm16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteTestImmediateToALOrAX(const InstructionContext* ctx);

// ============================================================================
// Control flow instructions - instructions_ctrl_flow.c
// ============================================================================

// Common logic for far jumps.
YAX86_MODULE_PRIVATE InstructionResult ExecuteFarJump(
    const InstructionContext* ctx, OperandValue segment, OperandValue offset);
// Common logic for far calls.
YAX86_MODULE_PRIVATE InstructionResult ExecuteFarCall(
    const InstructionContext* ctx, OperandValue segment, OperandValue offset);
// Common logic for returning from an interrupt.
YAX86_MODULE_PRIVATE InstructionResult
ExecuteReturnFromInterrupt(CPUState* cpu);

// JMP rel8
// JMP rel16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteShortOrNearJump(const InstructionContext* ctx);
// JMP ptr16:16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteDirectFarJump(const InstructionContext* ctx);
// Unsigned conditional jumps.
YAX86_MODULE_PRIVATE InstructionResult
ExecuteUnsignedConditionalJump(const InstructionContext* ctx);
// JL/JGNE and JNL/JGE
YAX86_MODULE_PRIVATE InstructionResult
ExecuteSignedConditionalJumpJLOrJNL(const InstructionContext* ctx);
// JLE/JG and JNLE/JG
YAX86_MODULE_PRIVATE InstructionResult
ExecuteSignedConditionalJumpJLEOrJNLE(const InstructionContext* ctx);
// LOOP rel8
YAX86_MODULE_PRIVATE InstructionResult
ExecuteLoop(const InstructionContext* ctx);
// LOOPZ rel8
// LOOPNZ rel8
YAX86_MODULE_PRIVATE InstructionResult
ExecuteLoopZOrNZ(const InstructionContext* ctx);
// JCXZ rel8
YAX86_MODULE_PRIVATE InstructionResult
ExecuteJumpIfCXIsZero(const InstructionContext* ctx);
// CALL rel16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteDirectNearCall(const InstructionContext* ctx);
// CALL ptr16:16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteDirectFarCall(const InstructionContext* ctx);
// RET
YAX86_MODULE_PRIVATE InstructionResult
ExecuteNearReturn(const InstructionContext* ctx);
// RET imm16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteNearReturnAndPop(const InstructionContext* ctx);
// RETF
YAX86_MODULE_PRIVATE InstructionResult
ExecuteFarReturn(const InstructionContext* ctx);
// RETF imm16
YAX86_MODULE_PRIVATE InstructionResult
ExecuteFarReturnAndPop(const InstructionContext* ctx);
// IRET
YAX86_MODULE_PRIVATE InstructionResult
ExecuteIret(const InstructionContext* ctx);
// INT 3
YAX86_MODULE_PRIVATE InstructionResult
ExecuteInt3(const InstructionContext* ctx);
// INTO
YAX86_MODULE_PRIVATE InstructionResult
ExecuteInto(const InstructionContext* ctx);
// INT n
YAX86_MODULE_PRIVATE InstructionResult
ExecuteIntN(const InstructionContext* ctx);
// HLT
YAX86_MODULE_PRIVATE InstructionResult
ExecuteHlt(const InstructionContext* ctx);

// ============================================================================
// Stack instructions - instructions_stack.c
// ============================================================================

// PUSH AX/CX/DX/BX/SP/BP/SI/DI
YAX86_MODULE_PRIVATE InstructionResult
ExecutePushRegister(const InstructionContext* ctx);
// POP AX/CX/DX/BX/SP/BP/SI/DI
YAX86_MODULE_PRIVATE InstructionResult
ExecutePopRegister(const InstructionContext* ctx);
// PUSH ES/CS/SS/DS
YAX86_MODULE_PRIVATE InstructionResult
ExecutePushSegmentRegister(const InstructionContext* ctx);
// POP ES/CS/SS/DS
YAX86_MODULE_PRIVATE InstructionResult
ExecutePopSegmentRegister(const InstructionContext* ctx);
// PUSHF
YAX86_MODULE_PRIVATE InstructionResult
ExecutePushFlags(const InstructionContext* ctx);
// POPF
YAX86_MODULE_PRIVATE InstructionResult
ExecutePopFlags(const InstructionContext* ctx);
// POP r/m16
YAX86_MODULE_PRIVATE InstructionResult
ExecutePopRegisterOrMemory(const InstructionContext* ctx);
// LAHF
YAX86_MODULE_PRIVATE InstructionResult
ExecuteLoadAHFromFlags(const InstructionContext* ctx);
// SAHF
YAX86_MODULE_PRIVATE InstructionResult
ExecuteStoreAHToFlags(const InstructionContext* ctx);

// ============================================================================
// Flag manipulation instructions - instructions_flags.c
// ============================================================================

// CLC, STC, CLI, STI, CLD, STD
YAX86_MODULE_PRIVATE InstructionResult
ExecuteClearOrSetFlag(const InstructionContext* ctx);
// CMC
YAX86_MODULE_PRIVATE InstructionResult
ExecuteComplementCarryFlag(const InstructionContext* ctx);
// SALC
YAX86_MODULE_PRIVATE InstructionResult
ExecuteSetALFromCarry(const InstructionContext* ctx);

// ============================================================================
// IN and OUT instructions - instructions_io.c
// ============================================================================

// IN AL, imm8
// IN AX, imm8
YAX86_MODULE_PRIVATE InstructionResult
ExecuteInImmediate(const InstructionContext* ctx);
// IN AL, DX
// IN AX, DX
YAX86_MODULE_PRIVATE InstructionResult
ExecuteInDX(const InstructionContext* ctx);
// OUT imm8, AL
// OUT imm8, AX
YAX86_MODULE_PRIVATE InstructionResult
ExecuteOutImmediate(const InstructionContext* ctx);
// OUT DX, AL
// OUT DX, AX
YAX86_MODULE_PRIVATE InstructionResult
ExecuteOutDX(const InstructionContext* ctx);

// ============================================================================
// String instructions - instructions_string.c
// ============================================================================

// MOVS
YAX86_MODULE_PRIVATE InstructionResult
ExecuteMovs(const InstructionContext* ctx);
// STOS
YAX86_MODULE_PRIVATE InstructionResult
ExecuteStos(const InstructionContext* ctx);
// LODS
YAX86_MODULE_PRIVATE InstructionResult
ExecuteLods(const InstructionContext* ctx);
// SCAS
YAX86_MODULE_PRIVATE InstructionResult
ExecuteScas(const InstructionContext* ctx);
// CMPS
YAX86_MODULE_PRIVATE InstructionResult
ExecuteCmps(const InstructionContext* ctx);

// ============================================================================
// BCD and ASCII arithmetic instructions - instructions_bcd_ascii.c
// ============================================================================

// AAA
YAX86_MODULE_PRIVATE InstructionResult
ExecuteAaa(const InstructionContext* ctx);
// AAS
YAX86_MODULE_PRIVATE InstructionResult
ExecuteAas(const InstructionContext* ctx);
// AAM
YAX86_MODULE_PRIVATE InstructionResult
ExecuteAam(const InstructionContext* ctx);
// AAD
YAX86_MODULE_PRIVATE InstructionResult
ExecuteAad(const InstructionContext* ctx);
// DAA
YAX86_MODULE_PRIVATE InstructionResult
ExecuteDaa(const InstructionContext* ctx);
// DAS
YAX86_MODULE_PRIVATE InstructionResult
ExecuteDas(const InstructionContext* ctx);

// ============================================================================
// Group 1 instructions - instructions_group_1.c
// ============================================================================

// Group 1 instruction handler.
YAX86_MODULE_PRIVATE InstructionResult
ExecuteGroup1Instruction(const InstructionContext* ctx);

// Group 1 instruction handler, but sign-extends the 8-bit immediate value.
YAX86_MODULE_PRIVATE InstructionResult
ExecuteGroup1InstructionWithSignExtension(const InstructionContext* ctx);

// ============================================================================
// Group 2 instructions - instructions_group_2.c
// ============================================================================

// Group 2 shift / rotate by 1.
YAX86_MODULE_PRIVATE InstructionResult
ExecuteGroup2ShiftOrRotateBy1Instruction(const InstructionContext* ctx);
// Group 2 shift / rotate by CL.
YAX86_MODULE_PRIVATE InstructionResult
ExecuteGroup2ShiftOrRotateByCLInstruction(const InstructionContext* ctx);

// ============================================================================
// Group 3 instructions - instructions_group_3.c
// ============================================================================

// Group 3 instruction handler.
YAX86_MODULE_PRIVATE InstructionResult
ExecuteGroup3Instruction(const InstructionContext* ctx);

// ============================================================================
// Group 4 instructions - instructions_group_4.c
// ============================================================================

// Group 4 instruction handler.
YAX86_MODULE_PRIVATE InstructionResult
ExecuteGroup4Instruction(const InstructionContext* ctx);

// ============================================================================
// Group 5 instructions - instructions_group_5.c
// ============================================================================

// Group 5 instruction handler.
YAX86_MODULE_PRIVATE InstructionResult
ExecuteGroup5Instruction(const InstructionContext* ctx);

#endif  // YAX86_CPU_INSTRUCTIONS_H
