# core - the emulation modules

What applies to the core as a whole, and to its modules other than the CPU,
which has `src/cpu/AGENTS.md`. The project-wide rules - code style, naming,
visibility, and how performance work is measured - are in the root
`AGENTS.md`.

## How the core fits together

- `core` holds the emulation: the CPU and the hardware around it - video,
  keyboard, floppy and hard disk controllers, timer, interrupt and DMA
  controllers. **No module depends on an external runtime**; each reaches the
  host only through callbacks, which is what lets the same code build for
  SDL, Emscripten and an RP2040.
- **Every module comes up the same way**: zero-initialize its `XState`, fill
  in `state.config`, call `XInit(&state)`. The config is a field of the
  state, so nothing has to outlive the module, and `XInit()` is where a
  host's mistake is checked and reported.
- **No `XInit()` zeroes anything.** Each sets only the fields with a non-zero
  power-on value, so a caller that zeroed the state to fill in its config
  does not pay twice. Every module's state is a field of `YPlatformState`, so
  **a host must zero its `YPlatformState`** - `static YPlatformState
  platform;` or `= {0}` - or every module comes up with garbage.
- **A reset is not an init.** `YDMAWritePort()`'s master reset clears the
  controller's state, but saves and restores the config across it: the reset
  is the chip's, and the config is what the host wired up. The DMA tests
  catch a reset that calls `YDMAInit()`.
- The logger stays a pointer: `YLogger` is a util type, not an emulated
  module, and several may share one `YLoggerConfig`.
- **Each `core/src/<module>` is bundled by its `bundle.json` into one header
  in `core`** - `core/src/cpu` becomes `core/cpu.h` - and all of them into
  `core/yax86_core.h`, which `libyax86_core.a` is built from. A module's
  external interface is its `public.h`.
- **`util` has a bundle of its own, `core/util.h`**, which every
  module's bundle includes rather than copies: `"requires": ["util"]` in its
  `bundle.json` puts a guarded `#include "util.h"` ahead of the module's
  `extern "C"` block. `util.h` comes first in `yax86_core.h`, so each util
  file is there once, and the later includes are skipped without the file
  being opened. Copied into every bundle instead, the util headers were
  25,364 of the bundles' 63,532 lines, and a change to `util/log.h` repeated
  twenty-two times. The bundler refuses a `bundle.json` that names a file
  twice.
- **The bundles and generated ROM arrays are committed, and `.gitattributes`
  marks them generated**, so GitHub collapses them. Review the sources under
  `core/src`.
- `platform` is the motherboard: it connects the modules and owns the memory
  and port maps.
- **ROMs are compiled in**, since targets like the Pico have no file system.
  `core/tools/generate-rom-data-files.js` turns an image into a C array; the
  `generate_rom_data` calls in `core/CMakeLists.txt` say what. The generated
  `*_rom_data.{c,h}` are committed and CI runs `git diff --exit-code`, so
  regenerate and commit them when a ROM changes.
- **A ROM array is module-private and reaches the library only through the
  bundle.** Its `.c` is a source of `yax86_core` only so a ROM change reruns
  the generator, and carries `HEADER_FILE_ONLY` so it is not compiled again
  as dead weight; the header's `extern` is guarded by the bundle's include
  guard. `--module <name>` makes the array `Y_MODULE_PRIVATE` and names it in
  the namespace - symbol `BIOSROMData` in `bios` gives `YBIOSROMData` and
  `Y_BIOS_ROM_DATA_SIZE` - and refuses a symbol not led by the owner.
  `pico/bench` runs the generator without `--module` for its floppy, getting
  a standalone translation unit under its own names, `kDOSFloppyData` and
  `kDOSFloppyDataSize`.

## platform - the memory map

- `YPlatformGetMemoryMapEntryForAddress()` is the hottest lookup in the
  emulator - every instruction byte and nearly every operand - and answers
  from an index rather than walking the map, which on a Cortex-M0+ misses the
  first entry every time code runs from the BIOS ROM. Worth **7.0%**.
- `memory_page_map` holds an entry index per 4KB page of the 1MB space, 256
  bytes, so a lookup is a shift, a load and a compare. 4KB is the coarsest
  page that leaves every region aligned, not a tuned figure.
- `Y_PLATFORM_MEMORY_PAGE_UNMAPPED` answers `NULL` directly.
  `Y_PLATFORM_MEMORY_PAGE_STRADDLED` is a page shared by more than one entry,
  which falls back to a walk. Nothing registers a misaligned region today,
  which is why `platform_memory_map_test.cpp` registers one to test it. A
  region that is not a whole number of pages - an option ROM - gets a
  straddled last page by construction.
- **Registering a region and telling the CPU are separate steps**:
  `YPlatformRegisterMemoryMapEntry()` records the entry and
  `YPlatformUpdateAfterMemoryMapChange()` discards what the CPU derived from
  the map; a caller may do the second once after several of the first, as
  `YPlatformInit()` does last of all. **The decode cache makes the second
  obligatory**: an unmapped address reads as open bus, so an instruction
  there can be cached as 0xFF bytes, and mapping a region over it writes
  nothing that would move a page generation. The two memory windows are
  safe either way - entries cannot overlap, and a missed update leaves the
  data window closed rather than wrong.
- Registration extends the index rather than rebuilding it: entries cannot
  overlap, so only the new entry's pages change.
  `YPlatformUpdateMemoryPageMapForEntry()` takes the entry's index, since that
  is what goes in the map.
- **An entry past the top of the address space is rejected**, which is what
  makes the index the whole answer rather than a cache in front of the map:
  `YPlatformGetMemoryPageMapIndex()` answers an address above the space as
  unmapped directly. Guest addresses never reach it - `YCPUToRawAddress()`
  masks them to 20 bits - so this bounds what a host may register.

## platform - paged memory

- **What it is for**: a host with less fast memory than the guest has RAM sets
  `YPlatformConfig.paged_memory`, and conventional memory above
  `direct_memory_size` is then held in pages, only some of them in fast
  memory ("frames") at a time - a microcontroller, say, holding the pages in
  slower external memory. `direct_memory_size` may be 0. The PicoCalc port
  is the user today; its `AGENTS.md` has how it pages and what it measured.
- **The host owns the page table and the frames**: one `uint16_t` per page,
  bit 15 `DIRTY`, bit 14 `RESIDENT`, bits 0-13 the frame number - laid out
  at `Y_CPU_PAGED_DATA_RESIDENT`. An access the entry allows goes to the
  frame; anything else calls the host's `page_fault_fn`, which must set the
  entry so that it allows it.
- **Two bits so that the host learns what it needs and pays nothing per
  access**: bringing a page in without `DIRTY` makes its first write fault,
  which is how the host knows which pages to write back - a page never written
  is evicted for free. With `pico/picocalc` on the PicoCalc that is most code:
  seven DOS commands brought in 4,971 pages and wrote back 3,394, a Word
  session 17,502 and 7,633. A host that does not track dirty pages sets
  `DIRTY` with `RESIDENT` on every page and never takes a write fault.
  Clearing `RESIDENT` on a page it keeps in its frame makes the next access
  fault, which is how it samples which pages are in use. The frame number is
  read only with `RESIDENT` set.
- **The bit is `DIRTY`, not `WRITABLE`, and a write needs both bits.** Every
  page is writable - a write with `DIRTY` clear goes through the host - so
  `WRITABLE` read as "read-only" when clear. With the CPU writing on
  `RESIDENT` and `DIRTY` together, clearing `RESIDENT` to sample use leaves
  `DIRTY` alone, and the entry is the one record of whether a page needs
  writing back; when the CPU wrote on the one bit, sampling had to clear both
  and a host had to keep its own dirty flag.
- **The CPU reads the page table itself**: `YCPUPagedDataWindow`, which
  `YPlatformUpdateAfterMemoryMapChange()` assigns from the paged entry, so an
  access to a page in its frame is a subtract, a compare and two loads, and
  only a fault reaches the platform. Going through the platform's memory map
  for every access instead, the DOS330 boot at 640KB took 7.00 seconds with
  `pico/picocalc` on the PicoCalc's RP2040 at 250MHz, all of it paged; with
  the CPU indexing, 6.32.
- **Instruction fetch opens a page's frame the same way**, without asking the
  host, in `YCPUOpenPagedFetchWindow()`. The fetch window is one page, since
  the next page's frame can be anywhere, so running or jumping onto another
  page reopens it.
- **A page leaving its frame discards nothing the CPU derived from it**: its
  bytes have not changed, so the decode cache keeps its decodes. The one
  thing that points into a frame rather than at a page's contents is the
  fetch window, so `YPlatformFaultPagedMemory()` drops it after every fault,
  in case the host gave that frame to another page. The one-frame tests in
  `platform_paged_memory_test.cpp`, with code and data taking the frame from
  each other, fail without it.
- **A write that faults is reported to the decode cache once, not twice.** The
  CPU reports every write it makes, and `YPlatformWriteMemoryByte()` reports
  every write it is handed, so a write that goes through the callback is
  counted twice. That changes when page counters wrap, so where runs of
  instructions end, so when the guest takes its timer interrupts. For paged
  memory it would mean the number of frames changes what the guest does, so
  `YPlatformCPUCallbackWriteMemoryByte()` sends the CPU's paged writes past
  the second report. Every other region is still counted twice, and the
  instruction and cycle counts that `pico/bench`'s `dos-boot` workload is held
  to depend on that. With this, MS-DOS 3.30 running five programs at 640KB, in
  an off-tree harness, retires the same instructions with 128 frames as with
  4, and with 256 as with 2.
- **Both data windows stay, and a host may set either, both or neither.** The
  direct window could be modelled as a paged one - frames over
  `direct_memory`, entry *i* `RESIDENT | DIRTY | i` - but every host that does
  not page would pay for it: `dos-boot` (`pico/bench`, RP2040 at 250MHz,
  `-O3`) with all of 128KB paged and every page in its frame is 15% slower
  than direct. Nor is there a switch between them: an empty window already is
  one (`end` 0, `size` 0), the CPU's code is the same whichever are set, and a
  flag would cost the same test. The combinations in use are direct only, for
  SDL and the Pico hosts without PSRAM; paged only, for the Pico hosts with
  it; and neither, for the CPU tests' helper and mocks. Both is for a host
  that holds part of memory directly and pages the rest, which the PicoCalc
  measured and dropped (`pico/picocalc/AGENTS.md`).
- **A paged-only build pays for the direct window it does not have**, by
  reading of the disassembly, not yet measured. Every inlined data access
  first loads `direct_data_window.end`, compares and falls through, about 4
  cycles on a Cortex-M0+, and then makes a call: the paged lookup is in the
  out-of-line `YCPUReadMemoryByteOutsideDirectWindow()` and its write twin, an
  arrangement measured only with paging off, where it won. So every data
  access in a paged-only build costs a failed compare, a call and a return
  before the lookup itself. And `paged_data_window` is last in `YCPUState`,
  past the offsets a Cortex-M0+ load encodes, so the paged path spends 4-5
  instructions forming addresses (`adds #117; adds #255`,
  `movs #186; lsls #1`). It is last so that no field the direct path reads moves. Against
  roughly 500 host cycles an emulated instruction on that board, each is
  probably around 1%. Changes to try, measured with `pico/picocalc` on the
  PicoCalc's RP2040 at 250MHz, `-O3`, running its seven DOS commands and the
  DOS330 boot; and with `dos-boot` (`pico/bench`, RP2040 at 250MHz, `-O3`) to
  show direct-only builds lose nothing:
  - a core build option compiling the direct check out, the PicoCalc's
    no-PSRAM fallback then paging its 128KB to itself with every page in its
    frame;
  - in that build, the paged lookup inlined at each access in place of the
    direct check, so a page in its frame costs no call, weighed against the
    larger code at every access site, all of it in SRAM;
  - `paged_data_window` near the front of `YCPUState`, checking with
    `objdump` that the direct path's fields stay within reach.
- **REP string runs plan only over the direct data window**, so in paged
  memory they go element by element, and a boot with more of memory paged
  retires a few hundred fewer instructions - the difference the bulk runs'
  decode cache reporting makes (see `src/cpu/AGENTS.md`).
- **What paging costs when nothing faults**: `dos-boot` (`pico/bench`, RP2040
  at 250MHz, `-O3`) at 128KB, with every page in a frame, is 2% slower with
  the first 64KB direct and the rest paged, and 15% slower with all of it
  paged. A direct access is a compare and a load, a paged one a call and two
  more loads. `pico/picocalc` builds both; see its `AGENTS.md`.

## platform - the CPU clock ratio

- **`cycles_per_pit_tick` is the number of CPU cycles per 1.193MHz PIT tick**,
  between `Y_PLATFORM_CYCLES_PER_PIT_TICK` (`4`, for the PC/XT's 4.77MHz) and
  `Y_PLATFORM_MAX_CYCLES_PER_PIT_TICK` (`256`, 64x 4.77MHz, ~305.5MHz).
  `YPlatformInit()` sets it to `4`, and `YPlatformSetCyclesPerPITTick()` changes
  it between runs, clamping out-of-range values to `[4, 256]`;
  `YPlatformGetCPUCyclesPerSecond()` returns the corresponding CPU clock in
  cycles per second (`Y_PLATFORM_CPU_CYCLES_PER_SECOND`, `4,772,727`, at `4`).
- **The FDC, keyboard and video ticks, the event interval and the idle-skip cap
  scale with it**, as integer multiples of a PIT tick (`5` PIT ticks per FDC
  tick, `1,193` per millisecond, `119,318` per tenth-second idle-skip ceiling),
  so every device follows the same guest time while the CPU runs faster relative
  to the timer, and at `4` every cycle count is identical to the 4.77MHz
  constants. The default 1ms event interval is capped at
  `Y_PLATFORM_MAX_EVENT_INTERVAL_CYCLES` (`0xFFFF`) so `next_event_ticks - ticks`
  fits in `YCPUTick()`'s `uint16_t max_run_cycles` above ratio `54`.
- **The video beam advances by `elapsed` at `4` and `pit_ticks *
  Y_PLATFORM_CYCLES_PER_PIT_TICK` above `4`.** The adapter's scan-line geometry
  is in 4.77MHz cycles (`4` per PIT tick), and passing `pit_ticks * 4` at `4`
  quantizes sub-tick elapsed cycles when GLaBIOS polls `0x3DA` and moves
  `dos-boot` by 83 instructions (`2,397,299` vs `2,397,216`). Passing `elapsed *
  4` without a branch and scaling `cycles_per_scan_line` and
  `display_cycles_per_scan_line` on `YVideoState` by `cycles_per_pit_tick`
  measured 3.7169s on the Pico 2 (`core .text` +224 bytes, `YPlatformState` +8
  bytes) against 3.7107s for the branch in `YPlatformSync()`.
- **`YPlatformSetCyclesPerPITTick()` syncs under the old ratio first, clamps
  pending remainders, and reschedules.** Without clamping `pit_cycles` to
  `cycles_per_pit_tick - 1` and `fdc_cycles` to `cycles_per_fdc_tick - 1`,
  lowering the ratio below a stored remainder makes
  `pit_ticks * cycles_per_pit_tick - platform->pit_cycles` in
  `YPlatformCyclesUntilNextEvent()` underflow and wrap past `2^31`, missing the
  device deadline. Rescheduling goes through a second `YPlatformSync()` (where
  `elapsed == 0`) rather than a third direct call to
  `YPlatformCyclesUntilNextEvent()`, which `arm-none-eabi-nm` showed stopped GCC
  inlining `YPlatformCyclesUntilNextEvent()` into `YPlatformSync()` in SRAM.
  Conversely, replacing `YPlatformInit()`'s direct `YPlatformCyclesUntilNextEvent()`
  call with `YPlatformSync()` changes GCC's unit-wide auto-inlining heuristic so
  `YCPUReadByteFromPort()` is no longer auto-inlined into
  `YCPUExecuteInImmediate()`, costing 0.49% on the Pico 2 (`3.7288s` vs
  `3.7107s`); with `YPlatformInit()` calling `YPlatformCyclesUntilNextEvent()`
  directly, `arm-none-eabi-nm -S` confirms no function outside `YPlatformSync()`
  changes size (`YPlatformSync()` itself grows by 60 bytes from `0x2a0` to
  `0x2dc` for the video scaling and `0xFFFF` interval cap).
- **`pit_tick_shift` holds `log2(cycles_per_pit_tick)` for power-of-two ratios**
  (`2` at `4`, `3` at `8`, and `0` when not a power of two) so `YPlatformSync()`
  shifts `pit_cycles` instead of dividing at 4.77MHz and power-of-two turbo
  steps. On the Pico 2 (`pico/bench`, 250MHz, `-O3`, 640KB paged), `dos-boot` at
  `4` takes 3.7107s with `pit_tick_shift` on `pit_cycles` (0.34% slower than
  master's 3.6980s), against 3.7173s with a plain runtime divide. On the
  Cortex-M0+, which has no divide instruction, `arm-none-eabi-objdump` of
  `build-pico/O3/yax86_pico_bench.elf` shows `pit_tick_shift` avoiding
  `__aeabi_uidivmod` on `pit_cycles`, while `fdc_cycles` (which crosses its
  20-cycle threshold on almost every sync) still calls `__aeabi_uidivmod` with a
  runtime divisor where `master` divided by a constant `20`; RP2040 hardware
  timing remains unmeasured until an RP2040 board is connected.
  `cycles_per_pit_tick` sits at the end of `YPlatformState` and `pit_tick_shift`
  in the padding byte after `is_guest_idle`, so no existing field offset moves
  and Thumb immediate load offsets for earlier fields are unchanged.

## platform - idle skipping

- MS-DOS waits for a key by polling, never halting: at the 3.30 prompt the
  guest runs about 390,000 instructions a second through `INT 21h`, the CON
  driver and `INT 16h`. It does issue `INT 28h`, the DOS idle interrupt, 783
  times a second. **`YPlatformSetDOSIdleSkip()` makes `YPlatformRun()` take
  that as a signal to advance the clock** to the next device deadline or the
  end of its budget, whichever is first. The guest's handler still runs.
- **Time is advanced, not discarded**: `YPlatformSync()` runs across the
  interval, so every device sees every cycle and the timer tick count is
  unchanged. Bounding the skip by the budget is what keeps the clock from
  running fast. `YPlatformSkipIdleTime()` also sets `platform->did_skip_idle`
  (in the padding byte after `pit_tick_shift`, never cleared by the core), so a
  host measuring execution throughput can clear it before a batch and ignore
  batches where idle skipping ran.
- It is opt in - a program timing a loop inside its own `INT 28h` handler
  would see time jump. The SDL runtime turns it on after init, the PicoCalc
  once DOS reaches its prompt, since DOS issues `INT 28h` at the date and
  time prompts and on from power on it would move the `dos-boot` invariant.
  Tests leave it off.
- **It is a setter, not config**: the skip takes the CPU's
  `handle_interrupt` callback as well as a flag, and the setter is the sole
  writer of both. A config field read at init would be a second way to set
  it that silently stops working once the machine is up.
- **The skip is capped by the nearest device deadline**, which is why
  `YPITTicksUntilNextEvent()` schedules channel 0 alone. After POST the BIOS
  leaves channel 2 at a reload of 1356 with the speaker gated off; waking
  every 678 ticks for it made the skip worth 1.4x at the prompt instead of
  5.6x. Only channel 0's output leaves the PIT, as IRQ 0; channels 1 and 2
  record transitions, recomputed whenever the PIT is advanced. **Give another
  channel's output an effect and the scheduling must change with it** - both
  ends have a comment saying so.

## hdc - the hard disk controller

- An XT-IDE rev 2: an 8-bit ATA task file in PIO mode, no DMA, no interrupt.
- GLaBIOS's INT 13h handles floppies only and never sets up a hard disk, so
  **hard disk support comes from an option ROM**: the XTIDE Universal BIOS,
  `core/src/hdc/XTIDE_Universal_BIOS_XT_r631.rom`, compiled in and mapped at
  0xC8000, its region sized from the ROM. GLaBIOS's POST finds it by its
  `55 AA` signature and checksum on a 2KB boundary from C800:0000 and calls
  offset 3. Its stock configuration is a rev 2 controller at 0x300, which
  POST reports as `Master at 300h`.
- **Rev 2 crosses address lines A0 and A3**: a port offset maps to an ATA
  register with bits 0 and 3 swapped, so status is polled at 0x30E, not
  0x307.
- An empty slot leaves the task file undriven, reading status as 0; the ROM
  decides a drive is absent when the ready bit never appears.
- Commands complete inside the port write that issues them - no `HDCTick`,
  no seek timing. The guest polls, so it cannot tell.
- **The two data-port directions use opposite byte orders.** The drive's
  data port is 16 bits and the card's bus 8, and only the low byte port runs
  a bus cycle: a read of it moves a word, handing the low byte over and
  latching the high one for the high port. A write is reversed - the guest
  loads the latch through the high port first, and writing the low byte
  commits the pair. A shared byte stream gets writes backwards: a partition
  table's 0xAA55 reads as 0x55AA and DOS reports "Invalid drive
  specification".
- Identify Device reports cylinders for the geometry the guest set with
  Initialize Device Parameters, worked out from the capacity, so nothing it
  advertises is unaddressable.
- The device control register is ATA 0xE, physical 0x307: a read is the
  alternate status, the same as status, and a write with the reset bit
  abandons any transfer. The ROM uses neither, but an unanswered alternate
  status would read 0xFF, a permanently busy drive.

## fdc - the floppy controller

- **Four formats: 360KB (`YFDCFormat360KB`, 40 tracks, 9 sectors), 720KB
  (`YFDCFormat720KB`, 80 tracks, 9 sectors), 1.2MB (`YFDCFormat1200KB`, 80
  tracks, 15 sectors) and 1.44MB (`YFDCFormat1440KB`, 80 tracks, 18
  sectors)**, all 2 heads and 512 bytes a sector (368,640, 737,280, 1,228,800
  and 1,474,560 bytes). Hosts pick the format from the image's byte size with
  `YFDCFindFormat()`, which walks `YFDCFormats`, and list accepted sizes from
  `YFDCFormatImageSizes` (`Y_FDC_NUM_FORMATS`, largest `Y_FDC_MAX_IMAGE_SIZE`);
  `YFDCDoesDriveTypeSupportFormat()` checks whether a drive mechanism accepts a
  format.
- **Per-drive mechanism types (`YFDCDriveType`, values 1-4 matching PC/AT CMOS
  codes) and data rates (`YFDCDataRate`)**: a `360KB` drive takes `360KB` disks
  at 250kbps; a `720KB` drive takes `720KB` at 250kbps; a `1.2MB` drive takes
  `360KB` at 300kbps (double-stepped) and `1.2MB` at 500kbps; and a `1.44MB`
  drive takes `720KB` at 250kbps and `1.44MB` at 500kbps. `Y_FDC_DRIVE_TYPE_NONE`
  (`0`, the zero-initialized default) marks an absent drive in the option ROM's
  configuration block; `YPlatformInit()` maps a zero-initialized
  `floppy_drive_types[i]` on a present drive (`i < num_floppy_drives`) to
  `Y_FDC_DRIVE_TYPE_360KB` (`1`) and sets `i >= num_floppy_drives` to
  `Y_FDC_DRIVE_TYPE_NONE`, and `YFDCDoesDriveTypeSupportFormat()` treats `0` as
  a `360KB` drive. Writing port `0x3F7` (CCR) sets the active data rate
  (`0 = 500kbps`, `1 = 300kbps`, `2 = 250kbps`, `3 = 1Mbps`, initialized to
  `250kbps` by `YFDCInit()`); Read Data, Write Data, Read ID and Format Track at
  the wrong rate for the drive and disk fail with
  `Y_FDC_ST0_ABNORMAL_TERMINATION` and `Y_FDC_ST1_MISSING_ADDRESS_MARK`, which
  is how the option ROM's media-rate probe identifies the disk in the drive.
  Reading port `0x3F7` (DIR) returns the selected drive's disk-change line in
  bit 7 (`Y_FDC_DIR_DISK_CHANGED`, with bits 6-0 undriven as `0x7F`), raised by
  `YFDCInsertDisk()` and `YFDCEjectDisk()` and cleared by `Seek` or
  `Recalibrate` while a disk is in the drive.
- **Floppy BIOS support comes from an option ROM, always mapped at `0xCC000`**:
  Sergey Kiselev's Multi-Floppy BIOS Extension 2.7
  (`core/src/fdc/Multi_Floppy_BIOS_2.7_emu.rom`, built from
  `skiselev/floppy_bios` with the patch below by
  `tools/build-fdc-option-rom.sh` and checked against its SHA-256), compiled
  in and mapped at `0xCC000-0xCDFFF` (clear of the XTIDE ROM at `0xC8000-0xCAFFF`
  and aligned to the memory map's 4KB pages). GLaBIOS is an 8KB PC/XT BIOS with
  no `INT 13h AH=08h/15h/16h/17h/18h`, data-rate probing, disk-change handling
  or double stepping, whereas the option ROM hooks `INT 13h` and `INT 1Eh` for
  all of them; mapping it unconditionally gives every machine one floppy BIOS
  path, so a `360KB` drive works alongside a high-density drive and MS-DOS
  3.30's `FORMAT` works directly on every supported drive and media combination
  (`FORMAT B:` on a `720KB` drive with no `CONFIG.SYS`, `FORMAT B: /4` for
  `360KB` in a `1.2MB` drive, `FORMAT B: /T:80 /N:9` for `720KB` in a `1.44MB`
  drive). Its 64-byte configuration block at offset `0x1FC0` is built once in
  `fdc->option_rom_config` by `YFDCInit()` from `fdc->config.drive_types`, with
  the boot F2 prompt cleared and byte `0x1FC0` checksummed so the 64-byte block
  sums to `0` (`mod 256`). The first 4KB page (`0xCC000-0xCCFFF`,
  `Y_PLATFORM_MEMORY_MAP_ENTRY_FDC_OPTION_ROM`) is mapped with `.read_data =
  YFDCGetOptionROMData()` so the CPU's instruction fetch window indexes it
  directly, and only the second 4KB page (`0xCD000-0xCDFFF`,
  `Y_PLATFORM_MEMORY_MAP_ENTRY_FDC_OPTION_ROM_CONFIG`), which holds the config
  block at `0x1FC0`, goes through `.read_byte_fn` (`YFDCReadOptionROMByte()`) -
  1.20% faster on `dos-boot` (6.4191s against 6.4972s at 250MHz, `-O3`) than
  routing the whole 8KB through `.read_byte_fn`.
- **The ROM is built patched for an emulator**: `tools/floppy_bios-emu.patch`
  puts two changes under `%ifdef EMULATOR`, and the build script assembles
  with `-DEMULATOR`. It first checks that upstream, and the patched source
  without `EMULATOR`, both build upstream's `floppy_bios.bin` byte for byte,
  so the patch is the whole difference. Upstream does all its waiting by
  polling the PIT, which an emulator pays for in instructions; GLaBIOS's
  `ARCH_EMU` build halts or skips the same waits.
  - **`fdc_wait_irq` waits with `HLT`**, as GLaBIOS's `FDC_HLT_WAIT` does. The
    IRQ flag is tested with interrupts off and `sti` is followed directly by
    `hlt`, so the IRQ cannot slip in between. It returns with interrupts
    enabled, where upstream keeps IF: the wait only succeeds once the IRQ 6
    handler has run, so every caller that works has them on already. The
    timeout counts BIOS timer ticks, since one PIT reading per wake-up sees
    the same count every 55ms: 18 ticks, the first partial, so 0.93 to 0.99s
    against the 0.99s upstream's loop really waits (36 x 65536 counts at
    2,386,364Hz, mode 3 counting by two), not the 2 seconds its comment says.
    A drive that never interrupts gives DOS's "Not ready" after the same
    emulated time as upstream.
  - **The timeout needs the timer to tick**, where upstream reads the PIT
    itself: a guest that masks IRQ 0, or hooks `INT 8` without chaining, and
    then reaches a controller that never interrupts would wait for good.
    GLaBIOS's own `HLT` wait has the same dependence, and such a guest has
    stopped the BIOS clock too. The emulated controller always interrupts -
    an empty drive answers in 0.20s - so it does not happen here.
  - **The head settle time after a seek is skipped**, as GLaBIOS's
    `ARCH_EMU` build skips `FDC_SEEK_SETTLE`: upstream waits 15ms after every
    seek, reads included (byte 9 of all its parameter tables is `0Fh`). The
    code is patched rather than the tables, which `INT 1Eh` shows the guest.
  - `dos-boot` at 250MHz, `-O3`, 640KB paged: 6.419s and 2,682,650
    instructions unpatched, 6.399s with the head settle skipped, 6.075s with
    `HLT`, and 6.064s and 2,397,216 with both. Before the ROM was mapped it was
    5.785s and 2,353,188. What remains is the ROM's own code and GLaBIOS's
    POST finding it - the halted time is the same as GLaBIOS's own `HLT`
    waits - and a halted tick is not free: it is `Y_CPU_HALTED_CYCLES` (4)
    cycles of the run loop.
  - Copying and formatting gain more than the boot: `FORMAT B: /S` and a
    `COPY` to a 1.44MB disk take 3.98 million instructions instead of 9.91.
- **Read ID (`0x0A`) and Format Track (`0x0D`) map an 80-track 1.2MB drive's
  physical head position (`drive->track`) to a 40-track 360KB image's cylinder
  with `drive->track >> 1` (`YFDCDiskCylinderForDrive()`)**: when the option
  ROM accesses a 360KB disk in a 1.2MB drive at 300kbps, it double-steps
  `Seek` to physical tracks `0, 2, ..., 78` while passing logical cylinder
  `0..39` in Read/Write Data's command bytes; Read ID and Format Track have no
  cylinder command byte and address the track under the head.
- **Format Track fills a track's sectors from the IDs the guest gives it**,
  four bytes a sector by DMA: cylinder, head, sector number and size code.
  An image holds the standard layout and nothing else, so a sector is
  written where the image keeps the sector its ID numbers, on the cylinder the
  head is over and the head the command names; the ID's cylinder and head
  are only reported back. A sector numbered past the track is left alone.
  With it DOS's FORMAT formats an image of zeros, `/S` included.
- **The DMA's terminal count comes with the last byte of a transfer**: the
  BIOS sets the count to the transfer's exact length. So Write Data and
  Format Track take a byte that has arrived before ending on the count.
  Ending first loses the last byte of every multi-sector floppy write: a
  file DOS copies reads back with byte 511 wrong in the sector each write
  ends on.
- **A DMA verify cycle takes the device's byte and drops it** - the 8237
  acknowledges the device as for a transfer to memory, and leaves out only
  the memory write. The BIOS verifies sectors so, as DOS's FORMAT does each
  track, and a controller whose byte is never taken never finishes the read.
  Verify's case of its own grows `YDMATransferByte()` by 108 bytes and
  moves an inlining decision into `YPlatformWriteMemoryByte()`: alone, 0.11%
  of `dos-boot`; beside the FDC's code below, within 0.01% of sharing the
  write's device read, which grows neither.
- **The code costs `dos-boot` 0.44% without running**: none of it runs in
  the boot, and the invariant holds, but the FDC is bundled ahead of most
  modules and 490 bytes more of it moves them along in flash - 4.7460
  seconds against 4.7251 at 250MHz, `-O3`.

## video - host text fonts

- The adapters draw text in 8x8 (CGA) or 9x14 (MDA) cells, 640 or 720 pixels
  across, and shrinking those is unreadable. A host can supply a font drawn
  for a narrower cell: `YVideoConfig.text_font`, or
  `YPlatformConfig.video_text_font`, since `YPlatformInit()` replaces the
  video config wholesale. All zeros keeps the adapter's.
- **With a host font the frame size follows the mode**: text renders in the
  font's cells, 80 * width by rows * height, a 40-column cell drawn twice as
  wide; graphics keep the adapter's frame. Hosts ask `YVideoGetFrameSize()`
  rather than reading the adapter's `frame_buffer_width`, now only the frame
  without a font. A mode change forces a full redraw, so the new size
  arrives with a region covering it.
- **Nothing the guest can observe changes**: timing reads only the adapter's
  `cycles_per_scan_line` and `scan_lines_per_frame`, and `char_height`
  reaches only render-side `YVideoGetDirtyGeometry()`.
- **The cursor and MDA underline are scaled to the font's cell**: a range of
  the adapter's scan lines maps from the first font row its first line
  covers to the last row its last line covers, so a non-empty range stays
  non-empty - the CGA's lines 6-7 of 8 become rows 7-9 of 10. A single line,
  the underline, takes the start: line 12 of 14 lands on row 8 of 10.
- `YVideoInit()` refuses a font wider than a byte or with no height, logs it,
  and uses the adapter's. It records a `has_text_font` flag, so a copy of the
  state does not point back into the original.
- `text_font_test.cpp` catches rounding the cursor's end down, dirty rows left
  at the adapter's height, and region widths taken from the adapter's frame.

## video - rendering from a copy

- `YVideoCopyForRender()` is for a host that renders where the machine is not
  running - the PicoCalc's second core - and so cannot read video memory and
  registers while the CPU writes them. It copies the state and video memory
  into the host's buffers and **moves the record of what changed with them**,
  leaving the live state up to date, so the copy draws exactly what the live
  state would have and later changes go with the next copy. A host still
  rendering simply does not copy, and the changes accumulate.
- **The copy loop keeps its pointers in locals**: read through
  `video->config.vram`, the pointer reloads per byte because `frame_vram`
  might alias the state, and the copy goes a byte at a time - 46ms over
  `dos-boot` on the PicoCalc against 9.7ms. Copying the state itself is a
  `memcpy()` the compiler emits, once a frame.
- `copy_for_render_test.cpp` catches the live record left set and the copy
  left pointing at live memory.

## util - logging

- **Everything the core and hosts log is logfmt**, a line of space separated
  `key=value` pairs written by `util/logfmt.h` and rendered by the logger, so
  a host only says where a line goes. The SDL runtime and both Pico targets
  print the same lines, and one set of tools reads them.
- **A line's first pair is its kind.** A `log` is something that happened,
  said unprompted, with `level`, `module`, `tick`, `cs`, `ip` and `event`
  before its fields:

  ```
  kind=log level=warn module=platform tick=27702858 cs=0xF000 ip=0xE0A3 event=unmapped_port_read port=0x0201
  ```

  A `reply` answers a command - `kind=reply id=7 cmd=screen status=ok` - and
  is never filtered, since whoever sent the command is waiting. Severity
  belongs to logs only: a failed command is an answer, `status=error`.
- **Values are typed by how they are written**: decimals; hex with `0x` at a
  fixed width (`0x0201` a port, `0x9A` a byte); `true`/`false`; and strings,
  bare when they can be and quoted otherwise, with `"` and `\` escaped by a
  backslash and anything outside printable ASCII as `\xNN`, so a line is
  printable ASCII end to end. Keys and events are snake_case; an event is a
  name, not a sentence.
- **A call names an event and passes fields**: `Y_PLATFORM_WARN(
  "unmapped_port_read", Y_LOG_HEX16("port", port))`. Each module defines its
  four levels - `error`, `warn`, `info`, `debug` - as macros at the top of
  its source, and hosts do the same (the SDL runtime's `APP_ERROR()`). The
  event is the first variadic argument so an event with no fields is valid
  C99; fields go by value through `...`, ended by `YLogEnd()`, which the
  macro adds.
- **A filtered log costs only its filter**: `Y_LOG()` tests module and level
  before building a field, which `DoesNotEvaluateTheFieldsOfASuppressedLog`
  keeps true.
- **But a log written inline in a hot function costs it even when it never
  runs**: its fields are 16-byte structs built on the stack and copied with
  `memcpy()`, growing the frame and the code around it. The platform's four
  memory and port accessors call `Y_NOINLINE` helpers,
  `YPlatformLogUnmappedPortRead()` and its siblings: inline they cost
  `dos-boot` 0.88%, out of line 0.16%. `YCPUTick()`'s two logs measured 0.06%
  worse moved out, so they stay. Measure a new log in a `Y_HOT` function.
- **The platform stamps each log with its tick and CS:IP** through a
  `YLoggerSource` it sets once the CPU is up. `YCPUTick()` advances IP past
  an instruction before running it, so a log made during one gives the next
  instruction's address, which is also what an 8086 pushes on an interrupt.
  A log that needs the instruction's own address, like
  `invalid_instruction`, passes it as a field.
- **Module IDs 0-15 are the core's and 16-31 the hosts'**
  (`Y_LOG_FIRST_HOST_MODULE_ID`); `module_ids_test.cpp` holds the core to
  its half.
- **`YLogfmtParse()` reads a line back** into pairs in place, undoing the
  escapes, with `YLogfmtFind()` and `YLogfmtParseUInt()`. Hosts read commands
  with it - `id=7 cmd=screen`. It refuses escapes the writer does not write
  and tokens that are not pairs.
- A line is at most `Y_LOG_MAX_LINE_LENGTH` bytes, in the logger's own
  buffer. A pair that does not fit is dropped with every pair after it, and
  the line ends `truncated=true`, so it is still valid.
- **`YLoggerWrite()`, `YLoggerWriteReply()` and `YLogfmt*` are defined
  once, out of line**, as `Y_PUBLIC` functions in `util/log.c` and
  `util/logfmt.c`; `util/snprintf.{h,c}` are in `util`'s private half.
  `YLoggerIsEnabled()` and the `YLogField` builders stay `inline` in
  `util/log.h`, so a suppressed log still makes no call. Defining the rest
  `static` in the header instead gives every host file that logs its own
  specialised copy of `YLoggerWrite`, about 4.4KB each and 68KB across
  `pico/picocalc`, and moves the flash layout each time another file logs.

## util - hot path placement

- **`Y_HOT` marks a function on the per-instruction hot path.** It is empty
  by default; a target whose fast memory must be chosen defines it. The Pico
  targets define it to `__not_in_flash()` in `pico/common/yax86_hot.h`,
  putting the function in SRAM instead of flash behind the 16KB XIP cache.
- **86 functions carry it, chosen from an on-target profile.** It is worth
  **1.59x at 400MHz** and 1.23x at 125MHz - more overclocked because the
  flash clock does not scale with the core. Marking all 491 functions gives
  the last 2.8% for three times the SRAM, 46.5KB against 15KB, and rules out
  192KB of guest RAM.
- `YCPUTick()` and `YPlatformTickInternal()` carry it though `-O3` inlines
  both into `YPlatformRun()`: at `-O2` they do not inline, and would run from
  flash. The mark costs 1.1KB, because a section attribute overrides
  `-ffunction-sections` and `--gc-sections` can no longer drop the
  out-of-line copies. The mark is on `YPlatformTickInternal()`, the body
  `YPlatformRun()` calls, not on the wrapper `YPlatformTick()`. Check
  placement with `nm` - `0x2000xxxx` is SRAM, `0x1000xxxx` flash:
  ```sh
  arm-none-eabi-nm -nS build-pico/O3/yax86_pico_bench.elf | grep ' YCPUTick$'
  ```
- **The marks live in `core/src`, not a pass over the bundle**: a pass is
  silently undone by the next regeneration, at the cost of the whole 1.59x.
  If the count drops, the bundler is wrong:
  ```sh
  grep -c Y_HOT core/yax86_core.h    # 93
  ```
  That is 86 marks plus the seven lines of the macro block in
  `util/common.h`.
- **`Y_ALWAYS_INLINE` exists for a small helper with one hot caller**, which
  every level inlines until a second caller appears and `-Os` and `-O2` emit
  it out of line for both - 3.6% at `-O2`. `Y_HOT` on the helper recovers
  nothing, which says the cost is the call, not where it lands.
- **It omits `inline` when unbundled**, and that is load-bearing:
  `Y_MODULE_PRIVATE` is empty there, so the function has external linkage,
  and C99 6.7.4p3 forbids an external `inline` definition from referencing an
  internal identifier - as `YCPUGetRegisterOrMemoryOperandAddress()` does.
  Only clang diagnoses it, `-Wstatic-in-inline`, so the unbundled check
  catches it on clang legs only.
- **Pinning one decision moves others**: forcing that helper inline made GCC
  stop inlining `YCPUGetMemoryOperandAddress()` into
  `YCPUReadRegisterOrMemoryOperand()` at `-O3`, putting the effective address
  computation in flash - 0.49%, of which `Y_HOT` on it recovers 0.12%. Worth
  paying for the `-O2` win, and found only by measuring both halves.
- **`Y_HOT_DATA` is the same for data**, separate because a code section on a
  `const` array is a compile error. Nothing uses it: the obvious candidate,
  the 2KB opcode table, measures neutral in SRAM. The XIP counters
  (`xip_ctrl_hw->ctr_hit`, `ctr_acc`) show why: 559 flash accesses per
  instruction at a 98.59% hit rate, of which the table is one eight-byte line,
  0.18%. Even with the whole core in flash, moving it was worth 0.14%.
- **The marks cost 6.9% at `-Os`**, likely bus contention: an XIP hit and an
  SRAM access take different paths, so flash code does not compete with
  guest RAM while SRAM code does, and at `-Os` the core nearly fits the XIP
  cache. Placing hot code in a different SRAM bank from guest RAM has not
  been tried.
