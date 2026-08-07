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
YAX86_PRIVATE InstructionResult
ExecuteLoadEffectiveAddress(const InstructionContext* ctx) {
  Operand dest = ReadRegisterOperand(ctx);
  MemoryAddress memory_address =
      GetMemoryOperandAddress(ctx->cpu, ctx->instruction);
  // LEA yields the effective address - the offset within the segment - and
  // never resolves it against a segment or touches memory. Converting to a
  // linear address here would fold the segment base into the result, which is
  // invisible only while the segment register happens to be zero.
  WriteOperandAddress(ctx, &dest.address, memory_address.offset);
  return kInstructionExecuted;
}

// ============================================================================
// LES and LDS instructions
// ============================================================================

// Common logic for LES and LDS instructions.
static InstructionResult ExecuteLoadSegmentWithPointer(
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
  return kInstructionExecuted;
}

// LES r16, m
YAX86_PRIVATE InstructionResult
ExecuteLoadESWithPointer(const InstructionContext* ctx) {
  return ExecuteLoadSegmentWithPointer(ctx, kES);
}

// LDS r16, m
YAX86_PRIVATE InstructionResult
ExecuteLoadDSWithPointer(const InstructionContext* ctx) {
  return ExecuteLoadSegmentWithPointer(ctx, kDS);
}
