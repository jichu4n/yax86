#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 4 - INC, DEC
// ============================================================================

typedef YCPUInstructionResult (*YCPUGroup4ExecuteInstructionFn)(
    const YCPUInstructionContext* ctx, YCPUOperand* dest);

// Group 4 instruction implementations, indexed by the corresponding REG field
// value in the YCPUModRM byte.
Y_FILE_PRIVATE const YCPUGroup4ExecuteInstructionFn
    YCPUGroup4ExecuteInstructionFns[] = {
        YCPUExecuteInc,  // 0 - INC
        YCPUExecuteDec,  // 1 - DEC
};

enum {
  // Number of documented REG field values for this group.
  Y_CPU_NUM_GROUP4_INSTRUCTIONS = 2,
};

// Group 4 instruction handler.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup4Instruction(const YCPUInstructionContext* ctx) {
  // On real hardware REG 2-7 decode as byte-operand forms of the Group 5
  // instructions rather than being rejected. That behavior is deliberately not
  // emulated: it is unverifiable without hardware, and no assembler emits
  // these encodings. The bounds check matters regardless, because the REG
  // field is three bits wide and this table has only two entries.
  if (ctx->instruction->mod_rm.reg >= Y_CPU_NUM_GROUP4_INSTRUCTIONS) {
    return Y_CPU_INSTRUCTION_INVALID;
  }
  const YCPUGroup4ExecuteInstructionFn fn =
      YCPUGroup4ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  return fn(ctx, &dest);
}
