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
    const YCPUInstructionContext* ctx, uint32_t result);

// Apply the bits that are not flags to a value on its way into the flags
// register. POPF, IRET and SAHF all load flags from somewhere the guest
// controls, and none of them can change these bits.
YAX86_MODULE_PRIVATE uint16_t ToFlagsRegisterValue(uint16_t value);

// Push a value the caller has already worked out onto the stack - a return
// address, the flags, a segment register. Use this wherever the value does not
// depend on the stack pointer.
YAX86_MODULE_PRIVATE void PushValue(YCPUState* cpu, YCPUOperandValue value);
// Push a PUSH instruction's source operand onto the stack. The operand is
// taken as of after the stack pointer has moved, rather than as of the start
// of the instruction, which is what makes PUSH SP store the decremented value.
YAX86_MODULE_PRIVATE void PushSourceOperand(
    YCPUState* cpu, const YCPUOperand* src);
// Pop a value from the stack.
YAX86_MODULE_PRIVATE YCPUOperandValue Pop(YCPUState* cpu);

// Dummy instruction for unsupported opcodes.
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteNoOp(const YCPUInstructionContext* ctx);
// Handler for the opcode bytes that are prefixes rather than instructions.
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteInvalidOpcode(const YCPUInstructionContext* ctx);

// ============================================================================
// Opcode table - opcode_table.h
// ============================================================================

// Global opcode metadata lookup table.
#ifndef YAX86_IMPLEMENTATION
extern const YCPUOpcodeMetadata opcode_table[256];
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Move instructions - instructions_mov.h
// ============================================================================

// MOV r/m8, r8
// MOV r/m16, r16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteMoveRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx);
// MOV r8, r/m8
// MOV r16, r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteMoveRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx);
// MOV r/m16, sreg
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteMoveSegmentRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx);
// MOV sreg, r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteMoveRegisterOrMemoryToSegmentRegister(const YCPUInstructionContext* ctx);
// MOV AX/CX/DX/BX/SP/BP/SI/DI, imm16
// MOV AH/AL/CH/CL/DH/DL/BH/BL, imm8
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteMoveImmediateToRegister(const YCPUInstructionContext* ctx);
// MOV AL, moffs16
// MOV AX, moffs16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteMoveMemoryOffsetToALOrAX(const YCPUInstructionContext* ctx);
// MOV moffs16, AL
// MOV moffs16, AX
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteMoveALOrAXToMemoryOffset(const YCPUInstructionContext* ctx);
// MOV r/m8, imm8
// MOV r/m16, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteMoveImmediateToRegisterOrMemory(const YCPUInstructionContext* ctx);
// XCHG AX, AX/CX/DX/BX/SP/BP/SI/DI
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteExchangeRegister(const YCPUInstructionContext* ctx);
// XCHG r/m8, r8
// XCHG r/m16, r16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteExchangeRegisterOrMemory(const YCPUInstructionContext* ctx);
// XLAT
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteTranslateByte(const YCPUInstructionContext* ctx);

// ============================================================================
// LEA instructions - instructions_lea.h
// ============================================================================

// LEA r16, m
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteLoadEffectiveAddress(const YCPUInstructionContext* ctx);
// LES r16, m
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteLoadESWithPointer(const YCPUInstructionContext* ctx);
// LDS r16, m
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteLoadDSWithPointer(const YCPUInstructionContext* ctx);

// ============================================================================
// Addition instructions - instructions_add.h
// ============================================================================

// Common logic for ADD instructions
YAX86_MODULE_PRIVATE YCPUInstructionResult ExecuteAdd(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);
// Common logic for INC instructions
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteInc(const YCPUInstructionContext* ctx, YCPUOperand* dest);
// Common logic for ADC instructions
YAX86_MODULE_PRIVATE YCPUInstructionResult ExecuteAddWithCarry(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);

// ADD r/m8, r8
// ADD r/m16, r16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteAddRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx);
// ADD r8, r/m8
// ADD r16, r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteAddRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx);
// ADD AL, imm8
// ADD AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteAddImmediateToALOrAX(const YCPUInstructionContext* ctx);
// ADC r/m8, r8
// ADC r/m16, r16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteAddRegisterToRegisterOrMemoryWithCarry(
    const YCPUInstructionContext* ctx);
// ADC r8, r/m8
// ADC r16, r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteAddRegisterOrMemoryToRegisterWithCarry(
    const YCPUInstructionContext* ctx);
// ADC AL, imm8
// ADC AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteAddImmediateToALOrAXWithCarry(const YCPUInstructionContext* ctx);
// INC AX/CX/DX/BX/SP/BP/SI/DI
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteIncRegister(const YCPUInstructionContext* ctx);

// ============================================================================
// Subtraction instructions - instructions_sub.c
// ============================================================================

// Set CPU flags after a SUB, SBB, CMP, or NEG instruction.
YAX86_MODULE_PRIVATE void SetFlagsAfterSub(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_borrow);

// Common logic for SUB instructions
YAX86_MODULE_PRIVATE YCPUInstructionResult ExecuteSub(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);
// Common logic for SBB instructions
YAX86_MODULE_PRIVATE YCPUInstructionResult ExecuteSubWithBorrow(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);
// Common logic for DEC instructions
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteDec(const YCPUInstructionContext* ctx, YCPUOperand* dest);

// SUB r/m8, r8
// SUB r/m16, r16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteSubRegisterFromRegisterOrMemory(const YCPUInstructionContext* ctx);
// SUB r8, r/m8
// SUB r16, r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteSubRegisterOrMemoryFromRegister(const YCPUInstructionContext* ctx);
// SUB AL, imm8
// SUB AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteSubImmediateFromALOrAX(const YCPUInstructionContext* ctx);
// SBB r/m8, r8
// SBB r/m16, r16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteSubRegisterFromRegisterOrMemoryWithBorrow(
    const YCPUInstructionContext* ctx);
// SBB r8, r/m8
// SBB r16, r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteSubRegisterOrMemoryFromRegisterWithBorrow(
    const YCPUInstructionContext* ctx);
// SBB AL, imm8
// SBB AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteSubImmediateFromALOrAXWithBorrow(const YCPUInstructionContext* ctx);
// DEC AX/CX/DX/BX/SP/BP/SI/DI
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteDecRegister(const YCPUInstructionContext* ctx);

// ============================================================================
// Sign extension instructions - instructions_sign_ext.c
// ============================================================================

// CBW
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteCbw(const YCPUInstructionContext* ctx);
// CWD
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteCwd(const YCPUInstructionContext* ctx);

// ============================================================================
// CMP instructions - instructions_cmp.c
// ============================================================================

// Common logic for CMP instructions. Computes dest - src and sets flags.
YAX86_MODULE_PRIVATE YCPUInstructionResult ExecuteCmp(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);

// CMP r/m8, r8
// CMP r/m16, r16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteCmpRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx);
// CMP r8, r/m8
// CMP r16, r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteCmpRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx);
// CMP AL, imm8
// CMP AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteCmpImmediateToALOrAX(const YCPUInstructionContext* ctx);

// ============================================================================
// Boolean instructions - instructions_bool.c
// ============================================================================

YAX86_MODULE_PRIVATE void SetFlagsAfterBooleanInstruction(
    const YCPUInstructionContext* ctx, uint32_t result);
// Common logic for AND instructions.
YAX86_MODULE_PRIVATE YCPUInstructionResult ExecuteBooleanAnd(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);
// Common logic for OR instructions.
YAX86_MODULE_PRIVATE YCPUInstructionResult ExecuteBooleanOr(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);
// Common logic for XOR instructions.
YAX86_MODULE_PRIVATE YCPUInstructionResult ExecuteBooleanXor(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);
// Common logic for TEST instructions.
YAX86_MODULE_PRIVATE YCPUInstructionResult ExecuteTest(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);

// AND r/m8, r8
// AND r/m16, r16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteBooleanAndRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx);
// AND r8, r/m8
// AND r16, r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteBooleanAndRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx);
// AND AL, imm8
// AND AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteBooleanAndImmediateToALOrAX(const YCPUInstructionContext* ctx);
// OR r/m8, r8
// OR r/m16, r16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteBooleanOrRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx);
// OR r8, r/m8
// OR r16, r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteBooleanOrRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx);
// OR AL, imm8
// OR AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteBooleanOrImmediateToALOrAX(const YCPUInstructionContext* ctx);
// XOR r/m8, r8
// XOR r/m16, r16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteBooleanXorRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx);
// XOR r8, r/m8
// XOR r16, r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteBooleanXorRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx);
// XOR AL, imm8
// XOR AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteBooleanXorImmediateToALOrAX(const YCPUInstructionContext* ctx);
// TEST r/m8, r8
// TEST r/m16, r16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteTestRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx);
// TEST AL, imm8
// TEST AX, imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteTestImmediateToALOrAX(const YCPUInstructionContext* ctx);

// ============================================================================
// Control flow instructions - instructions_ctrl_flow.c
// ============================================================================

// Common logic for far jumps.
YAX86_MODULE_PRIVATE YCPUInstructionResult ExecuteFarJump(
    const YCPUInstructionContext* ctx, YCPUOperandValue segment,
    YCPUOperandValue offset);
// Common logic for far calls.
YAX86_MODULE_PRIVATE YCPUInstructionResult ExecuteFarCall(
    const YCPUInstructionContext* ctx, YCPUOperandValue segment,
    YCPUOperandValue offset);
// Common logic for returning from an interrupt.
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteReturnFromInterrupt(YCPUState* cpu);

// JMP rel8
// JMP rel16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteShortOrNearJump(const YCPUInstructionContext* ctx);
// JMP ptr16:16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteDirectFarJump(const YCPUInstructionContext* ctx);
// Unsigned conditional jumps.
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteUnsignedConditionalJump(const YCPUInstructionContext* ctx);
// JL/JGNE and JNL/JGE
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteSignedConditionalJumpJLOrJNL(const YCPUInstructionContext* ctx);
// JLE/JG and JNLE/JG
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteSignedConditionalJumpJLEOrJNLE(const YCPUInstructionContext* ctx);
// LOOP rel8
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteLoop(const YCPUInstructionContext* ctx);
// LOOPZ rel8
// LOOPNZ rel8
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteLoopZOrNZ(const YCPUInstructionContext* ctx);
// JCXZ rel8
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteJumpIfCXIsZero(const YCPUInstructionContext* ctx);
// CALL rel16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteDirectNearCall(const YCPUInstructionContext* ctx);
// CALL ptr16:16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteDirectFarCall(const YCPUInstructionContext* ctx);
// RET
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteNearReturn(const YCPUInstructionContext* ctx);
// RET imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteNearReturnAndPop(const YCPUInstructionContext* ctx);
// RETF
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteFarReturn(const YCPUInstructionContext* ctx);
// RETF imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteFarReturnAndPop(const YCPUInstructionContext* ctx);
// IRET
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteIret(const YCPUInstructionContext* ctx);
// INT 3
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteInt3(const YCPUInstructionContext* ctx);
// INTO
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteInto(const YCPUInstructionContext* ctx);
// INT n
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteIntN(const YCPUInstructionContext* ctx);
// HLT
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteHlt(const YCPUInstructionContext* ctx);

// ============================================================================
// Stack instructions - instructions_stack.c
// ============================================================================

// PUSH AX/CX/DX/BX/SP/BP/SI/DI
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecutePushRegister(const YCPUInstructionContext* ctx);
// POP AX/CX/DX/BX/SP/BP/SI/DI
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecutePopRegister(const YCPUInstructionContext* ctx);
// PUSH ES/CS/SS/DS
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecutePushSegmentRegister(const YCPUInstructionContext* ctx);
// POP ES/CS/SS/DS
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecutePopSegmentRegister(const YCPUInstructionContext* ctx);
// PUSHF
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecutePushFlags(const YCPUInstructionContext* ctx);
// POPF
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecutePopFlags(const YCPUInstructionContext* ctx);
// POP r/m16
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecutePopRegisterOrMemory(const YCPUInstructionContext* ctx);
// LAHF
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteLoadAHFromFlags(const YCPUInstructionContext* ctx);
// SAHF
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteStoreAHToFlags(const YCPUInstructionContext* ctx);

// ============================================================================
// Flag manipulation instructions - instructions_flags.c
// ============================================================================

// CLC, STC, CLI, STI, CLD, STD
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteClearOrSetFlag(const YCPUInstructionContext* ctx);
// CMC
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteComplementCarryFlag(const YCPUInstructionContext* ctx);
// SALC
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteSetALFromCarry(const YCPUInstructionContext* ctx);

// ============================================================================
// IN and OUT instructions - instructions_io.c
// ============================================================================

// IN AL, imm8
// IN AX, imm8
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteInImmediate(const YCPUInstructionContext* ctx);
// IN AL, DX
// IN AX, DX
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteInDX(const YCPUInstructionContext* ctx);
// OUT imm8, AL
// OUT imm8, AX
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteOutImmediate(const YCPUInstructionContext* ctx);
// OUT DX, AL
// OUT DX, AX
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteOutDX(const YCPUInstructionContext* ctx);

// ============================================================================
// String instructions - instructions_string.c
// ============================================================================

// MOVS
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteMovs(const YCPUInstructionContext* ctx);
// STOS
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteStos(const YCPUInstructionContext* ctx);
// LODS
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteLods(const YCPUInstructionContext* ctx);
// SCAS
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteScas(const YCPUInstructionContext* ctx);
// CMPS
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteCmps(const YCPUInstructionContext* ctx);

// ============================================================================
// BCD and ASCII arithmetic instructions - instructions_bcd_ascii.c
// ============================================================================

// AAA
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteAaa(const YCPUInstructionContext* ctx);
// AAS
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteAas(const YCPUInstructionContext* ctx);
// AAM
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteAam(const YCPUInstructionContext* ctx);
// AAD
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteAad(const YCPUInstructionContext* ctx);
// DAA
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteDaa(const YCPUInstructionContext* ctx);
// DAS
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteDas(const YCPUInstructionContext* ctx);

// ============================================================================
// Group 1 instructions - instructions_group_1.c
// ============================================================================

// Group 1 instruction handler.
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteGroup1Instruction(const YCPUInstructionContext* ctx);

// Group 1 instruction handler, but sign-extends the 8-bit immediate value.
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteGroup1InstructionWithSignExtension(const YCPUInstructionContext* ctx);

// ============================================================================
// Group 2 instructions - instructions_group_2.c
// ============================================================================

// Group 2 shift / rotate by 1.
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteGroup2ShiftOrRotateBy1Instruction(const YCPUInstructionContext* ctx);
// Group 2 shift / rotate by CL.
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteGroup2ShiftOrRotateByCLInstruction(const YCPUInstructionContext* ctx);

// ============================================================================
// Group 3 instructions - instructions_group_3.c
// ============================================================================

// Group 3 instruction handler.
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteGroup3Instruction(const YCPUInstructionContext* ctx);

// ============================================================================
// Group 4 instructions - instructions_group_4.c
// ============================================================================

// Group 4 instruction handler.
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteGroup4Instruction(const YCPUInstructionContext* ctx);

// ============================================================================
// Group 5 instructions - instructions_group_5.c
// ============================================================================

// Group 5 instruction handler.
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteGroup5Instruction(const YCPUInstructionContext* ctx);

#endif  // YAX86_CPU_INSTRUCTIONS_H
