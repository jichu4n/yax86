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
YCPUExecuteLoadEffectiveAddress(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUMemoryAddress memory_address =
      YCPUGetMemoryOperandAddress(ctx->cpu, ctx->instruction);
  // LEA yields the effective address - the offset within the segment - and
  // never resolves it against a segment or touches memory. Converting to a
  // linear address here would fold the segment base into the result, which is
  // invisible only while the segment register happens to be zero.
  YCPUWriteOperandAddress(ctx, &dest.address, memory_address.offset);
  return kInstructionExecuted;
}

// ============================================================================
// LES and LDS instructions
// ============================================================================

// Common logic for LES and LDS instructions.
YAX86_FILE_PRIVATE YCPUInstructionResult YCPUExecuteLoadSegmentWithPointer(
    const YCPUInstructionContext* ctx,
    YCPURegisterIndex segment_register_index) {
  YCPUOperand destRegister;
  YCPUReadRegisterOperand(ctx, &destRegister);
  YCPUOperand destSegmentRegister;
  YCPUReadRegisterOperandForRegisterIndex(
      ctx, segment_register_index, &destSegmentRegister);

  const YCPUMemoryAddress src_memory_address =
      YCPUGetMemoryOperandAddress(ctx->cpu, ctx->instruction);
  YCPUOperandAddress src_address = {
      .type = kOperandAddressTypeMemory,
      .register_index = (uint8_t)src_memory_address.segment_register_index,
      .offset = src_memory_address.offset,
  };
  YCPUOperandValue src_offset_value =
      YCPUReadMemoryOperandWord(ctx->cpu, &src_address);
  src_address.offset += 2;
  YCPUOperandValue src_segment_value =
      YCPUReadMemoryOperandWord(ctx->cpu, &src_address);

  YCPUWriteOperand(ctx, &destRegister, YCPUFromOperandValue(src_offset_value));
  YCPUWriteOperand(
      ctx, &destSegmentRegister, YCPUFromOperandValue(src_segment_value));
  return kInstructionExecuted;
}

// LES r16, m
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoadESWithPointer(const YCPUInstructionContext* ctx) {
  return YCPUExecuteLoadSegmentWithPointer(ctx, kES);
}

// LDS r16, m
YAX86_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoadDSWithPointer(const YCPUInstructionContext* ctx) {
  return YCPUExecuteLoadSegmentWithPointer(ctx, kDS);
}
