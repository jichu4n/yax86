#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "cycles.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// String instructions
// ============================================================================

// Get the repetition prefix of a string instruction, if any.
YAX86_FILE_PRIVATE inline uint8_t GetRepetitionPrefix(
    const InstructionContext* ctx) {
  return ctx->instruction->repetition_prefix;
}

// Get the source operand for string instructions. Typically DS:SI but can be
// overridden by a segment override prefix.
YAX86_FILE_PRIVATE void GetStringSourceOperand(
    const InstructionContext* ctx, Operand* operand) {
  OperandAddress address = {
      .type = kOperandAddressTypeMemory,
      .register_index = kDS,
      .offset = ctx->cpu->registers[kSI],
  };
  ApplySegmentOverride(ctx->instruction, &address.register_index);
  operand->address = address;
  operand->value = ReadOperandValue(ctx, &address);
}

// Get the destination operand address for string instructions. Always ES:DI.
YAX86_FILE_PRIVATE OperandAddress
GetStringDestinationOperandAddress(const InstructionContext* ctx) {
  OperandAddress address = {
      .type = kOperandAddressTypeMemory,
      .register_index = kES,
      .offset = ctx->cpu->registers[kDI],
  };
  return address;
}

// Get the destination operand for string instructions. Always ES:DI.
YAX86_FILE_PRIVATE void GetStringDestinationOperand(
    const InstructionContext* ctx, Operand* operand) {
  OperandAddress address = GetStringDestinationOperandAddress(ctx);
  operand->address = address;
  operand->value = ReadOperandValue(ctx, &address);
}

// Update the source address register (SI) after a string operation.
YAX86_FILE_PRIVATE void UpdateStringSourceAddress(
    const InstructionContext* ctx) {
  if (CPUGetFlag(ctx->cpu, kDF)) {
    ctx->cpu->registers[kSI] -= kNumBytes[ctx->metadata->width];
  } else {
    ctx->cpu->registers[kSI] += kNumBytes[ctx->metadata->width];
  }
}

// Update the destination address register (DI) after a string operation.
YAX86_FILE_PRIVATE void UpdateStringDestinationAddress(
    const InstructionContext* ctx) {
  if (CPUGetFlag(ctx->cpu, kDF)) {
    ctx->cpu->registers[kDI] -= kNumBytes[ctx->metadata->width];
  } else {
    ctx->cpu->registers[kDI] += kNumBytes[ctx->metadata->width];
  }
}

// ============================================================================
// Bulk runs
// ============================================================================

// A repeated MOVS or STOS carried out as a run over guest memory, rather than
// an element at a time through the operand machinery.
//
// An iteration of the element path builds an operand address, resolves a
// segment through it, dispatches on width and charges the bus, twice over, to
// move two bytes - and calls through a function pointer to do it. Where every
// byte the repeat touches lies in the window the CPU indexes directly, the
// whole repeat is a loop over that window instead.
//
// Three things hold for every loop below. Each width gets its own loop rather
// than one loop testing the width, since that test would otherwise be made
// once per element on a part with no branch predictor to hide it. Each visits
// elements in the order the element path visits them, which is what makes a
// repeat whose two ends overlap come out the same. And each walks a pointer
// rather than indexing an array, which is what keeps the compiler from
// rewriting it: written as memory[index], gcc -O3 recognizes the byte-wide
// STOS loop and emits a call to memset(). That measures the same either way,
// so what the index form costs is not speed but a link-time dependency on libc
// the core does not otherwise have.
//
// The copy loops are out of reach of that transform for a second reason: the
// step is a runtime value the compiler cannot prove positive, so it cannot
// conclude the two ranges do not overlap. That one is load-bearing. A memcpy()
// may copy in any order, which is precisely what the overlapping cases depend
// on it not doing, so specializing a loop on a forward direction would hand
// the compiler the proof it needs to introduce one.
typedef struct BulkStringRun {
  // Linear address of the first element at each end. source is left alone for
  // STOS, which reads no memory.
  uint32_t source;
  uint32_t destination;
  // Distance from one element to the next, negative while DF is set.
  int32_t step;
  uint16_t count;
  uint8_t element_size;
} BulkStringRun;

// Whether every byte a run touches is reachable by indexing the direct data
// window, starting from offset within its segment and linear within memory.
//
// Two things can put a byte out of reach, and both have to be ruled out. The
// offset is 16 bits and wraps within its segment, where a linear address does
// not, so a run that wraps carries on somewhere a straight run over memory
// would not reach - the element path recomputes the address from the wrapped
// offset every iteration. And a run that leaves the window has to go through
// the memory map for the part outside it.
YAX86_FILE_PRIVATE bool IsBulkRunAddressable(
    uint32_t window_end, uint16_t offset, uint32_t linear, uint16_t count,
    uint8_t element_size, bool backwards) {
  // Distance from the first element to the last, which is one element short of
  // the whole run.
  const uint32_t reach = (uint32_t)(count - 1) * element_size;
  if (backwards) {
    // Elements run downwards, so the last one is the lowest and the first one
    // reaches highest.
    return (uint32_t)offset + element_size <= kSegmentSize &&
           (uint32_t)offset >= reach && linear >= reach &&
           linear + element_size <= window_end;
  }
  return (uint32_t)offset + reach + element_size <= kSegmentSize &&
         linear + reach + element_size <= window_end;
}

// Works out whether a repeat can run in bulk, and describes it if so.
YAX86_FILE_PRIVATE bool PlanBulkStringRun(
    const InstructionContext* ctx, bool has_source, BulkStringRun* run) {
  const uint8_t prefix = GetRepetitionPrefix(ctx);
  if (prefix != kPrefixREP && prefix != kPrefixREPNZ) {
    return false;
  }
  CPUState* const cpu = ctx->cpu;
  const uint16_t count = cpu->registers[kCX];
  // end is 0 exactly when the window is closed, so this is the whole test for
  // whether there is a window to index.
  const uint32_t window_end = cpu->direct_data_window.end;
  if (count == 0 || window_end == 0) {
    return false;
  }
  const uint8_t element_size = kNumBytes[ctx->metadata->width];
  const bool backwards = CPUGetFlag(cpu, kDF);

  const uint16_t destination_offset = cpu->registers[kDI];
  const uint32_t destination = ToRawAddress(cpu, kES, destination_offset);
  if (!IsBulkRunAddressable(
          window_end, destination_offset, destination, count, element_size,
          backwards)) {
    return false;
  }

  if (has_source) {
    uint8_t source_segment = kDS;
    ApplySegmentOverride(ctx->instruction, &source_segment);
    const uint16_t source_offset = cpu->registers[kSI];
    const uint32_t source =
        ToRawAddress(cpu, source_segment, source_offset);
    if (!IsBulkRunAddressable(
            window_end, source_offset, source, count, element_size,
            backwards)) {
      return false;
    }
    run->source = source;
  }

  run->destination = destination;
  run->step = backwards ? -(int32_t)element_size : (int32_t)element_size;
  run->count = count;
  run->element_size = element_size;
  return true;
}

// Tells the decode cache that the run has written over whatever was decoded
// from the pages it covers.
//
// The element path reports every byte through WriteRawMemoryByte(). A page's
// counter only has to *change* for a decode taken from it to be discarded, not
// to change a particular number of times, so one report per page says the same
// thing - and leaves the counter coming back round after 256 runs rather than
// after 256 bytes, which is a flush avoided rather than a flush missed.
YAX86_FILE_PRIVATE void NotifyBulkStringWrite(
    CPUState* cpu, const BulkStringRun* run) {
  const uint32_t reach = (uint32_t)(run->count - 1) * run->element_size;
  const uint32_t low =
      run->step < 0 ? run->destination - reach : run->destination;
  const uint32_t high = low + reach + run->element_size - 1;
  for (uint32_t address = low & ~(uint32_t)kCodePageOffsetMask; address <= high;
       address += kCodePageSize) {
    CPUNotifyMemoryWrite(cpu, address);
  }
}

// Leaves CX, SI and DI where the element path would have left them, and
// charges what it would have charged.
YAX86_FILE_PRIVATE void FinishBulkStringRun(
    CPUState* cpu, const BulkStringRun* run, uint8_t bus_bytes_per_element,
    bool has_source) {
  const int32_t total = run->step * (int32_t)run->count;
  if (has_source) {
    cpu->registers[kSI] = (uint16_t)((int32_t)cpu->registers[kSI] + total);
  }
  cpu->registers[kDI] = (uint16_t)((int32_t)cpu->registers[kDI] + total);
  cpu->registers[kCX] = 0;
  // The element path charges this through AddBusCycles() once per access, into
  // a counter 16 bits wide that a long repeat runs past the top of. Summing
  // first and adding once wraps exactly where adding one at a time wraps,
  // which is what keeps the two costing the same.
  cpu->pending_cycles += (uint16_t)((uint32_t)run->count *
                                    bus_bytes_per_element * kBusCyclesPerByte);
}

// Execute a string instruction with optional REP prefix.
//
// MOVS, STOS and LODS set no flags, so a repetition prefix has no zero flag to
// test and the 8086/8088 does not tell the two prefixes apart here: 0xF2
// repeats exactly as 0xF3 does, counting CX down to zero. Only the comparison
// string instructions read the prefix as a condition.
YAX86_FILE_PRIVATE InstructionResult ExecuteStringInstructionWithREPPrefix(
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
YAX86_FILE_PRIVATE InstructionResult
ExecuteMovsIteration(const InstructionContext* ctx) {
  Operand src;
  GetStringSourceOperand(ctx, &src);
  OperandAddress dest_address = GetStringDestinationOperandAddress(ctx);
  WriteOperandAddress(ctx, &dest_address, FromOperand(&src));
  UpdateStringSourceAddress(ctx);
  UpdateStringDestinationAddress(ctx);
  return kInstructionExecuted;
}

// A whole repeated MOVS as one run, where the repeat qualifies for one.
// Returns whether it did.
YAX86_FILE_PRIVATE bool ExecuteMovsBulkRun(const InstructionContext* ctx) {
  BulkStringRun run;
  if (!PlanBulkStringRun(ctx, /*has_source=*/true, &run)) {
    return false;
  }
  CPUState* const cpu = ctx->cpu;
  uint8_t* const memory = cpu->direct_data_window.data;
  const uint8_t* source = memory + run.source;
  uint8_t* destination = memory + run.destination;
  if (run.element_size == kNumBytes[kWord]) {
    for (uint16_t remaining = run.count; remaining > 0; --remaining) {
      // Both bytes are read before either is written, which is the order the
      // element path reads and writes a word in - and is what a copy whose
      // ends overlap by exactly one byte comes out of differently otherwise.
      const uint8_t low = source[0];
      const uint8_t high = source[1];
      destination[0] = low;
      destination[1] = high;
      source += run.step;
      destination += run.step;
    }
  } else {
    for (uint16_t remaining = run.count; remaining > 0; --remaining) {
      *destination = *source;
      source += run.step;
      destination += run.step;
    }
  }
  NotifyBulkStringWrite(cpu, &run);
  // Every byte is read and written.
  FinishBulkStringRun(
      cpu, &run, (uint8_t)(run.element_size * 2), /*has_source=*/true);
  return true;
}

// MOVS
YAX86_MODULE_PRIVATE YAX86_HOT InstructionResult
ExecuteMovs(const InstructionContext* ctx) {
  if (ExecuteMovsBulkRun(ctx)) {
    return kInstructionExecuted;
  }
  return ExecuteStringInstructionWithREPPrefix(ctx, ExecuteMovsIteration);
}

// Single STOS iteration.
YAX86_FILE_PRIVATE InstructionResult
ExecuteStosIteration(const InstructionContext* ctx) {
  Operand src;
  ReadRegisterOperandForRegisterIndex(ctx, kAX, &src);
  OperandAddress dest_address = GetStringDestinationOperandAddress(ctx);
  WriteOperandAddress(ctx, &dest_address, FromOperand(&src));
  UpdateStringDestinationAddress(ctx);
  return kInstructionExecuted;
}

// A whole repeated STOS as one run, where the repeat qualifies for one.
// Returns whether it did.
YAX86_FILE_PRIVATE bool ExecuteStosBulkRun(const InstructionContext* ctx) {
  BulkStringRun run;
  if (!PlanBulkStringRun(ctx, /*has_source=*/false, &run)) {
    return false;
  }
  CPUState* const cpu = ctx->cpu;
  uint8_t* const memory = cpu->direct_data_window.data;
  const uint16_t value = cpu->registers[kAX];
  uint8_t* destination = memory + run.destination;
  if (run.element_size == kNumBytes[kWord]) {
    for (uint16_t remaining = run.count; remaining > 0; --remaining) {
      destination[0] = (uint8_t)value;
      destination[1] = (uint8_t)(value >> 8);
      destination += run.step;
    }
  } else {
    // A byte-wide STOS stores AL, which is the low byte of the same register.
    for (uint16_t remaining = run.count; remaining > 0; --remaining) {
      *destination = (uint8_t)value;
      destination += run.step;
    }
  }
  NotifyBulkStringWrite(cpu, &run);
  // Every byte is written, and none is read.
  FinishBulkStringRun(cpu, &run, run.element_size, /*has_source=*/false);
  return true;
}

// STOS
YAX86_MODULE_PRIVATE YAX86_HOT InstructionResult
ExecuteStos(const InstructionContext* ctx) {
  if (ExecuteStosBulkRun(ctx)) {
    return kInstructionExecuted;
  }
  return ExecuteStringInstructionWithREPPrefix(ctx, ExecuteStosIteration);
}

// Single LODS iteration.
YAX86_FILE_PRIVATE InstructionResult
ExecuteLodsIteration(const InstructionContext* ctx) {
  Operand src;
  GetStringSourceOperand(ctx, &src);
  Operand dest;
  ReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);
  WriteOperand(ctx, &dest, FromOperand(&src));
  UpdateStringSourceAddress(ctx);
  return kInstructionExecuted;
}

// LODS
YAX86_MODULE_PRIVATE YAX86_HOT InstructionResult
ExecuteLods(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPPrefix(ctx, ExecuteLodsIteration);
}

// Execute a string instruction with optional REPZ/REPE or REPNZ/REPNE prefix.
YAX86_FILE_PRIVATE InstructionResult
ExecuteStringInstructionWithREPZOrRepNZPrefix(
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
YAX86_FILE_PRIVATE YAX86_HOT InstructionResult
ExecuteScasIteration(const InstructionContext* ctx) {
  Operand src;
  GetStringDestinationOperand(ctx, &src);
  Operand dest;
  ReadRegisterOperandForRegisterIndex(ctx, kAX, &dest);
  ExecuteCmp(ctx, &dest, src.value);
  UpdateStringDestinationAddress(ctx);
  return kInstructionExecuted;
}

// SCAS
YAX86_MODULE_PRIVATE InstructionResult ExecuteScas(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPZOrRepNZPrefix(
      ctx, ExecuteScasIteration);
}

// Single CMPS iteration.
YAX86_FILE_PRIVATE InstructionResult
ExecuteCmpsIteration(const InstructionContext* ctx) {
  Operand dest;
  GetStringSourceOperand(ctx, &dest);
  Operand src;
  GetStringDestinationOperand(ctx, &src);
  ExecuteCmp(ctx, &dest, src.value);
  UpdateStringSourceAddress(ctx);
  UpdateStringDestinationAddress(ctx);
  return kInstructionExecuted;
}

// CMPS
YAX86_MODULE_PRIVATE InstructionResult ExecuteCmps(const InstructionContext* ctx) {
  return ExecuteStringInstructionWithREPZOrRepNZPrefix(
      ctx, ExecuteCmpsIteration);
}
