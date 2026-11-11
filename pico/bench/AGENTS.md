# pico/bench - the RP2040 benchmark harness

Builds an image that runs the core on an RP2040 and reports how fast it went.
This is the board the emulator is meant for, and it is not a small desktop -
see "Performance work" in the root `AGENTS.md` for what a performance PR
reports and why.

- It is a consumer of the core: `src/main.c` uses only the public interface,
  and nothing in `core` knows it exists.
- **CI does not build it** - GitHub's runners have no RP2040 toolchain. It
  shares `core/tools/generate-rom-data-files.js` with the core build, so a
  generator change can pass CI and break this: test generator changes with
  `pico/bench/build.sh O3` against a fresh `build-pico`, so the floppy source
  is regenerated.
- A standalone CMake project, because the top level builds native code that
  does not cross-compile, and the Pico SDK must own the toolchain from before
  `project()`.

## The dos-boot workload and its invariant

- One workload, `dos-boot`: power on, let GLaBIOS POST, boot MS-DOS 3.30 from
  the floppy in `resources`, and stop at the `A>` prompt, pressing Enter at
  DOS's date and time questions by watching the CGA text buffer. The whole
  machine - CPU, PIT, PIC, video, FDC - running what the BIOS and DOS do, so
  it compares only against another yax86 build.
- **It is deterministic: 27,704,200 emulated cycles and 2,328,231 retired
  instructions, every run, at every level.** If either moves, behaviour
  changed and the times are not comparable.
- The floppy is compiled into flash by the ROM generator, generated into the
  build directory rather than committed - at 360KB it is megabytes of C.
  Guest writes are discarded; nothing on the way to the prompt writes.
- The DOS idle skip is off: the run stops where DOS starts idling, and off
  keeps the workload a measure of executing instructions.

## Building and running

Needs `PICO_SDK_PATH` and an `arm-none-eabi` toolchain. `build.sh` builds into
`build-pico/<level>` and prints the image's flash and SRAM:

```sh
./pico/bench/build.sh                                         # -O3
YAX86_PICO_SYS_CLK_KHZ=250000 ./pico/bench/build.sh O3        # what a PR reports
./pico/bench/run.js build-pico/O3/yax86_pico_bench.uf2 --out capture.txt
```

- **`run.js` reboots the board into BOOTSEL with `picotool`, loads the image
  and captures what it prints**, exiting non-zero if the run did not finish;
  `--no-flash` captures from a board already running. The output is logfmt
  (`core/AGENTS.md`): the bench's header and result are shown as tables, with
  seconds, emulated MHz, MIPS and "vs a real 8088" worked out from the
  result's integers. `--out` saves the lines as they came, `--json` prints
  each as JSON. The run is over at `event=done` - for the PicoCalc, once it
  answers the `report` command `run.js` sends then. To pull a figure from a
  capture: `grep -o 'elapsed_us=[0-9]*' capture.txt`.
- `serial.js` opens the port and `logfmt.js` reads and writes lines, for
  `run.js` and the PicoCalc's `debug.js`.
- **`YAX86_CORE_ROOT` names the checkout whose `core` is compiled in**, which
  is how a branch and its baseline are measured with one harness:
  ```sh
  git worktree add ~/Scratch/yax86-baseline <commit>
  YAX86_CORE_ROOT=~/Scratch/yax86-baseline ./pico/bench/build.sh O2 O3
  ```
  The harness uses the core's public names and `Y_HOT`, so a baseline from
  before the `Y` namespace is built from its own checkout, harness and all.

Notes on the machinery:

- **Open the serial port exactly once.** The firmware waits for DTR before
  printing, and every open asserts it, so a separate `stty` first would start
  the run before a reader attached. `serial.js` wraps the descriptor it holds
  in a `tty.ReadStream` and calls `setRawMode()` on that. It finds
  `/dev/ttyACM*` rather than hard-coding it, since the port can come back on
  another node after a flash or reset.
- A build directory reused across core roots keeps an object per root, CMake
  naming objects by absolute source path; `build.sh` spells the path out so
  `core_text` is always the core just built.
- **`pico/common/yax86_hot.h` is force-included** and defines `Y_HOT` to
  `__not_in_flash()`, so the core knows nothing of the target and older cores
  are unaffected. It is shared with `pico/picocalc`, so the benchmark measures
  the placement the runtime ships: a drifted copy would cost the 1.59x
  silently.
- The level goes through `CMAKE_C_FLAGS_RELEASE`, so the SDK and C library
  build the same way: what is measured is a whole image in the XIP cache.
- `build.sh` is bash because it runs `cmake` in a loop. Anything that parses
  is Node using only the standard library - there is no `package.json` in
  the repository, and nothing should need one.
- The size columns come from `image-size.js`, which reads the ELF headers
  directly: parsing `readelf` needs awk's GNU-only `strtonum()`, which leaves
  empty columns under mawk and BSD awk. It matches `arm-none-eabi-size`'s
  text column exactly, which is what `core_text` has always been.

## Current figures

GCC 16.2.0, SDK 2.3.0, picotool 2.3.0, 250MHz, 128K of guest RAM, hot path in
SRAM, at #89:

| level | seconds | emulated MHz | MIPS | vs a real 8088 | image flash | image SRAM | core `.text` |
| ----- | ------- | ------------ | ---- | -------------- | ----------- | ---------- | ------------ |
| `-O3` | **4.723184** | **5.865** | **0.493** | **123.0%** | 471,420 | 179,860 | 84,411 |

- **A snapshot to sanity-check against, not a baseline.** The seconds go out
  of date with every optimization, and **the compiler moves them as much as a
  change does** - the same commit is 1.3% faster under GCC 16.2.0 than 16.1.0.
  Record the version, and rebuild a baseline rather than compare across one.
- `-Os` is not reported - nothing ships at it - though `build.sh` accepts it.
  It is the slowest level by a wide margin, and the one `Y_HOT` hurts.
- **The XIP cache can reorder the levels**: with the core all in flash, `-O2`
  was slowest and `-Os` beat it by 6%, the cache rewarding a small image.
  With the hot path in SRAM the usual order returns.
- Flash is mostly the 360KB floppy; the code is under 110KB. `core_text`
  includes about 32KB of constant data - ROMs, fonts, the opcode table.
- **`stack_peak`** is how deep the stack got, interrupt frames included:
  unused stack is painted `0x5A5A5A5A` from `main`'s frame and searched
  afterwards, costing nothing during the run. Roughly 700-970 bytes of a 4KB
  bank, varying between runs with where interrupts land - a bound with room
  to spare, not a figure to compare.
- **Guest memory and the stack cannot collide**: guest RAM is at the bottom of
  the 256KB bank, the stack in SCRATCH_Y's own 4KB, with SCRATCH_X and about
  100KB of unused heap between. The heap grows away from guest RAM.

## Timing discipline

- **Nothing is printed during a run**: host I/O costs orders of magnitude
  more than an instruction. The UART is off, and results wait in RAM.
- Everything printed is an integer, so no float formatting is linked;
  cycles per microsecond is emulated MHz, instructions per microsecond MIPS.
- **The harness logs through the core's logger**, as module `bench` at
  `info`. The machine has its own configuration at `error`, since the core's
  warnings come in numbers during a boot and would be I/O in the run.
- **The harness's code sits ahead of the core's in flash**, so code added to
  the harness moves every core function that runs from flash. Moving logs
  into the harness shifted it 4,464 bytes and cost 0.26%, with the core
  identical. Across a harness change, rebuild the baseline.
- **`PaintStack()` is never inlined**: inlined into `main()` it paints
  `main()`'s own frame, the board faults with the watchdog disarmed, and has
  to be unplugged.
- USB servicing runs from a timer interrupt and costs every build the same.
- The run goes through `YPlatformRun()` in batches, as an application drives
  it; the core counts retired instructions itself. An invalid opcode does not
  stop the run - an 8088 has no invalid opcode exception.

## Overclocking

- `YAX86_PICO_SYS_CLK_KHZ` raises the clock **after USB is up**: a clock the
  chip cannot take hangs it, and a hang before enumeration leaves only the
  BOOTSEL button.
- **Do not raise the clock past 400MHz** - a standing instruction, not a
  technical limit. **250MHz is the overclock PRs report at.**
- A watchdog turns a hang into a reboot, which lands in `main` with a marker
  saying not to try that clock again. `watchdog_caused_reboot()` alone proves
  nothing - `picotool` reboots through the watchdog too. The marker is in
  `__uninitialized_ram`, surviving a reset but not power, and is set only
  across the window where a hang can happen.
- **`PICO_FLASH_SPI_CLKDIV` rises with the clock**, or the flash is
  overclocked by the same factor. It must be an `add_compile_definitions()`
  **before** `pico_sdk_init()`: it goes into the second stage bootloader, an
  SDK target `target_compile_definitions` never reaches. Set on the firmware
  target it does nothing, and the hang looks exactly like an unstable
  overclock.

## Profiling

- `YAX86_PICO_PROFILE=1` builds in a sampling profiler: a 50kHz timer records
  the interrupted PC into a histogram, dumped as addresses, and
  `symbolize.js` names them.
  ```sh
  YAX86_PICO_PROFILE=1 ./pico/bench/build.sh O3
  ./pico/bench/run.js build-pico/O3/yax86_pico_bench.uf2 --out capture.txt
  ./pico/bench/symbolize.js build-pico/O3/yax86_pico_bench.elf capture.txt
  ```
- **Symbolize against the ELF that produced the capture.** Another build's
  addresses do not fail - they give a plausible ranking of functions the run
  never executed.
- It costs a little over a percent and 32KB of SRAM: off by default, and
  **never on in a timing run**. It says where time went, not how much.
- SRAM is bucketed at 16 bytes and flash at 128, so below the top few flash
  entries a bucket can be credited to the wrong function. Verify the tail
  before acting on it.
