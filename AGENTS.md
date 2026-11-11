# Project: yax86

A small IBM PC/XT emulator for minimal MCU platforms like the Raspberry Pi
Pico, and for the browser via SDL and Emscripten.

- Emulates a basic PC/XT with an 8086/8088, running MS-DOS 3.3 on GLaBIOS.
- Fits and runs well on an RP2040: no dynamic allocation, no file system, no
  libc dependencies, and a hot path chosen against on-target measurements.
- Runs as a real machine on the ClockworkPi PicoCalc, an RP2040 handheld with
  a 320x320 LCD and a keyboard.

## Where the notes are

This file holds what applies everywhere. Each part of the tree below has an
`AGENTS.md` of its own with what applies there: why the code is shaped as it
is, and the alternatives that were measured and lost. **Read the file for a
path before changing anything under it**, and before proposing a change
there - most of what looks like an easy improvement in these directories has
been tried. A child file is loaded only once a file in its directory has
been opened with Read, so do not count on it arriving by itself.

| before working on | read | which covers |
| --- | --- | --- |
| anything under `core/`, other than the CPU | `core/AGENTS.md` | how the modules fit together and are brought up, bundling and ROMs, the memory map, idle skipping, the hard disk controller, host text fonts, rendering from a copy, logging, and `Y_HOT` placement |
| `core/src/cpu/` | `core/src/cpu/AGENTS.md` - and `core/AGENTS.md` | decoding, instruction fetch, the decode cache, the data window, prefixes, operand dispatch and values, cycle costs, string runs, the interrupt hint, runs of instructions, and what each measured |
| `sdl/` | `sdl/AGENTS.md` | the SDL runtime: pacing to the wall clock, disks, video adapters, audio, logs |
| `pico/bench/` or `pico/common/`, or any measurement on hardware | `pico/bench/AGENTS.md` | the `dos-boot` workload and its invariant, building and running on a board, current figures, timing, overclocking, profiling |
| `pico/picocalc/` | `pico/picocalc/AGENTS.md` - and `pico/bench/AGENTS.md` to measure | the PicoCalc's hardware, pacing, LCD, drawing, keyboard, SD card and disks, speaker, the commands over USB and `debug.js` |

Logging - the logfmt format, log levels and modules, and replies to
commands - is in `core/AGENTS.md` for every host. A change to the core's
public interface reaches every host, so read the files for the hosts it
touches too.

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
| `.cache/` | Downloaded references - GLaBIOS source, the 8088 test suite, other emulators. Not in the repository. |

## Building and testing

CMake. Native and Emscripten output go to `build-native` and
`build-emscripten`; the Pico targets build into `build-pico`.

```sh
./tools/build.sh
./tools/run-tests.sh
```

- **Builds default to `Release`**, set in the top level `CMakeLists.txt`
  because CMake applies no optimization at all when no type is named, and an
  unoptimized emulator is about four times slower. `-DCMAKE_BUILD_TYPE` is
  respected when given. `-Wall -Wextra -Wpedantic -Werror` is in
  `CMAKE_C_FLAGS_DEBUG`, so warnings are errors only in `Debug`.
- CI builds both compilers at `Debug` - for the warnings - and `Release` -
  what ships - since undefined behavior tends to show under one and not the
  others. `MinSizeRel` is not built: nothing ships at it.

### The 8088 hardware test suite

The CPU is checked against the [8088 hardware test
suite](https://github.com/SingleStepTests/8088): the result a real 8088 gave
for about 3 million instruction encodings. Download it once, about 480MB into
`.cache/8088-tests`, and `run-tests.sh` runs it with everything else:

```sh
./tools/download-cpu-hardware-tests.sh             # or name opcodes: 8D 00 80.0
build-native/core/tests/cpu/cpu_hardware_tests --gtest_filter='*Opcode8D*'
```

- The revision is pinned in the script: the suite has no releases, and its
  results are what the tests compare against. The opcodes come from
  `tools/cpu-hardware-tests-opcodes.txt`, not the rate-limited GitHub API.
  Group opcodes take a ModRM REG suffix.
- With nothing downloaded the suite reports no tests, so a fresh checkout
  passes. ctest runs every downloaded opcode in parallel.
- Only the architectural result is compared, with the flags the 8088 leaves
  undefined masked out.
- **Every opcode passes except two divide behaviors**, which would need the
  divide microcode emulated step by step: the flags a divide error leaves,
  which the suite masks in the registers but which reappear in the flags
  word the interrupt pushes; and the sign `IDIV` gives some quotients, which
  can disagree with the remainder. They are matched narrowly by
  `KnownDivergence` in `core/tests/cpu/hardware_test.cpp`, with the count let
  through asserted per opcode in `kKnownDivergences` - diverging more fails,
  and fixing one means lowering the count.

### The checks in run-tests.sh

- **`tools/check-unbundled.sh`** runs `$CC -std=c99 -fsyntax-only -Wall
  -Wextra -Wpedantic -Werror` over every file in `core/src`, generated ROM
  sources included. The library only ever builds as one bundled translation
  unit, where a call from one file to a `static` in another compiles cleanly;
  this is the only thing that catches it, and missing includes. `-Werror` is
  unconditional, since an implicit declaration is an error by default only
  from GCC 14 and Clang 16. It runs first: it takes a quarter of a second.
- **`tools/check-public-symbols.sh`** compares `nm -g` of
  `libyax86_core.a` with `core/public_symbols.txt`, each symbol with its type
  letter (`T YBIOSGetROMData`), so it catches a definition missing its
  visibility macro and a table that gained external linkage (an `R`). The
  library exports only functions. The file is the record of the public
  surface, so nothing else restates its length. After an intentional change
  to the public API, run it with `--update`.
- **`tools/check-namespace.js`** checks every file-scope name
  `core/yax86_core.h` gives a host against "Naming" below, and that its owner
  is the module defining it. It covers what `nm` cannot see - types, enum
  values and macros - reading a clang JSON AST dump and `clang -E -dM`, and
  recovering owners from the bundle's `#line` directives. It needs `clang`
  whatever `CC` is, and fails rather than skips without it.

### Debugging the WASM build

Use the Chrome DevTools MCP server against
`http://localhost:3000/yax86_sdl.html`. A stale service worker can serve
mismatched `.wasm`/`.js`/`.data` and look like a regression - a blank canvas,
a failed boot - so unregister service workers and clear the origin's caches
before chasing the code. Kill the web server, the MCP instance and its
headless Chrome when done; that Chrome is heavy even idle.

## Code style

### Language and dependencies

- Core code is portable C99; tests are C++14 with Google Test. Everything
  follows the Google C++ style guide, formatted with `clang-format`, except
  for the core's file-scope names - see "Naming".
- **The core calls no libc functions**, like `printf` or `memset`, so it
  stays standalone. Headers for types, constants and macros (`uint8_t`,
  `NULL`) are fine.
- **The hosts use the C library** - `strlen()`, `strcasecmp()`,
  `toupper()`, `memcpy()`, `snprintf()` - rather than writing their own.
- No dynamic allocation, core or host - static storage and the stack only.
  In the core, zero data with static zero-initialization, `static MyStruct
  data = {0};`, not `memset`.

### Declarations

- Enums over `#define` for constants, and over literals in logic:
  `enum { Y_FDC_FOO = 0xF8 };` rather than `0xF8` inline.
- `static inline` functions over macros.
- Specific types like `uint8_t` over `int` in signatures and struct members.
- `typedef struct Name { ... } Name;` and `typedef enum Name { ... } Name;`.
- Prefix `++var` and `--var`.
- Unused parameters carry `Y_UNUSED` from `util/common.h`.

### Naming

`core/yax86_core.h` is the only file a host includes, so every file-scope
name it defines is taken from the host. All of them are in a `Y` namespace:

> **Anything with a type or storage** - function, typedef, struct or enum
> tag, file-scope variable, `const` object - is `Y` + owner + CamelCase.
>
> **Anything that is a compile-time value** - enum value or macro - is `Y_` +
> OWNER + `_` + ALL_CAPS.

| kind | example |
| --- | --- |
| function | `YCPUTick`, `YCPUReadRegisterOperandByte` |
| type | `YCPUState`, `YVideoPosition`, `YVideoRGB` |
| `const` object | `YCPULogModule`, `YVideoDefaultConfig`, `YCPUOpcodeTable` |
| enum value | `Y_CPU_AX`, `Y_CPU_BYTE`, `Y_FDC_MSR_BUSY`, `Y_PIC_ICW1_INIT` |
| macro | `Y_HOT`, `Y_CPU_LOG`, `Y_STATIC_VECTOR_TYPE` |

- **The line between the forms is storage**: `&YCPULogModule` takes an
  object's address, where `&Y_CPU_LOG_MODULE` would read as a macro's.
- **The owner is the module whose directory defines the name** - `CPU`,
  `Platform`, `Video`, `FDC`, `HDC`, `PIT`, `PIC`, `DMA`, `PPI`, `Keyboard`,
  `BIOS` - and a sub-component takes its module's: `YVideoCGARenderRegion`,
  `Y_VIDEO_CRTC_REGISTER_CURSOR_START`, `YPlatformMemoryMapEntry`. It is what
  keeps eleven modules' `Init`s apart in one translation unit.
- **util is not a module**; each header is its own owner: `Log` (`YLogger`,
  `YLoggerConfig`), `Logfmt` (`YLogfmtLine`), `StaticVector`, and `YSNPrintF`,
  whose `va_list` form is `YVSNPrintF`. There is no `YUtil`. The macros in
  `util/common.h` are infrastructure with no owner: `Y_PUBLIC`, `Y_HOT`,
  `Y_UNUSED`.
- **`YAX86_IMPLEMENTATION` and include guards are outside the rule**: they
  switch what a header contains rather than naming anything for the host, so
  they keep `YAX86_`. The namespace check holds guards to `YAX86_` + ALL_CAPS.
- **An owner is not said twice**: `YVideoDefaultConfig`, `Y_VIDEO_NUM_MODES`,
  `YCPULogModule`, unless the repeat is meaning - `Y_VIDEO_CONTROL_VIDEO_ENABLE`
  is the control register's video enable bit.
- **ALL_CAPS splits at every CamelCase boundary, digits staying with the word
  they qualify**: `Y_FDC_ST0_NOT_READY`, `Y_FDC_DOR_MOTOR0_ENABLE`,
  `Y_PPI_MEMORY_SIZE_128KB`. A resolution is one word:
  `Y_VIDEO_CGA_PIXEL_MASK_320X200`.
- **Block scope is unaffected**: locals, parameters and members are
  snake_case, and block-scope constants keep `kName`.
- This departs from Google's `kEnumName`, whose reason - collisions with
  macros - goes away when every enumerator and macro is under `Y_`, and C has
  no `enum class` or namespaces to scope one.
- **Hosts keep the Google names**: `sdl/`, `pico/bench` and `pico/picocalc`
  define nothing anything else includes, so `kName` constants, `g_name`
  file-scope variables, CamelCase functions led by the host's module word
  (`DisplayPutPixels()`), and plain `static` or nothing for visibility. A host
  must never take the `Y` prefix: a later core name could collide with it.

### Visibility

- Every core file-scope definition carries a macro from `util/common.h`
  naming who may reach it:

  | macro | expands to | for |
  | --- | --- | --- |
  | `Y_PUBLIC` | nothing | public API, defined in a source file |
  | `Y_PUBLIC_HEADER` | `static` | public API, defined in a header |
  | `Y_MODULE_PRIVATE` | `static` bundled, nothing unbundled | shared between a module's source files |
  | `Y_MODULE_PRIVATE_HEADER` | `static` | module-private, defined in a header |
  | `Y_FILE_PRIVATE` | `static` | used only in the file that defines it |

- **The macro names the tier and nothing else**: `inline`, `const` and the
  return type are written after it as they would be without it -
  `Y_PUBLIC_HEADER inline bool YCPUGetFlag(...)`. A separate macro is
  warranted only where the expansion differs, as between a header definition,
  which needs internal linkage, and a source one, which must not have it.
- **Public means reachable through the interface**, not used by a host
  today: the log module descriptors are public because
  `YLoggerEnableModule()` takes one. `YSNPrintF()` is the reverse - in every
  module's public half because `logfmt.h` needs it, and called by nothing
  outside the core.
- Bare `static` cannot say which tier was meant, so nothing is written that
  way. The one exception is the body of `Y_STATIC_VECTOR_TYPE`, whose helpers
  take their tier from wherever it is instantiated.

### Placement and inlining marks

- **The visibility macro comes first, then `Y_HOT`, `Y_NOINLINE` or
  `Y_ALWAYS_INLINE`, then the return type**: `Y_MODULE_PRIVATE Y_HOT
  YCPUInstructionResult YCPUExecuteSub(...)`. Every definition has a
  visibility macro and few have a mark, so this keeps the interface question
  in the same place on every line and makes `^Y_PUBLIC` an exact count. An
  attribute behind `static` still takes effect; check placement with
  `objdump -t` at `-O0`, where small statics do not inline away.
- `clang-format` never reorders tokens, so a mark put on a wrapped
  signature's continuation line is rejoined as `YCPUInstructionResult Y_HOT`,
  looking deliberate. Put both macros on the line the declaration starts on.

### Comments

- On the line before what they describe, not at the end of the line:

  ```c
  // Good comment - do this
  int var1;

  int var2;  // Bad comment - don't do this
  ```

- **Describe the code as it is, never as it was.** No tombstones - "this used
  to be a linear scan", "kept for the old callers". Git records history.
- The one thing worth writing about an alternative is what it costs:
  "assigning this from `metadata->has_modrm` measures 0.12% slower" is a
  comment, "we used to assign it from `metadata->has_modrm`" is a tombstone.
- The same applies to these `AGENTS.md` files: a note earns its place by
  telling the next change something it needs.

## Performance work

The emulator is optimized against the Raspberry Pi Pico, not a desktop. A
Cortex-M0+ has no branch predictor, no speculation and no cache but the 16KB
XIP window code runs from flash through, so a desktop measurement cannot
settle a question about it. `pico/bench` is the harness - see its
`AGENTS.md`.

### What a performance PR must report

A results table measured on hardware with the `dos-boot` workload:

- **`-O3`**, GCC, at **250MHz**, hot path in SRAM, the default 128K of guest
  RAM. `-O2` and `-Os` are not reported.
- **Seconds, emulated MHz, MIPS and "vs a real 8088"** for the baseline and
  the branch, and the **gain** as the increase in emulated MHz.

```
| | seconds | emulated MHz | MIPS | vs a real 8088 | gain | core `.text` |
| --- | --- | --- | --- | --- | --- | --- |
| master | 4.975485     | 5.568     | 0.468     | 116.7%     | —          | 83,043 |
| this   | **4.814238** | **5.754** | **0.484** | **120.6%** | **+3.35%** | 82,435 |
```

The harness reports `elapsed_us`, `cycles` and `instructions` as integers and
`run.js` derives the rest: for `C` cycles and `I` instructions over `t`
seconds, emulated MHz = `C / t / 1e6`, MIPS = `I / t / 1e6`, vs a real 8088 =
`(C / 4.77e6) / t` (a real 8088 takes **5.807 seconds**), and gain =
`t_baseline / t - 1`.

Alongside the table state:

- **The invariant, and that it did not move**: `dos-boot` retires 2,328,128
  instructions in 27,702,858 emulated cycles, at every level, every run. If
  either moved, behaviour changed and the times are not comparable - say so
  and explain why instead of presenting a speedup.
- **How the baseline was obtained**: rebuilt from its own commit through
  `YAX86_CORE_ROOT` against the same harness, on the same board, in the same
  session. A figure from an earlier PR is a cross-check, not a baseline.
- **How many runs, and their spread.** Two or three is enough here.
- **`core .text`** - several changes have won on speed and size together.
- Which tests ran: `./tools/run-tests.sh`, and the 8088 suite where decode or
  execute changed.

### Measurement discipline

- **Reproducibility is excellent, so small differences are real**: three
  flash-and-run cycles of one image spread 0.0065%. **A 1% difference is a
  result.** Do not demand a large margin, and do not accept "neutral" as a
  win.
- **Measure, do not reason.** Every speculative change made here on the
  strength of an argument about generated code has lost. The winners were
  built and measured alongside the two or three arrangements that looked as
  plausible.
- **A queued or prototyped figure is a reason to try a change, not a number
  to expect**: the changes underneath move what is left to win. Re-measure on
  the branch.
- **Forcing one inlining decision moves others.** Check `nm` after adding a
  mark rather than assuming the effect stayed local.
- **What matters is flash traffic.** `dos-boot` makes about 559 flash
  accesses per emulated instruction, so a structure read once per
  instruction is noise against the fetches around it - which is why moving
  code to SRAM is worth 1.59x and moving a per-instruction table read is
  worth nothing measurable.

## What to emulate, and what not to

- **No features the PC/XT does not use** under GLaBIOS or MS-DOS, and no
  support for other hypothetical x86 operating systems. The 8259A's
  level-triggered interrupts, auto-EOI and specific EOI are absent because
  the PC/XT uses edge triggering, manual EOI and fully nested mode.
- **Everything GLaBIOS needs** to run MS-DOS 3.3 and basic DOS applications.
- GLaBIOS is the BIOS: `.cache/GLaBIOS/src/GLABIOS.ASM`, built as
  `ARCH_TYPE_EMU`.

## Reference implementations

For how to emulate a component, read these, all under `.cache`:
8086tiny (`.cache/8086tiny`), 86Box (`.cache/86Box`) and MartyPC
(`.cache/martypc`).
