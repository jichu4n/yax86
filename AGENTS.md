# Project: yax86

## Project Goals

This project is a small IBM PC/XT emulator for minimal MCU platforms like
the Raspberry Pi Pico, as well as the browser via SDL and Emscripten.
- Emulates a basic PC/XT system with an 8086/8088 processor
- Runs MS-DOS 3.3 on top of GLaBIOS

## Codebase

### core - core emulation logic

- The `core` directory contains the core emulation logic, including modules
  that emulate the CPU and hardware such as video adapter, keyboard and floppy.
- Importantly, `core` modules do NOT depend on any external runtime (SDL etc)
  and only interact with the host via callbacks.
- Each directory under `core/src` corresponds to a module. Each module bundled
  into a single header in the root directory based on `bundle.json`.
    - For example, the source code in the CPU module `core/src/cpu` is bundled
      based on `core/src/cpu/bundle.json` into `cpu.h` in `core` directory
    - The external interface of each module is defined in its `public.h`.
- The `platform` module in `core/src/platform` is the "virtual motherboard"
  that connects the other modules together, including memory and port mapping.
- The `core/tests` directory contains unit tests for each module.
- All the module header bundles are concatenated into a single header
  `core/yax86_core.h`. It is also built into a static library
  `libyax86_core.a`.
- ROMs live alongside the module that maps them and are compiled into the
  library rather than read from a file at run time, since targets like the
  Raspberry Pi Pico have no file system. `core/tools/generate-rom-data-files.js`
  turns a ROM image into a C array and header - the `generate_rom_data` calls in
  `core/CMakeLists.txt` name the module, ROM file, output name and symbol. The
  generated `*_rom_data.{c,h}` are committed, and CI runs `git diff
  --exit-code`, so regenerate and commit them when a ROM changes.

#### core/src/cpu - instruction decoding

- `CPUFetchNextInstruction()` decodes directly into the caller's `Instruction`
  rather than filling a local and copying it out at the end. It runs for every
  instruction the machine executes, and that copy cost more than the whole
  decode: measured over a boot and idle run at the DOS prompt with `gcc -O3`,
  removing it is worth 3.5% of host instructions, 8.4% of data references (97M
  fewer reads and 97M fewer writes), and about 19% of wall clock time.
- The wall clock win being so much larger than either count is the interesting
  part, and it is not a cache effect - D1 misses are identical either way.
  Writing a 12 byte struct to the stack and immediately reading it back creates
  dependent store-to-load pairs that stall, and neither callgrind nor
  cachegrind models that. Anything on this path that round trips through memory
  is worth more than its instruction count suggests.
- The cost of the copy is why `Instruction` is worth keeping small, and its
  flag bitfields currently total exactly 8 bits. A ninth costs a whole byte and
  takes the struct from 12 to 13, which measured as ~1.8% more host
  instructions over a boot-and-idle run.
- Because the decode happens in place, a failed fetch leaves the caller's
  `Instruction` holding however much had been decoded rather than untouched.
  Every caller treats a failed fetch as fatal, so nothing reads it back.
- `immediate_size` is a three bit field but `immediate[]` holds 4 bytes, and
  `displacement_size` is a two bit field where `displacement[]` holds 2. No
  opcode table entry exceeds the arrays, but nothing in the types says so, and
  `gcc -O3` warns about the writes once it can no longer see the bound through
  a pointer. The immediate loop bounds itself by the array for that reason.

#### core/src/cpu - prefix decoding

- Both prefix groups are contiguous encoding families, so each is identified by
  one masked compare: segment overrides encode as `001ss110` and LOCK/REP as
  `111100rr`. The bits each mask leaves free are the selector - `ss` is in the
  8086's sreg order, which is the order `kES` through `kDS` are numbered in, so
  the segment register index is an offset from `kES`.
- Prefixes are decoded into fields on `Instruction` as they are fetched, rather
  than kept as raw bytes for each consumer to walk. `segment_override` uses 0
  (`kNoSegmentOverride`) for absent, which is `kAX` and never a segment, so a
  zero-initialized `Instruction` is correct by construction. LOCK is consumed
  but not recorded - nothing on a PC/XT acts on it.
- The test used to be a linear scan over an eight-element array. How much
  writing it out is worth depends entirely on whether the compiler would have
  derived the same tests anyway, and the split is sharp - measured over a boot
  and idle run at the DOS prompt, in host instructions retired:

  |       | gcc    | clang  |
  | ----- | ------ | ------ |
  | `-O1` | -5.30% | -7.79% |
  | `-Os` | -5.83% | -7.10% |
  | `-O2` | -6.25% | -0.09% |
  | `-O3` | -0.96% | -0.08% |

  Below `gcc -O3` and `clang -O2`, neither compiler converts the scan and this
  is worth 5-8%. At or above, both derive their own equivalent - `clang` picks
  a 64-bit bitmap and a `bt` for the four segment overrides, which is arguably
  better than the mask - and it comes out a wash.
- Classify and record in one pass. Deciding whether a byte is a prefix and
  deciding which group it belongs to are the same test, so `ApplyPrefixByte()`
  does both and returns whether it consumed anything, rather than the loop
  asking first and the recording asking again. Splitting them cost 0.7% on
  `clang`, which was enough on its own to make this change a net regression
  there at `-O2` and above.
- `kMaxPrefixBytes` is not a storage bound - nothing stores anything per
  prefix, and the count is a local in the fetch loop rather than a field. It
  exists because a run of prefix bytes would otherwise fetch forever: a real
  8086 hangs there too, since a prefix is not an instruction boundary and so no
  interrupt is ever recognized, but this has to hand control back to its
  caller. `Instruction.size` is a `uint8_t` that `CPUTick()` adds to IP, so the
  bound is derived from what that can still address - 247 prefixes plus the 8
  bytes an instruction can otherwise carry comes to exactly 255. It was 2 while
  prefixes were kept in a fixed array, which rejected legal encodings:
  `LOCK ES: REP MOVSB` is three.
- Write the `0xF0-0xF3` test as a range check rather than a mask. Both are
  exact, but a range folds into one subtract and compare where the mask needs a
  separate `mov`, `and` and `cmp` - which is how `gcc -O3` writes it when left
  to itself. Put it before the segment override test: bytes reaching here are
  usually not prefixes at all and so run both, making the combined cost what
  matters.

#### core/src/platform - the memory map

- `GetMemoryMapEntryForAddress()` is the hottest lookup in the emulator - every
  instruction byte and nearly every operand comes through it - and it used to
  walk the map linearly. A walk is cheap where branches are predicted and the
  region wanted is usually first. On a Cortex-M0+ it is neither: there is no
  branch predictor, and code running from the BIOS ROM misses the first entry
  every time. Indexing the map by page instead is worth **7.0%** on `dos-boot`.
- `memory_page_map` holds one entry index per 4KB page of the 8086's 1MB, so
  256 bytes of index answers a lookup with a shift, a load and a bounds
  compare. 4KB is not tuned - it is the coarsest page that leaves every region
  the machine registers aligned, and a finer one would only cost more memory.
- Two index values are not entry indices. `kMemoryPageUnmapped` is a page no
  entry covers, and answers `NULL` directly. `kMemoryPageStraddled` is a page
  more than one entry has a share of, which the index cannot answer at all, so
  the lookup falls back to the walk. Nothing registers a misaligned region
  today, and that is exactly why the fallback needs its own test - see
  `platform_memory_map_test.cpp`, which registers one deliberately.
- `RegisterMemoryMapEntry()` extends the index rather than rebuilding it. Only
  the pages the new entry touches can change, and entries may not overlap, so
  nothing already in the index can have a share of one of them - which means a
  registration never has to look at the rest of the map. Building the whole
  index costs one pass over the address space between every entry, where
  rebuilding it per registration cost a pass each. `UpdateMemoryPageMapForEntry()`
  takes the entry's index rather than the entry, because the index is what goes
  into the map - the entry is derivable from it, and not the other way round.
- An entry reaching past the top of the address space is rejected, because the
  index has a slot per page of that space and none above it. That rejection is
  what makes the index the whole answer: an address above the space is unmapped
  by definition, so `GetMemoryPageMapIndex()` says so directly rather than
  sending the lookup off to walk the map for an entry that cannot be there.
  Guest addresses never reach it in any case - `ToRawAddress()` masks every one
  of them to the 8086's 20 bits - so this is about what a host may register,
  not about what the CPU may ask for.
- A region whose size is not a whole number of pages is handled by
  construction rather than by arithmetic: its last page comes out straddled and
  takes the fallback. That is the case for any option ROM whose size is not a
  multiple of 4KB.

#### core/src/cpu - the execute path

- `CPUExecuteInstruction()` checks an instruction against the opcode table
  before running it: that `has_mod_rm` and `immediate_size` are what the entry
  says they should be. `CPUTick()` skips them and calls
  `CPUExecuteDecodedInstruction()` directly, because its own
  `CPUFetchNextInstruction()` *derived* both from that same entry a few lines
  earlier. The checks re-establish something that cannot have changed in
  between, and they are not free - a table read, two compares and a recomputed
  immediate size on every instruction the emulator runs.
- The public entry point keeps them, for a caller that built an `Instruction`
  by hand rather than decoding one. It is not on any hot path - nothing inside
  the core calls it - so the duplication costs nothing.
- Neither path checks that the opcode has a handler, because every entry in the
  table has one. The eight that used to be null are the prefix bytes - the four
  segment overrides and `0xF0-0xF3` - and they are exactly the bytes
  `ApplyPrefixByte()` consumes, so a successful fetch can never leave one in
  `opcode` and the check was already dead on the tick path. They now point at
  `ExecuteInvalidOpcode()`, which returns `kInstructionInvalid`: the same answer
  the null check gave, for the hand-built instruction that is the only way to
  reach one. What that buys is a table with no null in it, so dispatching
  without looking first is safe by construction rather than by argument.
  `EveryEntryHasAHandler` in `opcode_table_test.cpp` is what keeps it that way -
  an entry added later with the field left out would otherwise be a call
  through null rather than an invalid instruction.
- One behavioural consequence: `on_before_execute_instruction` now fires after
  validation rather than before it, so it no longer runs for an instruction
  that is about to be rejected as an encoding mismatch - though it does now run
  for a hand-built prefix opcode, which `ExecuteInvalidOpcode()` rejects from
  inside the handler rather than before the callback. Only
  `core/tools/cpu_demo` uses the callback, and it never builds an `Instruction`
  by hand.
- `CPUExecuteDecodedInstruction()` is `YAX86_NOINLINE`. Inlined into `CPUTick()`
  it measured 31% slower on a Cortex-M0+: the execute path wants registers, the
  core has few, and folding the two together makes both spill. The effect only
  exists once the hot path is in SRAM - running from flash, XIP misses dominate
  and hide it - which is why the mark and this change belong together.
- Worth 1.84% at `-O3` and 4.58% at `-O2`, measured at 400MHz on `dos-boot`.
  The gap is the `CPUTick()`/`PlatformTick()` marks, which are inert at `-O3`.

#### core/src/util - hot path placement

- `YAX86_HOT` marks a function as being on the per-instruction hot path. It is
  empty by default, because on a machine with a normal cache hierarchy there is
  nothing useful to say; a target whose fastest memory has to be chosen
  explicitly defines it to whatever placement attribute it needs. The Pico
  harness defines it to `__not_in_flash()`, which puts the function in SRAM
  instead of executing it from QSPI flash through a 16KB XIP cache.
- 81 functions carry it, chosen from an on-target profile rather than by
  intuition. On the Pico it is worth **1.59x at 400MHz** (13.647s to 8.597s on
  `dos-boot`) and **1.23x at 125MHz**. The win is larger when overclocked
  because the flash SPI clock does not scale with the core, so an XIP miss
  costs more core cycles the faster the core runs.
- `CPUTick()` and `PlatformTick()` carry it even though at `-O3` they are
  inlined into `PlatformRun()`, which already did. The mark buys nothing there
  and costs 1.1KB - an explicit section attribute overrides
  `-ffunction-sections`, so all the marked functions share one section and
  `--gc-sections` can no longer drop the out-of-line copies individually. It is
  worth paying: at `-O2` neither inlines, and without the mark the two hottest
  functions in the emulator execute from flash. Check placement with `nm`
  rather than assuming, since whether they inline depends on the level and on
  how large `PlatformRun()` has grown:
  ```sh
  arm-none-eabi-nm -nS build-pico/O3/yax86_pico_bench.elf | grep ' CPUTick$'
  ```
  An address of `0x1000xxxx` is flash and `0x2000xxxx` is SRAM.
- **The annotations live in `core/src`, not in a pass over the generated
  bundle.** That is the whole point: a post-processing pass over
  `core/yax86_core.h` is silently undone by the next bundle regeneration, which
  costs the entire 1.59x with no error and no warning. Annotating the source
  means regeneration carries the marks through by construction. If the
  annotation count in the bundle ever drops, something is wrong with the
  bundler, not with someone's memory:
  ```sh
  grep -c YAX86_HOT core/yax86_core.h    # 159
  ```
  That is 82 annotations plus the macro block in `util/common.h`, whose seven
  lines all match on the substring, once per each of the 11 module bundles.
- Annotating every function in the core instead was measured, and is not worth
  it. Against the same baseline: the targeted set gives 97.2% of the win for
  15KB of SRAM, where all 491 give 100% for 46.5KB. The extra 2.8% costs three times
  the memory, and it forecloses the 192KB guest RAM option outright - 206,664
  bytes of image plus 64KB more guest RAM does not fit in 256KB, where the
  targeted build does.
- `YAX86_HOT_DATA` is the same thing for data, and is separate because a
  compiler will not put executable code and read-only data in one section - a
  code section attribute on a `const` array is a hard compile error, not a
  warning. The macro is kept so that whoever finds a candidate that pays
  reaches for the right one rather than for `YAX86_HOT`, but nothing currently
  uses it. The obvious candidate is the 256-entry opcode table - 2KB, read once
  on every instruction fetch - and it does not pay: in SRAM it measured
  neutral, 2.6ms over an 8.6 second run and inside the noise floor.
- That result is not because the table is already cached, or not mainly, and
  the difference matters for whatever gets considered next. Putting the whole
  core back in flash so that the XIP cache is thrashed by 88KB of code - the
  condition most favourable to the table - and moving the table alone is still
  worth only 0.14%, 13.647s to 13.628s. Reading the RP2040's XIP hit and access
  counters (`xip_ctrl_hw->ctr_hit` and `ctr_acc`) across that same run says why:

  ```
  xip_acc  1,299,140,378    559 flash accesses per emulated instruction
  xip_hit  1,280,792,338    98.59% hit rate, so 7.9 misses per instruction
  ```

  The table is one eight-byte line per instruction, **0.18% of that traffic**,
  so the most it could ever have been worth is about 1% even if every read of
  it missed. Cache residency accounts for the rest of the gap - those few lines
  are re-referenced every instruction, so they are about the last thing an LRU
  cache gives up - but the ceiling was low before the cache did anything.
- The lesson generalizes, and is the one to carry into the next change: on this
  part, **what matters is how much flash traffic something generates, not how
  hot it feels.** A structure read once per instruction is noise against 559
  instruction fetches. That ratio is the whole reason moving code to SRAM is
  worth 1.59x while moving a table read on every single instruction is worth
  nothing measurable.
- The marks help at `-O2` and `-O3` and **hurt at `-Os`**, which went from
  34.264s to 36.614s, a 6.9% regression. The likely cause is bus contention:
  an XIP cache hit reaches the core over a different bus path than an SRAM
  access, so code in flash does not compete with the guest RAM array, while
  code in SRAM does. At `-Os` the core's code is small enough to sit in the
  16KB XIP cache and that trade is a loss. Placing hot code in a different SRAM
  bank than guest RAM has not been tried and is the obvious next question.

#### core/src/platform - idle skipping

- MS-DOS waits for a keystroke by polling rather than halting, so an idle
  command prompt costs exactly as much to emulate as a running program. Measured
  at the MS-DOS 3.30 prompt, the guest retires about 390,000 instructions a
  second and never once halts, all of it the same loop through the `INT 21h`
  dispatcher and the CON driver into `INT 16h`.
- It does announce itself, though: DOS issues `INT 28h`, the documented DOS idle
  interrupt, 783 times a second while it waits. `PlatformConfig`'s
  `enable_dos_idle_skip` makes `PlatformRun()` treat that as a signal to advance
  the clock to whichever comes first, the next device deadline or the end of the
  budget it was given, instead of executing the loop until it gets there. The
  guest's own handler still runs; only the waiting is skipped.
- Time is advanced, not discarded: `PlatformSync()` runs across the skipped
  interval, so every device sees every cycle and the guest's timer tick count is
  unchanged. Bounding the skip by the caller's budget is what keeps that true -
  without it the machine would be handed more emulated time than was asked for
  and would run its clock fast.
- It is opt in because it is not free of consequence: a program timing a loop
  from inside its own `INT 28h` handler would see time jump. The SDL runtime
  turns it on; the tests leave it off, so nothing else changes.
- What the skip is worth is capped by the nearest device deadline, which is why
  `PITTicksUntilNextEvent()` schedules one for channel 0 alone. After POST the
  BIOS leaves channel 2 programmed at a reload of 1356 with the speaker gated
  off, and asking to be woken every 678 ticks for an output nobody listens to
  truncated every skip. Over 20 emulated seconds at the prompt the skip is
  worth 1.4x with the other channels scheduled and 5.6x without.
- Only channel 0's output leaves the PIT - `PITChannelSetOutputState()` raises
  IRQ 0 from it. Channels 1 and 2 record a transition and nothing more, and
  that record is recomputed whenever the PIT is advanced, which every path that
  reads it does first. Give another channel's output an effect and the
  scheduling has to change with it; there is a comment at both ends saying so.

#### core/src/cpu - instruction counting

- `CPUState.instructions_retired` counts instructions the CPU actually ran; a
  halted tick advances the clock but retires nothing. It lives in the CPU
  because `CPUTick()` already knows which it did, and a caller can only find
  out by sampling `is_halted` before every tick - which is what the benchmark
  harnesses did, giving up `PlatformRun()`'s batching for a number the core
  already had. Measured with callgrind on the `compute` workload, counting in
  `CPUTick()` costs 0.17%; the same counter driven from `PlatformTick()`,
  where the flag has to be re-derived, cost 1.09%.

#### core/src/hdc - hard disk controller

- Emulates an XT-IDE rev 2 controller: an 8-bit ATA task file operated in PIO
  mode, with no DMA and no interrupt line.
- GLaBIOS implements INT 13h for floppies only and rejects drive numbers above
  3, and it never writes the hard disk count at 40:75 or the INT 41h parameter
  table pointer. Hard disk support in the guest therefore comes entirely from
  an option ROM: GLaBIOS's POST scans from C800:0000 on 2KB boundaries for a
  ROM starting with `55 AA` whose bytes sum to zero, and far calls offset 3 to
  let it install its own INT 13h.
- The ROM in use is the XTIDE Universal BIOS, in
  `core/src/hdc/XTIDE_Universal_BIOS_XT_r631.rom`. It is compiled into the
  library the same way the system BIOS is - see the ROM data generation section
  below - because reading it from a file at run time would not work on targets
  like the Raspberry Pi Pico, which have no file system. The module maps it at
  0xC8000 and the platform sizes the memory region from the ROM itself.
- The ROM's stock configuration is a rev 2 controller at port base 0x300, which
  POST reports as `Master at 300h`. Rev 2 crosses address lines A0 and A3, so a
  physical port offset maps to an ATA register offset with bits 0 and 3
  swapped - the BIOS polls status at physical 0x30E, not 0x307.
- An empty drive slot leaves the task file undriven, so its status register
  reads as 0. That is how the option ROM concludes a drive is absent: it
  selects the drive, polls status, and gives up when the ready bit never
  appears.
- Commands complete synchronously inside the port write that issues them, so
  there is no HDCTick and no seek or rotational timing. The controller is
  polled PIO, so nothing in the guest can observe the difference.
- The drive's data port is 16 bits wide and the card is on an 8-bit bus, so
  each word crosses the bus as two byte accesses against two ports, and only
  the low byte port runs a bus cycle. A read of it moves a whole word: the low
  byte goes to the guest and the card latches the high byte, which the high
  byte port hands back without touching the drive. A write is the other way
  round, because the card cannot start a cycle until it has the whole word -
  the guest loads the latch through the high byte port first, and writing the
  low byte commits the pair. So the two directions use opposite byte orders,
  and a byte stream shared by both ports models neither. Streaming got writes
  backwards, which put every word on the disk back to front: a partition
  table's 0xAA55 signature landed as 0x55AA and DOS reported "Invalid drive
  specification".
- Identify Device reports the cylinder count of the geometry the guest asked
  for with Initialize Device Parameters, worked out from the capacity rather
  than copied from the physical geometry. That keeps everything the drive
  advertises addressable: reporting the physical count alongside a different
  head or sector count would describe a larger drive than this one, and
  addresses in the part that does not exist would come back as ID not found.
- The device control register is at ATA register 0xE, which the address line
  swap puts at physical port 0x307. Reads of it are the alternate status
  register and return exactly what status returns; a write with the software
  reset bit set abandons any transfer in progress and returns the drive to
  idle. The option ROM uses neither - it polls status at 0x30E - but an
  unanswered alternate status reads as 0xFF, which decodes as a drive that is
  permanently busy.
- The SDL runtime loads a hard disk image into memory and discards guest
  writes, the same as the floppy. FDISK, FORMAT and booting from C: all work
  within a session, but the disk starts out the same way on every run.

### sdl - SDL runtime

- The `sdl` directory contains an SDL3-based runtime for the emulator.
- It is compiled via Emscripten to produce a WebAssembly binary and JavaScript
  wrapper (`yax86_sdl.{wasm,js}`).
- How much guest time a pass of the main loop runs comes from the wall clock,
  not from how often the host calls it. Under Emscripten the loop is driven by
  `requestAnimationFrame`, whose rate is the display's - and on a machine whose
  compositor has no vsync to lock to, it can fire well above it. Measured on an
  i7-3667U it ran at 120-200Hz, which with a fixed budget per callback ran the
  emulated 8088 at twice speed and its clock with it. A gap longer than
  `MAX_CATCH_UP_NS` is dropped rather than made up, so returning from a
  backgrounded tab does not produce a burst of catch-up emulation.
- The loop also declines to step more often than `MIN_STEP_NS`, a little under
  a sixtieth of a second. The emulated display refreshes sixty times a second,
  so stepping faster draws nothing new, and it is not free: with idle skipping
  on, a pass costs roughly one trip round the guest's idle loop whatever budget
  it was given, so an idle machine's cost tracks the callback rate rather than
  emulated time. The threshold sits under 1/60 deliberately - at exactly 1/60 a
  callback arriving a hair early would be deferred to the next one, halving the
  rate instead of capping it. Events are pumped on every callback either way,
  so input stays responsive.
- It takes an optional floppy image path, defaulting to `floppy_a.img`. A 10MB
  hard disk is attached by default - blank unless `--hdd <image>` names an
  image, so it can be partitioned and formatted from inside DOS - and
  `--no-hdd` leaves the machine without one. The default matters because the
  Emscripten build has no command line, so a hard disk gated behind a flag
  would be unreachable in the browser.
- An optional `--cga` or `--mda` flag selects the video adapter, defaulting to
  CGA. The adapter is fixed for the life of the machine: it is chosen through
  `PlatformConfig.video_adapter` before `PlatformInit()`, which decides both
  what the platform registers and what the PPI's DIP switches report, and the
  BIOS branches on those switches to decide which adapter to program.
- `src/audio.c` turns the frequency the core reports for the PC speaker into a
  square wave. The core hands over a frequency rather than a stream of samples,
  so nothing here has to reconcile emulated time with the audio clock - the
  synthesizer just holds a tone until told otherwise.
    - The amplitude is ramped over a millisecond whenever the tone starts or
      stops. Cutting a square wave off mid-cycle at full amplitude is louder
      than the click a real speaker makes, so the ramp is closer to the
      hardware than no ramp, not further from it.
    - Browsers refuse to start audio until the user has interacted with the
      page, so `AudioResume()` is called again on the first input event. The
      POST beep happens before any interaction is possible and so is never
      heard in the browser; beeps after that are.
    - To check the audio without listening to it, run the native build under
      SDL's disk audio driver, which writes raw samples to a file:
      `SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=out.raw`. SDL may upmix
      the mono stream, in which case the file is interleaved stereo.

### pico/bench - Raspberry Pi Pico benchmark harness

- `pico/bench` builds a firmware image that runs the core on an RP2040 and
  reports how fast it went. The Pico is the platform the emulator is meant to
  fit on, and it is not a small desktop: a Cortex-M0+ has no branch predictor,
  no speculation and no cache except the 16KB XIP window that code executes
  from flash through. A desktop ablation cannot close a question about it.
- It is a consumer of the core, not part of it. Nothing under `core` knows the
  harness exists and `pico/bench/src/main.c` uses only the public interface.
- The build is a standalone CMake project rather than a subdirectory of the top
  level one, because that build produces a native library, the tests and the
  SDL runtime, none of which cross-compile, and the Pico SDK has to own the
  toolchain from before `project()` onwards.

#### The workload, and its invariant

- There is one workload, `dos-boot`: power on the machine, let GLaBIOS POST,
  boot MS-DOS 3.30 off the floppy image in `resources`, and stop when the `A>`
  prompt appears. DOS asks for the date and then the time on the way, and the
  harness answers both by watching the CGA text buffer and pressing Enter.
- It is the whole machine - CPU, PIT, PIC, video, FDC - rather than a CPU loop,
  which is the mix of guest code the emulator exists to run. What it executes
  is whatever the BIOS and DOS do, so it is only comparable against another
  yax86 build.
- **It is deterministic: 27,724,061 emulated cycles and 2,324,726 retired
  instructions, every run, at every optimization level.** Both numbers are
  printed. If either moves, the change altered behaviour and the times either
  side of it are not comparable - check that before believing a speedup.
- The floppy is compiled into flash as a C array by
  `core/tools/generate-rom-data-files.js`, the same generator the ROM images
  use, because this target has no file system. It is generated into the build
  directory rather than committed the way the ROMs' arrays are, since at 360KB
  the image is a couple of megabytes of C source. Guest writes are discarded:
  360KB of writable copy is several times the SRAM left over, and nothing on
  the way to a command prompt writes to the disk.
- `enable_dos_idle_skip` is deliberately left off. The run stops at the prompt,
  which is where DOS starts idling, so the skip would have nothing to skip -
  and leaving it off keeps the workload a measure of how fast guest
  instructions execute rather than of how many are elided.

#### Building and running

- Needs `PICO_SDK_PATH` set and an `arm-none-eabi` toolchain. `build.sh`
  configures and builds into `build-pico/<level>`, and prints what the image
  costs in flash and SRAM:
  ```sh
  ./pico/bench/build.sh              # the default level, -O2
  ./pico/bench/build.sh Os O2 O3     # all three, to compare
  YAX86_PICO_SYS_CLK_KHZ=400000 ./pico/bench/build.sh O3
  ```
- `run.js` reboots the board into BOOTSEL with `picotool`, loads the image and
  captures what it prints:
  ```sh
  ./pico/bench/run.js build-pico/O3/yax86_pico_bench.uf2 --out capture.txt
  ```
  It exits non-zero if the run did not finish. `--no-flash` captures from a
  board already running the image.
- The serial port must be opened exactly once. The firmware waits for DTR
  before printing anything and every open asserts it, so configuring the line
  with a separate `stty` first starts the run and the header is gone before a
  reader attaches. `run.js` wraps the descriptor it already holds in a
  `tty.ReadStream` and calls `setRawMode()` on that, which configures the line
  without opening the device a second time. It finds `/dev/ttyACM*` rather than
  hard-coding it, because the port re-enumerates after a flash or a watchdog
  reset and can come back on a different node.
- `build.sh` is bash, like the scripts under `tools`, because what it does is
  run `cmake` in a loop. Everything it would otherwise have to parse is Node,
  like the ROM and bundle generators under `core/tools`, and uses only the
  standard library - there is no `package.json` anywhere in the repository and
  nothing here should need one.
- The summary table's figures come from `image-size.js`, which reads the ELF
  program and section headers directly rather than parsing `readelf` and
  `size`. That is not gratuitous: recovering those numbers from `readelf`
  output means parsing hex in awk, and `strtonum()` is a GNU extension, so the
  script that worked on the machine it was written on silently produces empty
  columns under mawk - the default awk on Debian and Ubuntu - and under the BSD
  awk on macOS. Reading the headers is shorter, portable, and drops two
  toolchain dependencies. It reproduces `arm-none-eabi-size`'s text column
  exactly, which is what the `core_text` column has always been.
- `src/yax86_hot.h` is force-included into every translation unit, and is where
  this target defines `YAX86_HOT` - the mark the core puts on its
  per-instruction hot path - to `__not_in_flash()`, placing those functions in
  SRAM rather than executing them from flash through the XIP cache. It is
  force-included rather than included from the core so that the core knows
  nothing about this target, and it is inert against a core revision that does
  not use the mark. See the hot path placement section under `core` for what it
  is worth and why only some functions carry it.
- The optimization level is a CMake option applied through
  `CMAKE_C_FLAGS_RELEASE` rather than to the emulator's target alone, so that
  the SDK and the C library are compiled the same way. What is being measured
  is how a whole image behaves in the XIP cache, and an emulator built one way
  inside an SDK built another would not answer that.
- `YAX86_CORE_ROOT` names the checkout whose `core` is compiled in, so two core
  revisions can be measured against one harness:
  ```sh
  git worktree add /tmp/yax86-variant <branch>
  YAX86_CORE_ROOT=/tmp/yax86-variant ./pico/bench/build.sh O3
  ```
  A build directory reused across two core roots keeps one object file per
  root, since CMake names an object after the absolute path of its source.
  `build.sh` spells the path out rather than searching, so its `core_text`
  column always refers to the core just built.

#### Baseline

- Measured with GCC 16.1.0, SDK 2.3.0 and picotool 2.3.0, at the default 128K
  of guest RAM and the stock 125MHz clock. "flash" is the core running entirely
  from flash, which is what the harness measured before `YAX86_HOT` existed;
  "SRAM" is with the hot path placed there:

  | level | flash | SRAM | image flash | image SRAM | core `.text` |
  | ----- | ----- | ---- | ----------- | ---------- | ------------ |
  | `-Os` | 34.264 | 36.614 | 443,752 | 165,052 | 59,895 |
  | `-O2` | 36.494 | 30.015 | 458,044 | 168,612 | 73,625 |
  | `-O3` | **33.492** | **26.858** | 471,676 | 176,292 | 88,497 |

  A real 8088 runs this in 5.812 seconds, so `-O3` at the stock clock is about
  a fifth of one. The flash column is historical - it is what the harness
  measured before `YAX86_HOT` existed, and is not re-derived as the core
  changes. The seconds in both columns are a snapshot and go out of date with
  every optimization; what is durable is how the levels rank against each
  other, which is the reason the table is here.
- The two columns do not rank the levels the same way, which is the whole
  reason this harness exists. Running from flash, `-O2` was the slowest of the
  three and `-Os` beat it by 6% - the XIP cache rewards a small image enough to
  overturn the desktop ordering. With the hot path in SRAM the ordering becomes
  the ordinary one, `-O3` fastest and `-Os` slowest by a wide margin, because
  the cache is no longer what decides. See the hot path placement section under
  `core` for why `-Os` gets *worse*.
- Reproducibility is excellent, which is what makes small differences worth
  believing: three independent flash-and-run cycles of the same `-O3` image
  gave 33.491892, 33.492932 and 33.490758 seconds - a spread of 2.2ms over 33.5
  seconds, or 0.0065%. **A 1% difference is real.** Do not demand a large
  margin before believing a result, and equally do not accept a "neutral"
  result as a win. `-O2` losing 9% to both of its neighbours is a genuine
  result of this kind, and the sort the XIP cache makes possible.
- At 400MHz, `-O3` takes 7.895 seconds, or 3.51 emulated MHz and 0.29 MIPS -
  against 13.647 seconds before the hot path moved to SRAM. **Do not raise the clock past
  400MHz.** That is a standing instruction, not a technical limit.
- Flash is dominated by the 360KB floppy image; the code itself is under 110KB.
  The `core_text` column is the whole core library section, which includes
  about 32KB of constant data - the ROM images, the font tables and the opcode
  table - that does not move with the optimization level.
- `stack_peak` reports how deep the stack got, which is a number a real Pico
  port needs. The unused stack is painted with `0x5A5A5A5A` from `main`'s own
  frame and afterwards searched for how far the paint was overwritten, so it
  costs nothing while the run is going. Interrupt frames count, making it the
  true peak rather than the emulator's share. The measured peak is 728-860
  bytes of a 4KB bank, depending on level - `-O3` inlines the instruction
  handlers into deeper frames than the others.
- Guest memory and the stack cannot collide, and not by a narrow margin. Guest
  RAM sits at the bottom of the 256KB bank while the stack is in SCRATCH_Y,
  growing down within its own 4KB. To reach a guest byte it would have to grow
  through the whole of SCRATCH_Y, then all of SCRATCH_X, then the ~100KB of
  unused heap. The heap grows the other way, up and away from guest RAM.

#### Timing discipline

- Host I/O on this target costs orders of magnitude more than the guest
  instruction it would be reporting on - the same trap as leaving a browser
  console open in front of the WASM build. So the UART is switched off entirely
  and nothing is printed from inside a run: results accumulate in RAM and are
  printed once the run has finished.
- Everything printed is derived from integers, so no floating point formatting
  is linked in. Emulated cycles per microsecond is emulated megahertz and
  retired instructions per microsecond is MIPS, so neither needs a unit
  conversion.
- The one perturbation that cannot be removed is USB servicing, which the SDK
  drives from a timer interrupt. It costs the same in every build, so it does
  not affect any comparison this harness exists to make.
- The run is driven a batch of emulated cycles at a time through
  `PlatformRun()`, which is how an application drives it, rather than an
  instruction at a time. The core counts retired instructions itself, so there
  is nothing left for a caller to do per instruction. An invalid opcode does
  not stop the run: the 8088 has no invalid opcode exception, so a real machine
  would carry on.

#### Overclocking

- `YAX86_PICO_SYS_CLK_KHZ` raises the system clock, and the change happens
  *after* USB is up rather than before. A clock the chip cannot take hangs it,
  and a hang before USB enumerates leaves no way in except the BOOTSEL button -
  which is no good on a board being flashed remotely.
- A watchdog turns a hang into a reboot, and the reboot lands back in `main`
  where a marker says not to try the same clock twice.
  `watchdog_caused_reboot()` alone is not evidence of a hang: `picotool`
  reboots the chip through the watchdog too, so it reads true after every
  flash. The marker lives in `__uninitialized_ram`, so it survives a reset but
  not a power cycle, and is only ever left set across the window where a hang
  is possible.
- `PICO_FLASH_SPI_CLKDIV` goes up with the clock, since the divider is against
  `clk_sys` and overclocking without raising it overclocks the flash by the
  same factor. It must be set with `add_compile_definitions()` **before**
  `pico_sdk_init()`: it is compiled into the second stage bootloader, which is
  a target of the SDK's own, so `target_compile_definitions` never reaches it.
  Setting it on the firmware target does nothing at all and the image hangs at
  a clock the flash could not keep up with - which looks exactly like an
  unstable overclock.

#### Profiling

- `YAX86_PICO_PROFILE=1` builds in a sampling profiler: a timer interrupt at
  50kHz records the interrupted PC into a histogram, dumped after the run as
  addresses so that no symbol table has to be carried on the board.
  `symbolize.js` turns the dump into function names.
  ```sh
  YAX86_PICO_PROFILE=1 ./pico/bench/build.sh O3
  ./pico/bench/run.js build-pico/O3/yax86_pico_bench.uf2 --out capture.txt
  ./pico/bench/symbolize.js build-pico/O3/yax86_pico_bench.elf capture.txt
  ```
  Symbolize against the ELF that produced the capture, and not merely one built
  from the same source at the same level. Addresses move between builds, so the
  wrong ELF does not fail - it silently reports a plausible ranking of
  functions the run never executed.
- It costs a little over a percent and the histogram costs 32KB of SRAM, so it
  is off by default and a timing run never has it on. A profiling build says
  where the time went, never how much of it there was.
- SRAM is bucketed at 16 bytes and flash at 128. The coarse flash buckets are
  not trustworthy below the top few entries: several small functions share a
  bucket and only the first is named, so a bucket can be credited to a function
  the workload never executes. Verify anything in the tail before acting on it.

## Code Style

- Core emulator code is written in portable C99.
- Tests are written in C++14 and use the Google Test framework.
- All C/C++ code conforms to Google C++ style guide.
- No dependencies on libc functions like `printf`, `memset`.
- No dynamic memory allocation - only uses compile-time static memory
  allocation and stack allocation.
- For zero-initialized data, use static zero-initialization instead of
  `memset`, for example `static MyStruct data = {0};`.
- OK to include standard library headers for types like `uint8_t` and
  compile-time constants and macros like `NULL`.
- Uses `clang-format` for code formatting.
- Prefer enums over `#define` for constants.
- Prefer enums over numeric literals - define an enum value `enum { kFoo = 0xF8
  };` instead of referencing `0xF8` directly in the logic.
- Prefer `static inline` functions over macros.
- Prefer specific types like `uint8_t` over generic types like `int` for
  interfaces like function signatures and struct members.
- Define structs and enums with `typedef struct Name { ... } Name;` or 
  `typedef enum Name { ... } Name;`.
- Unused function parameters should be annotated with `YAX86_UNUSED` from the
  util/common.h header to avoid unused parameter warnings.
- `YAX86_HOT` goes first in a definition, before the linkage macro and the
  return type: `YAX86_HOT YAX86_PRIVATE InstructionResult ExecuteSub(...)`, and
  `YAX86_HOT static uint8_t ReadByte(...)`. Both orders compile, so the point
  is only that there be one - and putting the mark first leaves the declaration
  after it exactly as it reads without the mark.
- `clang-format` will not fix a misplaced mark, and will disguise one. It
  chooses where to break lines and never reorders tokens, so a mark added to
  the start of a *continuation* line - which is where a wrapped signature's
  declarator lives, below its return type - is already mid-declaration.
  Reformatting then rejoins it as `YAX86_PRIVATE InstructionResult YAX86_HOT`,
  which reads as though the mark belonged to the return type and was put there
  on purpose. Add the mark to the line the declaration *starts* on and
  reformatting keeps it first, however it decides to wrap.
- When incrementing or decrementing a variable, prefer prefix syntax
  `++var` or `--var` instead of the suffix syntax `var++` or `var--`.
- Comments should generally be added on the line before a variable, field or
  type, rather than on the same line. For example:
    ```c
    // Good comment - do this
    int var1;

    int var2;  // Bad comment - don't do this
    ```

## Commands

The project uses CMake as the build system.
- The `build-native` directory contains output of native builds - native static
  libraries, test binaries and executables.
- The `build-emscripten` directory contains output of Emscripten builds -
  WebAssembly binary and JavaScript wrapper.

To build the emulator from the project root directory:
```
./tools/build.sh
```

Builds default to `Release`. CMake applies no optimization flags at all when no
build type is named, so the default is set explicitly in the top level
`CMakeLists.txt` rather than left to CMake - an unoptimized emulator is around
four times slower, which is most of the browser build's CPU usage at an idle DOS
prompt. Pass `-DCMAKE_BUILD_TYPE` to choose another type; it is left alone when
set. Note that `-Wall -Wextra -Wpedantic -Werror` is attached to
`CMAKE_C_FLAGS_DEBUG`, so warnings are errors only in a `Debug` build.

To run tests:
```
./tools/run-tests.sh
```

The CPU is also checked against the [8088 hardware test
suite](https://github.com/SingleStepTests/8088), which records the result a
real 8088 produced for each of roughly 3 million instruction encodings. The
data is not in the repository - download it once, after which `run-tests.sh`
picks it up along with everything else:
```
./tools/download-cpu-hardware-tests.sh
```
The download fetches about 480MB into `.cache/8088-tests` in around twenty
seconds, from a revision pinned in the script - the suite has no tags or
releases, and its recorded results are what the tests compare against, so
tracking a branch would let upstream change what the tests mean. The opcodes to
fetch come from `tools/cpu-hardware-tests-opcodes.txt` rather than from the
GitHub API, which is rate limited per IP for anonymous callers.

Pass opcodes to fetch only a subset, as in
`./tools/download-cpu-hardware-tests.sh 8D 00 80.0`; group opcodes take a ModRM
REG suffix. `run-tests.sh` always runs the suite; it reports no tests at all
when nothing has been downloaded, so a fresh checkout passes trivially rather
than failing. It can also be run on its own, and a single opcode picked out:
```
build-native/core/tests/cpu/cpu_hardware_tests
build-native/core/tests/cpu/cpu_hardware_tests --gtest_filter='*Opcode8D*'
```
The suite is registered with ctest via gtest_discover_tests so that
`./tools/run-tests.sh` runs all downloaded hardware test opcodes in parallel.

Only the architectural result is compared - the suite's per-cycle bus records
are for cycle-accurate emulators, and flags the 8088 leaves undefined are
masked out.

Every opcode is expected to pass. The one exception is the divide
instructions, where two differences from real hardware are deliberately not
reproduced, both of which would require emulating the divide microcode step by
step:

- The flags a divide leaves behind when it raises a divide error. The 8088
  divides one bit at a time and updates the arithmetic flags on every pass, so
  the flags are the last pass's. They are undefined, and the suite masks them
  out of its register comparison - but the divide error interrupt pushes them,
  so they reappear as the flags word on the stack, where nothing masks them.
- The sign `IDIV` gives the quotient for some operands, which can disagree with
  its own remainder. No real software can depend on an arithmetically wrong
  answer.

These are matched narrowly in `core/tests/cpu/hardware_test.cpp` - see
`KnownDivergence` - rather than by skipping the opcodes, so everything else
those opcodes do is still checked. The number of mismatches let through is
asserted per opcode in `kKnownDivergences`, so diverging more is a failure
rather than a larger number in the output. Fixing a divergence means bringing
the expected count down, which the failure message says.

CI downloads the data and runs the suite on every leg of the build matrix.
Both compilers and both optimization levels are worth covering: undefined
behavior in the emulator tends to show up as a difference from the hardware
under one of them and not the others.

To debug the WASM version of the emulator, use Chrome DevTools MCP server to
open `http://localhost:3000/yax86_sdl.html`. A stale service worker from an
earlier session can serve cached, mismatched `.wasm`/`.js`/`.data` and produce
symptoms that look like a real regression (e.g. a blank canvas or a boot
failure) when the code is fine; unregistering service workers and clearing
caches for the origin rules this out before chasing a code-side cause. Kill the
web server process and the Chrome DevTools MCP instance (and the headless
Chrome it spawns) once done with them rather than leaving them running in the
background - Chrome DevTools MCP's headless Chrome instance in particular uses
a lot of CPU and RAM even when idle.

## Additional Notes

- The project should NOT implement emulation for features not used on the IBM
  PC/XT by GLaBIOS or MS-DOS. The project does not attempt to support other
  hypothetical x86 operating systems, only MS-DOS and era-accurate software.
    - For example, we should NOT implement advanced features of the Intel 8259A
      PIC like level-triggered interrupts, auto-EOI mode, or specific
      end-of-interrupt mode, because the IBM PC/XT only uses edge-triggered
      interrupts, manual EOI mode, and normal fully-nested mode.
- The project uses GlaBIOS as the BIOS implementation. The project SHOULD
  implement emulation for any functionality required by GlaBIOS to run MS-DOS
  3.3 and basic DOS applications.
- The source code for GLaBIOS is found at `.cache/GLaBIOS/src/GLABIOS.ASM`
  under the project root. Specifically, we target GLaBIOS's `ARCH_TYPE_EMU`
  build type.
- When considering how to implement emulation for a hardware component or a
  feature of a hardware component, fetch and review the source code of similar
  projects listed below for reference.

## Similar projects

The source code for the following similar projects can be found in the
`.cache` directory under the project root:

- 8086tiny - `.cache/8086tiny`
- 86Box - `.cache/86Box`
- MartyPC - `.cache/martypc`

