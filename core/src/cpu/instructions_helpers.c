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
YAX86_MODULE_PRIVATE YAX86_HOT void YCPUSetCommonFlagsAfterInstruction(
    const YCPUInstructionContext* ctx, uint32_t result) {
  YCPUWidth width = ctx->metadata->width;
  result &= kMaxValue[width];
  // Zero flag (ZF)
  YCPUSetFlag(ctx->cpu, kZF, result == 0);
  // Sign flag (SF)
  YCPUSetFlag(ctx->cpu, kSF, result & kSignBit[width]);
  // Parity flag (PF)
  // Set if the number of set bits in the least significant byte is even
  uint8_t parity = result & 0xFF;  // Check only the low byte for parity
  parity ^= parity >> 4;
  parity ^= parity >> 2;
  parity ^= parity >> 1;
  YCPUSetFlag(ctx->cpu, kPF, (parity & 1) == 0);
}

YAX86_MODULE_PRIVATE uint16_t YCPUToFlagsRegisterValue(uint16_t value) {
  return (value | (uint16_t)kFlagsAlwaysSet) & ~(uint16_t)kFlagsAlwaysClear;
}

// Write a word where the stack pointer already points. The caller is
// responsible for having made room for it.
YAX86_FILE_PRIVATE void YCPUWriteToStackTop(
    YCPUState* cpu, YCPUOperandValue value) {
  YCPUOperandAddress address = {
      .type = kOperandAddressTypeMemory,
      .register_index = kSS,
      .offset = cpu->registers[kSP],
  };
  YCPUWriteMemoryOperandWord(cpu, &address, value);
}

YAX86_MODULE_PRIVATE void YCPUPushValue(
    YCPUState* cpu, YCPUOperandValue value) {
  cpu->registers[kSP] -= 2;
  YCPUWriteToStackTop(cpu, value);
}

YAX86_MODULE_PRIVATE void YCPUPushSourceOperand(
    YCPUState* cpu, const YCPUOperand* src) {
  cpu->registers[kSP] -= 2;
  // The 8086/8088 moves the stack pointer before it reads the source, so
  // PUSH SP stores the value SP has after the decrement rather than the one it
  // had on entry. SP is the only source that can tell the difference. The
  // 80286 and later store the entry value instead.
  const bool source_is_stack_pointer =
      src->address.type == kOperandAddressTypeRegister &&
      src->address.register_index == kSP;
  const YCPUOperandValue value =
      source_is_stack_pointer ? cpu->registers[kSP] : src->value;
  YCPUWriteToStackTop(cpu, value);
}

YAX86_MODULE_PRIVATE YCPUOperandValue YCPUPop(YCPUState* cpu) {
  YCPUOperandAddress address = {
      .type = kOperandAddressTypeMemory,
      .register_index = kSS,
      .offset = cpu->registers[kSP],
  };
  YCPUOperandValue value = YCPUReadMemoryOperandWord(cpu, &address);
  cpu->registers[kSP] += 2;
  return value;
}

// Dummy instruction for unsupported opcodes.
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteNoOp(YAX86_UNUSED const YCPUInstructionContext* ctx) {
  return kInstructionExecuted;
}

// Handler for the eight opcode bytes that are prefixes rather than
// instructions. The prefix decoder consumes those bytes before an opcode is
// looked up, so a decoded instruction never names one and this is unreachable
// from YCPUTick(). It exists so that every entry in the opcode table is
// callable, which is what lets the tick path dispatch without first checking
// for a null handler.
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInvalidOpcode(YAX86_UNUSED const YCPUInstructionContext* ctx) {
  return kInstructionInvalid;
}
