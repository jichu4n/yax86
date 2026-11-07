#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 5 - INC, DEC, CALL, JMP, PUSH
// ============================================================================

// Helper to get the segment register value for far JMP and CALL instructions.
YAX86_FILE_PRIVATE void YCPUGetSegmentRegisterOperandForIndirectFarJumpOrCall(
    const YCPUInstructionContext* ctx, const YCPUOperand* offset,
    YCPUOperand* operand) {
  YCPUOperandAddress segment_address = offset->address;
  segment_address.offset += 2;  // Skip the offset
  operand->address = segment_address;
  operand->value = YCPUReadMemoryOperandWord(ctx->cpu, &segment_address);
}

// JMP ptr16
YAX86_FILE_PRIVATE YCPUInstructionResult YCPUExecuteIndirectNearJump(
    const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  ctx->cpu->registers[kIP] = YCPUFromOperandValue(dest->value);
  return kInstructionExecuted;
}

// CALL ptr16
YAX86_FILE_PRIVATE YCPUInstructionResult YCPUExecuteIndirectNearCall(
    const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[kIP]);
  return YCPUExecuteIndirectNearJump(ctx, dest);
}

// CALL ptr16:16
YAX86_FILE_PRIVATE YCPUInstructionResult YCPUExecuteIndirectFarCall(
    const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUOperand segment;
  YCPUGetSegmentRegisterOperandForIndirectFarJumpOrCall(ctx, dest, &segment);
  return YCPUExecuteFarCall(ctx, segment.value, dest->value);
}

// JMP ptr16:16
YAX86_FILE_PRIVATE YCPUInstructionResult YCPUExecuteIndirectFarJump(
    const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUOperand segment;
  YCPUGetSegmentRegisterOperandForIndirectFarJumpOrCall(ctx, dest, &segment);
  return YCPUExecuteFarJump(ctx, segment.value, dest->value);
}

// PUSH r/m16
YAX86_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteIndirectPush(const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUPushSourceOperand(ctx->cpu, dest);
  return kInstructionExecuted;
}

typedef YCPUInstructionResult (*YCPUGroup5ExecuteInstructionFn)(
    const YCPUInstructionContext* ctx, YCPUOperand* dest);

// Group 5 instruction implementations, indexed by the corresponding REG
// field value in the YCPUModRM byte.
YAX86_FILE_PRIVATE const YCPUGroup5ExecuteInstructionFn
    YCPUGroup5ExecuteInstructionFns[] = {
        YCPUExecuteInc,               // 0 - INC r/m8/r/m16
        YCPUExecuteDec,               // 1 - DEC r/m8/r/m16
        YCPUExecuteIndirectNearCall,  // 2 - CALL rel16
        YCPUExecuteIndirectFarCall,   // 3 - CALL ptr16:16
        YCPUExecuteIndirectNearJump,  // 4 - JMP ptr16
        YCPUExecuteIndirectFarJump,   // 5 - JMP ptr16:16
        YCPUExecuteIndirectPush,      // 6 - PUSH r/m16
        // REG 7 is an undocumented alias of REG 6. Without this entry the
        // lookup below would read past the end of the table, since the REG
        // field is three bits wide.
        YCPUExecuteIndirectPush,  // 7 - PUSH r/m16
};

// Group 5 instruction handler.
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup5Instruction(const YCPUInstructionContext* ctx) {
  const YCPUGroup5ExecuteInstructionFn fn =
      YCPUGroup5ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  return fn(ctx, &dest);
}
