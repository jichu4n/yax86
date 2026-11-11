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
  `YFDCInit()` is empty, kept so every module is brought up alike.
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
- **The util headers are a bundle of their own, `core/util.h`**, which every
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
  `physical_memory_size` is then held in pages, only some of them in fast
  memory ("frames") at a time - a microcontroller, say, holding the pages in
  slower external memory. `physical_memory_size` may be 0. The PicoCalc port
  is the user today; its `AGENTS.md` has how it pages and what it measured.
- **The host owns the page table and the frames**: one `uint16_t` per page,
  bit 15 `WRITABLE`, bit 14 `RESIDENT`, bits 0-13 the frame number - laid out
  at `Y_CPU_PAGED_DATA_RESIDENT`. An access the entry allows goes to the
  frame; anything else calls the host's `page_fault`, which must set the
  entry so that it allows it.
- **Two bits so that the host learns what it needs and pays nothing per
  access**: bringing a page in without `WRITABLE` makes its first write fault,
  which is how the host knows which pages to write back; clearing `RESIDENT`
  on a page it keeps in its frame makes the next access fault, which is how
  it samples which pages are in use. The frame number is read only with
  `RESIDENT` set, so a host may leave it in an entry it has cleared.
- **The CPU reads the page table itself**: `YCPUPagedDataWindow`, which
  `YPlatformUpdateAfterMemoryMapChange()` assigns from the paged entry, so an
  access to a page in its frame is a subtract, a compare and two loads, and
  only a fault reaches the platform. Going through the platform's memory map
  for every access instead, the DOS330 boot at 640KB took 7.00 seconds on the
  PicoCalc with nothing pinned; with the CPU indexing, 6.32.
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
- **A write that faults is reported to the decode cache once, not twice.**
  The CPU reports every write it makes, and `YPlatformWriteMemoryByte()`
  reports every write it is handed, so a write that goes through the
  callback is counted twice. That changes when page counters wrap, so where
  runs of instructions end, so when the guest takes its timer interrupts.
  For paged memory it would mean the number of frames changes what the guest
  does, so `YPlatformCPUCallbackWriteMemoryByte()` sends the CPU's paged
  writes past the second report. Every other region is still counted twice,
  and the `dos-boot` invariant depends on that. With this, MS-DOS 3.30
  running five programs at 640KB retires the same instructions with 128
  frames as with 4, and with 256 as with 2.
- **Both data windows stay, and a host may set either, both or neither.**
  The direct window could be modelled as a paged one - frames over
  `physical_memory`, entry *i* `RESIDENT | WRITABLE | i` - but every host that
  does not page would pay for it: `dos-boot` with all of 128KB paged and every
  page in its frame is 15% slower than direct. Nor is there a switch between
  them: an empty window already is one (`end` 0, `size` 0), the CPU's code is
  the same whichever are set, and a flag would cost the same test. Each
  combination has a user: direct only for the bench, SDL and the PicoCalc
  without PSRAM; paged only for the PicoCalc's default; both for its
  `YAX86_PICOCALC_PINNED_KB`; neither for the CPU tests' helper and mocks.
- **A paged-only build pays for the direct window it does not have**, by
  reading of the disassembly, not yet measured. Every inlined data access
  first loads `direct_data_window.end`, compares and falls through, about 4
  cycles on a Cortex-M0+, before calling the paged path. And
  `paged_data_window` is last in `YCPUState`, past the offsets a Cortex-M0+
  load encodes, so the paged path spends 4-5 instructions forming addresses
  (`adds #117; adds #255`, `movs #186; lsls #1`). It is last so that no field
  the direct path reads moves. Against roughly 500 host cycles an emulated
  instruction, each is probably around 1%. Two changes to try, measured on
  the PicoCalc - seven commands and the DOS330 boot - with `dos-boot` on the
  bench to show direct-only builds lose nothing:
  - a core build option compiling the direct check out, the PicoCalc's
    no-PSRAM fallback then paging its 128KB to itself with every page in its
    frame;
  - `paged_data_window` near the front of `YCPUState`, checking with
    `objdump` that the direct path's fields stay within reach.
- **REP string runs plan only over the direct data window**, so in paged
  memory they go element by element, and a boot with more of memory paged
  retires a few hundred fewer instructions - the difference the bulk runs'
  decode cache reporting makes (see `src/cpu/AGENTS.md`).
- **What paging costs when nothing faults**: `dos-boot` at 128KB on the bench,
  every page in a frame, is 2% slower with the first 64KB direct and the rest
  paged, and 15% slower with all of it paged - a direct access is a compare
  and a load, a paged one a call and two more loads. The PicoCalc builds
  both; see its `AGENTS.md`.

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
  running fast.
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

- **Format Track fills a track's sectors from the IDs the guest gives it**,
  four bytes a sector by DMA: cylinder, head, sector number and size code.
  An image holds the standard layout and nothing else, so a sector is
  written where the image keeps the sector its ID numbers, on the track the
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
