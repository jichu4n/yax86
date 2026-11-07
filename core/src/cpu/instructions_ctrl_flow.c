#ifndef Y_IMPLEMENTATION
#include "../util/common.h"
#include "cycles.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // Y_IMPLEMENTATION

// ============================================================================
// JMP instructions
// ============================================================================

// Jump to a relative signed byte offset.
Y_FILE_PRIVATE Y_HOT YCPUInstructionResult YCPUExecuteRelativeJumpByte(
    const YCPUInstructionContext* ctx, YCPUOperandValue offset_value) {
  ctx->cpu->registers[Y_CPU_IP] = YCPUAddSignedOffsetByte(
      ctx->cpu->registers[Y_CPU_IP], YCPUFromOperandValue(offset_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Jump to a relative signed word offset.
Y_FILE_PRIVATE Y_HOT YCPUInstructionResult YCPUExecuteRelativeJumpWord(
    const YCPUInstructionContext* ctx, YCPUOperandValue offset_value) {
  ctx->cpu->registers[Y_CPU_IP] = YCPUAddSignedOffsetWord(
      ctx->cpu->registers[Y_CPU_IP], YCPUFromOperandValue(offset_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Table of relative jump instructions, indexed by width.
Y_FILE_PRIVATE
YCPUInstructionResult (*const YCPURelativeJumpFn[Y_CPU_NUM_WIDTHS])(
    const YCPUInstructionContext* ctx, YCPUOperandValue offset_value) = {
    YCPUExecuteRelativeJumpByte,  // Y_CPU_BYTE
    YCPUExecuteRelativeJumpWord,  // Y_CPU_WORD
};

// Common logic for JMP instructions.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteRelativeJump(
    const YCPUInstructionContext* ctx, YCPUOperandValue offset_value) {
  return YCPURelativeJumpFn[ctx->metadata->width](ctx, offset_value);
}

// JMP rel8
// JMP rel16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteShortOrNearJump(const YCPUInstructionContext* ctx) {
  YCPUOperandValue offset_value = YCPUReadImmediate(ctx);
  return YCPUExecuteRelativeJump(ctx, offset_value);
}

// Common logic for far jumps.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteFarJump(
    const YCPUInstructionContext* ctx, YCPUOperandValue segment,
    YCPUOperandValue offset) {
  ctx->cpu->registers[Y_CPU_CS] = YCPUFromOperandValue(segment);
  ctx->cpu->registers[Y_CPU_IP] = YCPUFromOperandValue(offset);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// JMP ptr16:16
Y_MODULE_PRIVATE YCPUInstructionResult
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
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteConditionalJump(
    const YCPUInstructionContext* ctx, bool value, bool success_value) {
  if (value == success_value) {
    // A taken jump throws away the prefetch queue and has to fill it again.
    // The base cost in the cycle table is for the branch not being taken.
    YCPUAddCycles(ctx->cpu, Y_CPU_JUMP_TAKEN_CYCLES);
    YCPUOperandValue offset_value = YCPUReadImmediate(ctx);
    return YCPUExecuteRelativeJump(ctx, offset_value);
  }
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Table of flag register bitmasks for conditional jumps. The index corresponds
// to (opcode & 0x0F) / 2, so that the undocumented 0x60-0x6F aliases share the
// entries of their 0x70-0x7F counterparts.
Y_FILE_PRIVATE const uint16_t YCPUUnsignedConditionalJumpFlagBitmasks[] = {
    Y_CPU_OF,             // 0x70 - JO, 0x71 - JNO
    Y_CPU_CF,             // 0x72 - JC, 0x73 - JNC
    Y_CPU_ZF,             // 0x74 - JE, 0x75 - JNE
    Y_CPU_CF | Y_CPU_ZF,  // 0x76 - JBE, 0x77 - JNBE
    Y_CPU_SF,             // 0x78 - JS, 0x79 - JNS
    Y_CPU_PF,             // 0x7A - JP, 0x7B - JNP
};

// Unsigned conditional jumps.
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
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
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteSignedConditionalJumpJLOrJNL(const YCPUInstructionContext* ctx) {
  const bool is_greater_or_equal =
      YCPUGetFlag(ctx->cpu, Y_CPU_SF) == YCPUGetFlag(ctx->cpu, Y_CPU_OF);
  const bool success_value = (ctx->instruction->opcode & 0x1);
  return YCPUExecuteConditionalJump(ctx, is_greater_or_equal, success_value);
}

// JLE/JG and JNLE/JG
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSignedConditionalJumpJLEOrJNLE(const YCPUInstructionContext* ctx) {
  const bool is_greater =
      !YCPUGetFlag(ctx->cpu, Y_CPU_ZF) &&
      (YCPUGetFlag(ctx->cpu, Y_CPU_SF) == YCPUGetFlag(ctx->cpu, Y_CPU_OF));
  const bool success_value = (ctx->instruction->opcode & 0x1);
  return YCPUExecuteConditionalJump(ctx, is_greater, success_value);
}

// ============================================================================
// Loop instructions
// ============================================================================

// LOOP rel8
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteLoop(const YCPUInstructionContext* ctx) {
  return YCPUExecuteConditionalJump(
      ctx, --(ctx->cpu->registers[Y_CPU_CX]) != 0, true);
}

// LOOPZ rel8
// LOOPNZ rel8
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoopZOrNZ(const YCPUInstructionContext* ctx) {
  bool condition1 = --(ctx->cpu->registers[Y_CPU_CX]) != 0;
  bool condition2 = YCPUGetFlag(ctx->cpu, Y_CPU_ZF) ==
                    (bool)(ctx->instruction->opcode - 0xE0);
  return YCPUExecuteConditionalJump(ctx, condition1 && condition2, true);
}

// JCXZ rel8
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteJumpIfCXIsZero(const YCPUInstructionContext* ctx) {
  return YCPUExecuteConditionalJump(
      ctx, ctx->cpu->registers[Y_CPU_CX] == 0, true);
}

// ============================================================================
// CALL and RET instructions
// ============================================================================

// Common logic for near calls.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteNearCall(
    const YCPUInstructionContext* ctx, YCPUOperandValue offset) {
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[Y_CPU_IP]);
  return YCPUExecuteRelativeJump(ctx, offset);
}

// CALL rel16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteDirectNearCall(const YCPUInstructionContext* ctx) {
  YCPUOperandValue offset = YCPUReadImmediate(ctx);
  return YCPUExecuteNearCall(ctx, offset);
}

// Common logic for far calls.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteFarCall(
    const YCPUInstructionContext* ctx, YCPUOperandValue segment,
    YCPUOperandValue offset) {
  // Push the current CS and IP onto the stack.
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[Y_CPU_CS]);
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[Y_CPU_IP]);
  return YCPUExecuteFarJump(ctx, segment, offset);
}

// CALL ptr16:16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDirectFarCall(const YCPUInstructionContext* ctx) {
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[Y_CPU_CS]);
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[Y_CPU_IP]);
  return YCPUExecuteDirectFarJump(ctx);
}

// Common logic for RET instructions.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteNearReturnCommon(
    const YCPUInstructionContext* ctx, uint16_t arg_size) {
  YCPUOperandValue new_ip = YCPUPop(ctx->cpu);
  ctx->cpu->registers[Y_CPU_IP] = YCPUFromOperandValue(new_ip);
  ctx->cpu->registers[Y_CPU_SP] += arg_size;
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// RET
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteNearReturn(const YCPUInstructionContext* ctx) {
  return YCPUExecuteNearReturnCommon(ctx, 0);
}

// RET imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteNearReturnAndPop(const YCPUInstructionContext* ctx) {
  YCPUOperandValue arg_size_value = YCPUReadImmediate(ctx);
  return YCPUExecuteNearReturnCommon(ctx, YCPUFromOperandValue(arg_size_value));
}

// Common logic for RETF instructions.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteFarReturnCommon(
    const YCPUInstructionContext* ctx, uint16_t arg_size) {
  YCPUOperandValue new_ip = YCPUPop(ctx->cpu);
  YCPUOperandValue new_cs = YCPUPop(ctx->cpu);
  ctx->cpu->registers[Y_CPU_IP] = YCPUFromOperandValue(new_ip);
  ctx->cpu->registers[Y_CPU_CS] = YCPUFromOperandValue(new_cs);
  ctx->cpu->registers[Y_CPU_SP] += arg_size;
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// RETF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteFarReturn(const YCPUInstructionContext* ctx) {
  return YCPUExecuteFarReturnCommon(ctx, 0);
}

// RETF imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteFarReturnAndPop(const YCPUInstructionContext* ctx) {
  YCPUOperandValue arg_size_value = YCPUReadImmediate(ctx);
  return YCPUExecuteFarReturnCommon(ctx, YCPUFromOperandValue(arg_size_value));
}

// ============================================================================
// Interrupt instructions
// ============================================================================

// Common logic for returning from an interrupt.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteReturnFromInterrupt(YCPUState* cpu) {
  YCPUOperandValue ip_value = YCPUPop(cpu);
  cpu->registers[Y_CPU_IP] = YCPUFromOperandValue(ip_value);
  YCPUOperandValue cs_value = YCPUPop(cpu);
  cpu->registers[Y_CPU_CS] = YCPUFromOperandValue(cs_value);
  YCPUOperandValue flags_value = YCPUPop(cpu);
  cpu->flags = YCPUToFlagsRegisterValue(YCPUFromOperandValue(flags_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// IRET
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteIret(const YCPUInstructionContext* ctx) {
  return YCPUExecuteReturnFromInterrupt(ctx->cpu);
}

// INT 3
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInt3(const YCPUInstructionContext* ctx) {
  YCPURaiseInternalInterrupt(ctx->cpu, Y_CPU_INTERRUPT_BREAKPOINT);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// INTO
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInto(const YCPUInstructionContext* ctx) {
  if (YCPUGetFlag(ctx->cpu, Y_CPU_OF)) {
    YCPURaiseInternalInterrupt(ctx->cpu, Y_CPU_INTERRUPT_OVERFLOW);
  }
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// INT n
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteIntN(const YCPUInstructionContext* ctx) {
  YCPUOperandValue interrupt_number_value = YCPUReadImmediate(ctx);
  YCPURaiseInternalInterrupt(
      ctx->cpu, YCPUFromOperandValue(interrupt_number_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// HLT
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteHlt(Y_UNUSED const YCPUInstructionContext* ctx) {
  // HLT executes successfully - it just leaves the CPU halted until an
  // interrupt wakes it. YCPUTick() reports the halted state to its caller.
  ctx->cpu->is_halted = true;
  return Y_CPU_INSTRUCTION_EXECUTED;
}
