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
static Operand GetSegmentRegisterOperandForIndirectFarJumpOrCall(
    const InstructionContext* ctx, const Operand* offset) {
  OperandAddress segment_address = offset->address;
  segment_address.value.memory_address.offset += 2;  // Skip the offset
  OperandValue segment_value =
      ReadMemoryOperandWord(ctx->cpu, &segment_address);
  Operand operand = {
      .address = segment_address,
      .value = segment_value,
  };
  return operand;
}

// JMP ptr16
static ExecuteStatus ExecuteIndirectNearJump(
    const InstructionContext* ctx, Operand* dest) {
  ctx->cpu->registers[kIP] = FromOperandValue(&dest->value);
  return kExecuteSuccess;
}

// CALL ptr16
static ExecuteStatus ExecuteIndirectNearCall(
    const InstructionContext* ctx, Operand* dest) {
  Push(ctx->cpu, WordValue(ctx->cpu->registers[kIP]));
  return ExecuteIndirectNearJump(ctx, dest);
}

// CALL ptr16:16
static ExecuteStatus ExecuteIndirectFarCall(
    const InstructionContext* ctx, Operand* dest) {
  Operand segment =
      GetSegmentRegisterOperandForIndirectFarJumpOrCall(ctx, dest);
  return ExecuteFarCall(ctx, &segment.value, &dest->value);
}

// JMP ptr16:16
static ExecuteStatus ExecuteIndirectFarJump(
    const InstructionContext* ctx, Operand* dest) {
  Operand segment =
      GetSegmentRegisterOperandForIndirectFarJumpOrCall(ctx, dest);
  return ExecuteFarJump(ctx, &segment.value, &dest->value);
}

// PUSH r/m16
static ExecuteStatus ExecuteIndirectPush(
    const InstructionContext* ctx, Operand* dest) {
  Push(ctx->cpu, dest->value);
  return kExecuteSuccess;
}

typedef ExecuteStatus (*Group5ExecuteInstructionFn)(
    const InstructionContext* ctx, Operand* dest);

// Group 5 instruction implementations, indexed by the corresponding REG
// field value in the ModRM byte.
static const Group5ExecuteInstructionFn kGroup5ExecuteInstructionFns[] = {
    ExecuteInc,               // 0 - INC r/m8/r/m16
    ExecuteDec,               // 1 - DEC r/m8/r/m16
    ExecuteIndirectNearCall,  // 2 - CALL rel16
    ExecuteIndirectFarCall,   // 3 - CALL ptr16:16
    ExecuteIndirectNearJump,  // 4 - JMP ptr16
    ExecuteIndirectFarJump,   // 5 - JMP ptr16:16
    ExecuteIndirectPush,      // 6 - PUSH r/m16
                              // 7 - Reserved
};

// Group 5 instruction handler.
YAX86_PRIVATE ExecuteStatus
ExecuteGroup5Instruction(const InstructionContext* ctx) {
  const Group5ExecuteInstructionFn fn =
      kGroup5ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  return fn(ctx, &dest);
}
