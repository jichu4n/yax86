#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 1 - ADD, OR, ADC, SBB, AND, SUB, XOR, CMP
// ============================================================================

typedef YCPUInstructionResult (*YCPUGroup1ExecuteInstructionFn)(
    const YCPUInstructionContext* ctx, YCPUOperand* dest, YCPUOperandValue src);

// Group 1 instruction implementations, indexed by the corresponding REG field
// value in the YCPUModRM byte.
Y_FILE_PRIVATE const YCPUGroup1ExecuteInstructionFn
    YCPUGroup1ExecuteInstructionFns[] = {
        YCPUExecuteAdd,            // 0 - ADD
        YCPUExecuteBooleanOr,      // 1 - OR
        YCPUExecuteAddWithCarry,   // 2 - ADC
        YCPUExecuteSubWithBorrow,  // 3 - SBB
        YCPUExecuteBooleanAnd,     // 4 - AND
        YCPUExecuteSub,            // 5 - SUB
        YCPUExecuteBooleanXor,     // 6 - XOR
        YCPUExecuteCmp,            // 7 - CMP
};

// Group 1 instruction handler.
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteGroup1Instruction(const YCPUInstructionContext* ctx) {
  const YCPUGroup1ExecuteInstructionFn fn =
      YCPUGroup1ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return fn(ctx, &dest, src_value);
}

// Group 1 instruction handler, but sign-extends the 8-bit immediate value.
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteGroup1InstructionWithSignExtension(
    const YCPUInstructionContext* ctx) {
  const YCPUGroup1ExecuteInstructionFn fn =
      YCPUGroup1ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperandValue src_value = YCPUReadImmediateOperandByte(
      ctx->instruction);  // immediate is always 8-bit
  YCPUOperandValue src_value_extended =
      (YCPUOperandValue)((int16_t)((int8_t)src_value));
  // Sign-extend the immediate value to the destination width.
  return fn(ctx, &dest, src_value_extended);
}
