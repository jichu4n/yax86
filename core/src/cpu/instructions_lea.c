#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// LEA instruction
// ============================================================================

// LEA r16, m
YAX86_PRIVATE ExecuteStatus
ExecuteLoadEffectiveAddress(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  MemoryAddress memory_address =
      GetMemoryOperandAddress(ctx->cpu, ctx->instruction);
  uint32_t raw_address = ToRawAddress(ctx->cpu, &memory_address);
  WriteOperandAddress(ctx, &dest.address, raw_address);
  return kExecuteSuccess;
}

// ============================================================================
// LES and LDS instructions
// ============================================================================

// Common logic for LES and LDS instructions.
static ExecuteStatus ExecuteLoadSegmentWithPointer(
    const InstructionContext* ctx, RegisterIndex segment_register_index) {
  Operand destRegister = ReadRegisterOperand(ctx);
  Operand destSegmentRegister =
      ReadRegisterOperandForRegisterIndex(ctx, segment_register_index);

  OperandAddress src_address = {
      .type = kOperandAddressTypeMemory,
      .value = {
          .memory_address = GetMemoryOperandAddress(ctx->cpu, ctx->instruction),
      }};
  OperandValue src_offset_value = ReadMemoryOperandWord(ctx->cpu, &src_address);
  src_address.value.memory_address.offset += 2;
  OperandValue src_segment_value =
      ReadMemoryOperandWord(ctx->cpu, &src_address);

  WriteOperand(ctx, &destRegister, FromOperandValue(&src_offset_value));
  WriteOperand(ctx, &destSegmentRegister, FromOperandValue(&src_segment_value));
  return kExecuteSuccess;
}

// LES r16, m
YAX86_PRIVATE ExecuteStatus
ExecuteLoadESWithPointer(const InstructionContext* ctx) {
  return ExecuteLoadSegmentWithPointer(ctx, kES);
}

// LDS r16, m
YAX86_PRIVATE ExecuteStatus
ExecuteLoadDSWithPointer(const InstructionContext* ctx) {
  return ExecuteLoadSegmentWithPointer(ctx, kDS);
}
