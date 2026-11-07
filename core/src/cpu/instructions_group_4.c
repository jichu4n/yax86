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
YAX86_FILE_PRIVATE const YCPUGroup4ExecuteInstructionFn
    kGroup4ExecuteInstructionFns[] = {
        ExecuteInc,  // 0 - INC
        ExecuteDec,  // 1 - DEC
};

enum {
  // Number of documented REG field values for this group.
  kNumGroup4Instructions = 2,
};

// Group 4 instruction handler.
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteGroup4Instruction(const YCPUInstructionContext* ctx) {
  // On real hardware REG 2-7 decode as byte-operand forms of the Group 5
  // instructions rather than being rejected. That behavior is deliberately not
  // emulated: it is unverifiable without hardware, and no assembler emits
  // these encodings. The bounds check matters regardless, because the REG
  // field is three bits wide and this table has only two entries.
  if (ctx->instruction->mod_rm.reg >= kNumGroup4Instructions) {
    return kInstructionInvalid;
  }
  const YCPUGroup4ExecuteInstructionFn fn =
      kGroup4ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand dest;
  ReadRegisterOrMemoryOperand(ctx, &dest);
  return fn(ctx, &dest);
}
