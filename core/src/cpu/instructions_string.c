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
Y_FILE_PRIVATE inline uint8_t YCPUGetRepetitionPrefix(
    const YCPUInstructionContext* ctx) {
  return ctx->instruction->repetition_prefix;
}

// Get the source operand for string instructions. Typically DS:SI but can be
// overridden by a segment override prefix.
Y_FILE_PRIVATE void YCPUGetStringSourceOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand) {
  YCPUOperandAddress address = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
      .register_index = Y_CPU_DS,
      .offset = ctx->cpu->registers[Y_CPU_SI],
  };
  YCPUApplySegmentOverride(ctx->instruction, &address.register_index);
  operand->address = address;
  operand->value = YCPUReadOperandValue(ctx, &address);
}

// Get the destination operand address for string instructions. Always ES:DI.
Y_FILE_PRIVATE YCPUOperandAddress
YCPUGetStringDestinationOperandAddress(const YCPUInstructionContext* ctx) {
  YCPUOperandAddress address = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
      .register_index = Y_CPU_ES,
      .offset = ctx->cpu->registers[Y_CPU_DI],
  };
  return address;
}

// Get the destination operand for string instructions. Always ES:DI.
Y_FILE_PRIVATE void YCPUGetStringDestinationOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand) {
  YCPUOperandAddress address = YCPUGetStringDestinationOperandAddress(ctx);
  operand->address = address;
  operand->value = YCPUReadOperandValue(ctx, &address);
}

// Update the source address register (SI) after a string operation.
Y_FILE_PRIVATE void YCPUUpdateStringSourceAddress(
    const YCPUInstructionContext* ctx) {
  if (YCPUGetFlag(ctx->cpu, Y_CPU_DF)) {
    ctx->cpu->registers[Y_CPU_SI] -= YCPUNumBytes[ctx->metadata->width];
  } else {
    ctx->cpu->registers[Y_CPU_SI] += YCPUNumBytes[ctx->metadata->width];
  }
}

// Update the destination address register (DI) after a string operation.
Y_FILE_PRIVATE void YCPUUpdateStringDestinationAddress(
    const YCPUInstructionContext* ctx) {
  if (YCPUGetFlag(ctx->cpu, Y_CPU_DF)) {
    ctx->cpu->registers[Y_CPU_DI] -= YCPUNumBytes[ctx->metadata->width];
  } else {
    ctx->cpu->registers[Y_CPU_DI] += YCPUNumBytes[ctx->metadata->width];
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
typedef struct YCPUBulkStringRun {
  // Linear address of the first element at each end. source is left alone for
  // STOS, which reads no memory.
  uint32_t source;
  uint32_t destination;
  // Distance from one element to the next, negative while DF is set.
  int32_t step;
  uint16_t count;
  uint8_t element_size;
} YCPUBulkStringRun;

// Whether every byte a run touches is reachable by indexing the direct data
// window, starting from offset within its segment and linear within memory.
//
// Two things can put a byte out of reach, and both have to be ruled out. The
// offset is 16 bits and wraps within its segment, where a linear address does
// not, so a run that wraps carries on somewhere a straight run over memory
// would not reach - the element path recomputes the address from the wrapped
// offset every iteration. And a run that leaves the window has to go through
// the memory map for the part outside it.
Y_FILE_PRIVATE bool YCPUIsBulkRunAddressable(
    uint32_t window_end, uint16_t offset, uint32_t linear, uint16_t count,
    uint8_t element_size, bool backwards) {
  // Distance from the first element to the last, which is one element short of
  // the whole run.
  const uint32_t reach = (uint32_t)(count - 1) * element_size;
  if (backwards) {
    // Elements run downwards, so the last one is the lowest and the first one
    // reaches highest.
    return (uint32_t)offset + element_size <= Y_CPU_SEGMENT_SIZE &&
           (uint32_t)offset >= reach && linear >= reach &&
           linear + element_size <= window_end;
  }
  return (uint32_t)offset + reach + element_size <= Y_CPU_SEGMENT_SIZE &&
         linear + reach + element_size <= window_end;
}

// Works out whether a repeat can run in bulk, and describes it if so.
Y_FILE_PRIVATE bool YCPUPlanBulkStringRun(
    const YCPUInstructionContext* ctx, bool has_source,
    YCPUBulkStringRun* run) {
  const uint8_t prefix = YCPUGetRepetitionPrefix(ctx);
  if (prefix != Y_CPU_PREFIX_REP && prefix != Y_CPU_PREFIX_REPNZ) {
    return false;
  }
  YCPUState* const cpu = ctx->cpu;
  const uint16_t count = cpu->registers[Y_CPU_CX];
  // end is 0 exactly when the window is closed, so this is the whole test for
  // whether there is a window to index.
  const uint32_t window_end = cpu->direct_data_window.end;
  if (count == 0 || window_end == 0) {
    return false;
  }
  const uint8_t element_size = YCPUNumBytes[ctx->metadata->width];
  const bool backwards = YCPUGetFlag(cpu, Y_CPU_DF);

  const uint16_t destination_offset = cpu->registers[Y_CPU_DI];
  const uint32_t destination =
      YCPUToRawAddress(cpu, Y_CPU_ES, destination_offset);
  if (!YCPUIsBulkRunAddressable(
          window_end, destination_offset, destination, count, element_size,
          backwards)) {
    return false;
  }

  if (has_source) {
    uint8_t source_segment = Y_CPU_DS;
    YCPUApplySegmentOverride(ctx->instruction, &source_segment);
    const uint16_t source_offset = cpu->registers[Y_CPU_SI];
    const uint32_t source =
        YCPUToRawAddress(cpu, source_segment, source_offset);
    if (!YCPUIsBulkRunAddressable(
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
// The element path reports every byte through YCPUWriteRawMemoryByte(). A
// page's counter only has to *change* for a decode taken from it to be
// discarded, not to change a particular number of times, so one report per page
// says the same thing - and leaves the counter coming back round after 256 runs
// rather than after 256 bytes, which is a flush avoided rather than a flush
// missed.
Y_FILE_PRIVATE void YCPUNotifyBulkStringWrite(
    YCPUState* cpu, const YCPUBulkStringRun* run) {
  const uint32_t reach = (uint32_t)(run->count - 1) * run->element_size;
  const uint32_t low =
      run->step < 0 ? run->destination - reach : run->destination;
  const uint32_t high = low + reach + run->element_size - 1;
  for (uint32_t address = low & ~(uint32_t)Y_CPU_CODE_PAGE_OFFSET_MASK;
       address <= high; address += Y_CPU_CODE_PAGE_SIZE) {
    YCPUNotifyMemoryWrite(cpu, address);
  }
}

// Leaves CX, SI and DI where the element path would have left them, and
// charges what it would have charged.
Y_FILE_PRIVATE void YCPUFinishBulkStringRun(
    YCPUState* cpu, const YCPUBulkStringRun* run, uint8_t bus_bytes_per_element,
    bool has_source) {
  const int32_t total = run->step * (int32_t)run->count;
  if (has_source) {
    cpu->registers[Y_CPU_SI] =
        (uint16_t)((int32_t)cpu->registers[Y_CPU_SI] + total);
  }
  cpu->registers[Y_CPU_DI] =
      (uint16_t)((int32_t)cpu->registers[Y_CPU_DI] + total);
  cpu->registers[Y_CPU_CX] = 0;
  // The element path charges this through YCPUAddBusCycles() once per access,
  // into a counter 16 bits wide that a long repeat runs past the top of.
  // Summing first and adding once wraps exactly where adding one at a time
  // wraps, which is what keeps the two costing the same.
  cpu->pending_cycles +=
      (uint16_t)((uint32_t)run->count * bus_bytes_per_element *
                 Y_CPU_BUS_CYCLES_PER_BYTE);
}

// Execute a string instruction with optional REP prefix.
//
// MOVS, STOS and LODS set no flags, so a repetition prefix has no zero flag to
// test and the 8086/8088 does not tell the two prefixes apart here: 0xF2
// repeats exactly as 0xF3 does, counting CX down to zero. Only the comparison
// string instructions read the prefix as a condition.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteStringInstructionWithREPPrefix(
    const YCPUInstructionContext* ctx,
    YCPUInstructionResult (*fn)(const YCPUInstructionContext*)) {
  uint8_t prefix = YCPUGetRepetitionPrefix(ctx);
  if (prefix != Y_CPU_PREFIX_REP && prefix != Y_CPU_PREFIX_REPNZ) {
    return fn(ctx);
  }
  while (ctx->cpu->registers[Y_CPU_CX]) {
    YCPUInstructionResult status = fn(ctx);
    if (status != Y_CPU_INSTRUCTION_EXECUTED) {
      return status;
    }
    --ctx->cpu->registers[Y_CPU_CX];
  }
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Single MOVS iteration.
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteMovsIteration(const YCPUInstructionContext* ctx) {
  YCPUOperand src;
  YCPUGetStringSourceOperand(ctx, &src);
  YCPUOperandAddress dest_address = YCPUGetStringDestinationOperandAddress(ctx);
  YCPUWriteOperandAddress(ctx, &dest_address, YCPUFromOperand(&src));
  YCPUUpdateStringSourceAddress(ctx);
  YCPUUpdateStringDestinationAddress(ctx);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// A whole repeated MOVS as one run, where the repeat qualifies for one.
// Returns whether it did.
Y_FILE_PRIVATE bool YCPUExecuteMovsBulkRun(const YCPUInstructionContext* ctx) {
  YCPUBulkStringRun run;
  if (!YCPUPlanBulkStringRun(ctx, /*has_source=*/true, &run)) {
    return false;
  }
  YCPUState* const cpu = ctx->cpu;
  uint8_t* const memory = cpu->direct_data_window.data;
  const uint8_t* source = memory + run.source;
  uint8_t* destination = memory + run.destination;
  if (run.element_size == YCPUNumBytes[Y_CPU_WORD]) {
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
  YCPUNotifyBulkStringWrite(cpu, &run);
  // Every byte is read and written.
  YCPUFinishBulkStringRun(
      cpu, &run, (uint8_t)(run.element_size * 2), /*has_source=*/true);
  return true;
}

// MOVS
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteMovs(const YCPUInstructionContext* ctx) {
  if (YCPUExecuteMovsBulkRun(ctx)) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  return YCPUExecuteStringInstructionWithREPPrefix(
      ctx, YCPUExecuteMovsIteration);
}

// Single STOS iteration.
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteStosIteration(const YCPUInstructionContext* ctx) {
  YCPUOperand src;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &src);
  YCPUOperandAddress dest_address = YCPUGetStringDestinationOperandAddress(ctx);
  YCPUWriteOperandAddress(ctx, &dest_address, YCPUFromOperand(&src));
  YCPUUpdateStringDestinationAddress(ctx);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// A whole repeated STOS as one run, where the repeat qualifies for one.
// Returns whether it did.
Y_FILE_PRIVATE bool YCPUExecuteStosBulkRun(const YCPUInstructionContext* ctx) {
  YCPUBulkStringRun run;
  if (!YCPUPlanBulkStringRun(ctx, /*has_source=*/false, &run)) {
    return false;
  }
  YCPUState* const cpu = ctx->cpu;
  uint8_t* const memory = cpu->direct_data_window.data;
  const uint16_t value = cpu->registers[Y_CPU_AX];
  uint8_t* destination = memory + run.destination;
  if (run.element_size == YCPUNumBytes[Y_CPU_WORD]) {
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
  YCPUNotifyBulkStringWrite(cpu, &run);
  // Every byte is written, and none is read.
  YCPUFinishBulkStringRun(cpu, &run, run.element_size, /*has_source=*/false);
  return true;
}

// STOS
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteStos(const YCPUInstructionContext* ctx) {
  if (YCPUExecuteStosBulkRun(ctx)) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  return YCPUExecuteStringInstructionWithREPPrefix(
      ctx, YCPUExecuteStosIteration);
}

// Single LODS iteration.
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteLodsIteration(const YCPUInstructionContext* ctx) {
  YCPUOperand src;
  YCPUGetStringSourceOperand(ctx, &src);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperand(&src));
  YCPUUpdateStringSourceAddress(ctx);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// LODS
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteLods(const YCPUInstructionContext* ctx) {
  return YCPUExecuteStringInstructionWithREPPrefix(
      ctx, YCPUExecuteLodsIteration);
}

// Execute a string instruction with optional REPZ/REPE or REPNZ/REPNE prefix.
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteStringInstructionWithREPZOrRepNZPrefix(
    const YCPUInstructionContext* ctx,
    YCPUInstructionResult (*fn)(const YCPUInstructionContext*)) {
  uint8_t prefix = YCPUGetRepetitionPrefix(ctx);
  if (prefix != Y_CPU_PREFIX_REP && prefix != Y_CPU_PREFIX_REPNZ) {
    return fn(ctx);
  }
  bool terminate_zf_value = prefix == Y_CPU_PREFIX_REPNZ;
  while (ctx->cpu->registers[Y_CPU_CX]) {
    YCPUInstructionResult status = fn(ctx);
    if (status != Y_CPU_INSTRUCTION_EXECUTED) {
      return status;
    }
    --ctx->cpu->registers[Y_CPU_CX];
    if (YCPUGetFlag(ctx->cpu, Y_CPU_ZF) == terminate_zf_value) {
      break;
    }
  }
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Single SCAS iteration.
Y_FILE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteScasIteration(const YCPUInstructionContext* ctx) {
  YCPUOperand src;
  YCPUGetStringDestinationOperand(ctx, &src);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUExecuteCmp(ctx, &dest, src.value);
  YCPUUpdateStringDestinationAddress(ctx);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// SCAS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteScas(const YCPUInstructionContext* ctx) {
  return YCPUExecuteStringInstructionWithREPZOrRepNZPrefix(
      ctx, YCPUExecuteScasIteration);
}

// Single CMPS iteration.
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteCmpsIteration(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUGetStringSourceOperand(ctx, &dest);
  YCPUOperand src;
  YCPUGetStringDestinationOperand(ctx, &src);
  YCPUExecuteCmp(ctx, &dest, src.value);
  YCPUUpdateStringSourceAddress(ctx);
  YCPUUpdateStringDestinationAddress(ctx);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// CMPS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCmps(const YCPUInstructionContext* ctx) {
  return YCPUExecuteStringInstructionWithREPZOrRepNZPrefix(
      ctx, YCPUExecuteCmpsIteration);
}
