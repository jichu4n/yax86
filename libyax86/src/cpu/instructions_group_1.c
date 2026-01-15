#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 1 - ADD, OR, ADC, SBB, AND, SUB, XOR, CMP
// ============================================================================

typedef ExecuteStatus (*Group1ExecuteInstructionFn)(
    const InstructionContext* ctx, Operand* dest, const OperandValue* src);

// Group 1 instruction implementations, indexed by the corresponding REG field
// value in the ModRM byte.
static const Group1ExecuteInstructionFn kGroup1ExecuteInstructionFns[] = {
    ExecuteAdd,            // 0 - ADD
    ExecuteBooleanOr,      // 1 - OR
    ExecuteAddWithCarry,   // 2 - ADC
    ExecuteSubWithBorrow,  // 3 - SBB
    ExecuteBooleanAnd,     // 4 - AND
    ExecuteSub,            // 5 - SUB
    ExecuteBooleanXor,     // 6 - XOR
    ExecuteCmp,            // 7 - CMP
};

// Group 1 instruction handler.
YAX86_PRIVATE ExecuteStatus
ExecuteGroup1Instruction(const InstructionContext* ctx) {
  const Group1ExecuteInstructionFn fn =
      kGroup1ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  OperandValue src_value = ReadImmediate(ctx);
  return fn(ctx, &dest, &src_value);
}

// Group 1 instruction handler, but sign-extends the 8-bit immediate value.
YAX86_PRIVATE ExecuteStatus
ExecuteGroup1InstructionWithSignExtension(const InstructionContext* ctx) {
  const Group1ExecuteInstructionFn fn =
      kGroup1ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  OperandValue src_value =
      ReadImmediateOperandByte(ctx->instruction);  // immediate is always 8-bit
  OperandValue src_value_extended =
      WordValue((uint16_t)((int16_t)((int8_t)src_value.value.byte_value)));
  // Sign-extend the immediate value to the destination width.
  return fn(ctx, &dest, &src_value_extended);
}
