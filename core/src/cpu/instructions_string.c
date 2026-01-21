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
static uint8_t GetRepetitionPrefix(const InstructionContext* ctx) {
  uint8_t prefix = 0;
  for (int i = 0; i < ctx->instruction->prefix_size; ++i) {
    switch (ctx->instruction->prefix[i]) {
      case kPrefixREP:
      case kPrefixREPNZ:
        prefix = ctx->instruction->prefix[i];
        break;
      default:
        continue;
    }
  }
  return prefix;
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
static ExecuteStatus ExecuteStringInstructionWithREPPrefix(
    const InstructionContext* ctx,
    ExecuteStatus (*fn)(const InstructionContext*)) {
  uint8_t prefix = GetRepetitionPrefix(ctx);
  if (prefix != kPrefixREP) {
    return fn(ctx);
  }
  while (ctx->cpu->registers[kCX]) {
    ExecuteStatus status = fn(ctx);
    if (status != kExecuteSuccess) {
      return status;
    }
    --ctx->cpu->registers[kCX];
  }
  return kExecuteSuccess;
}

// Single MOVS iteration.
static ExecuteStatus ExecuteMovsIteration(const InstructionContext* ctx) {
  Operand src = GetStringSourceOperand(ctx);
  OperandAddress dest_address = GetStringDestinationOperandAddress(ctx);
  WriteOperandAddress(ctx, &dest_address, FromOperand(&src));
  UpdateStringSourceAddress(ctx);
  UpdateStringDestinationAddress(ctx);
  return kExecuteSuccess;
}

// MOVS
YAX86_PRIVATE ExecuteStatus ExecuteMovs(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPPrefix(ctx, ExecuteMovsIteration);
}

// Single STOS iteration.
static ExecuteStatus ExecuteStosIteration(const InstructionContext* ctx) {
  Operand src = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  OperandAddress dest_address = GetStringDestinationOperandAddress(ctx);
  WriteOperandAddress(ctx, &dest_address, FromOperand(&src));
  UpdateStringDestinationAddress(ctx);
  return kExecuteSuccess;
}

// STOS
YAX86_PRIVATE ExecuteStatus ExecuteStos(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPPrefix(ctx, ExecuteStosIteration);
}

// Single LODS iteration.
static ExecuteStatus ExecuteLodsIteration(const InstructionContext* ctx) {
  Operand src = GetStringSourceOperand(ctx);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  WriteOperand(ctx, &dest, FromOperand(&src));
  UpdateStringSourceAddress(ctx);
  return kExecuteSuccess;
}

// LODS
YAX86_PRIVATE ExecuteStatus ExecuteLods(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPPrefix(ctx, ExecuteLodsIteration);
}

// Execute a string instruction with optional REPZ/REPE or REPNZ/REPNE prefix.
static ExecuteStatus ExecuteStringInstructionWithREPZOrRepNZPrefix(
    const InstructionContext* ctx,
    ExecuteStatus (*fn)(const InstructionContext*)) {
  uint8_t prefix = GetRepetitionPrefix(ctx);
  if (prefix != kPrefixREP && prefix != kPrefixREPNZ) {
    return fn(ctx);
  }
  bool terminate_zf_value = prefix == kPrefixREPNZ;
  while (ctx->cpu->registers[kCX]) {
    ExecuteStatus status = fn(ctx);
    if (status != kExecuteSuccess) {
      return status;
    }
    --ctx->cpu->registers[kCX];
    if (CPUGetFlag(ctx->cpu, kZF) == terminate_zf_value) {
      break;
    }
  }
  return kExecuteSuccess;
}

// Single SCAS iteration.
static ExecuteStatus ExecuteScasIteration(const InstructionContext* ctx) {
  Operand src = GetStringDestinationOperand(ctx);
  Operand dest = ReadRegisterOperandForRegisterIndex(ctx, kAX);
  ExecuteCmp(ctx, &dest, &src.value);
  UpdateStringDestinationAddress(ctx);
  return kExecuteSuccess;
}

// SCAS
YAX86_PRIVATE ExecuteStatus ExecuteScas(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPZOrRepNZPrefix(
      ctx, ExecuteScasIteration);
}

// Single CMPS iteration.
static ExecuteStatus ExecuteCmpsIteration(const InstructionContext* ctx) {
  Operand dest = GetStringSourceOperand(ctx);
  Operand src = GetStringDestinationOperand(ctx);
  ExecuteCmp(ctx, &dest, &src.value);
  UpdateStringSourceAddress(ctx);
  UpdateStringDestinationAddress(ctx);
  return kExecuteSuccess;
}

// CMPS
YAX86_PRIVATE ExecuteStatus ExecuteCmps(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPZOrRepNZPrefix(
      ctx, ExecuteCmpsIteration);
}
