#ifndef Y_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // Y_IMPLEMENTATION

// Set common CPU flags after an instruction. This includes:
// - Zero flag (ZF)
// - Sign flag (SF)
// - Parity Flag (PF)
Y_MODULE_PRIVATE Y_HOT void YCPUSetCommonFlagsAfterInstruction(
    const YCPUInstructionContext* ctx, uint32_t result) {
  YCPUWidth width = ctx->metadata->width;
  result &= YCPUMaxValue[width];
  // Zero flag (ZF)
  YCPUSetFlag(ctx->cpu, Y_CPU_ZF, result == 0);
  // Sign flag (SF)
  YCPUSetFlag(ctx->cpu, Y_CPU_SF, result & YCPUSignBit[width]);
  // Parity flag (PF)
  // Set if the number of set bits in the least significant byte is even
  uint8_t parity = result & 0xFF;  // Check only the low byte for parity
  parity ^= parity >> 4;
  parity ^= parity >> 2;
  parity ^= parity >> 1;
  YCPUSetFlag(ctx->cpu, Y_CPU_PF, (parity & 1) == 0);
}

Y_MODULE_PRIVATE uint16_t YCPUToFlagsRegisterValue(uint16_t value) {
  return (value | (uint16_t)Y_CPU_FLAGS_ALWAYS_SET) &
         ~(uint16_t)Y_CPU_FLAGS_ALWAYS_CLEAR;
}

// Write a word where the stack pointer already points. The caller is
// responsible for having made room for it.
Y_FILE_PRIVATE void YCPUWriteToStackTop(
    YCPUState* cpu, YCPUOperandValue value) {
  YCPUOperandAddress address = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
      .register_index = Y_CPU_SS,
      .offset = cpu->registers[Y_CPU_SP],
  };
  YCPUWriteMemoryOperandWord(cpu, &address, value);
}

Y_MODULE_PRIVATE void YCPUPushValue(YCPUState* cpu, YCPUOperandValue value) {
  cpu->registers[Y_CPU_SP] -= 2;
  YCPUWriteToStackTop(cpu, value);
}

Y_MODULE_PRIVATE void YCPUPushSourceOperand(
    YCPUState* cpu, const YCPUOperand* src) {
  cpu->registers[Y_CPU_SP] -= 2;
  // The 8086/8088 moves the stack pointer before it reads the source, so
  // PUSH SP stores the value SP has after the decrement rather than the one it
  // had on entry. SP is the only source that can tell the difference. The
  // 80286 and later store the entry value instead.
  const bool source_is_stack_pointer =
      src->address.type == Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER &&
      src->address.register_index == Y_CPU_SP;
  const YCPUOperandValue value =
      source_is_stack_pointer ? cpu->registers[Y_CPU_SP] : src->value;
  YCPUWriteToStackTop(cpu, value);
}

Y_MODULE_PRIVATE YCPUOperandValue YCPUPop(YCPUState* cpu) {
  YCPUOperandAddress address = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
      .register_index = Y_CPU_SS,
      .offset = cpu->registers[Y_CPU_SP],
  };
  YCPUOperandValue value = YCPUReadMemoryOperandWord(cpu, &address);
  cpu->registers[Y_CPU_SP] += 2;
  return value;
}

// Dummy instruction for unsupported opcodes.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteNoOp(Y_UNUSED const YCPUInstructionContext* ctx) {
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Handler for the eight opcode bytes that are prefixes rather than
// instructions. The prefix decoder consumes those bytes before an opcode is
// looked up, so a decoded instruction never names one and this is unreachable
// from YCPUTick(). It exists so that every entry in the opcode table is
// callable, which is what lets the tick path dispatch without first checking
// for a null handler.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInvalidOpcode(Y_UNUSED const YCPUInstructionContext* ctx) {
  return Y_CPU_INSTRUCTION_INVALID;
}
