#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// Set common CPU flags after an instruction. This includes:
// - Zero flag (ZF)
// - Sign flag (SF)
// - Parity Flag (PF)
YAX86_PRIVATE void SetCommonFlagsAfterInstruction(
    const InstructionContext* ctx, uint32_t result) {
  Width width = ctx->metadata->width;
  result &= kMaxValue[width];
  // Zero flag (ZF)
  SetFlag(ctx->cpu, kZF, result == 0);
  // Sign flag (SF)
  SetFlag(ctx->cpu, kSF, result & kSignBit[width]);
  // Parity flag (PF)
  // Set if the number of set bits in the least significant byte is even
  uint8_t parity = result & 0xFF;  // Check only the low byte for parity
  parity ^= parity >> 4;
  parity ^= parity >> 2;
  parity ^= parity >> 1;
  SetFlag(ctx->cpu, kPF, (parity & 1) == 0);
}

YAX86_PRIVATE void Push(CPUState* cpu, OperandValue value) {
  cpu->registers[kSP] -= 2;
  OperandAddress address = {
      .type = kOperandAddressTypeMemory,
      .value = {
          .memory_address = {
              .segment_register_index = kSS,
              .offset = cpu->registers[kSP],
          }}};
  WriteMemoryOperandWord(cpu, &address, value);
}

YAX86_PRIVATE OperandValue Pop(CPUState* cpu) {
  OperandAddress address = {
      .type = kOperandAddressTypeMemory,
      .value = {
          .memory_address = {
              .segment_register_index = kSS,
              .offset = cpu->registers[kSP],
          }}};
  OperandValue value = ReadMemoryOperandWord(cpu, &address);
  cpu->registers[kSP] += 2;
  return value;
}

// Dummy instruction for unsupported opcodes.
YAX86_PRIVATE ExecuteStatus ExecuteNoOp(const InstructionContext* ctx) {
  (void)ctx;
  return kExecuteSuccess;
}
