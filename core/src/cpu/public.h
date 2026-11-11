// Public interface for the CPU emulator module.
#ifndef YAX86_CPU_PUBLIC_H
#define YAX86_CPU_PUBLIC_H

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_CPU_BUNDLE_H
#include "../util/log.h"
#endif  // YAX86_CPU_BUNDLE_H

enum {
  // Log module ID for the CPU.
  Y_CPU_LOG_MODULE_ID = 1,
};

// Log module for the CPU.
Y_PUBLIC_HEADER const YLogModule YCPULogModule = {
    .id = Y_CPU_LOG_MODULE_ID,
    .name = "cpu",
};

// ============================================================================
// CPU state
// ============================================================================

// CPU registers.
// Note that the order / numeric values of these constants are important here as
// they must match how the registers are encoded in the ModR/M byte.
typedef enum YCPURegisterIndex {
  // General-purpose and index registers.

  // Accumulator Register
  Y_CPU_AX = 0,
  // Counter Register
  Y_CPU_CX = 1,
  // Data Register
  Y_CPU_DX = 2,
  // Base Register
  Y_CPU_BX = 3,
  // Stack Pointer Register
  Y_CPU_SP = 4,
  // Base Pointer Register
  Y_CPU_BP = 5,
  // Source Index Register
  Y_CPU_SI = 6,
  // Destination Index Register
  Y_CPU_DI = 7,

  // Segment registers.

  // Extra Segment Register
  Y_CPU_ES = 8,
  // Code Segment Register
  Y_CPU_CS = 9,
  // Stack Segment Register
  Y_CPU_SS = 10,
  // Data Segment Register
  Y_CPU_DS = 11,

  // Instruction Pointer Register
  Y_CPU_IP,
} YCPURegisterIndex;

enum {
  // Number of registers.
  Y_CPU_NUM_REGISTERS = Y_CPU_IP + 1,
};

// CPU flag masks.
typedef enum YCPUFlag {
  // Carry Flag
  Y_CPU_CF = (1 << 0),
  // Parity Flag
  Y_CPU_PF = (1 << 2),
  // Auxiliary Carry Flag
  Y_CPU_AF = (1 << 4),
  // Zero Flag
  Y_CPU_ZF = (1 << 6),
  // Sign Flag
  Y_CPU_SF = (1 << 7),
  // Trap Flag
  Y_CPU_TF = (1 << 8),
  // Interrupt Enable Flag
  Y_CPU_IF = (1 << 9),
  // Direction Flag
  Y_CPU_DF = (1 << 10),
  // Overflow Flag
  Y_CPU_OF = (1 << 11),
} YCPUFlag;

enum {
  // Bits of the flags register that are not flags. The 8086/8088 does not
  // store them: bit 1 and bits 12 through 15 always read as one, and bits 3
  // and 5 always read as zero, whatever gets written over them.
  Y_CPU_FLAGS_ALWAYS_SET = 0xF002,
  Y_CPU_FLAGS_ALWAYS_CLEAR = 0x0028,
  // CPU flags value on reset - no flags set, and the bits that are not flags
  // reading the only way they can.
  Y_CPU_INITIAL_FLAGS = Y_CPU_FLAGS_ALWAYS_SET,
};

// Standard interrupts.
typedef enum YCPUInterruptNumber {
  Y_CPU_INTERRUPT_DIVIDE_ERROR = 0,
  Y_CPU_INTERRUPT_SINGLE_STEP = 1,
  Y_CPU_INTERRUPT_NMI = 2,
  Y_CPU_INTERRUPT_BREAKPOINT = 3,
  Y_CPU_INTERRUPT_OVERFLOW = 4,
} YCPUInterruptNumber;

// Result of executing a single instruction.
typedef enum YCPUInstructionResult {
  // The instruction was executed.
  Y_CPU_INSTRUCTION_EXECUTED = 0,
  // The instruction could not be executed, because its opcode has no handler or
  // because its encoding does not match the expected format for its opcode.
  Y_CPU_INSTRUCTION_INVALID,
} YCPUInstructionResult;

// Result of a single CPU tick.
typedef enum YCPUTickResult {
  // An instruction was executed. This includes HLT: the tick that halts the
  // CPU still ran an instruction.
  Y_CPU_TICK_EXECUTED = 0,
  // The instruction at CS:IP could not be fetched or executed. The CPU is left
  // pointing past the offending instruction; it is up to the caller to decide
  // whether to continue.
  Y_CPU_TICK_INVALID,
  // The CPU was already halted and executed no instruction this tick. It stays
  // halted until an interrupt wakes it, so the caller must keep ticking the
  // rest of the machine.
  Y_CPU_TICK_HALTED,
} YCPUTickResult;

// Result of the handle_interrupt callback, directing how the CPU should
// proceed with an interrupt.
typedef enum YCPUInterruptHandlerResult {
  // The callback serviced the interrupt itself. The CPU restores the state
  // saved on entry and resumes at the interrupted instruction.
  Y_CPU_INTERRUPT_HANDLER_HANDLED = 0,
  // The callback did not service the interrupt. The CPU dispatches it through
  // the Interrupt Vector Table instead.
  Y_CPU_INTERRUPT_HANDLER_UNHANDLED,
} YCPUInterruptHandlerResult;

struct YCPUState;
struct YCPUInstruction;
struct YCPUDecodeCacheEntry;

// Caller-provided runtime configuration.
typedef struct YCPUConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger for this module. May be NULL.
  YLogger* logger;

  // Callback to read a byte from memory.
  //
  // On the 8086, accessing an invalid memory address will yield garbage data
  // rather than causing a page fault. This callback interface mirrors that
  // behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  uint8_t (*read_memory_byte)(struct YCPUState* cpu, uint32_t address);

  // Callback handing the CPU a run of bytes it may read directly, for
  // instruction fetch. Optional - when NULL, every instruction byte is read
  // through read_memory_byte, which is what happens anyway wherever this
  // declines.
  //
  // Given a linear address the CPU wants to fetch from, fills in
  // YCPUState.instruction_fetch_window with a range covering it, or sets its
  // data to NULL to decline. A host must decline wherever a read has to be
  // observed or computed rather than loaded: a device region, unmapped memory,
  // or an address covered by anything the host has to be told about.
  //
  // The range should be the whole of whatever region the address falls in
  // rather than the tail of it starting at the address, so that a jump
  // backwards within the same region still lands inside it.
  //
  // The window is kept across instructions. Writes through the same buffer
  // need no invalidation, and self-modifying code keeps working, because the
  // window is a pointer into the host's own storage rather than a copy of it.
  // A host that wants it dropped clears data itself.
  void (*get_instruction_fetch_window)(struct YCPUState* cpu, uint32_t address);

  // Callback to write a byte to memory.
  //
  // On the 8086, accessing an invalid memory address will yield garbage data
  // rather than causing a page fault. This callback interface mirrors that
  // behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  void (*write_memory_byte)(
      struct YCPUState* cpu, uint32_t address, uint8_t value);

  // Callback modeling the interrupt acknowledge cycle the CPU runs in response
  // to a request on its INTR pin. Returns false if no external interrupt is
  // requested; otherwise stores the vector number and marks the interrupt in
  // service in the controller, exactly as the two INTA pulses do on real
  // hardware.
  //
  // The CPU only calls this at an instruction boundary with interrupts
  // enabled, so acknowledging and taking the interrupt are a single step and
  // no request can be latched, stranded, or overwritten in between. If NULL,
  // the CPU takes no external interrupts.
  bool (*acknowledge_interrupt)(struct YCPUState* cpu, uint8_t* vector);

  // Optional flag saying whether acknowledge_interrupt could possibly have
  // anything to report. The CPU reads it at every instruction boundary with
  // interrupts enabled, which is nearly all of them, and skips the
  // acknowledge cycle entirely while it reads false.
  //
  // This points into the interrupt controller's own state rather than being a
  // value the host hands over, so there is nothing to keep in step and nothing
  // to invalidate - a change the controller makes is visible to the next
  // instruction.
  //
  // The host may make it conservative: true where nothing turns out to be
  // takeable costs only a call that reports nothing. It may never be falsely
  // false, because the CPU takes it as permission not to ask at all. A host
  // that cannot promise that leaves it NULL and is asked every time.
  const bool* interrupt_request_hint;

  // Optional storage for the decode cache, which lets an instruction the guest
  // runs more than once be decoded once. NULL means every instruction is
  // decoded.
  //
  // Read only by YCPUInit(), which checks the count and clears its own copy of
  // the pointer if it does not pass.
  struct YCPUDecodeCacheEntry* decode_cache;
  // Must be a power of two of at least two. See decode_cache_index_mask.
  uint32_t decode_cache_num_entries;

  // Callback to handle an interrupt. If NULL, every interrupt is dispatched
  // through the Interrupt Vector Table.
  YCPUInterruptHandlerResult (*handle_interrupt)(
      struct YCPUState* cpu, uint8_t interrupt_number);

  // Callback to read a byte from an I/O port.
  //
  // On the 8086, accessing an invalid I/O port will most likely yield garbage
  // data. This callback interface mirrors that behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  uint8_t (*read_port)(struct YCPUState* cpu, uint16_t port);

  // Callback to write a byte to an I/O port.
  //
  // On the 8086, accessing an invalid I/O port will most likely yield garbage
  // data. This callback interface mirrors that behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  void (*write_port)(struct YCPUState* cpu, uint16_t port, uint8_t value);
} YCPUConfig;

// A run of bytes instruction fetch may read directly, covering the linear
// addresses [start, end), with data pointing at the byte at start.
//
// A NULL data means there is no window and every byte takes the ordinary path
// through YCPUConfig.read_memory_byte.
typedef struct YCPUInstructionFetchWindow {
  const uint8_t* data;
  uint32_t start;
  uint32_t end;
} YCPUInstructionFetchWindow;

// Guest memory the CPU may read and write by indexing, covering the half-open
// range of linear addresses [0, end), with data pointing at the byte at 0.
//
// This is not the same thing as the whole of guest RAM. It is the part of the
// address space that is plain host storage reachable with a bounds check and
// an index, so a host whose memory is not all directly addressable - some of
// it behind a bus, or reached through a driver - hands over the part that is
// and serves the rest through the callbacks.
//
// There is no start to go with end because the window always begins at 0,
// which is where guest RAM begins. That is what makes the bounds test a single
// unsigned compare.
//
// end is 0 exactly when data is NULL, which is what lets that compare stand
// alone rather than needing a null check beside it.
// YCPUSetDirectDataWindow() is what keeps the two in step.
typedef struct YCPUDirectDataWindow {
  uint8_t* data;
  uint32_t end;
} YCPUDirectDataWindow;

enum {
  // Granularity at which a write to guest memory discards decoded
  // instructions: 4KB pages over the 8086's 1MB, which is 256 counters. It is
  // the coarsest page that still separates the code a program is running from
  // the data it is writing, and a finer one would only cost more memory.
  Y_CPU_CODE_PAGE_SHIFT = 12,
  Y_CPU_CODE_PAGE_SIZE = 1 << Y_CPU_CODE_PAGE_SHIFT,
  Y_CPU_CODE_PAGE_OFFSET_MASK = Y_CPU_CODE_PAGE_SIZE - 1,
  Y_CPU_NUM_CODE_PAGES = 0x100000 >> Y_CPU_CODE_PAGE_SHIFT,
};

// State of the emulated CPU.
//
// A caller zero-initializes one of these, fills in the fields of config, and
// calls YCPUInit(). Everything outside config is the CPU's own. A register or
// flag may be written directly, since nothing is derived from one; a field
// another field depends on has a setter, which is its sole writer.
typedef struct YCPUState {
  // Register values
  uint16_t registers[Y_CPU_NUM_REGISTERS];
  // Flag values
  uint16_t flags;

  // An internal interrupt, raised by the CPU itself as a result of the
  // instruction it just executed: INT n, INT 3, INTO, a divide error, or a
  // single-step trap. IF does not gate these - INT 21h works with interrupts
  // disabled, which is how DOS calls inside critical sections behave.
  bool has_pending_internal_interrupt;
  // Interrupt number of the pending internal interrupt.
  uint8_t pending_internal_interrupt_number;

  // Whether the CPU is in halted state. When true, YCPUTick() will not fetch
  // or execute any instructions until an external event (e.g., an interrupt)
  // clears this state.
  bool is_halted;

  // Instructions retired since YCPUInit(), as the two halves of a 64-bit count.
  // A halted tick retires none. Read them as one number with
  // YCPUInstructionsRetired().
  //
  // Counted here rather than left to the caller because YCPUTick() already
  // knows whether it ran an instruction, while a caller can only find out by
  // sampling is_halted before every tick - which is what every benchmark
  // harness did, at the cost of giving up batching.
  //
  // Split rather than held as a uint64_t because the increment sits in the
  // hottest basic block the emulator has. A 64-bit one is eight instructions
  // on a Cortex-M0+ - two loads, two constants, an add, an add with carry and
  // two stores - where the low half alone is three and the carry above it is a
  // branch not taken until 2^32 instructions have run. Worth 1.19% at -O3.
  // The count still has to reach 64 bits: a machine left running retires 2^32
  // in a couple of hours.
  uint32_t instructions_retired_low;
  uint32_t instructions_retired_high;

  // Cycles charged by the instruction currently executing on top of its base
  // cost: its time on the data bus, and whatever it adds for itself when its
  // cost depends on its operands. YCPUTick clears this before each instruction
  // and folds it into cycles_this_tick afterwards.
  uint16_t pending_cycles;

  // Cycles the last call to YCPUTick consumed, at the 4.77MHz CPU clock. The
  // caller drives the rest of the machine from this, so that everything timed
  // against the CPU keeps the ratio real hardware has.
  //
  // A tick that runs several instructions charges the whole run here, so this
  // is what the run cost rather than what one instruction cost.
  uint16_t cycles_this_tick;

  // The run of bytes instruction fetch is currently reading from, as handed
  // over by YCPUConfig.get_instruction_fetch_window.
  //
  // Kept across instructions rather than re-derived per instruction: execution
  // is sequential and a window spans a whole memory region, so the next
  // instruction is almost always inside the one already open.
  YCPUInstructionFetchWindow instruction_fetch_window;

  // Guest memory the CPU reads and writes by indexing, as handed over by
  // YCPUSetDirectDataWindow(). Unlike the fetch window this is not asked for
  // per access: data accesses are scattered rather than sequential, so there
  // is no locality for a per-access callback to exploit and the host sets it
  // once instead.
  YCPUDirectDataWindow direct_data_window;

  // One less than YCPUConfig.decode_cache_num_entries, so that an address
  // becomes an index with a mask rather than a remainder - a remainder by a
  // runtime value is a division, which this target has no instruction for.
  // Derived by YCPUInit().
  uint32_t decode_cache_index_mask;

  // One byte per 4KB page: 0 while the decode cache holds nothing from the
  // page, and otherwise a count of the writes to it that wraps. A cached
  // decode records what its page stood at and is discarded once the two
  // disagree, which is what makes caching safe against code that writes over
  // itself or is loaded over an earlier program.
  //
  // A write to a page at 0 is not counted, so a page of stack or data never
  // disturbs the cache. Caching a decode from such a page starts it at 1.
  uint8_t code_page_generation[Y_CPU_NUM_CODE_PAGES];

  // Caller-provided runtime configuration. Filled in before YCPUInit(), which
  // is where it is checked, and read-only afterwards.
  //
  // Held by value rather than by pointer: a pointer cost 3.37% at -O2 on a
  // path that reads a config field every instruction, and made the config an
  // object the caller had to keep alive for as long as the CPU.
  YCPUConfig config;
} YCPUState;

// Initialize CPU state. The caller zero-initializes the YCPUState and fills in
// the fields of its config first.
//
// This zeroes nothing - a struct the caller has already had to zero in order
// to fill in its config does not want zeroing twice - and it is the only place
// the config is checked, so a host is told about a mistake here or not at
// all.
Y_PUBLIC void YCPUInit(YCPUState* cpu);

// Instructions retired since YCPUInit(), as one number.
Y_PUBLIC_HEADER inline uint64_t YCPUInstructionsRetired(const YCPUState* cpu) {
  return ((uint64_t)cpu->instructions_retired_high << 32) |
         cpu->instructions_retired_low;
}

// Get the value of a CPU flag.
Y_PUBLIC_HEADER inline bool YCPUGetFlag(const YCPUState* cpu, YCPUFlag flag) {
  return (cpu->flags & flag) != 0;
}
// Set a CPU flag.
Y_PUBLIC_HEADER inline void YCPUSetFlag(
    YCPUState* cpu, YCPUFlag flag, bool value) {
  if (value) {
    cpu->flags |= flag;
  } else {
    cpu->flags &= ~flag;
  }
}

// Raise an interrupt from within the CPU, to be taken at the end of the
// instruction currently executing. This is for the sources the 8086 calls
// internal - INT n, INT 3, INTO, a divide error, a single-step trap - which
// are not maskable by IF. External requests arrive on the INTR pin instead,
// via the acknowledge_interrupt callback.
Y_PUBLIC_HEADER inline void YCPURaiseInternalInterrupt(
    YCPUState* cpu, uint8_t interrupt_number) {
  cpu->has_pending_internal_interrupt = true;
  cpu->pending_internal_interrupt_number = interrupt_number;
}

// Discard a pending internal interrupt without taking it.
Y_PUBLIC_HEADER inline void YCPUClearInternalInterrupt(YCPUState* cpu) {
  cpu->has_pending_internal_interrupt = false;
  cpu->pending_internal_interrupt_number = 0;
}

// Charge the instruction currently executing for cycles beyond its base cost.
// Used by the instructions whose cost is not a property of the opcode alone -
// a conditional jump that is taken, a shift by a count in CL, a multiply or a
// divide.
Y_PUBLIC void YCPUAddCycles(YCPUState* cpu, uint16_t cycles);

// Hands the CPU guest memory it may read and write by indexing, covering the
// half-open range of linear addresses [0, end). Optional - a host that
// supplies none has every access go through YCPUConfig.read_memory_byte and
// YCPUConfig.write_memory_byte, which is also what happens for every address at
// or above end.
//
// The window must be plain storage whose reads and writes are the caller's
// buffer and nothing else. A host must not hand over a region where a read has
// to be observed or computed - a device, or anything else the host has to be
// told about - because an access through the window is a load or a store and
// the host never learns of it.
//
// Writes through the same buffer need no further call, so a host writing guest
// memory itself, or by DMA, stays coherent with the CPU for free. What does
// need a call is a change to what an address means: remapping memory, or
// enabling something that has to observe accesses, calls
// YCPUInvalidateDirectDataWindow().
Y_PUBLIC_HEADER inline void YCPUSetDirectDataWindow(
    YCPUState* cpu, uint8_t* data, uint32_t end) {
  cpu->direct_data_window.data = data;
  cpu->direct_data_window.end = data ? end : 0;
}

// Discards the direct data window, so that every access goes back through
// YCPUConfig.read_memory_byte and YCPUConfig.write_memory_byte.
Y_PUBLIC_HEADER inline void YCPUInvalidateDirectDataWindow(YCPUState* cpu) {
  cpu->direct_data_window.data = NULL;
  cpu->direct_data_window.end = 0;
}

// Discards every cached decode.
//
// A host calls this when it changes what an address means rather than what is
// stored at it - remapping memory is the case that matters. Ordinary writes
// are covered by YCPUNotifyMemoryWrite() instead.
Y_PUBLIC void YCPUInvalidateDecodeCache(YCPUState* cpu);

// Discards every cached decode taken from one 4KB page, and marks the page as
// holding none.
Y_PUBLIC void YCPUInvalidateDecodeCachePage(YCPUState* cpu, uint32_t page);

// Tells the CPU that the byte at a linear address has been written, so that
// any decode taken from that page stops being used.
//
// The CPU calls this for every write it makes itself, so what a host has to
// report is a write it makes some other way - by DMA, or through the memory
// map from outside a tick.
//
// A page nothing is cached from has nothing to discard, and most writes land
// on one: a program's stack and data. The counter is a byte, so it comes back
// round after 255 writes to a page, at which point a decode taken that long
// ago would look current again - hence discarding the page's decodes there,
// which leaves it uncounted until something is cached from it again. The
// address is masked rather than range checked: aliasing onto a
// page costs a spurious invalidation, where indexing past the array would
// corrupt whatever follows it.
Y_PUBLIC_HEADER inline void YCPUNotifyMemoryWrite(
    YCPUState* cpu, uint32_t address) {
  const uint32_t page =
      (address >> Y_CPU_CODE_PAGE_SHIFT) & (Y_CPU_NUM_CODE_PAGES - 1);
  const uint8_t generation = cpu->code_page_generation[page];
  if (generation == 0) {
    return;
  }
  if (generation == 0xFF) {
    YCPUInvalidateDecodeCachePage(cpu, page);
    return;
  }
  cpu->code_page_generation[page] = (uint8_t)(generation + 1);
}

// ============================================================================
// Instructions
// ============================================================================

enum {
  // Maximum number of displacement bytes in an 8086 instruction.
  Y_CPU_MAX_DISPLACEMENT_BYTES = 2,
  // Maximum number of immediate data bytes in an 8086 instruction.
  Y_CPU_MAX_IMMEDIATE_BYTES = 4,
  // Bytes an instruction carries besides its prefixes: the opcode, a ModR/M
  // byte, displacement, and immediate data.
  Y_CPU_MAX_NON_PREFIX_BYTES = 2 + Y_CPU_MAX_DISPLACEMENT_BYTES +
      Y_CPU_MAX_IMMEDIATE_BYTES,
  // Maximum number of prefix bytes accepted.
  //
  // The 8086 puts no limit on this. A real one fetches prefixes for as long as
  // they keep coming, and a stream of nothing but prefixes hangs it outright -
  // a prefix is not an instruction boundary, so no interrupt is ever
  // recognized. Two things here cannot follow it that far: the fetch loop has
  // to hand control back to its caller, and YCPUInstruction.size is a uint8_t
  // that
  // YCPUTick adds to IP. Accepting as many prefixes as still leave the whole
  // encoding addressable by size satisfies both, and is far past anything an
  // assembler emits.
  Y_CPU_MAX_PREFIX_BYTES = 0xFF - Y_CPU_MAX_NON_PREFIX_BYTES,
};

// Instruction prefixes.
typedef enum {
  Y_CPU_PREFIX_ES = 0x26,    // ES segment override
  Y_CPU_PREFIX_CS = 0x2E,    // CS segment override
  Y_CPU_PREFIX_SS = 0x36,    // SS segment override
  Y_CPU_PREFIX_DS = 0x3E,    // DS segment override
  Y_CPU_PREFIX_LOCK = 0xF0,  // LOCK
  // Undocumented alias of LOCK on the 8086/8088, which does not decode bit 0
  // of this opcode.
  Y_CPU_PREFIX_LOCK_ALT = 0xF1,
  Y_CPU_PREFIX_REPNZ = 0xF2,  // REPNE/REPNZ
  Y_CPU_PREFIX_REP = 0xF3,    // REP/REPE/REPZ
} YCPUInstructionPrefix;

enum {
  // Value of YCPUInstruction.segment_override when an instruction carries no
  // segment override prefix. Zero is Y_CPU_AX, which is never a segment
  // register,
  // so a zero-initialized YCPUInstruction correctly has no override.
  Y_CPU_NO_SEGMENT_OVERRIDE = 0,
};

// The Mod R/M byte.
// The fields are stored a byte each rather than packed back into the bit
// positions they were decoded from. Every read of one would otherwise be a
// load, a shift and a mask, and mod and rm are read on the hottest path in the
// emulator - YCPUGetMemoryOperandAddress() takes them for every memory operand
// and YCPUGetEffectiveAddressCycles() for every instruction - while reg selects
// the handler for all five instruction groups.
typedef struct YCPUModRM {
  // Mod field - bits 6 and 7 of the ModR/M byte.
  uint8_t mod;
  // REG field - bits 3 to 5 of the ModR/M byte.
  uint8_t reg;
  // R/M field - bits 0 to 2 of the ModR/M byte.
  uint8_t rm;
} YCPUModRM;

// An encoded instruction.
//
// Prefixes are resolved into fields as they are fetched rather than kept as
// raw bytes. Every consumer wants to know what a prefix selected, not which
// byte encoded it, and a field spares each of them a walk over the bytes.
//
// How many prefixes there were is not among them - nothing needs it, and
// size already accounts for the bytes they occupied. LOCK and its
// undocumented 0xF1 alias are consumed but not recorded at all, because
// nothing acts on them: the bus is not shared on a PC/XT.
//
// No field here is a bitfield, for the reason given at YCPUModRM above. That
// puts the struct at 16 bytes, which is what a decode cache entry is sized
// around - so the size is worth watching again, though for how many decodes fit
// in a given amount of memory rather than for what a fetch costs.
typedef struct YCPUInstruction {
  // The segment register selected by a segment override prefix, as a
  // YCPURegisterIndex, or Y_CPU_NO_SEGMENT_OVERRIDE if the instruction carries
  // none. A later override wins over an earlier one, as on hardware.
  uint8_t segment_override;

  // The repetition prefix present - Y_CPU_PREFIX_REP or Y_CPU_PREFIX_REPNZ - or
  // 0 if the instruction carries neither. A later one wins over an earlier one.
  uint8_t repetition_prefix;

  // The primary opcode byte.
  uint8_t opcode;

  // The ModR/M byte, which specifies addressing modes. For some instructions,
  // the REG field within this byte acts as an opcode extension.
  YCPUModRM mod_rm;

  // Raw displacement bytes. If displacement_size is 1, only disp_bytes[0] is
  // valid (value is typically sign-extended). If displacement_size is 2,
  // disp_bytes[0] is the low byte, disp_bytes[1] is the high byte.
  uint8_t displacement[Y_CPU_MAX_DISPLACEMENT_BYTES];

  // Raw immediate data bytes.
  uint8_t immediate[Y_CPU_MAX_IMMEDIATE_BYTES];

  // Flags

  // Flag indicating if a ModR/M byte is part of this instruction.
  bool has_mod_rm;
  // Number of displacement bytes present: 0, 1, or 2. A whole byte can express
  // more than displacement[] holds, so a consumer bounds itself by the array
  // rather than trusting this - see the fetch, which clamps its own writes.
  uint8_t displacement_size;
  // Number of immediate data bytes present: 0, 1, 2, or 4. Same caveat as
  // displacement_size.
  uint8_t immediate_size;

  // Total length of the original encoded instruction in bytes.
  uint8_t size;

  // What the instruction costs before it runs: its base cost from the opcode
  // table plus the effective address computation. Both are settled by the
  // encoding, so the decode works them out once and every later run of the
  // same decode is charged from here.
  uint16_t base_cycles;
} YCPUInstruction;

// One cached decode.
//
// A hit is used in place - the executor is handed a pointer to the instruction
// inside the entry and nothing is copied. That is the whole of why caching
// pays: copying the struct out measures 3.25% slower at -O3, because it costs
// about what decoding a short instruction from an open fetch window costs. No
// instruction handler writes through the YCPUInstruction it is given, so
// lending out the entry is safe.
typedef struct YCPUDecodeCacheEntry {
  // The linear address the instruction starts at, which is the key. First so
  // that the entry packs into 24 bytes: YCPUInstruction is 18 and needs 2-byte
  // alignment, where the key needs 4.
  uint32_t address;
  YCPUInstruction instruction;
  // What code_page_generation said for that address's page when the decode was
  // taken. A hit requires it to still say the same.
  uint8_t generation;
  bool valid;
} YCPUDecodeCacheEntry;

// ============================================================================
// Execution
// ============================================================================

// Result status from fetching the next instruction.
typedef enum YCPUFetchNextInstructionStatus {
  Y_CPU_FETCH_SUCCESS = 0,
  // Prefix exceeds maximum allowed size.
  Y_CPU_FETCH_PREFIX_TOO_LONG = -1,
} YCPUFetchNextInstructionStatus;

// Fetch the next instruction from CS:IP.
//
// The instruction is decoded directly into dest_instruction, so on failure
// dest_instruction holds however much had been decoded when the fetch failed.
// Since this function is part of the core CPU execution loop, assembling and
// copying a whole instruction struct would have a measurable impact on
// performance.
Y_PUBLIC YCPUFetchNextInstructionStatus
YCPUFetchNextInstruction(YCPUState* cpu, YCPUInstruction* instruction);

// Execute a single fetched instruction.
Y_PUBLIC YCPUInstructionResult
YCPUExecuteInstruction(YCPUState* cpu, YCPUInstruction* instruction);

enum {
  // The most instructions one call to YCPUTick() will run back to back.
  //
  // This is a bound on how coarse a tick can be, not on how long a run should
  // be - what governs that is max_run_cycles, which is what keeps a device
  // from being serviced late. This only caps the case where the next deadline
  // is far away, and its cost is that YPlatformRun() overshoots the budget it
  // was given by up to one run.
  //
  // On dos-boot, raising it from 4 to 16 is worth 2.9% and 32 only 0.5% beyond
  // that, while doubling the overshoot - so this is where the curve stops
  // paying rather than where it flattens.
  Y_CPU_MAX_INSTRUCTIONS_PER_TICK = 16,
};

// Run a single instruction cycle, including fetching and executing the next
// instruction at CS:IP, and handling interrupts.
//
// max_run_cycles is how long the tick may keep running before the host needs
// control back, which for a host driving a machine is how far away the nearest
// device deadline is. A tick runs several instructions back to back where it
// can, and this is what stops it running past the point at which a device
// should have been serviced - so the guest sees an interrupt exactly where it
// would have without the batching.
//
// Zero runs exactly one instruction. That is what a host stepping the machine
// passes, and what a host that has to see every instruction boundary itself
// passes, and it is the behaviour this had before there were runs at all.
Y_PUBLIC YCPUTickResult YCPUTick(YCPUState* cpu, uint16_t max_run_cycles);

#endif  // YAX86_CPU_PUBLIC_H
