# Running the emulator on a Raspberry Pi Pico at 8088 speed

Working notes for the RP2040 optimization campaign. The emulator currently
reaches **parity with a real 4.77MHz 8088 at a 306MHz system clock**, and runs
at 42% of one at the stock 125MHz.

**This file is scaffolding.** It exists so that whoever picks the campaign up
does not re-derive the context or repeat the dead ends. Fold what is durable
into module comments and `AGENTS.md` as each PR lands, and delete this file
when the campaign is over.

Note that this file is **not currently tracked by git**. If it is meant to
survive, commit it; if not, the durable content has to reach `AGENTS.md`
before it is deleted.

- [Status](#status)
- [How to measure without fooling yourself](#how-to-measure-without-fooling-yourself)
- [What the workload actually is](#what-the-workload-actually-is)
- [What is built but not landed](#what-is-built-but-not-landed)
- [Methodology, or what actually pays](#methodology-or-what-actually-pays)
- [What did not help](#what-did-not-help)
- [Hardware envelope](#hardware-envelope)
- [Prior art: picocalc_x86](#prior-art-picocalc_x86)
- [Open questions](#open-questions)

## Status

The benchmark is `dos-boot`: boot MS-DOS 3.30 from a floppy image to the
command prompt. Whole system - CPU, PIT, PIC, DMA, video, FDC, HDC - not a CPU
loop. It is deterministic, and the exact figures are the check that a change
altered performance without altering behaviour.

**The invariant on master is 27,701,507 cycles and 2,328,015 instructions**,
exact and reproducible to the digit. It has moved twice: run chaining took the
original 27,724,061 / 2,324,726 to 27,724,018 / 2,324,662 (see the chaining
entry below), and the store fidelity fix in #64 took it to its current value by
making stores cheaper, which gives the guest's polling loops more turns before
the same number of PIT ticks elapse. **Prototype figures below predating #64
are against the old invariant and are not comparable to a run on master.**

| | @125MHz | @400MHz |
| --- | --- | --- |
| master, PR #60 merged | 26.900s | 8.597s |
| everything built since | **13.949s** | **4.404s** |
| | **1.93x** | **1.95x** |

At 306MHz the run takes 5.757s against a real 8088's 5.812s - **101.0% of an
8088**, at the stock 1.10V core voltage. Host cost is **750 cycles per emulated
instruction** at 125MHz.

Do not raise the clock past 400MHz. That is a standing instruction, not a
technical limit.

## How to measure without fooling yourself

### 1. The build compiles the generated bundle, not `core/src`

Builds compile `core/yax86_core.c`, which includes the **generated**
`core/yax86_core.h`. Editing a module under `core/src/**` and rebuilding only
the Pico image measures the old code. Run `./tools/build.sh` first so the
bundle regenerates, then check the symbol is actually there:

```sh
grep -c <new symbol> core/yax86_core.h
```

This once produced three "neutral" variants that were all unmodified master.

### 2. `hotify.py` is obsolete - do not run it

Earlier notes said to run a post-processing pass over the generated bundle to
add `YAX86_HOT`. **That is no longer true and following it will corrupt the
bundle.** Since PR #60 the annotations live in `core/src/**` and regeneration
carries them through by construction. The check is:

```sh
grep -c YAX86_HOT core/yax86_core.h    # 162
```

That is 85 annotations plus the seven-line macro block, counted once per each
of the 11 module bundles. The tell that placement has gone wrong: hot symbols at `0x1xxxxxxx`
(flash) rather than `0x2xxxxxxx` (SRAM), and `__*_veneer` thunks in the symbol
table.

```sh
arm-none-eabi-nm -nS build-pico/O3/yax86_pico_bench.elf | grep ' CPUTick$'
```

### 3. Forgetting the clock

Omit `YAX86_PICO_SYS_CLK_KHZ` and the board runs at 125MHz; the result looks
like a catastrophic regression. Check the `sys_clk_khz` line the harness
prints - it reports what was actually achieved, not what was asked for.

### 4. Opening the serial port twice, and `--no-flash`

`run.js` must open the port exactly once and set termios on that fd. Running
`stty` first asserts DTR, which starts the run, and the header is gone before
the reader attaches. The port re-enumerates after a watchdog reset, so find
`/dev/ttyACM*` dynamically rather than hard-coding it.

Separately: `--no-flash` captures from a board already running the image, but
the firmware runs once at boot. Using it for repeat runs hangs until the
timeout. Repeat runs must re-flash.

### 5. Changing timing changes the workload

Any change to emulated cycle costs alters how many times the guest's own
polling loops iterate, so the instruction count moves and cycles-per-instruction
stops being comparable. An attempt to price cycle accounting by stubbing it out
produced 1,894,058 instructions against 2,324,662 and told us nothing. If a
change touches timing, only same-invariant comparisons mean anything.

### Noise floor

Reproducibility is excellent: three runs of one image gave 4.403831, 4.403941
and 4.404011 seconds - a spread of 0.004%. **A 1% difference is real.** Do not
demand large margins before believing a result, and equally do not accept a
"neutral" result as a win.

### Commands

```sh
YAX86_PICO_SYS_CLK_KHZ=400000 ./pico/bench/build.sh O3
./pico/bench/run.js build-pico/O3/yax86_pico_bench.uf2 --out capture.txt

# Profile instead of time. Costs over a percent, so never leave it on.
YAX86_PICO_PROFILE=1 YAX86_PICO_SYS_CLK_KHZ=400000 ./pico/bench/build.sh O3
./pico/bench/run.js build-pico/O3/yax86_pico_bench.uf2 --out capture.txt
./pico/bench/symbolize.js build-pico/O3/yax86_pico_bench.elf capture.txt
```

Symbolize against the ELF that produced the capture, not merely one built from
the same source - addresses move, and the wrong ELF silently reports a
plausible ranking of functions the run never executed.

## What the workload actually is

Measured on the board with temporary instrumentation. This is the single most
useful section here: several optimizations were designed directly from these
numbers, and one earlier conclusion was wrong for want of them.

**Half of `dos-boot` is one spin loop.** Four opcodes are 51% of all
instructions executed, and 72.3% of instructions run inside the XT-IDE option
ROM. It is the XTIDE boot-menu countdown at `0xC9512`:

```
mov ax, ds:0x46C    ; BIOS tick counter at 0040:006C
sub ax, ds:0x7F08   ; minus the saved start tick
cmp ax, 0x48        ; 72 ticks = 3.96 seconds
jb  <back>
```

Patching the compare to zero - and adding 0x48 to the ROM's last byte to fix the
sum-to-zero checksum - gives a second bench: 1,140,915 instructions, 14,117,002
cycles, 2.9595 emulated seconds. **This is not a defect in the bench.** The
metric is host cycles per emulated instruction, and a tight loop is a
legitimate thing to be fast at. But a result should be checked against both,
because a change can be loop-specific: the decode cache is worth +15.5% on the
standard bench and +9.2% without the countdown, while everything else measured
within a point or two of each other.

Other characterization, all on the standard bench:

| | |
| --- | --- |
| straight-line run length, mean | **3.65 instructions** |
| runs of length 1 | 26.0% |
| runs of length 4 | 51.3% (the countdown body) |
| conditional jumps | 19.2% of instructions |
| ...immediately after the instruction setting their flags | **94.3%** |
| flag-setting instructions | 36.6% of instructions |
| ...whose ZF/SF/PF are ever read back | **15.7%** |
| execution from SRAM rather than flash | **95.3%** |

With the decode cache and fusion in place the per-instruction split is
**68.5% cache hit, ~24% fused, ~7.5% full decode**, with 2,146 cache flushes
over the whole run. The decode path is finished; there is nothing left to win
there.

There is no second common instruction pair. After `MOV/SUB`, `SUB/CMP` and
`CMP/JB` - all the same loop body, each ~11.8% - the distribution falls to a
flat tail at 0.7%.

## What is built but not landed

**#61, #62, #63 and #64 have all landed.** Seven prototype items remain on
`proto-explore`, plus the decode struct-clear, which landed as #65. All figures
below were measured at 400MHz against the row above them, on a master two
invariants old - **treat every one as a reason to try the change, not as a
number to expect.** The decode struct-clear is the worked example: recorded at
+4.15%, re-measured on current master at +4.43%.

| change | seconds | gain |
| --- | --- | --- |
| PR #63 tip | 7.1249 | |
| decode clears 5 fields, not 12 bytes (landed, #65) | 6.8414 | +4.15% |
| PIC interrupt-request hint (landed, #71: +5.97%) | 6.5645 | +4.22% |
| conventional-memory fast path (see note below) | 6.4293 | +2.10% |
| CPU direct data window (landed, #69: +3.28%) | 6.2428 | +2.99% |
| direct operand dispatch (landed, #67: +2.23%) | 5.9831 | +4.34% |
| displacement/immediate size tables | 5.9740 | +0.15% |
| fused rel8 block in `CPUTick` | 5.6175 | +6.35% |
| unified rel8 block (adds LOOP family) | 5.6507 | −0.59% |
| **decode cache, 256 entries** (landed, #72: +10.86%) | 4.9474 | **+14.2%** |
| base cycles precomputed into the entry | 4.8922 | +1.13% |
| conditional jump fused onto its predecessor | 4.8238 | +1.42% |
| bulk path for REP MOVS and REP STOS | 4.7425 | +1.72% |
| **up to four cached instructions per tick** | 4.4039 | **+7.7%** |

With #72 the decode cache has landed, at +10.86% at `-O3` and +10.12% at
`-O2` - the largest single win of the campaign. `-O3` now runs `dos-boot` in
5.368 seconds against a real 8088's 5.807. Two of its recorded
characterizations were re-measured on the current base and both held: using
entries in place rather than copying them out is worth 3.25% at `-O3` on its
own, and 256 entries is still the size to spend, with 512 only 0.11% ahead for
another 6KB.

It is 10.86% rather than the 12.86% first measured because the cache is held in
`CPUConfig` rather than in `CPUState`, which costs 1.80% at `-O3` in the extra
dereference through `cpu->config`. That is a deliberate trade for one shape
across everything a host provides, and it is the number that says what
embedding `CPUConfig` in `CPUState` would be worth.

With #71 the campaign items are done bar the OpcodeMetadata field widening,
which AGENTS.md already records the best of five arrangements for. The hint
re-measured at +5.97% against its recorded +4.22%, for the reason the notes
predict: nothing landed since removed the interrupt-acknowledge call, so the
work removed was unchanged while the denominator had shrunk.

Landing order matters. Suggested: the test-harness coverage fix first, then the
campaign items, then the decode cache alone, then fusion and bulk strings, then
chaining last and by itself because it restates the invariant and needs its own
`AGENTS.md` update. The decode cache prototype holds its table in a file-static
array, which is fine for one CPU and wrong for the library's contract - it
needs moving into `CPUState`, which is awkward because `Instruction` is
declared after `CPUState` in `public.h`.

### The conventional-memory fast path is now mostly unreachable

With #69 landed, the CPU indexes conventional memory itself and never calls
into the platform for it, so the platform-side fast path optimizes a path its
main caller no longer takes. What is left for it is DMA, out-of-RAM accesses,
and hosts calling `ReadMemoryByte()`/`WriteMemoryByte()` through the public
API - and neither `sdl/` nor `pico/bench` does the last of those. Re-measure it
on top of #69 before landing, and expect substantially less than the recorded
+2.10%; it may not be worth the state it adds to `PlatformState`.

Its premise is also the one PSRAM breaks. `address < conventional_memory_size`
assumes conventional memory is a single contiguous host buffer, which stops
being true as soon as part of guest RAM lives behind a bus. #69's window is
already documented as a directly-indexable *prefix* rather than as all of guest
RAM, so it tiers without rework; this would not.

### Plain OpcodeMetadata fields — measured, ready to land

Measured on top of #67 at 400MHz, all three levels positive and all three
*smaller*. `has_modrm`, `immediate_size` and `width` become whole bytes;
`width` stays `Width width : 8` rather than `uint8_t` because the C++ tests
will not implicitly convert an integer to an enum. `sizeof(OpcodeMetadata)`
stays 8, so the opcode table stays 2048 bytes.

| level | #67 | plain | gain | core `.text` |
| --- | --- | --- | --- | --- |
| `-O3` | 6.578612 | 6.492927 | +1.30% | 88,077 -> 87,569 |
| `-O2` | 7.244874 | 7.169891 | +1.04% | 73,913 -> 73,705 |
| `-Os` | 9.119126 | 8.933260 | +2.04% | 60,433 -> 60,237 |

**This measured mixed before #67** - +0.80% at `-O3` but −0.39% at `-O2` -
and was set aside for that reason. Removing the dispatch tables is what
turned it positive: `ctx->metadata->width` is now read at five explicit
sites instead of being consumed as a table index, so the load-shift-mask a
packed read costs became visible. The pattern matches the `Instruction`
bitfield result in #66, including the gain being widest where least is
inlined.

### The decode cache, and why the earlier attempt failed

These notes previously listed a decoded-instruction cache under "what did not
help" at **+1.4% for 21KB**, with the recorded reason that *"the 20-byte
Instruction copy plus validation costs about what decoding a 2-3 byte
instruction from an open fetch window costs."*

That reason describes a design flaw, not a property of the idea. If the copy is
what costs, do not copy: entries are used **in place**, so a hit passes
`&entry->instruction` to the executor and nothing moves. 256 entries, 6KB,
**+15.5%**.

Invalidation is a bitmap of one bit per 4KB page, set when the cache holds an
instruction decoded from that page and tested on every memory write. It must
sit on **every** writer including `WriteMemoryByte` in the platform, which is
the path DMA takes - DOS loads itself over the boot sector by DMA, and the
first version without invalidation retired 13,247,281 instructions instead of
2,324,662.

### Two bugs run chaining exposed

Both were real emulation defects, not timing granularity, and both are why the
change is trustworthy now:

- **Self-raised interrupts.** An instruction in a run can raise an interrupt on
  itself - `INT n`, `INTO`, a divide error - and the run carried on for three
  more instructions before the dispatch that should have been immediate.
  `INT 21h` is on every DOS service call. Guarding on
  `has_pending_internal_interrupt` and `stop_requested` took the drift from
  0.9% to 360 cycles.
- **`HLT`.** The test that skips execution while halted is made once, before a
  run starts, so a `HLT` inside a run was followed by the instructions after it
  instead of by a wait. Guarding on `is_halted` took the drift to 43 cycles.

**43 cycles remain unexplained.** What is known: it is constant - exactly 64
instructions and 43 cycles at both a 1/20 and a 1/40 second harness poll
interval, even though changing that interval moves the totals by 11,816
instructions on its own; it is not interrupt delivery, since both builds
dispatch exactly 1,950; and it is not noise. A per-instruction trace over the
first few thousand instructions of POST, diffed between builds, should name it.

### A fidelity defect found on the way - fixed in #64

Four handlers - `MOV r/m,r`, `MOV r/m,sreg`, `MOV r/m,imm`, `POP r/m` - begin
with `Operand dest = ReadRegisterOrMemoryOperand(ctx)`, **reading the
destination they are about to overwrite completely**. `ReadMemoryOperandWord`
charges two bus cycles for that read, so a store is billed for traffic the 8088
never puts on the bus:

| | before | correct | bus traffic |
| --- | --- | --- | --- |
| `MOV AX, [BX]` | 15 | 15 | one word read |
| `MOV [BX], AX` | **23** | 15 | one word written |
| `MOV [BX], AL` | **15** | 11 | one byte written |
| `ADD [BX], AX` | 24 | 24 | read *and* written - correct |

A store cost almost as much as a read-modify-write. Fixed in #64 by resolving
the address without fetching the value, through
`GetRegisterOrMemoryOperandAddress` and `WriteOperandAddress`. It was
**host-neutral** on the board, as a correctness change should be, and it moved
the invariant to its current value. `core/tests/cpu/cycles_test.cpp` covers it;
no architectural test can, since the registers and memory come out identical
either way.

### The test-harness coverage gap

Twice a change shipped a real bug past a green 926/926, because the strongest
test the project has could not reach the new code:

- The 8088 hardware suite supplied **no direct memory**, so `CPUSetDirectMemory`
  and everything gated on it - the operand fast path, the bulk string runs -
  was unreachable. A bulk-run bounds bug that indexed past guest RAM and hung
  the board passed the full suite.
- The suite supplies **no chain budget**, so it never chains a single
  instruction, and both chaining bugs above passed it too.

Both are one line in the harness. **Anything gated on a host-supplied
capability must have that capability enabled in the suite**, or the three
million encodings that would catch it never run.

## Methodology, or what actually pays

The clearest result of the campaign, and it held without exception:

**Removing work pays. Rearranging the same work does not.**

Everything that paid removed work outright - the decode cache (no fetch, no
decode), fusion and chaining (no dispatch, no tick), direct operand dispatch
(no indirect call), the PIC hint (no call), the decode struct-clear (no
writes). Every attempt to reorganize identical work into a nominally better
shape lost between 0.2% and 0.5%: address hoisting, addressing-mode tables,
EA-cycle tables, a parity table, forcing an inline. The compiler was already
doing better than the rewrite in every case.

**The inlining lottery is larger than most changes.** On this core the register
allocation of the fetch/dispatch cluster swings **±6%** between source variants
of near-identical size, on top of a 0.004% noise floor. Two builds differing by
48 bytes of code differed by 5.9% of runtime. Forcing the decision either way
was worse: `noinline` on the fused block cost 3.6%, on the decode 3.8%.
Corollary: an increment to `CPUTick` cannot be evaluated on its own, and several
small changes do not compose.

**Where bookkeeping sits matters more than what it does.** Pair fusion
implemented by tracking a predecessor across every cache hit was **−0.46%**; the
identical optimization, with the same fusion count, filled at decode time by
looking ahead two bytes, was **+1.42%**. The first charged the 68.5% that hit
the cache to benefit the 18% that pair up.

**Estimates about the per-tick loop were wrong twice, in both directions.**
Chaining was estimated at 4-6% from run lengths, then revised *down* after pair
fusion showed that removing 17.4% of dispatches was worth only 1.4% - reasoning
that the two removed the same thing. Measured, it was +7.7%. Pair fusion removes
a *dispatch*; chaining removes the whole per-tick round trip through
`PlatformTick` and the `CPUTick` prologue and epilogue for every instruction in
the run. Build it rather than reasoning about it.

**The invariant is what made this tractable.** Every serious bug in the campaign
was caught by the cycle count rather than by a test - a decode cache running
stale instructions, a chain executing past `INT 21h`, a chain running past
`HLT`. These are changes that produce subtly wrong emulation rather than
crashes, and a deterministic whole-system figure turns them into a one-line
failure at the end of a two-minute run.

## What did not help

Do not re-attempt without new information. Each was built and measured on
hardware.

| tried | result | why |
| --- | --- | --- |
| Pair fusion via predecessor tracking | **−0.46%** | Bookkeeping on every cache hit to benefit 18% of them. Decode-time lookahead is +1.42%. |
| Fused path with its own fetcher | **−9.1%** | Opened the fetch window twice for the 81% that fall through. Sharing one fetcher: +6.3%. |
| Fusing LOOP + MOV moffs as separate tests | **−8.2%** | Register pressure in `CPUTick`, paid by every instruction. One mask test over a contiguous family is fine; a chain of them is not. |
| `noinline` on the fused block | −3.6% | Pinning the compiler's choice is worse than letting it choose. |
| `noinline` on the decode | −3.8% | Same, other direction. |
| O(1) cache flush via generation in the tag | −1.0% | 2,146 flushes against 2.3M lookups. Optimizes the rare side at the hot side's expense. |
| Decoding in place to avoid the miss-side copy | −0.41% | A hot stack local beats a random 24-byte slot in a 6KB array; misses are only 7.5%. |
| 1024-entry / 128-entry decode cache | −0.4% / −0.2% | 256 is the sweet spot; the working set is small. |
| Addressing-mode lookup tables | −0.40% | Three table loads did not beat the eight-way switch. |
| Forcing `ReadOperandValue` inline | −0.48% | It is 5.5% of profile and *not* inlined; the compiler was right. |
| EA-cycle lookup table | −0.5% | The branch chain it replaced was cheaper than a load plus the override test. |
| Resolving both word addresses before either access | −0.24% | `ReadRawMemoryByte` mostly hits `direct_memory`, a plain index, so GCC already hoists. Confirms an earlier "neutral" result and explains it. |
| 256-byte parity table | ±0.0% | The four-step fold already compiles to something equivalent. |
| Sequential fetch-window resume | regression | Three stores plus a compare per instruction cost more than re-deriving the window. |
| Reading `has_debug_features` once per tick | neutral to slightly negative | |
| Removing `OperandValue.width` | ruled out | 354 sites across 16 files, but `ByteValue`/`WordValue`/`ToOperandValue`/`FromOperandValue` have **no standalone symbols** - all inlined, so the width is already folded. Would remove a byte-sized store, not a branch. |
| Refreshing the `YAX86_HOT` set | ceiling <4.7% | 95.3% of samples already execute from SRAM. |
| Batching decode (2/4/8 at a time) | ceiling ~1% | Decode is ~1.3% of runtime after the cache, and 26% of runs are a single instruction, so most speculation would be discarded. |
| RP2040 hardware interpolators | ruled out | Three SIO accesses against three ALU instructions for `(seg<<4)+off`, and the segment changes per access so the pipeline cannot stay configured. Also a shared peripheral a display driver would want. |
| Dynamic translation to Thumb | ruled out | Not on memory - ~100KB SRAM is free - but the decode-cache result shows the working set churns enough that translations would not amortize. |
| Decode on the second core | ruled out | Same handoff cost as the decode cache, plus synchronization, plus discarding work on every branch. |
| Second core for device emulation | ceiling ~5% | Devices are ~5% of core0. It is a display and audio argument, not a CPU one. |
| Interrupt hint without a `NULL` fallback | broke 3 CPU tests | Mock hosts do not maintain the flag, so interrupts were never taken. |
| 96KB guest RAM | does not boot | MS-DOS 3.30 wants 128KB. |
| Overclocking before USB init | bricked the board until BOOTSEL | Overclock *after* USB is up, guarded by a watchdog plus an `__uninitialized_ram` marker. |

Two measurement notes worth keeping:

- `watchdog_caused_reboot()` is **true after every `picotool` flash**, because
  picotool reboots through the watchdog. Distinguishing a real hang needs the
  `__uninitialized_ram` marker.
- `PICO_FLASH_SPI_CLKDIV` must be set with `add_compile_definitions()` **before**
  `pico_sdk_init()`. boot2 is a separate target (`bs2_default`), so
  `target_compile_definitions` never reaches it - this was the cause of a
  400MHz hang that looked like an unstable overclock.

## Hardware envelope

Measured on one board at room temperature. Silicon lottery applies.

- **306MHz is the lowest clock that clears parity** (101.0%); 300MHz reaches
  99.0% and misses by 60ms over a 5.87s run.
- **306MHz runs at the stock 1.10V core voltage.** The harness had been setting
  `VREG_VOLTAGE_1_30` - the regulator's ceiling - for anything above 250MHz;
  1.20, 1.15 and 1.10V all give identical times, and 1.10V repeated three times
  is stable. Dynamic power goes as V², so this is ~28% less core power than the
  1.30V assumption. **The threshold in `main.c` should be revisited** rather
  than left at 250MHz.
- **Not every frequency exists.** `sys_clk = 12MHz x N / (postdiv1 x postdiv2)`
  with the VCO in 750-1600MHz. 305, 310 and 320 have no solution; the harness
  correctly reports `PLL cannot produce that clock` and falls back to stock
  rather than hanging. Valid near the target: **300, 306, 308, 312, 315**.
- Cost per instruction creeps up with clock - 750 at 125MHz, 758 at 400 -
  because flash access for non-hot code and USB servicing do not scale. This is
  why the honest parity clock is ~303MHz rather than the 300 a linear
  extrapolation from the stock-clock figure gives.
- Community reports put RP2040's reliable ceiling at ~250MHz with flash
  misbehaving in the 250-300MHz band; `PICO_FLASH_SPI_CLKDIV=6` above 250MHz is
  what gets us through it. 1.30V is the regulator's hard maximum, so ~400MHz is
  the physical wall.

## Prior art: picocalc_x86

<https://github.com/shtirlic/picocalc_x86> - an 8086tiny-derived emulator for
the same PicoCalc target. **RP2350 RISC-V Hazard3 at 250MHz**, explicitly no
ARM plans, so it is not a drop-in comparison. GPLv3, so the code cannot be
borrowed; the techniques can.

It claims *"around ~1 MIPS"* with no stated workload or methodology, which works
out to ~250 host cycles per emulated instruction against our 750. Before
treating that 3x as real, note that our figure is a whole-system boot counted by
the emulator itself and theirs is unqualified.

What is genuinely different, in increasing order of likely importance:

- **No cycle accounting at all** - zero occurrences of "cycle" in their
  1,256-line CPU - and the **PIT runs off a host hardware alarm**
  (`alarm_pool_add_repeating_timer_us`), so the emulated timer ticks on wall
  clock while the CPU runs flat out. We deliberately do the opposite, so that a
  guest calibrating a delay loop against the timer gets the period-correct
  answer. Indirect evidence puts our whole timing apparatus at 10-15%, not 3x.
- **One function, computed goto, 58 dispatch targets.** `static const void
  *dispatch_table[58]` of `&&OP_n` label addresses in scratch RAM, with 256
  opcodes collapsed into 58 families by `TABLE_XLAT` before dispatch. Ours is 26
  handler functions across 19 files behind `ExecuteDecodedInstruction`.
- **Hazard3 has 32 registers; Cortex-M0+ gives most Thumb instructions 8.** This
  is the constraint behind our inlining lottery and the `noinline` result, and
  nothing in software fixes it.

**This corrects an earlier conclusion in these notes.** Computed-goto threading
was dismissed as "a trap - 256 handlers in one stack frame would be far worse."
It is not 256 handlers; the family collapse is what makes flat dispatch
tractable, and that transformation was not considered. It remains harder on 8
registers than on 32 and is unproven here, but it is now the most interesting
untested idea, and the only one that plausibly explains a multiple rather than a
percentage.

Their display solution, which also refutes an earlier claim that 80-column text
cannot work on a 320px panel: a purpose-drawn **4x10 font** (80 x 4 = 320
exactly, 25 x 10 = 250 rows plus margins for aspect correction), an 8x8 font for
40-column and 320x200 graphics, a **scanline renderer** with V-blank and retrace
timing, and a **PIO serializer** adapted from `st7789_lcd` sustaining 62.5 Mbps
at 125MHz sysclk. The PIO program ports to RP2040 unchanged. They push pixels
through a per-pixel callback rather than DMA, which is the one place the design
looks improvable.

## Open questions

- **What the display costs.** Every figure in this file is for a machine that
  draws nothing - `pico/` contains only `bench`, which fakes keystrokes and
  never renders. Until a scanline renderer and PIO driver exist, the parity
  clock is a CPU-only number. **Measure this before optimizing further**,
  otherwise the next round tunes against a benchmark that omits the display -
  the same mistake as tuning to a spin loop, one level up.
- **The last 43 cycles.** Constant, reproducible, not interrupt-related.
- **Flat dispatch with a family collapse.** The one idea that might be worth a
  multiple. A rewrite of the execute path, not an increment.
- **Lazy flags**, now worth ~0.9% rather than ~1.8% because fusion already
  captures half of it: only 15.7% of ZF/SF/PF writes are ever read back, but
  fusion consumes 49.4% of all flag writes at the point they are set. Scope it
  to ZF/SF/PF - CF is read constantly here and is written from 41 scattered
  sites.
- **Idling the host core.** The DOS idle skip advances the emulated clock but
  never sleeps the RP2040. On a battery device, `__wfe()` until the next device
  deadline would likely save more than the overclock costs.
