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
  kLogModuleIDCPU = 1,
};

// Log module for the CPU.
static const LogModule kLogModuleCPU = {
    .id = kLogModuleIDCPU,
    .name = "CPU",
};

// ============================================================================
// CPU state
// ============================================================================

// CPU registers.
// Note that the order / numeric values of these constants are important here as
// they must match how the registers are encoded in the ModR/M byte.
typedef enum RegisterIndex {
  // General-purpose and index registers.

  // Accumulator Register
  kAX = 0,
  // Counter Register
  kCX = 1,
  // Data Register
  kDX = 2,
  // Base Register
  kBX = 3,
  // Stack Pointer Register
  kSP = 4,
  // Base Pointer Register
  kBP = 5,
  // Source Index Register
  kSI = 6,
  // Destination Index Register
  kDI = 7,

  // Segment registers.

  // Extra Segment Register
  kES = 8,
  // Code Segment Register
  kCS = 9,
  // Stack Segment Register
  kSS = 10,
  // Data Segment Register
  kDS = 11,

  // Instruction Pointer Register
  kIP,
} RegisterIndex;

enum {
  // Number of registers.
  kNumRegisters = kIP + 1,
};

// CPU flag masks.
typedef enum Flag {
  // Carry Flag
  kCF = (1 << 0),
  // Parity Flag
  kPF = (1 << 2),
  // Auxiliary Carry Flag
  kAF = (1 << 4),
  // Zero Flag
  kZF = (1 << 6),
  // Sign Flag
  kSF = (1 << 7),
  // Trap Flag
  kTF = (1 << 8),
  // Interrupt Enable Flag
  kIF = (1 << 9),
  // Direction Flag
  kDF = (1 << 10),
  // Overflow Flag
  kOF = (1 << 11),
} Flag;

enum {
  // Bits of the flags register that are not flags. The 8086/8088 does not
  // store them: bit 1 and bits 12 through 15 always read as one, and bits 3
  // and 5 always read as zero, whatever gets written over them.
  kFlagsAlwaysSet = 0xF002,
  kFlagsAlwaysClear = 0x0028,
  // CPU flags value on reset - no flags set, and the bits that are not flags
  // reading the only way they can.
  kInitialFlags = kFlagsAlwaysSet,
};

// Standard interrupts.
typedef enum InterruptNumber {
  kInterruptDivideError = 0,
  kInterruptSingleStep = 1,
  kInterruptNMI = 2,
  kInterruptBreakpoint = 3,
  kInterruptOverflow = 4,
} InterruptNumer;

// Result of executing a single instruction.
typedef enum InstructionResult {
  // The instruction was executed.
  kInstructionExecuted = 0,
  // The instruction could not be executed, because its opcode has no handler or
  // because its encoding does not match the expected format for its opcode.
  kInstructionInvalid,
} InstructionResult;

// Result of a single CPU tick.
typedef enum CPUTickResult {
  // An instruction was executed. This includes HLT: the tick that halts the
  // CPU still ran an instruction.
  kCPUTickExecuted = 0,
  // The instruction at CS:IP could not be fetched or executed. The CPU is left
  // pointing past the offending instruction; it is up to the caller to decide
  // whether to continue.
  kCPUTickInvalid,
  // The CPU was already halted and executed no instruction this tick. It stays
  // halted until an interrupt wakes it, so the caller must keep ticking the
  // rest of the machine.
  kCPUTickHalted,
  // Execution was stopped part way through the tick via CPURequestStop().
  kCPUTickStopped,
} CPUTickResult;

// Result of the handle_interrupt callback, directing how the CPU should
// proceed with an interrupt.
typedef enum InterruptHandlerResult {
  // The callback serviced the interrupt itself. The CPU restores the state
  // saved on entry and resumes at the interrupted instruction.
  kInterruptHandlerHandled = 0,
  // The callback did not service the interrupt. The CPU dispatches it through
  // the Interrupt Vector Table instead.
  kInterruptHandlerUnhandled,
} InterruptHandlerResult;

struct CPUState;
struct Instruction;

// Caller-provided runtime configuration.
typedef struct CPUConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger for this module. May be NULL.
  Logger* logger;

  // Callback to read a byte from memory.
  //
  // On the 8086, accessing an invalid memory address will yield garbage data
  // rather than causing a page fault. This callback interface mirrors that
  // behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  uint8_t (*read_memory_byte)(struct CPUState* cpu, uint32_t address);

  // Callback handing the CPU a run of bytes it may read directly, for
  // instruction fetch. Optional - when NULL, every instruction byte is read
  // through read_memory_byte, which is what happens anyway wherever this
  // declines.
  //
  // Given a linear address the CPU wants to fetch from, fills in
  // CPUState.instruction_fetch_window with a range covering it, or sets its
  // data to NULL to decline. A host must decline wherever a read has to be
  // observed or computed rather than loaded: a device region, unmapped memory,
  // or an address covered by anything the host has to be told about.
  //
  // The range should be the whole of whatever region the address falls in
  // rather than the tail of it starting at the address, so that a jump
  // backwards within the same region still lands inside it.
  //
  // The window is kept across instructions, so a host that changes what an
  // address means - remapping memory, or turning on a watchpoint - has to call
  // CPUInvalidateInstructionFetchWindow(). Writes through the same buffer need
  // no such call, and self-modifying code keeps working, because the window is
  // a pointer into the host's own storage rather than a copy of it.
  void (*get_instruction_fetch_window)(struct CPUState* cpu, uint32_t address);

  // Callback to write a byte to memory.
  //
  // On the 8086, accessing an invalid memory address will yield garbage data
  // rather than causing a page fault. This callback interface mirrors that
  // behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  void (*write_memory_byte)(
      struct CPUState* cpu, uint32_t address, uint8_t value);

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
  bool (*acknowledge_interrupt)(struct CPUState* cpu, uint8_t* vector);

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

  // Callback to handle an interrupt. If NULL, every interrupt is dispatched
  // through the Interrupt Vector Table.
  InterruptHandlerResult (*handle_interrupt)(
      struct CPUState* cpu, uint8_t interrupt_number);

  // Callback invoked before executing an instruction. This can be used to
  // inspect or modify the instruction before it is executed, or to inject a
  // pending interrupt. To stop execution, call CPURequestStop().
  void (*on_before_execute_instruction)(
      struct CPUState* cpu, struct Instruction* instruction);

  // Callback invoked after executing an instruction. This can be used to
  // inspect the instruction after it is executed, or to inject a pending
  // interrupt. To stop execution, call CPURequestStop().
  void (*on_after_execute_instruction)(
      struct CPUState* cpu, const struct Instruction* instruction);

  // Callback to read a byte from an I/O port.
  //
  // On the 8086, accessing an invalid I/O port will most likely yield garbage
  // data. This callback interface mirrors that behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  uint8_t (*read_port)(struct CPUState* cpu, uint16_t port);

  // Callback to write a byte to an I/O port.
  //
  // On the 8086, accessing an invalid I/O port will most likely yield garbage
  // data. This callback interface mirrors that behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  void (*write_port)(struct CPUState* cpu, uint16_t port, uint8_t value);
} CPUConfig;

// A run of bytes instruction fetch may read directly, covering the linear
// addresses [start, end), with data pointing at the byte at start.
//
// A NULL data means there is no window and every byte takes the ordinary path
// through CPUConfig.read_memory_byte.
typedef struct CPUInstructionFetchWindow {
  const uint8_t* data;
  uint32_t start;
  uint32_t end;
} CPUInstructionFetchWindow;

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
// CPUSetDirectDataWindow() is what keeps the two in step.
typedef struct CPUDirectDataWindow {
  uint8_t* data;
  uint32_t end;
} CPUDirectDataWindow;

enum {
  // Granularity at which a write to guest memory discards decoded
  // instructions: 4KB pages over the 8086's 1MB, which is 256 counters. It is
  // the coarsest page that still separates the code a program is running from
  // the data it is writing, and a finer one would only cost more memory.
  kCodePageShift = 12,
  kCodePageSize = 1 << kCodePageShift,
  kCodePageOffsetMask = kCodePageSize - 1,
  kNumCodePages = 0x100000 >> kCodePageShift,
};

// Storage for one cached decode. Defined below, where Instruction is.
struct CPUDecodeCacheEntry;

// State of the emulated CPU.
typedef struct CPUState {
  // Pointer to caller-provided runtime configuration
  CPUConfig* config;

  // Register values
  uint16_t registers[kNumRegisters];
  // Flag values
  uint16_t flags;

  // An internal interrupt, raised by the CPU itself as a result of the
  // instruction it just executed: INT n, INT 3, INTO, a divide error, or a
  // single-step trap. IF does not gate these - INT 21h works with interrupts
  // disabled, which is how DOS calls inside critical sections behave.
  bool has_pending_internal_interrupt;
  // Interrupt number of the pending internal interrupt.
  uint8_t pending_internal_interrupt_number;

  // Whether the CPU is in halted state. When true, CPUTick() will not fetch
  // or execute any instructions until an external event (e.g., an interrupt)
  // clears this state.
  bool is_halted;

  // Instructions retired since CPUInit(). A halted tick retires none.
  //
  // Counted here rather than left to the caller because CPUTick() already
  // knows whether it ran an instruction, while a caller can only find out by
  // sampling is_halted before every tick - which is what every benchmark
  // harness did, at the cost of giving up batching. 64 bits because a machine
  // left running overflows 32 of them in under an hour.
  uint64_t instructions_retired;

  // Whether a stop has been requested during the current tick. See
  // CPURequestStop().
  bool stop_requested;

  // Cycles charged by the instruction currently executing on top of its base
  // cost: its time on the data bus, and whatever it adds for itself when its
  // cost depends on its operands. CPUTick clears this before each instruction
  // and folds it into cycles_this_tick afterwards.
  uint16_t pending_cycles;

  // Cycles the last call to CPUTick consumed, at the 4.77MHz CPU clock. The
  // caller drives the rest of the machine from this, so that everything timed
  // against the CPU keeps the ratio real hardware has.
  uint16_t cycles_this_tick;

  // The run of bytes instruction fetch is currently reading from, as handed
  // over by CPUConfig.get_instruction_fetch_window.
  //
  // Kept across instructions rather than re-derived per instruction: execution
  // is sequential and a window spans a whole memory region, so the next
  // instruction is almost always inside the one already open.
  CPUInstructionFetchWindow instruction_fetch_window;

  // Guest memory the CPU reads and writes by indexing, as handed over by
  // CPUSetDirectDataWindow(). Unlike the fetch window this is not asked for
  // per access: data accesses are scattered rather than sequential, so there
  // is no locality for a per-access callback to exploit and the host sets it
  // once instead.
  CPUDirectDataWindow direct_data_window;

  // How many times each 4KB page of the address space has been written, as a
  // wrapping byte. A cached decode records the count its page stood at when it
  // was taken and is discarded once the two disagree, which is what makes
  // caching safe against code that writes over itself or is loaded over an
  // earlier program.
  //
  // Maintained whether or not a decode cache exists. A write cannot cheaply
  // tell whether anything is holding a decode of the page it lands on, and the
  // test to find out would cost about what the counter does.
  uint8_t code_page_generation[kNumCodePages];

  // Decoded instructions, keyed by the linear address they start at, as handed
  // over by CPUSetDecodeCache(). NULL where the host supplies none, in which
  // case every instruction is decoded.
  struct CPUDecodeCacheEntry* decode_cache;
  // One less than the number of entries, so that an address becomes an index
  // with a mask rather than a remainder. That is why the count has to be a
  // power of two: a remainder by a runtime value is a division, which this
  // target has no instruction for.
  uint32_t decode_cache_index_mask;
} CPUState;

// Initialize CPU state.
void CPUInit(CPUState* cpu, CPUConfig* config);

// Get the value of a CPU flag.
static inline bool CPUGetFlag(const CPUState* cpu, Flag flag) {
  return (cpu->flags & flag) != 0;
}
// Set a CPU flag.
static inline void CPUSetFlag(CPUState* cpu, Flag flag, bool value) {
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
static inline void CPURaiseInternalInterrupt(
    CPUState* cpu, uint8_t interrupt_number) {
  cpu->has_pending_internal_interrupt = true;
  cpu->pending_internal_interrupt_number = interrupt_number;
}

// Discard a pending internal interrupt without taking it.
static inline void CPUClearInternalInterrupt(CPUState* cpu) {
  cpu->has_pending_internal_interrupt = false;
  cpu->pending_internal_interrupt_number = 0;
}

// Charge the instruction currently executing for cycles beyond its base cost.
// Used by the instructions whose cost is not a property of the opcode alone -
// a conditional jump that is taken, a shift by a count in CL, a multiply or a
// divide.
void CPUAddCycles(CPUState* cpu, uint16_t cycles);

// Request that the current tick stop as soon as the instruction in progress
// finishes, causing CPUTick() to return kCPUTickStopped.
//
// This is intended to be called from within a CPU callback - a memory or I/O
// port access, an interrupt handler, or an instruction hook - which is why
// stopping is signalled out of band rather than through a return value: a
// watchpoint fires inside read_memory_byte, which returns a uint8_t and has no
// way to carry a status.
//
// The request applies only to the tick during which it was made. CPUTick()
// clears it on entry, so a request made outside a tick has no effect.
static inline void CPURequestStop(CPUState* cpu) { cpu->stop_requested = true; }

// Discards the window instruction fetch is reading from, so that the next
// fetch asks CPUConfig.get_instruction_fetch_window again.
//
// A host must call this whenever it changes what an address means - remapping
// memory, or enabling something that has to observe reads - because the window
// is a direct pointer that would otherwise outlive the change. Writing through
// the same buffer does not need it: the window is a pointer into the host's
// storage, not a copy.
static inline void CPUInvalidateInstructionFetchWindow(CPUState* cpu) {
  cpu->instruction_fetch_window.data = NULL;
}

// Hands the CPU guest memory it may read and write by indexing, covering the
// half-open range of linear addresses [0, end). Optional - a host that
// supplies none has every access go through CPUConfig.read_memory_byte and
// CPUConfig.write_memory_byte, which is also what happens for every address at
// or above end.
//
// The window must be plain storage whose reads and writes are the caller's
// buffer and nothing else. A host must not hand over a region where a read has
// to be observed or computed - a device, or anything the host has to be told
// about, such as an address under a watchpoint - because an access through the
// window is a load or a store and the host never learns of it.
//
// Writes through the same buffer need no further call, so a host writing guest
// memory itself, or by DMA, stays coherent with the CPU for free. What does
// need a call is a change to what an address means: remapping memory, or
// enabling something that has to observe accesses, calls
// CPUInvalidateDirectDataWindow().
static inline void CPUSetDirectDataWindow(
    CPUState* cpu, uint8_t* data, uint32_t end) {
  cpu->direct_data_window.data = data;
  cpu->direct_data_window.end = data ? end : 0;
}

// Discards the direct data window, so that every access goes back through
// CPUConfig.read_memory_byte and CPUConfig.write_memory_byte.
static inline void CPUInvalidateDirectDataWindow(CPUState* cpu) {
  cpu->direct_data_window.data = NULL;
  cpu->direct_data_window.end = 0;
}

// Discards every cached decode.
//
// A host calls this when it changes what an address means rather than what is
// stored at it - remapping memory is the case that matters. An ordinary write
// needs no call: CPUNotifyMemoryWrite() covers those, and the CPU makes that
// call for itself for every write it makes.
void CPUInvalidateDecodeCache(CPUState* cpu);

// Tells the CPU that the byte at a linear address has been written, so that
// any decode taken from that page stops being used.
//
// The CPU calls this for every write it makes itself. What a host has to
// report is a write it makes some other way - by DMA, or through the memory
// map from outside a tick - because a decode cached from those bytes is
// otherwise indistinguishable from a current one.
//
// The counter is a byte, so it comes back round every 256 writes to a page. At
// that point a decode taken exactly 256 writes ago would look current again,
// and the whole cache goes instead.
//
// The address is masked to the 8086's 20 bits rather than range checked. Every
// address the CPU produces is already within them, but this is public, and an
// address above the space aliasing onto a page costs a spurious invalidation
// where indexing past the array would corrupt whatever follows it.
static inline void CPUNotifyMemoryWrite(CPUState* cpu, uint32_t address) {
  const uint32_t page = (address >> kCodePageShift) & (kNumCodePages - 1);
  if (++cpu->code_page_generation[page] == 0) {
    CPUInvalidateDecodeCache(cpu);
  }
}

// ============================================================================
// Instructions
// ============================================================================

enum {
  // Maximum number of displacement bytes in an 8086 instruction.
  kMaxDisplacementBytes = 2,
  // Maximum number of immediate data bytes in an 8086 instruction.
  kMaxImmediateBytes = 4,
  // Bytes an instruction carries besides its prefixes: the opcode, a ModR/M
  // byte, displacement, and immediate data.
  kMaxNonPrefixBytes = 2 + kMaxDisplacementBytes + kMaxImmediateBytes,
  // Maximum number of prefix bytes accepted.
  //
  // The 8086 puts no limit on this. A real one fetches prefixes for as long as
  // they keep coming, and a stream of nothing but prefixes hangs it outright -
  // a prefix is not an instruction boundary, so no interrupt is ever
  // recognized. Two things here cannot follow it that far: the fetch loop has
  // to hand control back to its caller, and Instruction.size is a uint8_t that
  // CPUTick adds to IP. Accepting as many prefixes as still leave the whole
  // encoding addressable by size satisfies both, and is far past anything an
  // assembler emits.
  kMaxPrefixBytes = 0xFF - kMaxNonPrefixBytes,
};

// Instruction prefixes.
typedef enum {
  kPrefixES = 0x26,    // ES segment override
  kPrefixCS = 0x2E,    // CS segment override
  kPrefixSS = 0x36,    // SS segment override
  kPrefixDS = 0x3E,    // DS segment override
  kPrefixLOCK = 0xF0,  // LOCK
  // Undocumented alias of LOCK on the 8086/8088, which does not decode bit 0
  // of this opcode.
  kPrefixLOCKAlt = 0xF1,
  kPrefixREPNZ = 0xF2,  // REPNE/REPNZ
  kPrefixREP = 0xF3,    // REP/REPE/REPZ
} InstructionPrefix;

enum {
  // Value of Instruction.segment_override when an instruction carries no
  // segment override prefix. Zero is kAX, which is never a segment register,
  // so a zero-initialized Instruction correctly has no override.
  kNoSegmentOverride = 0,
};

// The Mod R/M byte.
// The fields are stored a byte each rather than packed back into the bit
// positions they were decoded from. Every read of one would otherwise be a
// load, a shift and a mask, and mod and rm are read on the hottest path in the
// emulator - GetMemoryOperandAddress() takes them for every memory operand and
// GetEffectiveAddressCycles() for every instruction - while reg selects the
// handler for all five instruction groups.
typedef struct ModRM {
  // Mod field - bits 6 and 7 of the ModR/M byte.
  uint8_t mod;
  // REG field - bits 3 to 5 of the ModR/M byte.
  uint8_t reg;
  // R/M field - bits 0 to 2 of the ModR/M byte.
  uint8_t rm;
} ModRM;

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
// No field here is a bitfield, for the reason given at ModRM above. That puts
// the struct at 16 bytes, which is what a decode cache entry is sized around -
// so the size is worth watching again, though for how many decodes fit in a
// given amount of memory rather than for what a fetch costs.
typedef struct Instruction {
  // The segment register selected by a segment override prefix, as a
  // RegisterIndex, or kNoSegmentOverride if the instruction carries none. A
  // later override wins over an earlier one, as on hardware.
  uint8_t segment_override;

  // The repetition prefix present - kPrefixREP or kPrefixREPNZ - or 0 if the
  // instruction carries neither. A later one wins over an earlier one.
  uint8_t repetition_prefix;

  // The primary opcode byte.
  uint8_t opcode;

  // The ModR/M byte, which specifies addressing modes. For some instructions,
  // the REG field within this byte acts as an opcode extension.
  ModRM mod_rm;

  // Raw displacement bytes. If displacement_size is 1, only disp_bytes[0] is
  // valid (value is typically sign-extended). If displacement_size is 2,
  // disp_bytes[0] is the low byte, disp_bytes[1] is the high byte.
  uint8_t displacement[kMaxDisplacementBytes];

  // Raw immediate data bytes.
  uint8_t immediate[kMaxImmediateBytes];

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
} Instruction;

// One cached decode.
//
// A hit is used in place - the executor is handed a pointer to the instruction
// inside the entry and nothing is copied. That is the whole of why caching
// pays: copying the struct out costs about what decoding a two or three byte
// instruction from an open fetch window costs, which is most of what a hit
// would otherwise save. Nothing an instruction handler does writes through the
// Instruction it was given, so lending out the entry is safe.
typedef struct CPUDecodeCacheEntry {
  // The decode itself.
  Instruction instruction;
  // The linear address the instruction starts at, which is the key.
  uint32_t address;
  // What CPUState.code_page_generation said for this instruction's page when
  // the decode was taken. A hit requires it to still say the same.
  uint8_t generation;
  // Whether this entry holds a decode at all.
  bool valid;
} CPUDecodeCacheEntry;

// Hands the CPU storage for its decode cache. Optional - a host that supplies
// none has every instruction decoded, which is what happened before there was
// a cache.
//
// num_entries must be a power of two, and the storage must outlive the CPU.
// Anything else leaves the CPU with no cache.
//
// The entries need no initialization: CPUInit() zeroes the CPU, and an entry
// is only ever read after its own fields say it holds something.
void CPUSetDecodeCache(
    CPUState* cpu, CPUDecodeCacheEntry* entries, uint32_t num_entries);

// ============================================================================
// Execution
// ============================================================================

// Result status from fetching the next instruction.
typedef enum CPUFetchNextInstructionStatus {
  kFetchSuccess = 0,
  // Prefix exceeds maximum allowed size.
  kFetchPrefixTooLong = -1,
} CPUFetchNextInstructionStatus;

// Fetch the next instruction from CS:IP.
//
// The instruction is decoded directly into dest_instruction, so on failure
// dest_instruction holds however much had been decoded when the fetch failed.
// Since this function is part of the core CPU execution loop, assembling and
// copying a whole instruction struct would have a measurable impact on
// performance.
CPUFetchNextInstructionStatus CPUFetchNextInstruction(
    CPUState* cpu, Instruction* instruction);

// Execute a single fetched instruction.
InstructionResult CPUExecuteInstruction(
    CPUState* cpu, Instruction* instruction);

// Run a single instruction cycle, including fetching and executing the next
// instruction at CS:IP, and handling interrupts.
CPUTickResult CPUTick(CPUState* cpu);

#endif  // YAX86_CPU_PUBLIC_H
