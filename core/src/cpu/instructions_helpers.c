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
  CPUSetFlag(ctx->cpu, kZF, result == 0);
  // Sign flag (SF)
  CPUSetFlag(ctx->cpu, kSF, result & kSignBit[width]);
  // Parity flag (PF)
  // Set if the number of set bits in the least significant byte is even
  uint8_t parity = result & 0xFF;  // Check only the low byte for parity
  parity ^= parity >> 4;
  parity ^= parity >> 2;
  parity ^= parity >> 1;
  CPUSetFlag(ctx->cpu, kPF, (parity & 1) == 0);
}

YAX86_PRIVATE uint16_t ToFlagsRegisterValue(uint16_t value) {
  return (value | (uint16_t)kFlagsAlwaysSet) & ~(uint16_t)kFlagsAlwaysClear;
}

// Write a word where the stack pointer already points. The caller is
// responsible for having made room for it.
static void WriteToStackTop(CPUState* cpu, OperandValue value) {
  OperandAddress address = {
      .type = kOperandAddressTypeMemory,
      .value = {
          .memory_address = {
              .segment_register_index = kSS,
              .offset = cpu->registers[kSP],
          }}};
  WriteMemoryOperandWord(cpu, &address, value);
}

YAX86_PRIVATE void PushValue(CPUState* cpu, OperandValue value) {
  cpu->registers[kSP] -= 2;
  WriteToStackTop(cpu, value);
}

YAX86_PRIVATE void PushSourceOperand(CPUState* cpu, const Operand* src) {
  cpu->registers[kSP] -= 2;
  // The 8086/8088 moves the stack pointer before it reads the source, so
  // PUSH SP stores the value SP has after the decrement rather than the one it
  // had on entry. SP is the only source that can tell the difference. The
  // 80286 and later store the entry value instead.
  const bool source_is_stack_pointer =
      src->address.type == kOperandAddressTypeRegister &&
      src->address.value.register_address.register_index == kSP;
  const OperandValue value =
      source_is_stack_pointer ? WordValue(cpu->registers[kSP]) : src->value;
  WriteToStackTop(cpu, value);
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
YAX86_PRIVATE InstructionResult
ExecuteNoOp(YAX86_UNUSED const InstructionContext* ctx) {
  return kInstructionExecuted;
}
