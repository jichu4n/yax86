#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Sign extension instructions
// ============================================================================

// CBW
YAX86_PRIVATE ExecuteStatus ExecuteCbw(const InstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[kAX] & 0xFF;
  uint8_t ah = (al & kSignBit[kByte]) ? 0xFF : 0x00;
  ctx->cpu->registers[kAX] = (ah << 8) | al;
  return kExecuteSuccess;
}

// CWD
YAX86_PRIVATE ExecuteStatus ExecuteCwd(const InstructionContext* ctx) {
  ctx->cpu->registers[kDX] =
      (ctx->cpu->registers[kAX] & kSignBit[kWord]) ? 0xFFFF : 0x0000;
  return kExecuteSuccess;
}
