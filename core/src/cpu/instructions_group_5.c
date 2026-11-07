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
YAX86_FILE_PRIVATE void GetSegmentRegisterOperandForIndirectFarJumpOrCall(
    const YCPUInstructionContext* ctx, const YCPUOperand* offset,
    YCPUOperand* operand) {
  YCPUOperandAddress segment_address = offset->address;
  segment_address.offset += 2;  // Skip the offset
  operand->address = segment_address;
  operand->value = ReadMemoryOperandWord(ctx->cpu, &segment_address);
}

// JMP ptr16
YAX86_FILE_PRIVATE YCPUInstructionResult
ExecuteIndirectNearJump(const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  ctx->cpu->registers[kIP] = FromOperandValue(dest->value);
  return kInstructionExecuted;
}

// CALL ptr16
YAX86_FILE_PRIVATE YCPUInstructionResult
ExecuteIndirectNearCall(const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  PushValue(ctx->cpu, ctx->cpu->registers[kIP]);
  return ExecuteIndirectNearJump(ctx, dest);
}

// CALL ptr16:16
YAX86_FILE_PRIVATE YCPUInstructionResult
ExecuteIndirectFarCall(const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUOperand segment;
  GetSegmentRegisterOperandForIndirectFarJumpOrCall(ctx, dest, &segment);
  return ExecuteFarCall(ctx, segment.value, dest->value);
}

// JMP ptr16:16
YAX86_FILE_PRIVATE YCPUInstructionResult
ExecuteIndirectFarJump(const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUOperand segment;
  GetSegmentRegisterOperandForIndirectFarJumpOrCall(ctx, dest, &segment);
  return ExecuteFarJump(ctx, segment.value, dest->value);
}

// PUSH r/m16
YAX86_FILE_PRIVATE YCPUInstructionResult
ExecuteIndirectPush(const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  PushSourceOperand(ctx->cpu, dest);
  return kInstructionExecuted;
}

typedef YCPUInstructionResult (*YCPUGroup5ExecuteInstructionFn)(
    const YCPUInstructionContext* ctx, YCPUOperand* dest);

// Group 5 instruction implementations, indexed by the corresponding REG
// field value in the YCPUModRM byte.
YAX86_FILE_PRIVATE const YCPUGroup5ExecuteInstructionFn
    kGroup5ExecuteInstructionFns[] = {
        ExecuteInc,               // 0 - INC r/m8/r/m16
        ExecuteDec,               // 1 - DEC r/m8/r/m16
        ExecuteIndirectNearCall,  // 2 - CALL rel16
        ExecuteIndirectFarCall,   // 3 - CALL ptr16:16
        ExecuteIndirectNearJump,  // 4 - JMP ptr16
        ExecuteIndirectFarJump,   // 5 - JMP ptr16:16
        ExecuteIndirectPush,      // 6 - PUSH r/m16
        // REG 7 is an undocumented alias of REG 6. Without this entry the
        // lookup below would read past the end of the table, since the REG
        // field is three bits wide.
        ExecuteIndirectPush,  // 7 - PUSH r/m16
};

// Group 5 instruction handler.
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteGroup5Instruction(const YCPUInstructionContext* ctx) {
  const YCPUGroup5ExecuteInstructionFn fn =
      kGroup5ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand dest;
  ReadRegisterOrMemoryOperand(ctx, &dest);
  return fn(ctx, &dest);
}
