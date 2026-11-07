#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Sign extension instructions
// ============================================================================

// CBW
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCbw(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (al & kSignBit[kByte]) ? 0xFF : 0x00;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  return kInstructionExecuted;
}

// CWD
YAX86_MODULE_PRIVATE YAX86_HOT YCPUInstructionResult
YCPUExecuteCwd(const YCPUInstructionContext* ctx) {
  ctx->cpu->registers[kDX] =
      (ctx->cpu->registers[kAX] & kSignBit[kWord]) ? 0xFFFF : 0x0000;
  return kInstructionExecuted;
}
