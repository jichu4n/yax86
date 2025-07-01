#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 4 - INC, DEC
// ============================================================================

typedef ExecuteStatus (*Group4ExecuteInstructionFn)(
    const InstructionContext* ctx, Operand* dest);

// Group 4 instruction implementations, indexed by the corresponding REG field
// value in the ModRM byte.
static const Group4ExecuteInstructionFn kGroup4ExecuteInstructionFns[] = {
    ExecuteInc,  // 0 - INC
    ExecuteDec,  // 1 - DEC
};

// Group 4 instruction handler.
YAX86_PRIVATE ExecuteStatus ExecuteGroup4Instruction(const InstructionContext* ctx) {
  const Group4ExecuteInstructionFn fn =
      kGroup4ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  Operand dest = ReadRegisterOrMemoryOperand(ctx);
  return fn(ctx, &dest);
}
