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

### sdl - SDL runtime

- The `sdl` directory contains an SDL3-based runtime for the emulator.
- It is compiled via Emscripten to produce a WebAssembly binary and JavaScript
  wrapper (`yax86_sdl.{wasm,js}`).
- It takes an optional floppy image path, defaulting to `floppy_a.img`. A hard
  disk is attached by default, and `--no-hdd` leaves the machine without one.
  The default matters because the Emscripten build has no command line, so a
  hard disk gated behind a flag would be unreachable in the browser.
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
open `http://localhost:3000/yax86_sdl.html`.

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

