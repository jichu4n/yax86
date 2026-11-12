// ==============================================================================
// YAX86 PLATFORM MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_PLATFORM_BUNDLE_H
#define YAX86_PLATFORM_BUNDLE_H

#ifndef YAX86_UTIL_BUNDLE_H
#include "util.h"
#endif  // YAX86_UTIL_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/platform/public.h start
// ==============================================================================

#line 1 "./src/platform/public.h"
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
  Y_PLATFORM_LOG_MODULE_ID = 0,
};

// Log module for the Platform.
Y_PUBLIC_HEADER const YLogModule YPlatformLogModule = {
    .id = Y_PLATFORM_LOG_MODULE_ID,
    .name = "platform",
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

struct YPlatformState;

// ============================================================================
// Memory mapping
// ============================================================================

// Type ID of a memory map entry.
typedef uint8_t YPlatformMemoryMapEntryType;

enum {
  // Conventional memory held in YPlatformConfig.direct_memory, from 0x00000 up
  // to direct_memory_size. At most 640KB, so it ends by 0x9FFFF.
  Y_PLATFORM_MEMORY_MAP_ENTRY_CONVENTIONAL = 0,
  // BIOS ROM - mapped to 0xF0000 to 0xFFFFF (64KB).
  Y_PLATFORM_MEMORY_MAP_ENTRY_BIOS_ROM = 0x01,
  // HDC option ROM - mapped starting at 0xC8000.
  Y_PLATFORM_MEMORY_MAP_ENTRY_HDC_OPTION_ROM = 0x0C,
  // Paged conventional memory: the part above direct_memory_size, when
  // YPlatformConfig.paged_memory is set.
  Y_PLATFORM_MEMORY_MAP_ENTRY_PAGED_MEMORY = 0x02,
  // Video RAM - mapped to 0xB0000 (MDA) or 0xB8000 (CGA).
  Y_PLATFORM_MEMORY_MAP_ENTRY_VRAM = 0x10,

  // Maximum number of memory map entries.
  Y_PLATFORM_MAX_MEMORY_MAP_ENTRIES = 16,

  // How many decoded instructions the CPU is given room to keep: 512 entries,
  // 12KB. A boot to the DOS prompt barely notices the size, since 256 entries
  // already skip the decode on two thirds of its instructions. Programs that
  // run longer do notice, because an interpreter's loop is larger than a
  // boot's. See src/cpu/AGENTS.md.
  Y_PLATFORM_DECODE_CACHE_ENTRIES = 512,

  // The memory map is indexed by page so that a lookup is a load rather than a
  // walk. 4KB pages over the 8086's 1MB address space, which is 256 bytes of
  // index - small enough to keep in the platform state, and coarse enough that
  // every region the machine registers is page aligned.
  Y_PLATFORM_MEMORY_PAGE_SHIFT = 12,
  Y_PLATFORM_MEMORY_PAGE_SIZE = 1 << Y_PLATFORM_MEMORY_PAGE_SHIFT,
  Y_PLATFORM_MEMORY_ADDRESS_SPACE_SIZE = 0x100000,
  Y_PLATFORM_NUM_MEMORY_PAGES = Y_PLATFORM_MEMORY_ADDRESS_SPACE_SIZE >>
      Y_PLATFORM_MEMORY_PAGE_SHIFT,
  // A page no entry covers.
  Y_PLATFORM_MEMORY_PAGE_UNMAPPED = 0xFF,
  // A page more than one entry has a share of, which has to be resolved by
  // address. Nothing registers such a region today - every one of them is page
  // aligned - but the index must not quietly answer the wrong entry if one
  // ever does. Note that this is never the answer for an address above the
  // address space: no entry may reach one, so it is unmapped instead.
  Y_PLATFORM_MEMORY_PAGE_STRADDLED = 0xFE,

  // Maximum size of conventional memory in bytes, direct and paged together.
  Y_PLATFORM_MAX_CONVENTIONAL_MEMORY_SIZE = 640 * 1024,
  // Minimum size of conventional memory in bytes, direct and paged together.
  Y_PLATFORM_MIN_CONVENTIONAL_MEMORY_SIZE = 64 * 1024,
};

enum {
  // The largest page_shift, 4KB pages: the memory map indexes regions in 4KB
  // pages, and a larger paged page would span two of them.
  Y_PLATFORM_MAX_PAGED_MEMORY_PAGE_SHIFT = Y_PLATFORM_MEMORY_PAGE_SHIFT,
};

// Paged memory as a host gives it to the platform. Pages, frames, the page
// table and its bits (Y_CPU_PAGED_DATA_*) are described above
// Y_CPU_PAGED_DATA_RESIDENT. The platform hands pages, frames and page_shift
// to the CPU. What this adds is page_fault_fn, the host's handler for every
// access the CPU cannot make in a frame.
//
// A frame may be handed to another page only inside page_fault_fn.
// Instruction fetch holds a pointer into one frame, and the platform drops it
// whenever page_fault_fn returns. Reassigning a frame anywhere else needs a
// call to YPlatformUpdateAfterMemoryMapChange() afterwards. Clearing bits
// outside page_fault_fn needs nothing, because the next access the entry
// refuses comes to page_fault_fn.
typedef struct YPlatformPagedMemory {
  // The host's own data, for page_fault_fn to use.
  void* context;
  // The region's size in bytes, a whole number of pages.
  uint32_t size;
  // log2 of the page size: a page, and a frame, is 1 << page_shift bytes. At
  // most Y_PLATFORM_MAX_PAGED_MEMORY_PAGE_SHIFT.
  uint8_t page_shift;
  // The page table: size >> page_shift entries, entry i for page i.
  uint16_t* pages;
  // The frames: frame n is the 1 << page_shift bytes starting
  // n << page_shift bytes in.
  uint8_t* frames;
  // The host's page fault handler. Required.
  //
  // When it is called: on a read of a byte in a page whose entry lacks
  // RESIDENT, or a write to a byte in a page whose entry lacks RESIDENT or
  // DIRTY. The access can be the CPU's data access or instruction fetch, a DMA
  // transfer, or a host call to YPlatformReadMemoryByte() or
  // YPlatformWriteMemoryByte(). It is called once per such access, before the
  // access is made, and emulation waits for it to return.
  //
  // Parameters:
  //   - memory: this YPlatformPagedMemory, for context.
  //   - page: the page's index in pages, counted from the start of the
  //     region, not a guest address. Its first byte is at guest address
  //     direct_memory_size + (page << page_shift).
  //   - write: true for a write, false for a read or an instruction fetch.
  //
  // What it must do: set pages[page] so that it allows the access, that is
  // RESIDENT, plus DIRTY if write is true. Typically that means choosing a
  // frame, writing back the page already in it if that one is DIRTY, loading
  // this page into it and setting this page's entry. A page that is still in
  // its frame, with RESIDENT cleared to sample use, needs only RESIDENT set
  // again. The access then proceeds in the frame. If the entry still refuses
  // it, the read gives 0xFF and the write is dropped, as for unmapped memory.
  void (*page_fault_fn)(
      struct YPlatformPagedMemory* memory, uint32_t page, bool write);
} YPlatformPagedMemory;

// A memory map entry for a region in logical address space. Memory regions
// should not overlap.
typedef struct YPlatformMemoryMapEntry {
  // Custom data passed through to callbacks.
  void* context;

  // The memory map entry type, such as
  // Y_PLATFORM_MEMORY_MAP_ENTRY_CONVENTIONAL.
  YPlatformMemoryMapEntryType entry_type;
  // Start address of the memory region.
  uint32_t start;
  // Inclusive end address of the memory region.
  uint32_t end;

  // Direct pointer to the bytes backing this region, covering [start, end].
  //
  // Most regions are plain storage - RAM, and ROM images the caller already
  // holds in an array - and reading them through a callback costs an indirect
  // call per byte for no benefit. When these are set, accesses go straight to
  // the buffer and the callbacks below are not consulted.
  //
  // The buffer must remain valid, and must not move, for the lifetime of the
  // platform. A region whose accesses do something other than load or store a
  // value must leave the corresponding pointer NULL and supply a callback
  // instead. The two are independent: a region may serve reads directly while
  // routing writes through write_byte_fn, which is what an adapter that has
  // to notice writes - to track which parts of a framebuffer are dirty, say -
  // would want.
  //
  // read_data and write_data are separate pointers, rather than one pointer
  // plus a read/write/both flag, so that a read-only region can point at a
  // genuinely const array - the BIOS and option ROM images are compiled in as
  // const uint8_t[] - instead of casting away const to fit a single
  // non-const field. That keeps "this region cannot be written" a property
  // the compiler checks at the assignment into write_data, rather than a
  // runtime flag that a bug could get past; on a target where ROM is XIP
  // flash, writing through it faults rather than silently doing nothing.
  const uint8_t* read_data;
  // Direct pointer for writes. NULL for a read-only region such as a ROM,
  // whose writes are discarded.
  uint8_t* write_data;

  // Callback to read a byte from the memory map entry, where address is
  // relative to the start of the entry. Ignored if read_data is set.
  uint8_t (*read_byte_fn)(
      struct YPlatformMemoryMapEntry* entry, uint32_t relative_address);
  // Callback to write a byte to memory, where address is relative to the start
  // address. Ignored if write_data is set.
  void (*write_byte_fn)(
      struct YPlatformMemoryMapEntry* entry, uint32_t relative_address,
      uint8_t value);

  // Paged memory, as described at YPlatformPagedMemory, for a region with no
  // direct pointers or callbacks. Its size must be exactly end - start + 1.
  YPlatformPagedMemory* paged_memory;
} YPlatformMemoryMapEntry;

// Register a memory map entry in the platform state. Returns true if the entry
// was successfully registered, or false if:
//   - The entry's memory region does not lie within the address space.
//   - There already exists a memory map entry with the same type.
//   - The new entry's memory region overlaps with an existing entry.
//   - The number of memory map entries would exceed
//   Y_PLATFORM_MAX_MEMORY_MAP_ENTRIES.
//
// This does not tell the CPU that the map has changed. Follow a registration,
// or a run of them, with YPlatformUpdateAfterMemoryMapChange(). Skipping it
// leaves the CPU free to reuse a decoded instruction it took from an address
// that was unmapped at the time and so read as open bus - which no page
// generation catches, because nothing was written. YPlatformInit() registers
// every region the machine has and updates once at the end.
Y_PUBLIC bool YPlatformRegisterMemoryMapEntry(
    struct YPlatformState* platform, const YPlatformMemoryMapEntry* entry);
// Discard what the CPU derives from the memory map: its instruction fetch
// window, its direct data window and its decode cache.
Y_PUBLIC void YPlatformUpdateAfterMemoryMapChange(
    struct YPlatformState* platform);
// Look up the memory map entry corresponding to an address. Returns NULL if the
// address is not mapped to a known memory map entry.
Y_PUBLIC YPlatformMemoryMapEntry* YPlatformGetMemoryMapEntryForAddress(
    struct YPlatformState* platform, uint32_t address);
// Look up a memory map entry by type. Returns NULL if no entry found with the
// specified type.
Y_PUBLIC YPlatformMemoryMapEntry* YPlatformGetMemoryMapEntryByType(
    struct YPlatformState* platform, YPlatformMemoryMapEntryType entry_type);

// Read a byte from a logical memory address, either directly from the
// corresponding memory map entry's read_data buffer or via its read_byte_fn
// callback.
//
// On the 8086, accessing an invalid memory address will yield garbage data
// rather than causing a page fault. This interface mirrors that behavior.
Y_PUBLIC uint8_t
YPlatformReadMemoryByte(struct YPlatformState* platform, uint32_t address);
// Read a word from a logical memory address, either directly from the
// corresponding memory map entry's read_data buffer or via its read_byte_fn
// callback.
Y_PUBLIC uint16_t
YPlatformReadMemoryWord(struct YPlatformState* platform, uint32_t address);
// Write a byte to a logical memory address, either directly to the
// corresponding memory map entry's write_data buffer or via its
// write_byte_fn callback.
//
// On the 8086, accessing an invalid memory address will yield garbage data
// rather than causing a page fault. This interface mirrors that behavior.
Y_PUBLIC void YPlatformWriteMemoryByte(
    struct YPlatformState* platform, uint32_t address, uint8_t value);

// ============================================================================
// I/O port mapping
// ============================================================================

// Type ID of an I/O port map entry.
typedef uint16_t YPlatformPortMapEntryType;

enum {
  // Maximum number of I/O port mapping entries.
  Y_PLATFORM_MAX_PORT_MAP_ENTRIES = 16,
  // I/O port map entry for the master PIC (ports 0x20-0x21).
  Y_PLATFORM_PORT_MAP_ENTRY_PIC = 0x20,
  // I/O port map entry for the PIT (ports 0x40-0x43).
  Y_PLATFORM_PORT_MAP_ENTRY_PIT = 0x40,
  // I/O port map entry for the PPI (ports 0x60-0x63).
  Y_PLATFORM_PORT_MAP_ENTRY_PPI = 0x60,
  // I/O port map entry for the video adapter (ports 0x3B0-0x3BF or
  // 0x3D0-0x3DF).
  Y_PLATFORM_PORT_MAP_ENTRY_VIDEO = 0x10,
  // I/O port map entry for the FDC (ports 0x3F0-0x3F7).
  Y_PLATFORM_PORT_MAP_ENTRY_FDC = 0x3F0,
  // I/O port map entry for the HDC (ports 0x300-0x30F).
  Y_PLATFORM_PORT_MAP_ENTRY_HDC = 0x300,
  // I/O port map entry for the DMA controller (ports 0x00-0x0F).
  Y_PLATFORM_PORT_MAP_ENTRY_DMA = 0x00,
  // I/O port map entry for the DMA Page Registers (ports 0x80-0x8F).
  Y_PLATFORM_PORT_MAP_ENTRY_DMA_PAGE = 0x80,
};

// An I/O port map entry. Entries should not overlap.
typedef struct YPlatformPortMapEntry {
  // Custom data passed through to callbacks.
  void* context;

  // The I/O port map entry type.
  YPlatformPortMapEntryType entry_type;
  // Start of the I/O port range.
  uint16_t start;
  // Inclusive end of the I/O port range.
  uint16_t end;
  // Callback to read a byte from an I/O port within the range.
  uint8_t (*read_byte)(struct YPlatformPortMapEntry* entry, uint16_t port);
  // Callback to write a byte an I/O port within the range.
  void (*write_byte)(
      struct YPlatformPortMapEntry* entry, uint16_t port, uint8_t value);
} YPlatformPortMapEntry;

// Register an I/O port map entry in the platform state. Returns true if the
// entry was successfully registered, or false if:
//   - There already exists an I/O port map entry with the same type.
//   - The new entry's I/O port range overlaps with an existing entry.
Y_PUBLIC bool YPlatformRegisterPortMapEntry(
    struct YPlatformState* platform, const YPlatformPortMapEntry* entry);
// Look up the I/O port map entry corresponding to a port. Returns NULL if the
// port is not mapped to a known I/O port map entry.
Y_PUBLIC YPlatformPortMapEntry* YPlatformGetPortMapEntryForPort(
    struct YPlatformState* platform, uint16_t port);
// Look up an I/O port map entry by type. Returns NULL if no entry found with
// the specified type.
Y_PUBLIC YPlatformPortMapEntry* YPlatformGetPortMapEntryByType(
    struct YPlatformState* platform, YPlatformPortMapEntryType entry_type);

// Read a byte from an I/O port by invoking the corresponding I/O port map
// entry's read_byte callback.
Y_PUBLIC uint8_t
YPlatformReadPortByte(struct YPlatformState* platform, uint16_t port);
// Write a byte to an I/O port by invoking the corresponding I/O port map
// entry's write_byte callback.
Y_PUBLIC void YPlatformWritePortByte(
    struct YPlatformState* platform, uint16_t port, uint8_t value);

// ============================================================================
// Execution control
// ============================================================================

// Result of running the platform.
typedef enum YPlatformRunStatus {
  // The machine is running normally. Also returned by YPlatformRun() when it
  // exhausts its tick budget without stopping.
  Y_PLATFORM_RUNNING = 0,
  // The instruction at CS:IP could not be fetched or executed.
  Y_PLATFORM_INVALID,
  // The CPU halted with interrupts disabled and no interrupt pending, so
  // nothing can ever wake it. This is how GLaBIOS signals a fatal error.
  Y_PLATFORM_HUNG,
} YPlatformRunStatus;

// ============================================================================
// Platform state
// ============================================================================

// Caller-provided runtime configuration.
typedef struct YPlatformConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger configuration, shared by the platform and every module it owns. If
  // NULL, logging is disabled. The configuration is owned by the caller and
  // must outlive the platform.
  //
  // The platform stamps each log with its tick count and the CPU's CS:IP
  // itself - see YLoggerSource - so a host only says where lines go and
  // which of them it wants.
  YLoggerConfig* logger_config;

  // The size in bytes of direct_memory. Conventional memory is this plus
  // paged_memory's size, and must come to between 64K and 640K; with
  // paged_memory this may be 0.
  uint32_t direct_memory_size;

  // The video adapter installed in the machine. This also determines the
  // display type reported by the PPI's DIP switches, which is what the BIOS
  // branches on when it programs the adapter.
  YVideoAdapter video_adapter;

  // The number of floppy drives, 1 to 4, which the PPI's DIP switches report
  // to the BIOS. 0, the default, is taken as 1.
  uint8_t floppy_drives;

  // Conventional memory that the CPU reads and writes directly, through its
  // direct data window: direct_memory_size bytes from address 0. Required
  // unless direct_memory_size is 0, and YPlatformInit() fails without it.
  //
  // The caller owns the buffer and it must outlive the platform. Conventional
  // memory is plain storage, so the platform reads and writes it directly
  // rather than through a callback; regions that need a callback register one
  // themselves via YPlatformMemoryMapEntry.
  //
  // Addresses above conventional memory are not an error: on the 8086 an
  // access outside installed memory yields garbage rather than faulting, and
  // unmapped reads here return 0xFF.
  uint8_t* direct_memory;

  // Paged conventional memory, as described at YPlatformPagedMemory, or NULL
  // for none. It starts where direct_memory ends, at direct_memory_size, which
  // must then be a multiple of 4KB.
  //
  // An access to it costs more than one to direct_memory even when its page
  // is in a frame: a call and two more loads, against a compare and a load.
  // But direct_memory takes host memory that could have been frames, so where
  // to split the two is the host's to measure.
  //
  // The caller owns it and it must outlive the platform.
  YPlatformPagedMemory* paged_memory;

  // The video adapter's memory, at least vram_size bytes for the adapter named
  // by video_adapter. Required - YPlatformInit() fails without it.
  //
  // Separate from direct_memory because video memory sits above conventional
  // memory in the address space, at 0xB0000 or 0xB8000. A host whose memory
  // buffer spans the whole megabyte can point this into it.
  //
  // The caller owns the buffer and it must outlive the platform.
  uint8_t* vram;

  // The font text modes are drawn in, for a display too narrow for the
  // adapter's own - see YVideoTextFont. All zeros, the default, keeps the
  // adapter's font. The caller owns the glyphs and they must outlive the
  // platform.
  YVideoTextFont video_text_font;

  // Callback invoked when the PC speaker's output changes. frequency_hz is the
  // square wave frequency the speaker should emit, or 0 to turn it off. May be
  // NULL, in which case the speaker is silent.
  //
  // The speaker sounds when PIT channel 2 is producing a tone and both PPI
  // port B bits 0 and 1 are set. This reports current state rather than a
  // stream of events - see YPITConfig.set_pc_speaker_frequency.
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
      struct YPlatformState* platform, uint32_t frequency_hz);

} YPlatformConfig;

Y_STATIC_VECTOR_TYPE(
    YPlatformMemoryMap, YPlatformMemoryMapEntry,
    Y_PLATFORM_MAX_MEMORY_MAP_ENTRIES)
Y_STATIC_VECTOR_TYPE(
    YPlatformPortMap, YPlatformPortMapEntry, Y_PLATFORM_MAX_PORT_MAP_ENTRIES)

// State of the platform.
enum {
  // The CPU clock, in cycles per second. 4.77MHz is the IBM PC/XT's 14.318MHz
  // crystal divided by three.
  Y_PLATFORM_CPU_CYCLES_PER_SECOND = 4772727,
  // Cycles per millisecond, near enough for the devices that want one.
  Y_PLATFORM_CYCLES_PER_MILLISECOND = Y_PLATFORM_CPU_CYCLES_PER_SECOND / 1000,
  // The PIT is clocked at 1.193MHz, the same crystal divided by twelve, which
  // is a quarter of the CPU clock.
  Y_PLATFORM_CYCLES_PER_PIT_TICK = 4,
  // How often to step the floppy controller's state machine. This is not a
  // clock ratio - the controller is a state machine here rather than a
  // modelled device - so it keeps the rate it had before cycles were counted,
  // to leave floppy timing where it was.
  Y_PLATFORM_CYCLES_PER_FDC_TICK = 20,
};

typedef struct YPlatformState {
  // Caller-provided runtime configuration. Filled in before YPlatformInit(),
  // which is where it is checked, and read-only afterwards.
  YPlatformConfig config;

  // Logger shared by the platform and every module it owns.
  YLogger logger;

  // CPU state, which holds its own configuration.
  YCPUState cpu;
  // Storage for the CPU's decode cache, handed to it through
  // YCPUConfig.decode_cache.
  YCPUDecodeCacheEntry cpu_decode_cache[Y_PLATFORM_DECODE_CACHE_ENTRIES];

  // PIC state.
  YPICState pic;

  // PIT state.
  YPITState pit;

  // PPI state.
  YPPIState ppi;

  // Keyboard state.
  YKeyboardState keyboard;

  // DMA controller runtime configuration.
  // DMA controller state.
  YDMAState dma;

  // FDC state.
  YFDCState fdc;

  // HDC state.
  YHDCState hdc;

  // Video state.
  YVideoState video;

  // Memory map.
  YPlatformMemoryMap memory_map;
  // Which memory map entry owns each page of the address space, or
  // Y_PLATFORM_MEMORY_PAGE_UNMAPPED / Y_PLATFORM_MEMORY_PAGE_STRADDLED.
  // Extended by each registration, which only happens while the machine is
  // being set up.
  uint8_t memory_page_map[Y_PLATFORM_NUM_MEMORY_PAGES];
  // I/O port map.
  YPlatformPortMap io_port_map;

  // How many CPU clock cycles have run, at 4.77MHz. Every device in the
  // machine is clocked from this, so that what the guest measures with the
  // timer matches how long it spent executing.
  uint32_t ticks;

  // Cycles counted towards each device's next tick but not yet used by it.
  uint32_t pit_cycles;
  uint32_t fdc_cycles;
  uint32_t keyboard_cycles;

  // Devices are driven from deadlines rather than clocked on every
  // instruction. The PIT alone would otherwise want ticking about 2.5 times
  // per instruction, three channels at a time, almost always to decrement a
  // counter nothing is watching.
  //
  // Instead each device is brought up to date only when it has something to
  // do, and the instruction path does one comparison against the earliest
  // deadline of any of them. Anything that reads a device's state has to bring
  // that device up to date first, which is what the YPlatformSync* functions
  // do.

  // Whether the DOS idle skip is on - see YPlatformSetDOSIdleSkip(), which is
  // its sole writer. Off after YPlatformInit().
  bool is_dos_idle_skip_enabled;
  // Set when the guest has said it has nothing to do, and cleared by
  // YPlatformRun() once it has skipped the idle time. Only ever set while the
  // idle skip is on.
  bool is_guest_idle;

  // Value of ticks when the devices were last brought up to date.
  uint32_t last_sync_ticks;
  // Value of ticks at which the earliest device deadline falls due. Compared
  // as a signed difference against ticks so that it stays correct when the
  // 32-bit cycle counter wraps.
  uint32_t next_event_ticks;

} YPlatformState;

// Initialize the platform state. Returns true if the platform state was
// successfully initialized, or false if:
//   - Conventional memory, direct and paged, is not between 64K and 640K.
//   - direct_memory_size is not 0 but no direct_memory buffer was provided.
//   - paged_memory is set but invalid, or does not start on a 4KB boundary.
//   - No video memory buffer was provided.
// The caller zero-initializes the YPlatformState and fills in the fields of its
// config first. Nothing here zeroes anything: every module's state is a field
// of this one, so the caller's single zeroing covers all of them, and each
// module's init sets only what wants a non-zero value.
Y_PUBLIC bool YPlatformInit(YPlatformState* platform);

// Raise a hardware interrupt to the CPU via the PIC. Returns true if the
// IRQ was successfully raised, or false if the IRQ number is invalid.
Y_PUBLIC bool YPlatformRaiseIRQ(YPlatformState* platform, uint8_t irq);

// Execute one instruction, and bring any device whose deadline has come due up
// to date with the cycles it took.
//
// Note that this is an instruction, not a cycle: a tick runs the instruction at
// CS:IP to completion and charges the clock whatever that cost, so a REP string
// instruction can retire as a single tick worth thousands of cycles. A halted
// CPU retires no instruction but still advances the clock, so that whatever is
// meant to wake it can.
//
// Exactly one, which is what makes this the entry point to step a machine
// with: a host that wants to stop somewhere drives this in a loop and inspects
// the CPU itself. YPlatformRun() batches instructions into a tick instead.
//
// Returns Y_PLATFORM_RUNNING if the machine should keep running.
Y_PUBLIC YPlatformRunStatus YPlatformTick(YPlatformState* platform);

// Run up to max_ticks cycles of the platform, stopping early if a tick returns
// anything other than Y_PLATFORM_RUNNING. Returns the status of the tick that
// stopped the run, or Y_PLATFORM_RUNNING if the full budget was consumed.
//
// Unlike YPlatformTick(), a tick here may run several instructions back to
// back, never past the point where a device is due to be serviced or something
// else needs acting on at an instruction boundary. Nothing observable moves:
// every device still sees every cycle and an interrupt is still delivered
// where it would have been. What is saved is the per-tick work.
//
// max_cycles must be well under 2^31. Progress is measured as an unsigned
// difference from the tick count this call started at, which is what keeps it
// correct across the 32-bit cycle counter wrapping - every 15 minutes of
// emulated time - but that also means a budget approaching 2^32 can never be
// reached, because the difference wraps to zero first. A budget of a display
// frame or so, which is what a host driving the machine in real time passes,
// is nowhere near this.
Y_PUBLIC YPlatformRunStatus
YPlatformRun(YPlatformState* platform, uint32_t max_cycles);

// Turn on or off skipping emulated time when the guest says it has nothing to
// do. Off after YPlatformInit(), and may be switched at any point after it.
//
// MS-DOS waits for a keystroke by polling, not by halting, so an idle command
// prompt costs exactly as much to emulate as a running program - about 390,000
// guest instructions a second, all of them the same loop. It does however
// issue INT 28h, the documented DOS idle interrupt, on each pass: measured at
// the MS-DOS 3.30 prompt, 783 times a second.
//
// With this on, that interrupt makes YPlatformRun() advance the clock straight
// to whichever comes first, the next device deadline or the end of the budget
// it was given, instead of executing the loop until it gets there. The guest's
// own handler still runs; only the waiting is skipped.
//
// Time is advanced rather than discarded, so every device still sees every
// cycle and the guest's timer tick count is unchanged. What does change is
// that a program timing a loop from inside its own INT 28h handler would see
// time jump, which is why this is opt in.
//
// A setter rather than a config field, because the skip is a callback on the
// CPU's interrupt path as well as a flag, and a host may want it only once DOS
// has reached its prompt, which leaves everything before that exactly as it
// would be without it.
Y_PUBLIC void YPlatformSetDOSIdleSkip(YPlatformState* platform, bool enable);

// Bring every device up to date with the cycles that have run so far.
//
// Devices are driven from deadlines rather than clocked on every instruction,
// so between deadlines their state lags the CPU. Everything inside the
// platform that reads device state does this for itself, so a caller only
// needs it before inspecting a device directly - most usefully before
// YVideoRender(), so that the frame reflects where the CRT beam has actually
// reached.
Y_PUBLIC void YPlatformSync(YPlatformState* platform);

#endif  // YAX86_PLATFORM_PUBLIC_H


// ==============================================================================
// src/platform/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/platform/platform.c start
// ==============================================================================

#line 1 "./src/platform/platform.c"
#include "bios.h"
#include "pic.h"
#include "ppi.h"

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

#define Y_PLATFORM_ERROR(...) \
  Y_LOG(&platform->logger, &YPlatformLogModule, Y_LOG_LEVEL_ERROR, __VA_ARGS__)
#define Y_PLATFORM_WARN(...) \
  Y_LOG(&platform->logger, &YPlatformLogModule, Y_LOG_LEVEL_WARN, __VA_ARGS__)
#define Y_PLATFORM_INFO(...) \
  Y_LOG(&platform->logger, &YPlatformLogModule, Y_LOG_LEVEL_INFO, __VA_ARGS__)
#define Y_PLATFORM_DEBUG(...) \
  Y_LOG(&platform->logger, &YPlatformLogModule, Y_LOG_LEVEL_DEBUG, __VA_ARGS__)

// The memory and port accessors' logs, out of line. Built where they are
// logged, a log's fields take room in the accessor around them whether or not
// they are ever written, and the accessors are on the hot path.
Y_FILE_PRIVATE Y_NOINLINE void YPlatformLogUnmappedMemoryRead(
    YPlatformState* platform, uint32_t address) {
  Y_PLATFORM_DEBUG("unmapped_memory_read", Y_LOG_HEX20("address", address));
}

Y_FILE_PRIVATE Y_NOINLINE void YPlatformLogDiscardedMemoryWrite(
    YPlatformState* platform, uint32_t address, uint8_t value) {
  Y_PLATFORM_DEBUG(
      "discarded_memory_write", Y_LOG_HEX20("address", address),
      Y_LOG_HEX8("value", value));
}

Y_FILE_PRIVATE Y_NOINLINE void YPlatformLogUnmappedPortRead(
    YPlatformState* platform, uint16_t port) {
  Y_PLATFORM_WARN("unmapped_port_read", Y_LOG_HEX16("port", port));
}

Y_FILE_PRIVATE Y_NOINLINE void YPlatformLogUnmappedPortWrite(
    YPlatformState* platform, uint16_t port, uint8_t value) {
  Y_PLATFORM_WARN(
      "unmapped_port_write", Y_LOG_HEX16("port", port),
      Y_LOG_HEX8("value", value));
}

// How long until the next device needs attention, in CPU cycles from now.
Y_FILE_PRIVATE uint32_t YPlatformCyclesUntilNextEvent(
    const YPlatformState* platform, uint32_t max_cycles);

// Hand the CPU conventional memory to index directly, or take it away again.
Y_FILE_PRIVATE void YPlatformUpdateDirectDataWindow(YPlatformState* platform);
// Tell the CPU where paged memory is, or that there is none.
Y_FILE_PRIVATE void YPlatformUpdatePagedDataWindow(YPlatformState* platform);

enum {
  // Never let a deadline sit further out than this, so that a machine in which
  // nothing is scheduled still comes back regularly.
  //
  // Note that this is not a bound on how many cycles accumulate between syncs.
  // A deadline is only tested between instructions, and a REP string
  // instruction retires as a single tick charging thousands of cycles, so a
  // sync interval routinely runs past it - measured at 27,844 cycles, or
  // 5.8ms, over a DOS boot. Anything relying on a bound has to impose its own;
  // see Y_PLATFORM_MAX_IDLE_SKIP_CYCLES.
  Y_PLATFORM_MAX_EVENT_INTERVAL = Y_PLATFORM_CYCLES_PER_MILLISECOND,

  // The furthest an idle skip may move the clock in one go.
  //
  // A skip is otherwise bounded only by the budget its caller passed to
  // YPlatformRun(), which nothing checks. Every device's catch-up arithmetic in
  // YPlatformSync() is 32-bit and accumulates into a counter holding a
  // remainder, so a jump approaching 2^32 would overflow it, and the keyboard
  // is caught up a millisecond per iteration, so it would also spend ~900,000
  // of them in a single call. A tenth of a second is far more than any sane
  // budget and far less than either limit.
  Y_PLATFORM_MAX_IDLE_SKIP_CYCLES = Y_PLATFORM_CPU_CYCLES_PER_SECOND / 10,
};

// Record a newly registered entry in the page index.
//
// Takes the entry's index rather than the entry itself because the index is
// what goes into the map - the entry is derivable from it, and not the other
// way round.
//
// Only the pages the entry itself touches can change. Entries may not overlap,
// so nothing already in the index can have a share of one of them, and a
// registration never has to look at the rest of the map. Building the whole
// index therefore costs one pass over the address space between them, rather
// than one pass per entry.
Y_FILE_PRIVATE void YPlatformUpdateMemoryPageMapForEntry(
    YPlatformState* platform, uint8_t entry_index) {
  const YPlatformMemoryMapEntry* entry =
      YPlatformMemoryMapGet(&platform->memory_map, entry_index);
  for (uint32_t page = entry->start >> Y_PLATFORM_MEMORY_PAGE_SHIFT,
                last_page = entry->end >> Y_PLATFORM_MEMORY_PAGE_SHIFT;
       page <= last_page; ++page) {
    const uint32_t page_start = page << Y_PLATFORM_MEMORY_PAGE_SHIFT;
    const uint32_t page_end = page_start + Y_PLATFORM_MEMORY_PAGE_SIZE - 1;
    platform->memory_page_map[page] =
        entry->start <= page_start && entry->end >= page_end
            ? entry_index
            : Y_PLATFORM_MEMORY_PAGE_STRADDLED;
  }
}

// Whether a paged region is usable: a supported page size, a page table,
// frames and a page_fault_fn, and a size that is exactly the region's and a
// whole number of pages.
Y_FILE_PRIVATE bool YPlatformIsPagedMemoryValid(
    const YPlatformMemoryMapEntry* entry) {
  const YPlatformPagedMemory* memory = entry->paged_memory;
  if (memory->page_shift == 0 ||
      memory->page_shift > Y_PLATFORM_MAX_PAGED_MEMORY_PAGE_SHIFT ||
      memory->pages == NULL || memory->frames == NULL ||
      memory->page_fault_fn == NULL) {
    return false;
  }
  const uint32_t page_mask = ((uint32_t)1 << memory->page_shift) - 1;
  return memory->size == entry->end - entry->start + 1 &&
         (memory->size & page_mask) == 0;
}

// Register a memory map entry in the platform state. Returns true if the entry
// was successfully registered, or false if:
//   - The entry's memory region does not lie within the address space.
//   - There already exists a memory map entry with the same type.
//   - The new entry's memory region overlaps with an existing entry.
//   - The number of memory map entries would exceed
//   Y_PLATFORM_MAX_MEMORY_MAP_ENTRIES.
// Records a memory map entry without telling anything that the map has
// changed. What the CPU derives from the map - its two windows and its decode
// cache - is left to the caller, so that a run of registrations pays for one
// recompute rather than one each.
Y_PUBLIC bool YPlatformRegisterMemoryMapEntry(
    YPlatformState* platform, const YPlatformMemoryMapEntry* entry) {
  // The index has a slot per page of the address space and none above it, so
  // an entry reaching past the top could not be recorded in it. Rejecting one
  // here is what lets the lookup treat the index as the whole answer.
  if (entry->end >= Y_PLATFORM_MEMORY_ADDRESS_SPACE_SIZE ||
      entry->start > entry->end) {
    return false;
  }
  if (entry->paged_memory && !YPlatformIsPagedMemoryValid(entry)) {
    return false;
  }
  if (YPlatformMemoryMapLength(&platform->memory_map) >=
      Y_PLATFORM_MAX_MEMORY_MAP_ENTRIES) {
    return false;
  }
  for (uint8_t i = 0; i < YPlatformMemoryMapLength(&platform->memory_map);
       ++i) {
    YPlatformMemoryMapEntry* existing_entry =
        YPlatformMemoryMapGet(&platform->memory_map, i);
    if (existing_entry->entry_type == entry->entry_type) {
      return false;
    }
    if (!(existing_entry->start > entry->end ||
          entry->start > existing_entry->end)) {
      return false;
    }
  }
  if (!YPlatformMemoryMapAppend(&platform->memory_map, entry)) {
    return false;
  }
  YPlatformUpdateMemoryPageMapForEntry(
      platform, YPlatformMemoryMapLength(&platform->memory_map) - 1);
  return true;
}

Y_PUBLIC void YPlatformUpdateAfterMemoryMapChange(YPlatformState* platform) {
  // The data window is whichever entry covers address 0, which a registration
  // may have just become.
  YPlatformUpdateDirectDataWindow(platform);
  YPlatformUpdatePagedDataWindow(platform);
  // A decode taken before a region was mapped came from open bus. No page
  // generation moved, because nothing was written - what the bytes at those
  // addresses mean has changed.
  YCPUInvalidateDecodeCache(&platform->cpu);
}

// What the page index has to say about an address: the entry covering the whole
// of its page, Y_PLATFORM_MEMORY_PAGE_UNMAPPED where no entry covers it, or
// Y_PLATFORM_MEMORY_PAGE_STRADDLED where more than one entry has a share of the
// page and the caller has to walk the map. An address above the address space
// has no page, and no entry may reach it, so it is unmapped by definition.
//
// Inline rather than a function of its own: the body is a compare, a shift and
// a load, where a call on a core with no cheap way to do any of it is a push, a
// branch, a pop and a return.
Y_FILE_PRIVATE inline uint8_t YPlatformGetMemoryPageMapIndex(
    const YPlatformState* platform, uint32_t address) {
  return address < Y_PLATFORM_MEMORY_ADDRESS_SPACE_SIZE
             ? platform
                   ->memory_page_map[address >> Y_PLATFORM_MEMORY_PAGE_SHIFT]
             : Y_PLATFORM_MEMORY_PAGE_UNMAPPED;
}

// Look up the memory region corresponding to an address. Returns NULL if the
// address is not mapped to a known memory region.
Y_PUBLIC Y_HOT YPlatformMemoryMapEntry* YPlatformGetMemoryMapEntryForAddress(
    YPlatformState* platform, uint32_t address) {
  const uint8_t index = YPlatformGetMemoryPageMapIndex(platform, address);
  if (index < Y_PLATFORM_MAX_MEMORY_MAP_ENTRIES) {
    return YPlatformMemoryMapGet(&platform->memory_map, index);
  }
  if (index == Y_PLATFORM_MEMORY_PAGE_UNMAPPED) {
    return NULL;
  }
  // A page more than one entry has a share of, which the index cannot answer.
  for (uint8_t i = 0; i < YPlatformMemoryMapLength(&platform->memory_map);
       ++i) {
    YPlatformMemoryMapEntry* entry =
        YPlatformMemoryMapGet(&platform->memory_map, i);
    if (address >= entry->start && address <= entry->end) {
      return entry;
    }
  }
  return NULL;
}

// Look up a memory region by type. Returns NULL if no region found with the
// specified type.
Y_PUBLIC Y_HOT YPlatformMemoryMapEntry* YPlatformGetMemoryMapEntryByType(
    YPlatformState* platform, uint8_t entry_type) {
  for (uint8_t i = 0; i < YPlatformMemoryMapLength(&platform->memory_map);
       ++i) {
    YPlatformMemoryMapEntry* entry =
        YPlatformMemoryMapGet(&platform->memory_map, i);
    if (entry->entry_type == entry_type) {
      return entry;
    }
  }
  return NULL;
}

// Calls the host's page_fault_fn for a page and returns the page's entry
// afterwards. To make room the host may have moved another page out of the
// frame instruction fetch is reading from, so the fetch window is dropped and
// the next fetch opens a fresh one.
Y_FILE_PRIVATE Y_NOINLINE uint16_t YPlatformFaultPagedMemory(
    YPlatformState* platform, YPlatformPagedMemory* memory, uint32_t page,
    bool write) {
  memory->page_fault_fn(memory, page, write);
  YCPUInvalidateInstructionFetchWindow(&platform->cpu);
  return memory->pages[page];
}

// The byte at offset into the region, given the entry of the page it is in,
// which must have RESIDENT set.
Y_FILE_PRIVATE inline uint8_t* YPlatformGetPagedMemoryByte(
    const YPlatformPagedMemory* memory, uint16_t page_entry, uint32_t offset) {
  const uint32_t page_mask = ((uint32_t)1 << memory->page_shift) - 1;
  return memory->frames + (((uint32_t)(page_entry & Y_CPU_PAGED_DATA_FRAME_MASK)
                            << memory->page_shift) |
                           (offset & page_mask));
}

// Read the byte at offset into a paged region, calling page_fault_fn first if
// its page is not in a frame.
Y_FILE_PRIVATE Y_HOT uint8_t YPlatformReadPagedMemoryByte(
    YPlatformState* platform, YPlatformPagedMemory* memory, uint32_t offset) {
  const uint32_t page = offset >> memory->page_shift;
  uint16_t page_entry = memory->pages[page];
  if (!(page_entry & Y_CPU_PAGED_DATA_RESIDENT)) {
    page_entry = YPlatformFaultPagedMemory(platform, memory, page, false);
    if (!(page_entry & Y_CPU_PAGED_DATA_RESIDENT)) {
      return 0xFF;
    }
  }
  return *YPlatformGetPagedMemoryByte(memory, page_entry, offset);
}

// Write the byte at offset into a paged region, calling page_fault_fn first if
// its page is not in a frame or not writable.
Y_FILE_PRIVATE Y_HOT void YPlatformWritePagedMemoryByte(
    YPlatformState* platform, YPlatformPagedMemory* memory, uint32_t offset,
    uint8_t value) {
  const uint32_t page = offset >> memory->page_shift;
  // A write needs the page in its frame and already dirty; otherwise the host
  // is told, so that it can mark the page dirty.
  const uint16_t writable = Y_CPU_PAGED_DATA_RESIDENT | Y_CPU_PAGED_DATA_DIRTY;
  uint16_t page_entry = memory->pages[page];
  if ((page_entry & writable) != writable) {
    page_entry = YPlatformFaultPagedMemory(platform, memory, page, true);
    if ((page_entry & writable) != writable) {
      return;
    }
  }
  *YPlatformGetPagedMemoryByte(memory, page_entry, offset) = value;
}

// Read a byte from a logical memory address.
Y_PUBLIC Y_HOT uint8_t
YPlatformReadMemoryByte(YPlatformState* platform, uint32_t address) {
  YPlatformMemoryMapEntry* entry =
      YPlatformGetMemoryMapEntryForAddress(platform, address);
  if (entry) {
    // Plain storage, which is what every region except video memory is. Going
    // straight to the buffer saves an indirect call on the hottest path in the
    // emulator: every byte of every instruction is fetched through here.
    if (entry->read_data) {
      return entry->read_data[address - entry->start];
    }
    if (entry->read_byte_fn) {
      return entry->read_byte_fn(entry, address - entry->start);
    }
    // Last, so that nothing above pays for it.
    if (entry->paged_memory) {
      return YPlatformReadPagedMemoryByte(
          platform, entry->paged_memory, address - entry->start);
    }
  }
  // Logged at debug rather than warning level: scanning unmapped memory is
  // normal on a PC/XT. GLaBIOS reads every byte of 0xF6000-0xF7FFF looking
  // for option ROMs, for instance.
  YPlatformLogUnmappedMemoryRead(platform, address);
  return 0xFF;
}

// Read a word from a logical memory address.
Y_PUBLIC uint16_t
YPlatformReadMemoryWord(YPlatformState* platform, uint32_t address) {
  uint8_t low_byte = YPlatformReadMemoryByte(platform, address);
  uint8_t high_byte = YPlatformReadMemoryByte(platform, address + 1);
  return (high_byte << 8) | low_byte;
}

// Write a byte to a logical memory address.
Y_PUBLIC Y_HOT void YPlatformWriteMemoryByte(
    YPlatformState* platform, uint32_t address, uint8_t value) {
  // Writes that do not come from the CPU arrive here, and can land on bytes it
  // has already decoded. DMA is the one that matters: DOS loads itself over
  // the boot sector that way.
  YCPUNotifyMemoryWrite(&platform->cpu, address);
  YPlatformMemoryMapEntry* entry =
      YPlatformGetMemoryMapEntryForAddress(platform, address);
  if (entry) {
    if (entry->write_data) {
      entry->write_data[address - entry->start] = value;
      return;
    }
    if (entry->write_byte_fn) {
      entry->write_byte_fn(entry, address - entry->start, value);
      return;
    }
    if (entry->paged_memory) {
      YPlatformWritePagedMemoryByte(
          platform, entry->paged_memory, address - entry->start, value);
      return;
    }
  }
  // Either unmapped, or a read-only region such as a ROM. Both discard the
  // write, and both are logged.
  YPlatformLogDiscardedMemoryWrite(platform, address, value);
}

// Register an I/O port map entry in the platform state. Returns true if the
// entry was successfully registered, or false if:
//   - There already exists an I/O port map entry with the same type.
//   - The new entry's I/O port range overlaps with an existing entry.
Y_PUBLIC bool YPlatformRegisterPortMapEntry(
    YPlatformState* platform, const YPlatformPortMapEntry* entry) {
  if (YPlatformPortMapLength(&platform->io_port_map) >=
      Y_PLATFORM_MAX_PORT_MAP_ENTRIES) {
    return false;
  }
  for (uint8_t i = 0; i < YPlatformPortMapLength(&platform->io_port_map); ++i) {
    YPlatformPortMapEntry* existing_entry =
        YPlatformPortMapGet(&platform->io_port_map, i);
    if (existing_entry->entry_type == entry->entry_type) {
      return false;
    }
    if (!(existing_entry->start > entry->end ||
          entry->start > existing_entry->end)) {
      return false;
    }
  }
  return YPlatformPortMapAppend(&platform->io_port_map, entry);
}

// Look up the I/O port map entry corresponding to a port. Returns NULL if the
// port is not mapped to a known I/O port map entry.
Y_PUBLIC YPlatformPortMapEntry* YPlatformGetPortMapEntryForPort(
    YPlatformState* platform, uint16_t port) {
  for (uint8_t i = 0; i < YPlatformPortMapLength(&platform->io_port_map); ++i) {
    YPlatformPortMapEntry* entry =
        YPlatformPortMapGet(&platform->io_port_map, i);
    if (port >= entry->start && port <= entry->end) {
      return entry;
    }
  }
  return NULL;
}
// Look up an I/O port map entry by type. Returns NULL if no entry found with
// the specified type.
Y_PUBLIC Y_HOT YPlatformPortMapEntry* YPlatformGetPortMapEntryByType(
    YPlatformState* platform, YPlatformPortMapEntryType entry_type) {
  for (uint8_t i = 0; i < YPlatformPortMapLength(&platform->io_port_map); ++i) {
    YPlatformPortMapEntry* entry =
        YPlatformPortMapGet(&platform->io_port_map, i);
    if (entry->entry_type == entry_type) {
      return entry;
    }
  }
  return NULL;
}

// Read a byte from an I/O port by invoking the corresponding I/O port map
// entry's read_byte callback.
Y_PUBLIC Y_HOT uint8_t
YPlatformReadPortByte(YPlatformState* platform, uint16_t port) {
  YPlatformPortMapEntry* entry =
      YPlatformGetPortMapEntryForPort(platform, port);
  if (!entry || !entry->read_byte) {
    // Unlike unmapped memory, an unmapped port usually means a device is
    // missing from the port map, so this stays at warning level.
    YPlatformLogUnmappedPortRead(platform, port);
    return 0xFF;
  }
  return entry->read_byte(entry, port);
}

// Write a byte to an I/O port by invoking the corresponding I/O port map
// entry's write_byte callback.
Y_PUBLIC Y_HOT void YPlatformWritePortByte(
    YPlatformState* platform, uint16_t port, uint8_t value) {
  YPlatformPortMapEntry* entry =
      YPlatformGetPortMapEntryForPort(platform, port);
  if (!entry || !entry->write_byte) {
    YPlatformLogUnmappedPortWrite(platform, port, value);
    return;
  }
  entry->write_byte(entry, port, value);
}

// ============================================================================
// Callbacks for CPU module
// ============================================================================

Y_FILE_PRIVATE uint8_t
YPlatformCPUCallbackReadMemoryByte(YCPUState* cpu, uint32_t address) {
  return YPlatformReadMemoryByte((YPlatformState*)cpu->config.context, address);
}

// A CPU write to paged memory that the CPU could not make itself, because the
// page is not in a frame or not writable.
//
// This skips YPlatformWriteMemoryByte() on purpose. The CPU has already told
// its decode cache about the write, and YPlatformWriteMemoryByte() would tell
// it again. Counting a write twice changes when the cache discards decodes,
// which changes where runs of instructions end and so when the guest takes
// its timer interrupts. The number of frames decides which writes fault, so
// it would change what the guest does. Writes to every other region are
// still counted twice, and the instruction and cycle counts pico/bench's
// dos-boot workload is held to depend on that.
Y_FILE_PRIVATE Y_NOINLINE void YPlatformCPUWritePagedMemoryByte(
    YPlatformState* platform, uint32_t address, uint8_t value) {
  YPlatformMemoryMapEntry* entry =
      YPlatformGetMemoryMapEntryForAddress(platform, address);
  YPlatformWritePagedMemoryByte(
      platform, entry->paged_memory, address - entry->start, value);
}

Y_FILE_PRIVATE void YPlatformCPUCallbackWriteMemoryByte(
    YCPUState* cpu, uint32_t address, uint8_t value) {
  YPlatformState* platform = (YPlatformState*)cpu->config.context;
  if (address - cpu->paged_data_window.start < cpu->paged_data_window.size) {
    YPlatformCPUWritePagedMemoryByte(platform, address, value);
    return;
  }
  YPlatformWriteMemoryByte(platform, address, value);
}

Y_FILE_PRIVATE uint8_t
YPlatformCPUCallbackReadPortByte(YCPUState* cpu, uint16_t port) {
  return YPlatformReadPortByte((YPlatformState*)cpu->config.context, port);
}

Y_FILE_PRIVATE void YPlatformCPUCallbackWritePortByte(
    YCPUState* cpu, uint16_t port, uint8_t value) {
  YPlatformWritePortByte((YPlatformState*)cpu->config.context, port, value);
}

// ============================================================================
// Callbacks for 8259 PIC module
// ============================================================================

Y_FILE_PRIVATE uint8_t
YPlatformPICCallbackReadPortByte(YPlatformPortMapEntry* entry, uint16_t port) {
  return YPICReadPort((YPICState*)entry->context, port);
}

Y_FILE_PRIVATE Y_HOT void YPlatformPICCallbackWritePortByte(
    YPlatformPortMapEntry* entry, uint16_t port, uint8_t value) {
  YPICWritePort((YPICState*)entry->context, port, value);
}

Y_FILE_PRIVATE void YPlatformPICCallbackRaiseIRQ0(void* context) {
  YPlatformState* platform = (YPlatformState*)context;
  YPlatformRaiseIRQ(platform, 0);
}

// ============================================================================
// Callbacks for 8253 PIT module
// ============================================================================

Y_FILE_PRIVATE Y_HOT uint8_t
YPlatformPITCallbackReadPortByte(YPlatformPortMapEntry* entry, uint16_t port) {
  // A guest timing loop reads the counter expecting it to have moved, so the
  // PIT has to be caught up before it is read.
  YPlatformState* platform = (YPlatformState*)entry->context;
  YPlatformSync(platform);
  return YPITReadPort(&platform->pit, port);
}

Y_FILE_PRIVATE void YPlatformPITCallbackWritePortByte(
    YPlatformPortMapEntry* entry, uint16_t port, uint8_t value) {
  // Syncing first applies the cycles that ran under the old configuration;
  // syncing again afterwards reschedules against the new one.
  YPlatformState* platform = (YPlatformState*)entry->context;
  YPlatformSync(platform);
  YPITWritePort(&platform->pit, port, value);
  YPlatformSync(platform);
}

Y_FILE_PRIVATE void YPlatformPITCallbackSetPCSpeakerFrequency(
    void* context, uint32_t frequency_hz) {
  YPlatformState* platform = (YPlatformState*)context;
  YPPISetPCSpeakerFrequencyFromPIT(&platform->ppi, frequency_hz);
}

// ============================================================================
// Callbacks for 8255 PPI module
// ============================================================================

Y_FILE_PRIVATE uint8_t
YPlatformPPICallbackReadPortByte(YPlatformPortMapEntry* entry, uint16_t port) {
  YPlatformState* platform = (YPlatformState*)entry->context;
  YPlatformSync(platform);
  return YPPIReadPort(&platform->ppi, port);
}

Y_FILE_PRIVATE void YPlatformPPICallbackWritePortByte(
    YPlatformPortMapEntry* entry, uint16_t port, uint8_t value) {
  // Port B gates the speaker against PIT channel 2, so what the guest hears
  // depends on the channel's output state being current.
  YPlatformState* platform = (YPlatformState*)entry->context;
  YPlatformSync(platform);
  YPPIWritePort(&platform->ppi, port, value);
  YPlatformSync(platform);
}

Y_FILE_PRIVATE void YPlatformPPICallbackSetKeyboardControl(
    void* context, bool keyboard_enable_clear, bool keyboard_clock_low) {
  YPlatformState* platform = (YPlatformState*)context;
  YKeyboardHandleControl(
      &platform->keyboard, keyboard_enable_clear, keyboard_clock_low);
}

Y_FILE_PRIVATE void YPlatformPPICallbackSetPCSpeakerFrequency(
    void* context, uint32_t frequency_hz) {
  YPlatformState* platform = (YPlatformState*)context;
  if (platform->config.set_pc_speaker_frequency) {
    platform->config.set_pc_speaker_frequency(platform, frequency_hz);
  }
}

// ============================================================================
// Callbacks for Keyboard module
// ============================================================================

Y_FILE_PRIVATE void YPlatformKeyboardCallbackRaiseIRQ1(void* context) {
  YPlatformState* platform = (YPlatformState*)context;
  YPlatformRaiseIRQ(platform, 1);
}

Y_FILE_PRIVATE void YPlatformKeyboardCallbackSendScancode(
    void* context, uint8_t scancode) {
  YPlatformState* platform = (YPlatformState*)context;
  YPPISetScancode(&platform->ppi, scancode);
}

// ============================================================================
// Callbacks for uPD765 FDC module
// ============================================================================

enum {
  Y_PLATFORM_DMA_CHANNEL_FLOPPY = 2,
};

Y_FILE_PRIVATE void YPlatformFDCCallbackRaiseIRQ6(void* context) {
  YPlatformState* platform = (YPlatformState*)context;
  YPlatformRaiseIRQ(platform, 6);
}

Y_FILE_PRIVATE Y_HOT void YPlatformFDCCallbackRequestDMA(void* context) {
  YPlatformState* platform = (YPlatformState*)context;
  YDMATransferByte(&platform->dma, Y_PLATFORM_DMA_CHANNEL_FLOPPY);
}

Y_FILE_PRIVATE uint8_t
YPlatformFDCCallbackReadPortByte(YPlatformPortMapEntry* entry, uint16_t port) {
  YPlatformState* platform = (YPlatformState*)entry->context;
  YPlatformSync(platform);
  return YFDCReadPort(&platform->fdc, port);
}

Y_FILE_PRIVATE void YPlatformFDCCallbackWritePortByte(
    YPlatformPortMapEntry* entry, uint16_t port, uint8_t value) {
  // A write can start a command, which is what puts the controller into the
  // execution phase the scheduler watches for.
  YPlatformState* platform = (YPlatformState*)entry->context;
  YPlatformSync(platform);
  YFDCWritePort(&platform->fdc, port, value);
  YPlatformSync(platform);
}

// ============================================================================
// Callbacks for DMA module
// ============================================================================

Y_FILE_PRIVATE uint8_t
YPlatformDMACallbackReadMemoryByte(void* context, uint32_t address) {
  YPlatformState* platform = (YPlatformState*)context;
  return YPlatformReadMemoryByte(platform, address);
}

Y_FILE_PRIVATE void YPlatformDMACallbackWriteMemoryByte(
    void* context, uint32_t address, uint8_t value) {
  YPlatformState* platform = (YPlatformState*)context;
  YPlatformWriteMemoryByte(platform, address, value);
}

Y_FILE_PRIVATE uint8_t
YPlatformDMACallbackReadDeviceByte(void* context, uint8_t channel) {
  YPlatformState* platform = (YPlatformState*)context;
  switch (channel) {
    case Y_PLATFORM_DMA_CHANNEL_FLOPPY:
      return YFDCReadPort(&platform->fdc, Y_FDC_PORT_DATA);
    default:
      return 0xFF;
  }
}

Y_FILE_PRIVATE void YPlatformDMACallbackWriteDeviceByte(
    void* context, uint8_t channel, uint8_t value) {
  YPlatformState* platform = (YPlatformState*)context;
  switch (channel) {
    case Y_PLATFORM_DMA_CHANNEL_FLOPPY:
      YFDCWritePort(&platform->fdc, Y_FDC_PORT_DATA, value);
      break;
    default:
      break;
  }
}

Y_FILE_PRIVATE void YPlatformDMACallbackOnTerminalCount(
    void* context, uint8_t channel) {
  YPlatformState* platform = (YPlatformState*)context;
  switch (channel) {
    case Y_PLATFORM_DMA_CHANNEL_FLOPPY:
      YFDCHandleTC(&platform->fdc);
      break;
    default:
      break;
  }
}

Y_FILE_PRIVATE uint8_t
YPlatformDMACallbackReadPortByte(YPlatformPortMapEntry* entry, uint16_t port) {
  return YDMAReadPort((YDMAState*)entry->context, port);
}

Y_FILE_PRIVATE void YPlatformDMACallbackWritePortByte(
    YPlatformPortMapEntry* entry, uint16_t port, uint8_t value) {
  YDMAWritePort((YDMAState*)entry->context, port, value);
}

// ============================================================================
// Callbacks for Video module
// ============================================================================

Y_FILE_PRIVATE Y_HOT uint8_t YPlatformVideoCallbackReadPortByte(
    YPlatformPortMapEntry* entry, uint16_t port) {
  // The status port reports where the CRT beam is, which is only meaningful
  // once the beam has been advanced to now. Guests poll this to wait for
  // retrace.
  YPlatformState* platform = (YPlatformState*)entry->context;
  YPlatformSync(platform);
  return YVideoReadPort(&platform->video, port);
}

Y_FILE_PRIVATE void YPlatformVideoCallbackWritePortByte(
    YPlatformPortMapEntry* entry, uint16_t port, uint8_t value) {
  YPlatformState* platform = (YPlatformState*)entry->context;
  YPlatformSync(platform);
  YVideoWritePort(&platform->video, port, value);
}

Y_FILE_PRIVATE void YPlatformVideoCallbackWriteVRAMByte(
    YPlatformMemoryMapEntry* entry, uint32_t address, uint8_t value) {
  YVideoWriteVRAM((YVideoState*)entry->context, address, value);
}

// ============================================================================
// Callbacks for HDC module
// ============================================================================

Y_FILE_PRIVATE uint8_t
YPlatformHDCCallbackReadPortByte(YPlatformPortMapEntry* entry, uint16_t port) {
  return YHDCReadPort((YHDCState*)entry->context, port);
}

Y_FILE_PRIVATE void YPlatformHDCCallbackWritePortByte(
    YPlatformPortMapEntry* entry, uint16_t port, uint8_t value) {
  YHDCWritePort((YHDCState*)entry->context, port, value);
}

// ============================================================================
// Initialization
// ============================================================================

Y_FILE_PRIVATE void YPlatformInitBIOS(YPlatformState* platform) {
  uint32_t bios_size = YBIOSGetROMSize();
  YPlatformMemoryMapEntry bios_rom = {
      .context = NULL,
      .entry_type = Y_PLATFORM_MEMORY_MAP_ENTRY_BIOS_ROM,
      .start = Y_BIOS_ROM_START_ADDRESS,
      .end = Y_BIOS_ROM_START_ADDRESS + bios_size - 1,
      // The ROM image is a constant array in the library, so it is read
      // directly. write_data is left NULL because the BIOS ROM is read-only.
      .read_data = YBIOSGetROMData(),
  };
  YPlatformRegisterMemoryMapEntry(platform, &bios_rom);
}

// Runs the CPU's interrupt acknowledge cycle against the PIC. This is the
// only path by which an external interrupt reaches the CPU, and the PIC marks
// the interrupt in service as part of it - so a vector is never produced
// unless the CPU is taking it right now.
Y_FILE_PRIVATE bool YPlatformCPUCallbackAcknowledgeInterrupt(
    YCPUState* cpu, uint8_t* vector) {
  YPlatformState* platform = (YPlatformState*)cpu->config.context;
  const uint8_t interrupt_vector = YPICGetPendingInterrupt(&platform->pic);
  if (interrupt_vector == Y_PIC_NO_PENDING_INTERRUPT) {
    return false;
  } else {
    *vector = interrupt_vector;
    return true;
  }
}

enum {
  // The DOS idle interrupt. MS-DOS issues it from the loops in which it waits
  // for input, to tell anything listening that the machine has nothing to do.
  Y_PLATFORM_DOS_IDLE_INTERRUPT = 0x28,
};

// Notes the guest declaring itself idle. Nothing is serviced here - the guest's
// own handler runs as usual - so this only ever reports the interrupt onwards.
//
// Only installed while the idle skip is enabled, so a machine without it pays
// nothing per interrupt. YPlatformSetDOSIdleSkip() is what installs it.
Y_FILE_PRIVATE Y_HOT YCPUInterruptHandlerResult
YPlatformCPUCallbackHandleInterrupt(YCPUState* cpu, uint8_t interrupt_number) {
  if (interrupt_number == Y_PLATFORM_DOS_IDLE_INTERRUPT) {
    YPlatformState* platform = (YPlatformState*)cpu->config.context;
    platform->is_guest_idle = true;
  }
  return Y_CPU_INTERRUPT_HANDLER_UNHANDLED;
}

// Points instruction fetch at the frame holding the page an address is in,
// calling page_fault_fn first if the page is not in a frame.
//
// The window covers that one page, since the next page's frame can be
// anywhere; fetching past its end comes back here.
Y_FILE_PRIVATE void YPlatformGetPagedMemoryFetchWindow(
    YPlatformState* platform, const YPlatformMemoryMapEntry* entry,
    uint32_t address) {
  YPlatformPagedMemory* memory = entry->paged_memory;
  const uint32_t offset = address - entry->start;
  const uint32_t page = offset >> memory->page_shift;
  uint16_t page_entry = memory->pages[page];
  if (!(page_entry & Y_CPU_PAGED_DATA_RESIDENT)) {
    page_entry = YPlatformFaultPagedMemory(platform, memory, page, false);
    if (!(page_entry & Y_CPU_PAGED_DATA_RESIDENT)) {
      return;
    }
  }
  YCPUInstructionFetchWindow* window = &platform->cpu.instruction_fetch_window;
  window->data = YPlatformGetPagedMemoryByte(memory, page_entry, 0);
  window->start = entry->start + (page << memory->page_shift);
  window->end = window->start + ((uint32_t)1 << memory->page_shift);
}

// Hands the CPU a direct window for instruction fetch.
//
// Declines wherever a read has to be observed or computed rather than loaded:
// a device region, unmapped memory, or a page shared by two entries.
Y_FILE_PRIVATE void YPlatformCPUCallbackGetInstructionFetchWindow(
    YCPUState* cpu, uint32_t address) {
  YCPUInstructionFetchWindow* window = &cpu->instruction_fetch_window;
  window->data = NULL;
  YPlatformState* platform = (YPlatformState*)cpu->config.context;
  // Anything that is not a single entry's page - unmapped, or shared by more
  // than one entry - has no window to hand out.
  const uint8_t index = YPlatformGetMemoryPageMapIndex(platform, address);
  if (index >= Y_PLATFORM_MAX_MEMORY_MAP_ENTRIES) {
    return;
  }
  YPlatformMemoryMapEntry* entry =
      YPlatformMemoryMapGet(&platform->memory_map, index);
  if (entry->read_data == NULL) {
    if (entry->paged_memory) {
      YPlatformGetPagedMemoryFetchWindow(platform, entry, address);
    }
    return;
  }
  // The whole region, not the tail of it from this address, so that a jump
  // backwards within the region still lands inside the window.
  window->data = entry->read_data;
  window->start = entry->start;
  window->end = entry->end + 1;
}

Y_FILE_PRIVATE void YPlatformInitCPU(YPlatformState* platform) {
  platform->cpu.config.context = platform;
  platform->cpu.config.logger = &platform->logger;
  platform->cpu.config.read_memory_byte = YPlatformCPUCallbackReadMemoryByte;
  platform->cpu.config.get_instruction_fetch_window =
      YPlatformCPUCallbackGetInstructionFetchWindow;
  platform->cpu.config.write_memory_byte = YPlatformCPUCallbackWriteMemoryByte;
  platform->cpu.config.acknowledge_interrupt =
      YPlatformCPUCallbackAcknowledgeInterrupt;
  // Points into the PIC's own state, so the CPU sees a change the moment the
  // PIC makes one. YPICInit() below runs after this, which is fine - the
  // pointer is to storage that already exists, not to a value read now.
  platform->cpu.config.interrupt_request_hint =
      &platform->pic.has_unmasked_request;
  platform->cpu.config.decode_cache = platform->cpu_decode_cache;
  platform->cpu.config.decode_cache_num_entries =
      Y_PLATFORM_DECODE_CACHE_ENTRIES;
  platform->cpu.config.read_port = YPlatformCPUCallbackReadPortByte;
  platform->cpu.config.write_port = YPlatformCPUCallbackWritePortByte;
  YCPUInit(&platform->cpu);

  // Initialize CPU registers.
  // CS:IP points to the BIOS entry point at 0xFFFF0.
  platform->cpu.registers[Y_CPU_CS] = 0xF000;
  platform->cpu.registers[Y_CPU_IP] = 0xFFF0;
  platform->cpu.registers[Y_CPU_DS] = 0x0000;
  platform->cpu.registers[Y_CPU_SS] = 0x0000;
  platform->cpu.registers[Y_CPU_ES] = 0x0000;
  platform->cpu.registers[Y_CPU_SP] = 0xFFFE;
}

Y_FILE_PRIVATE void YPlatformInitMemoryMap(YPlatformState* platform) {
  YPlatformMemoryMapInit(&platform->memory_map);
  for (uint32_t page = 0; page < Y_PLATFORM_NUM_MEMORY_PAGES; ++page) {
    platform->memory_page_map[page] = Y_PLATFORM_MEMORY_PAGE_UNMAPPED;
  }
  const uint32_t direct_memory_size = platform->config.direct_memory_size;
  if (direct_memory_size > 0) {
    YPlatformMemoryMapEntry conventional_memory = {
        .context = platform,
        .entry_type = Y_PLATFORM_MEMORY_MAP_ENTRY_CONVENTIONAL,
        .start = 0x0000,
        .end = direct_memory_size - 1,
        // Conventional memory is the caller's buffer, accessed directly.
        .read_data = platform->config.direct_memory,
        .write_data = platform->config.direct_memory};
    YPlatformRegisterMemoryMapEntry(platform, &conventional_memory);
  }
  if (platform->config.paged_memory) {
    YPlatformMemoryMapEntry paged_memory = {
        .context = platform,
        .entry_type = Y_PLATFORM_MEMORY_MAP_ENTRY_PAGED_MEMORY,
        .start = direct_memory_size,
        .end = direct_memory_size + platform->config.paged_memory->size - 1,
        .paged_memory = platform->config.paged_memory};
    YPlatformRegisterMemoryMapEntry(platform, &paged_memory);
  }
}

Y_FILE_PRIVATE void YPlatformInitPIC(YPlatformState* platform) {
  platform->pic.config.sp = false;
  platform->pic.config.logger = &platform->logger;
  YPICInit(&platform->pic);
  YPlatformPortMapEntry pic_entry = {
      .entry_type = Y_PLATFORM_PORT_MAP_ENTRY_PIC,
      .start = 0x20,
      .end = 0x21,
      .read_byte = YPlatformPICCallbackReadPortByte,
      .write_byte = YPlatformPICCallbackWritePortByte,
      .context = &platform->pic,
  };
  YPlatformRegisterPortMapEntry(platform, &pic_entry);
}

Y_FILE_PRIVATE void YPlatformInitPIT(YPlatformState* platform) {
  platform->pit.config.context = platform;
  platform->pit.config.logger = &platform->logger;
  platform->pit.config.raise_irq_0 = YPlatformPICCallbackRaiseIRQ0;
  platform->pit.config.set_pc_speaker_frequency =
      YPlatformPITCallbackSetPCSpeakerFrequency;
  YPITInit(&platform->pit);
  YPlatformPortMapEntry pit_entry = {
      .entry_type = Y_PLATFORM_PORT_MAP_ENTRY_PIT,
      .start = 0x40,
      .end = 0x43,
      .read_byte = YPlatformPITCallbackReadPortByte,
      .write_byte = YPlatformPITCallbackWritePortByte,
      .context = platform,
  };
  YPlatformRegisterPortMapEntry(platform, &pit_entry);
}

Y_FILE_PRIVATE void YPlatformInitPPI(YPlatformState* platform) {
  platform->ppi.config.context = platform;
  platform->ppi.config.logger = &platform->logger;
  platform->ppi.config.num_floppy_drives =
      platform->config.floppy_drives > 0 ? platform->config.floppy_drives : 1;
  platform->ppi.config.memory_size = Y_PPI_MEMORY_SIZE_256KB;
  // The DIP switches are what the BIOS branches on to decide which adapter to
  // program, so they have to agree with the adapter the platform registers.
  platform->ppi.config.display_mode =
      platform->config.video_adapter == Y_VIDEO_ADAPTER_CGA
          ? Y_PPI_DISPLAY_CGA_80X25
          : Y_PPI_DISPLAY_MDA;
  platform->ppi.config.fpu_installed = false;
  platform->ppi.config.set_pc_speaker_frequency =
      YPlatformPPICallbackSetPCSpeakerFrequency;
  platform->ppi.config.set_keyboard_control =
      YPlatformPPICallbackSetKeyboardControl;
  YPPIInit(&platform->ppi);
  YPlatformPortMapEntry ppi_entry = {
      .entry_type = Y_PLATFORM_PORT_MAP_ENTRY_PPI,
      .start = 0x60,
      .end = 0x63,
      .read_byte = YPlatformPPICallbackReadPortByte,
      .write_byte = YPlatformPPICallbackWritePortByte,
      .context = platform,
  };
  YPlatformRegisterPortMapEntry(platform, &ppi_entry);
}

Y_FILE_PRIVATE void YPlatformInitKeyboard(YPlatformState* platform) {
  platform->keyboard.config.context = platform;
  platform->keyboard.config.logger = &platform->logger;
  platform->keyboard.config.raise_irq1 = YPlatformKeyboardCallbackRaiseIRQ1;
  platform->keyboard.config.send_scancode =
      YPlatformKeyboardCallbackSendScancode;
  YKeyboardInit(&platform->keyboard);
}

Y_FILE_PRIVATE void YPlatformInitFDC(YPlatformState* platform) {
  platform->fdc.config.context = platform;
  platform->fdc.config.logger = &platform->logger;
  platform->fdc.config.raise_irq6 = YPlatformFDCCallbackRaiseIRQ6;
  platform->fdc.config.request_dma = YPlatformFDCCallbackRequestDMA;
  platform->fdc.config.read_image_byte = NULL;
  platform->fdc.config.write_image_byte = NULL;
  YFDCInit(&platform->fdc);
  YPlatformPortMapEntry fdc_entry = {
      .entry_type = (YPlatformPortMapEntryType)Y_PLATFORM_PORT_MAP_ENTRY_FDC,
      .start = 0x3F0,
      .end = 0x3F7,
      .read_byte = YPlatformFDCCallbackReadPortByte,
      .write_byte = YPlatformFDCCallbackWritePortByte,
      .context = platform,
  };
  YPlatformRegisterPortMapEntry(platform, &fdc_entry);
}

Y_FILE_PRIVATE void YPlatformInitHDC(YPlatformState* platform) {
  platform->hdc.config.context = platform;
  platform->hdc.config.logger = &platform->logger;
  YHDCInit(&platform->hdc);

  const uint32_t option_rom_size = YHDCGetOptionROMSize();
  YPlatformMemoryMapEntry option_rom_entry = {
      .context = &platform->hdc,
      .entry_type = Y_PLATFORM_MEMORY_MAP_ENTRY_HDC_OPTION_ROM,
      .start = Y_HDC_OPTION_ROM_START_ADDRESS,
      .end = Y_HDC_OPTION_ROM_START_ADDRESS + option_rom_size - 1,
      // As with the BIOS ROM, a constant array read directly. write_data is
      // left NULL because the option ROM is read-only.
      .read_data = YHDCGetOptionROMData(),
  };
  YPlatformRegisterMemoryMapEntry(platform, &option_rom_entry);

  YPlatformPortMapEntry port_entry = {
      .context = &platform->hdc,
      .entry_type = (YPlatformPortMapEntryType)Y_PLATFORM_PORT_MAP_ENTRY_HDC,
      .start = Y_HDC_PORT_BASE,
      .end = Y_HDC_PORT_BASE + Y_HDC_NUM_PORTS - 1,
      .read_byte = YPlatformHDCCallbackReadPortByte,
      .write_byte = YPlatformHDCCallbackWritePortByte,
  };
  YPlatformRegisterPortMapEntry(platform, &port_entry);
}

Y_FILE_PRIVATE void YPlatformInitDMA(YPlatformState* platform) {
  platform->dma.config.context = platform;
  platform->dma.config.logger = &platform->logger;
  platform->dma.config.read_memory_byte = YPlatformDMACallbackReadMemoryByte;
  platform->dma.config.write_memory_byte = YPlatformDMACallbackWriteMemoryByte;
  platform->dma.config.read_device_byte = YPlatformDMACallbackReadDeviceByte;
  platform->dma.config.write_device_byte = YPlatformDMACallbackWriteDeviceByte;
  platform->dma.config.on_terminal_count = YPlatformDMACallbackOnTerminalCount;
  YDMAInit(&platform->dma);
  YPlatformPortMapEntry dma_entry = {
      .entry_type = (YPlatformPortMapEntryType)Y_PLATFORM_PORT_MAP_ENTRY_DMA,
      .start = 0x00,
      .end = 0x0F,
      .read_byte = YPlatformDMACallbackReadPortByte,
      .write_byte = YPlatformDMACallbackWritePortByte,
      .context = &platform->dma,
  };
  YPlatformRegisterPortMapEntry(platform, &dma_entry);
  YPlatformPortMapEntry dma_page_entry = {
      .entry_type =
          (YPlatformPortMapEntryType)Y_PLATFORM_PORT_MAP_ENTRY_DMA_PAGE,
      .start = 0x80,
      .end = 0x8F,
      .read_byte = YPlatformDMACallbackReadPortByte,
      .write_byte = YPlatformDMACallbackWritePortByte,
      .context = &platform->dma,
  };
  YPlatformRegisterPortMapEntry(platform, &dma_page_entry);
}

Y_FILE_PRIVATE void YPlatformInitVideo(YPlatformState* platform) {
  platform->video.config = YVideoDefaultConfig;
  platform->video.config.context = platform;
  platform->video.config.logger = &platform->logger;
  platform->video.config.adapter = platform->config.video_adapter;
  platform->video.config.vram = platform->config.vram;
  platform->video.config.text_font = platform->config.video_text_font;
  YVideoInit(&platform->video);

  const YVideoAdapterMetadata* adapter =
      YVideoGetAdapterMetadata(&platform->video);

  YPlatformMemoryMapEntry vram_entry = {
      .context = &platform->video,
      .entry_type = Y_PLATFORM_MEMORY_MAP_ENTRY_VRAM,
      .start = adapter->vram_address,
      .end = adapter->vram_address + adapter->vram_size - 1,
      // Reads go straight to the buffer: nothing observes them, and the guest
      // reads back what it wrote. Writes keep a callback so the adapter can
      // see them - see YVideoWriteVRAMByte.
      .read_data = platform->config.vram,
      .write_byte_fn = YPlatformVideoCallbackWriteVRAMByte,
  };
  YPlatformRegisterMemoryMapEntry(platform, &vram_entry);

  YPlatformPortMapEntry port_entry = {
      .context = platform,
      .entry_type = Y_PLATFORM_PORT_MAP_ENTRY_VIDEO,
      .start = adapter->port_start,
      .end = adapter->port_end,
      .read_byte = YPlatformVideoCallbackReadPortByte,
      .write_byte = YPlatformVideoCallbackWritePortByte,
  };
  YPlatformRegisterPortMapEntry(platform, &port_entry);
}

// What the platform's logger stamps each log with: the tick count, and where
// the CPU is. YCPUTick() moves IP past an instruction before running it, so
// while an instruction runs - making the port or memory access a log is
// about - CS:IP is the instruction after it, which is also what an 8086
// pushes when an interrupt arrives.
Y_FILE_PRIVATE uint64_t YPlatformGetLogTick(void* context) {
  return ((YPlatformState*)context)->ticks;
}

Y_FILE_PRIVATE bool YPlatformGetLogLocation(
    void* context, uint16_t* cs, uint16_t* ip) {
  const YPlatformState* platform = (const YPlatformState*)context;
  *cs = platform->cpu.registers[Y_CPU_CS];
  *ip = platform->cpu.registers[Y_CPU_IP];
  return true;
}

// Initialize the platform state with the provided configuration. Returns true
// if the platform state was successfully initialized, or false if:
//   - Conventional memory, direct and paged, is not between 64K and 640K.
Y_PUBLIC bool YPlatformInit(YPlatformState* platform) {
  // Initialized first, ahead of validation, so that a rejected config can
  // still be logged.
  YLoggerInit(&platform->logger, platform->config.logger_config);

  const YPlatformPagedMemory* paged_memory = platform->config.paged_memory;
  const uint32_t memory_size = platform->config.direct_memory_size +
                               (paged_memory ? paged_memory->size : 0);
  if (memory_size < Y_PLATFORM_MIN_CONVENTIONAL_MEMORY_SIZE ||
      memory_size > Y_PLATFORM_MAX_CONVENTIONAL_MEMORY_SIZE) {
    Y_PLATFORM_ERROR(
        "invalid_conventional_memory_size", Y_LOG_UINT("size", memory_size),
        Y_LOG_UINT("min", Y_PLATFORM_MIN_CONVENTIONAL_MEMORY_SIZE),
        Y_LOG_UINT("max", Y_PLATFORM_MAX_CONVENTIONAL_MEMORY_SIZE));
    return false;
  }
  // A machine with no memory would run until its first instruction fetch came
  // back as open bus, so this is rejected here rather than left to fail
  // obscurely later.
  if (platform->config.direct_memory_size > 0 &&
      platform->config.direct_memory == NULL) {
    Y_PLATFORM_ERROR("missing_direct_memory");
    return false;
  }
  // Paged memory starts where direct memory ends. That has to be on a 4KB
  // boundary, the granularity the memory map indexes regions at, and the
  // region has to be a usable size for its pages.
  if (paged_memory) {
    const YPlatformMemoryMapEntry entry = {
        .start = platform->config.direct_memory_size,
        .end = memory_size - 1,
        .paged_memory = platform->config.paged_memory};
    if ((platform->config.direct_memory_size &
         (Y_PLATFORM_MEMORY_PAGE_SIZE - 1)) != 0 ||
        !YPlatformIsPagedMemoryValid(&entry)) {
      Y_PLATFORM_ERROR(
          "invalid_paged_memory",
          Y_LOG_UINT("direct_memory_size", platform->config.direct_memory_size),
          Y_LOG_UINT("size", paged_memory->size),
          Y_LOG_UINT("page_shift", paged_memory->page_shift));
      return false;
    }
  }
  // The video adapter reads and writes this directly, so an absent buffer
  // would leave the screen permanently blank with nothing to say why.
  if (platform->config.vram == NULL) {
    Y_PLATFORM_ERROR("missing_vram");
    return false;
  }

  YPlatformInitCPU(platform);
  // Only once the CPU is up: a log from the config checks above would
  // otherwise give the location of an instruction that is not running.
  const YLoggerSource log_source = {
      .context = platform,
      .get_tick = YPlatformGetLogTick,
      .get_location = YPlatformGetLogLocation,
  };
  YLoggerSetSource(&platform->logger, &log_source);
  YPlatformInitMemoryMap(platform);
  YPlatformInitBIOS(platform);
  YPlatformInitPIC(platform);
  YPlatformInitPIT(platform);
  YPlatformInitPPI(platform);
  YPlatformInitKeyboard(platform);
  YPlatformInitFDC(platform);
  YPlatformInitHDC(platform);
  YPlatformInitDMA(platform);
  YPlatformInitVideo(platform);

  platform->ticks = 0;
  platform->pit_cycles = 0;
  platform->fdc_cycles = 0;
  platform->keyboard_cycles = 0;
  platform->last_sync_ticks = 0;
  // Schedule the first deadline from the devices' power-on state, so that the
  // first instruction is not treated as already overdue.
  platform->next_event_ticks =
      YPlatformCyclesUntilNextEvent(platform, Y_PLATFORM_MAX_EVENT_INTERVAL);

  // Set before the two calls below, because each of them recomputes the
  // enabled flags and one of those is derived from this.
  // Last, because every region the machine has is registered by now.
  YPlatformUpdateAfterMemoryMapChange(platform);

  return true;
}

Y_PUBLIC Y_HOT bool YPlatformRaiseIRQ(YPlatformState* platform, uint8_t irq) {
  if (irq >= 8) {
    return false;
  }
  YPICRaiseIRQ(&platform->pic, irq);
  return true;
}

// ============================================================================
// Device scheduling
// ============================================================================
//
// Devices are not clocked on every instruction. Each is brought up to date
// only when it next has something to do, and the instruction path compares
// against the earliest of those deadlines.
//
// The consequence is that device state is generally stale. Anything that reads
// it - a port handler, or the host asking what the CRT beam is doing - has to
// bring the device up to date first, which is what YPlatformSync() is
// for. Getting this wrong shows up as a guest timing loop reading a counter
// that never moves, so port handlers that touch a device call it.

// Half the range of the tick counter. A deadline this far behind the current
// tick or less has come due; anything further away is in the future with the
// counter having wrapped in between. Too large for an enum, which is an int.
//
// Shifted rather than written out, and shifted from a uint32_t rather than an
// int, because shifting a 1 into an int's sign bit is undefined.
Y_FILE_PRIVATE const uint32_t YPlatformTickCounterHalfRange = (uint32_t)1 << 31;

// Whether the earliest device deadline has come due. Compared as a difference
// so that a deadline stays in the future across the point where the 32-bit
// cycle counter wraps.
Y_FILE_PRIVATE inline bool YPlatformIsEventDue(const YPlatformState* platform) {
  // An unsigned comparison against half the range rather than a cast of the
  // difference to int32_t, because converting an out-of-range unsigned value is
  // only defined from C23 on and this is C99.
  return (uint32_t)(platform->ticks - platform->next_event_ticks) <
         YPlatformTickCounterHalfRange;
}

// Work out when the next device needs attention, in cycles from now, up to
// max_cycles. The instruction path passes Y_PLATFORM_MAX_EVENT_INTERVAL; the
// idle skip passes what is left of the budget it was given, because there the
// answer is how far the clock may be moved rather than how long until it is
// checked again.
Y_FILE_PRIVATE uint32_t YPlatformCyclesUntilNextEvent(
    const YPlatformState* platform, uint32_t max_cycles) {
  uint32_t cycles = max_cycles;

  // The PIT's next output change, converted from its own 1.19MHz clock and
  // offset by the cycles already credited towards its next tick.
  const uint32_t pit_ticks = YPITTicksUntilNextEvent(&platform->pit);
  if (pit_ticks != Y_PIT_NO_EVENT) {
    const uint32_t pit_cycles =
        pit_ticks * Y_PLATFORM_CYCLES_PER_PIT_TICK - platform->pit_cycles;
    if (pit_cycles < cycles) {
      cycles = pit_cycles;
    }
  }

  // The floppy controller only advances a command that is executing; when no
  // command is in flight there is nothing for a tick to do.
  if (platform->fdc.phase == Y_FDC_PHASE_EXECUTION) {
    const uint32_t fdc_cycles =
        Y_PLATFORM_CYCLES_PER_FDC_TICK - platform->fdc_cycles;
    if (fdc_cycles < cycles) {
      cycles = fdc_cycles;
    }
  }

  // Never return 0, which would leave the deadline permanently due.
  return cycles > 0 ? cycles : 1;
}

// Bring every device up to date with the cycles that have run since the last
// sync, and schedule the next deadline.
Y_PUBLIC Y_HOT void YPlatformSync(YPlatformState* platform) {
  const uint32_t elapsed = platform->ticks - platform->last_sync_ticks;
  platform->last_sync_ticks = platform->ticks;

  if (elapsed > 0) {
    platform->pit_cycles += elapsed;
    const uint32_t pit_ticks =
        platform->pit_cycles / Y_PLATFORM_CYCLES_PER_PIT_TICK;
    if (pit_ticks > 0) {
      platform->pit_cycles -= pit_ticks * Y_PLATFORM_CYCLES_PER_PIT_TICK;
      YPITAdvance(&platform->pit, pit_ticks);
    }

    platform->fdc_cycles += elapsed;
    const uint32_t fdc_ticks =
        platform->fdc_cycles / Y_PLATFORM_CYCLES_PER_FDC_TICK;
    if (fdc_ticks > 0) {
      platform->fdc_cycles -= fdc_ticks * Y_PLATFORM_CYCLES_PER_FDC_TICK;
      // YFDCTick() does nothing unless a command is executing, so an idle
      // controller does not need catching up at all - which matters because
      // an idle stretch is not bounded by an FDC deadline.
      if (platform->fdc.phase == Y_FDC_PHASE_EXECUTION) {
        for (uint32_t i = 0; i < fdc_ticks; ++i) {
          YFDCTick(&platform->fdc);
        }
      }
    }

    platform->keyboard_cycles += elapsed;
    const uint32_t keyboard_ticks =
        platform->keyboard_cycles / Y_PLATFORM_CYCLES_PER_MILLISECOND;
    if (keyboard_ticks > 0) {
      platform->keyboard_cycles -=
          keyboard_ticks * Y_PLATFORM_CYCLES_PER_MILLISECOND;
      for (uint32_t i = 0; i < keyboard_ticks; ++i) {
        YKeyboardTickMs(&platform->keyboard);
      }
    }

    // Advancing the beam costs the same whether it moved by one cycle or a
    // whole frame, so the video adapter needs no deadline of its own.
    YVideoTick(&platform->video, elapsed);
  }

  platform->next_event_ticks =
      platform->ticks +
      YPlatformCyclesUntilNextEvent(platform, Y_PLATFORM_MAX_EVENT_INTERVAL);
}

// The body of both entry points. YPlatformTick() promises its caller one
// instruction; YPlatformRun() is driving the machine rather than stepping it.
// Both pass a constant, so the tests below fold away in each.
Y_FILE_PRIVATE Y_HOT YPlatformRunStatus
YPlatformTickInternal(YPlatformState* platform, bool may_batch_instructions) {
  // How long the CPU may run before something in the machine needs to see it.
  // A deadline already due leaves no budget: the subtraction would otherwise
  // wrap and let the CPU run on past a device that is already waiting.
  const uint16_t max_run_cycles =
      may_batch_instructions && !YPlatformIsEventDue(platform)
          ? (uint16_t)(platform->next_event_ticks - platform->ticks)
          : 0;

  // Tick the CPU.
  YCPUTickResult cpu_result = YCPUTick(&platform->cpu, max_run_cycles);

  // The instruction took as long as it took, and every device is clocked from
  // that - but in arrears. Rather than offering each device its share of the
  // cycles on every instruction, this only checks whether the earliest device
  // deadline has come due, which is one comparison in the common case.
  const uint16_t cycles = platform->cpu.cycles_this_tick;
  platform->ticks += cycles;
  if (YPlatformIsEventDue(platform)) {
    YPlatformSync(platform);
  }

  if (cpu_result == Y_CPU_TICK_INVALID) {
    return Y_PLATFORM_INVALID;
  }

  // A halted CPU with interrupts disabled and nothing pending can never be
  // woken. Note that an ordinary halt is not reported as a stop: the PIT and
  // the rest of the machine must keep ticking so that an interrupt can wake
  // the CPU back up.
  if (platform->cpu.is_halted && !YCPUGetFlag(&platform->cpu, Y_CPU_IF) &&
      !platform->cpu.has_pending_internal_interrupt) {
    return Y_PLATFORM_HUNG;
  }

  return Y_PLATFORM_RUNNING;
}

Y_PUBLIC YPlatformRunStatus YPlatformTick(YPlatformState* platform) {
  return YPlatformTickInternal(platform, false);
}

// Advance the clock over time the guest has said it has no use for, up to
// max_cycles. Every device is brought up to date across the skipped interval
// rather than having it taken away from them, so the guest's timer tick count
// is the same either way - what is skipped is executing the loop it would have
// spent the interval in.
//
// The bound matters as much as the skip: without it a machine idling at a DOS
// prompt would be handed more emulated time per call than the caller asked for,
// and would run its clock fast. Y_PLATFORM_MAX_IDLE_SKIP_CYCLES bounds it a
// second time, in case the caller's budget is itself large enough to overflow
// the catch-up arithmetic.
Y_FILE_PRIVATE void YPlatformSkipIdleTime(
    YPlatformState* platform, uint32_t max_cycles) {
  if (max_cycles > Y_PLATFORM_MAX_IDLE_SKIP_CYCLES) {
    max_cycles = Y_PLATFORM_MAX_IDLE_SKIP_CYCLES;
  }
  const uint32_t cycles = YPlatformCyclesUntilNextEvent(platform, max_cycles);
  if (cycles == 0) {
    return;
  }
  platform->ticks += cycles;
  YPlatformSync(platform);
}

Y_PUBLIC Y_HOT YPlatformRunStatus
YPlatformRun(YPlatformState* platform, uint32_t max_cycles) {
  // Instructions are only ever run whole, so the last one of a run generally
  // takes the total a little past the budget. Unsigned subtraction keeps this
  // right across the counter wrapping.
  const uint32_t start = platform->ticks;
  uint32_t elapsed = 0;
  while (elapsed < max_cycles) {
    YPlatformRunStatus status = YPlatformTickInternal(platform, true);
    if (status != Y_PLATFORM_RUNNING) {
      return status;
    }
    elapsed = platform->ticks - start;
    // Skipping is done here rather than where the guest declared itself idle
    // because only this loop knows how much of the budget is left, which is
    // what bounds how far the clock may move.
    if (platform->is_guest_idle) {
      platform->is_guest_idle = false;
      if (elapsed < max_cycles) {
        YPlatformSkipIdleTime(platform, max_cycles - elapsed);
        elapsed = platform->ticks - start;
      }
    }
  }
  return Y_PLATFORM_RUNNING;
}

Y_PUBLIC void YPlatformSetDOSIdleSkip(YPlatformState* platform, bool enable) {
  platform->is_dos_idle_skip_enabled = enable;
  platform->cpu.config.handle_interrupt =
      enable ? YPlatformCPUCallbackHandleInterrupt : NULL;
  // An idle declared before the skip went off must not be acted on after it.
  platform->is_guest_idle = false;
}

// ============================================================================
// The CPU's view of guest memory
// ============================================================================

Y_FILE_PRIVATE void YPlatformUpdateDirectDataWindow(YPlatformState* platform) {
  YCPUInvalidateDirectDataWindow(&platform->cpu);
  // The window is a prefix of the address space, so it is whichever region
  // covers address 0 and only while that region is plain storage reached
  // through one buffer in both directions. Conventional memory is that region;
  // a read-only region, or one with a callback in either direction, is not,
  // and is left to the memory map.
  const YPlatformMemoryMapEntry* entry =
      YPlatformGetMemoryMapEntryForAddress(platform, 0);
  if (entry == NULL || entry->start != 0 || entry->read_data == NULL ||
      entry->write_data != entry->read_data) {
    return;
  }
  // A map entry's end is the last address in the region; a window's is one
  // past it.
  YCPUSetDirectDataWindow(&platform->cpu, entry->write_data, entry->end + 1);
}

// Tell the CPU where paged memory is, or that there is none.
Y_FILE_PRIVATE void YPlatformUpdatePagedDataWindow(YPlatformState* platform) {
  const YPlatformMemoryMapEntry* entry = YPlatformGetMemoryMapEntryByType(
      platform, Y_PLATFORM_MEMORY_MAP_ENTRY_PAGED_MEMORY);
  if (entry == NULL) {
    // size 0 puts every address outside the window.
    const YCPUPagedDataWindow none = {0};
    platform->cpu.paged_data_window = none;
    return;
  }
  const YPlatformPagedMemory* memory = entry->paged_memory;
  const YCPUPagedDataWindow window = {
      .pages = memory->pages,
      .frames = memory->frames,
      .start = entry->start,
      .size = memory->size,
      .page_shift = memory->page_shift};
  platform->cpu.paged_data_window = window;
}


// ==============================================================================
// src/platform/platform.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_PLATFORM_BUNDLE_H

