#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "cycles.h"
#include "instructions.h"
#include "operands.h"
#include "public.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

#define YAX86_CPU_LOG(level, ...) \
  YAX86_LOG(cpu->config.logger, &kLogModuleCPU, level, __VA_ARGS__)

// ============================================================================
// CPU state
// ============================================================================

void CPUInit(CPUState* cpu) {
  cpu->flags = kInitialFlags;

  // The only place the count is read, so a host gets told here or not at all.
  const uint32_t num_entries = cpu->config.decode_cache_num_entries;
  if (cpu->config.decode_cache == NULL) {
    return;
  }
  if (num_entries == 0 || (num_entries & (num_entries - 1)) != 0) {
    YAX86_CPU_LOG(
        kLogLevelError,
        "decode cache of %u entries is not a power of two, running without one",
        (unsigned)num_entries);
    // The CPU's own copy, so a rejected cache is simply not there.
    cpu->config.decode_cache = NULL;
    return;
  }
  cpu->decode_cache_index_mask = num_entries - 1;
}

// ============================================================================
// Instruction decoding
// ============================================================================

// The two prefix groups are each a contiguous encoding family, so a masked
// compare identifies a whole group and the bits the mask leaves free say which
// member it is.
enum {
  // Segment overrides encode as 001ss110, where ss selects the segment.
  kSegmentOverridePrefixMask = 0xE7,
  kSegmentOverridePrefixValue = 0x26,
  // Position of the ss field within a segment override prefix.
  kSegmentOverridePrefixShift = 3,
  kSegmentOverridePrefixSegmentMask = 0x03,

  // Within the LOCK and repetition prefix group, bit 1 separates the
  // repetition prefixes (REPNZ, REP) from LOCK and its undocumented 0xF1
  // alias.
  kRepetitionPrefixBit = 0x02,
};

// Whether a byte is a segment override prefix. The mask pins every bit but the
// two that select the segment, so it matches those four bytes and nothing else.
static inline bool IsSegmentOverridePrefix(uint8_t byte) {
  return (byte & kSegmentOverridePrefixMask) == kSegmentOverridePrefixValue;
}

// Whether a byte is a LOCK or repetition prefix. These four are consecutive,
// so this is a range check rather than a mask - which is both clearer and one
// instruction cheaper, since a compiler folds it into a single subtract and
// compare.
static inline bool IsLockOrRepetitionPrefix(uint8_t byte) {
  return byte >= kPrefixLOCK && byte <= kPrefixREP;
}

// Record what a prefix byte selects, and report whether it was a prefix at
// all. Deciding which group a byte belongs to is the same test as deciding
// whether it is a prefix, so both happen here rather than the caller asking
// first and this asking again.
//
// The cheaper test goes first. A byte that is not a prefix runs both, and that
// is the common case by far - every instruction ends the loop with one.
static bool ApplyPrefixByte(Instruction* instruction, uint8_t byte) {
  if (IsLockOrRepetitionPrefix(byte)) {
    // LOCK and its 0xF1 alias advance IP, but nothing acts on them, so only a
    // repetition prefix is worth recording.
    if (byte & kRepetitionPrefixBit) {
      instruction->repetition_prefix = byte;
    }
    return true;
  }
  if (IsSegmentOverridePrefix(byte)) {
    // The segment field is in the 8086's sreg encoding order, which is the
    // order kES through kDS are numbered in, so the register index is an
    // offset from kES.
    instruction->segment_override =
        (uint8_t)(kES + ((byte >> kSegmentOverridePrefixShift) &
                         kSegmentOverridePrefixSegmentMask));
    return true;
  }
  return false;
}

enum {
  // How many bytes a segment addresses. IP is 16 bits and wraps within the
  // segment, so this is also where a fetch through a window has to stop.
  kSegmentSize = 0x10000,
};

// Where one call to CPUFetchNextInstruction() has got to.
//
// Scratch for a single decode, and nothing more. What survives between
// instructions is the window itself, in CPUState.instruction_fetch_window;
// this is the cursor walking it, and it is thrown away when the instruction
// has been decoded.
//
// It is a struct because CPUFetchNextInstructionByte() has to advance the
// cursor and is called from five places in the decode, so the cursor has to be
// passed by pointer. Kept as three loose locals it would have to be passed as
// three out-parameters, and taking the address of each is what stops a
// compiler keeping them in registers - which is the whole point of the
// arrangement, since this is the hottest loop in the emulator.
//
// The two byte positions are the same position expressed twice, deliberately:
//
//   - next_byte_offset is where the cursor is in the segment, and is what
//     yields the instruction's size and what the byte-at-a-time path needs. It
//     is not the CPU's IP register, which has to keep naming the instruction
//     being decoded until CPUTick() advances it by instruction.size - so a
//     failed decode leaves IP alone, and the control flow instructions can add
//     their displacement to the address of the instruction after this one.
//   - next_byte is where the cursor is in the host's memory, and is cached
//     rather than derived because deriving it costs a shift, an add, a
//     subtract and an add per byte where advancing it costs an increment.
//
// bytes_remaining bounds the direct reads. Where the host hands out no window
// it is zero, and every byte takes the ordinary path through read_memory_byte
// - an indirect call per byte, two to six times per instruction, which is what
// the window exists to avoid.
typedef struct CPUInstructionFetchState {
  // The offset within CS of the next byte to read.
  uint16_t next_byte_offset;
  // The next byte to read, and how many may be read directly from it. NULL and
  // zero where there is no window.
  const uint8_t* next_byte;
  uint32_t bytes_remaining;
} CPUInstructionFetchState;

// Reads the next instruction byte, from the window where one is open and from
// the memory map where one is not.
//
// Either way the read goes straight to memory rather than through
// ReadMemoryOperandByte, so that the fetch is not charged for time on the data
// bus. The 8088 fetches ahead into a queue while the previous instruction
// executes, so most of that time is already paid for by the instruction being
// executed - and the published per-instruction figures the cycle table is
// built from assume the queue is full.
YAX86_HOT static inline uint8_t CPUFetchNextInstructionByte(
    CPUState* cpu, CPUInstructionFetchState* fetch_state) {
  if (fetch_state->bytes_remaining > 0) {
    --fetch_state->bytes_remaining;
    ++fetch_state->next_byte_offset;
    return *fetch_state->next_byte++;
  }
  const MemoryAddress address = {
      .segment_register_index = kCS,
      .offset = fetch_state->next_byte_offset++,
  };
  return ReadRawMemoryByte(cpu, ToRawAddress(cpu, &address));
}

// Points a fetch at whatever can be read directly from CS:ip.
YAX86_HOT static void CPUInitInstructionFetchState(
    CPUState* cpu, uint16_t ip, CPUInstructionFetchState* fetch_state) {
  fetch_state->next_byte_offset = ip;
  fetch_state->next_byte = NULL;
  fetch_state->bytes_remaining = 0;

  // Note that there is deliberately no early out for a host that supplies no
  // get_instruction_fetch_window, even though everything below is wasted on
  // one. Such a host reads every byte through an indirect call already, so it
  // would save a handful of arithmetic against several calls - and the test to
  // skip it would be paid by the hosts that do supply one, which is every host
  // that cares about the speed. Measured on x86-64, adding it grew
  // CPUFetchNextInstruction by 11 bytes.
  const MemoryAddress fetch_address = {
      .segment_register_index = kCS,
      .offset = ip,
  };
  const uint32_t raw_address = ToRawAddress(cpu, &fetch_address);

  const CPUInstructionFetchWindow* const window =
      &cpu->instruction_fetch_window;
  if (window->data == NULL || raw_address < window->start ||
      raw_address >= window->end) {
    // Nothing open covers this address, so ask for a window that does. The
    // host fills one in, or sets its data to NULL to decline.
    if (cpu->config.get_instruction_fetch_window != NULL) {
      cpu->config.get_instruction_fetch_window(cpu, raw_address);
    } else {
      cpu->instruction_fetch_window.data = NULL;
    }
  }
  // Usually the window was already open and already covered the address, which
  // costs a compare rather than a call: a window spans a whole memory region,
  // so both straight-line execution and a jump backwards within that region
  // land inside the one already open.
  if (window->data != NULL && raw_address >= window->start &&
      raw_address < window->end) {
    fetch_state->next_byte = window->data + (raw_address - window->start);
    fetch_state->bytes_remaining = window->end - raw_address;
  }

  // IP is 16 bits and wraps within the segment where the linear address does
  // not, so the fetch has to stop where the wrap would be. Past that point the
  // ordinary path recomputes the address from the wrapped IP and gets it
  // right.
  const uint32_t bytes_until_wrap = (uint32_t)kSegmentSize - (uint32_t)ip;
  if (fetch_state->bytes_remaining > bytes_until_wrap) {
    fetch_state->bytes_remaining = bytes_until_wrap;
  }
}

// Returns the number of displacement bytes based on the ModR/M byte.
static uint8_t GetDisplacementSize(uint8_t mod, uint8_t rm) {
  switch (mod) {
    case 0:
      // Special case: 16-bit displacement
      return rm == 6 ? 2 : 0;
    case 1:
    case 2:
      // 8 or 16-bit displacement
      return mod;
    default:
      // No displacement
      return 0;
  }
}

// Returns the number of immediate bytes in an instruction.
static uint8_t GetImmediateSize(const OpcodeMetadata* metadata, uint8_t reg) {
  switch (metadata->opcode) {
    // TEST r/m8, imm8
    case 0xF6:
    // TEST r/m16, imm16
    case 0xF7:
      // REG 0 and REG 1 are both TEST, which carries an immediate; the other
      // REG values do not. The 8086/8088 does not decode bit 0 of the REG
      // field here, which is what makes REG 1 an alias of REG 0.
      return reg <= 1 ? metadata->opcode - 0xF5 : 0;
    default:
      return metadata->immediate_size;
  }
}

YAX86_HOT CPUFetchNextInstructionStatus
CPUFetchNextInstruction(CPUState* cpu, Instruction* instruction) {
  // The prefix fields, which ApplyPrefixByte() writes only where a prefix is
  // actually present. Every other field a decode could leave behind is settled
  // where it becomes known rather than here: opcode, size and immediate_size
  // are assigned on every path that returns success, has_mod_rm and
  // displacement_size in both arms of the ModR/M branch below, mod_rm is read
  // only where has_mod_rm is set, and the displacement and immediate arrays
  // are read no further than their size fields say.
  instruction->segment_override = kNoSegmentOverride;
  instruction->repetition_prefix = 0;

  uint8_t current_byte;
  const uint16_t original_ip = cpu->registers[kIP];
  CPUInstructionFetchState fetch_state;
  CPUInitInstructionFetchState(cpu, original_ip, &fetch_state);

  // Prefix
  //
  // The count is local because nothing outside this loop wants it: what the
  // prefixes selected is in the instruction, and the bytes they occupied are
  // in its size. It exists only to stop a run of prefix bytes from fetching
  // forever - see kMaxPrefixBytes.
  uint8_t prefix_size = 0;
  current_byte = CPUFetchNextInstructionByte(cpu, &fetch_state);
  while (ApplyPrefixByte(instruction, current_byte)) {
    if (++prefix_size > kMaxPrefixBytes) {
      return kFetchPrefixTooLong;
    }
    current_byte = CPUFetchNextInstructionByte(cpu, &fetch_state);
  }

  // Opcode
  instruction->opcode = current_byte;
  const OpcodeMetadata* metadata = &opcode_table[instruction->opcode];

  // ModR/M
  //
  // The REG field is kept in a local, because the immediate size below is
  // computed whether or not the instruction carries a ModR/M byte.
  uint8_t reg = 0;
  if (metadata->has_modrm) {
    uint8_t mod_rm_byte = CPUFetchNextInstructionByte(cpu, &fetch_state);
    reg = (mod_rm_byte >> 3) & 0x07;  // Bits 3-5
    instruction->has_mod_rm = true;
    instruction->mod_rm.mod = (mod_rm_byte >> 6) & 0x03;  // Bits 6-7
    instruction->mod_rm.reg = reg;
    instruction->mod_rm.rm = mod_rm_byte & 0x07;  // Bits 0-2

    // Displacement
    const uint8_t displacement_size =
        GetDisplacementSize(instruction->mod_rm.mod, instruction->mod_rm.rm);
    instruction->displacement_size = displacement_size;
    for (uint8_t i = 0; i < displacement_size; ++i) {
      instruction->displacement[i] =
          CPUFetchNextInstructionByte(cpu, &fetch_state);
    }
  } else {
    // Cleared in the arm that skips them rather than before the decode starts,
    // so that a decode writes each of them exactly once whichever arm it
    // takes. Clearing them up front costs the ModR/M arm a second write of
    // both.
    instruction->has_mod_rm = false;
    instruction->displacement_size = 0;
  }

  // Immediate operand
  //
  // immediate_size is a three bit field, so it can express more bytes than
  // immediate[] holds. No entry in the opcode table does - the widest is the 4
  // of a far pointer - but nothing in the type says so, and a compiler that
  // cannot see it is right to warn about the writes below. Bounding it by the
  // array is what makes the invariant explicit.
  uint8_t immediate_size = GetImmediateSize(metadata, reg);
  if (immediate_size > kMaxImmediateBytes) {
    immediate_size = kMaxImmediateBytes;
  }
  instruction->immediate_size = immediate_size;
  for (uint8_t i = 0; i < immediate_size; ++i) {
    instruction->immediate[i] = CPUFetchNextInstructionByte(cpu, &fetch_state);
  }

  instruction->size = (uint8_t)(fetch_state.next_byte_offset - original_ip);

  return kFetchSuccess;
}

void CPUInvalidateDecodeCache(CPUState* cpu) {
  CPUDecodeCacheEntry* const cache = cpu->config.decode_cache;
  if (cache == NULL) {
    return;
  }
  for (uint32_t i = 0; i <= cpu->decode_cache_index_mask; ++i) {
    cache[i].valid = false;
  }
}

// Whether an entry holds a decode of the instruction at address that is still
// current. The generation is passed in because both callers have it already.
YAX86_ALWAYS_INLINE static bool IsDecodeCacheHit(
    const CPUDecodeCacheEntry* entry, uint32_t address, uint8_t generation) {
  return entry->valid && entry->address == address &&
         entry->generation == generation;
}

// What an instruction costs before it runs. Both terms read only the encoding,
// so the answer belongs to the decode and an entry can keep it.
YAX86_ALWAYS_INLINE static uint16_t GetInstructionBaseCycles(
    const Instruction* instruction) {
  return (uint16_t)kOpcodeBaseCycles[instruction->opcode] +
         GetEffectiveAddressCycles(instruction);
}

// Fetches the next instruction, from the decode cache where it is there.
//
// What comes back through entry is the entry holding the instruction to run:
// the cache entry on a hit, scratch where there is no cache, and on a miss the
// entry the decode went straight into. Nothing is ever copied.
//
// A caller must not hold the pointer across another fetch.
YAX86_HOT static CPUFetchNextInstructionStatus CPUFetchNextInstructionCached(
    CPUState* cpu, CPUDecodeCacheEntry* scratch, CPUDecodeCacheEntry** entry) {
  const uint16_t ip = cpu->registers[kIP];
  // NULL covers both having no cache and having asked for an unusable one,
  // since CPUInit() clears its own copy of a count that did not pass.
  CPUDecodeCacheEntry* const cache = cpu->config.decode_cache;

  // Where the decode is going to land, and the key it would be kept under. The
  // two paths below join up at the decode rather than each making their own
  // call, because a second call site is enough for GCC to stop inlining
  // GetEffectiveAddressCycles() into either.
  CPUDecodeCacheEntry* target = scratch;
  uint32_t address = 0;
  uint8_t generation = 0;

  if (cache != NULL) {
    const MemoryAddress start = {
        .segment_register_index = kCS,
        .offset = ip,
    };
    address = ToRawAddress(cpu, &start);
    generation = cpu->code_page_generation[address >> kCodePageShift];
    target = &cache[address & cpu->decode_cache_index_mask];
    if (IsDecodeCacheHit(target, address, generation)) {
      *entry = target;
      return kFetchSuccess;
    }
    // A decode that fails partway leaves the entry holding whatever it got to,
    // so the entry disowns its contents before the decode rather than after.
    target->valid = false;
  }

  *entry = target;
  const CPUFetchNextInstructionStatus status =
      CPUFetchNextInstruction(cpu, &target->instruction);
  if (status != kFetchSuccess) {
    return status;
  }
  // Written whether or not the decode is kept, since the caller charges it
  // either way.
  target->base_cycles = GetInstructionBaseCycles(&target->instruction);

  // Kept only when the whole instruction came from the page the generation was
  // read for, and did not run off the end of the segment. Past either boundary
  // the bytes are somewhere the key says nothing about, and both are too rare
  // to be worth a second check on every hit. Staying inside the page also
  // rules out wrapping the top of the address space.
  const uint32_t size = target->instruction.size;
  if (cache != NULL &&
      (address & kCodePageOffsetMask) + size <= kCodePageSize &&
      (uint32_t)ip + size <= kSegmentSize) {
    target->address = address;
    target->generation = generation;
    target->valid = true;
  }
  return kFetchSuccess;
}

// ============================================================================
// Execution
// ============================================================================

// Runs an instruction whose opcode table entry the caller already has, and
// which the caller has already established the entry agrees with.
//
// Kept out of line. Inlined into CPUTick() this measured 31% slower on a
// Cortex-M0+: the execute path wants registers, the core has few, and folding
// the two together makes both spill. It only shows up once the hot path is in
// SRAM - from flash the XIP cache dominates and hides it.
YAX86_HOT YAX86_NOINLINE YAX86_PRIVATE InstructionResult
CPUExecuteDecodedInstruction(
    CPUState* cpu, Instruction* instruction, const OpcodeMetadata* metadata) {
  // Run the instruction handler.
  InstructionContext context = {
      .cpu = cpu,
      .instruction = instruction,
      .metadata = metadata,
  };
  return metadata->handler(&context);
}

// Checks an instruction against the opcode table before running it.
//
// For a caller that built the Instruction itself rather than decoding one -
// CPUTick() goes straight to CPUExecuteDecodedInstruction(), because its own
// decode is what produced the encoding these checks would be re-examining.
YAX86_HOT InstructionResult
CPUExecuteInstruction(CPUState* cpu, Instruction* instruction) {
  const OpcodeMetadata* metadata = &opcode_table[instruction->opcode];

  // Check the encoded instruction against the expected format for its opcode.
  // Nothing checks that the opcode has a handler, because every entry in the
  // table has one: the eight bytes that are prefixes rather than instructions
  // are handled by ExecuteInvalidOpcode(), which returns what a missing handler
  // used to.
  if (instruction->has_mod_rm != metadata->has_modrm) {
    return kInstructionInvalid;
  }
  if (instruction->immediate_size !=
      (metadata->has_modrm ? GetImmediateSize(metadata, instruction->mod_rm.reg)
                           : metadata->immediate_size)) {
    return kInstructionInvalid;
  }

  return CPUExecuteDecodedInstruction(cpu, instruction, metadata);
}

// Save state and vector to the handler for an interrupt.
static void DispatchInterrupt(CPUState* cpu, uint8_t interrupt_number) {
  // Prepare for interrupt processing.
  cpu->is_halted = false;
  PushValue(cpu, cpu->flags);
  CPUSetFlag(cpu, kIF, false);
  CPUSetFlag(cpu, kTF, false);
  PushValue(cpu, cpu->registers[kCS]);
  PushValue(cpu, cpu->registers[kIP]);

  // Invoke the interrupt handler callback first. If the caller did not provide
  // an interrupt handler callback, handle the interrupt within the VM using the
  // Interrupt Vector Table.
  InterruptHandlerResult interrupt_handler_result =
      cpu->config.handle_interrupt
          ? cpu->config.handle_interrupt(cpu, interrupt_number)
          : kInterruptHandlerUnhandled;

  if (interrupt_handler_result == kInterruptHandlerHandled) {
    // If the interrupt was handled by the caller-provided interrupt handler
    // callback, restore state and continue execution.
    ExecuteReturnFromInterrupt(cpu);
    return;
  }

  // If the interrupt was not handled by the caller-provided interrupt handler
  // callback, handle it within the VM using the Interrupt Vector Table.
  uint16_t ivt_entry_offset = interrupt_number << 2;
  cpu->registers[kIP] = ReadRawMemoryWord(cpu, ivt_entry_offset);
  cpu->registers[kCS] = ReadRawMemoryWord(cpu, ivt_entry_offset + 2);
}

// Take a pending interrupt, if any. Returns whether one was dispatched.
static bool ExecutePendingInterrupt(CPUState* cpu) {
  // An internal interrupt goes first. It was raised by the instruction that
  // just executed, and taking it clears IF, which correctly holds off any
  // external request until the handler re-enables interrupts.
  if (cpu->has_pending_internal_interrupt) {
    const uint8_t interrupt_number = cpu->pending_internal_interrupt_number;
    CPUClearInternalInterrupt(cpu);
    DispatchInterrupt(cpu, interrupt_number);
    return true;
  }

  // An external request on the INTR pin is only taken while interrupts are
  // enabled. Acknowledging it is what produces its vector - there is nothing to
  // latch beforehand, and the controller keeps requesting until acknowledged.
  //
  // The hint is read first where the host supplies one. It is a load against an
  // indirect call into a controller that almost always reports nothing, and
  // this runs at every instruction boundary. A host that supplies none is
  // asked every time.
  const bool* const request_hint = cpu->config.interrupt_request_hint;
  uint8_t intr_vector;
  if (CPUGetFlag(cpu, kIF) && (request_hint == NULL || *request_hint) &&
      cpu->config.acknowledge_interrupt &&
      cpu->config.acknowledge_interrupt(cpu, &intr_vector)) {
    DispatchInterrupt(cpu, intr_vector);
    return true;
  }

  return false;
}

// ============================================================================
// Runs of instructions
// ============================================================================
//
// A tick runs several instructions back to back where it can. What that saves
// is the round trip: PlatformTick()'s deadline test and stop checks, and
// CPUTick()'s own prologue and epilogue, all of which are per tick rather than
// per instruction.
//
// Two things bound a run: CPUTick()'s max_run_cycles, so that a device
// deadline is not passed over, and kMaxInstructionsPerTick.

enum {
  // IN and OUT are two families of four - 0xE4-0xE7 takes the port number as
  // an immediate, 0xEC-0xEF takes it in DX - differing only in bit 3. Leaving
  // that bit free along with the two that select direction and width matches
  // both families and nothing else.
  kPortInstructionMask = 0xF4,
  kPortInstructionValue = 0xE4,
};

// Whether an opcode reads or writes an I/O port.
YAX86_ALWAYS_INLINE static bool IsPortInstruction(uint8_t opcode) {
  return (opcode & kPortInstructionMask) == kPortInstructionValue;
}

// Whether a run may carry on into the instruction after the one just executed.
//
// Everything here is something the end of a tick would otherwise have dealt
// with, and which running another instruction first would deal with too late.
YAX86_ALWAYS_INLINE static bool CPUCanContinueRun(
    const CPUState* cpu, uint8_t opcode, uint16_t max_run_cycles) {
  return
      // A budget of zero is how a host asks for one instruction per tick.
      cpu->pending_cycles < max_run_cycles &&
      // INT n, INTO and a divide error dispatch at the end of the tick.
      !cpu->has_pending_internal_interrupt &&
      // The test that skips execution while halted is made before the run
      // starts.
      !cpu->is_halted &&
      // IN and OUT can reprogram a device and move the deadline the budget was
      // computed from. The other half of this rule is at the call site.
      !IsPortInstruction(opcode) &&
      // An instruction that sets TF - POPF, IRET - must be the last of its
      // run, or the next one runs without trapping.
      !CPUGetFlag(cpu, kTF) &&
      // STI, POPF and IRET can enable interrupts with a request already
      // waiting. Without a hint that cannot be told without an acknowledge
      // cycle, so a host supplying none ends the run here.
      cpu->config.interrupt_request_hint != NULL &&
      !*cpu->config.interrupt_request_hint;
}

// The entry holding the decoded instruction at CS:IP if the cache holds a
// current one, and NULL otherwise.
//
// A run carries on only into a cached instruction, which keeps a step cheap
// and keeps a cold CPU to one instruction per tick however large its budget.
YAX86_HOT static CPUDecodeCacheEntry* CPUCachedEntryAtIP(CPUState* cpu) {
  CPUDecodeCacheEntry* const cache = cpu->config.decode_cache;
  if (cache == NULL) {
    return NULL;
  }
  const MemoryAddress start = {
      .segment_register_index = kCS,
      .offset = cpu->registers[kIP],
  };
  const uint32_t address = ToRawAddress(cpu, &start);
  CPUDecodeCacheEntry* const entry =
      &cache[address & cpu->decode_cache_index_mask];
  if (!IsDecodeCacheHit(
          entry, address,
          cpu->code_page_generation[address >> kCodePageShift])) {
    return NULL;
  }
  return entry;
}

YAX86_HOT CPUTickResult CPUTick(CPUState* cpu, uint16_t max_run_cycles) {
  // Whether this tick ran an instruction. A halted CPU runs none until an
  // interrupt wakes it.
  bool executed_instruction = false;

  // A halted CPU still consumes time - it is sitting in a wait state, not
  // stopped - so a tick that runs no instruction still has to advance the
  // clock, or the timer that is meant to wake it would never tick either.
  cpu->pending_cycles = 0;
  cpu->cycles_this_tick = kHaltedCycles;

  // The trap flag is sampled before the instruction runs, not after. An
  // instruction that sets TF - POPF or IRET - must not trap on itself, and one
  // that clears TF still traps once for the instruction it was set during.
  const bool trap_flag_was_set = CPUGetFlag(cpu, kTF);

  // Execute next CPU instruction if not halted.
  if (!cpu->is_halted) {
    // Step 1: Fetch the next instruction.
    //
    // The local entry is only where a decode lands when there is no cache to
    // decode into. What runs is whatever the fetch points at.
    CPUDecodeCacheEntry scratch;
    CPUDecodeCacheEntry* entry;
    CPUFetchNextInstructionStatus fetch_status =
        CPUFetchNextInstructionCached(cpu, &scratch, &entry);
    if (fetch_status != kFetchSuccess) {
      YAX86_CPU_LOG(
          kLogLevelError, "%04X:%04X failed to fetch instruction, status %d",
          cpu->registers[kCS], cpu->registers[kIP], (int)fetch_status);
      return kCPUTickInvalid;
    }

    // A successful fetch is always followed by an instruction: the only way
    // out of the loop below without running one is a return.
    executed_instruction = true;

    // A tick that has to trap runs exactly one instruction: TF was set when it
    // started, so the single-step interrupt belongs to the instruction about
    // to run, and one that clears TF still owes that trap.
    uint8_t remaining =
        trap_flag_was_set ? 1 : (uint8_t)kMaxInstructionsPerTick;

    for (;;) {
      Instruction* const instruction = &entry->instruction;
      const uint16_t instruction_cs = cpu->registers[kCS];
      const uint16_t instruction_ip = cpu->registers[kIP];
      cpu->registers[kIP] += instruction->size;

      // What the instruction costs before it runs came with the decode; its
      // bus traffic and anything that depends on its operands it charges
      // itself as it runs. A run accumulates all of it, which is what the
      // budget below is spent against.
      CPUAddCycles(cpu, entry->base_cycles);

      // Step 2: Execute the instruction. The fetch above derived has_mod_rm
      // and immediate_size from this same table entry, so the checks
      // CPUExecuteInstruction() makes cannot fail here.
      const OpcodeMetadata* const metadata = &opcode_table[instruction->opcode];
      if (CPUExecuteDecodedInstruction(cpu, instruction, metadata) !=
          kInstructionExecuted) {
        YAX86_CPU_LOG(
            kLogLevelError, "%04X:%04X invalid instruction, opcode %02X",
            instruction_cs, instruction_ip, instruction->opcode);
        return kCPUTickInvalid;
      }
      ++cpu->instructions_retired;

      // Step 3: Carry on into the next instruction where nothing needs the
      // host's attention and the cache already holds it.
      if (--remaining == 0 ||
          !CPUCanContinueRun(cpu, instruction->opcode, max_run_cycles)) {
        break;
      }
      entry = CPUCachedEntryAtIP(cpu);
      // A port handler calls PlatformSync(), making it the one instruction
      // that looks at the clock - and part way through a run the clock stops
      // short of the instructions already run in it, since a tick's cost is
      // reported once it is over. Ending before one leaves every port access
      // the first instruction of its tick.
      if (entry == NULL || IsPortInstruction(entry->instruction.opcode)) {
        break;
      }
    }
    cpu->cycles_this_tick = cpu->pending_cycles;
  }

  // Step 4: Handle a pending interrupt. This runs even while halted, because
  // an interrupt is the only thing that can clear the halted state -
  // ExecutePendingInterrupt() resets is_halted when it dispatches one.
  const bool dispatched_interrupt = ExecutePendingInterrupt(cpu);

  // Step 5: The trap flag raises a single-step interrupt after an instruction
  // executes, so a halted CPU must not trap - otherwise the trap would wake it
  // and then fire again on every subsequent tick.
  //
  // Single-stepping is the lowest priority of the interrupt sources recognized
  // at an instruction boundary, so an interrupt dispatched above takes its
  // place rather than both firing.
  if (executed_instruction && trap_flag_was_set && !dispatched_interrupt) {
    CPURaiseInternalInterrupt(cpu, kInterruptSingleStep);
    ExecutePendingInterrupt(cpu);
  }

  // This reports what the tick did, not what state the CPU ended up in. A tick
  // that executes HLT ran an instruction, so it reports kCPUTickExecuted even
  // though the CPU is now halted; the ticks that follow report kCPUTickHalted.
  return executed_instruction ? kCPUTickExecuted : kCPUTickHalted;
}
