#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// JMP instructions
// ============================================================================

// Jump to a relative signed byte offset.
static InstructionResult ExecuteRelativeJumpByte(
    const InstructionContext* ctx, const OperandValue* offset_value) {
  ctx->cpu->registers[kIP] = AddSignedOffsetByte(
      ctx->cpu->registers[kIP], FromOperandValue(offset_value));
  return kInstructionExecuted;
}

// Jump to a relative signed word offset.
static InstructionResult ExecuteRelativeJumpWord(
    const InstructionContext* ctx, const OperandValue* offset_value) {
  ctx->cpu->registers[kIP] = AddSignedOffsetWord(
      ctx->cpu->registers[kIP], FromOperandValue(offset_value));
  return kInstructionExecuted;
}

// Table of relative jump instructions, indexed by width.
static InstructionResult (*const kRelativeJumpFn[kNumWidths])(
    const InstructionContext* ctx, const OperandValue* offset_value) = {
    ExecuteRelativeJumpByte,  // kByte
    ExecuteRelativeJumpWord,  // kWord
};

// Common logic for JMP instructions.
static InstructionResult ExecuteRelativeJump(
    const InstructionContext* ctx, const OperandValue* offset_value) {
  return kRelativeJumpFn[ctx->metadata->width](ctx, offset_value);
}

// JMP rel8
// JMP rel16
YAX86_PRIVATE InstructionResult
ExecuteShortOrNearJump(const InstructionContext* ctx) {
  OperandValue offset_value = ReadImmediate(ctx);
  return ExecuteRelativeJump(ctx, &offset_value);
}

// Common logic for far jumps.
YAX86_PRIVATE InstructionResult ExecuteFarJump(
    const InstructionContext* ctx, const OperandValue* segment,
    const OperandValue* offset) {
  ctx->cpu->registers[kCS] = FromOperandValue(segment);
  ctx->cpu->registers[kIP] = FromOperandValue(offset);
  return kInstructionExecuted;
}

// JMP ptr16:16
YAX86_PRIVATE InstructionResult
ExecuteDirectFarJump(const InstructionContext* ctx) {
  OperandValue new_cs = WordValue(
      ((uint16_t)ctx->instruction->immediate[2]) |
      (((uint16_t)ctx->instruction->immediate[3]) << 8));
  OperandValue new_ip = WordValue(
      ((uint16_t)ctx->instruction->immediate[0]) |
      (((uint16_t)ctx->instruction->immediate[1]) << 8));
  return ExecuteFarJump(ctx, &new_cs, &new_ip);
}

// ============================================================================
// Conditional jumps
// ============================================================================

// Common logic for conditional jumps.
static InstructionResult ExecuteConditionalJump(
    const InstructionContext* ctx, bool value, bool success_value) {
  if (value == success_value) {
    OperandValue offset_value = ReadImmediate(ctx);
    return ExecuteRelativeJump(ctx, &offset_value);
  }
  return kInstructionExecuted;
}

// Table of flag register bitmasks for conditional jumps. The index corresponds
// to (opcode & 0x0F) / 2, so that the undocumented 0x60-0x6F aliases share the
// entries of their 0x70-0x7F counterparts.
static const uint16_t kUnsignedConditionalJumpFlagBitmasks[] = {
    kOF,        // 0x70 - JO, 0x71 - JNO
    kCF,        // 0x72 - JC, 0x73 - JNC
    kZF,        // 0x74 - JE, 0x75 - JNE
    kCF | kZF,  // 0x76 - JBE, 0x77 - JNBE
    kSF,        // 0x78 - JS, 0x79 - JNS
    kPF,        // 0x7A - JP, 0x7B - JNP
};

// Unsigned conditional jumps.
YAX86_PRIVATE InstructionResult
ExecuteUnsignedConditionalJump(const InstructionContext* ctx) {
  // Masking off the high nibble handles both 0x70-0x7F and their undocumented
  // 0x60-0x6F aliases, which the 8086/8088 decodes identically.
  uint16_t flag_mask = kUnsignedConditionalJumpFlagBitmasks
      [(ctx->instruction->opcode & 0x0F) / 2];
  bool flag_value = (ctx->cpu->flags & flag_mask) != 0;
  // Even opcode => jump if the flag is set
  // Odd opcode => jump if the flag is not set
  bool success_value = ((ctx->instruction->opcode & 0x1) == 0);
  return ExecuteConditionalJump(ctx, flag_value, success_value);
}

// JL/JGNE and JNL/JGE
YAX86_PRIVATE InstructionResult
ExecuteSignedConditionalJumpJLOrJNL(const InstructionContext* ctx) {
  const bool is_greater_or_equal =
      CPUGetFlag(ctx->cpu, kSF) == CPUGetFlag(ctx->cpu, kOF);
  const bool success_value = (ctx->instruction->opcode & 0x1);
  return ExecuteConditionalJump(ctx, is_greater_or_equal, success_value);
}

// JLE/JG and JNLE/JG
YAX86_PRIVATE InstructionResult
ExecuteSignedConditionalJumpJLEOrJNLE(const InstructionContext* ctx) {
  const bool is_greater =
      !CPUGetFlag(ctx->cpu, kZF) &&
      (CPUGetFlag(ctx->cpu, kSF) == CPUGetFlag(ctx->cpu, kOF));
  const bool success_value = (ctx->instruction->opcode & 0x1);
  return ExecuteConditionalJump(ctx, is_greater, success_value);
}

// ============================================================================
// Loop instructions
// ============================================================================

// LOOP rel8
YAX86_PRIVATE InstructionResult ExecuteLoop(const InstructionContext* ctx) {
  return ExecuteConditionalJump(ctx, --(ctx->cpu->registers[kCX]) != 0, true);
}

// LOOPZ rel8
// LOOPNZ rel8
YAX86_PRIVATE InstructionResult
ExecuteLoopZOrNZ(const InstructionContext* ctx) {
  bool condition1 = --(ctx->cpu->registers[kCX]) != 0;
  bool condition2 =
      CPUGetFlag(ctx->cpu, kZF) == (bool)(ctx->instruction->opcode - 0xE0);
  return ExecuteConditionalJump(ctx, condition1 && condition2, true);
}

// JCXZ rel8
YAX86_PRIVATE InstructionResult
ExecuteJumpIfCXIsZero(const InstructionContext* ctx) {
  return ExecuteConditionalJump(ctx, ctx->cpu->registers[kCX] == 0, true);
}

// ============================================================================
// CALL and RET instructions
// ============================================================================

// Common logic for near calls.
static InstructionResult ExecuteNearCall(
    const InstructionContext* ctx, const OperandValue* offset) {
  Push(ctx->cpu, WordValue(ctx->cpu->registers[kIP]));
  return ExecuteRelativeJump(ctx, offset);
}

// CALL rel16
YAX86_PRIVATE InstructionResult
ExecuteDirectNearCall(const InstructionContext* ctx) {
  OperandValue offset = ReadImmediate(ctx);
  return ExecuteNearCall(ctx, &offset);
}

// Common logic for far calls.
YAX86_PRIVATE InstructionResult ExecuteFarCall(
    const InstructionContext* ctx, const OperandValue* segment,
    const OperandValue* offset) {
  // Push the current CS and IP onto the stack.
  Push(ctx->cpu, WordValue(ctx->cpu->registers[kCS]));
  Push(ctx->cpu, WordValue(ctx->cpu->registers[kIP]));
  return ExecuteFarJump(ctx, segment, offset);
}

// CALL ptr16:16
YAX86_PRIVATE InstructionResult
ExecuteDirectFarCall(const InstructionContext* ctx) {
  Push(ctx->cpu, WordValue(ctx->cpu->registers[kCS]));
  Push(ctx->cpu, WordValue(ctx->cpu->registers[kIP]));
  return ExecuteDirectFarJump(ctx);
}

// Common logic for RET instructions.
static InstructionResult ExecuteNearReturnCommon(
    const InstructionContext* ctx, uint16_t arg_size) {
  OperandValue new_ip = Pop(ctx->cpu);
  ctx->cpu->registers[kIP] = FromOperandValue(&new_ip);
  ctx->cpu->registers[kSP] += arg_size;
  return kInstructionExecuted;
}

// RET
YAX86_PRIVATE InstructionResult
ExecuteNearReturn(const InstructionContext* ctx) {
  return ExecuteNearReturnCommon(ctx, 0);
}

// RET imm16
YAX86_PRIVATE InstructionResult
ExecuteNearReturnAndPop(const InstructionContext* ctx) {
  OperandValue arg_size_value = ReadImmediate(ctx);
  return ExecuteNearReturnCommon(ctx, FromOperandValue(&arg_size_value));
}

// Common logic for RETF instructions.
static InstructionResult ExecuteFarReturnCommon(
    const InstructionContext* ctx, uint16_t arg_size) {
  OperandValue new_ip = Pop(ctx->cpu);
  OperandValue new_cs = Pop(ctx->cpu);
  ctx->cpu->registers[kIP] = FromOperandValue(&new_ip);
  ctx->cpu->registers[kCS] = FromOperandValue(&new_cs);
  ctx->cpu->registers[kSP] += arg_size;
  return kInstructionExecuted;
}

// RETF
YAX86_PRIVATE InstructionResult
ExecuteFarReturn(const InstructionContext* ctx) {
  return ExecuteFarReturnCommon(ctx, 0);
}

// RETF imm16
YAX86_PRIVATE InstructionResult
ExecuteFarReturnAndPop(const InstructionContext* ctx) {
  OperandValue arg_size_value = ReadImmediate(ctx);
  return ExecuteFarReturnCommon(ctx, FromOperandValue(&arg_size_value));
}

// ============================================================================
// Interrupt instructions
// ============================================================================

// Common logic for returning from an interrupt.
YAX86_PRIVATE InstructionResult ExecuteReturnFromInterrupt(CPUState* cpu) {
  OperandValue ip_value = Pop(cpu);
  cpu->registers[kIP] = FromOperandValue(&ip_value);
  OperandValue cs_value = Pop(cpu);
  cpu->registers[kCS] = FromOperandValue(&cs_value);
  OperandValue flags_value = Pop(cpu);
  cpu->flags = FromOperandValue(&flags_value);
  return kInstructionExecuted;
}

// IRET
YAX86_PRIVATE InstructionResult ExecuteIret(const InstructionContext* ctx) {
  return ExecuteReturnFromInterrupt(ctx->cpu);
}

// INT 3
YAX86_PRIVATE InstructionResult ExecuteInt3(const InstructionContext* ctx) {
  CPURaiseInternalInterrupt(ctx->cpu, kInterruptBreakpoint);
  return kInstructionExecuted;
}

// INTO
YAX86_PRIVATE InstructionResult ExecuteInto(const InstructionContext* ctx) {
  if (CPUGetFlag(ctx->cpu, kOF)) {
    CPURaiseInternalInterrupt(ctx->cpu, kInterruptOverflow);
  }
  return kInstructionExecuted;
}

// INT n
YAX86_PRIVATE InstructionResult ExecuteIntN(const InstructionContext* ctx) {
  OperandValue interrupt_number_value = ReadImmediate(ctx);
  CPURaiseInternalInterrupt(ctx->cpu, FromOperandValue(&interrupt_number_value));
  return kInstructionExecuted;
}

// HLT
YAX86_PRIVATE InstructionResult
ExecuteHlt(YAX86_UNUSED const InstructionContext* ctx) {
  // HLT executes successfully - it just leaves the CPU halted until an
  // interrupt wakes it. CPUTick() reports the halted state to its caller.
  ctx->cpu->is_halted = true;
  return kInstructionExecuted;
}
