#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// String instructions
// ============================================================================

// Get the repetition prefix of a string instruction, if any.
static inline uint8_t GetRepetitionPrefix(const InstructionContext* ctx) {
  return ctx->instruction->repetition_prefix;
}

// Get the source operand for string instructions. Typically DS:SI but can be
// overridden by a segment override prefix.
static Operand GetStringSourceOperand(const InstructionContext* ctx) {
  OperandAddress address = {
      .type = kOperandAddressTypeMemory,
      .value =
          {
              .memory_address =
                  {
                      .segment_register_index = kDS,
                      .offset = ctx->cpu->registers[kSI],
                  },
          },
  };
  ApplySegmentOverride(ctx->instruction, &address.value.memory_address);
  Operand operand = {
      .address = address,
      .value = ReadOperandValue(ctx, &address),
  };
  return operand;
}

// Get the destination operand address for string instructions. Always ES:DI.
static OperandAddress GetStringDestinationOperandAddress(
    const InstructionContext* ctx) {
  OperandAddress address = {
      .type = kOperandAddressTypeMemory,
      .value =
          {
              .memory_address =
                  {
                      .segment_register_index = kES,
                      .offset = ctx->cpu->registers[kDI],
                  },
          },
  };
  return address;
}

// Get the destination operand for string instructions. Always ES:DI.
static Operand GetStringDestinationOperand(const InstructionContext* ctx) {
  OperandAddress address = GetStringDestinationOperandAddress(ctx);
  Operand operand = {
      .address = address,
      .value = ReadOperandValue(ctx, &address),
  };
  return operand;
}

// Update the source address register (SI) after a string operation.
static void UpdateStringSourceAddress(const InstructionContext* ctx) {
  if (CPUGetFlag(ctx->cpu, kDF)) {
    ctx->cpu->registers[kSI] -= kNumBytes[ctx->metadata->width];
  } else {
    ctx->cpu->registers[kSI] += kNumBytes[ctx->metadata->width];
  }
}

// Update the destination address register (DI) after a string operation.
static void UpdateStringDestinationAddress(const InstructionContext* ctx) {
  if (CPUGetFlag(ctx->cpu, kDF)) {
    ctx->cpu->registers[kDI] -= kNumBytes[ctx->metadata->width];
  } else {
    ctx->cpu->registers[kDI] += kNumBytes[ctx->metadata->width];
  }
}

// Execute a string instruction with optional REP prefix.
//
// MOVS, STOS and LODS set no flags, so a repetition prefix has no zero flag to
// test and the 8086/8088 does not tell the two prefixes apart here: 0xF2
// repeats exactly as 0xF3 does, counting CX down to zero. Only the comparison
// string instructions read the prefix as a condition.
static InstructionResult ExecuteStringInstructionWithREPPrefix(
    const InstructionContext* ctx,
    InstructionResult (*fn)(const InstructionContext*)) {
  uint8_t prefix = GetRepetitionPrefix(ctx);
  if (prefix != kPrefixREP && prefix != kPrefixREPNZ) {
    return fn(ctx);
  }
  while (ctx->cpu->registers[kCX]) {
    InstructionResult status = fn(ctx);
    if (status != kInstructionExecuted) {
      return status;
    }
    --ctx->cpu->registers[kCX];
  }
  return kInstructionExecuted;
}

// Single MOVS iteration.
static InstructionResult ExecuteMovsIteration(const InstructionContext* ctx) {
  Operand src = GetStringSourceOperand(ctx);
  OperandAddress dest_address = GetStringDestinationOperandAddress(ctx);
  WriteOperandAddress(ctx, &dest_address, FromOperand(&src));
  UpdateStringSourceAddress(ctx);
  UpdateStringDestinationAddress(ctx);
  return kInstructionExecuted;
}

// MOVS
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteMovs(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPPrefix(ctx, ExecuteMovsIteration);
}

// Single STOS iteration.
static InstructionResult ExecuteStosIteration(const InstructionContext* ctx) {
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandAddress dest_address = GetStringDestinationOperandAddress(ctx);
  WriteOperandAddress(ctx, &dest_address, FromOperand(&src));
  UpdateStringDestinationAddress(ctx);
  return kInstructionExecuted;
}

// STOS
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteStos(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPPrefix(ctx, ExecuteStosIteration);
}

// Single LODS iteration.
static InstructionResult ExecuteLodsIteration(const InstructionContext* ctx) {
  Operand src = GetStringSourceOperand(ctx);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  WriteOperand(ctx, &dest, FromOperand(&src));
  UpdateStringSourceAddress(ctx);
  return kInstructionExecuted;
}

// LODS
YAX86_HOT YAX86_PRIVATE InstructionResult
ExecuteLods(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPPrefix(ctx, ExecuteLodsIteration);
}

// Execute a string instruction with optional REPZ/REPE or REPNZ/REPNE prefix.
static InstructionResult ExecuteStringInstructionWithREPZOrRepNZPrefix(
    const InstructionContext* ctx,
    InstructionResult (*fn)(const InstructionContext*)) {
  uint8_t prefix = GetRepetitionPrefix(ctx);
  if (prefix != kPrefixREP && prefix != kPrefixREPNZ) {
    return fn(ctx);
  }
  bool terminate_zf_value = prefix == kPrefixREPNZ;
  while (ctx->cpu->registers[kCX]) {
    InstructionResult status = fn(ctx);
    if (status != kInstructionExecuted) {
      return status;
    }
    --ctx->cpu->registers[kCX];
    if (CPUGetFlag(ctx->cpu, kZF) == terminate_zf_value) {
      break;
    }
  }
  return kInstructionExecuted;
}

// Single SCAS iteration.
YAX86_HOT static InstructionResult ExecuteScasIteration(
    const InstructionContext* ctx) {
  Operand src = GetStringDestinationOperand(ctx);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  ExecuteCmp(ctx, &dest, &src.value);
  UpdateStringDestinationAddress(ctx);
  return kInstructionExecuted;
}

// SCAS
YAX86_PRIVATE InstructionResult ExecuteScas(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPZOrRepNZPrefix(
      ctx, ExecuteScasIteration);
}

// Single CMPS iteration.
static InstructionResult ExecuteCmpsIteration(const InstructionContext* ctx) {
  Operand dest = GetStringSourceOperand(ctx);
  Operand src = GetStringDestinationOperand(ctx);
  ExecuteCmp(ctx, &dest, &src.value);
  UpdateStringSourceAddress(ctx);
  UpdateStringDestinationAddress(ctx);
  return kInstructionExecuted;
}

// CMPS
YAX86_PRIVATE InstructionResult ExecuteCmps(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPZOrRepNZPrefix(
      ctx, ExecuteCmpsIteration);
}
