#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "cycles.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// JMP instructions
// ============================================================================

// Jump to a relative signed byte offset.
YAX86_FILE_PRIVATE YAX86_HOT YCPUInstructionResult YCPUExecuteRelativeJumpByte(
    const YCPUInstructionContext* ctx, YCPUOperandValue offset_value) {
  ctx->cpu->registers[kIP] = YCPUAddSignedOffsetByte(
      ctx->cpu->registers[kIP], YCPUFromOperandValue(offset_value));
  return kInstructionExecuted;
}

// Jump to a relative signed word offset.
YAX86_FILE_PRIVATE YAX86_HOT YCPUInstructionResult YCPUExecuteRelativeJumpWord(
    const YCPUInstructionContext* ctx, YCPUOperandValue offset_value) {
  ctx->cpu->registers[kIP] = YCPUAddSignedOffsetWord(
      ctx->cpu->registers[kIP], YCPUFromOperandValue(offset_value));
  return kInstructionExecuted;
}

// Table of relative jump instructions, indexed by width.
YAX86_FILE_PRIVATE
YCPUInstructionResult (*const YCPURelativeJumpFn[kNumWidths])(
    const YCPUInstructionContext* ctx, YCPUOperandValue offset_value) = {
  YCPUExecuteRelativeJumpByte,  // kByte
  YCPUExecuteRelativeJumpWord,  // kWord
};

// Common logic for JMP instructions.
YAX86_FILE_PRIVATE YCPUInstructionResult YCPUExecuteRelativeJump(
    const YCPUInstructionContext* ctx, YCPUOperandValue offset_value) {
  return YCPURelativeJumpFn[ctx->metadata->width](ctx, offset_value);
}

// JMP rel8
// JMP rel16
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteShortOrNearJump(const YCPUInstructionContext* ctx) {
  YCPUOperandValue offset_value = YCPUReadImmediate(ctx);
  return YCPUExecuteRelativeJump(ctx, offset_value);
}

// Common logic for far jumps.
YAX86_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteFarJump(
    const YCPUInstructionContext* ctx, YCPUOperandValue segment,
    YCPUOperandValue offset) {
  ctx->cpu->registers[kCS] = YCPUFromOperandValue(segment);
  ctx->cpu->registers[kIP] = YCPUFromOperandValue(offset);
  return kInstructionExecuted;
}

// JMP ptr16:16
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDirectFarJump(const YCPUInstructionContext* ctx) {
  YCPUOperandValue new_cs =
      (YCPUOperandValue)(((uint16_t)ctx->instruction->immediate[2]) |
                         (((uint16_t)ctx->instruction->immediate[3]) << 8));
  YCPUOperandValue new_ip =
      (YCPUOperandValue)(((uint16_t)ctx->instruction->immediate[0]) |
                         (((uint16_t)ctx->instruction->immediate[1]) << 8));
  return YCPUExecuteFarJump(ctx, new_cs, new_ip);
}

// ============================================================================
// Conditional jumps
// ============================================================================

// Common logic for conditional jumps.
YAX86_FILE_PRIVATE YCPUInstructionResult YCPUExecuteConditionalJump(
    const YCPUInstructionContext* ctx, bool value, bool success_value) {
  if (value == success_value) {
    // A taken jump throws away the prefetch queue and has to fill it again.
    // The base cost in the cycle table is for the branch not being taken.
    YCPUAddCycles(ctx->cpu, kJumpTakenCycles);
    YCPUOperandValue offset_value = YCPUReadImmediate(ctx);
    return YCPUExecuteRelativeJump(ctx, offset_value);
  }
  return kInstructionExecuted;
}

// Table of flag register bitmasks for conditional jumps. The index corresponds
// to (opcode & 0x0F) / 2, so that the undocumented 0x60-0x6F aliases share the
// entries of their 0x70-0x7F counterparts.
YAX86_FILE_PRIVATE const uint16_t YCPUUnsignedConditionalJumpFlagBitmasks[] = {
    kOF,        // 0x70 - JO, 0x71 - JNO
    kCF,        // 0x72 - JC, 0x73 - JNC
    kZF,        // 0x74 - JE, 0x75 - JNE
    kCF | kZF,  // 0x76 - JBE, 0x77 - JNBE
    kSF,        // 0x78 - JS, 0x79 - JNS
    kPF,        // 0x7A - JP, 0x7B - JNP
};

// Unsigned conditional jumps.
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
YCPUExecuteUnsignedConditionalJump(const YCPUInstructionContext* ctx) {
  // Masking off the high nibble handles both 0x70-0x7F and their undocumented
  // 0x60-0x6F aliases, which the 8086/8088 decodes identically.
  uint16_t flag_mask = YCPUUnsignedConditionalJumpFlagBitmasks
      [(ctx->instruction->opcode & 0x0F) / 2];
  bool flag_value = (ctx->cpu->flags & flag_mask) != 0;
  // Even opcode => jump if the flag is set
  // Odd opcode => jump if the flag is not set
  bool success_value = ((ctx->instruction->opcode & 0x1) == 0);
  return YCPUExecuteConditionalJump(ctx, flag_value, success_value);
}

// JL/JGNE and JNL/JGE
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
YCPUExecuteSignedConditionalJumpJLOrJNL(const YCPUInstructionContext* ctx) {
  const bool is_greater_or_equal =
      YCPUGetFlag(ctx->cpu, kSF) == YCPUGetFlag(ctx->cpu, kOF);
  const bool success_value = (ctx->instruction->opcode & 0x1);
  return YCPUExecuteConditionalJump(ctx, is_greater_or_equal, success_value);
}

// JLE/JG and JNLE/JG
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSignedConditionalJumpJLEOrJNLE(const YCPUInstructionContext* ctx) {
  const bool is_greater =
      !YCPUGetFlag(ctx->cpu, kZF) &&
      (YCPUGetFlag(ctx->cpu, kSF) == YCPUGetFlag(ctx->cpu, kOF));
  const bool success_value = (ctx->instruction->opcode & 0x1);
  return YCPUExecuteConditionalJump(ctx, is_greater, success_value);
}

// ============================================================================
// Loop instructions
// ============================================================================

// LOOP rel8
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
YCPUExecuteLoop(const YCPUInstructionContext* ctx) {
  return YCPUExecuteConditionalJump(
      ctx, --(ctx->cpu->registers[kCX]) != 0, true);
}

// LOOPZ rel8
// LOOPNZ rel8
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoopZOrNZ(const YCPUInstructionContext* ctx) {
  bool condition1 = --(ctx->cpu->registers[kCX]) != 0;
  bool condition2 =
      YCPUGetFlag(ctx->cpu, kZF) == (bool)(ctx->instruction->opcode - 0xE0);
  return YCPUExecuteConditionalJump(ctx, condition1 && condition2, true);
}

// JCXZ rel8
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteJumpIfCXIsZero(const YCPUInstructionContext* ctx) {
  return YCPUExecuteConditionalJump(ctx, ctx->cpu->registers[kCX] == 0, true);
}

// ============================================================================
// CALL and RET instructions
// ============================================================================

// Common logic for near calls.
YAX86_FILE_PRIVATE YCPUInstructionResult YCPUExecuteNearCall(
    const YCPUInstructionContext* ctx, YCPUOperandValue offset) {
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[kIP]);
  return YCPUExecuteRelativeJump(ctx, offset);
}

// CALL rel16
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
YCPUExecuteDirectNearCall(const YCPUInstructionContext* ctx) {
  YCPUOperandValue offset = YCPUReadImmediate(ctx);
  return YCPUExecuteNearCall(ctx, offset);
}

// Common logic for far calls.
YAX86_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteFarCall(
    const YCPUInstructionContext* ctx, YCPUOperandValue segment,
    YCPUOperandValue offset) {
  // Push the current CS and IP onto the stack.
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[kCS]);
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[kIP]);
  return YCPUExecuteFarJump(ctx, segment, offset);
}

// CALL ptr16:16
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDirectFarCall(const YCPUInstructionContext* ctx) {
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[kCS]);
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[kIP]);
  return YCPUExecuteDirectFarJump(ctx);
}

// Common logic for RET instructions.
YAX86_FILE_PRIVATE YCPUInstructionResult YCPUExecuteNearReturnCommon(
    const YCPUInstructionContext* ctx, uint16_t arg_size) {
  YCPUOperandValue new_ip = YCPUPop(ctx->cpu);
  ctx->cpu->registers[kIP] = YCPUFromOperandValue(new_ip);
  ctx->cpu->registers[kSP] += arg_size;
  return kInstructionExecuted;
}

// RET
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
YCPUExecuteNearReturn(const YCPUInstructionContext* ctx) {
  return YCPUExecuteNearReturnCommon(ctx, 0);
}

// RET imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteNearReturnAndPop(const YCPUInstructionContext* ctx) {
  YCPUOperandValue arg_size_value = YCPUReadImmediate(ctx);
  return YCPUExecuteNearReturnCommon(ctx, YCPUFromOperandValue(arg_size_value));
}

// Common logic for RETF instructions.
YAX86_FILE_PRIVATE YCPUInstructionResult YCPUExecuteFarReturnCommon(
    const YCPUInstructionContext* ctx, uint16_t arg_size) {
  YCPUOperandValue new_ip = YCPUPop(ctx->cpu);
  YCPUOperandValue new_cs = YCPUPop(ctx->cpu);
  ctx->cpu->registers[kIP] = YCPUFromOperandValue(new_ip);
  ctx->cpu->registers[kCS] = YCPUFromOperandValue(new_cs);
  ctx->cpu->registers[kSP] += arg_size;
  return kInstructionExecuted;
}

// RETF
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteFarReturn(const YCPUInstructionContext* ctx) {
  return YCPUExecuteFarReturnCommon(ctx, 0);
}

// RETF imm16
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteFarReturnAndPop(const YCPUInstructionContext* ctx) {
  YCPUOperandValue arg_size_value = YCPUReadImmediate(ctx);
  return YCPUExecuteFarReturnCommon(ctx, YCPUFromOperandValue(arg_size_value));
}

// ============================================================================
// Interrupt instructions
// ============================================================================

// Common logic for returning from an interrupt.
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteReturnFromInterrupt(YCPUState* cpu) {
  YCPUOperandValue ip_value = YCPUPop(cpu);
  cpu->registers[kIP] = YCPUFromOperandValue(ip_value);
  YCPUOperandValue cs_value = YCPUPop(cpu);
  cpu->registers[kCS] = YCPUFromOperandValue(cs_value);
  YCPUOperandValue flags_value = YCPUPop(cpu);
  cpu->flags = YCPUToFlagsRegisterValue(YCPUFromOperandValue(flags_value));
  return kInstructionExecuted;
}

// IRET
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteIret(const YCPUInstructionContext* ctx) {
  return YCPUExecuteReturnFromInterrupt(ctx->cpu);
}

// INT 3
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInt3(const YCPUInstructionContext* ctx) {
  YCPURaiseInternalInterrupt(ctx->cpu, kInterruptBreakpoint);
  return kInstructionExecuted;
}

// INTO
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInto(const YCPUInstructionContext* ctx) {
  if (YCPUGetFlag(ctx->cpu, kOF)) {
    YCPURaiseInternalInterrupt(ctx->cpu, kInterruptOverflow);
  }
  return kInstructionExecuted;
}

// INT n
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteIntN(const YCPUInstructionContext* ctx) {
  YCPUOperandValue interrupt_number_value = YCPUReadImmediate(ctx);
  YCPURaiseInternalInterrupt(
      ctx->cpu, YCPUFromOperandValue(interrupt_number_value));
  return kInstructionExecuted;
}

// HLT
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteHlt(YAX86_UNUSED const YCPUInstructionContext* ctx) {
  // HLT executes successfully - it just leaves the CPU halted until an
  // interrupt wakes it. YCPUTick() reports the halted state to its caller.
  ctx->cpu->is_halted = true;
  return kInstructionExecuted;
}
