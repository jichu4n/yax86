// ==============================================================================
// YAX86 CPU MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_CPU_BUNDLE_H
#define YAX86_CPU_BUNDLE_H

#ifndef YAX86_UTIL_BUNDLE_H
#include "util.h"
#endif  // YAX86_UTIL_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/cpu/public.h start
// ==============================================================================

#line 1 "./src/cpu/public.h"
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

// How the CPU reaches guest memory for data reads and writes, cheapest first:
//
//   1. The direct data window, YCPUDirectDataWindow: one host buffer holding
//      guest addresses [0, end). An access is a compare and a load, inline.
//   2. The paged data window, YCPUPagedDataWindow: guest addresses
//      [start, start + size), held in pages the host keeps in frames. Tried
//      only for an address the direct window does not cover; an access to a
//      page in its frame is a call and a few loads.
//   3. YCPUConfig.read_memory_byte and write_memory_byte: everything else,
//      such as ROM, video memory and devices, and any paged access whose
//      page's entry does not allow it.
//
// A host may set either window, both or neither, and the two must not
// overlap. The platform uses both when paged memory is configured: the direct
// window is direct_memory, [0, direct_memory_size), and the paged window
// starts right where it ends, so together they cover conventional memory. With
// direct_memory_size 0 the direct window is empty and all of it is paged.
//
// Instruction fetch does not go through either data window. It keeps its own
// YCPUInstructionFetchWindow, which opens on a page in its frame by itself
// and asks the host for anything else.

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

// Paged memory: guest memory that the host keeps in pages, only some of them
// in fast memory at a time, bringing the rest in on demand. It is for a host
// with less fast memory than the guest has RAM, such as a microcontroller
// holding the pages in slower external memory.
//
// Terms:
//   - A page is a fixed-size block of the paged range's guest addresses: page
//     0 is its first 1 << page_shift bytes, page 1 the next, and so on.
//   - A frame is a page-sized block of the host's fast memory, holding one
//     page's bytes while that page is in use.
//   - The page table has one uint16_t entry per page, saying whether the page
//     is in a frame, which one, and whether it has been written there:
//
//     bit   15         14         13 ........................ 0
//         +----------+----------+-------------------------------+
//         |  DIRTY   | RESIDENT |         frame number          |
//         +----------+----------+-------------------------------+
//
// What the CPU does with an entry:
//   - RESIDENT clear: every read and write of the page goes to the host. The
//     page is not in a frame, or the host wants to hear of the next access to
//     it. The frame number is not read.
//   - RESIDENT set, DIRTY clear: the CPU reads the page in the frame numbered
//     by bits 0-13, and a write goes to the host, which sets DIRTY.
//   - RESIDENT and DIRTY set: the CPU reads and writes the page in its frame.
// Every page is writable: DIRTY clear only means a write goes through the host
// first. So 0x0000 is "not in a frame", 0x4005 "in frame 5, not written
// since it came in", and 0xC005 "in frame 5, written".
//
// How an access goes: one the entry allows goes straight to the frame, with
// no call; YCPUPagedDataWindow shows the arithmetic. Any other goes to the
// host through YCPUConfig.read_memory_byte or write_memory_byte, and the host
// sets the entry so that it allows the access. In the platform, those
// callbacks call YPlatformPagedMemory.page_fault_fn.
//
// Why DIRTY exists: a page not written since it came into its frame still
// matches its copy in backing memory, so evicting it costs nothing, while a
// written ("dirty") one has to be written back first. Bringing a page in with
// DIRTY clear sends its first write to the host, once, which sets DIRTY; at
// eviction DIRTY says whether to write the page back. A page that is never
// written, as most code is not, is evicted without a write-back. A host that
// writes
// every page back, or whose frames are the memory itself, sets DIRTY on every
// page and never sees a write.
//
// Using RESIDENT to sample use: clearing RESIDENT on a page that stays in its
// frame sends the next access to the host, which sets it again; that is how a
// clock or LRU pager learns which pages were used recently. DIRTY is left as
// it is, so it stays the record of whether the page needs writing back.
//
// Rules for the host:
//   - The page table and frames are the host's. It may change entries at any
//     time between accesses: the CPU never writes one, and reads it afresh on
//     every access.
//   - A page's bytes must not change except through the guest's own writes,
//     since the CPU keeps instructions it has decoded from a page while the
//     page is out of its frame.
enum {
  // The page is in the frame the entry names, and may be read there.
  Y_CPU_PAGED_DATA_RESIDENT = 0x4000,
  // The page has been written since it came into its frame. With RESIDENT,
  // the CPU writes it in the frame; without, a write goes to the host, which
  // is how a host learns which pages need writing back.
  Y_CPU_PAGED_DATA_DIRTY = 0x8000,
  // Bits 0-13: the frame's number, so up to 16,384 frames.
  Y_CPU_PAGED_DATA_FRAME_MASK = 0x3FFF,
};

// Where the paged range is and how to find a page in its frame. Paging itself
// is described above Y_CPU_PAGED_DATA_RESIDENT. The platform fills this in
// from YPlatformPagedMemory. The CPU translates an address like this:
//
//     offset = address - start                (outside if offset >= size)
//     entry  = pages[offset >> page_shift]
//     frame  = entry & Y_CPU_PAGED_DATA_FRAME_MASK
//     byte   = frames[(frame << page_shift) | (offset & (page size - 1))]
//
// and goes to the host instead if the entry lacks the bits the access needs -
// RESIDENT to read, RESIDENT and DIRTY to write.
typedef struct YCPUPagedDataWindow {
  // The page table: entry i covers the guest addresses from
  // start + (i << page_shift) up to the next page.
  const uint16_t* pages;
  // Where the frames are. Frame n is the page-sized block starting
  // n << page_shift bytes in.
  uint8_t* frames;
  // The first guest linear address of the paged range.
  uint32_t start;
  // The paged range's size in bytes, a whole number of pages; 0 when there is
  // no paged memory. With 0 every address falls outside the range, so the CPU
  // needs only the bounds test, one subtract and one unsigned compare, and
  // never checks pages against NULL.
  uint32_t size;
  // log2 of the page size: a page, and a frame, is 1 << page_shift bytes.
  uint8_t page_shift;
} YCPUPagedDataWindow;

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

  // How many times each 4KB page has been written, as a wrapping byte. A
  // cached decode records what its page stood at and is discarded once the two
  // disagree, which is what makes caching safe against code that writes over
  // itself or is loaded over an earlier program.
  //
  // Maintained whether or not a decode cache exists: a write cannot cheaply
  // tell whether anything holds a decode of the page it lands on, and the test
  // to find out would cost about what the counter does.
  uint8_t code_page_generation[Y_CPU_NUM_CODE_PAGES];

  // Caller-provided runtime configuration. Filled in before YCPUInit(), which
  // is where it is checked, and read-only afterwards.
  //
  // Held by value rather than by pointer: a pointer cost 3.37% at -O2 on a
  // path that reads a config field every instruction, and made the config an
  // object the caller had to keep alive for as long as the CPU.
  YCPUConfig config;

  // Paged guest memory. The platform assigns it whole from
  // YPlatformConfig.paged_memory, and leaves it zero for none. Changing page
  // table entries needs no update here: the CPU reads them on every access.
  // Read only where the direct data window does not reach, so it is last,
  // where it moves no hotter field's offset.
  YCPUPagedDataWindow paged_data_window;
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

// Discards the instruction fetch window, so that the next fetch asks
// YCPUConfig.get_instruction_fetch_window for one again. A host calls this when
// the storage the window points into stops holding the addresses it covers.
Y_PUBLIC_HEADER inline void YCPUInvalidateInstructionFetchWindow(
    YCPUState* cpu) {
  cpu->instruction_fetch_window.data = NULL;
}

// Discards every cached decode.
//
// A host calls this when it changes what an address means rather than what is
// stored at it - remapping memory is the case that matters. Ordinary writes
// are covered by YCPUNotifyMemoryWrite() instead.
Y_PUBLIC void YCPUInvalidateDecodeCache(YCPUState* cpu);

// Tells the CPU that the byte at a linear address has been written, so that
// any decode taken from that page stops being used.
//
// The CPU calls this for every write it makes itself, so what a host has to
// report is a write it makes some other way - by DMA, or through the memory
// map from outside a tick.
//
// The counter is a byte, so it comes back round every 256 writes to a page, at
// which point a decode taken exactly that long ago would look current again -
// hence the flush. The address is masked rather than range checked: aliasing
// onto a page costs a spurious invalidation, where indexing past the array
// would corrupt whatever follows it.
Y_PUBLIC_HEADER inline void YCPUNotifyMemoryWrite(
    YCPUState* cpu, uint32_t address) {
  const uint32_t page =
      (address >> Y_CPU_CODE_PAGE_SHIFT) & (Y_CPU_NUM_CODE_PAGES - 1);
  if (++cpu->code_page_generation[page] == 0) {
    YCPUInvalidateDecodeCache(cpu);
  }
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


// ==============================================================================
// src/cpu/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/cpu/types.h start
// ==============================================================================

#line 1 "./src/cpu/types.h"
#ifndef YAX86_CPU_TYPES_H
#define YAX86_CPU_TYPES_H

#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Data width helpers.

// Data widths supported by the 8086 CPU.
typedef enum YCPUWidth {
  Y_CPU_BYTE = 0,
  Y_CPU_WORD,
} YCPUWidth;

enum {
  // Number of data width types.
  Y_CPU_NUM_WIDTHS = Y_CPU_WORD + 1,
};

enum {
  // How many bytes a segment addresses. An offset is 16 bits and wraps within
  // its segment where a linear address does not, so this is where anything
  // walking memory through an offset has to stop.
  Y_CPU_SEGMENT_SIZE = 0x10000,
};

// Bitmask to extract the sign bit of a value.
Y_MODULE_PRIVATE_HEADER const uint32_t YCPUSignBit[Y_CPU_NUM_WIDTHS] = {
    1 << 7,   // Y_CPU_BYTE
    1 << 15,  // Y_CPU_WORD
};

// Maximum unsigned value for each data width.
Y_MODULE_PRIVATE_HEADER const uint32_t YCPUMaxValue[Y_CPU_NUM_WIDTHS] = {
    0xFF,   // Y_CPU_BYTE
    0xFFFF  // Y_CPU_WORD
};

// Maximum signed value for each data width.
Y_MODULE_PRIVATE_HEADER const int32_t YCPUMaxSignedValue[Y_CPU_NUM_WIDTHS] = {
    0x7F,   // Y_CPU_BYTE
    0x7FFF  // Y_CPU_WORD
};

// Minimum signed value for each data width.
Y_MODULE_PRIVATE_HEADER const int32_t YCPUMinSignedValue[Y_CPU_NUM_WIDTHS] = {
    -0x80,   // Y_CPU_BYTE
    -0x8000  // Y_CPU_WORD
};

// Number of bytes in each data width.
Y_MODULE_PRIVATE_HEADER const uint8_t YCPUNumBytes[Y_CPU_NUM_WIDTHS] = {
    1,  // Y_CPU_BYTE
    2,  // Y_CPU_WORD
};

// Number of bits in each data width.
Y_MODULE_PRIVATE_HEADER const uint8_t YCPUNumBits[Y_CPU_NUM_WIDTHS] = {
    8,   // Y_CPU_BYTE
    16,  // Y_CPU_WORD
};

// Operand types.

// The address of a register operand.
typedef struct YCPURegisterAddress {
  // Register index. A uint8_t so the layout does not depend on
  // -fshort-enums.
  uint8_t register_index;
  // Byte offset within the register; only relevant for byte-sized operands.
  // 0 for low byte (AL, CL, DL, BL), 8 for high byte (AH, CH, DH, BH).
  uint8_t byte_offset;
} YCPURegisterAddress;

// The address of a memory operand.
typedef struct YCPUMemoryAddress {
  // Segment register. A uint8_t because YCPUApplySegmentOverride() writes it
  // through a pointer, and an enum's width is the target's business - one byte
  // under -fshort-enums and four otherwise.
  uint8_t segment_register_index;
  // Effective address offset.
  uint16_t offset;
} YCPUMemoryAddress;

// Whether the operand is a register or memory operand.
typedef enum YCPUOperandAddressType {
  Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER = 0,
  Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
} YCPUOperandAddressType;

enum {
  // Number of operand address types.
  Y_CPU_NUM_OPERAND_ADDRESS_TYPES = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY + 1,
};

// Where an operand lives.
//
// Four bytes, which is the largest a composite AAPCS returns in a register
// rather than through memory.
typedef struct YCPUOperandAddress {
  // Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER or Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY. A
  // uint8_t rather than the enum to ensure the four byte layout.
  uint8_t type;
  // The register the operand is in, or the segment register it is addressed
  // through.
  uint8_t register_index;
  // The byte within that register - 0 for AL, 8 for AH - or the effective
  // address offset within that segment.
  uint16_t offset;
} YCPUOperandAddress;

// The value of an operand.
//
// A number rather than a struct, because a struct here is a struct on the
// stack: an operand value is produced and consumed on the hottest path in the
// emulator, and one that does not fit in a register travels through memory
// every time it is passed, returned or assigned.
//
// A byte-wide value is held in the low byte with the high byte zero. Every
// path that produces one truncates to a byte, which is what lets a consumer
// widen it by doing nothing. What the type no longer carries is the width, and
// the width is what sign extension needs - so that is passed explicitly, from
// the opcode table entry the caller already has.
typedef uint16_t YCPUOperandValue;

// An operand.
typedef struct YCPUOperand {
  // Address of the operand.
  YCPUOperandAddress address;
  // Value of the operand.
  YCPUOperandValue value;
} YCPUOperand;

// Instruction types.

struct YCPUOpcodeMetadata;

// Context during instruction execution.
typedef struct {
  YCPUState* cpu;
  const YCPUInstruction* instruction;
  const struct YCPUOpcodeMetadata* metadata;
} YCPUInstructionContext;

// Handler function for an opcode.
typedef YCPUInstructionResult (*YCPUOpcodeHandler)(
    const YCPUInstructionContext* context);

// An entry in the opcode lookup table.
typedef struct YCPUOpcodeMetadata {
  // What the instruction costs before the effective address computation and
  // its time on the data bus - see cycles.c for how the three fit together.
  uint8_t base_cycles;

  // Instruction has ModR/M byte.
  bool has_modrm;

  // Number of immediate data bytes: 0, 1, 2, or 4.
  uint8_t immediate_size;

  // Width of the instruction's operands.
  YCPUWidth width : 8;

  // Handler function.
  YCPUOpcodeHandler handler;
} YCPUOpcodeMetadata;

#endif  // YAX86_CPU_TYPES_H


// ==============================================================================
// src/cpu/types.h end
// ==============================================================================

// ==============================================================================
// src/cpu/cycles.h start
// ==============================================================================

#line 1 "./src/cpu/cycles.h"
#ifndef YAX86_CPU_CYCLES_H
#define YAX86_CPU_CYCLES_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

enum {
  // What a tick costs while the CPU is halted. It is waiting for an interrupt
  // rather than stopped, so time still passes - and the timer that will wake
  // it is driven from that time.
  Y_CPU_HALTED_CYCLES = 4,
  // Extra cycles a taken jump costs, for the prefetch queue it throws away.
  Y_CPU_JUMP_TAKEN_CYCLES = 12,
  // Extra cycles a shift or rotate costs for each bit it moves, when the count
  // comes from CL rather than being 1.
  Y_CPU_SHIFT_CYCLES_PER_BIT = 4,
  // Cycles per byte transferred over the data bus. The 8088's bus is 8 bits
  // wide, so a word costs twice a byte.
  Y_CPU_BUS_CYCLES_PER_BYTE = 4,
};

// Cycles to compute the effective address of a ModR/M memory operand.
Y_MODULE_PRIVATE uint8_t
YCPUGetEffectiveAddressCycles(const YCPUInstruction* instruction);

// Charge the instruction currently executing for time on the data bus.
Y_MODULE_PRIVATE void YCPUAddBusCycles(YCPUState* cpu, uint8_t num_bytes);

#endif  // YAX86_CPU_CYCLES_H


// ==============================================================================
// src/cpu/cycles.h end
// ==============================================================================

// ==============================================================================
// src/cpu/operands.h start
// ==============================================================================

#line 1 "./src/cpu/operands.h"
#ifndef YAX86_CPU_OPERANDS_H
#define YAX86_CPU_OPERANDS_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// Narrow a computed result to what an operand of the given width holds.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUToOperandValue(YCPUWidth width, uint32_t raw_value);

// Helper function to zero-extend YCPUOperandValue to a 32-bit value. This makes
// it simpler to do direct arithmetic without worrying about overflow.
Y_MODULE_PRIVATE uint32_t YCPUFromOperandValue(YCPUOperandValue value);

// Helper function to sign-extend YCPUOperandValue to a 32-bit value. This makes
// it simpler to do direct arithmetic without worrying about overflow.
Y_MODULE_PRIVATE int32_t
YCPUFromSignedOperandValue(YCPUWidth width, YCPUOperandValue value);

// Helper function to extract a zero-extended value from an operand.
Y_MODULE_PRIVATE uint32_t YCPUFromOperand(const YCPUOperand* operand);

// Helper function to extract a sign-extended value from an operand.
Y_MODULE_PRIVATE int32_t
YCPUFromSignedOperand(YCPUWidth width, const YCPUOperand* operand);

// Computes the raw address a segment register and an offset address.
Y_MODULE_PRIVATE uint32_t YCPUToRawAddress(
    const YCPUState* cpu, uint8_t segment_register_index, uint16_t offset);

// Read a byte from memory as a uint8_t.
Y_MODULE_PRIVATE uint8_t
YCPUReadRawMemoryByte(YCPUState* cpu, uint32_t raw_address);

// Read a word from memory as a uint16_t.
Y_MODULE_PRIVATE uint16_t
YCPUReadRawMemoryWord(YCPUState* cpu, uint32_t raw_address);

// Read a byte from memory as a YCPUOperandValue.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadMemoryOperandByte(YCPUState* cpu, const YCPUOperandAddress* address);

// Read a word from memory as a YCPUOperandValue.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadMemoryOperandWord(YCPUState* cpu, const YCPUOperandAddress* address);

// Read a byte from a register as a YCPUOperandValue.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadRegisterOperandByte(YCPUState* cpu, const YCPUOperandAddress* address);

// Read a word from a register as a YCPUOperandValue.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadRegisterOperandWord(YCPUState* cpu, const YCPUOperandAddress* address);

// Write a byte as uint8_t to memory.
Y_MODULE_PRIVATE void YCPUWriteRawMemoryByte(
    YCPUState* cpu, uint32_t address, uint8_t value);

// Write a byte to memory.
Y_MODULE_PRIVATE void YCPUWriteMemoryOperandByte(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value);

// Write a word to memory.
Y_MODULE_PRIVATE void YCPUWriteMemoryOperandWord(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value);

// Write a byte to a register.
Y_MODULE_PRIVATE void YCPUWriteRegisterOperandByte(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value);

// Write a word to a register.
Y_MODULE_PRIVATE void YCPUWriteRegisterOperandWord(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value);

// Add an 8-bit signed relative offset to a 16-bit unsigned base address.
Y_MODULE_PRIVATE uint16_t
YCPUAddSignedOffsetByte(uint16_t base, uint8_t raw_offset);

// Add a 16-bit signed relative offset to a 16-bit unsigned base address.
Y_MODULE_PRIVATE uint16_t
YCPUAddSignedOffsetWord(uint16_t base, uint16_t raw_offset);

// Get the register operand for a byte instruction based on the ModR/M byte's
// reg or R/M field.
Y_MODULE_PRIVATE YCPURegisterAddress
YCPUGetRegisterAddressByte(YCPUState* cpu, uint8_t reg_or_rm);

// Get the register operand for a word instruction based on the ModR/M byte's
// reg or R/M field.
Y_MODULE_PRIVATE YCPURegisterAddress
YCPUGetRegisterAddressWord(YCPUState* cpu, uint8_t reg_or_rm);

// Replace a segment register index with whatever the instruction's segment
// override prefix names, if it carries one.
Y_MODULE_PRIVATE void YCPUApplySegmentOverride(
    const YCPUInstruction* instruction, uint8_t* segment_register_index);

// Compute the memory address for an instruction.
Y_MODULE_PRIVATE YCPUMemoryAddress
YCPUGetMemoryOperandAddress(YCPUState* cpu, const YCPUInstruction* instruction);

// Get a register or memory operand address based on the ModR/M byte and
// displacement, without reading the value currently there.
Y_MODULE_PRIVATE YCPUOperandAddress
YCPUGetRegisterOrMemoryOperandAddress(const YCPUInstructionContext* ctx);

// Read an 8-bit immediate value.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadImmediateOperandByte(const YCPUInstruction* instruction);

// Read a 16-bit immediate value.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadImmediateOperandWord(const YCPUInstruction* instruction);

// Read a value from an operand address.
Y_MODULE_PRIVATE YCPUOperandValue YCPUReadOperandValue(
    const YCPUInstructionContext* ctx, const YCPUOperandAddress* address);

// Get a register or memory operand for an instruction based on the ModR/M
// byte and displacement.
Y_MODULE_PRIVATE void YCPUReadRegisterOrMemoryOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand);

// Get a register operand for an instruction.
Y_MODULE_PRIVATE void YCPUReadRegisterOperandForRegisterIndex(
    const YCPUInstructionContext* ctx, YCPURegisterIndex register_index,
    YCPUOperand* operand);

// Get a register operand for an instruction from the REG field of the Mod/RM
// byte.
Y_MODULE_PRIVATE void YCPUReadRegisterOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand);

// Get a segment register operand for an instruction from the REG field of the
// Mod/RM byte.
Y_MODULE_PRIVATE void YCPUReadSegmentRegisterOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand);

// Write a value to a register or memory operand address.
Y_MODULE_PRIVATE void YCPUWriteOperandAddress(
    const YCPUInstructionContext* ctx, const YCPUOperandAddress* address,
    uint32_t raw_value);

// Write a value to a register or memory operand.
Y_MODULE_PRIVATE void YCPUWriteOperand(
    const YCPUInstructionContext* ctx, const YCPUOperand* operand,
    uint32_t raw_value);

// Read an immediate value from the instruction.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadImmediate(const YCPUInstructionContext* ctx);

#endif  // YAX86_CPU_OPERANDS_H


// ==============================================================================
// src/cpu/operands.h end
// ==============================================================================

// ==============================================================================
// src/cpu/cycles.c start
// ==============================================================================

#line 1 "./src/cpu/cycles.c"
#ifndef YAX86_IMPLEMENTATION
#include "cycles.h"

#include "../util/common.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Instruction timing
// ============================================================================
//
// How long each instruction takes, in 4.77MHz CPU clock cycles. This is not a
// cycle-accurate model - it does not track the prefetch queue, and it charges
// a whole instruction at its boundary rather than spreading it over the bus
// cycles it really occupies. What it does give is a clock that means
// something: the ratio between time spent executing and time measured by the
// PIT comes out right, so a guest that calibrates a delay loop against the
// timer arrives at roughly the figure real hardware would.
//
// The cost of an instruction is built from three parts.
//
// 1. A base cost per opcode, which is YCPUOpcodeMetadata.base_cycles in
//    YCPUOpcodeTable.c. These are the published 8086 figures for the register
//    form, less the time the figure already accounts for on the bus, which is
//    charged separately in part 3. Where the published figure covers an
//    instruction that necessarily touches memory - the stack instructions, the
//    string instructions, the software interrupts - that bus time has been
//    taken back out, so that charging the traffic separately does not count it
//    twice. PUSH ES is 2 rather than 10 for this reason, and POP r16 is 0.
//
// 2. The effective address calculation, for instructions that address memory
//    through a ModR/M byte.
//
// 3. Four cycles for every byte the instruction moves over the data bus. The
//    8088 has an 8-bit bus, so a word costs twice a byte - this is the main
//    reason it is slower than the 8086 it shares timings with, and it is the
//    dominant term for most instructions. Charging it from the actual
//    accesses rather than from a table means the string instructions, the
//    stack and the interrupt sequence all cost what their traffic costs,
//    including when REP runs them many times over.
//
// Instructions whose cost depends on more than their operands - a conditional
// jump that is taken, a shift by a count in CL, a multiply or divide - add the
// difference themselves through YCPUAddCycles().

// Cycles to compute an effective address, by addressing mode. The 8086 pays
// for each component it has to add together.
enum {
  // A displacement on its own.
  Y_CPU_EA_CYCLES_DISPLACEMENT_ONLY = 6,
  // A single base or index register.
  Y_CPU_EA_CYCLES_BASE_OR_INDEX = 5,
  // A base or index register plus a displacement.
  Y_CPU_EA_CYCLES_BASE_OR_INDEX_AND_DISPLACEMENT = 9,
  // Base plus index. BP+DI and BX+SI cost one cycle less than the other two
  // pairings, which this does not distinguish.
  Y_CPU_EA_CYCLES_BASE_AND_INDEX = 8,
  // Base plus index plus a displacement.
  Y_CPU_EA_CYCLES_BASE_AND_INDEX_AND_DISPLACEMENT = 12,
  // A segment override prefix costs two more, since the address has to be
  // formed against a different segment base.
  Y_CPU_EA_CYCLES_SEGMENT_OVERRIDE = 2,
};

// Cycles to compute the effective address of a ModR/M memory operand.
Y_MODULE_PRIVATE uint8_t
YCPUGetEffectiveAddressCycles(const YCPUInstruction* instruction) {
  if (!instruction->has_mod_rm || instruction->mod_rm.mod == 0x03) {
    // A register operand needs no address computed.
    return 0;
  }

  const uint8_t mod = instruction->mod_rm.mod;
  const uint8_t rm = instruction->mod_rm.rm;
  const bool has_displacement =
      mod == 0x01 || mod == 0x02 || (mod == 0x00 && rm == 0x06);
  // R/M values 0 through 3 pair a base register with an index register. The
  // rest name a single register, except for the direct address at mod 0, rm 6.
  const bool has_base_and_index = rm <= 0x03;
  const bool is_direct_address = mod == 0x00 && rm == 0x06;

  uint8_t cycles;
  if (is_direct_address) {
    cycles = Y_CPU_EA_CYCLES_DISPLACEMENT_ONLY;
  } else if (has_base_and_index) {
    cycles = has_displacement ? Y_CPU_EA_CYCLES_BASE_AND_INDEX_AND_DISPLACEMENT
                              : Y_CPU_EA_CYCLES_BASE_AND_INDEX;
  } else {
    cycles = has_displacement ? Y_CPU_EA_CYCLES_BASE_OR_INDEX_AND_DISPLACEMENT
                              : Y_CPU_EA_CYCLES_BASE_OR_INDEX;
  }

  // Charged once for an instruction that carries a segment override, rather
  // than per override prefix. Only one can take effect, and real code never
  // emits more than one.
  if (instruction->segment_override != Y_CPU_NO_SEGMENT_OVERRIDE) {
    cycles += Y_CPU_EA_CYCLES_SEGMENT_OVERRIDE;
  }
  return cycles;
}

Y_MODULE_PRIVATE void YCPUAddBusCycles(YCPUState* cpu, uint8_t num_bytes) {
  cpu->pending_cycles += (uint16_t)num_bytes * Y_CPU_BUS_CYCLES_PER_BYTE;
}

Y_PUBLIC void YCPUAddCycles(YCPUState* cpu, uint16_t cycles) {
  cpu->pending_cycles += cycles;
}


// ==============================================================================
// src/cpu/cycles.c end
// ==============================================================================

// ==============================================================================
// src/cpu/operands.c start
// ==============================================================================

#line 1 "./src/cpu/operands.c"
#ifndef YAX86_IMPLEMENTATION
#include "operands.h"

#include "../util/common.h"
#include "cycles.h"
#endif  // YAX86_IMPLEMENTATION

// What the unreachable arm of a width dispatch returns.
//
// YCPUWidth names two values and every switch below handles both, so nothing
// reaches these arms. They exist because C does not promise an enum object
// holds only the values its enumerators name, and a function with a return
// type has to return something.
//
// All ones in the low byte is what an 8086 reads from an address nothing
// drives, and it is a legal value at either width. 0xFFFF is not: at Y_CPU_BYTE
// it would break the zero high byte a byte-wide value is promised to have,
// which is the invariant YCPUFromOperandValue() widens by doing nothing.
enum { Y_CPU_UNREACHABLE_OPERAND_VALUE = 0xFF };

// Narrow a computed result to what an operand of the given width holds. A
// byte keeps a zero high byte, which is what makes widening it again free.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUToOperandValue(YCPUWidth width, uint32_t raw_value) {
  return (YCPUOperandValue)(raw_value & YCPUMaxValue[width]);
}

// Widen an operand value for arithmetic, which is done in 32 bits so that
// nothing overflows on the way. A byte value already has a zero high byte, so
// this is where that invariant is spent rather than a width being consulted.
Y_MODULE_PRIVATE uint32_t YCPUFromOperandValue(YCPUOperandValue value) {
  return value;
}

// The same, sign-extended. This one does need the width, since which bit is
// the sign bit is exactly what the value no longer says.
//
// One test, byte or not, rather than a switch with a return after it for the
// values outside the enum that a width can still hold. That return is a third
// path, and once this is inlined GCC carries it on to the caller's next
// lookup in a table indexed by width - YCPUMaxSignedValue in IMUL and
// YCPUMulDivResultHighHalfAddress in IDIV - and warns that the index can be 2.
Y_MODULE_PRIVATE int32_t
YCPUFromSignedOperandValue(YCPUWidth width, YCPUOperandValue value) {
  if (width == Y_CPU_BYTE) {
    return (int32_t)((int8_t)value);
  }
  return (int32_t)((int16_t)value);
}

// Helper function to extract a zero-extended value from an operand.
Y_MODULE_PRIVATE uint32_t YCPUFromOperand(const YCPUOperand* operand) {
  return YCPUFromOperandValue(operand->value);
}

// Helper function to extract a sign-extended value from an operand.
Y_MODULE_PRIVATE int32_t
YCPUFromSignedOperand(YCPUWidth width, const YCPUOperand* operand) {
  return YCPUFromSignedOperandValue(width, operand->value);
}

enum {
  // The address bus is 20 bits wide. A segment base plus an offset can sum to
  // as much as 0x10FFEF, and the carry out of bit 19 goes nowhere, so an
  // address past the top of memory wraps around to the bottom.
  Y_CPU_PHYSICAL_ADDRESS_MASK = 0xFFFFF,
};

// Computes the raw address a segment register and an offset address.
Y_MODULE_PRIVATE uint32_t YCPUToRawAddress(
    const YCPUState* cpu, uint8_t segment_register_index, uint16_t offset) {
  uint16_t segment = cpu->registers[segment_register_index];
  return ((((uint32_t)segment) << 4) + (uint32_t)offset) &
         Y_CPU_PHYSICAL_ADDRESS_MASK;
}

// The byte in its frame that a guest address in the paged range refers to, or
// NULL if the address is outside the range or its page's entry lacks any of
// flags. A read passes Y_CPU_PAGED_DATA_RESIDENT, and a write that and
// Y_CPU_PAGED_DATA_DIRTY. NULL sends the access to the host's callback.
//
// This is the translation described at YCPUPagedDataWindow. With 256-byte
// pages starting at 0x10000, guest address 0x11234 whose page's entry is
// 0x4005 is byte 0x34 of page 0x12, held in frame 5: frames + 0x534.
Y_FILE_PRIVATE inline uint8_t* YCPUGetPagedDataByte(
    const YCPUState* cpu, uint32_t raw_address, uint16_t flags) {
  const YCPUPagedDataWindow* window = &cpu->paged_data_window;

  // How far into the paged range the address is. An address below start
  // wraps to a huge value, so this one unsigned compare rejects addresses on
  // both sides of the range, and every address when size is 0.
  const uint32_t offset = raw_address - window->start;
  if (offset >= window->size) {
    return NULL;
  }

  // The page the address is in, and the page's entry in the page table.
  const uint32_t page = offset >> window->page_shift;
  const uint16_t entry = window->pages[page];

  // The access needs every bit in flags. Testing that any one is set would let
  // a write through to a dirty page whose RESIDENT the host cleared to sample
  // use, and the host would never hear of it.
  if ((entry & flags) != flags) {
    return NULL;
  }

  // Where the page's frame starts in frames: frames are a page long and laid
  // out back to back, so frame n starts n pages in. The frame number is the
  // entry with RESIDENT and DIRTY masked off, widened before the shift so the
  // arithmetic is unsigned.
  const uint32_t frame = entry & Y_CPU_PAGED_DATA_FRAME_MASK;
  const uint32_t frame_start = frame << window->page_shift;

  // How far into its page the address is: the offset's low page_shift bits.
  const uint32_t page_mask = ((uint32_t)1 << window->page_shift) - 1;
  const uint32_t offset_in_page = offset & page_mask;

  // frame_start's low page_shift bits are all 0 and offset_in_page fits in
  // them, so OR-ing them is the same as adding them.
  return window->frames + (frame_start | offset_in_page);
}
const uint16_t entry = window->pages[offset >> window->page_shift];
if ((entry & flags) != flags) {
  return NULL;
}
const uint32_t page_mask = ((uint32_t)1 << window->page_shift) - 1;
return window->frames + (((uint32_t)(entry & Y_CPU_PAGED_DATA_FRAME_MASK)
                          << window->page_shift) |
                         (offset & page_mask));
}

// Read a byte at an address the direct data window does not cover: from its
// frame if the address is paged and its page is in one, and otherwise from the
// host, which brings a paged-out page in.
//
// This is the slow half of YCPUReadRawMemoryByte(), split out so that the fast
// half stays small. YCPUReadRawMemoryByte() is inlined into every place the
// CPU reads memory, and its fast half is the direct window's compare and load.
// Everything else, the paged lookup and the host's callback, is in this one
// copy, and each inlined read carries only a call to it. Y_NOINLINE keeps it
// that way: the compiler would otherwise inline a small static function back
// into its callers.
//
// The split favours a host whose guest RAM is in the direct window, where the
// fast half is all a read of RAM needs. Measured with pico/bench's dos-boot
// workload on an RP2040 at 250MHz, -O3, with paging off: the paged lookup
// inlined into every read cost 2.6%, and with the callback moved out of line
// as well, the boot is 2.6% faster than with the callback inline. Y_HOT puts
// it in SRAM; in flash it measured 0.26% slower.
//
// A host that pages all of conventional memory has an empty direct window, so
// every data access fails the compare and makes this call. What that costs,
// and the changes to try, are in core/AGENTS.md under paged memory.
Y_FILE_PRIVATE Y_HOT Y_NOINLINE uint8_t
YCPUReadMemoryByteOutsideDirectWindow(YCPUState* cpu, uint32_t raw_address) {
  const uint8_t* paged =
      YCPUGetPagedDataByte(cpu, raw_address, Y_CPU_PAGED_DATA_RESIDENT);
  if (paged) {
    return *paged;
  }
  return cpu->config.read_memory_byte
             ? cpu->config.read_memory_byte(cpu, raw_address)
             : 0xFF;
}

// Read a byte from memory as a uint8_t.
Y_MODULE_PRIVATE uint8_t
YCPUReadRawMemoryByte(YCPUState* cpu, uint32_t raw_address) {
  // Guest RAM, where nearly every operand lands, is reached by indexing. The
  // call below ends up indexing an array too, having gone through a function
  // pointer, the host's context and a memory map lookup to arrive at it.
  if (raw_address < cpu->direct_data_window.end) {
    return cpu->direct_data_window.data[raw_address];
  }
  return YCPUReadMemoryByteOutsideDirectWindow(cpu, raw_address);
}

// Read a word from memory as a uint16_t.
Y_MODULE_PRIVATE uint16_t
YCPUReadRawMemoryWord(YCPUState* cpu, uint32_t raw_address) {
  uint8_t low_byte_value = YCPUReadRawMemoryByte(cpu, raw_address);
  uint8_t high_byte_value = YCPUReadRawMemoryByte(cpu, raw_address + 1);
  return (((uint16_t)high_byte_value) << 8) | (uint16_t)low_byte_value;
}

// Read a byte from memory to a YCPUOperandValue.
Y_MODULE_PRIVATE Y_HOT YCPUOperandValue
YCPUReadMemoryOperandByte(YCPUState* cpu, const YCPUOperandAddress* address) {
  YCPUAddBusCycles(cpu, 1);
  return YCPUReadRawMemoryByte(
      cpu, YCPUToRawAddress(cpu, address->register_index, address->offset));
}

// Read a word from memory to a YCPUOperandValue.
Y_MODULE_PRIVATE Y_HOT YCPUOperandValue
YCPUReadMemoryOperandWord(YCPUState* cpu, const YCPUOperandAddress* address) {
  YCPUAddBusCycles(cpu, 2);
  // The offset is 16 bits wide and wraps within the segment, so the high byte
  // of a word at offset 0xFFFF comes from offset 0 of the same segment rather
  // than from the paragraph above it.
  const uint8_t segment = address->register_index;
  uint8_t low_byte_value = YCPUReadRawMemoryByte(
      cpu, YCPUToRawAddress(cpu, segment, address->offset));
  uint8_t high_byte_value = YCPUReadRawMemoryByte(
      cpu, YCPUToRawAddress(cpu, segment, (uint16_t)(address->offset + 1)));
  return (YCPUOperandValue)((((uint16_t)high_byte_value) << 8) |
                            (uint16_t)low_byte_value);
}

// Read a memory operand of the given width to a YCPUOperandValue.
Y_FILE_PRIVATE YCPUOperandValue YCPUReadMemoryOperandValue(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUWidth width) {
  switch (width) {
    case Y_CPU_BYTE:
      return YCPUReadMemoryOperandByte(cpu, address);
    case Y_CPU_WORD:
      return YCPUReadMemoryOperandWord(cpu, address);
  }
  // Should never reach here, but return a default value to avoid warnings.
  return Y_CPU_UNREACHABLE_OPERAND_VALUE;
}

// Read a byte from a register to a YCPUOperandValue.
Y_MODULE_PRIVATE Y_HOT YCPUOperandValue
YCPUReadRegisterOperandByte(YCPUState* cpu, const YCPUOperandAddress* address) {
  // Truncated to a byte, which is the invariant every consumer widens by
  // doing nothing: AH and the high half of a word register live in the bits
  // this drops.
  return (uint8_t)(cpu->registers[address->register_index] >> address->offset);
}

// Read a word from a register to a YCPUOperandValue.
Y_MODULE_PRIVATE Y_HOT YCPUOperandValue
YCPUReadRegisterOperandWord(YCPUState* cpu, const YCPUOperandAddress* address) {
  return cpu->registers[address->register_index];
}

// Read a register operand of the given width to a YCPUOperandValue.
Y_FILE_PRIVATE YCPUOperandValue YCPUReadRegisterOperandValue(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUWidth width) {
  switch (width) {
    case Y_CPU_BYTE:
      return YCPUReadRegisterOperandByte(cpu, address);
    case Y_CPU_WORD:
      return YCPUReadRegisterOperandWord(cpu, address);
  }
  // Should never reach here, but return a default value to avoid warnings.
  return Y_CPU_UNREACHABLE_OPERAND_VALUE;
}

// Write a byte at an address the direct data window does not cover: into its
// frame if the address is paged and its page is in one and dirty, and
// otherwise through the host, which brings the page in and marks it dirty.
//
// The slow half of YCPUWriteRawMemoryByte(), out of line for the same reason
// as YCPUReadMemoryByteOutsideDirectWindow().
Y_FILE_PRIVATE Y_HOT Y_NOINLINE void YCPUWriteMemoryByteOutsideDirectWindow(
    YCPUState* cpu, uint32_t address, uint8_t value) {
  uint8_t* paged = YCPUGetPagedDataByte(
      cpu, address, Y_CPU_PAGED_DATA_RESIDENT | Y_CPU_PAGED_DATA_DIRTY);
  if (paged) {
    *paged = value;
    return;
  }
  if (cpu->config.write_memory_byte) {
    cpu->config.write_memory_byte(cpu, address, value);
  }
}

// Write a byte as uint8_t to memory.
Y_MODULE_PRIVATE void YCPUWriteRawMemoryByte(
    YCPUState* cpu, uint32_t address, uint8_t value) {
  // Every write the CPU makes comes through here - operands, stack pushes and
  // the interrupt vector alike - which is what lets a host report only the
  // writes it makes some other way.
  YCPUNotifyMemoryWrite(cpu, address);
  if (address < cpu->direct_data_window.end) {
    cpu->direct_data_window.data[address] = value;
    return;
  }
  YCPUWriteMemoryByteOutsideDirectWindow(cpu, address, value);
}

// Write a byte to memory.
Y_MODULE_PRIVATE Y_HOT void YCPUWriteMemoryOperandByte(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value) {
  YCPUAddBusCycles(cpu, 1);
  YCPUWriteRawMemoryByte(
      cpu, YCPUToRawAddress(cpu, address->register_index, address->offset),
      (uint8_t)value);
}

// Write a word to memory.
Y_MODULE_PRIVATE Y_HOT void YCPUWriteMemoryOperandWord(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value) {
  YCPUAddBusCycles(cpu, 2);
  // See YCPUReadMemoryOperandWord() for why the high byte's offset wraps within
  // the segment.
  const uint8_t segment = address->register_index;
  YCPUWriteRawMemoryByte(
      cpu, YCPUToRawAddress(cpu, segment, address->offset), (uint8_t)value);
  YCPUWriteRawMemoryByte(
      cpu, YCPUToRawAddress(cpu, segment, (uint16_t)(address->offset + 1)),
      (uint8_t)(value >> 8));
}

// Write a memory operand of the given width.
Y_FILE_PRIVATE void YCPUWriteMemoryOperand(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value,
    YCPUWidth width) {
  switch (width) {
    case Y_CPU_BYTE:
      YCPUWriteMemoryOperandByte(cpu, address, value);
      return;
    case Y_CPU_WORD:
      YCPUWriteMemoryOperandWord(cpu, address, value);
      return;
  }
  // Should never reach here. Writing nothing is the safe default.
}

// Write a byte to a register.
Y_MODULE_PRIVATE Y_HOT void YCPUWriteRegisterOperandByte(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value) {
  const uint16_t updated_byte = ((uint16_t)(uint8_t)value) << address->offset;
  const uint16_t other_byte = cpu->registers[address->register_index] &
                              (((uint16_t)0xFF) << (8 - address->offset));
  cpu->registers[address->register_index] = other_byte | updated_byte;
}

// Write a word to a register.
Y_MODULE_PRIVATE Y_HOT void YCPUWriteRegisterOperandWord(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value) {
  cpu->registers[address->register_index] = value;
}

// Write a register operand of the given width.
Y_FILE_PRIVATE void YCPUWriteRegisterOperand(
    YCPUState* cpu, const YCPUOperandAddress* address, YCPUOperandValue value,
    YCPUWidth width) {
  switch (width) {
    case Y_CPU_BYTE:
      YCPUWriteRegisterOperandByte(cpu, address, value);
      return;
    case Y_CPU_WORD:
      YCPUWriteRegisterOperandWord(cpu, address, value);
      return;
  }
  // Should never reach here. Writing nothing is the safe default.
}

// Add an 8-bit signed relative offset to a 16-bit unsigned base address.
Y_MODULE_PRIVATE uint16_t
YCPUAddSignedOffsetByte(uint16_t base, uint8_t raw_offset) {
  // Sign-extend the offset to 32 bits
  int32_t signed_offset = (int32_t)((int8_t)raw_offset);
  // Zero-extend base to 32 bits
  int32_t signed_base = (int32_t)base;
  // Add the two 32-bit signed values then truncate back down to 16-bit unsigned
  return (uint16_t)(signed_base + signed_offset);
}

// Add a 16-bit signed relative offset to a 16-bit unsigned base address.
Y_MODULE_PRIVATE uint16_t
YCPUAddSignedOffsetWord(uint16_t base, uint16_t raw_offset) {
  // Sign-extend the offset to 32 bits
  int32_t signed_offset = (int32_t)((int16_t)raw_offset);
  // Zero-extend base to 32 bits
  int32_t signed_base = (int32_t)base;
  // Add the two 32-bit signed values then truncate back down to 16-bit unsigned
  return (uint16_t)(signed_base + signed_offset);
}

// Get the register operand for a byte instruction based on the ModR/M byte's
// reg or R/M field.
Y_MODULE_PRIVATE Y_HOT YCPURegisterAddress
YCPUGetRegisterAddressByte(Y_UNUSED YCPUState* cpu, uint8_t reg_or_rm) {
  YCPURegisterAddress address;
  if (reg_or_rm < 4) {
    // AL, CL, DL, BL
    address.register_index = reg_or_rm;
    address.byte_offset = 0;
  } else {
    // AH, CH, DH, BH
    address.register_index = reg_or_rm - 4;
    address.byte_offset = 8;
  }
  return address;
}

// Get the register operand for a word instruction based on the ModR/M byte's
// reg or R/M field.
Y_MODULE_PRIVATE YCPURegisterAddress
YCPUGetRegisterAddressWord(Y_UNUSED YCPUState* cpu, uint8_t reg_or_rm) {
  const YCPURegisterAddress address = {
      .register_index = reg_or_rm, .byte_offset = 0};
  return address;
}

// Get the register operand of the given width from the ModR/M byte's reg or
// R/M field.
Y_FILE_PRIVATE YCPURegisterAddress
YCPUGetRegisterAddress(YCPUState* cpu, uint8_t reg_or_rm, YCPUWidth width) {
  switch (width) {
    case Y_CPU_BYTE:
      return YCPUGetRegisterAddressByte(cpu, reg_or_rm);
    case Y_CPU_WORD:
      return YCPUGetRegisterAddressWord(cpu, reg_or_rm);
  }
  // Should never reach here. AL is in range for both widths, so a caller that
  // somehow got here names a real register rather than running off the array.
  const YCPURegisterAddress fallback = {
      .register_index = Y_CPU_AX, .byte_offset = 0};
  return fallback;
}

// Replace a segment register index with whatever the instruction's segment
// override prefix names, if it carries one.
Y_MODULE_PRIVATE void YCPUApplySegmentOverride(
    const YCPUInstruction* instruction, uint8_t* segment_register_index) {
  if (instruction->segment_override != Y_CPU_NO_SEGMENT_OVERRIDE) {
    *segment_register_index = instruction->segment_override;
  }
}

// Compute the memory address for an instruction.
Y_MODULE_PRIVATE Y_HOT YCPUMemoryAddress YCPUGetMemoryOperandAddress(
    YCPUState* cpu, const YCPUInstruction* instruction) {
  YCPUMemoryAddress address;
  uint8_t mod = instruction->mod_rm.mod;
  uint8_t rm = instruction->mod_rm.rm;
  switch (rm) {
    case 0:  // [BX + SI]
      address.offset = cpu->registers[Y_CPU_BX] + cpu->registers[Y_CPU_SI];
      address.segment_register_index = Y_CPU_DS;
      break;
    case 1:  // [BX + DI]
      address.offset = cpu->registers[Y_CPU_BX] + cpu->registers[Y_CPU_DI];
      address.segment_register_index = Y_CPU_DS;
      break;
    case 2:  // [BP + SI]
      address.offset = cpu->registers[Y_CPU_BP] + cpu->registers[Y_CPU_SI];
      address.segment_register_index = Y_CPU_SS;
      break;
    case 3:  // [BP + DI]
      address.offset = cpu->registers[Y_CPU_BP] + cpu->registers[Y_CPU_DI];
      address.segment_register_index = Y_CPU_SS;
      break;
    case 4:  // [SI]
      address.offset = cpu->registers[Y_CPU_SI];
      address.segment_register_index = Y_CPU_DS;
      break;
    case 5:  // [DI]
      address.offset = cpu->registers[Y_CPU_DI];
      address.segment_register_index = Y_CPU_DS;
      break;
    case 6:
      if (mod == 0) {
        // Direct memory address with 16-bit displacement
        address.offset = 0;
        address.segment_register_index = Y_CPU_DS;
      } else {
        // [BP]
        address.offset = cpu->registers[Y_CPU_BP];
        address.segment_register_index = Y_CPU_SS;
      }
      break;
    case 7:  // [BX]
      address.offset = cpu->registers[Y_CPU_BX];
      address.segment_register_index = Y_CPU_DS;
      break;
    default:
      // Not possible as RM field is 3 bits (0-7).
      address.offset = 0xFFFF;
      address.segment_register_index = Y_CPU_DS;  // Invalid RM field
      break;
  }

  // Apply segment override prefixes if present
  YCPUApplySegmentOverride(instruction, &address.segment_register_index);

  // Add displacement if present
  switch (instruction->displacement_size) {
    case 1: {
      uint8_t raw_displacement = instruction->displacement[0];
      address.offset =
          YCPUAddSignedOffsetByte(address.offset, raw_displacement);
      break;
    }
    case 2: {
      // Concatenate the two displacement bytes as an unsigned 16-bit integer
      uint16_t raw_displacement =
          ((uint16_t)instruction->displacement[0]) |
          (((uint16_t)instruction->displacement[1]) << 8);
      address.offset =
          YCPUAddSignedOffsetWord(address.offset, raw_displacement);
      break;
    }
    default:
      // No displacement
      break;
  }

  return address;
}

// Get a register or memory operand address based on the ModR/M byte and
// displacement, without reading the value currently there.
//
// An instruction that overwrites its destination completely - as opposed to a
// read-modify-write - resolves the address with this and stores through
// YCPUWriteOperandAddress(). Reading the destination on the way is not free: it
// charges the bus cycles of a memory access the 8088 never performs.
//
// Always inlined. With more than one caller, -Os and -O2 emit it out of line,
// which puts a call and its register shuffling on the hottest path in the
// emulator - 3.6% at -O2.
Y_MODULE_PRIVATE Y_ALWAYS_INLINE YCPUOperandAddress
YCPUGetRegisterOrMemoryOperandAddress(const YCPUInstructionContext* ctx) {
  YCPUState* cpu = ctx->cpu;
  const YCPUInstruction* instruction = ctx->instruction;
  YCPUOperandAddress address;
  uint8_t mod = instruction->mod_rm.mod;
  uint8_t rm = instruction->mod_rm.rm;
  if (mod == 3) {
    // Register operand
    const YCPURegisterAddress register_address =
        YCPUGetRegisterAddress(cpu, rm, ctx->metadata->width);
    address.type = Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER;
    address.register_index = register_address.register_index;
    address.offset = register_address.byte_offset;
  } else {
    // Memory operand
    const YCPUMemoryAddress memory_address =
        YCPUGetMemoryOperandAddress(cpu, instruction);
    address.type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY;
    address.register_index = (uint8_t)memory_address.segment_register_index;
    address.offset = memory_address.offset;
  }
  return address;
}

// Read an 8-bit immediate value.
Y_MODULE_PRIVATE Y_HOT YCPUOperandValue
YCPUReadImmediateOperandByte(const YCPUInstruction* instruction) {
  return instruction->immediate[0];
}

// Read a 16-bit immediate value.
Y_MODULE_PRIVATE Y_HOT YCPUOperandValue
YCPUReadImmediateOperandWord(const YCPUInstruction* instruction) {
  return (YCPUOperandValue)(((uint16_t)instruction->immediate[0]) |
                            (((uint16_t)instruction->immediate[1]) << 8));
}

// Read an immediate value of the given width.
Y_FILE_PRIVATE YCPUOperandValue
YCPUReadImmediateOperand(const YCPUInstruction* instruction, YCPUWidth width) {
  switch (width) {
    case Y_CPU_BYTE:
      return YCPUReadImmediateOperandByte(instruction);
    case Y_CPU_WORD:
      return YCPUReadImmediateOperandWord(instruction);
  }
  // Should never reach here, but return a default value to avoid warnings.
  return Y_CPU_UNREACHABLE_OPERAND_VALUE;
}

// Read a value from an operand address.
//
// Always inlined. Left to itself GCC emits this out of line, in flash, and
// puts a veneer and an XIP fetch on every operand read - 5.4% at -O3.
Y_MODULE_PRIVATE Y_ALWAYS_INLINE YCPUOperandValue YCPUReadOperandValue(
    const YCPUInstructionContext* ctx, const YCPUOperandAddress* address) {
  // Not a switch, unlike the width dispatch it calls into.
  // YCPUOperandAddressType is a plain enum field rather than a bitfield, so
  // most of the values it can hold are ones the enum does not name and the
  // compiler cannot prove a switch's default arm unreachable: the memory path,
  // which is the common one, ends up paying an extra compare and branch for a
  // case that cannot happen. YCPUWidth is a one-bit bitfield, which has no
  // unnamed value to represent, so there the default arm disappears from the
  // output entirely. Switching this one too costs 1.06% at -O3.
  //
  // Narrowing type to a one-bit bitfield to make the switch free was measured
  // and is worse still - YCPUGetRegisterOrMemoryOperandAddress() writes this
  // field for every operand, and a write to a bitfield is a read-modify-write.
  // Widening YCPUOpcodeMetadata.width to a byte would likewise put the width
  // switches in this same position; see core/src/cpu/AGENTS.md before doing
  // either.
  const YCPUWidth width = ctx->metadata->width;
  if (address->type == Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER) {
    return YCPUReadRegisterOperandValue(ctx->cpu, address, width);
  }
  return YCPUReadMemoryOperandValue(ctx->cpu, address, width);
}

// Get a register or memory operand for an instruction based on the ModR/M
// byte and displacement.
Y_MODULE_PRIVATE Y_HOT void YCPUReadRegisterOrMemoryOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand) {
  operand->address = YCPUGetRegisterOrMemoryOperandAddress(ctx);
  operand->value = YCPUReadOperandValue(ctx, &operand->address);
}

// Get a register operand for an instruction.
Y_MODULE_PRIVATE Y_HOT void YCPUReadRegisterOperandForRegisterIndex(
    const YCPUInstructionContext* ctx, YCPURegisterIndex register_index,
    YCPUOperand* operand) {
  const YCPUWidth width = ctx->metadata->width;
  // Do not fold these into an initializer: C would zero the whole struct
  // before a single field of it is written.
  operand->address.type = Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER;
  const YCPURegisterAddress register_address =
      YCPUGetRegisterAddress(ctx->cpu, register_index, width);
  operand->address.register_index = register_address.register_index;
  operand->address.offset = register_address.byte_offset;
  operand->value = YCPUReadOperandValue(ctx, &operand->address);
}

// Get a register operand for an instruction from the REG field of the Mod/RM
// byte.
Y_MODULE_PRIVATE void YCPUReadRegisterOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand) {
  YCPUReadRegisterOperandForRegisterIndex(
      ctx, (YCPURegisterIndex)ctx->instruction->mod_rm.reg, operand);
}

// Get a segment register operand for an instruction from the REG field of the
// Mod/RM byte.
Y_MODULE_PRIVATE void YCPUReadSegmentRegisterOperand(
    const YCPUInstructionContext* ctx, YCPUOperand* operand) {
  // The segment register field is only two bits wide. The 8086/8088 does not
  // decode the third bit at all, so REG 4 through 7 name the same four
  // registers over again - which is what makes 0x8C and 0x8E accept every REG
  // value. Masking it also keeps the index inside the register array, which
  // REG 4 and above would otherwise run past the end of.
  YCPUReadRegisterOperandForRegisterIndex(
      ctx,
      (YCPURegisterIndex)(Y_CPU_ES + (ctx->instruction->mod_rm.reg & 0x03)),
      operand);
}

// Write a value to a register or memory operand address.
Y_MODULE_PRIVATE Y_HOT void YCPUWriteOperandAddress(
    const YCPUInstructionContext* ctx, const YCPUOperandAddress* address,
    uint32_t raw_value) {
  const YCPUWidth width = ctx->metadata->width;
  const YCPUOperandValue value = YCPUToOperandValue(width, raw_value);
  // See YCPUReadOperandValue() for why this one is not a switch.
  if (address->type == Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER) {
    YCPUWriteRegisterOperand(ctx->cpu, address, value, width);
  } else {
    YCPUWriteMemoryOperand(ctx->cpu, address, value, width);
  }
}

// Write a value to a register or memory operand.
Y_MODULE_PRIVATE void YCPUWriteOperand(
    const YCPUInstructionContext* ctx, const YCPUOperand* operand,
    uint32_t raw_value) {
  YCPUWriteOperandAddress(ctx, &operand->address, raw_value);
}

// Read an immediate value from the instruction.
Y_MODULE_PRIVATE YCPUOperandValue
YCPUReadImmediate(const YCPUInstructionContext* ctx) {
  return YCPUReadImmediateOperand(ctx->instruction, ctx->metadata->width);
}


// ==============================================================================
// src/cpu/operands.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions.h start
// ==============================================================================

#line 1 "./src/cpu/instructions.h"
#ifndef YAX86_CPU_INSTRUCTIONS_H
#define YAX86_CPU_INSTRUCTIONS_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Helpers - instructions_helpers.h
// ============================================================================

// Set common CPU flags after an instruction. This includes:
// - Zero flag (ZF)
// - Sign flag (SF)
// - Parity Flag (PF)
Y_MODULE_PRIVATE void YCPUSetCommonFlagsAfterInstruction(
    const YCPUInstructionContext* ctx, uint32_t result);

// Apply the bits that are not flags to a value on its way into the flags
// register. POPF, IRET and SAHF all load flags from somewhere the guest
// controls, and none of them can change these bits.
Y_MODULE_PRIVATE uint16_t YCPUToFlagsRegisterValue(uint16_t value);

// Push a value the caller has already worked out onto the stack - a return
// address, the flags, a segment register. Use this wherever the value does not
// depend on the stack pointer.
Y_MODULE_PRIVATE void YCPUPushValue(YCPUState* cpu, YCPUOperandValue value);
// Push a PUSH instruction's source operand onto the stack. The operand is
// taken as of after the stack pointer has moved, rather than as of the start
// of the instruction, which is what makes PUSH SP store the decremented value.
Y_MODULE_PRIVATE void YCPUPushSourceOperand(
    YCPUState* cpu, const YCPUOperand* src);
// Pop a value from the stack.
Y_MODULE_PRIVATE YCPUOperandValue YCPUPop(YCPUState* cpu);

// Dummy instruction for unsupported opcodes.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteNoOp(const YCPUInstructionContext* ctx);
// Handler for the opcode bytes that are prefixes rather than instructions.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInvalidOpcode(const YCPUInstructionContext* ctx);

// ============================================================================
// Opcode table - YCPUOpcodeTable.h
// ============================================================================

// Global opcode metadata lookup table.
#ifndef YAX86_IMPLEMENTATION
extern const YCPUOpcodeMetadata YCPUOpcodeTable[256];
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Move instructions - instructions_mov.h
// ============================================================================

// MOV r/m8, r8
// MOV r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx);
// MOV r8, r/m8
// MOV r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx);
// MOV r/m16, sreg
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveSegmentRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx);
// MOV sreg, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveRegisterOrMemoryToSegmentRegister(
    const YCPUInstructionContext* ctx);
// MOV AX/CX/DX/BX/SP/BP/SI/DI, imm16
// MOV AH/AL/CH/CL/DH/DL/BH/BL, imm8
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveImmediateToRegister(const YCPUInstructionContext* ctx);
// MOV AL, moffs16
// MOV AX, moffs16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveMemoryOffsetToALOrAX(const YCPUInstructionContext* ctx);
// MOV moffs16, AL
// MOV moffs16, AX
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveALOrAXToMemoryOffset(const YCPUInstructionContext* ctx);
// MOV r/m8, imm8
// MOV r/m16, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveImmediateToRegisterOrMemory(const YCPUInstructionContext* ctx);
// XCHG AX, AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteExchangeRegister(const YCPUInstructionContext* ctx);
// XCHG r/m8, r8
// XCHG r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteExchangeRegisterOrMemory(const YCPUInstructionContext* ctx);
// XLAT
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteTranslateByte(const YCPUInstructionContext* ctx);

// ============================================================================
// LEA instructions - instructions_lea.h
// ============================================================================

// LEA r16, m
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoadEffectiveAddress(const YCPUInstructionContext* ctx);
// LES r16, m
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoadESWithPointer(const YCPUInstructionContext* ctx);
// LDS r16, m
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoadDSWithPointer(const YCPUInstructionContext* ctx);

// ============================================================================
// Addition instructions - instructions_add.h
// ============================================================================

// Common logic for ADD instructions
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteAdd(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);
// Common logic for INC instructions
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInc(const YCPUInstructionContext* ctx, YCPUOperand* dest);
// Common logic for ADC instructions
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteAddWithCarry(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);

// ADD r/m8, r8
// ADD r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx);
// ADD r8, r/m8
// ADD r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx);
// ADD AL, imm8
// ADD AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddImmediateToALOrAX(const YCPUInstructionContext* ctx);
// ADC r/m8, r8
// ADC r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddRegisterToRegisterOrMemoryWithCarry(
    const YCPUInstructionContext* ctx);
// ADC r8, r/m8
// ADC r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddRegisterOrMemoryToRegisterWithCarry(
    const YCPUInstructionContext* ctx);
// ADC AL, imm8
// ADC AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddImmediateToALOrAXWithCarry(const YCPUInstructionContext* ctx);
// INC AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteIncRegister(const YCPUInstructionContext* ctx);

// ============================================================================
// Subtraction instructions - instructions_sub.c
// ============================================================================

// Set CPU flags after a SUB, SBB, CMP, or NEG instruction.
Y_MODULE_PRIVATE void YCPUSetFlagsAfterSub(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_borrow);

// Common logic for SUB instructions
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteSub(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);
// Common logic for SBB instructions
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteSubWithBorrow(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);
// Common logic for DEC instructions
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDec(const YCPUInstructionContext* ctx, YCPUOperand* dest);

// SUB r/m8, r8
// SUB r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubRegisterFromRegisterOrMemory(const YCPUInstructionContext* ctx);
// SUB r8, r/m8
// SUB r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubRegisterOrMemoryFromRegister(const YCPUInstructionContext* ctx);
// SUB AL, imm8
// SUB AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubImmediateFromALOrAX(const YCPUInstructionContext* ctx);
// SBB r/m8, r8
// SBB r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubRegisterFromRegisterOrMemoryWithBorrow(
    const YCPUInstructionContext* ctx);
// SBB r8, r/m8
// SBB r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubRegisterOrMemoryFromRegisterWithBorrow(
    const YCPUInstructionContext* ctx);
// SBB AL, imm8
// SBB AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubImmediateFromALOrAXWithBorrow(const YCPUInstructionContext* ctx);
// DEC AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDecRegister(const YCPUInstructionContext* ctx);

// ============================================================================
// Sign extension instructions - instructions_sign_ext.c
// ============================================================================

// CBW
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCbw(const YCPUInstructionContext* ctx);
// CWD
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCwd(const YCPUInstructionContext* ctx);

// ============================================================================
// CMP instructions - instructions_cmp.c
// ============================================================================

// Common logic for CMP instructions. Computes dest - src and sets flags.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteCmp(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);

// CMP r/m8, r8
// CMP r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCmpRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx);
// CMP r8, r/m8
// CMP r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCmpRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx);
// CMP AL, imm8
// CMP AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCmpImmediateToALOrAX(const YCPUInstructionContext* ctx);

// ============================================================================
// Boolean instructions - instructions_bool.c
// ============================================================================

Y_MODULE_PRIVATE void YCPUSetFlagsAfterBooleanInstruction(
    const YCPUInstructionContext* ctx, uint32_t result);
// Common logic for AND instructions.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteBooleanAnd(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);
// Common logic for OR instructions.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteBooleanOr(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);
// Common logic for XOR instructions.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteBooleanXor(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);
// Common logic for TEST instructions.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteTest(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value);

// AND r/m8, r8
// AND r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanAndRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx);
// AND r8, r/m8
// AND r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanAndRegisterOrMemoryToRegister(
    const YCPUInstructionContext* ctx);
// AND AL, imm8
// AND AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanAndImmediateToALOrAX(const YCPUInstructionContext* ctx);
// OR r/m8, r8
// OR r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanOrRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx);
// OR r8, r/m8
// OR r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanOrRegisterOrMemoryToRegister(
    const YCPUInstructionContext* ctx);
// OR AL, imm8
// OR AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanOrImmediateToALOrAX(const YCPUInstructionContext* ctx);
// XOR r/m8, r8
// XOR r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanXorRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx);
// XOR r8, r/m8
// XOR r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanXorRegisterOrMemoryToRegister(
    const YCPUInstructionContext* ctx);
// XOR AL, imm8
// XOR AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanXorImmediateToALOrAX(const YCPUInstructionContext* ctx);
// TEST r/m8, r8
// TEST r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteTestRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx);
// TEST AL, imm8
// TEST AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteTestImmediateToALOrAX(const YCPUInstructionContext* ctx);

// ============================================================================
// Control flow instructions - instructions_ctrl_flow.c
// ============================================================================

// Common logic for far jumps.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteFarJump(
    const YCPUInstructionContext* ctx, YCPUOperandValue segment,
    YCPUOperandValue offset);
// Common logic for far calls.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteFarCall(
    const YCPUInstructionContext* ctx, YCPUOperandValue segment,
    YCPUOperandValue offset);
// Common logic for returning from an interrupt.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteReturnFromInterrupt(YCPUState* cpu);

// JMP rel8
// JMP rel16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteShortOrNearJump(const YCPUInstructionContext* ctx);
// JMP ptr16:16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDirectFarJump(const YCPUInstructionContext* ctx);
// Unsigned conditional jumps.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteUnsignedConditionalJump(const YCPUInstructionContext* ctx);
// JL/JGNE and JNL/JGE
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSignedConditionalJumpJLOrJNL(const YCPUInstructionContext* ctx);
// JLE/JG and JNLE/JG
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSignedConditionalJumpJLEOrJNLE(const YCPUInstructionContext* ctx);
// LOOP rel8
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoop(const YCPUInstructionContext* ctx);
// LOOPZ rel8
// LOOPNZ rel8
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoopZOrNZ(const YCPUInstructionContext* ctx);
// JCXZ rel8
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteJumpIfCXIsZero(const YCPUInstructionContext* ctx);
// CALL rel16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDirectNearCall(const YCPUInstructionContext* ctx);
// CALL ptr16:16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDirectFarCall(const YCPUInstructionContext* ctx);
// RET
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteNearReturn(const YCPUInstructionContext* ctx);
// RET imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteNearReturnAndPop(const YCPUInstructionContext* ctx);
// RETF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteFarReturn(const YCPUInstructionContext* ctx);
// RETF imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteFarReturnAndPop(const YCPUInstructionContext* ctx);
// IRET
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteIret(const YCPUInstructionContext* ctx);
// INT 3
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInt3(const YCPUInstructionContext* ctx);
// INTO
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInto(const YCPUInstructionContext* ctx);
// INT n
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteIntN(const YCPUInstructionContext* ctx);
// HLT
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteHlt(const YCPUInstructionContext* ctx);

// ============================================================================
// Stack instructions - instructions_stack.c
// ============================================================================

// PUSH AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePushRegister(const YCPUInstructionContext* ctx);
// POP AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePopRegister(const YCPUInstructionContext* ctx);
// PUSH ES/CS/SS/DS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePushSegmentRegister(const YCPUInstructionContext* ctx);
// POP ES/CS/SS/DS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePopSegmentRegister(const YCPUInstructionContext* ctx);
// PUSHF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePushFlags(const YCPUInstructionContext* ctx);
// POPF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePopFlags(const YCPUInstructionContext* ctx);
// POP r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePopRegisterOrMemory(const YCPUInstructionContext* ctx);
// LAHF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoadAHFromFlags(const YCPUInstructionContext* ctx);
// SAHF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteStoreAHToFlags(const YCPUInstructionContext* ctx);

// ============================================================================
// Flag manipulation instructions - instructions_flags.c
// ============================================================================

// CLC, STC, CLI, STI, CLD, STD
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteClearOrSetFlag(const YCPUInstructionContext* ctx);
// CMC
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteComplementCarryFlag(const YCPUInstructionContext* ctx);
// SALC
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSetALFromCarry(const YCPUInstructionContext* ctx);

// ============================================================================
// IN and OUT instructions - instructions_io.c
// ============================================================================

// IN AL, imm8
// IN AX, imm8
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInImmediate(const YCPUInstructionContext* ctx);
// IN AL, DX
// IN AX, DX
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInDX(const YCPUInstructionContext* ctx);
// OUT imm8, AL
// OUT imm8, AX
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteOutImmediate(const YCPUInstructionContext* ctx);
// OUT DX, AL
// OUT DX, AX
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteOutDX(const YCPUInstructionContext* ctx);

// ============================================================================
// String instructions - instructions_string.c
// ============================================================================

// MOVS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMovs(const YCPUInstructionContext* ctx);
// STOS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteStos(const YCPUInstructionContext* ctx);
// LODS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLods(const YCPUInstructionContext* ctx);
// SCAS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteScas(const YCPUInstructionContext* ctx);
// CMPS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCmps(const YCPUInstructionContext* ctx);

// ============================================================================
// BCD and ASCII arithmetic instructions - instructions_bcd_ascii.c
// ============================================================================

// AAA
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAaa(const YCPUInstructionContext* ctx);
// AAS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAas(const YCPUInstructionContext* ctx);
// AAM
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAam(const YCPUInstructionContext* ctx);
// AAD
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAad(const YCPUInstructionContext* ctx);
// DAA
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDaa(const YCPUInstructionContext* ctx);
// DAS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDas(const YCPUInstructionContext* ctx);

// ============================================================================
// Group 1 instructions - instructions_group_1.c
// ============================================================================

// Group 1 instruction handler.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup1Instruction(const YCPUInstructionContext* ctx);

// Group 1 instruction handler, but sign-extends the 8-bit immediate value.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup1InstructionWithSignExtension(
    const YCPUInstructionContext* ctx);

// ============================================================================
// Group 2 instructions - instructions_group_2.c
// ============================================================================

// Group 2 shift / rotate by 1.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup2ShiftOrRotateBy1Instruction(const YCPUInstructionContext* ctx);
// Group 2 shift / rotate by CL.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup2ShiftOrRotateByCLInstruction(
    const YCPUInstructionContext* ctx);

// ============================================================================
// Group 3 instructions - instructions_group_3.c
// ============================================================================

// Group 3 instruction handler.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup3Instruction(const YCPUInstructionContext* ctx);

// ============================================================================
// Group 4 instructions - instructions_group_4.c
// ============================================================================

// Group 4 instruction handler.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup4Instruction(const YCPUInstructionContext* ctx);

// ============================================================================
// Group 5 instructions - instructions_group_5.c
// ============================================================================

// Group 5 instruction handler.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup5Instruction(const YCPUInstructionContext* ctx);

#endif  // YAX86_CPU_INSTRUCTIONS_H


// ==============================================================================
// src/cpu/instructions.h end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_helpers.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_helpers.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// Set common CPU flags after an instruction. This includes:
// - Zero flag (ZF)
// - Sign flag (SF)
// - Parity Flag (PF)
Y_MODULE_PRIVATE Y_HOT void YCPUSetCommonFlagsAfterInstruction(
    const YCPUInstructionContext* ctx, uint32_t result) {
  YCPUWidth width = ctx->metadata->width;
  result &= YCPUMaxValue[width];
  // Zero flag (ZF)
  YCPUSetFlag(ctx->cpu, Y_CPU_ZF, result == 0);
  // Sign flag (SF)
  YCPUSetFlag(ctx->cpu, Y_CPU_SF, result & YCPUSignBit[width]);
  // Parity flag (PF)
  // Set if the number of set bits in the least significant byte is even
  uint8_t parity = result & 0xFF;  // Check only the low byte for parity
  parity ^= parity >> 4;
  parity ^= parity >> 2;
  parity ^= parity >> 1;
  YCPUSetFlag(ctx->cpu, Y_CPU_PF, (parity & 1) == 0);
}

Y_MODULE_PRIVATE uint16_t YCPUToFlagsRegisterValue(uint16_t value) {
  return (value | (uint16_t)Y_CPU_FLAGS_ALWAYS_SET) &
         ~(uint16_t)Y_CPU_FLAGS_ALWAYS_CLEAR;
}

// Write a word where the stack pointer already points. The caller is
// responsible for having made room for it.
Y_FILE_PRIVATE void YCPUWriteToStackTop(
    YCPUState* cpu, YCPUOperandValue value) {
  YCPUOperandAddress address = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
      .register_index = Y_CPU_SS,
      .offset = cpu->registers[Y_CPU_SP],
  };
  YCPUWriteMemoryOperandWord(cpu, &address, value);
}

Y_MODULE_PRIVATE void YCPUPushValue(YCPUState* cpu, YCPUOperandValue value) {
  cpu->registers[Y_CPU_SP] -= 2;
  YCPUWriteToStackTop(cpu, value);
}

Y_MODULE_PRIVATE void YCPUPushSourceOperand(
    YCPUState* cpu, const YCPUOperand* src) {
  cpu->registers[Y_CPU_SP] -= 2;
  // The 8086/8088 moves the stack pointer before it reads the source, so
  // PUSH SP stores the value SP has after the decrement rather than the one it
  // had on entry. SP is the only source that can tell the difference. The
  // 80286 and later store the entry value instead.
  const bool source_is_stack_pointer =
      src->address.type == Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER &&
      src->address.register_index == Y_CPU_SP;
  const YCPUOperandValue value =
      source_is_stack_pointer ? cpu->registers[Y_CPU_SP] : src->value;
  YCPUWriteToStackTop(cpu, value);
}

Y_MODULE_PRIVATE YCPUOperandValue YCPUPop(YCPUState* cpu) {
  YCPUOperandAddress address = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
      .register_index = Y_CPU_SS,
      .offset = cpu->registers[Y_CPU_SP],
  };
  YCPUOperandValue value = YCPUReadMemoryOperandWord(cpu, &address);
  cpu->registers[Y_CPU_SP] += 2;
  return value;
}

// Dummy instruction for unsupported opcodes.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteNoOp(Y_UNUSED const YCPUInstructionContext* ctx) {
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Handler for the eight opcode bytes that are prefixes rather than
// instructions. The prefix decoder consumes those bytes before an opcode is
// looked up, so a decoded instruction never names one and this is unreachable
// from YCPUTick(). It exists so that every entry in the opcode table is
// callable, which is what lets the tick path dispatch without first checking
// for a null handler.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInvalidOpcode(Y_UNUSED const YCPUInstructionContext* ctx) {
  return Y_CPU_INSTRUCTION_INVALID;
}


// ==============================================================================
// src/cpu/instructions_helpers.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_mov.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_mov.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// MOV instructions
// ============================================================================

// MOV r/m8, r8
// MOV r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperandAddress dest = YCPUGetRegisterOrMemoryOperandAddress(ctx);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  YCPUWriteOperandAddress(ctx, &dest, YCPUFromOperand(&src));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV r8, r/m8
// MOV r16, r/m16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteMoveRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperand(&src));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV r/m16, sreg
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveSegmentRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx) {
  YCPUOperandAddress dest = YCPUGetRegisterOrMemoryOperandAddress(ctx);
  YCPUOperand src;
  YCPUReadSegmentRegisterOperand(ctx, &src);
  YCPUWriteOperandAddress(ctx, &dest, YCPUFromOperand(&src));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV sreg, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveRegisterOrMemoryToSegmentRegister(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadSegmentRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperand(&src));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV AX/CX/DX/BX/SP/BP/SI/DI, imm16
// MOV AH/AL/CH/CL/DH/DL/BH/BL, imm8
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteMoveImmediateToRegister(const YCPUInstructionContext* ctx) {
  static const uint8_t register_index_opcode_base[Y_CPU_NUM_WIDTHS] = {
      0xB0,  // Y_CPU_BYTE
      0xB8,  // Y_CPU_WORD
  };
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode -
                          register_index_opcode_base[ctx->metadata->width]);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperandValue(src_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV AL, moffs16
// MOV AX, moffs16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteMoveMemoryOffsetToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  // Offset is always 16 bits, even though the data width of the operation may
  // be 8 bits.
  YCPUOperandValue src_offset_value =
      YCPUReadImmediateOperandWord(ctx->instruction);
  // The source is DS:offset by default, but can be overridden by a segment
  // override prefix.
  YCPUOperandAddress src_address = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
      .register_index = Y_CPU_DS,
      .offset = (uint16_t)YCPUFromOperandValue(src_offset_value),
  };
  YCPUApplySegmentOverride(ctx->instruction, &src_address.register_index);
  YCPUOperandValue src_value = YCPUReadOperandValue(ctx, &src_address);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperandValue(src_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV moffs16, AL
// MOV moffs16, AX
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteMoveALOrAXToMemoryOffset(const YCPUInstructionContext* ctx) {
  YCPUOperand src;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &src);
  // Offset is always 16 bits, even though the data width of the operation may
  // be 8 bits.
  YCPUOperandValue dest_offset_value =
      YCPUReadImmediateOperandWord(ctx->instruction);
  // The destination is DS:offset by default, but can be overridden by a segment
  // override prefix.
  YCPUOperandAddress dest_address = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
      .register_index = Y_CPU_DS,
      .offset = (uint16_t)YCPUFromOperandValue(dest_offset_value),
  };
  YCPUApplySegmentOverride(ctx->instruction, &dest_address.register_index);
  YCPUWriteOperandAddress(ctx, &dest_address, YCPUFromOperand(&src));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MOV r/m8, imm8
// MOV r/m16, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteMoveImmediateToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperandAddress dest = YCPUGetRegisterOrMemoryOperandAddress(ctx);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  YCPUWriteOperandAddress(ctx, &dest, YCPUFromOperandValue(src_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ============================================================================
// XCHG instructions
// ============================================================================

// XCHG AX, AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteExchangeRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode - 0x90);
  if (register_index == Y_CPU_AX) {
    // No-op
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  YCPUOperand src;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &src);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  uint32_t temp = YCPUFromOperand(&dest);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperand(&src));
  YCPUWriteOperand(ctx, &src, temp);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// XCHG r/m8, r8
// XCHG r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteExchangeRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  uint32_t temp = YCPUFromOperand(&dest);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperand(&src));
  YCPUWriteOperand(ctx, &src, temp);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ============================================================================
// XLAT
// ============================================================================

// XLAT
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteTranslateByte(const YCPUInstructionContext* ctx) {
  // Read the AL register
  YCPUOperand al;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &al);
  // The translation table is at DS:BX by default, but can be overridden by a
  // segment override prefix.
  YCPUOperandAddress src_address = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
      .register_index = Y_CPU_DS,
      .offset =
          (uint16_t)(ctx->cpu->registers[Y_CPU_BX] + YCPUFromOperand(&al)),
  };
  YCPUApplySegmentOverride(ctx->instruction, &src_address.register_index);
  YCPUOperandValue src_value =
      YCPUReadMemoryOperandByte(ctx->cpu, &src_address);
  YCPUWriteOperandAddress(ctx, &al.address, YCPUFromOperandValue(src_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}


// ==============================================================================
// src/cpu/instructions_mov.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_lea.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_lea.c"
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
Y_MODULE_PRIVATE YCPUInstructionResult
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
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ============================================================================
// LES and LDS instructions
// ============================================================================

// Common logic for LES and LDS instructions.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteLoadSegmentWithPointer(
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
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
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
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// LES r16, m
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoadESWithPointer(const YCPUInstructionContext* ctx) {
  return YCPUExecuteLoadSegmentWithPointer(ctx, Y_CPU_ES);
}

// LDS r16, m
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoadDSWithPointer(const YCPUInstructionContext* ctx) {
  return YCPUExecuteLoadSegmentWithPointer(ctx, Y_CPU_DS);
}


// ==============================================================================
// src/cpu/instructions_lea.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_add.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_add.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// ADD, ADC, and INC instructions
// ============================================================================

// Set CPU flags after an INC instruction.
// Other than common flags, the INC instruction sets the following flags:
// - Overflow Flag (OF) - Set when result has wrong sign
// - Auxiliary Carry Flag (AF) - carry from bit 3 to bit 4
Y_FILE_PRIVATE void YCPUSetFlagsAfterInc(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_carry) {
  YCPUSetCommonFlagsAfterInstruction(ctx, result);

  // Overflow Flag (OF) Set when result has wrong sign (both operands have same
  // sign but result has different sign)
  uint32_t sign_bit = YCPUSignBit[ctx->metadata->width];
  bool op1_sign = (op1 & sign_bit) != 0;
  bool op2_sign = (op2 & sign_bit) != 0;
  bool result_sign = (result & sign_bit) != 0;
  YCPUSetFlag(
      ctx->cpu, Y_CPU_OF, (op1_sign == op2_sign) && (result_sign != op1_sign));

  // Auxiliary Carry Flag (AF) - carry from bit 3 to bit 4
  YCPUSetFlag(
      ctx->cpu, Y_CPU_AF,
      ((op1 & 0xF) + (op2 & 0xF) + (did_carry ? 1 : 0)) > 0xF);
}

// Set CPU flags after an ADD or ADC instruction.
// Other than the flags set by the INC instruction, the ADD instruction sets the
// following flags:
// - Carry Flag (CF) - Set when result overflows the maximum width
Y_FILE_PRIVATE void YCPUSetFlagsAfterAdd(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_carry) {
  YCPUSetFlagsAfterInc(ctx, op1, op2, result, did_carry);
  // Carry Flag (CF)
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, result > YCPUMaxValue[ctx->metadata->width]);
}

// Common signature of YCPUSetFlagsAfterAdd and YCPUSetFlagsAfterInc.
typedef void (*YCPUSetFlagsAfterAddFn)(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_carry);

// Common logic for ADD, ADC, and INC instructions.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteAddCommon(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value, bool carry,
    YCPUSetFlagsAfterAddFn set_flags_after_fn) {
  uint32_t raw_dest_value = YCPUFromOperand(dest);
  uint32_t raw_src_value = YCPUFromOperandValue(src_value);
  bool should_carry = carry && YCPUGetFlag(ctx->cpu, Y_CPU_CF);
  uint32_t result = raw_dest_value + raw_src_value + (should_carry ? 1 : 0);
  YCPUWriteOperand(ctx, dest, result);
  (*set_flags_after_fn)(
      ctx, raw_dest_value, raw_src_value, result, should_carry);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Common logic for ADD instructions
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteAdd(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  return YCPUExecuteAddCommon(
      ctx, dest, src_value, /* carry */ false, YCPUSetFlagsAfterAdd);
}

// ADD r/m8, r8
// ADD r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteAdd(ctx, &dest, src.value);
}

// ADD r8, r/m8
// ADD r16, r/m16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteAddRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteAdd(ctx, &dest, src.value);
}

// ADD AL, imm8
// ADD AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteAdd(ctx, &dest, src_value);
}

// Common logic for ADC instructions
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult YCPUExecuteAddWithCarry(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  return YCPUExecuteAddCommon(
      ctx, dest, src_value, /* carry */ true, YCPUSetFlagsAfterAdd);
}

// ADC r/m8, r8
// ADC r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddRegisterToRegisterOrMemoryWithCarry(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteAddWithCarry(ctx, &dest, src.value);
}
// ADC r8, r/m8
// ADC r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddRegisterOrMemoryToRegisterWithCarry(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteAddWithCarry(ctx, &dest, src.value);
}

// ADC AL, imm8
// ADC AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAddImmediateToALOrAXWithCarry(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteAddWithCarry(ctx, &dest, src_value);
}

// Common logic for INC instructions
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInc(const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUOperandValue src_value = 1;
  return YCPUExecuteAddCommon(
      ctx, dest, src_value, /* carry */ false, YCPUSetFlagsAfterInc);
}

// INC AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteIncRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode - 0x40);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &dest);
  return YCPUExecuteInc(ctx, &dest);
}


// ==============================================================================
// src/cpu/instructions_add.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_sub.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_sub.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// SUB, SBB, and DEC instructions
// ============================================================================

// Set CPU flags after a DEC or SUB/SBB operation (base function).
// This function sets ZF, SF, PF, OF, AF. It does NOT affect CF.
// - OF is for the full operation op1 - (op2 + did_borrow).
// - AF is for the full operation op1 - (op2 + did_borrow).
Y_FILE_PRIVATE void YCPUSetFlagsAfterDec(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_borrow) {
  YCPUSetCommonFlagsAfterInstruction(ctx, result);

  uint32_t sign_bit = YCPUSignBit[ctx->metadata->width];
  uint32_t max_val = YCPUMaxValue[ctx->metadata->width];

  // Overflow Flag (OF)
  // A subtraction overflows when the operands have different signs and the
  // result takes the sign of the subtrahend rather than the minuend. The
  // borrow does not enter into it: it shifts the result by one, and the result
  // is already what the test looks at. Folding the borrow into the subtrahend
  // instead would change that operand's sign whenever it is 0x7F or 0x7FFF,
  // and give the wrong answer for exactly those values.
  bool op1_sign = (op1 & sign_bit) != 0;
  bool op2_sign = ((op2 & max_val) & sign_bit) != 0;
  bool result_sign = (result & sign_bit) != 0;
  YCPUSetFlag(
      ctx->cpu, Y_CPU_OF, (op1_sign != op2_sign) && (result_sign != op1_sign));

  // Auxiliary Carry Flag (AF) - borrow from bit 3 to bit 4
  YCPUSetFlag(
      ctx->cpu, Y_CPU_AF, (op1 & 0xF) < ((op2 & 0xF) + (did_borrow ? 1 : 0)));
}

// Set CPU flags after a SUB, SBB, CMP or NEG instruction.
// This calls YCPUSetFlagsAfterDec and then sets the Carry Flag (CF).
Y_MODULE_PRIVATE void YCPUSetFlagsAfterSub(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_borrow) {
  YCPUSetFlagsAfterDec(ctx, op1, op2, result, did_borrow);

  // Carry Flag (CF) - Set when a borrow is generated
  // CF is set if op1 < (op2 + did_borrow) (unsigned comparison)
  uint32_t val_being_subtracted =
      (op2 & YCPUMaxValue[ctx->metadata->width]) + (did_borrow ? 1 : 0);
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, op1 < val_being_subtracted);
}

// Common signature of YCPUSetFlagsAfterSub and YCPUSetFlagsAfterDec.
typedef void (*YCPUSetFlagsAfterSubFn)(
    const YCPUInstructionContext* ctx, uint32_t op1, uint32_t op2,
    uint32_t result, bool did_borrow);

// Common logic for SUB, SBB, and DEC instructions.
Y_FILE_PRIVATE Y_HOT YCPUInstructionResult YCPUExecuteSubCommon(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value, bool borrow,
    YCPUSetFlagsAfterSubFn set_flags_after_fn) {
  uint32_t raw_dest_value = YCPUFromOperand(dest);
  uint32_t raw_src_value = YCPUFromOperandValue(src_value);
  bool should_borrow = borrow && YCPUGetFlag(ctx->cpu, Y_CPU_CF);
  uint32_t result = raw_dest_value - raw_src_value - (should_borrow ? 1 : 0);
  YCPUWriteOperand(ctx, dest, result);
  (*set_flags_after_fn)(
      ctx, raw_dest_value, raw_src_value, result, should_borrow);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Common logic for SUB instructions
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteSub(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  return YCPUExecuteSubCommon(
      ctx, dest, src_value, /* borrow */ false, YCPUSetFlagsAfterSub);
}

// SUB r/m8, r8
// SUB r/m16, r16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteSubRegisterFromRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteSub(ctx, &dest, src.value);
}

// SUB r8, r/m8
// SUB r16, r/m16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteSubRegisterOrMemoryFromRegister(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteSub(ctx, &dest, src.value);
}

// SUB AL, imm8
// SUB AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubImmediateFromALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteSub(ctx, &dest, src_value);
}

// Common logic for SBB instructions
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteSubWithBorrow(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  return YCPUExecuteSubCommon(
      ctx, dest, src_value, /* borrow */ true, YCPUSetFlagsAfterSub);
}

// SBB r/m8, r8
// SBB r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubRegisterFromRegisterOrMemoryWithBorrow(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteSubWithBorrow(ctx, &dest, src.value);
}

// SBB r8, r/m8
// SBB r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubRegisterOrMemoryFromRegisterWithBorrow(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteSubWithBorrow(ctx, &dest, src.value);
}

// SBB AL, imm8
// SBB AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSubImmediateFromALOrAXWithBorrow(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteSubWithBorrow(ctx, &dest, src_value);
}

// Common logic for DEC instructions
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDec(const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUOperandValue src_value = 1;
  return YCPUExecuteSubCommon(
      ctx, dest, src_value, /* borrow */ false, YCPUSetFlagsAfterDec);
}

// DEC AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDecRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode - 0x48);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &dest);
  return YCPUExecuteDec(ctx, &dest);
}


// ==============================================================================
// src/cpu/instructions_sub.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_sign_ext.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_sign_ext.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Sign extension instructions
// ============================================================================

// CBW
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCbw(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[Y_CPU_AX] & 0xFF;
  uint8_t ah = (al & YCPUSignBit[Y_CPU_BYTE]) ? 0xFF : 0x00;
  ctx->cpu->registers[Y_CPU_AX] = (ah << 8) | al;
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// CWD
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteCwd(const YCPUInstructionContext* ctx) {
  ctx->cpu->registers[Y_CPU_DX] =
      (ctx->cpu->registers[Y_CPU_AX] & YCPUSignBit[Y_CPU_WORD]) ? 0xFFFF
                                                                : 0x0000;
  return Y_CPU_INSTRUCTION_EXECUTED;
}


// ==============================================================================
// src/cpu/instructions_sign_ext.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_cmp.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_cmp.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// CMP instructions
// ============================================================================

// Common logic for CMP instructions. Computes dest - src and sets flags.
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult YCPUExecuteCmp(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  uint32_t raw_dest_value = YCPUFromOperand(dest);
  uint32_t raw_src_value = YCPUFromOperandValue(src_value);
  uint32_t result = raw_dest_value - raw_src_value;
  YCPUSetFlagsAfterSub(ctx, raw_dest_value, raw_src_value, result, false);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// CMP r/m8, r8
// CMP r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteCmpRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteCmp(ctx, &dest, src.value);
}

// CMP r8, r/m8
// CMP r16, r/m16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteCmpRegisterOrMemoryToRegister(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteCmp(ctx, &dest, src.value);
}

// CMP AL, imm8
// CMP AX, imm16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteCmpImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteCmp(ctx, &dest, src_value);
}


// ==============================================================================
// src/cpu/instructions_cmp.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_bool.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_bool.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Boolean AND, OR and XOR instructions
// ============================================================================

Y_MODULE_PRIVATE void YCPUSetFlagsAfterBooleanInstruction(
    const YCPUInstructionContext* ctx, uint32_t result) {
  YCPUSetCommonFlagsAfterInstruction(ctx, result);
  // Carry Flag (CF) should be cleared
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, false);
  // Overflow Flag (OF) should be cleared
  YCPUSetFlag(ctx->cpu, Y_CPU_OF, false);
}

// Common logic for AND instructions.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteBooleanAnd(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  uint32_t result = YCPUFromOperand(dest) & YCPUFromOperandValue(src_value);
  YCPUWriteOperand(ctx, dest, result);
  YCPUSetFlagsAfterBooleanInstruction(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// AND r/m8, r8
// AND r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanAndRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteBooleanAnd(ctx, &dest, src.value);
}

// AND r8, r/m8
// AND r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanAndRegisterOrMemoryToRegister(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteBooleanAnd(ctx, &dest, src.value);
}

// AND AL, imm8
// AND AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanAndImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteBooleanAnd(ctx, &dest, src_value);
}

// Common logic for OR instructions.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteBooleanOr(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  uint32_t result = YCPUFromOperand(dest) | YCPUFromOperandValue(src_value);
  YCPUWriteOperand(ctx, dest, result);
  YCPUSetFlagsAfterBooleanInstruction(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// OR r/m8, r8
// OR r/m16, r16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanOrRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteBooleanOr(ctx, &dest, src.value);
}

// OR r8, r/m8
// OR r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanOrRegisterOrMemoryToRegister(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteBooleanOr(ctx, &dest, src.value);
}

// OR AL, imm8
// OR AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanOrImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteBooleanOr(ctx, &dest, src_value);
}

// Common logic for XOR instructions.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteBooleanXor(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  uint32_t result = YCPUFromOperand(dest) ^ YCPUFromOperandValue(src_value);
  YCPUWriteOperand(ctx, dest, result);
  YCPUSetFlagsAfterBooleanInstruction(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// XOR r/m8, r8
// XOR r/m16, r16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteBooleanXorRegisterToRegisterOrMemory(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteBooleanXor(ctx, &dest, src.value);
}

// XOR r8, r/m8
// XOR r16, r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanXorRegisterOrMemoryToRegister(
    const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOrMemoryOperand(ctx, &src);
  return YCPUExecuteBooleanXor(ctx, &dest, src.value);
}

// XOR AL, imm8
// XOR AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteBooleanXorImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteBooleanXor(ctx, &dest, src_value);
}

// ============================================================================
// TEST instructions
// ============================================================================

// Common logic for TEST instructions.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteTest(
    const YCPUInstructionContext* ctx, YCPUOperand* dest,
    YCPUOperandValue src_value) {
  uint32_t result = YCPUFromOperand(dest) & YCPUFromOperandValue(src_value);
  YCPUSetFlagsAfterBooleanInstruction(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// TEST r/m8, r8
// TEST r/m16, r16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteTestRegisterToRegisterOrMemory(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperand src;
  YCPUReadRegisterOperand(ctx, &src);
  return YCPUExecuteTest(ctx, &dest, src.value);
}

// TEST AL, imm8
// TEST AX, imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteTestImmediateToALOrAX(const YCPUInstructionContext* ctx) {
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteTest(ctx, &dest, src_value);
}


// ==============================================================================
// src/cpu/instructions_bool.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_ctrl_flow.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_ctrl_flow.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "cycles.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// JMP instructions
// ============================================================================

// Jump to a relative signed byte offset.
Y_FILE_PRIVATE Y_HOT YCPUInstructionResult YCPUExecuteRelativeJumpByte(
    const YCPUInstructionContext* ctx, YCPUOperandValue offset_value) {
  ctx->cpu->registers[Y_CPU_IP] = YCPUAddSignedOffsetByte(
      ctx->cpu->registers[Y_CPU_IP], YCPUFromOperandValue(offset_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Jump to a relative signed word offset.
Y_FILE_PRIVATE Y_HOT YCPUInstructionResult YCPUExecuteRelativeJumpWord(
    const YCPUInstructionContext* ctx, YCPUOperandValue offset_value) {
  ctx->cpu->registers[Y_CPU_IP] = YCPUAddSignedOffsetWord(
      ctx->cpu->registers[Y_CPU_IP], YCPUFromOperandValue(offset_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Table of relative jump instructions, indexed by width.
Y_FILE_PRIVATE
YCPUInstructionResult (*const YCPURelativeJumpFn[Y_CPU_NUM_WIDTHS])(
    const YCPUInstructionContext* ctx, YCPUOperandValue offset_value) = {
    YCPUExecuteRelativeJumpByte,  // Y_CPU_BYTE
    YCPUExecuteRelativeJumpWord,  // Y_CPU_WORD
};

// Common logic for JMP instructions.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteRelativeJump(
    const YCPUInstructionContext* ctx, YCPUOperandValue offset_value) {
  return YCPURelativeJumpFn[ctx->metadata->width](ctx, offset_value);
}

// JMP rel8
// JMP rel16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteShortOrNearJump(const YCPUInstructionContext* ctx) {
  YCPUOperandValue offset_value = YCPUReadImmediate(ctx);
  return YCPUExecuteRelativeJump(ctx, offset_value);
}

// Common logic for far jumps.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteFarJump(
    const YCPUInstructionContext* ctx, YCPUOperandValue segment,
    YCPUOperandValue offset) {
  ctx->cpu->registers[Y_CPU_CS] = YCPUFromOperandValue(segment);
  ctx->cpu->registers[Y_CPU_IP] = YCPUFromOperandValue(offset);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// JMP ptr16:16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDirectFarJump(const YCPUInstructionContext* ctx) {
  YCPUOperandValue new_cs =
      (YCPUOperandValue)(((uint16_t)ctx->instruction->immediate[2]) |
                         (((uint16_t)ctx->instruction->immediate[3]) << 8));
  YCPUOperandValue new_ip =
      (YCPUOperandValue)(((uint16_t)ctx->instruction->immediate[0]) |
                         (((uint16_t)ctx->instruction->immediate[1]) << 8));
  return YCPUExecuteFarJump(ctx, new_cs, new_ip);
}

// ============================================================================
// Conditional jumps
// ============================================================================

// Common logic for conditional jumps.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteConditionalJump(
    const YCPUInstructionContext* ctx, bool value, bool success_value) {
  if (value == success_value) {
    // A taken jump throws away the prefetch queue and has to fill it again.
    // The base cost in the cycle table is for the branch not being taken.
    YCPUAddCycles(ctx->cpu, Y_CPU_JUMP_TAKEN_CYCLES);
    YCPUOperandValue offset_value = YCPUReadImmediate(ctx);
    return YCPUExecuteRelativeJump(ctx, offset_value);
  }
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Table of flag register bitmasks for conditional jumps. The index corresponds
// to (opcode & 0x0F) / 2, so that the undocumented 0x60-0x6F aliases share the
// entries of their 0x70-0x7F counterparts.
Y_FILE_PRIVATE const uint16_t YCPUUnsignedConditionalJumpFlagBitmasks[] = {
    Y_CPU_OF,             // 0x70 - JO, 0x71 - JNO
    Y_CPU_CF,             // 0x72 - JC, 0x73 - JNC
    Y_CPU_ZF,             // 0x74 - JE, 0x75 - JNE
    Y_CPU_CF | Y_CPU_ZF,  // 0x76 - JBE, 0x77 - JNBE
    Y_CPU_SF,             // 0x78 - JS, 0x79 - JNS
    Y_CPU_PF,             // 0x7A - JP, 0x7B - JNP
};

// Unsigned conditional jumps.
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteUnsignedConditionalJump(const YCPUInstructionContext* ctx) {
  // Masking off the high nibble handles both 0x70-0x7F and their undocumented
  // 0x60-0x6F aliases, which the 8086/8088 decodes identically.
  uint16_t flag_mask = YCPUUnsignedConditionalJumpFlagBitmasks
      [(ctx->instruction->opcode & 0x0F) / 2];
  bool flag_value = (ctx->cpu->flags & flag_mask) != 0;
  // Even opcode => jump if the flag is set
  // Odd opcode => jump if the flag is not set
  bool success_value = ((ctx->instruction->opcode & 0x1) == 0);
  return YCPUExecuteConditionalJump(ctx, flag_value, success_value);
}

// JL/JGNE and JNL/JGE
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteSignedConditionalJumpJLOrJNL(const YCPUInstructionContext* ctx) {
  const bool is_greater_or_equal =
      YCPUGetFlag(ctx->cpu, Y_CPU_SF) == YCPUGetFlag(ctx->cpu, Y_CPU_OF);
  const bool success_value = (ctx->instruction->opcode & 0x1);
  return YCPUExecuteConditionalJump(ctx, is_greater_or_equal, success_value);
}

// JLE/JG and JNLE/JG
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSignedConditionalJumpJLEOrJNLE(const YCPUInstructionContext* ctx) {
  const bool is_greater =
      !YCPUGetFlag(ctx->cpu, Y_CPU_ZF) &&
      (YCPUGetFlag(ctx->cpu, Y_CPU_SF) == YCPUGetFlag(ctx->cpu, Y_CPU_OF));
  const bool success_value = (ctx->instruction->opcode & 0x1);
  return YCPUExecuteConditionalJump(ctx, is_greater, success_value);
}

// ============================================================================
// Loop instructions
// ============================================================================

// LOOP rel8
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteLoop(const YCPUInstructionContext* ctx) {
  return YCPUExecuteConditionalJump(
      ctx, --(ctx->cpu->registers[Y_CPU_CX]) != 0, true);
}

// LOOPZ rel8
// LOOPNZ rel8
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoopZOrNZ(const YCPUInstructionContext* ctx) {
  bool condition1 = --(ctx->cpu->registers[Y_CPU_CX]) != 0;
  bool condition2 = YCPUGetFlag(ctx->cpu, Y_CPU_ZF) ==
                    (bool)(ctx->instruction->opcode - 0xE0);
  return YCPUExecuteConditionalJump(ctx, condition1 && condition2, true);
}

// JCXZ rel8
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteJumpIfCXIsZero(const YCPUInstructionContext* ctx) {
  return YCPUExecuteConditionalJump(
      ctx, ctx->cpu->registers[Y_CPU_CX] == 0, true);
}

// ============================================================================
// CALL and RET instructions
// ============================================================================

// Common logic for near calls.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteNearCall(
    const YCPUInstructionContext* ctx, YCPUOperandValue offset) {
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[Y_CPU_IP]);
  return YCPUExecuteRelativeJump(ctx, offset);
}

// CALL rel16
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteDirectNearCall(const YCPUInstructionContext* ctx) {
  YCPUOperandValue offset = YCPUReadImmediate(ctx);
  return YCPUExecuteNearCall(ctx, offset);
}

// Common logic for far calls.
Y_MODULE_PRIVATE YCPUInstructionResult YCPUExecuteFarCall(
    const YCPUInstructionContext* ctx, YCPUOperandValue segment,
    YCPUOperandValue offset) {
  // Push the current CS and IP onto the stack.
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[Y_CPU_CS]);
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[Y_CPU_IP]);
  return YCPUExecuteFarJump(ctx, segment, offset);
}

// CALL ptr16:16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDirectFarCall(const YCPUInstructionContext* ctx) {
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[Y_CPU_CS]);
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[Y_CPU_IP]);
  return YCPUExecuteDirectFarJump(ctx);
}

// Common logic for RET instructions.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteNearReturnCommon(
    const YCPUInstructionContext* ctx, uint16_t arg_size) {
  YCPUOperandValue new_ip = YCPUPop(ctx->cpu);
  ctx->cpu->registers[Y_CPU_IP] = YCPUFromOperandValue(new_ip);
  ctx->cpu->registers[Y_CPU_SP] += arg_size;
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// RET
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteNearReturn(const YCPUInstructionContext* ctx) {
  return YCPUExecuteNearReturnCommon(ctx, 0);
}

// RET imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteNearReturnAndPop(const YCPUInstructionContext* ctx) {
  YCPUOperandValue arg_size_value = YCPUReadImmediate(ctx);
  return YCPUExecuteNearReturnCommon(ctx, YCPUFromOperandValue(arg_size_value));
}

// Common logic for RETF instructions.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteFarReturnCommon(
    const YCPUInstructionContext* ctx, uint16_t arg_size) {
  YCPUOperandValue new_ip = YCPUPop(ctx->cpu);
  YCPUOperandValue new_cs = YCPUPop(ctx->cpu);
  ctx->cpu->registers[Y_CPU_IP] = YCPUFromOperandValue(new_ip);
  ctx->cpu->registers[Y_CPU_CS] = YCPUFromOperandValue(new_cs);
  ctx->cpu->registers[Y_CPU_SP] += arg_size;
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// RETF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteFarReturn(const YCPUInstructionContext* ctx) {
  return YCPUExecuteFarReturnCommon(ctx, 0);
}

// RETF imm16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteFarReturnAndPop(const YCPUInstructionContext* ctx) {
  YCPUOperandValue arg_size_value = YCPUReadImmediate(ctx);
  return YCPUExecuteFarReturnCommon(ctx, YCPUFromOperandValue(arg_size_value));
}

// ============================================================================
// Interrupt instructions
// ============================================================================

// Common logic for returning from an interrupt.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteReturnFromInterrupt(YCPUState* cpu) {
  YCPUOperandValue ip_value = YCPUPop(cpu);
  cpu->registers[Y_CPU_IP] = YCPUFromOperandValue(ip_value);
  YCPUOperandValue cs_value = YCPUPop(cpu);
  cpu->registers[Y_CPU_CS] = YCPUFromOperandValue(cs_value);
  YCPUOperandValue flags_value = YCPUPop(cpu);
  cpu->flags = YCPUToFlagsRegisterValue(YCPUFromOperandValue(flags_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// IRET
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteIret(const YCPUInstructionContext* ctx) {
  return YCPUExecuteReturnFromInterrupt(ctx->cpu);
}

// INT 3
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInt3(const YCPUInstructionContext* ctx) {
  YCPURaiseInternalInterrupt(ctx->cpu, Y_CPU_INTERRUPT_BREAKPOINT);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// INTO
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteInto(const YCPUInstructionContext* ctx) {
  if (YCPUGetFlag(ctx->cpu, Y_CPU_OF)) {
    YCPURaiseInternalInterrupt(ctx->cpu, Y_CPU_INTERRUPT_OVERFLOW);
  }
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// INT n
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteIntN(const YCPUInstructionContext* ctx) {
  YCPUOperandValue interrupt_number_value = YCPUReadImmediate(ctx);
  YCPURaiseInternalInterrupt(
      ctx->cpu, YCPUFromOperandValue(interrupt_number_value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// HLT
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteHlt(Y_UNUSED const YCPUInstructionContext* ctx) {
  // HLT executes successfully - it just leaves the CPU halted until an
  // interrupt wakes it. YCPUTick() reports the halted state to its caller.
  ctx->cpu->is_halted = true;
  return Y_CPU_INSTRUCTION_EXECUTED;
}


// ==============================================================================
// src/cpu/instructions_ctrl_flow.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_stack.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_stack.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// PUSH and POP instructions
// ============================================================================

// PUSH AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecutePushRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode - 0x50);
  YCPUOperand src;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &src);
  YCPUPushSourceOperand(ctx->cpu, &src);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// POP AX/CX/DX/BX/SP/BP/SI/DI
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecutePopRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(ctx->instruction->opcode - 0x58);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &dest);
  YCPUOperandValue value = YCPUPop(ctx->cpu);
  YCPUWriteOperandAddress(ctx, &dest.address, YCPUFromOperandValue(value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// PUSH ES/CS/SS/DS
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecutePushSegmentRegister(const YCPUInstructionContext* ctx) {
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(((ctx->instruction->opcode >> 3) & 0x03) + 8);
  YCPUOperand src;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &src);
  YCPUPushValue(ctx->cpu, src.value);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// POP ES/CS/SS/DS
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecutePopSegmentRegister(const YCPUInstructionContext* ctx) {
  // The segment register field is only two bits wide, which is what makes
  // 0x0F decode as POP CS on the 8086/8088. Popping into CS is legal there -
  // it just makes the next instruction fetch come from the new segment.
  YCPURegisterIndex register_index =
      (YCPURegisterIndex)(((ctx->instruction->opcode >> 3) & 0x03) + 8);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, register_index, &dest);
  YCPUOperandValue value = YCPUPop(ctx->cpu);
  YCPUWriteOperandAddress(ctx, &dest.address, YCPUFromOperandValue(value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// PUSHF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePushFlags(const YCPUInstructionContext* ctx) {
  YCPUPushValue(ctx->cpu, ctx->cpu->flags);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// POPF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePopFlags(const YCPUInstructionContext* ctx) {
  YCPUOperandValue value = YCPUPop(ctx->cpu);
  ctx->cpu->flags = YCPUToFlagsRegisterValue(YCPUFromOperandValue(value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// POP r/m16
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecutePopRegisterOrMemory(const YCPUInstructionContext* ctx) {
  // The 8086/8088 does not decode the REG field of 0x8F at all, so every value
  // pops. Only REG 0 is documented.
  //
  // The destination address is resolved before the pop, because SP and BP can
  // both address it and the 8086 computes the effective address first.
  YCPUOperandAddress dest = YCPUGetRegisterOrMemoryOperandAddress(ctx);
  YCPUOperandValue value = YCPUPop(ctx->cpu);
  YCPUWriteOperandAddress(ctx, &dest, YCPUFromOperandValue(value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ============================================================================
// LAHF and SAHF
// ============================================================================

// Returns the AH register address.
Y_FILE_PRIVATE const YCPUOperandAddress* YCPUGetAHRegisterAddress(void) {
  static YCPUOperandAddress ah = {
      .type = Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER,
      .register_index = Y_CPU_AX,
      .offset = 8,
  };
  return &ah;
}

// LAHF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteLoadAHFromFlags(const YCPUInstructionContext* ctx) {
  YCPUWriteRegisterOperandByte(
      ctx->cpu, YCPUGetAHRegisterAddress(),
      (YCPUOperandValue)(ctx->cpu->flags & 0x00FF));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// SAHF
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteStoreAHToFlags(const YCPUInstructionContext* ctx) {
  YCPUOperandValue value =
      YCPUReadRegisterOperandByte(ctx->cpu, YCPUGetAHRegisterAddress());
  // Clear the lower byte of flags and set it to the value in AH
  ctx->cpu->flags =
      YCPUToFlagsRegisterValue((ctx->cpu->flags & 0xFF00) | value);
  return Y_CPU_INSTRUCTION_EXECUTED;
}


// ==============================================================================
// src/cpu/instructions_stack.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_flags.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_flags.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// CLC, STC, CLI, STI, CLD, STD instructions
// ============================================================================

// Table of flags corresponding to the CLC, STC, CLI, STI, CLD, and STD
// instructions, indexed by (opcode - 0xF8) / 2.
Y_FILE_PRIVATE const YCPUFlag YCPUFlagsForClearAndSetInstructions[] = {
    Y_CPU_CF,  // CLC, STC
    Y_CPU_IF,  // CLI, STI
    Y_CPU_DF,  // CLD, STD
};

Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteClearOrSetFlag(const YCPUInstructionContext* ctx) {
  uint8_t opcode_index = ctx->instruction->opcode - 0xF8;
  YCPUFlag flag = YCPUFlagsForClearAndSetInstructions[opcode_index / 2];
  bool value = (opcode_index & 0x1) != 0;
  YCPUSetFlag(ctx->cpu, flag, value);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ============================================================================
// CMC instruction
// ============================================================================

// CMC
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteComplementCarryFlag(const YCPUInstructionContext* ctx) {
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, !YCPUGetFlag(ctx->cpu, Y_CPU_CF));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ============================================================================
// SALC instruction
// ============================================================================

// SALC - Set AL from Carry
//
// Undocumented on every x86 generation, but consistently implemented: AL
// becomes 0xFF if CF is set and 0x00 otherwise. No flags are affected.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteSetALFromCarry(const YCPUInstructionContext* ctx) {
  const uint8_t value = YCPUGetFlag(ctx->cpu, Y_CPU_CF) ? 0xFF : 0x00;
  ctx->cpu->registers[Y_CPU_AX] =
      (ctx->cpu->registers[Y_CPU_AX] & 0xFF00) | value;
  return Y_CPU_INSTRUCTION_EXECUTED;
}


// ==============================================================================
// src/cpu/instructions_flags.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_io.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_io.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// IN and OUT instructions
// ============================================================================

// Read a byte from an I/O port.
Y_FILE_PRIVATE Y_HOT YCPUOperandValue
YCPUReadByteFromPort(YCPUState* cpu, uint16_t port) {
  return (YCPUOperandValue)(cpu->config.read_port
                                ? cpu->config.read_port(cpu, port)
                                : 0xFF);
}

// Read a word from an I/O port as a uint16_t. The 8088 has an 8-bit data bus,
// so a word access is two byte accesses to consecutive ports. Peripherals rely
// on this: writing a 6845 register index and its data in one OUT DX, AX is the
// standard idiom, and it is what the BIOS uses.
Y_FILE_PRIVATE YCPUOperandValue
YCPUReadWordFromPort(YCPUState* cpu, uint16_t port) {
  uint8_t low = (uint8_t)YCPUReadByteFromPort(cpu, port);
  uint8_t high = (uint8_t)YCPUReadByteFromPort(cpu, (uint16_t)(port + 1));
  return (YCPUOperandValue)((high << 8) | low);
}

// Table of functions to read from an I/O port, indexed by data width.
Y_FILE_PRIVATE YCPUOperandValue (*const YCPUReadFromPortFns[])(
    YCPUState*, uint16_t) = {
  YCPUReadByteFromPort,  // Y_CPU_BYTE
  YCPUReadWordFromPort,  // Y_CPU_WORD
};

// Common logic for IN instructions.
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteIn(const YCPUInstructionContext* ctx, uint16_t port) {
  YCPUOperandValue value =
      YCPUReadFromPortFns[ctx->metadata->width](ctx->cpu, port);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  YCPUWriteOperand(ctx, &dest, YCPUFromOperandValue(value));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// IN AL, imm8
// IN AX, imm8
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteInImmediate(const YCPUInstructionContext* ctx) {
  YCPUOperandValue port = YCPUReadImmediateOperandByte(ctx->instruction);
  return YCPUExecuteIn(ctx, YCPUFromOperandValue(port));
}

// IN AL, DX
// IN AX, DX
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteInDX(const YCPUInstructionContext* ctx) {
  return YCPUExecuteIn(ctx, ctx->cpu->registers[Y_CPU_DX]);
}

// Write a byte to an I/O port.
Y_FILE_PRIVATE void YCPUWriteByteToPort(
    YCPUState* cpu, uint16_t port, YCPUOperandValue value) {
  if (!cpu->config.write_port) {
    return;
  }
  cpu->config.write_port(cpu, port, YCPUFromOperandValue(value));
}

// Write a word to an I/O port. As with reads, this is two byte accesses to
// consecutive ports.
Y_FILE_PRIVATE void YCPUWriteWordToPort(
    YCPUState* cpu, uint16_t port, YCPUOperandValue value) {
  uint32_t raw_value = YCPUFromOperandValue(value);
  YCPUWriteByteToPort(cpu, port, (YCPUOperandValue)(raw_value & 0xFF));
  YCPUWriteByteToPort(
      cpu, (uint16_t)(port + 1), (YCPUOperandValue)((raw_value >> 8) & 0xFF));
}

// Table of functions to write to an I/O port, indexed by data width.
Y_FILE_PRIVATE void (*const YCPUWriteToPortFns[])(
    YCPUState*, uint16_t, YCPUOperandValue) = {
    YCPUWriteByteToPort,  // Y_CPU_BYTE
    YCPUWriteWordToPort,  // Y_CPU_WORD
};

// Common logic for OUT instructions.
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteOut(const YCPUInstructionContext* ctx, uint16_t port) {
  YCPUOperand src;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &src);
  YCPUWriteToPortFns[ctx->metadata->width](ctx->cpu, port, src.value);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// OUT imm8, AL
// OUT imm8, AX
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteOutImmediate(const YCPUInstructionContext* ctx) {
  YCPUOperandValue port = YCPUReadImmediateOperandByte(ctx->instruction);
  return YCPUExecuteOut(ctx, YCPUFromOperandValue(port));
}

// OUT DX, AL
// OUT DX, AX
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteOutDX(const YCPUInstructionContext* ctx) {
  return YCPUExecuteOut(ctx, ctx->cpu->registers[Y_CPU_DX]);
}


// ==============================================================================
// src/cpu/instructions_io.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_string.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_string.c"
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


// ==============================================================================
// src/cpu/instructions_string.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_bcd_ascii.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_bcd_ascii.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// BCD and ASCII arithmetic instructions
// ============================================================================

// The value DAA and DAS compare AL against to decide whether the high digit
// needs adjusting. Both test AL as it was on entry, before the low digit was
// adjusted, so a carry out of the low digit does not drag the high one along
// with it.
//
// The limit is one higher when the auxiliary carry flag was already set on
// entry. That is not what Intel's published pseudocode says, which uses 0x99
// throughout, but it is what the 8086/8088 does - and with AL between 0x9A and
// 0x9F it is the difference between adjusting and not.
Y_FILE_PRIVATE uint8_t YCPUGetBCDHighDigitLimit(bool auxiliary_carry) {
  return auxiliary_carry ? 0x9F : 0x99;
}

// AAA
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAaa(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[Y_CPU_AX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[Y_CPU_AX] >> 8) & 0xFF;
  uint8_t al_low = al & 0x0F;
  if (al_low > 9 || YCPUGetFlag(ctx->cpu, Y_CPU_AF)) {
    al += 6;
    ++ah;
    YCPUSetFlag(ctx->cpu, Y_CPU_AF, true);
    YCPUSetFlag(ctx->cpu, Y_CPU_CF, true);
  } else {
    YCPUSetFlag(ctx->cpu, Y_CPU_AF, false);
    YCPUSetFlag(ctx->cpu, Y_CPU_CF, false);
  }
  al &= 0x0F;
  ctx->cpu->registers[Y_CPU_AX] = (ah << 8) | al;
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// AAS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAas(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[Y_CPU_AX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[Y_CPU_AX] >> 8) & 0xFF;
  uint8_t al_low = al & 0x0F;
  if (al_low > 9 || YCPUGetFlag(ctx->cpu, Y_CPU_AF)) {
    al -= 6;
    --ah;
    YCPUSetFlag(ctx->cpu, Y_CPU_AF, true);
    YCPUSetFlag(ctx->cpu, Y_CPU_CF, true);
  } else {
    YCPUSetFlag(ctx->cpu, Y_CPU_AF, false);
    YCPUSetFlag(ctx->cpu, Y_CPU_CF, false);
  }
  al &= 0x0F;
  ctx->cpu->registers[Y_CPU_AX] = (ah << 8) | al;
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// AAM
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAam(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[Y_CPU_AX] & 0xFF;
  YCPUOperandValue base = YCPUReadImmediate(ctx);
  uint16_t base_value = YCPUFromOperandValue(base);
  if (base_value == 0) {
    // AAM divides by its immediate operand, so a base of 0 raises a divide
    // error just like DIV by zero does, rather than being an invalid encoding.
    // The division never produces a result, and the flags are left set as
    // though it had produced zero.
    YCPUSetCommonFlagsAfterInstruction(ctx, 0);
    YCPUSetFlag(ctx->cpu, Y_CPU_CF, false);
    YCPUSetFlag(ctx->cpu, Y_CPU_AF, false);
    YCPUSetFlag(ctx->cpu, Y_CPU_OF, false);
    YCPURaiseInternalInterrupt(ctx->cpu, Y_CPU_INTERRUPT_DIVIDE_ERROR);
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  uint8_t ah = al / base_value;
  al %= base_value;
  ctx->cpu->registers[Y_CPU_AX] = (ah << 8) | al;
  YCPUSetCommonFlagsAfterInstruction(ctx, al);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// AAD
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteAad(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[Y_CPU_AX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[Y_CPU_AX] >> 8) & 0xFF;
  YCPUOperandValue base = YCPUReadImmediate(ctx);
  uint8_t base_value = YCPUFromOperandValue(base);
  al += ah * base_value;
  ah = 0;
  ctx->cpu->registers[Y_CPU_AX] = (ah << 8) | al;
  YCPUSetCommonFlagsAfterInstruction(ctx, al);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// DAA
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDaa(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[Y_CPU_AX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[Y_CPU_AX] >> 8) & 0xFF;
  const uint8_t original_al = al;
  const bool original_carry = YCPUGetFlag(ctx->cpu, Y_CPU_CF);
  const uint8_t high_digit_limit =
      YCPUGetBCDHighDigitLimit(YCPUGetFlag(ctx->cpu, Y_CPU_AF));

  if ((al & 0x0F) > 9 || YCPUGetFlag(ctx->cpu, Y_CPU_AF)) {
    al += 6;
    YCPUSetFlag(ctx->cpu, Y_CPU_AF, true);
  } else {
    YCPUSetFlag(ctx->cpu, Y_CPU_AF, false);
  }
  if (original_al > high_digit_limit || original_carry) {
    al += 0x60;
    YCPUSetFlag(ctx->cpu, Y_CPU_CF, true);
  } else {
    YCPUSetFlag(ctx->cpu, Y_CPU_CF, false);
  }
  ctx->cpu->registers[Y_CPU_AX] = (ah << 8) | al;
  YCPUSetCommonFlagsAfterInstruction(ctx, al);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// DAS
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteDas(const YCPUInstructionContext* ctx) {
  uint8_t al = ctx->cpu->registers[Y_CPU_AX] & 0xFF;
  uint8_t ah = (ctx->cpu->registers[Y_CPU_AX] >> 8) & 0xFF;
  const uint8_t original_al = al;
  const bool original_carry = YCPUGetFlag(ctx->cpu, Y_CPU_CF);
  const uint8_t high_digit_limit =
      YCPUGetBCDHighDigitLimit(YCPUGetFlag(ctx->cpu, Y_CPU_AF));

  if ((al & 0x0F) > 9 || YCPUGetFlag(ctx->cpu, Y_CPU_AF)) {
    al -= 6;
    YCPUSetFlag(ctx->cpu, Y_CPU_AF, true);
  } else {
    YCPUSetFlag(ctx->cpu, Y_CPU_AF, false);
  }
  if (original_al > high_digit_limit || original_carry) {
    al -= 0x60;
    YCPUSetFlag(ctx->cpu, Y_CPU_CF, true);
  } else {
    YCPUSetFlag(ctx->cpu, Y_CPU_CF, false);
  }
  ctx->cpu->registers[Y_CPU_AX] = (ah << 8) | al;
  YCPUSetCommonFlagsAfterInstruction(ctx, al);
  return Y_CPU_INSTRUCTION_EXECUTED;
}


// ==============================================================================
// src/cpu/instructions_bcd_ascii.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_group_1.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_group_1.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 1 - ADD, OR, ADC, SBB, AND, SUB, XOR, CMP
// ============================================================================

typedef YCPUInstructionResult (*YCPUGroup1ExecuteInstructionFn)(
    const YCPUInstructionContext* ctx, YCPUOperand* dest, YCPUOperandValue src);

// Group 1 instruction implementations, indexed by the corresponding REG field
// value in the YCPUModRM byte.
Y_FILE_PRIVATE const YCPUGroup1ExecuteInstructionFn
    YCPUGroup1ExecuteInstructionFns[] = {
        YCPUExecuteAdd,            // 0 - ADD
        YCPUExecuteBooleanOr,      // 1 - OR
        YCPUExecuteAddWithCarry,   // 2 - ADC
        YCPUExecuteSubWithBorrow,  // 3 - SBB
        YCPUExecuteBooleanAnd,     // 4 - AND
        YCPUExecuteSub,            // 5 - SUB
        YCPUExecuteBooleanXor,     // 6 - XOR
        YCPUExecuteCmp,            // 7 - CMP
};

// Group 1 instruction handler.
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteGroup1Instruction(const YCPUInstructionContext* ctx) {
  const YCPUGroup1ExecuteInstructionFn fn =
      YCPUGroup1ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return fn(ctx, &dest, src_value);
}

// Group 1 instruction handler, but sign-extends the 8-bit immediate value.
Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
YCPUExecuteGroup1InstructionWithSignExtension(
    const YCPUInstructionContext* ctx) {
  const YCPUGroup1ExecuteInstructionFn fn =
      YCPUGroup1ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  YCPUOperandValue src_value = YCPUReadImmediateOperandByte(
      ctx->instruction);  // immediate is always 8-bit
  YCPUOperandValue src_value_extended =
      (YCPUOperandValue)((int16_t)((int8_t)src_value));
  // Sign-extend the immediate value to the destination width.
  return fn(ctx, &dest, src_value_extended);
}


// ==============================================================================
// src/cpu/instructions_group_1.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_group_2.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_group_2.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "cycles.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 2 - ROL, ROR, RCL, RCR, SHL, SHR, SAL, SAR
// ============================================================================

typedef YCPUInstructionResult (*YCPUGroup2ExecuteInstructionFn)(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count);

// The 8086/8088 shifts one bit at a time and recomputes the overflow flag on
// every pass, so a multi-bit shift leaves behind the flag from its last pass
// rather than an undefined value. Each rule below is that last pass written in
// closed form.

// Overflow after a left shift or rotate: the bit shifted out of the top
// differs from the sign bit left behind, which is to say the last pass changed
// the sign of the value.
Y_FILE_PRIVATE void YCPUSetOverflowFlagAfterLeftShift(
    const YCPUInstructionContext* ctx, uint32_t result, bool carry) {
  const bool result_sign = (result & YCPUSignBit[ctx->metadata->width]) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_OF, carry != result_sign);
}

// Overflow after a right rotate: the top two bits of the result differ. The
// bit rotated into the top came from the bottom, so this again says the last
// pass changed the sign of the value.
Y_FILE_PRIVATE void YCPUSetOverflowFlagAfterRightRotate(
    const YCPUInstructionContext* ctx, uint32_t result) {
  const uint32_t sign_bit = YCPUSignBit[ctx->metadata->width];
  const bool result_sign = (result & sign_bit) != 0;
  const bool below_sign = (result & (sign_bit >> 1)) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_OF, result_sign != below_sign);
}

// The 8086/8088 does not mask the shift count the way the 80186 and later do,
// so a count taken from CL can be as large as 255. Once an operand has been
// shifted past its own width there is nothing left in it and nothing left to
// fall out of it, so every larger count behaves alike. Clamping to just past
// the width keeps that true while holding the shifts below the width of the
// intermediate they are computed in, where C leaves them undefined.
Y_FILE_PRIVATE uint8_t
YCPUClampShiftCount(const YCPUInstructionContext* ctx, uint8_t count) {
  const uint8_t limit = YCPUNumBits[ctx->metadata->width] + 1;
  return count > limit ? limit : count;
}

// SHL r/m8, 1
// SHL r/m16, 1
// SHL r/m8, CL
// SHL r/m16, CL
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Shl(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  count = YCPUClampShiftCount(ctx, count);
  uint32_t value = YCPUFromOperand(op);
  uint32_t result = (value << count) & YCPUMaxValue[ctx->metadata->width];
  YCPUWriteOperand(ctx, op, result);
  bool last_msb =
      ((value << (count - 1)) & YCPUSignBit[ctx->metadata->width]) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_msb);
  YCPUSetOverflowFlagAfterLeftShift(ctx, result, last_msb);
  YCPUSetCommonFlagsAfterInstruction(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// SHR r/m8, 1
// SHR r/m16, 1
// SHR r/m8, CL
// SHR r/m16, CL
Y_FILE_PRIVATE Y_HOT YCPUInstructionResult YCPUExecuteGroup2Shr(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  count = YCPUClampShiftCount(ctx, count);
  uint32_t value = YCPUFromOperand(op);
  uint32_t result = value >> count;
  YCPUWriteOperand(ctx, op, result);
  bool last_lsb = ((value >> (count - 1)) & 1) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_lsb);
  // A right shift clears the sign bit on its first pass, so only a shift by
  // one can leave a sign change behind.
  bool original_msb = ((value & YCPUSignBit[ctx->metadata->width]) != 0);
  YCPUSetFlag(ctx->cpu, Y_CPU_OF, count == 1 && original_msb);
  YCPUSetCommonFlagsAfterInstruction(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// SAR r/m8, 1
// SAR r/m16, 1
// SAR r/m8, CL
// SAR r/m16, CL
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Sar(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  count = YCPUClampShiftCount(ctx, count);
  int32_t value = YCPUFromSignedOperand(ctx->metadata->width, op);
  int32_t result = value >> count;
  YCPUWriteOperand(ctx, op, result);
  bool last_lsb = ((value >> (count - 1)) & 1) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_lsb);
  // An arithmetic right shift preserves the sign bit, so it can never
  // overflow.
  YCPUSetFlag(ctx->cpu, Y_CPU_OF, false);
  YCPUSetCommonFlagsAfterInstruction(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ROL r/m8, 1
// ROL r/m16, 1
// ROL r/m8, CL
// ROL r/m16, CL
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Rol(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  // The 8086 computes the modulus of the count after the zero check, which is
  // different than the 80286 and later processors.
  uint8_t effective_count = count % YCPUNumBits[ctx->metadata->width];
  uint32_t value = YCPUFromOperand(op);
  uint32_t result =
      (value << effective_count) |
      (value >> (YCPUNumBits[ctx->metadata->width] - effective_count));
  YCPUWriteOperand(ctx, op, result);
  bool last_msb = (result & 1) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_msb);
  YCPUSetOverflowFlagAfterLeftShift(ctx, result, last_msb);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// ROR r/m8, 1
// ROR r/m16, 1
// ROR r/m8, CL
// ROR r/m16, CL
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Ror(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  // The 8086 computes the modulus of the count after the zero check, which is
  // different than the 80286 and later processors.
  uint8_t effective_count = count % YCPUNumBits[ctx->metadata->width];
  uint32_t value = YCPUFromOperand(op);
  uint32_t result =
      (value >> effective_count) |
      (value << (YCPUNumBits[ctx->metadata->width] - effective_count));
  YCPUWriteOperand(ctx, op, result);
  bool last_lsb = (result & YCPUSignBit[ctx->metadata->width]) != 0;
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_lsb);
  YCPUSetOverflowFlagAfterRightRotate(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// RCL r/m8, 1
// RCL r/m16, 1
// RCL r/m8, CL
// RCL r/m16, CL
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Rcl(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  const YCPUWidth width = ctx->metadata->width;
  // Rotating through the carry flag cycles over one more bit than the operand
  // is wide. A whole number of cycles puts the value and the carry back the
  // way they were, but the passes still happened, so the overflow flag is
  // still left over from the last one.
  uint8_t effective_count = count % (YCPUNumBits[width] + 1);
  uint32_t value = YCPUFromOperand(op);
  uint32_t result = value;
  bool last_msb = YCPUGetFlag(ctx->cpu, Y_CPU_CF);
  if (effective_count > 0) {
    uint32_t cf_value = last_msb ? (1 << (effective_count - 1)) : 0;
    result = ((value << effective_count) | cf_value |
              (value >> (YCPUNumBits[width] - (effective_count - 1)))) &
             YCPUMaxValue[width];
    YCPUWriteOperand(ctx, op, result);
    last_msb = ((value << (effective_count - 1)) & YCPUSignBit[width]) != 0;
  }
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_msb);
  YCPUSetOverflowFlagAfterLeftShift(ctx, result, last_msb);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// RCR r/m8, 1
// RCR r/m16, 1
// RCR r/m8, CL
// RCR r/m16, CL
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Rcr(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  const YCPUWidth width = ctx->metadata->width;
  // As with RCL, a whole number of cycles restores the value and the carry but
  // still leaves the overflow flag from the last pass.
  uint8_t effective_count = count % (YCPUNumBits[width] + 1);
  uint32_t value = YCPUFromOperand(op);
  uint32_t result = value;
  bool last_lsb = YCPUGetFlag(ctx->cpu, Y_CPU_CF);
  if (effective_count > 0) {
    uint32_t cf_value =
        last_lsb ? (YCPUSignBit[width] >> (effective_count - 1)) : 0;
    result = ((value >> effective_count) | cf_value |
              (value << (YCPUNumBits[width] - (effective_count - 1)))) &
             YCPUMaxValue[width];
    YCPUWriteOperand(ctx, op, result);
    last_lsb = ((value >> (effective_count - 1)) & 1) != 0;
  }
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, last_lsb);
  YCPUSetOverflowFlagAfterRightRotate(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// SETMO r/m8, 1
// SETMO r/m16, 1
// SETMOC r/m8, CL
// SETMOC r/m16, CL
//
// Undocumented. REG 6 of the shift group is a distinct operation on the
// 8086/8088 rather than an alias of SAL: it sets every bit of the operand,
// hence "set minus one". The count still gates it, so the CL forms - SETMOC,
// "set minus one conditional" - do nothing at all when CL is zero.
//
// Nothing an IBM PC/XT runs uses this, but leaving REG 6 aliased to SAL would
// silently give a different answer than the hardware for the same encoding,
// and the operation is a single store.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteGroup2Setmo(
    const YCPUInstructionContext* ctx, YCPUOperand* op, uint8_t count) {
  // Return early if count is 0, so as to not affect flags.
  if (count == 0) {
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  const uint32_t result = YCPUMaxValue[ctx->metadata->width];
  YCPUWriteOperand(ctx, op, result);
  YCPUSetFlag(ctx->cpu, Y_CPU_CF, false);
  YCPUSetFlag(ctx->cpu, Y_CPU_AF, false);
  YCPUSetFlag(ctx->cpu, Y_CPU_OF, false);
  YCPUSetCommonFlagsAfterInstruction(ctx, result);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

Y_FILE_PRIVATE const YCPUGroup2ExecuteInstructionFn
    YCPUGroup2ExecuteInstructionFns[] = {
        YCPUExecuteGroup2Rol,    // 0 - ROL
        YCPUExecuteGroup2Ror,    // 1 - ROR
        YCPUExecuteGroup2Rcl,    // 2 - RCL
        YCPUExecuteGroup2Rcr,    // 3 - RCR
        YCPUExecuteGroup2Shl,    // 4 - SHL
        YCPUExecuteGroup2Shr,    // 5 - SHR
        YCPUExecuteGroup2Setmo,  // 6 - SETMO / SETMOC
        YCPUExecuteGroup2Sar,    // 7 - SAR
};

// Group 2 shift / rotate by 1.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup2ShiftOrRotateBy1Instruction(
    const YCPUInstructionContext* ctx) {
  const YCPUGroup2ExecuteInstructionFn fn =
      YCPUGroup2ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand op;
  YCPUReadRegisterOrMemoryOperand(ctx, &op);
  return fn(ctx, &op, 1);
}

// Group 2 shift / rotate by CL.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup2ShiftOrRotateByCLInstruction(
    const YCPUInstructionContext* ctx) {
  const YCPUGroup2ExecuteInstructionFn fn =
      YCPUGroup2ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand op;
  YCPUReadRegisterOrMemoryOperand(ctx, &op);
  const uint8_t count = ctx->cpu->registers[Y_CPU_CX] & 0xFF;
  // A shift by CL works through the count a bit at a time.
  YCPUAddCycles(ctx->cpu, (uint16_t)count * Y_CPU_SHIFT_CYCLES_PER_BIT);
  return fn(ctx, &op, count);
}


// ==============================================================================
// src/cpu/instructions_group_2.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_group_3.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_group_3.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 3 - TEST, NOT, NEG, MUL, IMUL, DIV, IDIV
// ============================================================================

typedef YCPUInstructionResult (*YCPUGroup3ExecuteInstructionFn)(
    const YCPUInstructionContext* ctx, YCPUOperand* op);

// TEST r/m8, imm8
// TEST r/m16, imm16
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup3Test(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  YCPUOperandValue src_value = YCPUReadImmediate(ctx);
  return YCPUExecuteTest(ctx, op, src_value);
}

// NOT r/m8
// NOT r/m16
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteNot(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  YCPUWriteOperand(ctx, op, ~YCPUFromOperand(op));
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// NEG r/m8
// NEG r/m16
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteNeg(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  int32_t op_value = YCPUFromSignedOperand(ctx->metadata->width, op);
  int32_t result_value = -op_value;
  YCPUWriteOperand(ctx, op, result_value);
  YCPUSetFlagsAfterSub(ctx, 0, op_value, result_value, false);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// Table of where to store the higher half of the result for
// MUL, IMUL, DIV, and IDIV instructions, indexed by the data width.
Y_FILE_PRIVATE const YCPUOperandAddress
    YCPUMulDivResultHighHalfAddress[Y_CPU_NUM_WIDTHS] = {
        {.type = Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER,
         .register_index = Y_CPU_AX,
         .offset = 8},
        {.type = Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER,
         .register_index = Y_CPU_DX,
         .offset = 0},
};

// Number of bits to shift to extract the high part of the result of MUL, IMUL,
// DIV, and IDIV instructions, indexed by the data width.
Y_FILE_PRIVATE const uint8_t
    YCPUMulDivResultHighHalfShiftWidth[Y_CPU_NUM_WIDTHS] = {
        8,   // Y_CPU_BYTE
        16,  // Y_CPU_WORD
};

// What the multiply and divide instructions cost. These dominate their own
// timing by an order of magnitude - the 8086/8088 works through them a bit at
// a time in microcode - so unlike everything else in the cycle table they are
// worth charging individually. The published figures are ranges that depend on
// the operands; these are the low end of each.
Y_FILE_PRIVATE const uint16_t YCPUMulDivCycles[Y_CPU_NUM_WIDTHS][2] = {
    // Y_CPU_BYTE: unsigned, signed
    {70, 80},
    // Y_CPU_WORD: unsigned, signed
    {118, 128},
};

// Common logic for MUL and IMUL instructions.
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteMulCommon(
    const YCPUInstructionContext* ctx, YCPUOperand* dest, uint32_t result,
    bool overflow) {
  YCPUWidth width = ctx->metadata->width;

  uint32_t result_low_half = result & YCPUMaxValue[width];
  YCPUWriteOperand(ctx, dest, result_low_half);

  uint32_t result_high_half =
      (result >> YCPUMulDivResultHighHalfShiftWidth[width]) &
      YCPUMaxValue[width];
  YCPUWriteOperandAddress(
      ctx, &YCPUMulDivResultHighHalfAddress[width], result_high_half);

  YCPUSetFlag(ctx->cpu, Y_CPU_CF, overflow);
  YCPUSetFlag(ctx->cpu, Y_CPU_OF, overflow);

  return Y_CPU_INSTRUCTION_EXECUTED;
}

// MUL r/m8
// MUL r/m16
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteMul(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  YCPUAddCycles(ctx->cpu, YCPUMulDivCycles[ctx->metadata->width][0]);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  uint32_t result = YCPUFromOperand(&dest) * YCPUFromOperand(op);
  return YCPUExecuteMulCommon(
      ctx, &dest, result, result > YCPUMaxValue[ctx->metadata->width]);
}

// IMUL r/m8
// IMUL r/m16
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteImul(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  YCPUAddCycles(ctx->cpu, YCPUMulDivCycles[ctx->metadata->width][1]);
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);
  const YCPUWidth width = ctx->metadata->width;
  int32_t result =
      YCPUFromSignedOperand(width, &dest) * YCPUFromSignedOperand(width, op);
  return YCPUExecuteMulCommon(
      ctx, &dest, result,
      result > YCPUMaxSignedValue[ctx->metadata->width] ||
          result < YCPUMinSignedValue[ctx->metadata->width]);
}

Y_FILE_PRIVATE YCPUInstructionResult YCPUWriteDivResult(
    const YCPUInstructionContext* ctx, YCPUOperand* dest, uint32_t quotient,
    uint32_t remainder) {
  YCPUWriteOperand(ctx, dest, quotient);
  YCPUWriteOperandAddress(
      ctx, &YCPUMulDivResultHighHalfAddress[ctx->metadata->width], remainder);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// DIV r/m8
// DIV r/m16
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteDiv(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  YCPUAddCycles(ctx->cpu, YCPUMulDivCycles[ctx->metadata->width][0]);
  uint32_t divisor = YCPUFromOperand(op);
  if (divisor == 0) {
    YCPURaiseInternalInterrupt(ctx->cpu, Y_CPU_INTERRUPT_DIVIDE_ERROR);
    return Y_CPU_INSTRUCTION_EXECUTED;
  }

  YCPUWidth width = ctx->metadata->width;
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);

  YCPUOperandValue dest_high_half =
      YCPUReadOperandValue(ctx, &YCPUMulDivResultHighHalfAddress[width]);
  uint32_t dividend =
      YCPUFromOperand(&dest) | (YCPUFromOperandValue(dest_high_half)
                                << YCPUMulDivResultHighHalfShiftWidth[width]);
  uint32_t quotient = dividend / divisor;
  if (quotient > YCPUMaxValue[ctx->metadata->width]) {
    YCPURaiseInternalInterrupt(ctx->cpu, Y_CPU_INTERRUPT_DIVIDE_ERROR);
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  return YCPUWriteDivResult(ctx, &dest, quotient, dividend % divisor);
}

// IDIV r/m8
// IDIV r/m16
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteIdiv(const YCPUInstructionContext* ctx, YCPUOperand* op) {
  YCPUAddCycles(ctx->cpu, YCPUMulDivCycles[ctx->metadata->width][1]);
  int32_t divisor = YCPUFromSignedOperand(ctx->metadata->width, op);
  if (divisor == 0) {
    YCPURaiseInternalInterrupt(ctx->cpu, Y_CPU_INTERRUPT_DIVIDE_ERROR);
    return Y_CPU_INSTRUCTION_EXECUTED;
  }

  YCPUWidth width = ctx->metadata->width;
  YCPUOperand dest;
  YCPUReadRegisterOperandForRegisterIndex(ctx, Y_CPU_AX, &dest);

  YCPUOperandValue dest_high_half =
      YCPUReadOperandValue(ctx, &YCPUMulDivResultHighHalfAddress[width]);
  int32_t dividend = YCPUFromOperand(&dest) |
                     (YCPUFromSignedOperandValue(width, dest_high_half)
                      << YCPUMulDivResultHighHalfShiftWidth[width]);
  int32_t quotient = dividend / divisor;
  // The 8086/8088 divides the magnitudes and checks the result against the
  // largest quotient that fits, so it rejects a quotient of -128 or -32768
  // even though those are representable. The valid range is symmetric, one
  // narrower at the bottom than the operand's own range.
  if (quotient > YCPUMaxSignedValue[ctx->metadata->width] ||
      quotient < -YCPUMaxSignedValue[ctx->metadata->width]) {
    YCPURaiseInternalInterrupt(ctx->cpu, Y_CPU_INTERRUPT_DIVIDE_ERROR);
    return Y_CPU_INSTRUCTION_EXECUTED;
  }
  return YCPUWriteDivResult(ctx, &dest, quotient, dividend % divisor);
}

// Group 3 instruction implementations, indexed by the corresponding REG field
// value in the YCPUModRM byte and data width.
Y_FILE_PRIVATE const YCPUGroup3ExecuteInstructionFn
    YCPUGroup3ExecuteInstructionFns[] = {
        YCPUExecuteGroup3Test,  // 0 - TEST
        // REG 1 is an undocumented alias of REG 0: the 8086/8088 does not
        // decode bit 0 of the REG field for this group.
        YCPUExecuteGroup3Test,  // 1 - TEST
        YCPUExecuteNot,         // 2 - NOT
        YCPUExecuteNeg,         // 3 - NEG
        YCPUExecuteMul,         // 4 - MUL
        YCPUExecuteImul,        // 5 - IMUL
        YCPUExecuteDiv,         // 6 - DIV
        YCPUExecuteIdiv,        // 7 - IDIV
};

// Group 3 instruction handler.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup3Instruction(const YCPUInstructionContext* ctx) {
  const YCPUGroup3ExecuteInstructionFn fn =
      YCPUGroup3ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  return fn(ctx, &dest);
}


// ==============================================================================
// src/cpu/instructions_group_3.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_group_4.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_group_4.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 4 - INC, DEC
// ============================================================================

typedef YCPUInstructionResult (*YCPUGroup4ExecuteInstructionFn)(
    const YCPUInstructionContext* ctx, YCPUOperand* dest);

// Group 4 instruction implementations, indexed by the corresponding REG field
// value in the YCPUModRM byte.
Y_FILE_PRIVATE const YCPUGroup4ExecuteInstructionFn
    YCPUGroup4ExecuteInstructionFns[] = {
        YCPUExecuteInc,  // 0 - INC
        YCPUExecuteDec,  // 1 - DEC
};

enum {
  // Number of documented REG field values for this group.
  Y_CPU_NUM_GROUP4_INSTRUCTIONS = 2,
};

// Group 4 instruction handler.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup4Instruction(const YCPUInstructionContext* ctx) {
  // On real hardware REG 2-7 decode as byte-operand forms of the Group 5
  // instructions rather than being rejected. That behavior is deliberately not
  // emulated: it is unverifiable without hardware, and no assembler emits
  // these encodings. The bounds check matters regardless, because the REG
  // field is three bits wide and this table has only two entries.
  if (ctx->instruction->mod_rm.reg >= Y_CPU_NUM_GROUP4_INSTRUCTIONS) {
    return Y_CPU_INSTRUCTION_INVALID;
  }
  const YCPUGroup4ExecuteInstructionFn fn =
      YCPUGroup4ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  return fn(ctx, &dest);
}


// ==============================================================================
// src/cpu/instructions_group_4.c end
// ==============================================================================

// ==============================================================================
// src/cpu/instructions_group_5.c start
// ==============================================================================

#line 1 "./src/cpu/instructions_group_5.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Group 5 - INC, DEC, CALL, JMP, PUSH
// ============================================================================

// Helper to get the segment register value for far JMP and CALL instructions.
Y_FILE_PRIVATE void YCPUGetSegmentRegisterOperandForIndirectFarJumpOrCall(
    const YCPUInstructionContext* ctx, const YCPUOperand* offset,
    YCPUOperand* operand) {
  YCPUOperandAddress segment_address = offset->address;
  segment_address.offset += 2;  // Skip the offset
  operand->address = segment_address;
  operand->value = YCPUReadMemoryOperandWord(ctx->cpu, &segment_address);
}

// JMP ptr16
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteIndirectNearJump(
    const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  ctx->cpu->registers[Y_CPU_IP] = YCPUFromOperandValue(dest->value);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

// CALL ptr16
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteIndirectNearCall(
    const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUPushValue(ctx->cpu, ctx->cpu->registers[Y_CPU_IP]);
  return YCPUExecuteIndirectNearJump(ctx, dest);
}

// CALL ptr16:16
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteIndirectFarCall(
    const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUOperand segment;
  YCPUGetSegmentRegisterOperandForIndirectFarJumpOrCall(ctx, dest, &segment);
  return YCPUExecuteFarCall(ctx, segment.value, dest->value);
}

// JMP ptr16:16
Y_FILE_PRIVATE YCPUInstructionResult YCPUExecuteIndirectFarJump(
    const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUOperand segment;
  YCPUGetSegmentRegisterOperandForIndirectFarJumpOrCall(ctx, dest, &segment);
  return YCPUExecuteFarJump(ctx, segment.value, dest->value);
}

// PUSH r/m16
Y_FILE_PRIVATE YCPUInstructionResult
YCPUExecuteIndirectPush(const YCPUInstructionContext* ctx, YCPUOperand* dest) {
  YCPUPushSourceOperand(ctx->cpu, dest);
  return Y_CPU_INSTRUCTION_EXECUTED;
}

typedef YCPUInstructionResult (*YCPUGroup5ExecuteInstructionFn)(
    const YCPUInstructionContext* ctx, YCPUOperand* dest);

// Group 5 instruction implementations, indexed by the corresponding REG
// field value in the YCPUModRM byte.
Y_FILE_PRIVATE const YCPUGroup5ExecuteInstructionFn
    YCPUGroup5ExecuteInstructionFns[] = {
        YCPUExecuteInc,               // 0 - INC r/m8/r/m16
        YCPUExecuteDec,               // 1 - DEC r/m8/r/m16
        YCPUExecuteIndirectNearCall,  // 2 - CALL rel16
        YCPUExecuteIndirectFarCall,   // 3 - CALL ptr16:16
        YCPUExecuteIndirectNearJump,  // 4 - JMP ptr16
        YCPUExecuteIndirectFarJump,   // 5 - JMP ptr16:16
        YCPUExecuteIndirectPush,      // 6 - PUSH r/m16
        // REG 7 is an undocumented alias of REG 6. Without this entry the
        // lookup below would read past the end of the table, since the REG
        // field is three bits wide.
        YCPUExecuteIndirectPush,  // 7 - PUSH r/m16
};

// Group 5 instruction handler.
Y_MODULE_PRIVATE YCPUInstructionResult
YCPUExecuteGroup5Instruction(const YCPUInstructionContext* ctx) {
  const YCPUGroup5ExecuteInstructionFn fn =
      YCPUGroup5ExecuteInstructionFns[ctx->instruction->mod_rm.reg];
  YCPUOperand dest;
  YCPUReadRegisterOrMemoryOperand(ctx, &dest);
  return fn(ctx, &dest);
}


// ==============================================================================
// src/cpu/instructions_group_5.c end
// ==============================================================================

// ==============================================================================
// src/cpu/opcode_table.c start
// ==============================================================================

#line 1 "./src/cpu/opcode_table.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "instructions.h"
#include "operands.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Opcode table
// ============================================================================

// Global opcode metadata lookup table.
//
// Const because nothing writes it, which on a target that executes from flash
// keeps 2KB of it out of RAM.
//
// base_cycles is part 1 of the cycle model, and is only half of what an
// instruction costs before it runs - cycles.c has the other half and the
// rules these figures were derived under, which is where to read them
// against.
Y_MODULE_PRIVATE const YCPUOpcodeMetadata YCPUOpcodeTable[256] = {
    // ADD r/m8, r8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteAddRegisterToRegisterOrMemory},
    // ADD r/m16, r16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteAddRegisterToRegisterOrMemory},
    // ADD r8, r/m8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteAddRegisterOrMemoryToRegister},
    // ADD r16, r/m16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteAddRegisterOrMemoryToRegister},
    // ADD AL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteAddImmediateToALOrAX},
    // ADD AX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteAddImmediateToALOrAX},
    // PUSH ES
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushSegmentRegister},
    // POP ES
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopSegmentRegister},
    // OR r/m8, r8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteBooleanOrRegisterToRegisterOrMemory},
    // OR r/m16, r16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteBooleanOrRegisterToRegisterOrMemory},
    // OR r8, r/m8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteBooleanOrRegisterOrMemoryToRegister},
    // OR r16, r/m16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteBooleanOrRegisterOrMemoryToRegister},
    // OR AL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteBooleanOrImmediateToALOrAX},
    // OR AX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteBooleanOrImmediateToALOrAX},
    // PUSH CS
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushSegmentRegister},
    // POP CS
    //
    // Undocumented, but real: on the 8086/8088 the segment register field of a
    // POP sreg is only two bits wide, so 0x0F decodes as POP CS. Later x86
    // parts repurposed 0x0F as the two-byte opcode prefix.
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopSegmentRegister},
    // ADC r/m8, r8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteAddRegisterToRegisterOrMemoryWithCarry},
    // ADC r/m16, r16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteAddRegisterToRegisterOrMemoryWithCarry},
    // ADC r8, r/m8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteAddRegisterOrMemoryToRegisterWithCarry},
    // ADC r16, r/m16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteAddRegisterOrMemoryToRegisterWithCarry},
    // ADC AL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteAddImmediateToALOrAXWithCarry},
    // ADC AX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteAddImmediateToALOrAXWithCarry},
    // PUSH SS
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushSegmentRegister},
    // POP SS
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopSegmentRegister},
    // SBB r/m8, r8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSubRegisterFromRegisterOrMemoryWithBorrow},
    // SBB r/m16, r16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteSubRegisterFromRegisterOrMemoryWithBorrow},
    // SBB r8, r/m8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSubRegisterOrMemoryFromRegisterWithBorrow},
    // SBB r16, r/m16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteSubRegisterOrMemoryFromRegisterWithBorrow},
    // SBB AL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSubImmediateFromALOrAXWithBorrow},
    // SBB AX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteSubImmediateFromALOrAXWithBorrow},
    // PUSH DS
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushSegmentRegister},
    // POP DS
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopSegmentRegister},
    // AND r/m8, r8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteBooleanAndRegisterToRegisterOrMemory},
    // AND r/m16, r16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteBooleanAndRegisterToRegisterOrMemory},
    // AND r8, r/m8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteBooleanAndRegisterOrMemoryToRegister},
    // AND r16, r/m16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteBooleanAndRegisterOrMemoryToRegister},
    // AND AL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteBooleanAndImmediateToALOrAX},
    // AND AX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteBooleanAndImmediateToALOrAX},
    // ES prefix - 0x26
    {.base_cycles = 2, .handler = YCPUExecuteInvalidOpcode},
    // DAA
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteDaa},
    // SUB r/m8, r8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSubRegisterFromRegisterOrMemory},
    // SUB r/m16, r16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteSubRegisterFromRegisterOrMemory},
    // SUB r8, r/m8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSubRegisterOrMemoryFromRegister},
    // SUB r16, r/m16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteSubRegisterOrMemoryFromRegister},
    // SUB AL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSubImmediateFromALOrAX},
    // SUB AX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteSubImmediateFromALOrAX},
    // CS prefix - 0x2E
    {.base_cycles = 2, .handler = YCPUExecuteInvalidOpcode},
    // DAS
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteDas},
    // XOR r/m8, r8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteBooleanXorRegisterToRegisterOrMemory},
    // XOR r/m16, r16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteBooleanXorRegisterToRegisterOrMemory},
    // XOR r8, r/m8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteBooleanXorRegisterOrMemoryToRegister},
    // XOR r16, r/m16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteBooleanXorRegisterOrMemoryToRegister},
    // XOR AL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteBooleanXorImmediateToALOrAX},
    // XOR AX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteBooleanXorImmediateToALOrAX},
    // SS prefix - 0x36
    {.base_cycles = 2, .handler = YCPUExecuteInvalidOpcode},
    // AAA
    {.base_cycles = 8,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteAaa},
    // CMP r/m8, r8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteCmpRegisterToRegisterOrMemory},
    // CMP r/m16, r16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteCmpRegisterToRegisterOrMemory},
    // CMP r8, r/m8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteCmpRegisterOrMemoryToRegister},
    // CMP r16, r/m16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteCmpRegisterOrMemoryToRegister},
    // CMP AL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteCmpImmediateToALOrAX},
    // CMP AX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteCmpImmediateToALOrAX},
    // DS prefix - 0x3E
    {.base_cycles = 2, .handler = YCPUExecuteInvalidOpcode},
    // AAS
    {.base_cycles = 8,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteAas},
    // INC AX
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteIncRegister},
    // INC CX
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteIncRegister},
    // INC DX
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteIncRegister},
    // INC BX
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteIncRegister},
    // INC SP
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteIncRegister},
    // INC BP
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteIncRegister},
    // INC SI
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteIncRegister},
    // INC DI
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteIncRegister},
    // DEC AX
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDecRegister},
    // DEC CX
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDecRegister},
    // DEC DX
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDecRegister},
    // DEC BX
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDecRegister},
    // DEC SP
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDecRegister},
    // DEC BP
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDecRegister},
    // DEC SI
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDecRegister},
    // DEC DI
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDecRegister},
    // PUSH AX
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushRegister},
    // PUSH CX
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushRegister},
    // PUSH DX
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushRegister},
    // PUSH BX
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushRegister},
    // PUSH SP
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushRegister},
    // PUSH BP
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushRegister},
    // PUSH SI
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushRegister},
    // PUSH DI
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushRegister},
    // POP AX
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopRegister},
    // POP CX
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopRegister},
    // POP DX
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopRegister},
    // POP BX
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopRegister},
    // POP SP
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopRegister},
    // POP BP
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopRegister},
    // POP SI
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopRegister},
    // POP DI
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopRegister},
    // 0x60 - 0x6F - UNSUPPORTED
    // 0x60-0x6F - conditional jumps, aliases of 0x70-0x7F
    //
    // Undocumented, but real: the 8086/8088 decoder ignores bit 4 of these
    // opcodes, so each one behaves exactly like its 0x7X counterpart. They are
    // two bytes wide, so decoding them as unknown single-byte opcodes would
    // desynchronize the instruction stream.
    // JO rel8 (alias of 0x70)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNO rel8 (alias of 0x71)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JB/JNAE/JC rel8 (alias of 0x72)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNB/JAE/JNC rel8 (alias of 0x73)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JE/JZ rel8 (alias of 0x74)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNE/JNZ rel8 (alias of 0x75)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JBE/JNA rel8 (alias of 0x76)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNBE/JA rel8 (alias of 0x77)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JS rel8 (alias of 0x78)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNS rel8 (alias of 0x79)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JP/JPE rel8 (alias of 0x7A)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNP/JPO rel8 (alias of 0x7B)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JL/JNGE rel8 (alias of 0x7C)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSignedConditionalJumpJLOrJNL},
    // JNL/JGE rel8 (alias of 0x7D)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSignedConditionalJumpJLOrJNL},
    // JLE/JNG rel8 (alias of 0x7E)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSignedConditionalJumpJLEOrJNLE},
    // JNLE/JG rel8 (alias of 0x7F)
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSignedConditionalJumpJLEOrJNLE},
    // JO rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNO rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JB/JNAE/JC rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNB/JAE/JNC rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JE/JZ rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNE/JNZ rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JBE/JNA rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNBE/JA rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JS rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNS rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JP/JPE rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JNP/JPO rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteUnsignedConditionalJump},
    // JL/JNGE rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSignedConditionalJumpJLOrJNL},
    // JNL/JGE rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSignedConditionalJumpJLOrJNL},
    // JLE/JNG rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSignedConditionalJumpJLEOrJNLE},
    // JNLE/JG rel8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSignedConditionalJumpJLEOrJNLE},
    // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m8, imm8 (Group 1)
    {.base_cycles = 4,
     .has_modrm = true,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteGroup1Instruction},
    // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m16, imm16 (Group 1)
    {.base_cycles = 4,
     .has_modrm = true,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteGroup1Instruction},
    // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m8, imm8 (Group 1)
    {.base_cycles = 4,
     .has_modrm = true,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteGroup1Instruction},
    // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m16, imm8 (Group 1)
    {.base_cycles = 4,
     .has_modrm = true,
     // This is a special case - the immediate is 8 bits but the destination is
     // 16 bits.
     .immediate_size = 1,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteGroup1InstructionWithSignExtension},
    // TEST r/m8, r8
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteTestRegisterToRegisterOrMemory},
    // TEST r/m16, r16
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteTestRegisterToRegisterOrMemory},
    // XCHG r/m8, r8
    {.base_cycles = 4,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteExchangeRegisterOrMemory},
    // XCHG r/m16, r16
    {.base_cycles = 4,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteExchangeRegisterOrMemory},
    // MOV r/m8, r8
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveRegisterToRegisterOrMemory},
    // MOV r/m16, r16
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveRegisterToRegisterOrMemory},
    // MOV r8, r/m8
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveRegisterOrMemoryToRegister},
    // MOV r16, r/m16
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveRegisterOrMemoryToRegister},
    // MOV r/m16, sreg
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveSegmentRegisterToRegisterOrMemory},
    // LEA r16, m
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteLoadEffectiveAddress},
    // MOV sreg, r/m16
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveRegisterOrMemoryToSegmentRegister},
    // POP r/m16
    {.base_cycles = 0,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopRegisterOrMemory},
    // XCHG AX, AX (NOP)
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteExchangeRegister},
    // XCHG AX, CX
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteExchangeRegister},
    // XCHG AX, DX
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteExchangeRegister},
    // XCHG AX, BX
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteExchangeRegister},
    // XCHG AX, SP
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteExchangeRegister},
    // XCHG AX, BP
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteExchangeRegister},
    // XCHG AX, SI
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteExchangeRegister},
    // XCHG AX, DI
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteExchangeRegister},
    // CBW
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteCbw},
    // CWD
    {.base_cycles = 5,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteCwd},
    // CALL ptr16:16 (4 bytes: 2 for offset, 2 for segment)
    {.base_cycles = 12,
     .has_modrm = false,
     .immediate_size = 4,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDirectFarCall},
    // WAIT
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteNoOp},
    // PUSHF
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePushFlags},
    // POPF
    {.base_cycles = 0,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecutePopFlags},
    // SAHF
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteStoreAHToFlags},
    // LAHF
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteLoadAHFromFlags},
    // MOV AL, moffs16
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveMemoryOffsetToALOrAX},
    // MOV AX, moffs16
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveMemoryOffsetToALOrAX},
    // MOV moffs16, AL
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveALOrAXToMemoryOffset},
    // MOV moffs16, AX
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveALOrAXToMemoryOffset},
    // MOVSB
    {.base_cycles = 10,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMovs},
    // MOVSW
    {.base_cycles = 10,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMovs},
    // CMPSB
    {.base_cycles = 14,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteCmps},
    // CMPSW
    {.base_cycles = 14,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteCmps},
    // TEST AL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteTestImmediateToALOrAX},
    // TEST AX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteTestImmediateToALOrAX},
    // STOSB
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteStos},
    // STOSW
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteStos},
    // LODSB
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteLods},
    // LODSW
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteLods},
    // SCASB
    {.base_cycles = 7,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteScas},
    // SCASW
    {.base_cycles = 7,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteScas},
    // MOV AL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV CL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV DL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV BL, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV AH, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV CH, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV DH, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV BH, imm8
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV AX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV CX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV DX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV BX, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV SP, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV BP, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV SI, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // MOV DI, imm16
    {.base_cycles = 4,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveImmediateToRegister},
    // RET imm16 (alias of 0xC2)
    //
    // Undocumented, but real: the 8086/8088 decoder ignores bit 1 of these
    // opcodes. 0xC0 takes a 16-bit immediate, so decoding it as an unknown
    // single-byte opcode would desynchronize the instruction stream.
    {.base_cycles = 12,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteNearReturnAndPop},
    // RET (alias of 0xC3)
    {.base_cycles = 8,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteNearReturn},
    // RET imm16
    {.base_cycles = 12,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteNearReturnAndPop},
    // RET
    {.base_cycles = 8,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteNearReturn},
    // LES r16, m32
    {.base_cycles = 8,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteLoadESWithPointer},
    // LDS r16, m32
    {.base_cycles = 8,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteLoadDSWithPointer},
    // MOV r/m8, imm8
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteMoveImmediateToRegisterOrMemory},
    // MOV r/m16, imm16
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteMoveImmediateToRegisterOrMemory},
    // RETF imm16 (alias of 0xCA)
    {.base_cycles = 9,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteFarReturnAndPop},
    // RETF (alias of 0xCB)
    {.base_cycles = 10,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteFarReturn},
    // RETF imm16
    {.base_cycles = 9,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteFarReturnAndPop},
    // RETF
    {.base_cycles = 10,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteFarReturn},
    // INT 3
    {.base_cycles = 1,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteInt3},
    // INT imm8
    {.base_cycles = 1,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteIntN},
    // INTO
    {.base_cycles = 3,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteInto},
    // IRET
    {.base_cycles = 32,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteIret},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m8, 1 (Group 2)
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteGroup2ShiftOrRotateBy1Instruction},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m16, 1 (Group 2)
    {.base_cycles = 2,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteGroup2ShiftOrRotateBy1Instruction},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m8, CL (Group 2)
    {.base_cycles = 8,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteGroup2ShiftOrRotateByCLInstruction},
    // ROL/ROR/RCL/RCR/SHL/SHR/SAR r/m16, CL (Group 2)
    {.base_cycles = 8,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteGroup2ShiftOrRotateByCLInstruction},
    // AAM
    {.base_cycles = 8,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteAam},
    // AAD
    {.base_cycles = 8,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteAad},
    // SALC - Set AL from Carry
    //
    // Undocumented, but real and stable across x86 generations: sets AL to
    // 0xFF if CF is set and 0x00 otherwise. Affects no flags.
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteSetALFromCarry},
    // XLAT/XLATB
    {.base_cycles = 11,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteTranslateByte},
    // ESC instruction 0xD8 for 8087 numeric coprocessor
    {.base_cycles = 0,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = YCPUExecuteNoOp},
    // ESC instruction 0xD9 for 8087 numeric coprocessor
    {.base_cycles = 0,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = YCPUExecuteNoOp},
    // ESC instruction 0xDA for 8087 numeric coprocessor
    {.base_cycles = 0,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = YCPUExecuteNoOp},
    // ESC instruction 0xDB for 8087 numeric coprocessor
    {.base_cycles = 0,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = YCPUExecuteNoOp},
    // ESC instruction 0xDC for 8087 numeric coprocessor
    {.base_cycles = 0,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = YCPUExecuteNoOp},
    // ESC instruction 0xDD for 8087 numeric coprocessor
    {.base_cycles = 0,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = YCPUExecuteNoOp},
    // ESC instruction 0xDE for 8087 numeric coprocessor
    {.base_cycles = 0,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = YCPUExecuteNoOp},
    // ESC instruction 0xDF for 8087 numeric coprocessor
    {.base_cycles = 0,
     .has_modrm = true,
     .immediate_size = 0,
     .handler = YCPUExecuteNoOp},
    // LOOPNE/LOOPNZ rel8
    {.base_cycles = 5,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteLoopZOrNZ},
    // LOOPE/LOOPZ rel8
    {.base_cycles = 6,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteLoopZOrNZ},
    // LOOP rel8
    {.base_cycles = 6,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteLoop},
    // JCXZ rel8
    {.base_cycles = 6,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteJumpIfCXIsZero},
    // IN AL, imm8
    {.base_cycles = 10,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteInImmediate},
    // IN AX, imm8
    {.base_cycles = 10,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteInImmediate},
    // OUT imm8, AL
    {.base_cycles = 10,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteOutImmediate},
    // OUT imm8, AX
    {.base_cycles = 10,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteOutImmediate},
    // CALL rel16
    {.base_cycles = 11,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDirectNearCall},
    // JMP rel16
    {.base_cycles = 15,
     .has_modrm = false,
     .immediate_size = 2,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteShortOrNearJump},
    // JMP ptr16:16 (4 bytes: 2 for offset, 2 for segment)
    {.base_cycles = 15,
     .has_modrm = false,
     .immediate_size = 4,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteDirectFarJump},
    // JMP rel8
    {.base_cycles = 15,
     .has_modrm = false,
     .immediate_size = 1,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteShortOrNearJump},
    // IN AL, DX
    {.base_cycles = 8,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteInDX},
    // IN AX, DX
    {.base_cycles = 8,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteInDX},
    // OUT DX, AL
    {.base_cycles = 8,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteOutDX},
    // OUT DX, AX
    {.base_cycles = 8,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteOutDX},
    // 0xF0 - LOCK prefix
    {.base_cycles = 2, .handler = YCPUExecuteInvalidOpcode},
    // LOCK prefix (alias of 0xF0) - consumed by the prefix decoder.
    {.base_cycles = 2, .handler = YCPUExecuteInvalidOpcode},
    // 0xF2 - REPNE prefix
    {.base_cycles = 2, .handler = YCPUExecuteInvalidOpcode},
    // 0xF3 - REP/REPE prefix
    {.base_cycles = 2, .handler = YCPUExecuteInvalidOpcode},
    // HLT
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteHlt},
    // CMC
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .handler = YCPUExecuteComplementCarryFlag},
    // TEST/NOT/NEG/MUL/IMUL/DIV/IDIV r/m8 (Group 3)
    // The immediate size depends on the ModR/M byte.
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteGroup3Instruction},
    // TEST/NOT/NEG/MUL/IMUL/DIV/IDIV r/m16 (Group 3)
    // The immediate size depends on the ModR/M byte.
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteGroup3Instruction},
    // CLC
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteClearOrSetFlag},
    // STC
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteClearOrSetFlag},
    // CLI
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteClearOrSetFlag},
    // STI
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteClearOrSetFlag},
    // CLD
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteClearOrSetFlag},
    // STD
    {.base_cycles = 2,
     .has_modrm = false,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteClearOrSetFlag},
    // INC/DEC r/m8 (Group 4)
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_BYTE,
     .handler = YCPUExecuteGroup4Instruction},
    // INC/DEC/CALL/JMP/PUSH r/m16 (Group 5)
    {.base_cycles = 3,
     .has_modrm = true,
     .immediate_size = 0,
     .width = Y_CPU_WORD,
     .handler = YCPUExecuteGroup5Instruction},
};


// ==============================================================================
// src/cpu/opcode_table.c end
// ==============================================================================

// ==============================================================================
// src/cpu/cpu.c start
// ==============================================================================

#line 1 "./src/cpu/cpu.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "cycles.h"
#include "instructions.h"
#include "operands.h"
#include "public.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

#define Y_CPU_ERROR(...) \
  Y_LOG(cpu->config.logger, &YCPULogModule, Y_LOG_LEVEL_ERROR, __VA_ARGS__)
#define Y_CPU_WARN(...) \
  Y_LOG(cpu->config.logger, &YCPULogModule, Y_LOG_LEVEL_WARN, __VA_ARGS__)
#define Y_CPU_INFO(...) \
  Y_LOG(cpu->config.logger, &YCPULogModule, Y_LOG_LEVEL_INFO, __VA_ARGS__)
#define Y_CPU_DEBUG(...) \
  Y_LOG(cpu->config.logger, &YCPULogModule, Y_LOG_LEVEL_DEBUG, __VA_ARGS__)

// ============================================================================
// CPU state
// ============================================================================

Y_PUBLIC void YCPUInit(YCPUState* cpu) {
  cpu->flags = Y_CPU_INITIAL_FLAGS;

  // The only place the count is read, so a host gets told here or not at all.
  const uint32_t num_entries = cpu->config.decode_cache_num_entries;
  if (cpu->config.decode_cache == NULL) {
    return;
  }
  if (num_entries == 0 || (num_entries & (num_entries - 1)) != 0) {
    // The CPU runs without a cache.
    Y_CPU_ERROR(
        "decode_cache_size_not_power_of_two",
        Y_LOG_UINT("entries", num_entries));
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
  Y_CPU_SEGMENT_OVERRIDE_PREFIX_MASK = 0xE7,
  Y_CPU_SEGMENT_OVERRIDE_PREFIX_VALUE = 0x26,
  // Position of the ss field within a segment override prefix.
  Y_CPU_SEGMENT_OVERRIDE_PREFIX_SHIFT = 3,
  Y_CPU_SEGMENT_OVERRIDE_PREFIX_SEGMENT_MASK = 0x03,

  // Within the LOCK and repetition prefix group, bit 1 separates the
  // repetition prefixes (REPNZ, REP) from LOCK and its undocumented 0xF1
  // alias.
  Y_CPU_REPETITION_PREFIX_BIT = 0x02,
};

// Whether a byte is a segment override prefix. The mask pins every bit but the
// two that select the segment, so it matches those four bytes and nothing else.
Y_FILE_PRIVATE inline bool YCPUIsSegmentOverridePrefix(uint8_t byte) {
  return (byte & Y_CPU_SEGMENT_OVERRIDE_PREFIX_MASK) ==
         Y_CPU_SEGMENT_OVERRIDE_PREFIX_VALUE;
}

// Whether a byte is a LOCK or repetition prefix. These four are consecutive,
// so this is a range check rather than a mask - which is both clearer and one
// instruction cheaper, since a compiler folds it into a single subtract and
// compare.
Y_FILE_PRIVATE inline bool YCPUIsLockOrRepetitionPrefix(uint8_t byte) {
  return byte >= Y_CPU_PREFIX_LOCK && byte <= Y_CPU_PREFIX_REP;
}

// Record what a prefix byte selects, and report whether it was a prefix at
// all. Deciding which group a byte belongs to is the same test as deciding
// whether it is a prefix, so both happen here rather than the caller asking
// first and this asking again.
//
// The cheaper test goes first. A byte that is not a prefix runs both, and that
// is the common case by far - every instruction ends the loop with one.
Y_FILE_PRIVATE bool YCPUApplyPrefixByte(
    YCPUInstruction* instruction, uint8_t byte) {
  if (YCPUIsLockOrRepetitionPrefix(byte)) {
    // LOCK and its 0xF1 alias advance IP, but nothing acts on them, so only a
    // repetition prefix is worth recording.
    if (byte & Y_CPU_REPETITION_PREFIX_BIT) {
      instruction->repetition_prefix = byte;
    }
    return true;
  }
  if (YCPUIsSegmentOverridePrefix(byte)) {
    // The segment field is in the 8086's sreg encoding order, which is the
    // order Y_CPU_ES through Y_CPU_DS are numbered in, so the register index is
    // an offset from Y_CPU_ES.
    instruction->segment_override =
        (uint8_t)(Y_CPU_ES + ((byte >> Y_CPU_SEGMENT_OVERRIDE_PREFIX_SHIFT) &
                              Y_CPU_SEGMENT_OVERRIDE_PREFIX_SEGMENT_MASK));
    return true;
  }
  return false;
}

// Where one call to YCPUFetchNextInstruction() has got to.
//
// Scratch for a single decode, and nothing more. What survives between
// instructions is the window itself, in YCPUState.instruction_fetch_window;
// this is the cursor walking it, and it is thrown away when the instruction
// has been decoded.
//
// It is a struct because YCPUFetchNextInstructionByte() has to advance the
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
//     being decoded until YCPUTick() advances it by instruction.size - so a
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
typedef struct YCPUInstructionFetchState {
  // The offset within CS of the next byte to read.
  uint16_t next_byte_offset;
  // The next byte to read, and how many may be read directly from it. NULL and
  // zero where there is no window.
  const uint8_t* next_byte;
  uint32_t bytes_remaining;
} YCPUInstructionFetchState;

// Reads the next instruction byte, from the window where one is open and from
// the memory map where one is not.
//
// Either way the read goes straight to memory rather than through
// YCPUReadMemoryOperandByte, so that the fetch is not charged for time on the
// data bus. The 8088 fetches ahead into a queue while the previous instruction
// executes, so most of that time is already paid for by the instruction being
// executed - and the published per-instruction figures the cycle table is
// built from assume the queue is full.
Y_FILE_PRIVATE inline Y_HOT uint8_t YCPUFetchNextInstructionByte(
    YCPUState* cpu, YCPUInstructionFetchState* fetch_state) {
  if (fetch_state->bytes_remaining > 0) {
    --fetch_state->bytes_remaining;
    ++fetch_state->next_byte_offset;
    return *fetch_state->next_byte++;
  }
  return YCPUReadRawMemoryByte(
      cpu, YCPUToRawAddress(cpu, Y_CPU_CS, fetch_state->next_byte_offset++));
}

// If an address is paged and its page is in a frame, points the fetch window
// at that frame and returns true. The window covers only that one page,
// because the next page's frame can be anywhere, so running or jumping onto
// another page comes back here. Doing this without asking the host saves a
// call and a memory map lookup each time.
Y_FILE_PRIVATE Y_HOT Y_NOINLINE bool YCPUOpenPagedFetchWindow(
    YCPUState* cpu, uint32_t raw_address) {
  const YCPUPagedDataWindow* paged = &cpu->paged_data_window;
  const uint32_t offset = raw_address - paged->start;
  if (offset >= paged->size) {
    return false;
  }
  const uint16_t entry = paged->pages[offset >> paged->page_shift];
  if (!(entry & Y_CPU_PAGED_DATA_RESIDENT)) {
    return false;
  }
  const uint32_t page_size = (uint32_t)1 << paged->page_shift;
  YCPUInstructionFetchWindow* window = &cpu->instruction_fetch_window;
  window->data =
      paged->frames +
      ((uint32_t)(entry & Y_CPU_PAGED_DATA_FRAME_MASK) << paged->page_shift);
  window->start = paged->start + (offset & ~(page_size - 1));
  window->end = window->start + page_size;
  return true;
}

// Points a fetch at whatever can be read directly from CS:ip.
Y_FILE_PRIVATE Y_HOT void YCPUInitInstructionFetchState(
    YCPUState* cpu, uint16_t ip, YCPUInstructionFetchState* fetch_state) {
  fetch_state->next_byte_offset = ip;
  fetch_state->next_byte = NULL;
  fetch_state->bytes_remaining = 0;

  // Note that there is deliberately no early out for a host that supplies no
  // get_instruction_fetch_window, even though everything below is wasted on
  // one. Such a host reads every byte through an indirect call already, so it
  // would save a handful of arithmetic against several calls - and the test to
  // skip it would be paid by the hosts that do supply one, which is every host
  // that cares about the speed. Measured on x86-64, adding it grew
  // YCPUFetchNextInstruction by 11 bytes.
  const uint32_t raw_address = YCPUToRawAddress(cpu, Y_CPU_CS, ip);

  const YCPUInstructionFetchWindow* const window =
      &cpu->instruction_fetch_window;
  if (window->data == NULL || raw_address < window->start ||
      raw_address >= window->end) {
    // Nothing open covers this address, so open a window that does: its page's
    // frame if it is paged and in one, and otherwise whatever the host offers.
    // The host fills one in, or sets its data to NULL to decline.
    if (YCPUOpenPagedFetchWindow(cpu, raw_address)) {
      // Opened.
    } else if (cpu->config.get_instruction_fetch_window != NULL) {
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
  const uint32_t bytes_until_wrap = (uint32_t)Y_CPU_SEGMENT_SIZE - (uint32_t)ip;
  if (fetch_state->bytes_remaining > bytes_until_wrap) {
    fetch_state->bytes_remaining = bytes_until_wrap;
  }
}

// Returns the number of displacement bytes based on the ModR/M byte.
Y_FILE_PRIVATE uint8_t YCPUGetDisplacementSize(uint8_t mod, uint8_t rm) {
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
Y_FILE_PRIVATE uint8_t YCPUGetImmediateSize(
    const YCPUOpcodeMetadata* metadata, uint8_t opcode, uint8_t reg) {
  switch (opcode) {
    // TEST r/m8, imm8
    case 0xF6:
    // TEST r/m16, imm16
    case 0xF7:
      // REG 0 and REG 1 are both TEST, which carries an immediate; the other
      // REG values do not. The 8086/8088 does not decode bit 0 of the REG
      // field here, which is what makes REG 1 an alias of REG 0.
      return reg <= 1 ? opcode - 0xF5 : 0;
    default:
      return metadata->immediate_size;
  }
}

Y_PUBLIC Y_HOT YCPUFetchNextInstructionStatus
YCPUFetchNextInstruction(YCPUState* cpu, YCPUInstruction* instruction) {
  // The prefix fields, which YCPUApplyPrefixByte() writes only where a prefix
  // is actually present. Every other field a decode could leave behind is
  // settled where it becomes known rather than here: opcode, size and
  // immediate_size are assigned on every path that returns success, has_mod_rm
  // and displacement_size in both arms of the ModR/M branch below, mod_rm is
  // read only where has_mod_rm is set, and the displacement and immediate
  // arrays are read no further than their size fields say.
  instruction->segment_override = Y_CPU_NO_SEGMENT_OVERRIDE;
  instruction->repetition_prefix = 0;

  uint8_t current_byte;
  const uint16_t original_ip = cpu->registers[Y_CPU_IP];
  YCPUInstructionFetchState fetch_state;
  YCPUInitInstructionFetchState(cpu, original_ip, &fetch_state);

  // Prefix
  //
  // The count is local because nothing outside this loop wants it: what the
  // prefixes selected is in the instruction, and the bytes they occupied are
  // in its size. It exists only to stop a run of prefix bytes from fetching
  // forever - see Y_CPU_MAX_PREFIX_BYTES.
  uint8_t prefix_size = 0;
  current_byte = YCPUFetchNextInstructionByte(cpu, &fetch_state);
  while (YCPUApplyPrefixByte(instruction, current_byte)) {
    if (++prefix_size > Y_CPU_MAX_PREFIX_BYTES) {
      return Y_CPU_FETCH_PREFIX_TOO_LONG;
    }
    current_byte = YCPUFetchNextInstructionByte(cpu, &fetch_state);
  }

  // Opcode
  instruction->opcode = current_byte;
  const YCPUOpcodeMetadata* metadata = &YCPUOpcodeTable[instruction->opcode];

  // ModR/M
  //
  // The REG field is kept in a local, because the immediate size below is
  // computed whether or not the instruction carries a ModR/M byte.
  uint8_t reg = 0;
  if (metadata->has_modrm) {
    uint8_t mod_rm_byte = YCPUFetchNextInstructionByte(cpu, &fetch_state);
    reg = (mod_rm_byte >> 3) & 0x07;  // Bits 3-5
    instruction->has_mod_rm = true;
    instruction->mod_rm.mod = (mod_rm_byte >> 6) & 0x03;  // Bits 6-7
    instruction->mod_rm.reg = reg;
    instruction->mod_rm.rm = mod_rm_byte & 0x07;  // Bits 0-2

    // Displacement
    const uint8_t displacement_size = YCPUGetDisplacementSize(
        instruction->mod_rm.mod, instruction->mod_rm.rm);
    instruction->displacement_size = displacement_size;
    for (uint8_t i = 0; i < displacement_size; ++i) {
      instruction->displacement[i] =
          YCPUFetchNextInstructionByte(cpu, &fetch_state);
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
  // No entry in the opcode table exceeds the immediate[] array - the widest is
  // the 4 of a far pointer - but a compiler that cannot see it is right to warn
  // about the writes below. Bounding it by the array is what makes the
  // invariant explicit.
  uint8_t immediate_size =
      YCPUGetImmediateSize(metadata, instruction->opcode, reg);
  if (immediate_size > Y_CPU_MAX_IMMEDIATE_BYTES) {
    immediate_size = Y_CPU_MAX_IMMEDIATE_BYTES;
  }
  instruction->immediate_size = immediate_size;
  for (uint8_t i = 0; i < immediate_size; ++i) {
    instruction->immediate[i] = YCPUFetchNextInstructionByte(cpu, &fetch_state);
  }

  instruction->size = (uint8_t)(fetch_state.next_byte_offset - original_ip);

  // What the instruction costs before it runs. Settled here because metadata
  // is already in hand, so the base cost is read from an entry the decode has
  // loaded rather than indexed again from the caller.
  instruction->base_cycles = (uint16_t)metadata->base_cycles +
                             YCPUGetEffectiveAddressCycles(instruction);

  return Y_CPU_FETCH_SUCCESS;
}

Y_PUBLIC void YCPUInvalidateDecodeCache(YCPUState* cpu) {
  YCPUDecodeCacheEntry* const cache = cpu->config.decode_cache;
  if (cache == NULL) {
    return;
  }
  for (uint32_t i = 0; i <= cpu->decode_cache_index_mask; ++i) {
    cache[i].valid = false;
  }
}

// Whether an entry holds a decode of the instruction at address that is still
// current. The generation is passed in because both callers have it already.
Y_FILE_PRIVATE Y_ALWAYS_INLINE bool YCPUIsDecodeCacheHit(
    const YCPUDecodeCacheEntry* entry, uint32_t address, uint8_t generation) {
  return entry->valid && entry->address == address &&
         entry->generation == generation;
}

// Fetches the next instruction, from the decode cache where it is there.
//
// What comes back through entry is the entry holding the instruction to run:
// the cache entry on a hit, scratch where there is no cache, and on a miss the
// entry the decode went straight into. Nothing is ever copied.
//
// Handing back the entry rather than the YCPUInstruction inside it is worth
// 0.68% at -O3, even though YCPUTick() reads nothing from an entry but
// ->instruction. One entry pointer addresses both the key fields and every
// instruction field, all inside the 5 bit immediate offset a Cortex-M0+
// encodes; handing back the YCPUInstruction makes the probe at the bottom of
// the run loop hold the entry for its hit test and entry->instruction to carry
// across the back-edge, which is two live pointers where there was one.
// YCPUTick() has no register to spare for that - its frame grows from 52 bytes
// to 60 and the loop picks up a spill and reload - so the smaller-looking
// signature is the slower one.
//
// A caller must not hold the pointer across another fetch.
Y_FILE_PRIVATE Y_HOT YCPUFetchNextInstructionStatus
YCPUFetchNextInstructionCached(
    YCPUState* cpu, YCPUDecodeCacheEntry* scratch,
    YCPUDecodeCacheEntry** entry) {
  const uint16_t ip = cpu->registers[Y_CPU_IP];
  // NULL covers both having no cache and having asked for an unusable one,
  // since YCPUInit() clears its own copy of a count that did not pass.
  YCPUDecodeCacheEntry* const cache = cpu->config.decode_cache;

  // Where the decode is going to land, and the key it would be kept under. The
  // two paths below join up at the decode rather than each making their own
  // call, because a second call site is enough for GCC to stop inlining what
  // it inlines into one.
  YCPUDecodeCacheEntry* target = scratch;
  uint32_t address = 0;
  uint8_t generation = 0;

  if (cache != NULL) {
    address = YCPUToRawAddress(cpu, Y_CPU_CS, ip);
    generation = cpu->code_page_generation[address >> Y_CPU_CODE_PAGE_SHIFT];
    target = &cache[address & cpu->decode_cache_index_mask];
    if (YCPUIsDecodeCacheHit(target, address, generation)) {
      *entry = target;
      return Y_CPU_FETCH_SUCCESS;
    }
    // A decode that fails partway leaves the entry holding whatever it got to,
    // so the entry disowns its contents before the decode rather than after.
    target->valid = false;
  }

  *entry = target;
  const YCPUFetchNextInstructionStatus status =
      YCPUFetchNextInstruction(cpu, &target->instruction);
  if (status != Y_CPU_FETCH_SUCCESS) {
    return status;
  }

  // Kept only when the whole instruction came from the page the generation was
  // read for, and did not run off the end of the segment. Past either boundary
  // the bytes are somewhere the key says nothing about, and both are too rare
  // to be worth a second check on every hit. Staying inside the page also
  // rules out wrapping the top of the address space.
  const uint32_t size = target->instruction.size;
  if (cache != NULL &&
      (address & Y_CPU_CODE_PAGE_OFFSET_MASK) + size <= Y_CPU_CODE_PAGE_SIZE &&
      (uint32_t)ip + size <= Y_CPU_SEGMENT_SIZE) {
    target->address = address;
    target->generation = generation;
    target->valid = true;
  }
  return Y_CPU_FETCH_SUCCESS;
}

// ============================================================================
// Execution
// ============================================================================

// Runs an instruction whose opcode table entry the caller already has, and
// which the caller has already established the entry agrees with.
//
// Kept out of line. Inlined into YCPUTick() this measured 31% slower on a
// Cortex-M0+: the execute path wants registers, the core has few, and folding
// the two together makes both spill. It only shows up once the hot path is in
// SRAM - from flash the XIP cache dominates and hides it.
Y_FILE_PRIVATE Y_HOT Y_NOINLINE YCPUInstructionResult
YCPUExecuteDecodedInstruction(
    YCPUState* cpu, YCPUInstruction* instruction,
    const YCPUOpcodeMetadata* metadata) {
  // Run the instruction handler.
  YCPUInstructionContext context = {
      .cpu = cpu,
      .instruction = instruction,
      .metadata = metadata,
  };
  return metadata->handler(&context);
}

// Checks an instruction against the opcode table before running it.
//
// For a caller that built the YCPUInstruction itself rather than decoding one -
// YCPUTick() goes straight to YCPUExecuteDecodedInstruction(), because its own
// decode is what produced the encoding these checks would be re-examining.
Y_PUBLIC Y_HOT YCPUInstructionResult
YCPUExecuteInstruction(YCPUState* cpu, YCPUInstruction* instruction) {
  const YCPUOpcodeMetadata* metadata = &YCPUOpcodeTable[instruction->opcode];

  // Check the encoded instruction against the expected format for its opcode.
  // Nothing checks that the opcode has a handler, because every entry in the
  // table has one: the eight bytes that are prefixes rather than instructions
  // are handled by YCPUExecuteInvalidOpcode(), which returns what a missing
  // handler used to.
  if (instruction->has_mod_rm != metadata->has_modrm) {
    return Y_CPU_INSTRUCTION_INVALID;
  }
  if (instruction->immediate_size !=
      (metadata->has_modrm
           ? YCPUGetImmediateSize(
                 metadata, instruction->opcode, instruction->mod_rm.reg)
           : metadata->immediate_size)) {
    return Y_CPU_INSTRUCTION_INVALID;
  }

  return YCPUExecuteDecodedInstruction(cpu, instruction, metadata);
}

// Save state and vector to the handler for an interrupt.
Y_FILE_PRIVATE void YCPUDispatchInterrupt(
    YCPUState* cpu, uint8_t interrupt_number) {
  // Prepare for interrupt processing.
  cpu->is_halted = false;
  YCPUPushValue(cpu, cpu->flags);
  YCPUSetFlag(cpu, Y_CPU_IF, false);
  YCPUSetFlag(cpu, Y_CPU_TF, false);
  YCPUPushValue(cpu, cpu->registers[Y_CPU_CS]);
  YCPUPushValue(cpu, cpu->registers[Y_CPU_IP]);

  // Invoke the interrupt handler callback first. If the caller did not provide
  // an interrupt handler callback, handle the interrupt within the VM using the
  // Interrupt Vector Table.
  YCPUInterruptHandlerResult interrupt_handler_result =
      cpu->config.handle_interrupt
          ? cpu->config.handle_interrupt(cpu, interrupt_number)
          : Y_CPU_INTERRUPT_HANDLER_UNHANDLED;

  if (interrupt_handler_result == Y_CPU_INTERRUPT_HANDLER_HANDLED) {
    // If the interrupt was handled by the caller-provided interrupt handler
    // callback, restore state and continue execution.
    YCPUExecuteReturnFromInterrupt(cpu);
    return;
  }

  // If the interrupt was not handled by the caller-provided interrupt handler
  // callback, handle it within the VM using the Interrupt Vector Table.
  uint16_t ivt_entry_offset = interrupt_number << 2;
  cpu->registers[Y_CPU_IP] = YCPUReadRawMemoryWord(cpu, ivt_entry_offset);
  cpu->registers[Y_CPU_CS] = YCPUReadRawMemoryWord(cpu, ivt_entry_offset + 2);
}

// Take a pending interrupt, if any. Returns whether one was dispatched.
Y_FILE_PRIVATE bool YCPUExecutePendingInterrupt(YCPUState* cpu) {
  // An internal interrupt goes first. It was raised by the instruction that
  // just executed, and taking it clears IF, which correctly holds off any
  // external request until the handler re-enables interrupts.
  if (cpu->has_pending_internal_interrupt) {
    const uint8_t interrupt_number = cpu->pending_internal_interrupt_number;
    YCPUClearInternalInterrupt(cpu);
    YCPUDispatchInterrupt(cpu, interrupt_number);
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
  if (YCPUGetFlag(cpu, Y_CPU_IF) && (request_hint == NULL || *request_hint) &&
      cpu->config.acknowledge_interrupt &&
      cpu->config.acknowledge_interrupt(cpu, &intr_vector)) {
    YCPUDispatchInterrupt(cpu, intr_vector);
    return true;
  }

  return false;
}

// ============================================================================
// Runs of instructions
// ============================================================================
//
// A tick runs several instructions back to back where it can. What that saves
// is the round trip: YPlatformTick()'s deadline test and stop checks, and
// YCPUTick()'s own prologue and epilogue, all of which are per tick rather than
// per instruction.
//
// Two things bound a run: YCPUTick()'s max_run_cycles, so that a device
// deadline is not passed over, and Y_CPU_MAX_INSTRUCTIONS_PER_TICK.

enum {
  // IN and OUT are two families of four - 0xE4-0xE7 takes the port number as
  // an immediate, 0xEC-0xEF takes it in DX - differing only in bit 3. Leaving
  // that bit free along with the two that select direction and width matches
  // both families and nothing else.
  Y_CPU_PORT_INSTRUCTION_MASK = 0xF4,
  Y_CPU_PORT_INSTRUCTION_VALUE = 0xE4,
};

// Whether an opcode reads or writes an I/O port.
Y_FILE_PRIVATE Y_ALWAYS_INLINE bool YCPUIsPortInstruction(uint8_t opcode) {
  return (opcode & Y_CPU_PORT_INSTRUCTION_MASK) == Y_CPU_PORT_INSTRUCTION_VALUE;
}

// Whether a run may carry on into the instruction after the one just executed.
//
// Everything here is something the end of a tick would otherwise have dealt
// with, and which running another instruction first would deal with too late.
Y_FILE_PRIVATE Y_ALWAYS_INLINE bool YCPUCanContinueRun(
    const YCPUState* cpu, uint8_t opcode, uint16_t max_run_cycles) {
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
      !YCPUIsPortInstruction(opcode) &&
      // An instruction that sets TF - POPF, IRET - must be the last of its
      // run, or the next one runs without trapping.
      !YCPUGetFlag(cpu, Y_CPU_TF) &&
      // STI, POPF and IRET can enable interrupts with a request already
      // waiting. Without a hint that cannot be told without an acknowledge
      // cycle, so a host supplying none ends the run here.
      cpu->config.interrupt_request_hint != NULL &&
      !*cpu->config.interrupt_request_hint;
}

// The entry holding the decoded instruction at CS:IP if the cache holds a
// current one, and NULL otherwise. The entry rather than the instruction for
// the same reason the fetch hands one back - see there.
//
// A run carries on only into a cached instruction, which keeps a step cheap
// and keeps a cold CPU to one instruction per tick however large its budget.
Y_FILE_PRIVATE Y_HOT YCPUDecodeCacheEntry* YCPUCachedEntryAtIP(YCPUState* cpu) {
  YCPUDecodeCacheEntry* const cache = cpu->config.decode_cache;
  if (cache == NULL) {
    return NULL;
  }
  const uint32_t address =
      YCPUToRawAddress(cpu, Y_CPU_CS, cpu->registers[Y_CPU_IP]);
  YCPUDecodeCacheEntry* const entry =
      &cache[address & cpu->decode_cache_index_mask];
  if (!YCPUIsDecodeCacheHit(
          entry, address,
          cpu->code_page_generation[address >> Y_CPU_CODE_PAGE_SHIFT])) {
    return NULL;
  }
  return entry;
}

Y_PUBLIC Y_HOT YCPUTickResult
YCPUTick(YCPUState* cpu, uint16_t max_run_cycles) {
  // Whether this tick ran an instruction. A halted CPU runs none until an
  // interrupt wakes it.
  bool executed_instruction = false;

  // A halted CPU still consumes time - it is sitting in a wait state, not
  // stopped - so a tick that runs no instruction still has to advance the
  // clock, or the timer that is meant to wake it would never tick either.
  cpu->pending_cycles = 0;
  cpu->cycles_this_tick = Y_CPU_HALTED_CYCLES;

  // The trap flag is sampled before the instruction runs, not after. An
  // instruction that sets TF - POPF or IRET - must not trap on itself, and one
  // that clears TF still traps once for the instruction it was set during.
  const bool trap_flag_was_set = YCPUGetFlag(cpu, Y_CPU_TF);

  // Execute next CPU instruction if not halted.
  if (!cpu->is_halted) {
    // Step 1: Fetch the next instruction.
    //
    // The local entry is only where a decode lands when there is no cache to
    // decode into. What runs is whatever the fetch points at - the cost of the
    // instruction rides in the YCPUInstruction, so nothing here reads the entry
    // for anything but that.
    YCPUDecodeCacheEntry scratch;
    YCPUDecodeCacheEntry* entry;
    YCPUFetchNextInstructionStatus fetch_status =
        YCPUFetchNextInstructionCached(cpu, &scratch, &entry);
    if (fetch_status != Y_CPU_FETCH_SUCCESS) {
      // CS:IP, which the logger adds, is where the fetch started.
      Y_CPU_ERROR(
          "instruction_fetch_failed", Y_LOG_INT("status", fetch_status));
      return Y_CPU_TICK_INVALID;
    }

    // A successful fetch is always followed by an instruction: the only way
    // out of the loop below without running one is a return.
    executed_instruction = true;

    // A tick that has to trap runs exactly one instruction: TF was set when it
    // started, so the single-step interrupt belongs to the instruction about
    // to run, and one that clears TF still owes that trap.
    uint8_t remaining =
        trap_flag_was_set ? 1 : (uint8_t)Y_CPU_MAX_INSTRUCTIONS_PER_TICK;

    for (;;) {
      YCPUInstruction* const instruction = &entry->instruction;
      const uint16_t instruction_cs = cpu->registers[Y_CPU_CS];
      const uint16_t instruction_ip = cpu->registers[Y_CPU_IP];
      cpu->registers[Y_CPU_IP] += instruction->size;

      // What the instruction costs before it runs came with the decode; its
      // bus traffic and anything that depends on its operands it charges
      // itself as it runs. A run accumulates all of it, which is what the
      // budget below is spent against.
      YCPUAddCycles(cpu, instruction->base_cycles);

      // Step 2: Execute the instruction. The fetch above derived has_mod_rm
      // and immediate_size from this same table entry, so the checks
      // YCPUExecuteInstruction() makes cannot fail here.
      const YCPUOpcodeMetadata* const metadata =
          &YCPUOpcodeTable[instruction->opcode];
      if (YCPUExecuteDecodedInstruction(cpu, instruction, metadata) !=
          Y_CPU_INSTRUCTION_EXECUTED) {
        // IP has already moved past the instruction, so its own address
        // goes in as well.
        Y_CPU_ERROR(
            "invalid_instruction", Y_LOG_HEX8("opcode", instruction->opcode),
            Y_LOG_HEX16("instruction_cs", instruction_cs),
            Y_LOG_HEX16("instruction_ip", instruction_ip));
        return Y_CPU_TICK_INVALID;
      }
      // The carry is a branch not taken until 2^32 instructions have run;
      // see instructions_retired_low for why the count is split.
      if (++cpu->instructions_retired_low == 0) {
        ++cpu->instructions_retired_high;
      }

      // Step 3: Carry on into the next instruction where nothing needs the
      // host's attention and the cache already holds it.
      if (--remaining == 0 ||
          !YCPUCanContinueRun(cpu, instruction->opcode, max_run_cycles)) {
        break;
      }
      entry = YCPUCachedEntryAtIP(cpu);
      // A port handler calls YPlatformSync(), making it the one instruction
      // that looks at the clock - and part way through a run the clock stops
      // short of the instructions already run in it, since a tick's cost is
      // reported once it is over. Ending before one leaves every port access
      // the first instruction of its tick.
      if (entry == NULL || YCPUIsPortInstruction(entry->instruction.opcode)) {
        break;
      }
    }
    cpu->cycles_this_tick = cpu->pending_cycles;
  }

  // Step 4: Handle a pending interrupt. This runs even while halted, because
  // an interrupt is the only thing that can clear the halted state -
  // YCPUExecutePendingInterrupt() resets is_halted when it dispatches one.
  const bool dispatched_interrupt = YCPUExecutePendingInterrupt(cpu);

  // Step 5: The trap flag raises a single-step interrupt after an instruction
  // executes, so a halted CPU must not trap - otherwise the trap would wake it
  // and then fire again on every subsequent tick.
  //
  // Single-stepping is the lowest priority of the interrupt sources recognized
  // at an instruction boundary, so an interrupt dispatched above takes its
  // place rather than both firing.
  if (executed_instruction && trap_flag_was_set && !dispatched_interrupt) {
    YCPURaiseInternalInterrupt(cpu, Y_CPU_INTERRUPT_SINGLE_STEP);
    YCPUExecutePendingInterrupt(cpu);
  }

  // This reports what the tick did, not what state the CPU ended up in. A tick
  // that executes HLT ran an instruction, so it reports Y_CPU_TICK_EXECUTED
  // even though the CPU is now halted; the ticks that follow report
  // Y_CPU_TICK_HALTED.
  return executed_instruction ? Y_CPU_TICK_EXECUTED : Y_CPU_TICK_HALTED;
}


// ==============================================================================
// src/cpu/cpu.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_CPU_BUNDLE_H

