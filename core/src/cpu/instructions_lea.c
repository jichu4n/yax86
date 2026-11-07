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
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteLoadEffectiveAddress(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  ReadRegisterOperand(ctx, &dest);
  YCPUMemoryAddress memory_address =
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
YAX86_FILE_PRIVATE YCPUInstructionResult ExecuteLoadSegmentWithPointer(
    const YCPUInstructionContext* ctx,
    YCPURegisterIndex segment_register_index) {
  YCPUOperand destRegister;
  ReadRegisterOperand(ctx, &destRegister);
  YCPUOperand destSegmentRegister;
  ReadRegisterOperandForRegisterIndex(
      ctx, segment_register_index, &destSegmentRegister);

  const YCPUMemoryAddress src_memory_address =
      GetMemoryOperandAddress(ctx->cpu, ctx->instruction);
  YCPUOperandAddress src_address = {
      .type = kOperandAddressTypeMemory,
      .register_index = (uint8_t)src_memory_address.segment_register_index,
      .offset = src_memory_address.offset,
  };
  YCPUOperandValue src_offset_value =
      ReadMemoryOperandWord(ctx->cpu, &src_address);
  src_address.offset += 2;
  YCPUOperandValue src_segment_value =
      ReadMemoryOperandWord(ctx->cpu, &src_address);

  WriteOperand(ctx, &destRegister, FromOperandValue(src_offset_value));
  WriteOperand(ctx, &destSegmentRegister, FromOperandValue(src_segment_value));
  return kInstructionExecuted;
}

// LES r16, m
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteLoadESWithPointer(const YCPUInstructionContext* ctx) {
  return ExecuteLoadSegmentWithPointer(ctx, kES);
}

// LDS r16, m
YAX86_MODULE_PRIVATE YCPUInstructionResult
ExecuteLoadDSWithPointer(const YCPUInstructionContext* ctx) {
  return ExecuteLoadSegmentWithPointer(ctx, kDS);
}
