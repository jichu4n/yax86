// Public interface for the Platform module.
#ifndef YAX86_PLATFORM_PUBLIC_H
#define YAX86_PLATFORM_PUBLIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef YAX86_PLATFORM_BUNDLE_H
#include "../util/log.h"
#include "../util/static_vector.h"
#endif  // YAX86_PLATFORM_BUNDLE_H

enum {
  // Log module ID for the Platform.
  kLogModuleIDPlatform = 0,
};

// Log module for the Platform.
static const LogModule kLogModulePlatform = {
    .id = kLogModuleIDPlatform,
    .name = "PLATFORM",
};

#include "cpu.h"
#include "dma.h"
#include "fdc.h"
#include "hdc.h"
#include "keyboard.h"
#include "pic.h"
#include "pit.h"
#include "ppi.h"
#include "video.h"

struct PlatformState;

// ============================================================================
// Memory mapping
// ============================================================================

// Type ID of a memory map entry.
typedef uint8_t MemoryMapEntryType;

enum {
  // Conventional memory - first 640KB of physical memory, mapped to 0x00000 to
  // 0x9FFFF (640KB).
  kMemoryMapEntryConventional = 0,

  // Maximum number of memory map entries.
  kMaxMemoryMapEntries = 16,

  // Maximum size of physical memory in bytes.
  kMaxPhysicalMemorySize = 640 * 1024,
  // Minimum size of physical memory in bytes.
  kMinPhysicalMemorySize = 64 * 1024,
};

// A memory map entry for a region in logical address space. Memory regions
// should not overlap.
typedef struct MemoryMapEntry {
  // Custom data passed through to callbacks.
  void* context;

  // The memory map entry type, such as kMemoryMapEntryConventional.
  MemoryMapEntryType entry_type;
  // Start address of the memory region.
  uint32_t start;
  // Inclusive end address of the memory region.
  uint32_t end;
  // Callback to read a byte from the memory map entry, where address is
  // relative to the start of the entry.
  uint8_t (*read_byte)(struct MemoryMapEntry* entry, uint32_t relative_address);
  // Callback to write a byte to memory, where address is relative to the start
  // address.
  void (*write_byte)(
      struct MemoryMapEntry* entry, uint32_t relative_address, uint8_t value);
} MemoryMapEntry;

// Register a memory map entry in the platform state. Returns true if the entry
// was successfully registered, or false if:
//   - There already exists a memory map entry with the same type.
//   - The new entry's memory region overlaps with an existing entry.
//   - The number of memory map entries would exceed kMaxMemoryMapEntries.
bool RegisterMemoryMapEntry(
    struct PlatformState* platform, const MemoryMapEntry* entry);
// Look up the memory map entry corresponding to an address. Returns NULL if the
// address is not mapped to a known memory map entry.
MemoryMapEntry* GetMemoryMapEntryForAddress(
    struct PlatformState* platform, uint32_t address);
// Look up a memory map entry by type. Returns NULL if no entry found with the
// specified type.
MemoryMapEntry* GetMemoryMapEntryByType(
    struct PlatformState* platform, MemoryMapEntryType entry_type);

// Read a byte from a logical memory address by invoking the corresponding
// memory map entry's read_byte callback.
//
// On the 8086, accessing an invalid memory address will yield garbage data
// rather than causing a page fault. This callback interface mirrors that
// behavior.
uint8_t ReadMemoryByte(struct PlatformState* platform, uint32_t address);
// Read a word from a logical memory address by invoking the corresponding
// memory map entry's read_byte callback.
uint16_t ReadMemoryWord(struct PlatformState* platform, uint32_t address);
// Write a byte to a logical memory address by invoking the corresponding
// memory map entry's write_byte callback.
//
// On the 8086, accessing an invalid memory address will yield garbage data
// rather than causing a page fault. This callback interface mirrors that
// behavior.
void WriteMemoryByte(
    struct PlatformState* platform, uint32_t address, uint8_t value);
// Write a word to a logical memory address by invoking the corresponding
// memory map entry's write_byte callback.
void WriteMemoryWord(
    struct PlatformState* platform, uint32_t address, uint16_t value);

// ============================================================================
// I/O port mapping
// ============================================================================

// Type ID of an I/O port map entry.
typedef uint16_t PortMapEntryType;

enum {
  // Maximum number of I/O port mapping entries.
  kMaxPortMapEntries = 16,
  // I/O port map entry for the master PIC (ports 0x20-0x21).
  kPortMapEntryPIC = 0x20,
  // I/O port map entry for the PIT (ports 0x40-0x43).
  kPortMapEntryPIT = 0x40,
  // I/O port map entry for the PPI (ports 0x60-0x63).
  kPortMapEntryPPI = 0x60,
  // I/O port map entry for the FDC (ports 0x3F0-0x3F7).
  kPortMapEntryFDC = 0x3F0,
  // I/O port map entry for the HDC (ports 0x300-0x30F).
  kPortMapEntryHDC = 0x300,
  // I/O port map entry for the DMA controller (ports 0x00-0x0F).
  kPortMapEntryDMA = 0x00,
  // I/O port map entry for the DMA Page Registers (ports 0x80-0x8F).
  kPortMapEntryDMAPage = 0x80,
};

// An I/O port map entry. Entries should not overlap.
typedef struct PortMapEntry {
  // Custom data passed through to callbacks.
  void* context;

  // The I/O port map entry type.
  PortMapEntryType entry_type;
  // Start of the I/O port range.
  uint16_t start;
  // Inclusive end of the I/O port range.
  uint16_t end;
  // Callback to read a byte from an I/O port within the range.
  uint8_t (*read_byte)(struct PortMapEntry* entry, uint16_t port);
  // Callback to write a byte an I/O port within the range.
  void (*write_byte)(struct PortMapEntry* entry, uint16_t port, uint8_t value);
} PortMapEntry;

// Register an I/O port map entry in the platform state. Returns true if the
// entry was successfully registered, or false if:
//   - There already exists an I/O port map entry with the same type.
//   - The new entry's I/O port range overlaps with an existing entry.
bool RegisterPortMapEntry(
    struct PlatformState* platform, const PortMapEntry* entry);
// Look up the I/O port map entry corresponding to a port. Returns NULL if the
// port is not mapped to a known I/O port map entry.
PortMapEntry* GetPortMapEntryForPort(
    struct PlatformState* platform, uint16_t port);
// Look up an I/O port map entry by type. Returns NULL if no entry found with
// the specified type.
PortMapEntry* GetPortMapEntryByType(
    struct PlatformState* platform, PortMapEntryType entry_type);

// Read a byte from an I/O port by invoking the corresponding I/O port map
// entry's read_byte callback.
uint8_t ReadPortByte(struct PlatformState* platform, uint16_t port);
// Read a word from an I/O port by invoking the corresponding I/O port map
// entry's read_byte callback. This reads two consecutive bytes from the port.
uint16_t ReadPortWord(struct PlatformState* platform, uint16_t port);
// Write a byte to an I/O port by invoking the corresponding I/O port map
// entry's write_byte callback.
void WritePortByte(
    struct PlatformState* platform, uint16_t port, uint8_t value);
// Write a word to an I/O port by invoking the corresponding I/O port map
// entry's write_byte callback. This writes two consecutive bytes to the port.
void WritePortWord(
    struct PlatformState* platform, uint16_t port, uint16_t value);

// ============================================================================
// Execution control
// ============================================================================

enum {
  // Maximum number of execution breakpoints.
  kMaxBreakpoints = 8,
  // Maximum number of memory watchpoints.
  kMaxMemoryWatchpoints = 8,
  // Returned by PlatformAddBreakpoint() and PlatformAddMemoryWatchpoint() when
  // no slot is available.
  kInvalidWatchIndex = -1,
};

// An execution breakpoint. Execution stops before the instruction at cs:ip is
// executed.
typedef struct PlatformBreakpoint {
  // Whether this slot is in use.
  bool enabled;
  // Code segment of the instruction to break on.
  uint16_t cs;
  // Instruction pointer of the instruction to break on.
  uint16_t ip;
} PlatformBreakpoint;

// A memory watchpoint. Execution stops once the instruction or DMA transfer
// that accessed the watched region has finished.
typedef struct PlatformMemoryWatchpoint {
  // Whether this slot is in use.
  bool enabled;
  // Start address of the watched region.
  uint32_t start;
  // Inclusive end address of the watched region.
  uint32_t end;
  // Whether to stop on reads from the region.
  bool on_read;
  // Whether to stop on writes to the region.
  bool on_write;
} PlatformMemoryWatchpoint;

// Why execution stopped.
typedef enum PlatformStopReason {
  // Reached an instruction with a breakpoint on it.
  kPlatformStopBreakpoint = 0,
  // Accessed a watched memory region.
  kPlatformStopMemoryWatchpoint,
  // Completed one instruction in step mode.
  kPlatformStopStep,
} PlatformStopReason;

// Details of the most recent stop.
typedef struct PlatformStopInfo {
  // Why execution stopped.
  PlatformStopReason reason;

  // CS:IP at the point of the stop. For a breakpoint stop this is the
  // breakpoint address itself, since the instruction has not run yet. For a
  // watchpoint or step stop the instruction has completed, so this points at
  // the next instruction.
  uint16_t cs;
  uint16_t ip;

  // Index of the breakpoint or watchpoint that fired. Unused for a step stop.
  uint8_t index;

  // For a memory watchpoint stop, the address that was accessed.
  uint32_t address;
  // For a memory watchpoint stop, whether the access was a write.
  bool is_write;
} PlatformStopInfo;

// Result of running the platform.
typedef enum PlatformRunStatus {
  // The machine is running normally. Also returned by PlatformRun() when it
  // exhausts its tick budget without stopping.
  kPlatformRunning = 0,
  // The instruction at CS:IP could not be fetched or executed.
  kPlatformInvalid,
  // The CPU halted with interrupts disabled and no interrupt pending, so
  // nothing can ever wake it. This is how GLaBIOS signals a fatal error.
  kPlatformHung,
  // Execution stopped at a breakpoint, watchpoint, or single step. See
  // PlatformGetStopInfo() for which, and where.
  kPlatformStopped,
} PlatformRunStatus;

// ============================================================================
// Platform state
// ============================================================================

// Caller-provided runtime configuration.
typedef struct PlatformConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger configuration, shared by the platform and every module it owns. If
  // NULL, logging is disabled. The configuration is owned by the caller and
  // must outlive the platform.
  //
  // Hosts that want tick numbers in their log output should wire get_tick to
  // return PlatformState.ticks.
  LoggerConfig* logger_config;

  // Physical memory size in bytes. Must be between 64K and 640K.
  uint32_t physical_memory_size;

  // The video adapter installed in the machine. This also determines the
  // display type reported by the PPI's DIP switches, which is what the BIOS
  // branches on when it programs the adapter.
  VideoAdapter video_adapter;

  // Callback to read a byte from physical memory.
  //
  // On the 8086, accessing an invalid memory address will yield garbage data
  // rather than causing a page fault. This callback interface mirrors that
  // behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  uint8_t (*read_physical_memory_byte)(
      struct PlatformState* platform, uint32_t address);

  // Callback to write a byte to physical memory.
  //
  // On the 8086, accessing an invalid memory address will yield garbage data
  // rather than causing a page fault. This callback interface mirrors that
  // behavior.
  //
  // For simplicity, we use a single 8-bit interface for memory access, similar
  // to the real-life 8088.
  void (*write_physical_memory_byte)(
      struct PlatformState* platform, uint32_t address, uint8_t value);

  // Callback invoked when the PC speaker's output changes. frequency_hz is the
  // square wave frequency the speaker should emit, or 0 to turn it off. May be
  // NULL, in which case the speaker is silent.
  //
  // The speaker sounds when PIT channel 2 is producing a tone and both PPI
  // port B bits 0 and 1 are set. This reports current state rather than a
  // stream of events - see PITConfig.set_pc_speaker_frequency.
  //
  // A frequency deliberately cannot express everything the hardware does. On a
  // real PC the speaker line is the AND of channel 2's output and port B bit
  // 1, driving the cone directly, so turning the speaker off parks that line
  // at a constant level and the cone audibly settles. Neither that click nor
  // the digitized audio some software produces by toggling bit 1 directly can
  // be represented here.
  //
  // However, we're choosing this approach so that basic PC speaker audio will
  // work without requiring PIT to be perfectly cycle accurate.
  void (*set_pc_speaker_frequency)(
      struct PlatformState* platform, uint32_t frequency_hz);
} PlatformConfig;

STATIC_VECTOR_TYPE(MemoryMap, MemoryMapEntry, kMaxMemoryMapEntries)
STATIC_VECTOR_TYPE(PortMap, PortMapEntry, kMaxPortMapEntries)

// State of the platform.
enum {
  // The CPU clock, in cycles per second. 4.77MHz is the IBM PC/XT's 14.318MHz
  // crystal divided by three.
  kCPUCyclesPerSecond = 4772727,
  // Cycles per millisecond, near enough for the devices that want one.
  kCyclesPerMillisecond = kCPUCyclesPerSecond / 1000,
  // The PIT is clocked at 1.193MHz, the same crystal divided by twelve, which
  // is a quarter of the CPU clock.
  kCyclesPerPITTick = 4,
  // How often to step the floppy controller's state machine. This is not a
  // clock ratio - the controller is a state machine here rather than a
  // modelled device - so it keeps the rate it had before cycles were counted,
  // to leave floppy timing where it was.
  kCyclesPerFDCTick = 20,
};

typedef struct PlatformState {
  // Pointer to caller-provided runtime configuration.
  PlatformConfig* config;

  // Logger shared by the platform and every module it owns.
  Logger logger;

  // CPU runtime configuration.
  CPUConfig cpu_config;
  // CPU state.
  CPUState cpu;

  // PIC runtime configuration.
  PICConfig pic_config;
  // PIC state.
  PICState pic;

  // PIT runtime configuration.
  PITConfig pit_config;
  // PIT state.
  PITState pit;

  // PPI runtime configuration.
  PPIConfig ppi_config;
  // PPI state.
  PPIState ppi;

  // Keyboard runtime configuration.
  KeyboardConfig keyboard_config;
  // Keyboard state.
  KeyboardState keyboard;

  // DMA controller runtime configuration.
  DMAConfig dma_config;
  // DMA controller state.
  DMAState dma;

  // FDC state.
  FDCConfig fdc_config;
  FDCState fdc;

  // HDC runtime configuration.
  HDCConfig hdc_config;
  // HDC state.
  HDCState hdc;

  // Video runtime configuration.
  VideoConfig video_config;
  // Video state.
  VideoState video;

  // Memory map.
  MemoryMap memory_map;
  // I/O port map.
  PortMap io_port_map;

  // How many CPU clock cycles have run, at 4.77MHz. Every device in the
  // machine is clocked from this, so that what the guest measures with the
  // timer matches how long it spent executing.
  uint32_t ticks;

  // Cycles counted towards each device's next tick but not yet used by it.
  uint16_t pit_cycles;
  uint16_t fdc_cycles;
  uint16_t keyboard_cycles;

  // Execution breakpoints.
  PlatformBreakpoint breakpoints[kMaxBreakpoints];
  // Memory watchpoints.
  PlatformMemoryWatchpoint memory_watchpoints[kMaxMemoryWatchpoints];

  // Whether any breakpoint or memory watchpoint slot is in use. Cached so that
  // the instruction and memory access hot paths can skip the checks entirely
  // in the common case where nothing is being watched.
  bool has_enabled_breakpoints;
  bool has_enabled_memory_watchpoints;

  // Whether to stop after each instruction.
  bool is_step_mode;

  // Whether stop_info describes a stop that has occurred.
  bool has_stop_info;
  // Details of the most recent stop.
  PlatformStopInfo stop_info;

  // Whether a stop has been triggered but not yet reported to the caller.
  bool stop_pending;
  // Set when execution stopped at a breakpoint, so that resuming executes the
  // instruction instead of stopping again at the same address.
  bool skip_breakpoint_check;
} PlatformState;

// Initialize the platform state with the provided configuration. Returns true
// if the platform state was successfully initialized, or false if:
//   - The physical memory size is not between 64K and 640K.
bool PlatformInit(PlatformState* platform, PlatformConfig* config);

// Raise a hardware interrupt to the CPU via the PIC. Returns true if the
// IRQ was successfully raised, or false if the IRQ number is invalid.
bool PlatformRaiseIRQ(PlatformState* platform, uint8_t irq);

// Run a single cycle of the platform, including ticking all sub-modules. This
// should be called at the CPU clock rate (4.77MHz for the 8088).
//
// Returns kPlatformRunning if the machine should keep running.
PlatformRunStatus PlatformTick(PlatformState* platform);

// Run up to max_ticks cycles of the platform, stopping early if a tick returns
// anything other than kPlatformRunning. Returns the status of the tick that
// stopped the run, or kPlatformRunning if the full budget was consumed.
PlatformRunStatus PlatformRun(PlatformState* platform, uint32_t max_cycles);

// Add an execution breakpoint at cs:ip. Returns the breakpoint index, or
// kInvalidWatchIndex if all kMaxBreakpoints slots are in use.
int8_t PlatformAddBreakpoint(PlatformState* platform, uint16_t cs, uint16_t ip);

// Remove the breakpoint with the given index. Returns false if the index is
// out of range or the slot is not in use.
bool PlatformRemoveBreakpoint(PlatformState* platform, uint8_t index);

// Remove all breakpoints.
void PlatformClearBreakpoints(PlatformState* platform);

// Add a memory watchpoint covering the physical addresses [start, end]
// inclusive. Returns the watchpoint index, or kInvalidWatchIndex if all
// kMaxMemoryWatchpoints slots are in use, if start is greater than end, or if
// neither on_read nor on_write is set.
//
// A watchpoint sees every access that goes through the memory map, which
// includes instruction fetches and DMA transfers as well as data accesses.
int8_t PlatformAddMemoryWatchpoint(
    PlatformState* platform, uint32_t start, uint32_t end, bool on_read,
    bool on_write);

// Remove the memory watchpoint with the given index. Returns false if the
// index is out of range or the slot is not in use.
bool PlatformRemoveMemoryWatchpoint(PlatformState* platform, uint8_t index);

// Remove all memory watchpoints.
void PlatformClearMemoryWatchpoints(PlatformState* platform);

// Enable or disable step mode. In step mode every tick that executes an
// instruction stops with reason kPlatformStopStep.
void PlatformSetStepMode(PlatformState* platform, bool is_step_mode);

// Returns details of the most recent stop, or NULL if execution has never
// stopped. The returned pointer stays valid until the next stop.
const PlatformStopInfo* PlatformGetStopInfo(const PlatformState* platform);

#endif  // YAX86_PLATFORM_PUBLIC_H
