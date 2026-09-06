# Project: yax86

## Project goals

A small IBM PC/XT emulator for minimal MCU platforms like the Raspberry Pi
Pico, and for the browser via SDL and Emscripten.

- Emulates a basic PC/XT system with an 8086/8088 processor.
- Runs MS-DOS 3.3 on top of GLaBIOS.
- Fits and runs well on an RP2040: no dynamic allocation, no file system, no
  libc dependencies, and a hot path chosen against on-target measurements.

## Repository layout

| path | what it is |
| --- | --- |
| `core/` | The emulator. Portable C99, no external runtime. |
| `core/src/<module>/` | One module each: `cpu`, `platform`, `video`, `fdc`, `hdc`, `pit`, `pic`, `dma`, `ppi`, `keyboard`, `bios`, `util`. |
| `core/tests/` | Unit tests per module, C++14 and Google Test. |
| `sdl/` | SDL3 runtime, native and Emscripten. |
| `pico/bench/` | RP2040 benchmark harness. Standalone CMake project. |
| `tools/` | Build and test scripts for the native and Emscripten builds. |
| `resources/` | Floppy and disk images. |
| `.cache/` | Downloaded references — GLaBIOS source, the 8088 test suite, other emulators. Not in the repository. |

## Building, testing and debugging

The project uses CMake. Native and Emscripten output go to `build-native` and
`build-emscripten`; the Pico harness builds into `build-pico`.

### Building

```sh
./tools/build.sh
```

Builds default to `Release`. CMake applies no optimization flags at all when no
build type is named, so the default is set explicitly in the top level
`CMakeLists.txt` rather than left to CMake — an unoptimized emulator is around
four times slower, which is most of the browser build's CPU usage at an idle
DOS prompt. Pass `-DCMAKE_BUILD_TYPE` to choose another type; it is left alone
when set. `-Wall -Wextra -Wpedantic -Werror` is attached to
`CMAKE_C_FLAGS_DEBUG`, so warnings are errors only in a `Debug` build.

### Tests

```sh
./tools/run-tests.sh
```

### The 8088 hardware test suite

The CPU is checked against the [8088 hardware test
suite](https://github.com/SingleStepTests/8088), which records the result a
real 8088 produced for each of roughly 3 million instruction encodings. The
data is not in the repository — download it once, after which `run-tests.sh`
picks it up along with everything else:

```sh
./tools/download-cpu-hardware-tests.sh
```

About 480MB into `.cache/8088-tests` in around twenty seconds, from a revision
pinned in the script. The suite has no tags or releases and its recorded
results are what the tests compare against, so tracking a branch would let
upstream change what the tests mean. The opcodes to fetch come from
`tools/cpu-hardware-tests-opcodes.txt` rather than from the GitHub API, which
is rate limited per IP for anonymous callers.

Pass opcodes to fetch only a subset, as in
`./tools/download-cpu-hardware-tests.sh 8D 00 80.0`; group opcodes take a ModRM
REG suffix. `run-tests.sh` always runs the suite, and reports no tests at all
when nothing has been downloaded, so a fresh checkout passes trivially rather
than failing. It can also be run on its own, and a single opcode picked out:

```sh
build-native/core/tests/cpu/cpu_hardware_tests
build-native/core/tests/cpu/cpu_hardware_tests --gtest_filter='*Opcode8D*'
```

The suite is registered with ctest via `gtest_discover_tests`, so
`./tools/run-tests.sh` runs every downloaded opcode in parallel.

Only the architectural result is compared — the suite's per-cycle bus records
are for cycle-accurate emulators, and flags the 8088 leaves undefined are
masked out.

Every opcode is expected to pass. The one exception is the divide
instructions, where two differences from real hardware are deliberately not
reproduced, both of which would require emulating the divide microcode step by
step:

- The flags a divide leaves behind when it raises a divide error. The 8088
  divides one bit at a time and updates the arithmetic flags on every pass, so
  the flags are the last pass's. They are undefined, and the suite masks them
  out of its register comparison — but the divide error interrupt pushes them,
  so they reappear as the flags word on the stack, where nothing masks them.
- The sign `IDIV` gives the quotient for some operands, which can disagree with
  its own remainder. No real software can depend on an arithmetically wrong
  answer.

These are matched narrowly in `core/tests/cpu/hardware_test.cpp` — see
`KnownDivergence` — rather than by skipping the opcodes, so everything else
those opcodes do is still checked. The number of mismatches let through is
asserted per opcode in `kKnownDivergences`, so diverging more is a failure
rather than a larger number in the output. Fixing a divergence means bringing
the expected count down, which the failure message says.

CI downloads the data and runs the suite on every leg of the build matrix.
Both compilers and both optimization levels are worth covering: undefined
behavior in the emulator tends to show up as a difference from the hardware
under one of them and not the others.

### Debugging the WASM build

Use the Chrome DevTools MCP server against
`http://localhost:3000/yax86_sdl.html`. A stale service worker from an earlier
session can serve cached, mismatched `.wasm`/`.js`/`.data` and produce symptoms
that look like a real regression — a blank canvas, a boot failure — when the
code is fine. Unregister service workers and clear caches for the origin to
rule that out before chasing a code-side cause.

Kill the web server and the Chrome DevTools MCP instance, along with the
headless Chrome it spawns, once done. That headless Chrome uses a lot of CPU
and RAM even when idle.

## Code style

### Language and dependencies

- Core emulator code is portable C99. Tests are C++14 and use Google Test.
- All C/C++ code conforms to the Google C++ style guide, formatted with
  `clang-format`.
- No dependencies on libc functions like `printf` or `memset`. Standard library
  headers are fine for types like `uint8_t` and for constants and macros like
  `NULL`.
- No dynamic memory allocation — compile-time static allocation and the stack
  only.
- For zero-initialized data use static zero-initialization rather than
  `memset`, as in `static MyStruct data = {0};`.

### Declarations

- Prefer enums over `#define` for constants, and over numeric literals in
  logic: define `enum { kFoo = 0xF8 };` rather than writing `0xF8` inline.
- Prefer `static inline` functions over macros.
- Prefer specific types like `uint8_t` over generic ones like `int` in
  interfaces — function signatures and struct members.
- Define structs and enums as `typedef struct Name { ... } Name;` and
  `typedef enum Name { ... } Name;`.
- Prefix increment and decrement, `++var` and `--var`, not `var++` and `var--`.
- Annotate unused parameters with `YAX86_UNUSED` from `util/common.h`.

### Placement and inlining marks

- `YAX86_HOT`, `YAX86_NOINLINE` and `YAX86_ALWAYS_INLINE` go first in a
  definition, before the linkage macro and the return type: `YAX86_HOT
  YAX86_PRIVATE InstructionResult ExecuteSub(...)`, `YAX86_HOT static uint8_t
  ReadByte(...)`. Both orders compile, so the point is only that there be one —
  putting the mark first leaves the declaration after it reading exactly as it
  would without the mark. Only one of the three is ever on a given function.
- `clang-format` will not fix a misplaced mark, and will disguise one. It
  chooses where to break lines and never reorders tokens, so a mark added to
  the start of a *continuation* line — which is where a wrapped signature's
  declarator lives, below its return type — is already mid-declaration.
  Reformatting rejoins it as `YAX86_PRIVATE InstructionResult YAX86_HOT`, which
  reads as though the mark belonged to the return type and was put there on
  purpose. Add the mark to the line the declaration *starts* on and
  reformatting keeps it first, however it decides to wrap.

### Comments

- Put a comment on the line before what it describes, not on the same line:

  ```c
  // Good comment - do this
  int var1;

  int var2;  // Bad comment - don't do this
  ```

- **Comments describe the code as it is, never as it used to be.** Do not leave
  tombstones — "this used to be a linear scan", "replaces the old dispatch
  table", "previously read the destination first", "kept for the old callers".
  Git history records what changed; a comment about a version that no longer
  exists is one more thing that has to be kept true, and it is the first thing
  to rot.
- The same goes for this file. A note here earns its place by telling the next
  change something it needs, not by narrating how the current code was arrived
  at.
- The one thing worth writing down about an alternative is what it *costs*, and
  that is a fact about the present code rather than a story about a past one.
  "Assigning this from `metadata->has_modrm` instead measures 0.12% slower" is
  a comment; "we used to assign this from `metadata->has_modrm`" is a
  tombstone. Write the first.

## Performance work

The emulator is optimized against the Raspberry Pi Pico, not against a desktop.
A Cortex-M0+ has no branch predictor, no speculation and no cache except the
16KB XIP window that code executes from flash through, so a desktop ablation
cannot close a question about it. `pico/bench` is the harness; see its section
below for how to build, flash and run.

### What a performance PR must report

Every PR that claims a speed or size change carries a results table, measured
on hardware with the `dos-boot` workload:

- **`-O3` and `-O2`**, both levels, GCC, at **400MHz** with the hot path in
  SRAM and the default 128K of guest RAM. `-Os` is not reported: it is the
  slowest level here by a wide margin, it is the one `YAX86_HOT` hurts, and
  nothing ships at it.
- **Seconds, emulated MHz, MIPS and "vs a real 8088"**, for the baseline and
  for the branch, at each level.
- The **gain**, as the increase in emulated MHz over the baseline row.

```
| level | | seconds | emulated MHz | MIPS | vs a real 8088 | gain | core `.text` |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `-O3` | master   | 6.728870     | 4.117     | 0.346     | 86.3%     | —          | 89,285 |
| `-O3` | this     | **6.513592** | **4.253** | **0.357** | **89.2%** | **+3.31%** | 87,229 |
| `-O2` | master   | 7.352727     | 3.768     | 0.317     | 79.0%     | —          | 74,457 |
| `-O2` | this     | 7.213556     | 3.840     | 0.323     | 80.5%     | +1.93%     | 73,853 |
```

The derived columns are computed from the seconds rather than read off the
harness, which truncates MHz and MIPS to two decimals when it prints them. For
`C` emulated cycles and `I` retired instructions over `t` seconds:

- emulated MHz = `C / t / 1e6`
- MIPS = `I / t / 1e6`
- vs a real 8088 = `(C / 4.77e6) / t` — a real 4.77MHz 8088 runs the current
  workload in **5.807 seconds**
- gain = `t_baseline / t - 1`, which is the increase in emulated MHz

Alongside the table, state:

- **The invariant, and that it did not move.** `dos-boot` retires 2,328,015
  instructions in 27,701,507 emulated cycles, at every level and on every run.
  If either number changed, the change altered behaviour and the times either
  side of it are not comparable — say so and explain why, rather than
  presenting the speedup.
- **How the baseline was obtained.** Rebuild it from its own commit through
  `YAX86_CORE_ROOT` against the same harness, on the same board, in the same
  session. A figure recorded in an earlier PR is a cross-check, not a baseline.
- **How many runs, and their spread.** Two or three is enough at this noise
  floor.
- **`core .text` at both levels**, since several changes here have won on speed
  and size at once and a size regression is worth knowing about either way.
- Which tests were run. `./tools/run-tests.sh`, and the 8088 hardware suite
  where the change touches decode or execute.

### Measurement discipline

- **Reproducibility is excellent, so small differences are real.** Three
  independent flash-and-run cycles of one image have given a spread of 0.0065%.
  **A 1% difference is a result.** Do not demand a large margin before
  believing one, and equally do not accept a "neutral" result as a win.
- **Measure, do not reason.** Every speculative change made here on the
  strength of an argument about the generated code has lost. The ones that won
  were built and measured, usually along with the two or three arrangements
  that looked equally plausible.
- **A queued or prototyped figure is a reason to try a change, not a number to
  expect.** A prototype is measured on whatever base it was written against,
  and the changes underneath it move what is left to win. Re-measure on the
  branch rather than carrying the recorded figure across.
- **Forcing a compiler's hand moves decisions elsewhere.** Pinning one function
  inline or out of line has repeatedly changed where an unrelated one landed —
  see the hot path placement section. Check `nm` after adding a mark rather
  than assuming the effect was local.
- **What matters on this part is flash traffic, not how hot something feels.**
  `dos-boot` generates about 559 flash accesses per emulated instruction, so a
  structure read once per instruction is noise against the instruction fetches
  that surround it. That ratio is why moving code to SRAM is worth 1.59x while
  moving a table read on every single instruction is worth nothing measurable.

## core — emulation modules

### How the core fits together

- `core` holds the emulation logic: the CPU, and hardware such as the video
  adapter, keyboard, floppy and hard disk controllers.
- **Core modules depend on no external runtime.** They reach the host only
  through callbacks, which is what lets the same code build for SDL, Emscripten
  and an RP2040.
- Each directory under `core/src` is a module, bundled into a single header in
  `core` according to its `bundle.json` — `core/src/cpu` becomes `core/cpu.h`.
  A module's external interface is its `public.h`.
- All module bundles are concatenated into `core/yax86_core.h`, which is also
  built into `libyax86_core.a`.
- `platform` is the virtual motherboard: it connects the modules together and
  owns the memory and port maps.
- ROMs live alongside the module that maps them and are compiled into the
  library rather than read from a file, since targets like the Pico have no
  file system. `core/tools/generate-rom-data-files.js` turns a ROM image into a
  C array and header; the `generate_rom_data` calls in `core/CMakeLists.txt`
  name the module, ROM file, output name and symbol. The generated
  `*_rom_data.{c,h}` are committed and CI runs `git diff --exit-code`, so
  regenerate and commit them when a ROM changes.

### cpu — instruction decoding

- `CPUFetchNextInstruction()` decodes directly into the caller's `Instruction`.
  Filling a local and copying it out at the end costs more than the whole
  decode: with `gcc -O3` over a boot and idle run at the DOS prompt, 3.5% of
  host instructions, 8.4% of data references — 97M extra reads and 97M extra
  writes — and about 19% of wall clock time.
- The wall clock cost being so much larger than either count is the part worth
  carrying forward, and it is not a cache effect — D1 misses are identical
  either way. Writing a struct to the stack and immediately reading it back
  creates dependent store-to-load pairs that stall, and neither callgrind nor
  cachegrind models that. **Anything on this path that round trips through
  memory costs more than its instruction count suggests.**
- Because the decode happens in place, a failed fetch leaves the caller's
  `Instruction` holding however much had been decoded. Every caller treats a
  failed fetch as fatal, so nothing reads it back.
- A decode settles each field where it becomes known rather than zeroing the
  struct up front. `opcode`, `size` and `immediate_size` are assigned on every
  path that returns success; `has_mod_rm` and `displacement_size` in both arms
  of the ModR/M branch; `mod_rm` is read only where `has_mod_rm` is set; and
  `displacement[]` and `immediate[]` are read no further than their size fields
  say. Only the two prefix fields are cleared before the decode starts. Worth
  **4.63% at `-O3`** and 3.96% at `-O2`.
- What makes that safe is that a reused destination keeps nothing of the
  instruction before it, and reuse is the ordinary case — `CPUTick()` holds a
  single `Instruction` for the life of the CPU.
  `ADecodeKeepsNothingOfThePreviousInstruction` pins it directly. The hardware
  suite catches a dropped clear as well, and emphatically — removing one fails
  thousands of encodings across most opcodes — but as a diffuse result rather
  than a named one.
- **No field of `Instruction` is a bitfield.** `has_mod_rm`,
  `displacement_size`, `immediate_size` and all three of `ModRM` take a whole
  byte each, which puts the struct at 16 bytes and is worth **0.95% at `-O3`
  and 2.18% at `-O2`** — while also making the core 112–120 bytes *smaller* at
  every level.
- The reason is that a packed field is never simply loaded or stored. A write
  to one is a read-modify-write of the byte it shares — `ldrb`/`bics`/`strb`,
  or `ldrb`/`bics`/`orrs`/`strb` to set as well as clear — and a read is a load
  plus a shift plus a mask. `ModRM` is where that bites hardest: `mod` and `rm`
  are taken by `GetMemoryOperandAddress()` for every memory operand and by
  `GetEffectiveAddressCycles()` for every instruction, and `reg` selects the
  handler for all five instruction groups. The margin is widest at `-O2`, where
  less is inlined and those reads stay explicit.
- **Do not re-derive a rule that `Instruction` should be kept small.** It was
  true only while the decode copied the struct, which it no longer does;
  nothing copies an `Instruction` per instruction, so its size buys almost
  nothing and paying for it in packing is a straight loss.
- `has_mod_rm` and `displacement_size` are cleared in the ModR/M branch's
  `else` arm rather than before the decode starts, so each is written exactly
  once whichever arm runs. Two alternatives were built and measured, and both
  lose: assigning `has_mod_rm` from `metadata->has_modrm`, which always carries
  the same value, costs 0.12%, because it adds a write on every instruction
  where the branch only pays on the half that carry a ModR/M byte; writing all
  three fields together after the branch costs 0.60%. Clearing `immediate_size`
  alongside the other two is neutral and dead, since it is assigned
  unconditionally further down.
- The one field read before it is known to exist is the ModR/M `REG` field,
  which `GetImmediateSize()` needs whether or not the instruction carries a
  ModR/M byte. Only `0xF6` and `0xF7` consult it and both do carry one, so the
  value is never used — but it still has to be *defined*. The decode keeps it
  in a local initialized to 0 rather than reading `mod_rm.reg` back, which is
  the cheaper of the two available fixes: a ternary on `has_mod_rm` puts a
  branch on the hot path and clearing `mod_rm` puts back a store, where the
  local costs neither and removes a load.
- `immediate_size` and `displacement_size` can each express far more bytes than
  `immediate[]` and `displacement[]` hold. No opcode table entry exceeds the
  arrays, but nothing in the types says so, and `gcc -O3` warns about the
  writes once it can no longer see the bound through a pointer. The immediate
  loop bounds itself by the array for that reason, and with whole-byte size
  fields that `kMaxImmediateBytes` clamp is load-bearing rather than
  belt-and-braces.

### cpu — instruction fetch

- `CPUConfig.get_instruction_fetch_window` lets the host fill in
  `CPUState.instruction_fetch_window` — a `data` pointer and the half-open
  range `[start, end)` it covers — and the fetch reads bytes straight out of
  it. Reading each byte through `CPUConfig.read_memory_byte` instead is an
  indirect call per byte, two to six times per instruction, into a host that
  then resolves the address and indexes an array: worth **9.8%** on `dos-boot`.
- The host fills the window in directly rather than returning it, so the CPU
  never has to decide what range a returned pointer stood for. A host hands
  back the whole region the address falls in rather than the tail of it from
  that address, which is what makes a jump backwards to before the point
  fetching started stay inside the window — see
  `WindowCoversTheRegionBeforeTheFetchAddress`. `data == NULL` is how a host
  declines.
- The window is kept across instructions in `CPUState`, not re-derived per
  instruction, and is held as a range of addresses rather than a cursor. Both
  straight-line execution and a jump backwards within the same region land
  inside the one already open, so reopening is a compare rather than a call.
- `CPUInstructionFetchState` is the per-decode cursor derived from it, and
  holds neither a `CPUState` pointer nor a reference to the CPU's IP: IP has to
  keep naming the instruction being decoded until `CPUTick()` advances it by
  `instruction.size`, and a byte-by-byte write into `cpu->registers[]` is a
  store the compiler cannot keep in a register.
- **Caching a pointer is safe where caching bytes would not be.** The window
  points into the host's own storage, so a write through the memory map is
  visible to the next fetch with no invalidation and self-modifying code keeps
  working. What needs invalidating is a change to what an address *means*,
  which is what `CPUInvalidateInstructionFetchWindow()` is for. The platform
  calls it from `RegisterMemoryMapEntry()`, and unconditionally at the end of
  `PlatformUpdateEnabledFlags()` — so a breakpoint change discards the window
  too, which costs nothing and keeps the rule simple to state.
- `PlatformUpdateEnabledFlags()` is the **sole writer** of
  `has_enabled_breakpoints` and `has_enabled_memory_watchpoints`. Every path
  that changes a breakpoint or watchpoint recomputes both through it, so a flag
  can never disagree with the array it summarizes, and no path can change one
  without also discarding the window.
- The watchpoint case is the subtle one. A direct read cannot fire a
  watchpoint, so the platform declines to hand out a window at all while any is
  enabled — but declining does nothing about a window already open, which is
  the reason for the rule above.
- The window stops at a segment wrap. IP is 16 bits and wraps within the
  segment where the linear address does not, so `remaining` is clamped to
  `0x10000 - ip` and the bytes past it go through the ordinary path, which
  recomputes the address from the wrapped IP.
- The 8088 hardware suite runs with a window supplied, which is the strongest
  check the fetch has — every encoding length, prefix count and segment wrap
  the part can produce, about three million times. The path taken when a host
  supplies no window is what the mock configs in `cpu_test.cpp` exercise.

### cpu — prefix decoding

- Both prefix groups are contiguous encoding families, so each is identified by
  one masked compare: segment overrides encode as `001ss110` and LOCK/REP as
  `111100rr`. The bits each mask leaves free are the selector — `ss` is in the
  8086's sreg order, which is the order `kES` through `kDS` are numbered in, so
  the segment register index is an offset from `kES`.
- Testing for a prefix this way rather than scanning an eight-element array is
  worth 5–8% of host instructions below `gcc -O3` and `clang -O2`, and nothing
  at or above them, where both compilers derive their own equivalent — `clang`
  picks a 64-bit bitmap and a `bt`, which is arguably better than the mask.
- Prefixes are decoded into fields on `Instruction` as they are fetched, rather
  than kept as raw bytes for each consumer to walk. `segment_override` uses 0
  (`kNoSegmentOverride`) for absent, which is `kAX` and never a segment, so a
  zero-initialized `Instruction` is correct by construction. LOCK is consumed
  but not recorded — nothing on a PC/XT acts on it.
- **Classify and record in one pass.** Deciding whether a byte is a prefix and
  deciding which group it belongs to are the same test, so `ApplyPrefixByte()`
  does both and returns whether it consumed anything, rather than the loop
  asking first and the recording asking again. Splitting them costs 0.7% on
  `clang`, enough on its own to make the masked compare a net regression there
  at `-O2` and above.
- Write the `0xF0`–`0xF3` test as a range check rather than a mask. Both are
  exact, but a range folds into one subtract and compare where the mask needs a
  separate `mov`, `and` and `cmp` — which is how `gcc -O3` writes it when left
  to itself. Put it before the segment override test: bytes reaching here are
  usually not prefixes at all and so run both, making the combined cost what
  matters.
- `kMaxPrefixBytes` is not a storage bound — nothing is stored per prefix and
  the count is a local in the fetch loop. It exists because a run of prefix
  bytes would otherwise fetch forever: a real 8086 hangs there too, since a
  prefix is not an instruction boundary and so no interrupt is ever recognized,
  but this has to hand control back to its caller. `Instruction.size` is a
  `uint8_t` that `CPUTick()` adds to IP, so the bound is what that can still
  address — 247 prefixes plus the 8 bytes an instruction can otherwise carry
  comes to exactly 255. It has to be this generous: `LOCK ES: REP MOVSB` is a
  legal three-prefix encoding.

### cpu — the execute path

- `CPUExecuteInstruction()` checks an instruction against the opcode table
  before running it: that `has_mod_rm` and `immediate_size` are what the entry
  says they should be. `CPUTick()` skips those checks and calls
  `CPUExecuteDecodedInstruction()` directly, because its own
  `CPUFetchNextInstruction()` *derived* both from that same entry a few lines
  earlier. Re-establishing something that cannot have changed in between is not
  free — a table read, two compares and a recomputed immediate size on every
  instruction the emulator runs.
- The public entry point keeps the checks, for a caller that built an
  `Instruction` by hand rather than decoding one. Nothing inside the core calls
  it, so the duplication costs nothing.
- **Every entry in the opcode table has a handler**, so neither path checks for
  one before dispatching. The eight bytes that have no instruction of their own
  — the four segment overrides and `0xF0`–`0xF3` — are exactly the bytes
  `ApplyPrefixByte()` consumes, so a successful fetch can never leave one in
  `opcode`; they point at `ExecuteInvalidOpcode()`, which returns
  `kInstructionInvalid` for the hand-built instruction that is the only way to
  reach one. `EveryEntryHasAHandler` in `opcode_table_test.cpp` keeps it that
  way — an entry added later with the field left out would be a call through
  null rather than an invalid instruction.
- `on_before_execute_instruction` fires after validation, so it does not run
  for an instruction about to be rejected as an encoding mismatch, and it does
  run for a hand-built prefix opcode, which `ExecuteInvalidOpcode()` rejects
  from inside the handler. Only `core/tools/cpu_demo` uses the callback, and it
  never builds an `Instruction` by hand.
- `CPUExecuteDecodedInstruction()` is `YAX86_NOINLINE`. Inlined into
  `CPUTick()` it measures 31% slower on a Cortex-M0+: the execute path wants
  registers, the core has few, and folding the two together makes both spill.
  The effect only exists once the hot path is in SRAM — running from flash, XIP
  misses dominate and hide it — which is why this and `YAX86_HOT` belong
  together. Worth 1.84% at `-O3` and 4.58% at `-O2`, the gap being the
  `CPUTick()`/`PlatformTick()` marks, which are inert at `-O3`.

### cpu — operand dispatch

- Which operand handler runs is decided by a `switch` on the width and an
  `if`/`else` on the operand address type, not by indexing tables of function
  pointers. Every operand of every instruction goes through this, and dropping
  the tables is worth **3.20% at `-O3`** and 1.89% at `-O2`.
- An indirect call is two loads and a branch the compiler cannot see through,
  and on a Cortex-M0+ there is no predictor and no speculation to overlap it
  with. The functions behind it are a handful of instructions each, so the
  dispatch cost more than the work.
- **The larger half of that win is second-order.** A function whose address is
  stored in a table has to exist out of line whether or not anything reaches it
  that way. With the tables gone, seven of the eight leaf handlers vanish from
  the image entirely — only `ReadMemoryOperandWord()` is still emitted — and
  the core is **1,208 bytes smaller at `-O3`**, against 48 bytes of table.
- Before concluding a dispatch table has callers that need it, check whether
  they are really indexing. The one apparent external caller here was two sites
  in `instructions_mov.c` reading a 16-bit offset with
  `kReadImmediateValueFn[kWord]` — a constant index, which is a direct call to
  `ReadImmediateOperandWord()` written the long way.
- **Switch on the width, but not on the operand address type.** The two look
  like the same dispatch and are not. `Width width : 1` is a one-bit bitfield,
  so there is no representable value outside the enum: the compiler proves the
  `default` arm unreachable and emits nothing for it, which is what makes the
  explicit invalid return free and lets these read like `ToOperandValue()` and
  `FromOperandValue()` rather than as bare ternaries. `OperandAddress.type` is
  a whole `OperandAddressType`, and while `arm-none-eabi` defaults to
  `-fshort-enums` and so gives it one byte, 254 of that byte's 256 values are
  outside the enum and C does not promise a variable holds only named ones. A
  `switch` on it emits a live third branch where an `if`/`else` emits two: on
  the *memory* path — the common one — that is an extra `cmp`/`bne` plus the
  constant load for the default, where the one-bit `Width` switch compiles to a
  single `lsls`/`bmi` bit test. Switching the type as well costs **1.06% at
  `-O3`** and 0.70% at `-O2`. There is a comment at `ReadOperandValue()` and
  `WriteOperandAddress()` saying so.
- **The distinction is bitfield width, not enum size** — do not restate it as
  the type being "int-sized", which it is not here.
- Narrowing `type` to `OperandAddressType type : 1` does make its switch free,
  and still does not pay: **0.22% worse at `-O3`** and 0.10% at `-O2`, for a
  larger core. The read is only half the story —
  `GetRegisterOrMemoryOperandAddress()` *writes* `address.type` for every
  operand, and a write to a bitfield is a read-modify-write. Cheapening a
  dispatch that runs once per operand does not pay for a store that also runs
  once per operand. So do not "tidy" the `if` into a `switch` for symmetry, and
  do not narrow the field to enable it; both were built and measured.
- `OpcodeMetadata` still packs `has_modrm : 1`, `immediate_size : 3` and
  `width : 1`, and widening them to whole bytes is a candidate change. **Take
  care with `width`**: widening it is what makes the width switch stop being
  free, because a byte has 254 values the enum does not name. Widening the
  other two while leaving `width : 1` is the best `-O3` arrangement of the five
  built, at 6.4808s.
- `-Os` loses 1.00% to the tables, and the cause is inlining rather than the
  `default` arm — marking all six wrappers `YAX86_ALWAYS_INLINE` recovers it,
  at 0.26% and 0.06% off `-O3` and `-O2`. The marks are not taken: `-O3` and
  `-O2` are what this target is measured at.
- `ReadImmediateOperand()` is the one wrapper the compiler still emits out of
  line, and it lands in flash. `YAX86_HOT` moves it to SRAM and is worth 0.11%
  at `-O3`, but costs 0.35% at `-O2` by moving other inlining decisions, so it
  is left unmarked.

### cpu — what an instruction costs

- The cycle model in `cycles.c` is a base cost per opcode, plus the effective
  address calculation, plus four cycles for every byte the instruction moves
  over the 8088's 8-bit data bus. The third term dominates, and it is charged
  from the accesses that actually happen rather than from a table — so an
  instruction that touches memory it has no reason to touch is billed for it.
- Stores must therefore resolve their destination with
  `GetRegisterOrMemoryOperandAddress()` and go straight to
  `WriteOperandAddress()`, never through `ReadRegisterOrMemoryOperand()`.
  Reading a destination that is about to be overwritten completely prices a
  store as a read-modify-write: `MOV [BX],AX` costs 23 cycles where the
  hardware charges 15, and `MOV [BX],AL` 15 against 11.
- **No architectural test can see this.** The registers and memory come out
  identical either way, so the 8088 hardware suite passes on both — it compares
  results, not bus traffic. `cycles_test.cpp` is what covers it: it runs one
  instruction through `CPUTick()` and asserts `cycles_this_tick`, pinning a
  store against the load and the read-modify-write of the same shape.
- **Any change to cycle costs moves the `dos-boot` invariant**, so runs either
  side of one are not comparable. Cheaper stores do not mean the guest does
  less work — its timer-polling loops get more turns before the same number of
  PIT ticks elapse, so retired instructions go *up* as emulated cycles go down.

### cpu — instruction counting

- `CPUState.instructions_retired` counts instructions the CPU actually ran; a
  halted tick advances the clock but retires nothing. It lives in the CPU
  because `CPUTick()` already knows which it did, where a caller could only
  find out by sampling `is_halted` before every tick and giving up
  `PlatformRun()`'s batching for a number the core already has. Counting in
  `CPUTick()` costs 0.17% under callgrind; the same counter driven from
  `PlatformTick()`, where the flag has to be re-derived, costs 1.09%.

### platform — the memory map

- `GetMemoryMapEntryForAddress()` is the hottest lookup in the emulator —
  every instruction byte and nearly every operand comes through it — and it is
  answered from an index rather than by walking the map. A walk is cheap where
  branches are predicted and the region wanted is usually first; on a
  Cortex-M0+ it is neither, since there is no branch predictor and code running
  from the BIOS ROM misses the first entry every time. The index is worth
  **7.0%** on `dos-boot`.
- `memory_page_map` holds one entry index per 4KB page of the 8086's 1MB, so
  256 bytes of index answers a lookup with a shift, a load and a bounds
  compare. 4KB is not tuned — it is the coarsest page that leaves every region
  the machine registers aligned, and a finer one would only cost more memory.
- Two index values are not entry indices. `kMemoryPageUnmapped` is a page no
  entry covers, and answers `NULL` directly. `kMemoryPageStraddled` is a page
  more than one entry has a share of, which the index cannot answer at all, so
  the lookup falls back to a walk. Nothing registers a misaligned region today,
  and that is exactly why the fallback needs its own test — see
  `platform_memory_map_test.cpp`, which registers one deliberately.
- A region whose size is not a whole number of pages is handled by construction
  rather than by arithmetic: its last page comes out straddled and takes the
  fallback. That is the case for any option ROM whose size is not a multiple of
  4KB.
- `RegisterMemoryMapEntry()` extends the index rather than rebuilding it. Only
  the pages the new entry touches can change, and entries may not overlap, so
  nothing already in the index can have a share of one of them — a registration
  never has to look at the rest of the map. `UpdateMemoryPageMapForEntry()`
  takes the entry's index rather than the entry, because the index is what goes
  into the map; the entry is derivable from it and not the other way round.
- An entry reaching past the top of the address space is rejected, because the
  index has a slot per page of that space and none above it. **That rejection
  is what makes the index the whole answer** rather than a cache in front of
  the map: an address above the space is unmapped by definition, so
  `GetMemoryPageMapIndex()` says so directly rather than sending the lookup off
  to walk for an entry that cannot be there. Guest addresses never reach it in
  any case — `ToRawAddress()` masks every one to the 8086's 20 bits — so this
  is about what a host may register, not what the CPU may ask for.

### platform — idle skipping

- MS-DOS waits for a keystroke by polling rather than halting, so an idle
  command prompt costs exactly as much to emulate as a running program. At the
  MS-DOS 3.30 prompt the guest retires about 390,000 instructions a second and
  never once halts, all of it the same loop through the `INT 21h` dispatcher
  and the CON driver into `INT 16h`.
- It does announce itself: DOS issues `INT 28h`, the documented DOS idle
  interrupt, 783 times a second while it waits. `PlatformConfig`'s
  `enable_dos_idle_skip` makes `PlatformRun()` treat that as a signal to
  advance the clock to whichever comes first, the next device deadline or the
  end of the budget it was given, instead of executing the loop until it gets
  there. The guest's own handler still runs; only the waiting is skipped.
- **Time is advanced, not discarded.** `PlatformSync()` runs across the skipped
  interval, so every device sees every cycle and the guest's timer tick count
  is unchanged. Bounding the skip by the caller's budget is what keeps that
  true — without it the machine would be handed more emulated time than was
  asked for and would run its clock fast.
- It is opt in because it is not free of consequence: a program timing a loop
  from inside its own `INT 28h` handler would see time jump. The SDL runtime
  turns it on; the tests leave it off.
- What the skip is worth is capped by the nearest device deadline, which is why
  `PITTicksUntilNextEvent()` schedules one for channel 0 alone. After POST the
  BIOS leaves channel 2 programmed at a reload of 1356 with the speaker gated
  off, and asking to be woken every 678 ticks for an output nobody listens to
  truncates every skip. Over 20 emulated seconds at the prompt the skip is
  worth 1.4x with the other channels scheduled and 5.6x without.
- Only channel 0's output leaves the PIT — `PITChannelSetOutputState()` raises
  IRQ 0 from it. Channels 1 and 2 record a transition and nothing more, and
  that record is recomputed whenever the PIT is advanced, which every path that
  reads it does first. **Give another channel's output an effect and the
  scheduling has to change with it**; there is a comment at both ends saying so.

### hdc — hard disk controller

- Emulates an XT-IDE rev 2 controller: an 8-bit ATA task file operated in PIO
  mode, with no DMA and no interrupt line.
- GLaBIOS implements INT 13h for floppies only, rejects drive numbers above 3,
  and never writes the hard disk count at 40:75 or the INT 41h parameter table
  pointer. Hard disk support in the guest therefore comes entirely from an
  option ROM: GLaBIOS's POST scans from C800:0000 on 2KB boundaries for a ROM
  starting with `55 AA` whose bytes sum to zero, and far calls offset 3 to let
  it install its own INT 13h.
- The ROM is the XTIDE Universal BIOS, in
  `core/src/hdc/XTIDE_Universal_BIOS_XT_r631.rom`, compiled into the library
  the same way the system BIOS is. The module maps it at 0xC8000 and the
  platform sizes the memory region from the ROM itself.
- Its stock configuration is a rev 2 controller at port base 0x300, which POST
  reports as `Master at 300h`. **Rev 2 crosses address lines A0 and A3**, so a
  physical port offset maps to an ATA register offset with bits 0 and 3
  swapped — the BIOS polls status at physical 0x30E, not 0x307.
- An empty drive slot leaves the task file undriven, so its status register
  reads as 0. That is how the option ROM concludes a drive is absent: it
  selects the drive, polls status, and gives up when the ready bit never
  appears.
- Commands complete synchronously inside the port write that issues them, so
  there is no `HDCTick` and no seek or rotational timing. The controller is
  polled PIO, so nothing in the guest can observe the difference.
- **The two data-port directions use opposite byte orders.** The drive's data
  port is 16 bits wide and the card is on an 8-bit bus, so each word crosses as
  two byte accesses against two ports, and only the low byte port runs a bus
  cycle. A read of it moves a whole word: the low byte goes to the guest and
  the card latches the high byte, which the high byte port hands back without
  touching the drive. A write is the other way round, because the card cannot
  start a cycle until it has the whole word — the guest loads the latch through
  the high byte port first, and writing the low byte commits the pair. A byte
  stream shared by both ports models neither, and gets writes backwards: every
  word lands on the disk back to front, a partition table's 0xAA55 signature
  reads as 0x55AA, and DOS reports "Invalid drive specification".
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
  idle. The option ROM uses neither — it polls status at 0x30E — but an
  unanswered alternate status reads as 0xFF, which decodes as a drive that is
  permanently busy.

### util — hot path placement

- `YAX86_HOT` marks a function as being on the per-instruction hot path. It is
  empty by default, because on a machine with a normal cache hierarchy there is
  nothing useful to say; a target whose fastest memory has to be chosen
  explicitly defines it to whatever placement attribute it needs. The Pico
  harness defines it to `__not_in_flash()`, which puts the function in SRAM
  instead of executing it from QSPI flash through a 16KB XIP cache.
- **85 functions carry it, chosen from an on-target profile rather than by
  intuition.** On the Pico it is worth **1.59x at 400MHz** and 1.23x at 125MHz.
  The win is larger when overclocked because the flash SPI clock does not scale
  with the core, so an XIP miss costs more core cycles the faster the core runs.
- Annotating every function in the core instead was measured and is not worth
  it: the targeted set gives 97.2% of the win for 15KB of SRAM, where all 491
  give 100% for 46.5KB. The extra 2.8% costs three times the memory and
  forecloses the 192KB guest RAM option outright — 206,664 bytes of image plus
  64KB more guest RAM does not fit in 256KB, where the targeted build does.
- `CPUTick()` and `PlatformTick()` carry the mark even though at `-O3` they are
  inlined into `PlatformRun()`, which already did. The mark buys nothing there
  and costs 1.1KB, because an explicit section attribute overrides
  `-ffunction-sections`: all the marked functions share one section, so
  `--gc-sections` can no longer drop the out-of-line copies individually. It is
  worth paying, because at `-O2` neither inlines and without the mark the two
  hottest functions in the emulator execute from flash. Check placement with
  `nm` rather than assuming, since whether they inline depends on the level and
  on how large `PlatformRun()` has grown:
  ```sh
  arm-none-eabi-nm -nS build-pico/O3/yax86_pico_bench.elf | grep ' CPUTick$'
  ```
  `0x1000xxxx` is flash and `0x2000xxxx` is SRAM.
- **The annotations live in `core/src`, not in a pass over the generated
  bundle.** A post-processing pass over `core/yax86_core.h` is silently undone
  by the next bundle regeneration, which costs the entire 1.59x with no error
  and no warning. Annotating the source means regeneration carries the marks
  through by construction. If the count in the bundle ever drops, something is
  wrong with the bundler:
  ```sh
  grep -c YAX86_HOT core/yax86_core.h    # 162
  ```
  That is 85 annotations plus the macro block in `util/common.h`, whose seven
  lines all match on the substring, once per each of the 11 module bundles.
- `YAX86_ALWAYS_INLINE` is not about placement, but exists for the same reason:
  something the compiler was doing for free stops being free and nothing in the
  source says so. The case it was added for is a small helper with one hot
  caller, inlined into it at every level — until a second caller appears, at
  which point `-Os` and `-O2` emit it out of line for *both* and the hot path
  grows a call it never had. That measured **3.6% at `-O2`**. Marking the
  helper `YAX86_HOT` instead recovers nothing, which is the tell that the cost
  is the call itself rather than where it landed.
- **Forcing one inlining decision moves others, and not always in your favour.**
  Pinning that helper inline made GCC stop inlining the larger
  `GetMemoryOperandAddress()` into `ReadRegisterOrMemoryOperand()` at `-O3`,
  putting the effective address computation in flash on the hottest path in the
  emulator — 0.49%, of which marking it `YAX86_HOT` gives back 0.12%. The
  residue is the price of the `-O2` win and is worth paying at that ratio; what
  matters is that both halves get measured, because the second one is somewhere
  the change was never made.
- `YAX86_HOT_DATA` is the same thing for data, and is separate because a
  compiler will not put executable code and read-only data in one section — a
  code section attribute on a `const` array is a hard compile error, not a
  warning. The macro is kept so that whoever finds a candidate that pays
  reaches for the right one, but nothing currently uses it. **The obvious
  candidate does not pay.** The 256-entry opcode table — 2KB, read once on
  every instruction fetch — measures neutral in SRAM, 2.6ms over an 8.6 second
  run and inside the noise floor.
- That result is not mainly about cache residency, and the difference matters
  for whatever gets considered next. Putting the whole core back in flash so
  the XIP cache is thrashed by 88KB of code — the condition most favourable to
  the table — and moving the table alone is still worth only 0.14%. The
  RP2040's XIP counters (`xip_ctrl_hw->ctr_hit` and `ctr_acc`) say why: 559
  flash accesses per emulated instruction at a 98.59% hit rate. The table is
  one eight-byte line per instruction, **0.18% of that traffic**, so the
  ceiling was about 1% even if every read of it missed.
- **The marks hurt at `-Os`**, a 6.9% regression. The likely cause is bus
  contention: an XIP cache hit reaches the core over a different bus path than
  an SRAM access, so code in flash does not compete with the guest RAM array
  while code in SRAM does, and at `-Os` the core is small enough to sit in the
  16KB XIP cache. Placing hot code in a different SRAM bank than guest RAM has
  not been tried and is the obvious next question.

## sdl — SDL runtime

- An SDL3-based runtime for the emulator, also compiled via Emscripten to
  `yax86_sdl.{wasm,js}`.
- **How much guest time a pass of the main loop runs comes from the wall clock,
  not from how often the host calls it.** Under Emscripten the loop is driven
  by `requestAnimationFrame`, whose rate is the display's — and on a machine
  whose compositor has no vsync to lock to, it can fire well above it. Measured
  on an i7-3667U it ran at 120–200Hz, which with a fixed budget per callback
  ran the emulated 8088 at twice speed and its clock with it. A gap longer than
  `MAX_CATCH_UP_NS` is dropped rather than made up, so returning from a
  backgrounded tab does not produce a burst of catch-up emulation.
- The loop also declines to step more often than `MIN_STEP_NS`, a little under
  a sixtieth of a second. The emulated display refreshes sixty times a second,
  so stepping faster draws nothing new, and it is not free: with idle skipping
  on, a pass costs roughly one trip round the guest's idle loop whatever budget
  it was given, so an idle machine's cost tracks the callback rate rather than
  emulated time. The threshold sits *under* 1/60 deliberately — at exactly 1/60
  a callback arriving a hair early would be deferred to the next one, halving
  the rate instead of capping it. Events are pumped on every callback either
  way, so input stays responsive.
- Takes an optional floppy image path, defaulting to `floppy_a.img`. A 10MB
  hard disk is attached by default — blank unless `--hdd <image>` names an
  image, so it can be partitioned and formatted from inside DOS — and
  `--no-hdd` leaves the machine without one. The default matters because the
  Emscripten build has no command line, so a hard disk gated behind a flag
  would be unreachable in the browser.
- `--cga` or `--mda` selects the video adapter, defaulting to CGA. The adapter
  is fixed for the life of the machine: it is chosen through
  `PlatformConfig.video_adapter` before `PlatformInit()`, which decides both
  what the platform registers and what the PPI's DIP switches report, and the
  BIOS branches on those switches to decide which adapter to program.
- Both disks are loaded into memory with guest writes discarded. FDISK, FORMAT
  and booting from C: all work within a session, but the disks start out the
  same way on every run.
- `src/audio.c` turns the frequency the core reports for the PC speaker into a
  square wave. The core hands over a frequency rather than a stream of samples,
  so nothing here has to reconcile emulated time with the audio clock — the
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

## pico/bench — Raspberry Pi Pico benchmark harness

- Builds a firmware image that runs the core on an RP2040 and reports how fast
  it went. This is the board the emulator is meant to fit on, and it is not a
  small desktop — see the performance section above for why that matters.
- It is a consumer of the core, not part of it. Nothing under `core` knows the
  harness exists, and `pico/bench/src/main.c` uses only the public interface.
- The build is a standalone CMake project rather than a subdirectory of the top
  level one, because that build produces a native library, the tests and the
  SDL runtime, none of which cross-compile, and the Pico SDK has to own the
  toolchain from before `project()` onwards.

### The dos-boot workload and its invariant

- There is one workload, `dos-boot`: power on the machine, let GLaBIOS POST,
  boot MS-DOS 3.30 off the floppy image in `resources`, and stop when the `A>`
  prompt appears. DOS asks for the date and then the time on the way, and the
  harness answers both by watching the CGA text buffer and pressing Enter.
- It is the whole machine — CPU, PIT, PIC, video, FDC — rather than a CPU loop,
  which is the mix of guest code the emulator exists to run. What it executes
  is whatever the BIOS and DOS do, so it is only comparable against another
  yax86 build.
- **It is deterministic: 27,701,507 emulated cycles and 2,328,015 retired
  instructions, every run, at every optimization level.** Both are printed. If
  either moves, the change altered behaviour and the times either side of it
  are not comparable — check that before believing a speedup.
- The floppy is compiled into flash as a C array by
  `core/tools/generate-rom-data-files.js`, the same generator the ROM images
  use, because this target has no file system. It is generated into the build
  directory rather than committed the way the ROMs' arrays are, since at 360KB
  the image is a couple of megabytes of C source. Guest writes are discarded:
  360KB of writable copy is several times the SRAM left over, and nothing on
  the way to a command prompt writes to the disk.
- `enable_dos_idle_skip` is deliberately left off. The run stops at the prompt,
  which is where DOS starts idling, so the skip would have nothing to skip —
  and leaving it off keeps the workload a measure of how fast guest
  instructions execute rather than of how many are elided.

### Building and running

Needs `PICO_SDK_PATH` set and an `arm-none-eabi` toolchain. `build.sh`
configures and builds into `build-pico/<level>` and prints what the image costs
in flash and SRAM:

```sh
./pico/bench/build.sh                                  # the default level, -O2
YAX86_PICO_SYS_CLK_KHZ=400000 ./pico/bench/build.sh O2 O3   # what a PR reports
```

`run.js` reboots the board into BOOTSEL with `picotool`, loads the image and
captures what it prints. It exits non-zero if the run did not finish;
`--no-flash` captures from a board already running the image.

```sh
./pico/bench/run.js build-pico/O3/yax86_pico_bench.uf2 --out capture.txt
```

`YAX86_CORE_ROOT` names the checkout whose `core` is compiled in, which is how
a branch and its baseline are measured against one harness:

```sh
git worktree add ~/Scratch/yax86-baseline <commit>
YAX86_CORE_ROOT=~/Scratch/yax86-baseline ./pico/bench/build.sh O2 O3
```

Notes on the machinery:

- **The serial port must be opened exactly once.** The firmware waits for DTR
  before printing anything and every open asserts it, so configuring the line
  with a separate `stty` first starts the run and the header is gone before a
  reader attaches. `run.js` wraps the descriptor it already holds in a
  `tty.ReadStream` and calls `setRawMode()` on that, configuring the line
  without opening the device a second time. It finds `/dev/ttyACM*` rather than
  hard-coding it, because the port re-enumerates after a flash or a watchdog
  reset and can come back on a different node.
- A build directory reused across two core roots keeps one object file per
  root, since CMake names an object after the absolute path of its source.
  `build.sh` spells the path out rather than searching, so its `core_text`
  column always refers to the core just built.
- `src/yax86_hot.h` is force-included into every translation unit, and is where
  this target defines `YAX86_HOT` to `__not_in_flash()`. Force-included rather
  than included from the core, so that the core knows nothing about this
  target, and inert against a core revision that does not use the mark.
- The optimization level is applied through `CMAKE_C_FLAGS_RELEASE` rather than
  to the emulator's target alone, so the SDK and the C library are compiled the
  same way. What is being measured is how a whole image behaves in the XIP
  cache, and an emulator built one way inside an SDK built another would not
  answer that.
- `build.sh` is bash, like the scripts under `tools`, because what it does is
  run `cmake` in a loop. Everything that would otherwise have to parse
  something is Node, like the generators under `core/tools`, and uses only the
  standard library — there is no `package.json` anywhere in the repository and
  nothing here should need one.
- The summary table's figures come from `image-size.js`, which reads the ELF
  program and section headers directly rather than parsing `readelf` and
  `size`. Recovering those numbers from `readelf` output means parsing hex in
  awk, and `strtonum()` is a GNU extension, so the script silently produces
  empty columns under mawk — the default awk on Debian and Ubuntu — and under
  the BSD awk on macOS. Reading the headers is shorter, portable, and drops two
  toolchain dependencies. It reproduces `arm-none-eabi-size`'s text column
  exactly, which is what `core_text` has always been.

### Current figures

GCC 16.1.0, SDK 2.3.0, picotool 2.3.0, 400MHz, 128K of guest RAM, hot path in
SRAM, at master `05585ad`:

| level | seconds | emulated MHz | MIPS | vs a real 8088 | image flash | image SRAM | core `.text` |
| ----- | ------- | ------------ | ---- | -------------- | ----------- | ---------- | ------------ |
| `-O3` | **6.513592** | **4.253** | **0.357** | **89.2%** | 472,700 | 176,240 | 87,229 |
| `-O2` | 7.213556 | 3.840 | 0.323 | 80.5% | 459,116 | 169,120 | 73,853 |

- A real 4.77MHz 8088 runs this in 5.807 seconds. **The seconds go out of date
  with every optimization** — this table is a snapshot to sanity-check a fresh
  measurement against, not a baseline to compare a branch to. Build the
  baseline from its own commit, as above.
- `-Os` is not reported and nothing ships at it, though `build.sh` still
  accepts it. It is the slowest level here by a wide margin and the one
  `YAX86_HOT` hurts.
- **The XIP cache can reorder the optimization levels**, which is the whole
  reason this harness exists: with the core running entirely from flash, `-O2`
  was the slowest of the three and `-Os` beat it by 6%, because the cache
  rewards a small image enough to overturn the desktop ordering. With the hot
  path in SRAM the cache is no longer what decides and the ordinary ranking
  returns.
- Flash is dominated by the 360KB floppy image; the code itself is under 110KB.
  `core_text` is the whole core library section, which includes about 32KB of
  constant data — the ROM images, the font tables and the opcode table — that
  does not move with the optimization level.
- `stack_peak` reports how deep the stack got, which is a number a real Pico
  port needs. The unused stack is painted with `0x5A5A5A5A` from `main`'s own
  frame and afterwards searched for how far the paint was overwritten, so it
  costs nothing while the run is going. Interrupt frames count, making it the
  true peak rather than the emulator's share. It measures roughly 700–970 bytes
  of a 4KB bank, and moves both with the optimization level and between runs of
  one image, because where an interrupt frame lands is not deterministic —
  treat it as a bound with room to spare, not a figure to compare builds by.
- **Guest memory and the stack cannot collide**, and not by a narrow margin.
  Guest RAM sits at the bottom of the 256KB bank while the stack is in
  SCRATCH_Y, growing down within its own 4KB. To reach a guest byte it would
  have to grow through the whole of SCRATCH_Y, then all of SCRATCH_X, then the
  ~100KB of unused heap. The heap grows the other way, up and away from guest
  RAM.

### Timing discipline

- **Nothing is printed from inside a run.** Host I/O on this target costs
  orders of magnitude more than the guest instruction it would be reporting on
  — the same trap as leaving a browser console open in front of the WASM build
  — so the UART is switched off entirely and results accumulate in RAM until
  the run has finished.
- Everything printed is derived from integers, so no floating point formatting
  is linked in. Emulated cycles per microsecond is emulated megahertz and
  retired instructions per microsecond is MIPS, so neither needs a unit
  conversion. Both are truncated to two decimals, which is why a PR computes
  them from the seconds instead.
- The one perturbation that cannot be removed is USB servicing, which the SDK
  drives from a timer interrupt. It costs the same in every build, so it does
  not affect any comparison this harness exists to make.
- The run is driven a batch of emulated cycles at a time through
  `PlatformRun()`, which is how an application drives it, rather than an
  instruction at a time. The core counts retired instructions itself, so there
  is nothing left for a caller to do per instruction. An invalid opcode does
  not stop the run: the 8088 has no invalid opcode exception, so a real machine
  would carry on.

### Overclocking

- `YAX86_PICO_SYS_CLK_KHZ` raises the system clock, and the change happens
  *after* USB is up rather than before. A clock the chip cannot take hangs it,
  and a hang before USB enumerates leaves no way in except the BOOTSEL button —
  no good on a board being flashed remotely.
- **Do not raise the clock past 400MHz.** That is a standing instruction, not a
  technical limit. 400MHz is also the clock a PR reports at.
- A watchdog turns a hang into a reboot, and the reboot lands back in `main`
  where a marker says not to try the same clock twice.
  `watchdog_caused_reboot()` alone is not evidence of a hang — `picotool`
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
  a clock the flash could not keep up with — which looks exactly like an
  unstable overclock.

### Profiling

- `YAX86_PICO_PROFILE=1` builds in a sampling profiler: a timer interrupt at
  50kHz records the interrupted PC into a histogram, dumped after the run as
  addresses so that no symbol table has to be carried on the board.
  `symbolize.js` turns the dump into function names.
  ```sh
  YAX86_PICO_PROFILE=1 ./pico/bench/build.sh O3
  ./pico/bench/run.js build-pico/O3/yax86_pico_bench.uf2 --out capture.txt
  ./pico/bench/symbolize.js build-pico/O3/yax86_pico_bench.elf capture.txt
  ```
- **Symbolize against the ELF that produced the capture**, not merely one built
  from the same source at the same level. Addresses move between builds, so the
  wrong ELF does not fail — it silently reports a plausible ranking of
  functions the run never executed.
- It costs a little over a percent and the histogram costs 32KB of SRAM, so it
  is off by default and **a timing run never has it on**. A profiling build
  says where the time went, never how much of it there was.
- SRAM is bucketed at 16 bytes and flash at 128. The coarse flash buckets are
  not trustworthy below the top few entries: several small functions share a
  bucket and only the first is named, so a bucket can be credited to a function
  the workload never executes. Verify anything in the tail before acting on it.

## What to emulate, and what not to

- **The project does not implement features the IBM PC/XT does not use** under
  GLaBIOS or MS-DOS. It does not attempt to support other hypothetical x86
  operating systems, only MS-DOS and era-accurate software. For example, the
  Intel 8259A PIC's level-triggered interrupts, auto-EOI mode and specific
  end-of-interrupt mode are all deliberately absent, because the PC/XT uses
  edge-triggered interrupts, manual EOI and normal fully-nested mode.
- **The project does implement anything GLaBIOS needs** to run MS-DOS 3.3 and
  basic DOS applications.
- GLaBIOS is the BIOS. Its source is at `.cache/GLaBIOS/src/GLABIOS.ASM`;
  the target is its `ARCH_TYPE_EMU` build type.

## Reference implementations

When working out how to emulate a hardware component or one of its features,
read how these do it. All are checked out under `.cache`:

- 8086tiny — `.cache/8086tiny`
- 86Box — `.cache/86Box`
- MartyPC — `.cache/martypc`
