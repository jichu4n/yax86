# Project: yax86

## Project goals

A small IBM PC/XT emulator for minimal MCU platforms like the Raspberry Pi
Pico, and for the browser via SDL and Emscripten.

- Emulates a basic PC/XT system with an 8086/8088 processor.
- Runs MS-DOS 3.3 on top of GLaBIOS.
- Fits and runs well on an RP2040: no dynamic allocation, no file system, no
  libc dependencies, and a hot path chosen against on-target measurements.
- Runs as a real machine on the ClockworkPi PicoCalc, an RP2040 handheld with
  a 320x320 LCD and a keyboard - see `pico/picocalc` below.

## Repository layout

| path | what it is |
| --- | --- |
| `core/` | The emulator. Portable C99, no external runtime. |
| `core/src/<module>/` | One module each: `cpu`, `platform`, `video`, `fdc`, `hdc`, `pit`, `pic`, `dma`, `ppi`, `keyboard`, `bios`, `util`. |
| `core/tests/` | Unit tests per module, C++14 and Google Test. |
| `sdl/` | SDL3 runtime, native and Emscripten. |
| `pico/bench/` | RP2040 benchmark harness. Standalone CMake project. |
| `pico/picocalc/` | The runtime for the ClockworkPi PicoCalc. Standalone CMake project. |
| `pico/common/` | What both Pico targets compile in. |
| `tools/` | Build and test scripts for the native and Emscripten builds. |
| `resources/` | Floppy and disk images, and third-party code kept as released. |
| `.cache/` | Downloaded references — GLaBIOS source, the 8088 test suite, other emulators. Not in the repository. |

## Building, testing and debugging

The project uses CMake. Native and Emscripten output go to `build-native` and
`build-emscripten`; the Pico targets build into `build-pico`.

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
Both compilers and both build types — `Debug` for the warnings-as-errors flags
it carries, `Release` for what actually ships — are worth covering: undefined
behavior in the emulator tends to show up as a difference from the hardware
under one of them and not the others. `MinSizeRel` is not built, for the same
reason `-Os` is not benchmarked: nothing ships at it.

### The unbundled syntax check

The library ships as a single bundled translation unit, which is also the only
configuration CMake builds. The unbundled form — compiling each source file in
`core/src` independently — is what `Y_MODULE_PRIVATE`'s second arm and the
file-local convention both rest on, and nothing else builds it.

`tools/check-unbundled.sh` runs `$CC -std=c99 -fsyntax-only` over every source
file with `-Wall -Wextra -Wpedantic -Werror -Icore` to catch missing includes or
accidental dependencies on earlier files in the bundle before they rot. The
generated ROM sources are included, which is what keeps them self-contained
even though nothing compiles them on their own. It runs automatically as part
of `./tools/run-tests.sh`, which CI invokes with `CC` set
to the leg's compiler, and it is first in that script because it costs a quarter
of a second and the hardware test data below it is a 480MB download.

**A cross-file reference to a file-local symbol is what it exists to catch, and
nothing else can.** `instructions_add.c` precedes `instructions_sub.c` in
`bundle.json`, so a call from the second to a `static` in the first compiles
cleanly in the bundle: `static` gives linkage internal to the translation unit,
and the translation unit is the whole bundle. File locality is a convention held
up by nothing except this check and the absence of a declaration in a header.

`-Werror` is unconditional here, where the main build carries it only in
`CMAKE_C_FLAGS_DEBUG`. This is a lint rather than a build, and an implicit
function declaration is an error by default only on GCC 14 and Clang 16 and
later, so `-Werror` is what makes the check mean the same thing on every
compiler that runs it. The cost is that a new warning in a future compiler
fails every leg of the matrix rather than the `Debug` ones.

### The public symbols allowlist check

Where the unbundled syntax check inspects source code,
`tools/check-public-symbols.sh` inspects the compiled static library
(`build-native/core/libyax86_core.a`) using `nm -g`. It compares the exported
symbols against `core/public_symbols.txt`.

**An unannotated definition that leaks into the public namespace is what it
exists to catch.** In C, a file-scope function or variable defined without
`static` has external linkage by default. Because the library is compiled as a
single bundle, a helper defined without `Y_FILE_PRIVATE` or
`Y_MODULE_PRIVATE` compiles cleanly and passes all tests, but silently
pollutes the symbol table of any host application linking against
`libyax86_core.a`.

The allowlist records each exported symbol with its `nm` symbol type letter,
as in `T YBIOSGetROMData`, so it pins both which symbols are exported and that
every one of them is a function. The library exports no data at all, and a
const table that gained external linkage would show up as an `R` rather than
only as a new name. The file is the record of what the public surface is -
nothing restates its length, which would be a second place to keep true.

The check runs automatically in `./tools/run-tests.sh`. When public API is
intentionally added, removed, or renamed, update the allowlist:

```sh
./tools/check-public-symbols.sh --update
```

### The namespace check

`tools/check-namespace.js` checks that every file-scope name
`core/yax86_core.h` brings into a host's translation unit follows the naming
rule under "Naming" below, and that the owner it names is the module that
defines it.

**Types, enum values and macros are what it exists to catch**, because the
public symbols allowlist cannot see them: `nm` reads a linker symbol table, and
none of those three ever reaches one. They are also where most of a header's
exposure is - a host with a type of its own called Position, or one that
includes another library with an enum value called kAX, fails to compile
against the core however clean its symbol table is.

It reads names from the compiler rather than from tokens: the top-level
declarations in a clang JSON AST dump of the bundle, so that block scope is
excluded by construction, and the macros from `clang -E -dM` less those the
system headers define. clang's JSON reports physical locations, so the owner is
recovered from the bundle's `#line` directives. It needs `clang` whatever `CC`
is, since only clang dumps an AST, and fails saying so rather than skipping
when there is none.

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
  `clang-format`, except for how file-scope names in the core are spelled -
  see "Naming" below.
- No dependencies on libc functions like `printf` or `memset`. Standard library
  headers are fine for types like `uint8_t` and for constants and macros like
  `NULL`.
- No dynamic memory allocation — compile-time static allocation and the stack
  only.
- For zero-initialized data use static zero-initialization rather than
  `memset`, as in `static MyStruct data = {0};`.

### Declarations

- Prefer enums over `#define` for constants, and over numeric literals in
  logic: define `enum { Y_FDC_FOO = 0xF8 };` rather than writing `0xF8`
  inline.
- Prefer `static inline` functions over macros.
- Prefer specific types like `uint8_t` over generic ones like `int` in
  interfaces — function signatures and struct members.
- Define structs and enums as `typedef struct Name { ... } Name;` and
  `typedef enum Name { ... } Name;`.
- Prefix increment and decrement, `++var` and `--var`, not `var++` and `var--`.
- Annotate unused parameters with `Y_UNUSED` from `util/common.h`.

### Naming

`core/yax86_core.h` is the only file a host includes, so every name it defines
at file scope is a name the host can no longer use. Every one of them is in a
`Y` namespace, spelled the way SDL, SQLite and Vulkan spell theirs - a prefixed
CamelCase for what has storage and a prefixed ALL_CAPS for what does not:

> **Anything with a type or storage** — function, typedef, struct or enum tag,
> file-scope variable, `const` object — is `Y` + owner + CamelCase.
>
> **Anything that is a compile-time value** — enum value or macro — is `Y_` +
> OWNER + `_` + ALL_CAPS.

| kind | example |
| --- | --- |
| function | `YCPUTick`, `YCPUReadRegisterOperandByte` |
| type | `YCPUState`, `YVideoPosition`, `YVideoRGB` |
| `const` object | `YCPULogModule`, `YVideoDefaultConfig`, `YCPUOpcodeTable` |
| enum value | `Y_CPU_AX`, `Y_CPU_BYTE`, `Y_FDC_MSR_BUSY`, `Y_PIC_ICW1_INIT` |
| macro | `Y_HOT`, `Y_CPU_LOG`, `Y_STATIC_VECTOR_TYPE` |

- **The line between the two forms is storage.** An enum value or a macro has
  no address; a `const` object does, and `&YCPULogModule` reads as taking the
  address of an object where `&Y_CPU_LOG_MODULE` would read as taking the
  address of a macro.
- **The owner is the module whose directory defines the name**: `CPU`,
  `Platform`, `Video`, `FDC`, `HDC`, `PIT`, `PIC`, `DMA`, `PPI`, `Keyboard`,
  `BIOS`. A sub-component follows its module - CGA, MDA and the CRTC are part of
  video and the memory and port maps part of platform, so
  `YVideoCGARenderRegion`, `Y_VIDEO_CRTC_REGISTER_CURSOR_START` and
  `YPlatformMemoryMapEntry`. The module prefixes are what keep the modules
  apart from each other in the one translation unit they compile into, where
  eleven init functions would otherwise all be called Init.
- **util is not a module**, and each of its headers is its own owner: `Log`
  (whose state and config are `YLogger` and `YLoggerConfig`), `Logfmt` (the
  line writer, `YLogfmtLine`), `StaticVector`, and `YSNPrintF`, whose `va_list` form keeps C's prefix at the front as
  `YVSNPrintF`. There is no `YUtil`. The macros in `util/common.h` are the
  project's own infrastructure and have no owner: `Y_PUBLIC`, `Y_HOT` and
  `Y_UNUSED`.
- **`YAX86_IMPLEMENTATION` and the include guards are outside the rule.** A
  host defines `YAX86_IMPLEMENTATION`, before including the header, to choose
  whether the header compiles to declarations or to the implementation as
  well, and an include guard such as `YAX86_CPU_BUNDLE_H` or
  `YAX86_CPU_PUBLIC_H` decides whether a header's contents are compiled at all.
  Both are switches on what the header includes rather than names it gives the
  host, so they keep the project's full name, `YAX86_`. The namespace check
  leaves `YAX86_IMPLEMENTATION` alone and holds every include guard to
  `YAX86_` then ALL_CAPS.
- **A name that already says its owner does not say it twice.** The owner
  leads, and a repeat elsewhere in the name goes: `YVideoDefaultConfig`,
  `Y_VIDEO_NUM_MODES`, `YCPULogModule`. A second occurrence that is part of
  what the name means stays - `Y_VIDEO_CONTROL_VIDEO_ENABLE` is the CGA
  control register's video enable bit.
- **ALL_CAPS is split at every CamelCase word boundary, and a digit group stays
  with the word it qualifies**: `Y_FDC_ST0_NOT_READY`,
  `Y_FDC_DOR_MOTOR0_ENABLE`, `Y_VIDEO_CRTC_REGISTER_RESERVED16`, and
  `Y_PPI_MEMORY_SIZE_128KB`, where the number quantifies the unit after it. A
  resolution is a word of its own, as in `Y_VIDEO_CGA_PIXEL_MASK_320X200`.
- **Block scope is not affected.** Locals, parameters and struct members are
  snake_case, and a block-scope constant keeps Google's `kName`. Only file
  scope is shared with a host, and the namespace check reads only top-level
  declarations.
- **This departs from the Google style guide**, which names enumerators
  `kEnumName`. The guide's own reason for that is that macro-style enumerators
  collided with macros; with every enumerator and every macro under `Y_`, a
  collision can only be with a name this project owns. The guide is also
  written for C++, where `enum class` and namespaces scope an enumerator, and C
  has neither.
- **The rule covers the core, not the hosts.** `sdl/`, `pico/bench` and
  `pico/picocalc` define nothing that anything else includes, so the reason for
  the namespace does not reach them, and they keep the Google style guide's
  names: `kName` for constants and enumerators, `g_name` for file-scope
  variables, and CamelCase for functions, led by the host's own module word as
  in `DisplayPutPixels()` or `LcdInit()`. The same goes for the visibility
  macros below: a host file-scope definition is plain `static` or nothing. A
  host must not take the `Y` prefix for its own names. The namespace is the
  core's, and a host name in it could collide with one the core adds later.

### Visibility

- Every file-scope definition carries a macro from `util/common.h` naming who
  may reach it, so that what a symbol is part of is answered by the line it is
  defined on rather than by which header happens to declare it:

  | macro | expands to | for |
  | --- | --- | --- |
  | `Y_PUBLIC` | nothing | public API, defined in a source file |
  | `Y_PUBLIC_HEADER` | `static` | public API, defined in a header |
  | `Y_MODULE_PRIVATE` | `static` bundled, nothing unbundled | shared between a module's source files |
  | `Y_MODULE_PRIVATE_HEADER` | `static` | module-private, defined in a header |
  | `Y_FILE_PRIVATE` | `static` | used only in the file that defines it |

- **The macro names the tier and nothing else.** `inline`, `const` and the
  return type are written out after it, exactly as they would be without it:
  `Y_PUBLIC_HEADER inline bool YCPUGetFlag(...)`, `Y_FILE_PRIVATE const
  YCPUFlag YCPUFlagsForClearAndSetInstructions[]`. Folding a keyword into a
  macro name needs one macro per tier per declaration shape, and the shapes do
  not stop at `inline` - a header-defined public function that is not inline has
  nowhere to go in such a scheme, which `YLoggerWrite()` is.
- A separate macro is warranted where the **expansion** differs, never where a
  keyword does. That is why `Y_PUBLIC` and `Y_PUBLIC_HEADER` are two
  macros: a definition in a header needs internal linkage and one in a source
  file must not have it.
- **Public means reachable through the interface, not referenced by a host
  today.** The ten log module descriptors, `YCPULogModule` and its siblings,
  are used only inside `core/src`, and are public because
  `YLoggerEnableModule()` is public and takes one - a host turning on the CPU's
  logging has nothing else to pass. `YSNPrintF()` is
  the other way round: it sits in every module's public half because
  `logfmt.h` needs it, and nothing outside the core calls it.
- `static` alone says a symbol has internal linkage, which in the bundled build
  means internal to the whole bundle rather than to its file. It is the marker
  that cannot express which of the tiers above was meant, which is why none of
  them is spelled that way.
- The one deliberate exception is the macro body of `Y_STATIC_VECTOR_TYPE` in
  `util/static_vector.h`, which generates `static` helper functions into
  whichever header or source file instantiates it. Its tier depends on the
  instantiation site, so its functions remain plain `static`.

### Placement and inlining marks

- **The visibility macro comes first, then the placement and inlining marks,
  then the return type**: `Y_MODULE_PRIVATE Y_HOT YCPUInstructionResult
  YCPUExecuteSub(...)`, `Y_FILE_PRIVATE Y_HOT uint8_t ReadByte(...)`.
  Every definition carries a visibility macro and only 91 carry a mark, so
  leading with the universal one puts the answer to "is this part of the
  interface" in the same place on every line, and makes `^Y_PUBLIC` an exact
  count rather than an approximate one. `Y_HOT`, `Y_NOINLINE` and
  `Y_ALWAYS_INLINE` follow it, in whatever combination applies -
  `YCPUExecuteDecodedInstruction()` carries two.
- Both orders compile, and an attribute behind `static` (or `Y_FILE_PRIVATE`,
  which expands to it) still takes effect: `Y_FILE_PRIVATE Y_HOT`,
  `Y_FILE_PRIVATE inline Y_HOT` and `Y_FILE_PRIVATE Y_HOT
  Y_NOINLINE` all land in the named section under gcc and clang. Check that
  with `objdump -t` at `-O0` rather than at `-O3`, where a small static inlines
  away and its section never appears.
- `clang-format` will not fix a misplaced mark, and will disguise one. It
  chooses where to break lines and never reorders tokens, so a mark added to
  the start of a *continuation* line — which is where a wrapped signature's
  declarator lives, below its return type — is already mid-declaration.
  Reformatting rejoins it as `YCPUInstructionResult Y_HOT`, which reads as
  though the mark belonged to the return type and was put there on purpose.
  Put both macros on the line the declaration *starts* on and reformatting
  keeps them there, however it decides to wrap. A displaced visibility macro is
  the worse of the two, since it is what the line is meant to be scannable by.

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

- **`-O3`**, GCC, at **250MHz** with the hot path in SRAM and the default 128K
  of guest RAM. `-O2` and `-Os` are not reported: `-O3` is what ships, and with
  performance now clearing 8088 parity below 250MHz, 250MHz is the intended
  operating overclock.
- **Seconds, emulated MHz, MIPS and "vs a real 8088"**, for the baseline and
  for the branch.
- The **gain**, as the increase in emulated MHz over the baseline row.

```
| | seconds | emulated MHz | MIPS | vs a real 8088 | gain | core `.text` |
| --- | --- | --- | --- | --- | --- | --- |
| master | 4.975485     | 5.568     | 0.468     | 116.7%     | —          | 83,043 |
| this   | **4.814238** | **5.754** | **0.484** | **120.6%** | **+3.35%** | 82,435 |
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

- **The invariant, and that it did not move.** `dos-boot` retires 2,328,128
  instructions in 27,702,858 emulated cycles, at every level and on every run.
  If either number changed, the change altered behaviour and the times either
  side of it are not comparable — say so and explain why, rather than
  presenting the speedup.
- **How the baseline was obtained.** Rebuild it from its own commit through
  `YAX86_CORE_ROOT` against the same harness, on the same board, in the same
  session, or with its own harness where the names differ - see "Building and
  running" below. A figure recorded in an earlier PR is a cross-check, not a
  baseline.
- **How many runs, and their spread.** Two or three is enough at this noise
  floor.
- **`core .text`**, since several changes here have won on speed and size at
  once and a size regression is worth knowing about either way.
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
- **Every module is brought up the same way**: zero-initialize its `XState`,
  fill in the fields of `state.config`, call `XInit(&state)`. The config is a
  field of the state rather than a separate object, so there is nothing whose
  lifetime has to outlast the module, and `XInit()` is where a host's mistake
  is checked and reported.
- **No `XInit()` zeroes anything.** Each sets only the fields that want a
  non-zero power-on value, and a caller that has already zeroed the state in
  order to fill in its config would otherwise be paying for it twice. For the
  whole machine that is one zeroing: every module's state is a field of
  `YPlatformState`, so zeroing that covers all of them.
- The consequence is that **a host must zero-initialize its `YPlatformState`**.
  `static YPlatformState platform;` does it, as does `= {0}`. Miss it and the
  machine comes up with garbage in every module rather than in one.
- `YFDCInit()` is empty, because an FDC powers on with every field at zero. It
  is kept so that every module is brought up the same way, and so that a
  non-zero default acquired later has somewhere to go.
- **A reset is not an init.** `YDMAWritePort()`'s master reset does have to
  clear the controller's state, and it saves and restores the config across
  doing so - a reset is the chip's, not the machine's, and the config is what
  the host wired up. That distinction is what the DMA tests caught when the
  reset was left calling `YDMAInit()` after it stopped zeroing.
- The logger is the exception and stays a pointer: `YLogger` is a `util` type
  rather than an emulated module, and several of them may reasonably share one
  `YLoggerConfig`.
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
- **A ROM array is module-private, and its source reaches the library only
  through the bundle.** The generated `.c` file is listed as a source of
  `yax86_core` so that changing a ROM file reruns the generator. It carries
  `HEADER_FILE_ONLY` so CMake does not compile it into a separate object file:
  the array is already defined in the bundle, so a standalone object in the
  archive would be dead weight that is never linked. The header's `extern`
  declaration is guarded by the module's bundle include guard, preventing
  linkage conflicts with the bundle's `static` definition.
- **Module-private generation is opt-in via `--module`.** `core/CMakeLists.txt`
  passes `--module <name>`, which marks the array `Y_MODULE_PRIVATE`, adds
  bundle guards, and names both identifiers in the core's namespace: the symbol
  `BIOSROMData` in `bios` gives the array `YBIOSROMData` and its size
  `Y_BIOS_ROM_DATA_SIZE`. The symbol has to begin with the module's owner, and
  the generator refuses one that does not. In contrast, `pico/bench` invokes the
  generator without `--module` for its boot floppy image, producing a
  standalone translation unit with external linkage that
  `pico/bench/src/main.c` can link against, under the harness's own names
  `kDOSFloppyData` and `kDOSFloppyDataSize`.

### cpu — state and config

- **`YCPUConfig` is a field of `YCPUState`, and a caller fills it in before
  `YCPUInit()`** — the same shape every module follows, described under "How the
  core fits together". What is specific to the CPU is why it is held by value
  rather than by pointer, and where in the struct it sits.
- Held by value rather than by pointer. The dereference cost **+0.34% at `-O3`
  and 3.37% at `-O2`** on a path that reads a config field every instruction,
  and a pointer made the config an object the caller had to keep alive for as
  long as the CPU — a rule nothing stated and nothing enforced.
- **Three kinds of field, and the difference is what may write them:**
  - **Config — constructor arguments.** Written by the caller before
    `YCPUInit()`, checked there, read-only afterwards. `YCPUInit()` is the only
    reader of `decode_cache_num_entries`, so a host that changes it later
    changes nothing.
  - **Plain state.** `registers`, `flags`, `instructions_retired`,
    `cycles_this_tick`. Nothing is derived from them, so a debugger or a test
    writing one directly is fine and reads better than a setter would. There
    are about 1,400 such writes in the tests.
  - **Coupled state — a setter, which is its sole writer.** The fields another
    field's correctness depends on: `YCPUSetDirectDataWindow()` and
    `CPUInvalidateInstructionFetchWindow()`. This
    is the sole-writer discipline `PlatformUpdateEnabledFlags()` and
    `YPICUpdateUnmaskedRequest()` already follow, stated as a rule.
- **`YCPUSetDirectDataWindow()` is not an aberration**, and an earlier note here
  called it one. The window is *derived* — the platform computes which slice of
  its memory the CPU may index, from whichever map entry covers address 0 and
  only while no watchpoint is enabled — rather than handed over raw, and its
  `data`/`end` pair carries an invariant that makes the bounds test a single
  compare. That is coupled state, and a setter is what coupled state gets.
  What made `CPUSetDecodeCache()` wrong was different: it passed *constructor
  arguments* through a setter.
- **A per-call parameter is none of the three.** `YCPUTick()`'s `max_run_cycles`
  is host-provided but set every tick, so it is an argument, not a field. Do
  not "fix" it into the config: config is written once, and a value that
  changes every call written once is wrong rather than tidy.
- **Where a field sits in `YCPUState` is worth 1.0–1.7%.** A Cortex-M0+ encodes
  only a 5-bit immediate offset in `ldr`, so a field beyond about 124 bytes
  into the struct costs extra instructions to address. `YCPUConfig` is ~70 bytes
  and `code_page_generation` is 256, and putting either ahead of the hot
  scalars pushes them out of range: config first measures **1.04% slower at
  `-O3` and 1.69% at `-O2`** than config last, and moving the generations after
  the decode cache fields is worth another 0.56% at `-O2`. Large or cold
  members belong at the end.

### cpu — instruction decoding

- `YCPUFetchNextInstruction()` decodes directly into the caller's
  `YCPUInstruction`. Filling a local and copying it out at the end costs more
  than the whole decode: with `gcc -O3` over a boot and idle run at the DOS
  prompt, 3.5% of host instructions, 8.4% of data references — 97M extra reads
  and 97M extra writes — and about 19% of wall clock time.
- The wall clock cost being so much larger than either count is the part worth
  carrying forward, and it is not a cache effect — D1 misses are identical
  either way. Writing a struct to the stack and immediately reading it back
  creates dependent store-to-load pairs that stall, and neither callgrind nor
  cachegrind models that. **Anything on this path that round trips through
  memory costs more than its instruction count suggests.**
- Because the decode happens in place, a failed fetch leaves the caller's
  `YCPUInstruction` holding however much had been decoded. Every caller treats a
  failed fetch as fatal, so nothing reads it back.
- A decode settles each field where it becomes known rather than zeroing the
  struct up front. `opcode`, `size` and `immediate_size` are assigned on every
  path that returns success; `has_mod_rm` and `displacement_size` in both arms
  of the ModR/M branch; `mod_rm` is read only where `has_mod_rm` is set; and
  `displacement[]` and `immediate[]` are read no further than their size fields
  say. Only the two prefix fields are cleared before the decode starts. Worth
  **4.63% at `-O3`** and 3.96% at `-O2`.
- What makes that safe is that a reused destination keeps nothing of the
  instruction before it, and reuse is the ordinary case — `YCPUTick()` holds a
  single `YCPUInstruction` for the life of the CPU.
  `ADecodeKeepsNothingOfThePreviousInstruction` pins it directly. The hardware
  suite catches a dropped clear as well, and emphatically — removing one fails
  thousands of encodings across most opcodes — but as a diffuse result rather
  than a named one.
- **No field of `YCPUInstruction` is a bitfield.** `has_mod_rm`,
  `displacement_size`, `immediate_size` and all three of `YCPUModRM` take a
  whole byte each, which puts the struct at 16 bytes and is worth **0.95% at
  `-O3` and 2.18% at `-O2`** — while also making the core 112–120 bytes
  *smaller* at every level.
- The reason is that a packed field is never simply loaded or stored. A write
  to one is a read-modify-write of the byte it shares — `ldrb`/`bics`/`strb`,
  or `ldrb`/`bics`/`orrs`/`strb` to set as well as clear — and a read is a load
  plus a shift plus a mask. `YCPUModRM` is where that bites hardest: `mod` and
  `rm` are taken by `YCPUGetMemoryOperandAddress()` for every memory operand and
  by `YCPUGetEffectiveAddressCycles()` for every instruction, and `reg` selects
  the handler for all five instruction groups. The margin is widest at `-O2`,
  where less is inlined and those reads stay explicit.
- **Do not re-derive a rule that `YCPUInstruction` should be kept small.** It
  was true only while the decode copied the struct, which it no longer does;
  nothing copies a `YCPUInstruction` per instruction, so its size buys almost
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
  which `YCPUGetImmediateSize()` needs whether or not the instruction carries a
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
  fields that `Y_CPU_MAX_IMMEDIATE_BYTES` clamp is load-bearing rather than
  belt-and-braces.

### cpu — instruction fetch

- `YCPUConfig.get_instruction_fetch_window` lets the host fill in
  `YCPUState.instruction_fetch_window` — a `data` pointer and the half-open
  range `[start, end)` it covers — and the fetch reads bytes straight out of
  it. Reading each byte through `YCPUConfig.read_memory_byte` instead is an
  indirect call per byte, two to six times per instruction, into a host that
  then resolves the address and indexes an array: worth **9.8%** on `dos-boot`.
- The host fills the window in directly rather than returning it, so the CPU
  never has to decide what range a returned pointer stood for. A host hands
  back the whole region the address falls in rather than the tail of it from
  that address, which is what makes a jump backwards to before the point
  fetching started stay inside the window — see
  `WindowCoversTheRegionBeforeTheFetchAddress`. `data == NULL` is how a host
  declines.
- The window is kept across instructions in `YCPUState`, not re-derived per
  instruction, and is held as a range of addresses rather than a cursor. Both
  straight-line execution and a jump backwards within the same region land
  inside the one already open, so reopening is a compare rather than a call.
- `YCPUInstructionFetchState` is the per-decode cursor derived from it, and
  holds neither a `YCPUState` pointer nor a reference to the CPU's IP: IP has to
  keep naming the instruction being decoded until `YCPUTick()` advances it by
  `instruction.size`, and a byte-by-byte write into `cpu->registers[]` is a
  store the compiler cannot keep in a register.
- **Caching a pointer is safe where caching bytes would not be.** The window
  points into the host's own storage, so a write through the memory map is
  visible to the next fetch with no invalidation and self-modifying code keeps
  working.
- **Nothing invalidates the window, and nothing needs to.** A window spans
  exactly the memory map entry it was handed out from; registering a region
  cannot overlap an existing entry, and there is no way to unregister or
  change one, so no change to the map can reach inside an open window. Add
  unregistration and that stops being true - the window would then have to be
  dropped where the map changes.
- The window stops at a segment wrap. IP is 16 bits and wraps within the
  segment where the linear address does not, so `remaining` is clamped to
  `0x10000 - ip` and the bytes past it go through the ordinary path, which
  recomputes the address from the wrapped IP.
- The 8088 hardware suite runs with a window supplied, which is the strongest
  check the fetch has — every encoding length, prefix count and segment wrap
  the part can produce, about three million times. The path taken when a host
  supplies no window is what the mock configs in `cpu_test.cpp` exercise.

### cpu — the decode cache

- `YCPUConfig.decode_cache` keeps decoded instructions, keyed by the linear
  address they start at, so that an instruction the guest runs more than once
  is decoded once. Most guest code is a loop, and decoding was a fifth of the
  emulator's time: worth **10.86% at `-O3`** and 10.12% at `-O2`, the largest
  single win in the campaign. `-O3` runs `dos-boot` in less time than a real
  4.77MHz 8088 takes.
- **A hit is used in place.** The executor is handed `&entry->instruction` and
  nothing is copied; a miss decodes straight into the entry it is going to
  occupy, so a decode writes the instruction exactly once whichever way the
  lookup went. Copying instead measures **3.25% slower at `-O3`**, which is
  most of the win — an earlier attempt that copied was recorded at +1.4%
  against this one's +10.86%. Lending the entry out is safe because no
  instruction handler writes through the `YCPUInstruction` it is given.
- **The storage and its size are constructor arguments, so they are in
  `YCPUConfig`** and checked in `YCPUInit()` — see the state and config section
  below for why that is where host-provided data goes. A compile-time array
  would have been worse: it would put the size of `YCPUState` behind a macro,
  and a single-header library whose layout depends on a define is a mismatch
  waiting to happen between `libyax86_core.a` and whatever includes it.
- What changes over a CPU's life is not the storage but whether the CPU may use
  it, which is `YCPUState.decode_cache` — derived by `YCPUInit()` and by
  `CPUSetDecodeCacheEnabled()`, which is its sole writer afterwards. A rejected
  count and a cache withdrawn for a watchpoint both leave it NULL, so the fetch
  tests one thing rather than two.
- `YCPUInvalidateDecodeCache()` walks `config.decode_cache` rather than the
  derived pointer, so entries are discarded even while the cache is withdrawn.
  A host that hands it back must not find stale decodes waiting in it, and
  `InvalidatingReachesEntriesWhileTheCacheIsAway` is what says so.
- The count must be a power of two of at least two, because the index is a
  mask — a remainder by a runtime value is a division, which this target has no
  instruction for. A bad count is logged and the CPU runs without a cache.
- **The fetch tests two guards and both earn their place**, because the two
  ways there is no cache are recorded in different places: the config pointer
  is the host's and may be cleared at any time, and the mask is zero where
  `YCPUInit()` rejected the count. Dropping the second measures **1.9% slower at
  `-O3`** — one of the clearer examples here of an arrangement that reads
  cheaper and is not. It is also why the minimum count is two rather than one.
- **A decode records what the instruction costs as well as what it is.**
  `YCPUInstruction.base_cycles` is the opcode's base cost plus the effective
  address computation, both of which depend only on the encoding, so a hit
  charges the clock from a field instead of reading a table and re-deriving the
  addressing mode. An on-target profile put those two terms at 2.5% of the run;
  caching them is worth **2.83% at `-O3`** and 3.13% at `-O2`. It rides in the
  `YCPUInstruction` rather than beside it in the entry, so the cost is kept
  exactly where the decode it belongs to is kept, and
  `AHitChargesForTheAddressItComputes` and `ARunStopsAtTheBudget` are the two
  tests that catch it going stale.
- **The base cost is settled inside `YCPUFetchNextInstruction()`, and where it
  is read from decides whether one table or two pays.** `YCPUOpcodeMetadata` has
  two bytes of padding, so `base_cycles` lives there for free and there is no
  second table — but only because the decoder reads it through the `metadata`
  pointer it already holds. Reading it from the caller instead, by indexing
  `YCPUOpcodeTable[opcode].base_cycles`, measures **0.63% slower at `-O3` and
  0.65% at `-O2`**: it is one index either way, but a 1-byte field at an 8-byte
  stride spreads 256 costs over 2KB where a packed array keeps them in a couple
  of XIP lines. Caching the cost is what made that read cold — it happens once
  per decode now rather than once per instruction, so it no longer follows the
  decoder through the table and cannot rely on the line being warm.
- **The fetch and the run loop carry a `YCPUDecodeCacheEntry*`, not the
  `YCPUInstruction*` inside it**, and that is measured rather than left over
  from when the cost lived in the entry: `YCPUTick()` reads nothing from an
  entry but `->instruction`, yet handing back the instruction instead costs
  **0.68% at `-O3`** and 0.10% at `-O2`. What matters is the type of the pointer
  carried across the fetch boundary and the loop. One entry pointer addresses
  the key fields at offsets 0, 22 and 23 and every instruction field at 4
  through 21, all within the 5-bit immediate a Cortex-M0+ encodes in a load.
  Handing back the `YCPUInstruction` makes the inlined probe at the bottom of
  the run loop hold the entry for its hit test *and* `entry->instruction` to
  carry across the back-edge and the `YCPUExecuteDecodedInstruction()` call -
  two live pointers where there was one, in a function with none to spare.
  `YCPUTick()`'s frame goes from 52 bytes to 60 and the loop picks up a spill
  and a reload, while the function gets 4 bytes *smaller*: it is register
  pressure paid per instruction, not code size.
- **That was worth disassembling rather than accepting.** `nm` says the two
  builds have the same symbols at the same addresses with the same SRAM and
  flash placement, so the usual suspect - a change moving an inlining decision
  somewhere else - is ruled out, and the cost is local to `YCPUTick()`. Three
  ways of writing it with a `YCPUInstruction*` (the scratch left as an entry,
  the scratch made a bare `YCPUInstruction`, and a `const` local re-derived each
  iteration) all land within 0.005% of each other and 0.68% behind, which is
  what says the shape rather than the spelling is what costs.
- `YCPUInstruction.base_cycles` is `uint16_t` though 46 is the largest value it
  can hold. A byte measures **0.51% worse at `-O3`** and 0.55% at `-O2`, and
  buys no space: `address` leads `YCPUDecodeCacheEntry` so that an 18-byte
  `YCPUInstruction` still packs into 24 bytes, and a 17-byte one rounds up to
  the same 24.
- **The fetch has exactly one call to the decoder**, which is load-bearing
  rather than tidiness. The obvious shape — an early return for the no-cache
  path, a second `YCPUFetchNextInstruction()` below it — gives `-O2` two call
  sites, at which point it stops inlining the decoder and puts it in flash
  behind a veneer. That measures **2.54% slower at `-O2`**, where `-O3` is
  indifferent between the two (0.01%, inside the noise floor). It is the
  `Y_ALWAYS_INLINE` case from the placement section, met by removing the
  second call site rather than by a mark - and it is why the no-cache path
  joins the cached one at the decode instead of returning early, which is the
  one piece of this function that does not read as the simplest thing.
- The platform spends **256 entries, 6KB**, which is not the fastest
  arrangement measured. 512 is 0.11% faster for another 6KB, 1024 is 0.04%
  slower, and 128 is 0.83% slower. 0.11% does not buy 6KB on this part: at 256
  the image leaves room for the 192KB guest RAM option, which 512 would make
  very tight.
- **`code_page_generation` is the whole invalidation mechanism.** One wrapping
  byte per 4KB page; an entry records what its page stood at and is discarded
  once the two disagree. The CPU bumps it for every write it makes itself, in
  `YCPUWriteRawMemoryByte()`, which is where operands, stack pushes and
  interrupt vector writes all end up — so a host only has to report the writes
  it makes some *other* way. The platform reports those from
  `YPlatformWriteMemoryByte()`, which is the path DMA takes. Without that call
  DOS loads itself over the boot sector unseen, which the first prototype did:
  13,247,281 instructions retired against the 2.3 million the workload runs.
- The counter coming back round is the case that has no honest local answer. At
  256 writes to a page a decode taken exactly that long ago looks current
  again, so the wrap discards the whole cache instead — a 256-iteration loop
  about 2,100 times over the run, which is nothing.
- **What is not kept is as load-bearing as what is.** An instruction whose
  later bytes are on the next page, or which ran off the end of the segment, is
  keyed on a page that says nothing about those bytes, so it is not cached at
  all. Staying inside the page is also what rules out an instruction wrapping
  the top of the address space.
- A decode that fails partway has already written the prefix fields into the
  entry, so the entry stops claiming to hold anything *before* the decode
  starts rather than after it succeeds. Otherwise it goes on offering the
  address it used to hold alongside the wreckage of a different instruction —
  and the address tag would not catch it, because the address is the field the
  failed decode leaves alone.
- The cache is handed over once at init and never taken away. What does discard
  every decode is `YPlatformUpdateAfterMemoryMapChange()`, because a
  registration writes nothing — what changed is which region owns the address,
  which no counter can express.
- **The 8088 hardware suite supplies a cache**, for the same reason it supplies
  the two windows. Each of its tests runs one instruction from a cold CPU, so
  nothing there can be a hit: what three million encodings check is the fill.
  The hit is covered by `core/tests/cpu/decode_cache_test.cpp` and by the
  `dos-boot` invariant, where most instructions are hits and any staleness
  moves it loudly. Of eleven deliberate breakages, **ten are caught by the two
  new test files and by nothing else**; only dropping the address tag is caught
  broadly, by six platform tests as well.
- `YCPUWriteRawMemoryByte()` is out of line in flash at both levels and now
  carries the generation bump. `Y_HOT` on it is **0.71% worse at `-O3`** and
  0.68% better at `-O2`, so it is left unmarked: same shape as
  `YCPUReadImmediateOperand()`, and `-O3` is the level that decides.

### cpu — the direct data window

- Operand, stack and interrupt-vector accesses read and write guest memory by
  indexing `YCPUState.direct_data_window` rather than calling into the host.
  What that replaces is two calls, one of them indirect, plus a memory map
  lookup — `YCPUReadRawMemoryByte()` tested a function pointer and called
  through it, the platform's callback chased `config->context`, and
  `YPlatformReadMemoryByte()` resolved the address through the page index to
  reach an array. Worth **3.28% at `-O3`** and 5.78% at `-O2`, and the core is
  552 and 712 bytes *smaller*.
- It is the same trick as the instruction fetch window and covers everything
  that window does not, because `YCPUReadRawMemoryByte()` and
  `YCPUWriteRawMemoryByte()` are also where stack pushes and pops, the IVT reads
  on interrupt dispatch, and the fetch fallback all end up.
- **The window is set, not asked for.** The fetch window is pulled through a
  callback because fetch is sequential and one window answers a whole run of
  it; data accesses are scattered, so there is no locality for a per-access
  callback to exploit and the host calls `YCPUSetDirectDataWindow()` once
  instead.
- **`end` is how far guest memory is directly indexable, not how much guest
  memory there is.** The two coincide today because conventional memory is one
  host buffer, and they need not: a host whose memory is not all directly
  addressable — part of it behind a bus, or reached through a driver — hands
  over the part that is and serves the rest through the callbacks and the
  memory map, which already expresses a callback-backed region. Do not
  reintroduce the assumption that the window is all of RAM.
- `end` is exclusive, as the fetch window's is — the two are the same kind of
  thing and are spelled the same way. A `YPlatformMemoryMapEntry`'s `end` is the
  *last* address in its region rather than one past it, which is why the
  platform converts with a `+ 1` where it derives one from the other.
- The window carries no `start` to go with `end`, because it always begins at
  0, which is where guest RAM begins. That is what makes the bounds test a
  single unsigned compare; a pair would cost a second one and buy nothing.
- `end` is 0 exactly when `data` is NULL, which is what lets that compare stand
  alone rather than being a compare plus a null check.
  `YCPUSetDirectDataWindow()` is what keeps the two in step, so set the window
  through it rather than assigning the fields. An inclusive `end` could not
  express an empty window at all — it would need to be below zero — so this is
  load-bearing rather than a matter of taste.
- The platform derives the window from whichever memory map entry covers
  address 0, and only while that entry is plain storage whose `read_data` and
  `write_data` are the same buffer. Video memory reads from a buffer but writes
  through `YPlatformVideoCallbackWriteVRAMByte()`, so a region shaped like that
  is correctly excluded rather than having its writes swallowed.
- `YPlatformRegisterMemoryMapEntry()` recomputes the window rather than
  discarding it, since a new region usually has nothing to do with address 0.
  Writes through the same buffer need no call at all: the window is a pointer
  into the host's own storage, so a write the host makes itself, or one made by
  DMA, is visible to the next access.
- A word straddling the top of the window has its two halves take different
  paths, because a word is read and written as two independently bounds-checked
  bytes. `WordStraddlingTheTopOfTheWindow` pins it; it is one address in the
  machine and nothing else would reach it.
- **The 8088 hardware suite supplies a window**, for the same reason it
  supplies a fetch window: anything gated on a host-supplied capability is
  unreachable in the suite unless the suite supplies it, and this suite is the
  strongest check the operand path has. Sabotaging the direct read fails it
  within one opcode. The path taken when a host supplies no window is what
  `CPUTestHelper` and the mock configs in `cpu_test.cpp` exercise.

### cpu — prefix decoding

- Both prefix groups are contiguous encoding families, so each is identified by
  one masked compare: segment overrides encode as `001ss110` and LOCK/REP as
  `111100rr`. The bits each mask leaves free are the selector — `ss` is in the
  8086's sreg order, which is the order `Y_CPU_ES` through `Y_CPU_DS` are
  numbered in, so the segment register index is an offset from `Y_CPU_ES`.
- Testing for a prefix this way rather than scanning an eight-element array is
  worth 5–8% of host instructions below `gcc -O3` and `clang -O2`, and nothing
  at or above them, where both compilers derive their own equivalent — `clang`
  picks a 64-bit bitmap and a `bt`, which is arguably better than the mask.
- Prefixes are decoded into fields on `YCPUInstruction` as they are fetched,
  rather than kept as raw bytes for each consumer to walk. `segment_override`
  uses 0 (`Y_CPU_NO_SEGMENT_OVERRIDE`) for absent, which is `Y_CPU_AX` and never
  a segment, so a zero-initialized `YCPUInstruction` is correct by construction.
  LOCK is consumed but not recorded — nothing on a PC/XT acts on it.
- **Classify and record in one pass.** Deciding whether a byte is a prefix and
  deciding which group it belongs to are the same test, so
  `YCPUApplyPrefixByte()` does both and returns whether it consumed anything,
  rather than the loop asking first and the recording asking again. Splitting
  them costs 0.7% on `clang`, enough on its own to make the masked compare a net
  regression there at `-O2` and above.
- Write the `0xF0`–`0xF3` test as a range check rather than a mask. Both are
  exact, but a range folds into one subtract and compare where the mask needs a
  separate `mov`, `and` and `cmp` — which is how `gcc -O3` writes it when left
  to itself. Put it before the segment override test: bytes reaching here are
  usually not prefixes at all and so run both, making the combined cost what
  matters.
- `Y_CPU_MAX_PREFIX_BYTES` is not a storage bound — nothing is stored per prefix
  and the count is a local in the fetch loop. It exists because a run of prefix
  bytes would otherwise fetch forever: a real 8086 hangs there too, since a
  prefix is not an instruction boundary and so no interrupt is ever recognized,
  but this has to hand control back to its caller. `YCPUInstruction.size` is a
  `uint8_t` that `YCPUTick()` adds to IP, so the bound is what that can still
  address — 247 prefixes plus the 8 bytes an instruction can otherwise carry
  comes to exactly 255. It has to be this generous: `LOCK ES: REP MOVSB` is a
  legal three-prefix encoding.

### cpu — the execute path

- `YCPUExecuteInstruction()` checks an instruction against the opcode table
  before running it: that `has_mod_rm` and `immediate_size` are what the entry
  says they should be. `YCPUTick()` skips those checks and calls
  `YCPUExecuteDecodedInstruction()` directly, because its own
  `YCPUFetchNextInstruction()` *derived* both from that same entry a few lines
  earlier. Re-establishing something that cannot have changed in between is not
  free — a table read, two compares and a recomputed immediate size on every
  instruction the emulator runs.
- The public entry point keeps the checks, for a caller that built an
  `YCPUInstruction` by hand rather than decoding one. Nothing inside the core
  calls it, so the duplication costs nothing.
- **Every entry in the opcode table has a handler**, so neither path checks for
  one before dispatching. The eight bytes that have no instruction of their own
  — the four segment overrides and `0xF0`–`0xF3` — are exactly the bytes
  `YCPUApplyPrefixByte()` consumes, so a successful fetch can never leave one in
  `opcode`; they point at `YCPUExecuteInvalidOpcode()`, which returns
  `Y_CPU_INSTRUCTION_INVALID` for the hand-built instruction that is the only
  way to reach one. `EveryEntryHasAHandler` in `opcode_table_test.cpp` keeps it
  that way — an entry added later with the field left out would be a call
  through null rather than an invalid instruction.
- `YCPUExecuteDecodedInstruction()` is `Y_NOINLINE`. Inlined into
  `YCPUTick()` it measures 31% slower on a Cortex-M0+: the execute path wants
  registers, the core has few, and folding the two together makes both spill.
  The effect only exists once the hot path is in SRAM — running from flash, XIP
  misses dominate and hide it — which is why this and `Y_HOT` belong
  together. Worth 1.84% at `-O3` and 4.58% at `-O2`, the gap being the
  `YCPUTick()`/`YPlatformTick()` marks, which are inert at `-O3`.

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
  the image entirely — only `YCPUReadMemoryOperandWord()` is still emitted — and
  the core is **1,208 bytes smaller at `-O3`**, against 48 bytes of table.
- Before concluding a dispatch table has callers that need it, check whether
  they are really indexing. The one apparent external caller here was two sites
  in `instructions_mov.c` reading a 16-bit offset with
  `kReadImmediateValueFn[Y_CPU_WORD]` — a constant index, which is a direct call
  to `YCPUReadImmediateOperandWord()` written the long way.
- **Switch on the width, but not on the operand address type.** The two look
  like the same dispatch and are not. `YCPUWidth width : 1` is a one-bit
  bitfield, so there is no representable value outside the enum: the compiler
  proves the `default` arm unreachable and emits nothing for it, which is what
  makes the explicit invalid return free and lets these read like
  `YCPUReadRegisterOperandValue()` and `YCPUWriteMemoryOperand()` rather than as
  bare ternaries. `YCPUOperandAddress.type` is
  a whole `YCPUOperandAddressType`, and while `arm-none-eabi` defaults to
  `-fshort-enums` and so gives it one byte, 254 of that byte's 256 values are
  outside the enum and C does not promise a variable holds only named ones. A
  `switch` on it emits a live third branch where an `if`/`else` emits two: on
  the *memory* path — the common one — that is an extra `cmp`/`bne` plus the
  constant load for the default, where the one-bit `YCPUWidth` switch compiles
  to a single `lsls`/`bmi` bit test. Switching the type as well costs **1.06% at
  `-O3`** and 0.70% at `-O2`. There is a comment at `YCPUReadOperandValue()` and
  `YCPUWriteOperandAddress()` saying so.
- **The distinction is bitfield width, not enum size** — do not restate it as
  the type being "int-sized", which it is not here.
- Narrowing `type` to `YCPUOperandAddressType type : 1` does make its switch
  free, and still does not pay: **0.22% worse at `-O3`** and 0.10% at `-O2`, for
  a larger core. The read is only half the story —
  `YCPUGetRegisterOrMemoryOperandAddress()` *writes* `address.type` for every
  operand, and a write to a bitfield is a read-modify-write. Cheapening a
  dispatch that runs once per operand does not pay for a store that also runs
  once per operand. So do not "tidy" the `if` into a `switch` for symmetry, and
  do not narrow the field to enable it; both were built and measured.
- `YCPUOpcodeMetadata` holds `base_cycles`, `has_modrm`, `immediate_size` and
  `width` as full bytes without bitfields, and omits the redundant `opcode`
  field (since the table is indexed by opcode). On a 32-bit target, the four
  single-byte fields precede the 4-byte handler pointer, giving exactly 8 bytes
  per entry with zero padding and a power-of-two 2,048-byte table size. Every
  metadata field is read with a single aligned `ldrb` instruction without bit
  shifts or masking.
- `-Os` loses 1.00% to the tables, and the cause is inlining rather than the
  `default` arm — marking all six wrappers `Y_ALWAYS_INLINE` recovers it,
  at 0.26% and 0.06% off `-O3` and `-O2`. The marks are not taken: `-O3` and
  `-O2` are what this target is measured at.
- `YCPUReadImmediateOperand()` is the one wrapper the compiler still emits out
  of line, and it lands in flash. `Y_HOT` moves it to SRAM and is worth
  0.11% at `-O3`, but costs 0.35% at `-O2` by moving other inlining decisions,
  so it is left unmarked.

### cpu — operand values

- **An operand value is a number, not a struct.** `YCPUOperandValue` is a
  `uint16_t`, and every read, write, immediate and port helper takes and
  returns one by value. It used to be a `YCPUWidth` beside a union of a byte and
  a word, and what that cost was not the arithmetic — it was that a struct on
  this path is a struct *in memory*. Worth **23.15% at `-O3`** and 9.72% at
  `-O2`, and the core is 3,236 bytes smaller at `-O3` and 1,972 at `-O2`. It is
  the largest single result in the campaign by a wide margin.
- The mechanism is the ABI, and it is worth knowing exactly. AAPCS returns a
  composite of four bytes or fewer in a register and anything larger through
  memory, so a 10-byte `YCPUOperand` was returned through the stack. Worse, on
  Thumb-1 GCC turns a small copy it cannot do in aligned words into a **call to
  `memcpy`** — including a four-byte one, when the destination is a field at an
  odd halfword offset inside a larger struct. Reading one register operand came
  out as `memset(10)`, `memcpy(4)` and `memcpy(10)`, three calls through a
  veneer into flash to fetch a register.
- **An on-target profile is what found it**, and it found it by name:
  `memcpy`, `memset` and their veneers were 5.4% of the whole `dos-boot` run,
  and every hot-path call site of either was one of the three operand readers.
  Nothing about the source says "this calls into libc"; `nm` and `objdump` did.
  When a structure on this path is suspect, look at the call sites of
  `__wrap_memcpy` before theorizing.
- **A byte-wide value is held in the low byte with the high byte zero.** That
  invariant is what lets `YCPUFromOperandValue()` widen by doing nothing, where
  it used to switch on the width the value carried. Every path that produces a
  byte truncates to one, and the one that has to work for it is
  `YCPUReadRegisterOperandByte()`, where AH and the high half of a word register
  live in the bits the cast drops. Eight unit tests across `add_sub`, `group_2`
  and `mov_xchg_xlat` fail if that cast goes, and so does the hardware suite.
- **The unreachable arm of a width dispatch returns
  `Y_CPU_UNREACHABLE_OPERAND_VALUE`, which is `0xFF`.** `YCPUWidth` names two
  values and every switch handles both, so nothing reaches those arms; they
  exist because C does not promise an enum object holds only named values. All
  ones in the low byte is what an 8086 reads from an address nothing drives, and
  it is legal at either width, where `0xFFFF` is not - at `Y_CPU_BYTE` it would
  break the zero high byte the invariant above promises. The arms cost nothing:
  naming them, and bringing the one that said `0xFFFF` into line, leaves core
  `.text` unchanged at both levels.
- What the type no longer carries is the width, so **sign extension takes one
  explicitly** — `YCPUFromSignedOperandValue(width, value)` and
  `YCPUFromSignedOperand(width, operand)`, from the opcode table entry the
  caller already has. Zero extension needs none, by the invariant above.
- **Do not initialize an operand with a designated initializer.** `YCPUOperand
  operand = {.address = {...}}` makes C zero every member the initializer does
  not name, which is a `memset` call over the whole struct before a single
  field is written. Assigning the fields one at a time instead is **3.46% at
  `-O3` on its own**, from that one function. Same lesson as the decode, where
  settling each field where it becomes known rather than zeroing up front was
  worth 4.63%.
- **`YCPUOperandAddress` is flat, four bytes, and that is a size rather than a
  taste.** AAPCS returns a composite of four bytes or fewer in a register and
  anything larger through memory, so four is the line a returned struct has to
  stay under. A tagged union of `YCPURegisterAddress` and `YCPUMemoryAddress`
  cost six
  - a byte for the tag, a byte of padding, four for the larger arm - and the
  union was the wrong tool anyway, because **both arms are the same shape**: a
  register index and a small number beside it. Sharing the fields costs four.
  `register_index` is the register for a register operand and the segment
  register for a memory one; `offset` is the byte within the register, 0 or 8,
  or the effective address within the segment.
- **Three ways to stop copying it were built and measured, and the cheapest
  reading is the fastest.** Against master, on `dos-boot`:

  | | `-O3` | `-O2` |
  | --- | --- | --- |
  | passing the address by pointer | +11.07% | +6.68% |
  | shrinking it to four bytes | **+15.67%** | **+22.67%** |
  | both together | +8.83% | +21.16% |

  **Doing both is worse than shrinking alone at both levels.** Once the struct
  fits in a register, an out-parameter forces a store where a return would have
  kept it in `r0`, so the two changes fight. Prefer making a hot struct fit the
  return register over routing it through memory, and measure them separately
  rather than assuming they add.
- Afterwards the core contains **no call to `memcpy` or `memset` at all**; the
  two left in the image are TinyUSB's. Worth re-checking after anything that
  changes how operands are passed, and it is a `grep` rather than a judgement:
  ```sh
  arm-none-eabi-objdump -d build-pico/O3/yax86_pico_bench.elf |
      grep -cP 'bl\t2000....\s<____wrap_mem'
  ```
- `YCPUMemoryAddress.segment_register_index` is a `uint8_t` rather than a
  `YCPURegisterIndex`, because `YCPUApplySegmentOverride()` writes it through a
  pointer. An enum's width is the target's business - one byte under
  `-fshort-enums`, four otherwise - so a `uint8_t*` to one is correct only by
  accident of endianness, and the native test build is exactly the target where
  it is four.
- **`YCPUOperand` is handed back through a pointer, because it cannot be shrunk
  under the return line.** At six bytes - a four byte address and a 16-bit
  value - it is already past four before the tag, so the trick that worked for
  `YCPUOperandAddress` is unavailable and an out-parameter is the only move. The
  seven readers take a `YCPUOperand*` as their last argument.
- **That only pays alongside `Y_ALWAYS_INLINE` on `YCPUReadOperandValue()`,
  and on its own it is a large loss.** Measured against the flat-address
  baseline:

  | | `-O3` | `-O2` |
  | --- | --- | --- |
  | the out-parameter alone | **-5.46%** | -1.07% |
  | the inline mark alone | +0.11% | +1.98% |
  | both | **+0.97%** | **+4.15%** |

- The reason is the inlining lottery rather than anything about the copy, and
  the tell is in what the function calls. Returning by value,
  `YCPUReadRegisterOrMemoryOperand()` calls `YCPUReadMemoryOperandWord()` and
  `YPlatformReadMemoryByte()` - `YCPUReadOperandValue()` having been inlined
  into it, both arms landing in SRAM. With the out-parameter it is small enough
  that GCC stops inlining `YCPUReadOperandValue()` and calls it instead, and
  that one lives in flash, so every operand read grew a veneer and an XIP fetch.
  **Check what a changed function calls, not just whether it got smaller** - the
  out-parameter version is 94 bytes against 244 and was the slower of the two.
- The marked `YCPUReadOperandValue()` costs 608 bytes of core `.text` at `-O3`
  and 624 at `-O2`, which is the price of the win above.

### cpu — what an instruction costs

- The cycle model in `cycles.c` is a base cost per opcode, plus the effective
  address calculation, plus four cycles for every byte the instruction moves
  over the 8088's 8-bit data bus. The first two are settled by the encoding, so
  a decode works them out once into `YCPUInstruction.base_cycles`; the third is
  charged as the instruction runs. The third term dominates, and it is charged
  from the accesses that actually happen rather than from a table — so an
  instruction that touches memory it has no reason to touch is billed for it.
- Stores must therefore resolve their destination with
  `YCPUGetRegisterOrMemoryOperandAddress()` and go straight to
  `YCPUWriteOperandAddress()`, never through
  `YCPUReadRegisterOrMemoryOperand()`. Reading a destination that is about to be
  overwritten completely prices a store as a read-modify-write: `MOV [BX],AX`
  costs 23 cycles where the hardware charges 15, and `MOV [BX],AL` 15
  against 11.
- **No architectural test can see this.** The registers and memory come out
  identical either way, so the 8088 hardware suite passes on both — it compares
  results, not bus traffic. `cycles_test.cpp` is what covers it: it runs one
  instruction through `YCPUTick()` and asserts `cycles_this_tick`, pinning a
  store against the load and the read-modify-write of the same shape.
- **Any change to cycle costs moves the `dos-boot` invariant**, so runs either
  side of one are not comparable. Cheaper stores do not mean the guest does
  less work — its timer-polling loops get more turns before the same number of
  PIT ticks elapse, so retired instructions go *up* as emulated cycles go down.

### cpu — bulk string runs

- A `REP MOVS` or `REP STOS` whose every byte lies inside the direct data
  window is carried out as a loop over that window, rather than an element at a
  time through the operand machinery. An iteration otherwise builds an operand
  address, resolves a segment through it, dispatches on width and charges the
  bus, twice over, to move two bytes — and calls through a function pointer to
  do it. Worth **3.10% at `-O3`** on `dos-boot`.
- `REP SCAS` and `REP CMPS` get no bulk path, and not for want of trying: both
  read flags per element and stop on them, so there is no run to plan. `LODS`
  writes a register rather than memory, so repeating it does nothing a single
  iteration with the final count would not.
- **The element path is the definition, and the bulk path has to be
  indistinguishable from it** — same memory, same registers, same flags, same
  cycle count. `string_bulk_test.cpp` runs each case both ways and compares all
  four, which is what makes a divergence a failure rather than a judgement
  call. Of eight deliberate breakages, each is caught by the test aimed at it.
- **A run is declined rather than made to work.** Three things disqualify one,
  and the awkward cases are all on the far side of that line: a count of zero,
  an offset that would wrap the top of its segment, and any byte outside the
  window. What is left is a straight walk over an array.
- The segment wrap has to be tested separately from the window bound, because
  the two coincide on any host whose window happens to end at a segment
  boundary — which is why the tests put the run in a segment based part way up
  memory, where an offset can wrap with every address still inside the window.
- **Overlap needs no special handling, because the loops walk elements in the
  order the element path visits them.** A forward `REP MOVSB` from `SI` to
  `SI+1` propagates one byte through the whole range on real hardware, and a
  loop that copies in the same order reproduces that for free. What would break
  it is `memcpy`, which may copy in any order — see below.
- The word loop reads both bytes of an element before writing either, which is
  the order `YCPUReadMemoryOperandWord()` and `YCPUWriteMemoryOperandWord()`
  impose. It only shows up where the two ends overlap by exactly one byte, which
  is one test and nothing else.
- **All four loops walk pointers rather than indexing an array**, and that is
  what keeps the compiler from rewriting them. Written as `memory[index]`,
  `gcc -O3` recognizes the byte-wide `STOS` loop and emits a call to
  `memset()` — on this target the SDK's assembly one, in flash behind a veneer.
  It **measures the same**, 4.779637s against 4.779589s, so what the index form
  costs is not speed but a link-time dependency on libc the core does not
  otherwise have.
- The copy loops are out of reach of the same transform for a second reason:
  the step is a runtime value the compiler cannot prove positive, so it cannot
  conclude the two ranges do not overlap. That one is load-bearing rather than
  lucky — specializing a loop on a forward direction would hand it exactly the
  proof it needs, and a `memcpy()` may copy in any order, which is what the
  overlapping cases depend on it not doing.
- **A synthesized `mem*` call is not the same defect as a written one.** The
  check under "operand values" exists to catch a *struct* being copied on the
  operand path, which is where 5.4% of a run once went; a store loop the
  compiler recognized is a different thing, and whether it is worth avoiding is
  a question about libc dependence rather than about speed.
- Each instruction's bulk run lives in its own function beside the iteration it
  replaces, so the handler reads as the two paths it chooses between. Both are
  inlined into their caller at `-O3`, and `core .text` is identical either way.
- **`YCPUPlanBulkStringRun()` is out of line in flash, and is left that way.**
  It has two callers, which is the shape that usually stops GCC inlining a
  helper and costs a veneer on the hot path — but both marks that would change
  it measure worse or neutral: `Y_HOT` is **0.00%** for 296 bytes of SRAM,
  and `Y_ALWAYS_INLINE` is **0.73% slower** while making the core 216 bytes
  *smaller*, having grown both handlers enough to move the register allocation
  around them.
- The reason the mark buys nothing is that the planner runs **once per repeat,
  not once per element** — it is on the hot path by name and not by traffic,
  which is the distinction the placement section draws. A repeat that moves 64
  bytes amortizes one flash fetch over the whole run.
- **The run reports one write per 4KB page to the decode cache, not one per
  byte.** A page's generation only has to *change* for a decode taken from it
  to be discarded, so one report says the same thing — and it leaves the
  counter coming back round after 256 runs rather than after 256 bytes, which
  is a full flush avoided rather than a flush missed.
- **That is what moved the invariant, and none of it is emulation.** Fewer
  flushes mean more decode-cache hits, a run ends at a miss, so `YPlatformRun()`
  overshoots its budget by a different amount and the Enter answering DOS's
  date prompt lands at a different emulated time. Building both sides with
  `Y_CPU_MAX_INSTRUCTIONS_PER_TICK` set to 1 gives identical figures —
  2,328,015 / 27,701,507, the same pair #73 recorded — which is what says the
  emulation is unchanged. Reporting per byte instead restores the old invariant
  exactly and costs 0.13%.

### cpu — the interrupt request hint

- `YCPUConfig.interrupt_request_hint` is an optional `const bool*` the CPU reads
  at every instruction boundary with interrupts enabled, and skips the
  acknowledge cycle entirely while it reads false. What that replaces is an
  indirect call through `acknowledge_interrupt` into a callback that chases
  `config->context` and then calls `YPICGetPendingInterrupt()`, on nearly every
  instruction, to be told there is nothing. Worth **5.97% at `-O3`** and 4.17%
  at `-O2` — the largest single win in the campaign so far.
- The flag it points at is `YPICState.has_unmasked_request`, which is
  `(irr & ~imr) != 0`. **It is deliberately conservative rather than exact**: it
  ignores priority and the in-service register, so it is true in cases where
  `YPICGetPendingInterrupt()` still declines — a request held off by a
  higher-priority interrupt already in service is the one that happens. That
  costs a call that reports nothing, which is what the code did every time
  before.
- **What it may never be is falsely false**, because the CPU takes it as
  permission not to ask at all. A hint stuck false strands the interrupt
  silently and indefinitely, and nothing else in the machine would notice.
  `HintReadingFalseSuppressesTheAcknowledgeCycle` in `tick_test.cpp` pins
  exactly that, so the contract is stated as a test rather than only as a
  comment.
- `YPICUpdateUnmaskedRequest()` is the **sole writer** of the flag, and all six
  sites that change `irr` or `imr` call it — `YPICInit()`, `YPICRaiseIRQ()`,
  `YPICLowerIRQ()`, ICW1, OCW1, and the acknowledge in
  `YPICGetPendingInterrupt()`. Same discipline as
  `PlatformUpdateEnabledFlags()`: the flag cannot disagree with the registers
  it summarizes because no path changes one without recomputing the other.
- **The hint is a pointer into the PIC's own state, not a value handed over.**
  That is what makes it free of synchronization — a change the PIC makes is
  visible to the next instruction with nothing to invalidate, the same property
  the two memory windows have. A host that pushed a copy would need every PIC
  mutation to also poke the CPU, which is the failure mode the sole-writer rule
  exists to prevent, moved somewhere harder to see.
- **Dropping a recompute after the acknowledge cannot produce wrong behaviour**
  — the flag just stays conservatively true and the CPU goes back to asking
  every instruction — so no architectural or platform test can catch it. It is
  a pure performance regression, and
  `core/tests/pic/interrupt_request_hint_test.cpp` is what covers it, in the
  same way `cycles_test.cpp` covers store costs. Each of its cases compares the
  flag against `irr & ~imr` rather than against a literal, so a case added
  later cannot pin the wrong one.
- The PIC's unit tests set the mask through an OCW1 port write rather than
  assigning `imr`. Assigning it works and leaves the flag stale, which is
  invisible in the registers — going through the port is both what a guest does
  and what keeps the tests honest.
- A slave's request reaches the CPU as the master's cascade line, and the CPU
  only ever reads the master's flag, so the master's has to rise with it.
  `YPICRaiseIRQ()` on a slave already raises the cascade IRQ on the master,
  which recomputes the master's flag by the same path as any other request.
- A host that cannot promise the flag rises with every request leaves the
  pointer NULL and is asked every time, exactly as before. The mock configs in
  `cpu_test.cpp` and `CPUTestHelper` do, which is what exercises that path.

### cpu — runs of instructions

- A tick driven through `YPlatformRun()` executes up to
  `Y_CPU_MAX_INSTRUCTIONS_PER_TICK` instructions back to back. What that removes
  is not the instruction, it is the round trip — `YPlatformTick()`'s deadline
  test and stop checks, and `YCPUTick()`'s own prologue and epilogue, all of
  which are per tick. Worth **9.17% at `-O3`** and 9.74% at `-O2`, which is the
  largest win left after the decode cache and is only possible because of it.
- **Only the first instruction of a run is fetched**; every one after it comes
  from the decode cache, so a step in a run costs a probe rather than a decode.
  That is also what keeps a cold CPU to one instruction per tick however large
  a budget it is handed, which is what lets the 8088 hardware suite supply a
  budget and stay a single-instruction test.
- A run is not limited to straight-line code. The next instruction is looked up
  at CS:IP, wherever the one before it left that, so a backwards jump into
  cached code carries on inside the same tick.
- **`YCPUTick()`'s `max_run_cycles` is how long a tick may run**, and it is the
  switch as well as the bound: **zero runs exactly one instruction**.
  `YPlatformTick()` passes zero because it promises its caller one instruction;
  `YPlatformRun()` passes what is left before the nearest device deadline. Both
  call the same body with a constant, so the tests fold away in each.
- It is a parameter rather than a field on `YCPUState` because it is set on
  every tick. Config is for what a host wires up once and `YCPUInit()` checks; a
  value that changes every call is an argument, and writing it into the state
  first would be poking the object to pass a parameter.
- **`Y_CPU_MAX_INSTRUCTIONS_PER_TICK` is a cap on how coarse a tick can be, not
  a tuning knob for how long a run should be.** What governs that is
  `max_run_cycles`, which is what keeps a device from being serviced late; the
  instruction count only bounds the case where the next deadline is far away,
  and what it costs is that `YPlatformRun()` overshoots its budget by up to one
  run.
- It was 4, chosen from straight-line runs averaging 3.65 instructions, and
  that was the wrong basis: a run also carries on through a taken branch into
  cached code, so runs are longer than straight-line stretches. Measured on
  `dos-boot` in emulated MHz, which normalizes for each bound producing its own
  invariant: 2 gives 5.349, 4 gives 5.554, 8 gives 5.657, 16 gives 5.719 and 32
  gives 5.746. **16 is where the curve stops paying** — it is worth 2.9% over
  4, where 32 adds 0.5% for twice the overshoot.
- **A run ends at everything the end of a tick would otherwise have dealt
  with**, and each of those is a point where running one more instruction first
  would act too late: the budget, a self-raised interrupt
  (`has_pending_internal_interrupt` — `INT n`, `INTO`, a divide error), a
  requested stop, `HLT`, TF, and the PIC's interrupt request hint. A host that
  supplies no hint cannot be asked without an acknowledge call, so it ends the
  run rather than guessing.
- **A port access gets a tick to itself**, and needs both halves of that. A run
  ends *after* one because `IN`/`OUT` can reprogram a device and move the
  deadline the budget was computed from. A run also declines to carry on *into*
  one, because a port handler calls `YPlatformSync()` and so is the one
  instruction that looks at the clock — and part way through a run the clock
  stops short of the instructions already run in it, since the host is only
  told what a tick cost once the tick is over. `IN AL, 0x40` two instructions
  into a run reads a PIT count that is two instructions stale.
- Both are one masked compare: `IN` and `OUT` are 0xE4–0xE7 and 0xEC–0xEF,
  which differ only in bit 3, so `(opcode & 0xF4) == 0xE4` matches those eight
  bytes and nothing else. Writing it as two `& 0xF8` compares does not work and
  is not a near miss — `0xE4 & 0xF8` is `0xE0`, so both arms are dead and the
  guard never fires.
- **A host that needs to stop somewhere drives `YPlatformTick()` itself** and
  inspects the CPU between calls. The platform holds no breakpoints and no step
  mode: `YPlatformTick()` already runs exactly one instruction, so a stepping
  loop is a host loop, and a host comparing CS:IP costs the shared code
  nothing.
- **Batching is exactly equivalent to stepping, and there is a test that says
  so.** `ABatchedRunIsIndistinguishableFromSteppingIt` runs one program two
  ways — `YPlatformTick()` per instruction, and `YPlatformRun(platform, 1)`,
  which is one batched tick — and compares the tick count, the retired count
  and the registers exactly. It is a stronger check than the `dos-boot`
  invariant, which is measured through a harness that polls the screen on
  emulated-cycle boundaries and so moves when a batch ends at a different
  cycle. The program accumulates what it reads from the PIT rather than
  discarding it, and puts a port read one instruction into what would be a run;
  without that, alignment happens to place the read first in its tick and the
  test cannot see a stale clock.
- **`dos-boot` moves by 107 instructions and 1,266 cycles, and none of it is
  emulation.** Building the same branch with `Y_CPU_MAX_INSTRUCTIONS_PER_TICK`
  set to 1 reproduces master's 2,328,015 / 27,701,507 exactly. What moves is the
  harness: `YPlatformRun()` overshoots its budget by a run rather than by an
  instruction, so `result.cycles` crosses the screen-poll deadline at a
  different point and the Enter that answers DOS's date prompt is injected at a
  different emulated time. It is the same effect as the poll interval itself,
  which the campaign notes measured at 11,816 instructions.
- **This loop's arrangement has to be measured against the base it lands on,
  not carried across one.** The two locals naming the instruction for the
  invalid-instruction log are live across the whole loop body, and reporting
  them from a `Y_NOINLINE` helper instead — which reconstructs the address
  as `IP` less the instruction's size — was worth **10.7% at `-O2`** against
  the first version of the decode cache. Against the version that landed the
  same pair is a wash: the helper measures 0.12% better at `-O3` and 0.25%
  worse at `-O2`. The locals are kept, because at that margin the simpler code
  wins and the helper's correctness rests on no handler touching CS or IP
  before rejecting an instruction, which nothing enforces. A 10.7% result on
  one base and a 0.2% result on the next is the same inlining lottery the
  campaign measured at ±6%, and it is why a queued figure here is a reason to
  try a change rather than a number to expect.
- Hoisting `executed_instruction` out of the loop measures neutral and
  identical in size — GCC was already doing it.

### cpu — instruction counting

- The count is what the CPU actually ran; a halted tick advances the clock but
  retires nothing. It lives in the CPU because `YCPUTick()` already knows which
  it did, where a caller could only find out by sampling `is_halted` before
  every tick and giving up `YPlatformRun()`'s batching for a number the core
  already has. Counting in `YCPUTick()` costs 0.17% under callgrind; the same
  counter driven from `YPlatformTick()`, where the flag has to be re-derived,
  costs 1.09%.
- **It is kept as two `uint32_t` halves rather than one `uint64_t`**, read
  together by `YCPUInstructionsRetired()`. The increment sits in the hottest
  basic block the emulator has, and a 64-bit one is eight instructions on a
  Cortex-M0+ — two loads, two constants, an add, an add with carry, two stores
  — where the low half alone is three and the carry above it is a branch not
  taken until 2^32 instructions have run. Worth **1.19% at `-O3`**, for 16
  bytes of core `.text`. The count still has to reach 64 bits: a machine left
  running retires 2^32 instructions in a couple of hours.
- **Batching the count per run instead does not pay, and the reason is worth
  keeping.** Folding it in once a tick from what is left of the run budget —
  which removes the 64-bit add from the loop entirely — measures **0.11%**,
  and making the budget a constant so the subtraction has nothing to carry
  measures **0.31% slower**. `dos-boot` runs 2,328,128 instructions in
  1,018,378 ticks, so **a run averages 2.29 instructions**: eight instructions
  saved per instruction against ten paid per tick is a margin that register
  pressure eats. Anything moved from per-instruction to per-tick on this path
  has to clear that ratio, and it is a low bar.
- The carry is unreachable in any run a test could make, so
  `TheRetiredCountCarriesIntoItsHighHalf` starts the count just below the
  boundary rather than driving it up to one.

### platform — the memory map

- `YPlatformGetMemoryMapEntryForAddress()` is the hottest lookup in the emulator
  — every instruction byte and nearly every operand comes through it — and it is
  answered from an index rather than by walking the map. A walk is cheap where
  branches are predicted and the region wanted is usually first; on a
  Cortex-M0+ it is neither, since there is no branch predictor and code running
  from the BIOS ROM misses the first entry every time. The index is worth
  **7.0%** on `dos-boot`.
- `memory_page_map` holds one entry index per 4KB page of the 8086's 1MB, so
  256 bytes of index answers a lookup with a shift, a load and a bounds
  compare. 4KB is not tuned — it is the coarsest page that leaves every region
  the machine registers aligned, and a finer one would only cost more memory.
- Two index values are not entry indices. `Y_PLATFORM_MEMORY_PAGE_UNMAPPED` is a
  page no entry covers, and answers `NULL` directly.
  `Y_PLATFORM_MEMORY_PAGE_STRADDLED` is a page more than one entry has a share
  of, which the index cannot answer at all, so the lookup falls back to a walk.
  Nothing registers a misaligned region today, and that is exactly why the
  fallback needs its own test — see `platform_memory_map_test.cpp`, which
  registers one deliberately.
- A region whose size is not a whole number of pages is handled by construction
  rather than by arithmetic: its last page comes out straddled and takes the
  fallback. That is the case for any option ROM whose size is not a multiple of
  4KB.
- **Registering a region and telling the CPU about it are separate steps.**
  `YPlatformRegisterMemoryMapEntry()` records the entry;
  `YPlatformUpdateAfterMemoryMapChange()` discards what the CPU derives from the
  map. A caller does both, and may do the second once after a run of the first
  - which is what `YPlatformInit()` does for the four regions the machine has.
- **The decode cache is what makes the second call obligatory.** An address
  that is not mapped reads as open bus, so an instruction executed there can be
  cached as 0xFF bytes; mapping a region over it moves no page generation,
  because nothing was written. The two windows are less demanding - entries may
  not overlap, so a newly registered region cannot be inside an open fetch
  window, and the data window is whichever entry covers address 0, which a
  missed update leaves closed rather than wrong.
- That recompute is the last thing `YPlatformInit()` does, by which point every
  region the machine has is registered.
- `YPlatformRegisterMemoryMapEntry()` extends the index rather than rebuilding
  it. Only the pages the new entry touches can change, and entries may not
  overlap, so nothing already in the index can have a share of one of them — a
  registration never has to look at the rest of the map.
  `YPlatformUpdateMemoryPageMapForEntry()` takes the entry's index rather than
  the entry, because the index is what goes into the map; the entry is derivable
  from it and not the other way round.
- An entry reaching past the top of the address space is rejected, because the
  index has a slot per page of that space and none above it. **That rejection
  is what makes the index the whole answer** rather than a cache in front of
  the map: an address above the space is unmapped by definition, so
  `YPlatformGetMemoryPageMapIndex()` says so directly rather than sending the
  lookup off to walk for an entry that cannot be there. Guest addresses never
  reach it in any case — `YCPUToRawAddress()` masks every one to the 8086's 20
  bits — so this is about what a host may register, not what the CPU may ask
  for.

### platform — idle skipping

- MS-DOS waits for a keystroke by polling rather than halting, so an idle
  command prompt costs exactly as much to emulate as a running program. At the
  MS-DOS 3.30 prompt the guest retires about 390,000 instructions a second and
  never once halts, all of it the same loop through the `INT 21h` dispatcher
  and the CON driver into `INT 16h`.
- It does announce itself: DOS issues `INT 28h`, the documented DOS idle
  interrupt, 783 times a second while it waits. `YPlatformSetDOSIdleSkip()`
  makes `YPlatformRun()` treat that as a signal to
  advance the clock to whichever comes first, the next device deadline or the
  end of the budget it was given, instead of executing the loop until it gets
  there. The guest's own handler still runs; only the waiting is skipped.
- **Time is advanced, not discarded.** `YPlatformSync()` runs across the skipped
  interval, so every device sees every cycle and the guest's timer tick count
  is unchanged. Bounding the skip by the caller's budget is what keeps that
  true — without it the machine would be handed more emulated time than was
  asked for and would run its clock fast.
- It is opt in because it is not free of consequence: a program timing a loop
  from inside its own `INT 28h` handler would see time jump. The SDL runtime
  turns it on; the tests leave it off.
- **The skip is switched by a setter, not configured.** It is off after
  `YPlatformInit()`, and `YPlatformSetDOSIdleSkip()` turns it on or off at
  any point after that - the SDL runtime straight after init, the PicoCalc
  runtime once DOS reaches its prompt, because on from power on it would move
  the `dos-boot` invariant: DOS issues `INT 28h` at the date and time prompts
  as well. What the skip takes is the CPU's `handle_interrupt` callback as
  well as a flag, and the setter is the sole writer of both, the coupled state
  rule from the CPU's state and config section. A config field read at init
  would be a second way to set the same thing, and one that silently stops
  working once the machine is up.
- What the skip is worth is capped by the nearest device deadline, which is why
  `YPITTicksUntilNextEvent()` schedules one for channel 0 alone. After POST the
  BIOS leaves channel 2 programmed at a reload of 1356 with the speaker gated
  off, and asking to be woken every 678 ticks for an output nobody listens to
  truncates every skip. Over 20 emulated seconds at the prompt the skip is
  worth 1.4x with the other channels scheduled and 5.6x without.
- Only channel 0's output leaves the PIT — `YPITChannelSetOutputState()` raises
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

### video — host text fonts

- The adapters draw text in 8x8 (CGA) or 9x14 (MDA) cells, 640 or 720 pixels
  across. A display narrower than that cannot show 80 columns by shrinking
  those glyphs, which makes them unreadable, so a host can supply a font drawn
  for a narrower cell - `YVideoConfig.text_font`, or
  `YPlatformConfig.video_text_font` through the platform, since
  `YPlatformInit()` replaces the video config wholesale. All zeros is the
  default and keeps the adapter's font.
- **With a host font the frame size follows the mode.** Text modes render in
  the font's cells, so an 80-column text frame is 80 * width by rows * height
  pixels, and a 40-column cell is drawn twice the font's width to come to the
  same total. Graphics modes keep the adapter's frame. A host asks with
  `YVideoGetFrameSize()` rather than reading `frame_buffer_width` from the
  adapter metadata, which is now only the frame without a font. A mode change
  already forces a full redraw, so a host sees the new size with a region
  covering all of it.
- **Nothing the guest can observe changes.** Emulated timing reads only the
  adapter's `cycles_per_scan_line` and `scan_lines_per_frame`. A mode's
  `char_height` reaches only `YVideoGetDirtyGeometry()`, which is render-side
  and follows the font.
- **The cursor and the MDA underline are placed by scan line of the adapter's
  cell, and are scaled to the font's.** A scan line of the adapter's cell
  covers a span of the font's rows. A range maps from the first row its first
  line covers to the last row its last line covers - rounding the start down
  and the end up. That keeps a range that is not empty in the adapter's cell
  from coming out empty in the font's: the CGA's underline cursor, lines 6-7
  of 8, becomes rows 7-9 of 10, where rounding both ends down would leave the
  bottom row bare. A single line - the underline - takes the start, so the
  MDA's line 12 of 14 lands on row 8 of 10, second from the bottom in both.
- `YVideoInit()` refuses a font wider than a byte or with no height, logs it,
  and draws in the adapter's font. What it records is `has_text_font`, a flag,
  rather than a pointer to the font in the config, so that a copy of the state
  does not point back into the original.
- `core/tests/video/text_font_test.cpp` covers it. Rounding the cursor's end
  down, leaving dirty rows at the adapter's cell height, and taking the region
  column width from the adapter's frame are each caught there.

### video — rendering from a copy

- `YVideoCopyForRender()` is for a host that renders somewhere the machine is
  not running - the PicoCalc renders on its second core. Rendering the live
  state there would read video memory and registers while the CPU writes them,
  and would clear the record of what changed while the machine adds to it.
- It copies the state and the adapter's video memory into the host's buffers
  and moves the record of what changed with them, leaving the live state
  marked up to date. Rendering the copy draws exactly what rendering the live
  state would have, and a change made after the copy is drawn from the next
  one. A host whose renderer is still busy simply does not copy, and the
  changes pile up in the live state until it does.
- **The copy loop keeps its pointers in locals.** Read through
  `video->config.vram`, the pointer is reloaded for every byte, because the
  compiler cannot rule out that `frame_vram` overlaps the state, and the loop
  copies a byte at a time: 46ms over `dos-boot` on the PicoCalc, against
  9.7ms with locals, where GCC copies a word at a time. Copying the state
  itself is a `memcpy()` call the compiler emits, like the `memset()` in
  `YVideoRender()` - once a frame, off the hot path.
- `core/tests/video/copy_for_render_test.cpp` covers it. Leaving the live
  record set, and pointing the copy at the live video memory, are each caught
  there.

### util — logging

- **Everything the core and its hosts log is one format: logfmt**, a line of
  space separated `key=value` pairs, written by `util/logfmt.h` and rendered
  by the logger, so that a host only says where a line goes and never what
  it looks like. The SDL runtime and both Pico targets print the same lines,
  and one set of tools reads all of them.
- **A line's first pair is its kind.** A `log` is something that happened,
  said unprompted, and carries `level`, `module`, `tick`, `cs`, `ip` and
  `event` ahead of its own fields:

  ```
  kind=log level=warn module=platform tick=27702858 cs=0xF000 ip=0xE0A3 event=unmapped_port_read port=0x0201
  ```

  A `reply` answers a command a host was sent - `kind=reply id=7 cmd=screen
  status=ok` - and is never filtered, because whoever sent the command is
  waiting for it. Severity is a property of logs only: a failed command is
  still an answer, `status=error`, not an error the device is reporting
  about itself.
- **Values are typed by how they are written**: decimals, hex with a `0x`
  prefix at a fixed width (`0x0201` for a port, `0x9A` for a byte),
  `true`/`false`, and strings, bare when they can be and quoted otherwise. A
  quote or backslash is escaped with a backslash, and any byte outside
  printable ASCII as `\xNN`, so CP437 text from the guest still makes a line
  that is printable ASCII end to end. Keys and event names are snake_case;
  an event is a name, never a sentence.
- **A call names an event and passes fields**: `Y_PLATFORM_WARN(
  "unmapped_port_read", Y_LOG_HEX16("port", port))`. Each module defines its
  four levels - `error`, `warn`, `info` and `debug` - as macros at the top of
  its source file, and a host defines its own the same way, as the SDL
  runtime's `APP_ERROR()` does. The event is the first variadic argument
  rather than a parameter of its own so that an event with no fields is a
  valid C99 macro call, and the fields are passed to `YLoggerWrite()` by
  value through `...`, ended by `YLogEnd()`, which the macro adds.
- **A filtered log costs only its filter.** `Y_LOG()` tests the module and the
  level before it builds a single field, and
  `DoesNotEvaluateTheFieldsOfASuppressedLog` keeps it that way.
- **But a log written inline in a hot function costs that function even when
  it never runs.** Its fields are 16-byte structs built on the stack and
  copied into the call - with `memcpy()`, on this target - so the frame and
  the code around it grow whether or not the branch is taken. The platform's
  four memory and port accessors therefore call `Y_NOINLINE` helpers,
  `YPlatformLogUnmappedPortRead()` and its siblings, which do the logging.
  Written inline, they took `dos-boot` 0.88% slower than the format-string
  logger before them; out of line, 0.16%, which is what is left of this
  change. Moving `YCPUTick()`'s two logs out of line as well measured 0.06%
  slower again, so those stay where they are. Check what a new log in an
  `Y_HOT` function does to it before leaving it inline.
- **The platform stamps each log with its tick and the CPU's CS:IP**, through
  a `YLoggerSource` it sets once the CPU is up - a host does not wire either.
  `YCPUTick()` moves IP past an instruction before running it, so a log made
  while an instruction runs, as every port and memory log is, gives the
  address of the instruction after it, which is also what an 8086 pushes on
  an interrupt. Keeping the running instruction's own address would be a
  store on every instruction, so a log that needs it, as
  `invalid_instruction` does, passes it as a field.
- **Module IDs 0-15 are the core's and 16-31 the hosts'**
  (`Y_LOG_FIRST_HOST_MODULE_ID`), so a host can give its own messages a
  module without colliding with one the core adds later.
  `module_ids_test.cpp` holds the core to its half.
- A line is at most `Y_LOG_MAX_LINE_LENGTH` bytes, built in the logger's own
  buffer. A pair that does not fit is dropped whole, along with every pair
  after it, and the line ends `truncated=true`, so a truncated line is still
  a valid one.

### util — hot path placement

- `Y_HOT` marks a function as being on the per-instruction hot path. It is
  empty by default, because on a machine with a normal cache hierarchy there is
  nothing useful to say; a target whose fastest memory has to be chosen
  explicitly defines it to whatever placement attribute it needs. The Pico
  harness defines it to `__not_in_flash()`, which puts the function in SRAM
  instead of executing it from QSPI flash through a 16KB XIP cache.
- **86 functions carry it, chosen from an on-target profile rather than by
  intuition.** On the Pico it is worth **1.59x at 400MHz** and 1.23x at 125MHz.
  The win is larger when overclocked because the flash SPI clock does not scale
  with the core, so an XIP miss costs more core cycles the faster the core runs.
- Annotating every function in the core instead was measured and is not worth
  it: the targeted set gives 97.2% of the win for 15KB of SRAM, where all 491
  give 100% for 46.5KB. The extra 2.8% costs three times the memory and
  forecloses the 192KB guest RAM option outright — 206,664 bytes of image plus
  64KB more guest RAM does not fit in 256KB, where the targeted build does.
- `YCPUTick()` and `YPlatformTickInternal()` carry the mark even though at `-O3`
  they are inlined into `YPlatformRun()`, which already did. The mark is on
  `YPlatformTickInternal()` rather than on the public `YPlatformTick()`, because
  that is the body `YPlatformRun()` calls; `YPlatformTick()` is a wrapper
  nothing hot goes through. The mark buys nothing there
  and costs 1.1KB, because an explicit section attribute overrides
  `-ffunction-sections`: all the marked functions share one section, so
  `--gc-sections` can no longer drop the out-of-line copies individually. It is
  worth paying, because at `-O2` neither inlines and without the mark the two
  hottest functions in the emulator execute from flash. Check placement with
  `nm` rather than assuming, since whether they inline depends on the level and
  on how large `YPlatformRun()` has grown:
  ```sh
  arm-none-eabi-nm -nS build-pico/O3/yax86_pico_bench.elf | grep ' YCPUTick$'
  ```
  `0x1000xxxx` is flash and `0x2000xxxx` is SRAM.
- **The annotations live in `core/src`, not in a pass over the generated
  bundle.** A post-processing pass over `core/yax86_core.h` is silently undone
  by the next bundle regeneration, which costs the entire 1.59x with no error
  and no warning. Annotating the source means regeneration carries the marks
  through by construction. If the count in the bundle ever drops, something is
  wrong with the bundler:
  ```sh
  grep -c Y_HOT core/yax86_core.h    # 170
  ```
  That is 86 annotations plus the macro block in `util/common.h`, whose seven
  lines all match on the substring, once for each of the twelve times a
  `bundle.json` names it - every module's public list, and the cpu module's
  private list as well.
- `Y_ALWAYS_INLINE` is not about placement, but exists for the same reason:
  something the compiler was doing for free stops being free and nothing in the
  source says so. The case it was added for is a small helper with one hot
  caller, inlined into it at every level — until a second caller appears, at
  which point `-Os` and `-O2` emit it out of line for *both* and the hot path
  grows a call it never had. That measured **3.6% at `-O2`**. Marking the
  helper `Y_HOT` instead recovers nothing, which is the tell that the cost
  is the call itself rather than where it landed.
- **The macro omits `inline` when unbundled**, and that is load-bearing rather
  than an oversight. `Y_MODULE_PRIVATE` is empty in that configuration, so
  a function carrying both has external linkage, and C99 6.7.4p3 forbids an
  `inline` definition with external linkage from referencing an identifier with
  internal linkage - which `YCPUGetRegisterOrMemoryOperandAddress()` does, in
  `YCPUGetRegisterAddress()`. Only clang diagnoses it, as `-Wstatic-in-inline`,
  so `tools/check-unbundled.sh` catches it on the clang legs of the matrix and
  not the gcc ones. The unbundled form is only ever syntax-checked, never linked
  or shipped, so dropping the keyword there costs nothing the emulator runs -
  which is also why removing the condition looks safe and is not.
- **Forcing one inlining decision moves others, and not always in your favour.**
  Pinning that helper inline made GCC stop inlining the larger
  `YCPUGetMemoryOperandAddress()` into `YCPUReadRegisterOrMemoryOperand()` at
  `-O3`, putting the effective address computation in flash on the hottest path
  in the emulator — 0.49%, of which marking it `Y_HOT` gives back 0.12%. The
  residue is the price of the `-O2` win and is worth paying at that ratio; what
  matters is that both halves get measured, because the second one is somewhere
  the change was never made.
- `Y_HOT_DATA` is the same thing for data, and is separate because a
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
  `YPlatformConfig.video_adapter` before `YPlatformInit()`, which decides both
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
- **Logs go to stdout**, which Emscripten routes to the browser console, as
  the logfmt lines described under "util — logging". Only errors are shown
  by default; `min_level` in `main()` raises that. The runtime's own
  messages - an image that would not open, the key trace at `debug` - are
  module `sdl`, logged through a `YLogger` of its own that shares the
  platform's configuration, so that it can log before the platform is up.

## pico/bench — Raspberry Pi Pico benchmark harness

- Builds a firmware image that runs the core on an RP2040 and reports how fast
  it went. This is the board the emulator is meant to fit on, and it is not a
  small desktop — see the performance section above for why that matters.
- It is a consumer of the core, not part of it. Nothing under `core` knows the
  harness exists, and `pico/bench/src/main.c` uses only the public interface.
- **CI does not build it**, because the RP2040 toolchain and Pico SDK are not
  installed in GitHub Actions runners. Because `pico/bench` shares
  `core/tools/generate-rom-data-files.js` with the core build, changes to that
  generator script can pass CI while breaking this harness. Always test generator
  changes locally with `pico/bench/build.sh O3` against a fresh `build-pico`
  directory (to ensure the floppy source is regenerated rather than reused).
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
- **It is deterministic: 27,702,858 emulated cycles and 2,328,128 retired
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
- The DOS idle skip is deliberately left off. The run stops at the prompt,
  which is where DOS starts idling, so the skip would have nothing to skip —
  and leaving it off keeps the workload a measure of how fast guest
  instructions execute rather than of how many are elided.

### Building and running

Needs `PICO_SDK_PATH` set and an `arm-none-eabi` toolchain. `build.sh`
configures and builds into `build-pico/<level>` and prints what the image costs
in flash and SRAM:

```sh
./pico/bench/build.sh                                  # the default level, -O3
YAX86_PICO_SYS_CLK_KHZ=250000 ./pico/bench/build.sh O3       # what a PR reports
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

The harness calls the core's public API and defines `Y_HOT` by name, so it
only builds against a core that uses the same names. A baseline from before
the core's names moved into the `Y` namespace has to be built from its own
checkout, harness and all, by running that checkout's `build.sh`.

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
- `pico/common/yax86_hot.h` is force-included into every translation unit, and
  is where this target defines `Y_HOT` to `__not_in_flash()`. Force-included
  rather than included from the core, so that the core knows nothing about this
  target, and inert against a core revision that does not use the mark. It is
  shared with `pico/picocalc` rather than copied, so that the benchmark
  measures the placement the runtime ships with: a copy that drifted would cost
  the 1.59x with nothing to say so.
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

GCC 16.2.0, SDK 2.3.0, picotool 2.3.0, 250MHz, 128K of guest RAM, hot path in
SRAM, at #89:

| level | seconds | emulated MHz | MIPS | vs a real 8088 | image flash | image SRAM | core `.text` |
| ----- | ------- | ------------ | ---- | -------------- | ----------- | ---------- | ------------ |
| `-O3` | **4.723184** | **5.865** | **0.493** | **123.0%** | 471,420 | 179,860 | 84,411 |

- A real 4.77MHz 8088 runs this in 5.807 seconds, so `-O3` clears parity at the
  250MHz the campaign targets with room to spare. **The compiler
  moves these as much as a change does** - the same commit measures 1.3% faster
  under GCC 16.2.0 than under 16.1.0 - so record the version alongside them and
  rebuild a baseline rather than comparing across one. **The seconds go
  out of date with every optimization** — this table is a snapshot to
  sanity-check a fresh measurement against, not a baseline to compare a branch
  to. Build the baseline from its own commit, as above.
- `-Os` is not reported and nothing ships at it, though `build.sh` still
  accepts it. It is the slowest level here by a wide margin and the one
  `Y_HOT` hurts.
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
  `YPlatformRun()`, which is how an application drives it, rather than an
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
  technical limit. **250MHz is the intended overclock frequency that PRs report at.**
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

## pico/picocalc — the PicoCalc runtime

- The runtime for the ClockworkPi PicoCalc: an RP2040 Pico H in a handheld
  with a 320x320 LCD, a keyboard, an SD card slot and a battery. It raises the
  system clock to 250MHz, brings up the keyboard controller, and boots MS-DOS
  3.30 off an image on the SD card, or off the one in flash without it, with
  a hard disk image on the card attached as well if there is one, and with
  the guest's text screen on the LCD and status lines below it - drawn by the second core, while the first runs
  the machine. The machine is paced to real
  time, keeps running once it reaches the `A>` prompt, and takes the
  PicoCalc's keyboard as its own. DOS's date and time questions are still
  answered by watching the screen, the way `pico/bench` does it: there is no
  clock to set them from, and it keeps the boot the `dos-boot` workload.
- Like `pico/bench` it is a consumer of the core, uses only the public
  interface, and is not built in CI.

### The hardware

From ClockworkPi's own repository, <https://github.com/clockworkpi/PicoCalc>:
the mainboard schematic, the sample code under `Code/`, and the ST7365P
datasheet. Third-party pinouts online disagree with each other, and one has
the audio pins wrong, so check a new pin against that repository rather than
a search result.

| part | connection |
| --- | --- |
| LCD: ST7365P, 320x320 | SPI1's pins, driven by PIO. SCK 10, MOSI 11, MISO 12, CS 13, DC 14, RST 15 |
| Keyboard and power: an STM32 | I2C1. SDA 6, SCL 7, address `0x1F` |
| SD card | SPI0. MISO 16, CS 17, SCK 18, MOSI 19; card detect 22 |
| PSRAM, 8MB | PIO SPI. CS 20, SCK 21, MOSI 2, MISO 3 |
| Speaker | PWM on GP26 and GP27 |

### Building and flashing

```sh
PICO_SDK_PATH=/usr/share/pico-sdk ./pico/picocalc/build.sh
./pico/bench/run.js build-pico/picocalc/yax86_picocalc.uf2
```

- Everything the status screen shows is also sent over USB serial, as a report
  each time a host attaches - USB CDC drops what is written before then - and
  `done` once the boot is over. That is the shape `run.js` captures, so it
  flashes and reads this image unchanged.
- **Flashing an image directly replaces ClockworkPi's factory firmware.** That
  is a boot loader at offset 0, which offers an SD card menu or else starts the
  default app; PicoMite as the default app at 200KB; and PicoMite's own storage
  at 968KB. Back it up before the first flash, and put it back the same way:
  ```sh
  picotool save -a picocalc-factory-flash.bin
  picotool load -f -x -o 0x10000000 picocalc-factory-flash.bin
  ```
- `YAX86_PICOCALC_DRAW_SCREEN=0 ./pico/picocalc/build.sh` builds an image
  that does not draw the guest's screen, into `build-pico/picocalc-no-draw`.
  It is otherwise identical, which is what measures what drawing costs.
- `YAX86_PICOCALC_SYS_CLK_KHZ` sets the system clock, 250000 by default.
  Empty builds for the stock 125MHz, and anything past 400000 is refused, for
  the reason under `pico/bench`'s overclocking notes.
- **The factory firmware has no picotool reset interface**, so `picotool -f`,
  and with it `run.js`, cannot reboot it into BOOTSEL. Opening its USB serial
  port at 1200 baud does: `stty -F /dev/ttyACM0 1200`.

### The dos-boot invariant holds here too

- The boot reaches `A>` having retired **2,328,128 instructions**, the same
  count `pico/bench` reports for `dos-boot`. That is what says this runtime and
  the benchmark run the same machine, so a figure measured on one stands for
  the other. At 250MHz it takes 4.72 seconds of processor time without
  drawing the guest's screen and 4.82 with it - 123% and 121% of a real
  PC/XT - off the floppy in flash, and 4.89 off the card. At the stock
  125MHz, 9.44 seconds with drawing, which is 61.5%. Processor time rather
  than how long it took, because the boot is paced - see below.
- If the count moves while the bench's does not, the runtime has stopped
  running the machine the benchmark measures. A key pressed during the boot
  moves it too, which is the guest being typed at rather than anything
  broken - and the reason the keys the controller queued before the firmware
  started are dropped. So does booting off a floppy on the card that is not
  the one in `resources` byte for byte, or with a hard disk image on the
  card - see the SD card section.

### Running in real time

- **The machine is paced between batches, and nothing else.** After each
  batch of 48,000 emulated cycles, `PaceToRealTime()` sleeps until real time
  has caught up with the emulated time reached. The batches are the same
  whether or not there is time to spare, so pacing cannot move anything the
  guest sees, and the invariant is the same paced or not.
- When a batch is due is worked out from an anchor - a point at which the two
  clocks agreed - which moves forward a whole emulated second at a time. That
  keeps the multiplication from overflowing however long the machine runs,
  and loses nothing to rounding.
- **A machine that falls behind catches up, but only by 100ms.** A short stall,
  such as a screen that scrolls, is made up by running flat out afterwards. A
  longer one is dropped and the anchor moved to the present, so that it is not
  followed by a burst of emulation at full speed - the same limit the SDL
  runtime uses. What is dropped is lost from the guest's clock. The report
  counts it as `dropped_us`.
- **Speed is measured as processor time, not as how long the boot took.**
  Paced, the boot takes as long as on a real PC/XT however fast the emulator
  is, so the runtime records the time it spent running the machine - elapsed
  less waited - as `boot_busy_us`. That is what an unpaced boot takes, to
  within 0.1%: a build with the pacing taken out measured 5.0105 and 5.0107
  seconds, against 5.0147 to 5.0149 paced.
- **Faster on average is not faster throughout.** At 250MHz the boot drops
  0.40 seconds, 0.30 without drawing: part of the stretch before the date
  prompt, and the one from the time prompt to `A>`, run slower than real time.
  The second is DOS printing and scrolling, and runs at 85% of real time, 91%
  without drawing - so what holds it back is emulation, not drawing. At the
  stock 125MHz the boot drops 3.67 seconds.
- **What is dropped moves in steps of about 100ms**, because a lag is
  dropped only once it passes 100ms, so each drop is just over that. Two
  builds a few milliseconds apart can drop a step apart - 0.20 and 0.30
  seconds before the date prompt, two drops against three - with neither
  slower there. Compare processor time, and how fast each stage ran, rather
  than what was dropped.
- **The idle skip is turned on at the prompt**, with
  `YPlatformSetDOSIdleSkip()` - not from power on, which would move the
  `dos-boot` invariant. Without it the prompt's polling loop runs
  at about 83% of real time at 250MHz, slower than the boot does on average,
  and the machine never once waits. With it the processor is busy about 19% of
  the time at the prompt, and emulated time tracks real time exactly: 24.439
  seconds of each over a 24 second sample.
- The Clock status line sets the guest's own time of day, the BIOS tick count
  at `0040:006C`, against real time since power on. It starts about a quarter
  of a second behind, which is POST's time before it starts counting, and
  falls further behind only by what was dropped.
- **The overclock is made with the watchdog armed**, after USB is up, the way
  `pico/bench` makes it and with the same marker in uninitialized RAM: a clock
  the chip cannot take reboots it, and the next boot runs at the stock clock
  and reports `overclock skipped - the last attempt hung`. It matters more
  here than on a bare Pico, whose BOOTSEL button is not inside a case. The
  core voltage goes to 1.20V first, as in `pico/bench`.
- **The watchdog is disarmed once the clock change is survived, and must not
  be left running and fed.** The SDK carries out `picotool reboot` with
  `watchdog_reboot()`, which only shortens the watchdog's countdown to 100ms,
  and feeding it every batch reloads the count: the reboot is put off for as
  long as the machine runs, and `picotool` reports success all the same. A
  hang after the clock change is recovered with the power button, since the
  keyboard controller cuts the power.

### The LCD

- The register values are ClockworkPi's, except that the pixel format is
  RGB565 rather than the samples' 18-bit RGB666, which is two bytes a pixel
  rather than three. The datasheet's serial interface takes both.
- **The serial interface is driven by PIO, not the SPI block.**
  `src/lcd.pio` is two instructions, a bit every two cycles, with the clock
  on a side-set pin. The SPI block runs from the peripheral clock, and
  `set_sys_clock_khz()` moves that to the 48MHz USB PLL on an overclock -
  see `set_sys_clock_pll()` in the SDK's `clocks.c` - which caps the LCD at
  24MHz. `PICO_CLOCK_ADJUST_PERI_CLOCK_WITH_SYS_CLOCK` would keep it on the
  system clock, and the SPI block does draw at 62.5MHz like that, but it
  takes every peripheral to 250MHz with it - the keyboard controller's I2C
  and the SD card's SPI. PIO runs from the system clock, which the
  overclock raises anyway, and it is what ClockworkPi's own NES build drives
  this panel with at 250MHz.
- **The serial clock is 62.5MHz** at both 125MHz and 250MHz, the system clock
  divided by 1 and by 2. The datasheet rates writes at a 66ns cycle, about
  15MHz, and ClockworkPi's samples ask for 25MHz. The panel draws cleanly at
  62.5MHz, checked by eye, and at 125MHz it took the cost of drawing during
  `dos-boot` from 7.4% of the run at 20.83MHz to 5.2%.
- **The control lines are held 50ns around every transfer**, which covers
  each write timing in the datasheet's serial interface section, the
  tightest being CS high for 40ns between transfers. At 125MHz the code took
  long enough between changes that nothing broke. At 250MHz it does not:
  through PIO, grey text came out green - as it would with every pixel's
  bytes swapped - and through the SPI block the panel took no new image at
  all. That reads as the SPI block or the bit rate failing at 250MHz, and is
  neither: with the hold, the SPI block draws at every rate tried from 31.25
  to 62.5MHz.
- **`LcdWaitIdle()` waits for the FIFO to empty before it clears the stall
  flag.** The state machine was already stalled before the last byte was
  queued, and stays so until it pulls that byte, so a flag cleared any
  earlier can be set again by the old stall, ending the wait while the byte
  is still going out.
- A write is `LcdBeginWrite()`, then any number of `LcdWritePixels()`, then
  `LcdEndWrite()`. The window commands, DC and CS happen once per window
  rather than per call, because the core hands pixels over 32 at a time.
  Raising CS between transfers does not end a memory write - only the next
  command does.

### The guest's screen, and what drawing it costs

- The runtime hands the core the 4x10 font through
  `YPlatformConfig.video_text_font`, so a text mode renders 320x250, and the
  guest's screen takes the top 250 rows of the panel. Status lines take the
  rows below.
- The core renders only what changed, as rectangles, and the panel keeps its
  own frame memory, so each region goes straight to the panel as a window of
  its own: `begin_render_region` opens it and `write_pixels` streams RGB565
  into it. Graphics modes render 640 pixels across and are not drawn yet.
- **Core 1 draws everything on the LCD, and core 0 never touches it** -
  `src/display.c`. Core 0 runs the machine and hands core 1 what to draw: a
  copy of the guest's screen from `YVideoCopyForRender()`, the status line
  values as text, and the request to turn the panel off, one word through the
  inter-core FIFO each.
- **Each buffer handed over has a flag that only core 0 sets and only core 1
  clears.** Core 0 sets it once the buffer is filled in, core 1 clears it once
  the buffer is drawn, and while it is set core 0 leaves the buffer alone, so
  the two cores never use one at once and neither waits for the other. The
  drawing figures, which both cores write, are kept under a lock.
- Core 0 offers a frame once per emulated frame, 60 times an emulated second,
  between batches. Drawing more often shows nothing new. A frame offered while
  core 1 is still drawing the last one is not copied, and is counted as late:
  what changed waits in the live state and goes with the next. Four frames
  are late during `dos-boot`.
- **Drawing costs core 0 1.5% of `dos-boot`** at 250MHz: 4.7975 seconds of
  processor time against 4.7267 without drawing, the same 2,328,128
  instructions either way, three and two runs with a spread under 0.01%. On
  core 0 it cost 5.3%, measured against a build of the commit before it.
  Of the 0.071 seconds:
  - 0.010 is copying the guest's screen, 16KB of video memory, 70 times.
  - 0.037 is core 1 drawing at the same time: the two share the 16KB XIP
    cache and the SRAM, and the code core 1 draws with runs from flash. A
    build in which core 1 takes each frame and skips drawing it, with the
    drawing code still linked, measured 4.7607.
  - The other 0.024 is where the code lands in flash, which the build without
    drawing does not share.
- Core 1 spends 0.271 seconds inside `YVideoRender()` over 70 frames and
  366,320 pixels, about 4.6 screens' worth, against 0.245 when core 0 drew
  them alone - slower for the same sharing.
- **That figure moves with where the code lands in flash.** Adding the
  screen blanking below, which runs only after the boot, once took the cost
  at 125MHz from 5.2% to 4.8%, with the time inside `YVideoRender()`
  unchanged. Compare a change against a baseline built from the same code,
  never against these figures.
- **Most of what is left is processor time, not the bus.** 368,960 pixels of
  16 bits take 0.094 seconds on the wire at 62.5MHz and 0.283 at 20.83MHz,
  and at 125MHz the time in `YVideoRender()` fell only from 0.647 to 0.436
  seconds between the two. That leaves about 0.34 seconds, or roughly 115
  cycles a pixel, in producing each pixel - the core's text renderer, a
  callback per 32 pixels, and the conversion to RGB565 - which runs from
  flash, since none of it is in SRAM.
- `dos-boot` draws little. A screen that scrolls redraws all 80,000 of its
  pixels each line, about 53ms at 250MHz and 88ms at 125MHz. The machine
  keeps running meanwhile, and the frames that come due are drawn together in
  the next. Putting the drawing code in SRAM, so that core 1 shares less of
  the XIP cache with core 0, has not been tried.
- **The screen is blanked 30 seconds after the boot or the last key**,
  whichever was later, with the panel's display off and sleep in commands -
  `LcdSetDisplayOn(false)`, which core 1 carries out after whatever it was
  handed before. A static image left showing for hours can leave its mark on
  the panel. The guest's screen is not rendered while it is blank, so what
  changes meanwhile stays recorded in the live video state. A key turns the
  panel back on: the panel keeps its frame memory through sleep, so the next
  frame need only draw what changed, and the key goes on to the guest as
  well. The backlight is the controller's, and its
  firmware will not dim the LCD's below 16 of 255, so blanking is the panel's
  job rather than the backlight's.

### The keyboard controller

- It answers a register at a time: write the register number, then read two
  bytes. ClockworkPi's samples wait 16ms between the two. That is not
  needed: the controller prepares its answer as the register number arrives,
  and answered every read straight on. The one read made at startup, of the
  version, still waits.
- **The bus runs at 10kHz**, as in ClockworkPi's samples and its PicoMite
  build, whose headers say the keyboard's should be 10kHz without saying
  why, though some of its other builds run it at 400kHz. The controller answered
  every read at 10, 100 and 400kHz alike, about 6,000 reads a minute at each
  without an error. A faster bus would only shorten each read, and a read at
  10kHz, about 5ms, finishes well within the batch that collects it, so
  nothing here gains from one.
- **The key FIFO is read without waiting, once a batch, on core 0.**
  `SouthbridgePollKey()` queues a whole read - the register number, then a
  two byte read, each ending in a STOP - in the I2C block's own FIFO, and the
  next call collects the answer and queues the next. The block carries the
  bus out by itself, so core 0 spends about 3.5us a batch on it: 2.1ms over
  `dos-boot`. A read the controller does not acknowledge aborts and is
  counted; one that never completes resets the block after 50ms.
- One read a batch is about a hundred a second, a key event each. A key
  pressed and released is two events, a modifier held repeats ten times a
  second, and the controller's FIFO holds 31, so typing never comes near it.
- **The controller queues keys whether or not anything reads them, and keeps
  running while the RP2040 restarts**, so its FIFO can be full of keys
  pressed long before the firmware started. The first boot with the poll
  took 31, the FIFO's size, as typing and moved the `dos-boot` invariant.
  Everything up to the first read that finds the FIFO empty is dropped.
- **The poll's real cost to `dos-boot` is where it puts the code.** The boot
  took 0.42% longer than on master - 4.8174 seconds against 4.7971 - and a
  build whose poll returns at once, with everything still linked, took
  4.8153. So 18ms of the 20ms is layout, which is the same effect as the
  drawing section describes.

- **Write a register number before every read.** Firmware older than April
  2025 (ClockworkPi commit `f58e85e`) freezes SCL if a read arrives with no
  register number before it, which is exactly what a bus scan does.
- **The version register only answers from v1.6 on** (`410f8fc`, March 2026).
  Earlier firmware answers any register it does not know with two zero bytes,
  so a version of 0 is reported as "older than v1.6". **v1.7 reports itself as
  v1.6**: its one change did not touch `BIOSVERSION`, so the register cannot
  tell the two apart. The battery register answers `[0B, percent]` - the
  register number first, then the value.
- **The controller can stop acknowledging for minutes at a time.** On the
  development unit it acknowledged nothing for two boots in a row, then
  answered every read on every boot after - including straight after the
  factory firmware had been polling it, which was the obvious suspect and did
  not reproduce it. The cause is not known, and that unit's firmware is older
  than v1.6, which is the release that added an I2C restart after 2.5 quiet
  seconds "indicating a possible I2C malfunction". The boot-time read retries
  for three seconds, which spans one such restart on v1.6 and later. The key
  FIFO poll needs nothing extra: a read that is not acknowledged is counted
  and the next batch tries again, for as long as it takes.
- **Flashing the controller** is ClockworkPi's procedure, in the PicoCalc
  repository's wiki: set DIP switch 1 on the back of the mainboard, power on,
  and `stm32flash` its serial bootloader through the USB-C port, which is a
  CH340 adapter (`1a86:7523`). Read the old firmware back first - `stm32flash
  -r old.bin -S 0x08000000:131072 /dev/ttyUSB0` - and read the new one back
  afterwards to compare, since that is a check `-v` alone does not make of the
  whole image. The development unit came with v1.4, which is byte for byte
  ClockworkPi's `Bin/PicoCalc_BIOS_v1.4.bin`, and now runs v1.7.
- **After flashing, it may not power on again until it loses power
  completely.** With DIP 1 back off, the power button did nothing until both
  cables and the batteries were removed and put back. A long press is the
  wrong way to try in any case: the firmware shuts the device down on a long
  press, and has since at least v1.4.

### The keyboard

- The controller reports characters, not keys: ASCII, with the keys ASCII
  has no code for above 0x80, and each reported as pressed, held or
  released. `src/keymap.c` turns each into the PC/XT's set 1 scancodes.
- **Shifted characters arrive already shifted**, as `!` rather than Shift
  and 1. The controller reports Shift, Ctrl and Alt as keys of their own as
  well, so a character is sent as the key that types it on a PC, both cases
  of a letter and both characters on a key alike, and the modifiers as they
  come. The guest sees Shift held and the 1 key, and makes the `!` itself.
- **The keys a PC has and the PicoCalc does not arrive as shifted forms of
  other keys**: F6 to F10 on F1 to F5, Home on Tab, End on Delete, Page Up
  and Down on the Up and Down arrows, Insert on Enter, and Break on Esc. Alt
  and I is Insert as well. On a PC/XT those are keypad keys or keys of their
  own, and Shift held would change them - Shift and Page Up on the keypad is
  a 9. So each is sent as a whole press and release, with whichever of
  Shift and Alt the guest was told are held let go around it, and its
  release sends nothing. Ctrl is left alone, so Ctrl, Shift and Esc is
  Ctrl-Break.
- A key held down repeats as a run of make codes, as a PC/XT keyboard's
  does. The controller starts repeating after 300ms, ten times a second.
- **Releases do not always match their presses**, because the controller
  works out the character again when a key is let go, with the modifiers
  held at that moment: Shift let go before the 1 releases a `1`, not a `!`.
  Both are the same PC key, so the release is right. For the shifted forms
  it is not, which is the other reason their releases send nothing. What is
  left is an arrow pressed, Shift pressed, and the arrow let go, which
  reports Page Up released: the arrow's make code is never followed by its
  break. A PC/XT BIOS takes no notice of a keypad key's release.
- The controller keeps Alt with comma, full stop, space and B for the
  backlights and the battery, and never reports them, and Shift with Left
  or Right reports nothing at all.

### The SD card, drive A and the hard disk

- **Drive A is `yax86/floppy_a.img` on the card**, a 360KB image on its FAT
  volume, and what the guest writes to it is written to the card. Without
  one, it is the floppy built into flash, which takes no writes, and the
  Disk status line says why: no card, a card that did not answer, no FAT
  volume, no image, or an image of the wrong size. The card is looked at
  once, at power on.
- **The hard disk is `yax86/hdd.img`**, attached as the hard disk
  controller's master drive only when the card has one. It is the core's
  10MB drive, 306 cylinders of 4 heads and 17 sectors, and the image has to
  be exactly that, 10,653,696 bytes. A blank one is zeros -
  `dd if=/dev/zero of=hdd.img bs=512 count=20808` - which the guest
  partitions with FDISK and formats with `FORMAT C: /S`, both on the boot
  floppy. Nothing on the device creates one.
- `src/sdcard.c` drives the card in its SPI mode on SPI0, from the SD
  Association's simplified physical layer specification: 400kHz to bring it
  up, then 24MHz. The SPI block runs from the 48MHz peripheral clock - see
  the LCD section - and divides by an even number, so 25MHz, the most a card
  takes in its default speed mode, comes out at 24. Every command starts
  from a fresh select that waits for the card to be ready, and blocks move
  one at a time, CMD17 and CMD24.
- **The filesystem is FatFs R0.16**, ChaN's. `resources/FatFs/ff16.zip` is
  the release exactly as elm-chan.org serves it, and the build unpacks it
  into the build directory with `FetchContent`, which checks it against its
  SHA-256. Keeping the release rather than its sources says it is unchanged
  by a hash anyone can check against the download, and leaves the source
  tree holding only our own code. To move to a new release, replace the zip
  and its hash, and carry our settings over to the new `ffconf.h`.
- **`src/ffconf.h` is the release's, with four settings changed**: code page
  437, `FF_FS_TINY`, `FF_FS_NORTC` and `FF_USE_FASTSEEK`. Long names and exFAT are off. The
  build copies it over the stock one, since `ff.h` includes it from its own
  directory before any include path, and does so with `configure_file()`, so
  that a change to it reconfigures the build.
- FatFs is the one thing in the image that calls `memcpy()` and `memset()`
  itself; the runtime's own code calls neither.
- **Both controllers move a byte at a time, and `src/disk_image.c` keeps one
  sector of each image in memory**, so each sector the guest reads or writes
  costs one card access. A sector the guest writes goes to the card as soon
  as its last byte is in, so nothing it wrote waits in memory to be lost when
  the power goes. `src/floppy.c` and `src/harddisk.c` each hold an image and
  hand it to their controller.
- **Each image keeps a map of where its clusters are on the card**, FatFs's
  fast seek. Without one, a seek backwards walks the file's cluster chain
  from its start, and a hard disk seeks backwards all the time. The map has
  room for an image in 31 pieces, and a file copied onto a card is usually
  in one; a more fragmented image goes without, and the report says so as
  `fd_fast_seek` and `hd_fast_seek`.
- **The card costs 1.5% of `dos-boot`**: 4.8896 seconds of processor time
  against 4.8171 off the floppy in flash, two runs each with a spread under
  0.01%. DOS reads 214 sectors on the way to `A>`, which take 75ms - about
  0.35ms a sector, for which the machine waits.
- **The invariant holds only off an image identical to the one in
  `resources`, and with no hard disk.** A copy of `DISK01.IMG` boots in the
  same 2,328,128 instructions. The same image with one file added to its root
  directory took 2,328,239, so an image the guest has written to is not one
  to measure with. A hard disk changes the boot more: the drive's BIOS finds
  it during POST and DOS reads its partition table, so a blank drive took
  2,398,153 and a formatted one 2,415,480.
- **Code added here can cost the boot without running.** The hard disk took
  the boot from 4.8899 to 4.9317 seconds with no image on the card, 0.85%,
  with the same instructions and no new work on the way: the floppy's reads
  got faster with fast seek. Master with a 1,696 byte array added ahead of
  guest RAM, which moves it to where the branch has it, took 4.9052, so a
  third of that is guest RAM moving. The rest is most likely where code
  lands in flash, which nothing here has confirmed. The two images are 8-byte aligned, and `.bss` is sorted by
  alignment before name, so they land ahead of guest RAM and move it.

### The 4x10 font

- 80 columns across 320 pixels leaves four pixels a character, and **no
  emulator that has tried shrinking a 640 pixel text screen to fit has found it
  readable** - DOSBox users on 320x240 devices and on the PocketCHIP said the
  same. What works is a font drawn for the cell: C64 80-column software used
  4x8, and pico-286's HDMI path 4x6.
- `src/font_4x10.c` is the only complete code page 437 font of that width
  with glyphs taller than five rows that turned up. Its ASCII glyphs are
  rugosi's, from picocalc_runCPM, under MIT - the notice is in the file - and
  the rest of code page 437, every box drawing and block character included,
  is Serg Podtynnyi's under GPLv3-or-later.
- Every glyph outside the box drawing range sits one row lower in its cell
  than the box drawing characters do, which leaves a blank row between lines
  of text while box drawing characters fill the cell and join. The shift is in
  the table rather than left to a renderer, so it is a plain font any renderer
  can draw. The three glyphs that use the bottom row - `0x02`, `0x0F` and
  `0x16`, cell-filling patterns - are not shifted, so that it does not clip
  them.

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
