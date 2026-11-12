# core/src/cpu - the CPU

Why the CPU is shaped the way it is, and what the alternatives measured.
Figures are `dos-boot` on the RP2040, as the root `AGENTS.md` describes;
`core/AGENTS.md` covers the rest of the core.

Most of this is about cost on a Cortex-M0+: no predictor, no speculation,
few registers, a 5-bit immediate offset in `ldr`, and an ABI that returns
anything over four bytes through memory. **Anything on this path that
round-trips through memory costs more than its instruction count suggests.**

## State and config

- **`YCPUConfig` is a field of `YCPUState`**, filled in before `YCPUInit()`
  like every module's. Held by value: a pointer cost **0.34% at `-O3` and
  3.37% at `-O2`** on a path reading config every instruction, and made the
  config an object the caller had to keep alive.
- **Three kinds of field, told apart by what may write them:**
  - **Config** - constructor arguments, checked in `YCPUInit()` and read-only
    after. `decode_cache_num_entries` is read only there, so changing it later
    changes nothing.
  - **Plain state** - `registers`, `flags`, `instructions_retired`,
    `cycles_this_tick`. Nothing derives from them, so tests and debuggers
    write them directly (about 1,400 writes in the tests).
  - **Coupled state** - fields another field's correctness depends on, each
    with a setter as its sole writer: `YCPUSetDirectDataWindow()` and
    `CPUInvalidateInstructionFetchWindow()`, the same discipline as
    `PlatformUpdateEnabledFlags()` and `YPICUpdateUnmaskedRequest()`.
- The direct data window's setter is coupled state, not an aberration: the
  window is derived, and its `data`/`end` pair carries an invariant. A
  setter that passed constructor arguments would be wrong; this one is not.
- **A per-call value is an argument, not a field**: `YCPUTick()`'s
  `max_run_cycles` changes every tick, so it does not go in the config.
- **Where a field sits is worth 1.0-1.7%.** `ldr` encodes about 124 bytes of
  offset, so a field past that costs extra instructions. Config (~70 bytes)
  first measured **1.04% slower at `-O3` and 1.69% at `-O2`** than config
  last, and the 256-byte `code_page_generation` after the decode cache
  fields is worth another 0.56% at `-O2`. Large or cold members go last.

## Instruction decoding

- **`YCPUFetchNextInstruction()` decodes in place** into the caller's
  `YCPUInstruction`. Filling a local and copying it out cost about 19% of
  wall time on a desktop, far more than its 3.5% of instructions: a struct
  written and read straight back makes dependent store-to-load pairs that
  stall, which callgrind and cachegrind do not model.
- A failed fetch leaves partial fields behind; every caller treats it as
  fatal.
- **Each field is settled where it becomes known, not zeroed up front**:
  `opcode`, `size` and `immediate_size` on every success path, `has_mod_rm`
  and `displacement_size` in both arms of the ModR/M branch, `mod_rm` read
  only under `has_mod_rm`, and the arrays no further than their sizes. Only
  the two prefix fields are cleared first. Worth **4.63% at `-O3`** and 3.96%
  at `-O2`. It is safe because a reused destination - `YCPUTick()` reuses one
  for the life of the CPU - keeps nothing of the last instruction, which
  `ADecodeKeepsNothingOfThePreviousInstruction` pins.
- **No field of `YCPUInstruction` is a bitfield**: whole bytes put it at 16
  bytes and are worth **0.95% at `-O3` and 2.18% at `-O2`**, with a core
  112-120 bytes smaller. A packed field costs a read-modify-write to store and
  a shift and mask to load, and `YCPUModRM` is read for every memory operand
  and every instruction's cycle count. **Its size does not matter**: nothing
  copies one per instruction, so do not re-derive a rule that it should be
  small.
- `has_mod_rm` and `displacement_size` are cleared in the ModR/M branch's
  `else`, written once whichever arm runs. Assigning `has_mod_rm` from
  `metadata->has_modrm` costs 0.12%; writing all three after the branch
  0.60%.
- **The ModR/M `REG` field is read before it is known to exist**, by
  `YCPUGetImmediateSize()`. Only `0xF6`/`0xF7` use it and both have a ModR/M
  byte, but it must be defined, so it lives in a local initialized to 0 -
  cheaper than a branch or a store.
- `immediate_size` and `displacement_size` can express more bytes than the
  arrays hold, which `gcc -O3` warns about. The immediate loop clamps to
  `Y_CPU_MAX_IMMEDIATE_BYTES`, and that clamp is load-bearing.

## Instruction fetch

- **The host fills in `YCPUState.instruction_fetch_window`** through
  `YCPUConfig.get_instruction_fetch_window` - a `data` pointer and the
  half-open range it covers - and the fetch reads bytes straight from it.
  Calling `read_memory_byte` per byte instead is worth **9.8%**.
- The host hands back the whole region the address is in, so a jump
  backwards stays inside (`WindowCoversTheRegionBeforeTheFetchAddress`).
  `data == NULL` declines.
- The window persists across instructions as a range, so reopening is a
  compare. `YCPUInstructionFetchState` is the per-decode cursor, holding no
  pointer to the CPU's IP: IP must name the instruction being decoded until
  `YCPUTick()` advances it, and byte-wise writes to `cpu->registers[]` cannot
  stay in a register.
- **A cached pointer is safe where cached bytes would not be**: writes are
  seen by the next fetch, so self-modifying code works. Nothing invalidates
  the window, because map entries cannot overlap, change or be removed. Add
  unregistration and that stops being true.
- The window stops at a segment wrap: IP wraps at 16 bits where the linear
  address does not, so bytes past it take the ordinary path.
- The 8088 suite runs with a window; the mock configs in `cpu_test.cpp` run
  without.

## The decode cache

- **`YCPUConfig.decode_cache` keeps decoded instructions keyed by linear
  address**, so a loop is decoded once. Worth **10.86% at `-O3`** and 10.12%
  at `-O2`, the largest single win, which took `-O3` past a real 8088.
- **A hit is used in place**: the executor gets `&entry->instruction`, and a
  miss decodes straight into the entry. Copying measured **3.25% slower**.
  Safe because no handler writes through its `YCPUInstruction`.
- The storage and its size are constructor arguments in `YCPUConfig`. A
  compile-time array would put the size of `YCPUState` behind a macro, a
  mismatch waiting between the library and its includer.
- `YCPUState.decode_cache` is whether the CPU may use it, derived by
  `YCPUInit()` and `CPUSetDecodeCacheEnabled()` (its sole writer), and NULL
  for a rejected count or a cache withdrawn for a watchpoint.
  `YCPUInvalidateDecodeCache()` walks `config.decode_cache`, so entries are
  discarded even while withdrawn
  (`InvalidatingReachesEntriesWhileTheCacheIsAway`).
- **The count must be a power of two of at least two**: the index is a mask,
  since a remainder by a runtime value is a division the part lacks.
- **The fetch tests two guards and both earn their place**: the config
  pointer is the host's, and the mask is zero where `YCPUInit()` rejected the
  count. Dropping the second measures **1.9% slower**.
- **A decode records its cost**: `YCPUInstruction.base_cycles` is the
  opcode's base cost plus the effective address time, both fixed by the
  encoding, so a hit charges from a field. Worth **2.83% at `-O3`** and
  3.13% at `-O2`; `AHitChargesForTheAddressItComputes` and
  `ARunStopsAtTheBudget` catch it going stale.
- **The base cost is read through the decoder's `metadata` pointer**, from
  the padding of `YCPUOpcodeMetadata`. Indexing `YCPUOpcodeTable` from the
  caller instead is **0.63% slower at `-O3`** and 0.65% at `-O2`: the read is
  now cold, once per decode, and a 1-byte field at an 8-byte stride spreads
  256 costs over 2KB.
- **The fetch and run loop carry a `YCPUDecodeCacheEntry*`**, not the
  `YCPUInstruction*` inside it, though `YCPUTick()` reads only
  `->instruction`. Carrying the instruction pointer costs **0.68% at `-O3`**
  and 0.10% at `-O2`:
  one entry pointer addresses every field within the 5-bit `ldr` offset,
  where the instruction pointer leaves two live pointers across the
  back-edge, growing the frame from 52 to 60 bytes with a spill and reload.
  `nm` showed identical placement, and three spellings measured within
  0.005% of each other, so the shape is what costs.
- `base_cycles` is `uint16_t` though 46 is its largest value: a byte is
  **0.51% worse at `-O3`** and 0.55% at `-O2`, and saves no space - `address`
  leads the entry, which packs an 18-byte instruction into 24 bytes either
  way.
- **The fetch has exactly one call to the decoder.** A second call site -
  the obvious early return for the no-cache path - makes `-O2` stop inlining
  it into flash behind a veneer, **2.54% slower at `-O2`**. That is why the
  no-cache path joins the cached one instead of returning early.
- **512 entries, 12KB, sized on real programs rather than `dos-boot`.** On
  the PicoCalc at 640KB, seven commands compiling and running programs under
  DOS (`pico/bench`'s PicoCalc workloads) took 174.1 seconds of processor
  time with 512 entries against 179.7 with 256, and QBasic interpreting a
  program 77.7 against 81.5: an interpreter's loop is larger than a boot's.
  `dos-boot` hardly cares: with 512, 0.27% faster direct and 0.13% slower
  paged, and 128 was 0.83% slower than 256. 1024 entries saved another 0.4%
  on the seven commands, not worth 12KB more, which would leave the PicoCalc
  3KB of SRAM.
- **The cache's size moves `dos-boot`'s paged count**, from 2,356,813
  instructions and 27,964,698 cycles at 256 entries to 2,356,942 and
  27,966,123 at 512, and none of it is emulation: with
  `Y_CPU_MAX_INSTRUCTIONS_PER_TICK` at 1, both give 2,356,713 and 27,963,388.
  A run continues only into cached code, so the size changes where runs end,
  and with them where the bench's screen poll lands. The direct count
  happened not to move.
- **`code_page_generation` is the whole invalidation mechanism**: one byte
  per 4KB page, recorded by each entry and compared on use. Zero says the
  cache holds no decode from the page; anything else is a wrapping count of
  the writes to it since one was cached. The CPU counts in
  `YCPUWriteRawMemoryByte()`, where operands, stack pushes and vector writes
  end up, so a host reports only writes made some other way - the platform
  does, in `YPlatformWriteMemoryByte()`, for DMA. Without it DOS loads over
  the boot sector unseen: the first prototype retired 13,247,281
  instructions.
- **A count coming back round discards that page's decodes and no others**,
  since an entry taken 255 writes ago would look current.
  `YCPUInvalidateDecodeCachePage()` walks the cache for them and leaves the
  page at zero, so its writes go uncounted until an instruction is cached
  from it again. A stack or data page therefore never causes a walk.
  Discarding every decode instead, and counting writes to every page, is
  what this replaced: the stack alone wrapped a counter every 256 pushes,
  and QBasic interpreting a program decoded 50.6% of the instructions it ran
  where it now decodes 32.7%; two compilers and a linker went from 44-45% to
  28-30%. On the PicoCalc at 640KB the seven commands took 156.6 seconds of
  processor time against 174.2, and QBasic 69.5 against 77.7. `dos-boot`
  gained 2.86% paged and 2.52% direct.
- **Discarding less moves both `dos-boot` counts**: paged from 2,356,942
  instructions and 27,966,123 cycles to 2,353,177 and 27,922,089, and direct
  from 2,328,128 and 27,702,858 to 2,328,231 and 27,704,200. It is where runs
  end again, not emulation: with `Y_CPU_MAX_INSTRUCTIONS_PER_TICK` at 1 both
  sides give 2,356,713 and 27,963,388 paged, and 2,328,015 and 27,701,507
  direct.
- **Not caching is as load-bearing as caching**: an instruction that spills
  onto the next page or off the end of the segment is not cached, since its
  page says nothing about its later bytes. A failing decode clears the entry's
  claim before starting, or the untouched address tag would offer the
  wreckage as the old instruction.
- What discards every decode is `YPlatformUpdateAfterMemoryMapChange()`: a
  registration changes which region owns an address without writing anything.
- **The 8088 suite supplies a cache** but each test runs one instruction from
  cold, so it checks the fill; hits are covered by `decode_cache_test.cpp` and
  the `dos-boot` invariant. Of eleven deliberate breakages, ten are caught
  only by the two cache test files.
- `YCPUWriteRawMemoryByte()` stays unmarked: `Y_HOT` is **0.71% worse at
  `-O3`** and 0.68% better at `-O2`, and `-O3` decides.

## The direct data window

- **Operand, stack and interrupt-vector accesses index
  `YCPUState.direct_data_window`** rather than calling into the host, which
  was two calls, one indirect, and a map lookup. Worth **3.28% at `-O3`** and
  5.78% at `-O2`, with a core 552 and 712 bytes smaller. It covers everything
  that goes through `YCPUReadRawMemoryByte()` and `YCPUWriteRawMemoryByte()`.
- **It is set, not asked for**: data accesses are scattered, so a callback
  per access would have no locality to use. The host calls
  `YCPUSetDirectDataWindow()` once.
- **`end` is how far memory is directly indexable, not how much there is.**
  A host whose memory is partly behind a bus or driver hands over the part
  that is, and serves the rest through the callbacks and the memory map. Do
  not reintroduce the assumption that the window is all of RAM.
- `end` is exclusive, like the fetch window's, and there is no `start` -
  guest RAM begins at 0 - so the bounds test is one unsigned compare.
  `end` is 0 exactly when `data` is NULL, so the compare needs no null check;
  set it only through `YCPUSetDirectDataWindow()`. A map entry's `end` is
  inclusive, hence the platform's `+ 1`.
- The platform derives it from the entry covering address 0, only while that
  entry reads and writes the same buffer - video memory writes through a
  callback, so it is correctly excluded. Registration recomputes it.
- A word straddling the top has its halves take different paths
  (`WordStraddlingTheTopOfTheWindow`).
- **Outside the direct window, an access is a call to
  `YCPUReadMemoryByteOutsideDirectWindow()` or
  `YCPUWriteMemoryByteOutsideDirectWindow()`**, both `Y_HOT` and `Y_NOINLINE`,
  which try the paged data window and then the host's callback. They are the
  slow halves of `YCPUReadRawMemoryByte()` and `YCPUWriteRawMemoryByte()`,
  which are inlined at every memory access, so each inlined copy carries only
  the direct window's compare and load and a call. Measured with paging off,
  with `dos-boot` (`pico/bench`, RP2040 at 250MHz, `-O3`): the paged test
  inlined into every access cost 2.6%. With the callback moved out of line as
  well, it is **2.6% faster** than with the callback inline: 4.6269 seconds
  against 4.7458. The helpers in flash rather than SRAM measured 0.26% slower.
- The 8088 suite supplies a window - a sabotaged read fails within one opcode
  - and `CPUTestHelper` and the mocks run without one.

## Prefix decoding

- **Each prefix group is one masked compare**: segment overrides are
  `001ss110` and LOCK/REP `111100rr`. `ss` is in the order `Y_CPU_ES` to
  `Y_CPU_DS` are numbered, so the register is an offset from `Y_CPU_ES`.
  Worth 5-8% of host instructions below `gcc -O3` and `clang -O2`; at or
  above them both compilers derive it themselves.
- Prefixes are decoded into fields as they are fetched.
  `segment_override` uses 0 (`Y_CPU_NO_SEGMENT_OVERRIDE`, which is `Y_CPU_AX`
  and never a segment) for none, so a zeroed instruction is correct. LOCK is
  consumed and not recorded.
- **Classify and record in one pass**: `YCPUApplyPrefixByte()` returns whether
  it consumed the byte. Splitting the two costs 0.7% on clang.
- Test `0xF0`-`0xF3` as a range, not a mask - one subtract and compare - and
  before the segment test, since most bytes are not prefixes and run both.
- `Y_CPU_MAX_PREFIX_BYTES` bounds a run of prefixes, which would otherwise
  fetch forever - a real 8086 hangs there too. 247 prefixes plus 8 bytes of
  instruction is the 255 a `uint8_t` `size` can add to IP. `LOCK ES: REP
  MOVSB` is legal, so it has to be generous.

## The execute path

- `YCPUExecuteInstruction()` checks an instruction against the opcode table;
  `YCPUTick()` calls `YCPUExecuteDecodedInstruction()` directly, because its
  own fetch derived both checked fields from that entry. The public entry
  keeps the checks for hand-built instructions.
- **Every table entry has a handler.** The eight prefix bytes can never reach
  `opcode` after a fetch; they point at `YCPUExecuteInvalidOpcode()` for a
  hand-built instruction. `EveryEntryHasAHandler` keeps a later entry from
  being a call through null.
- **`YCPUExecuteDecodedInstruction()` is `Y_NOINLINE`**: inlined into
  `YCPUTick()` it is 31% slower, both spilling. The effect only appears with
  the hot path in SRAM. Worth 1.84% at `-O3` and 4.58% at `-O2`.

## Operand dispatch

- **A `switch` on width and an `if` on address type, not function pointer
  tables.** Worth **3.20% at `-O3`** and 1.89% at `-O2`: an indirect call is
  two loads and an unpredictable branch, more than the handlers it reached.
  The larger half is second-order: a function in a table must exist out of
  line, and without the tables seven of eight leaf handlers vanish - the core
  is **1,208 bytes smaller at `-O3`**.
- Check whether a table's callers really index it: `instructions_mov.c` used
  `kReadImmediateValueFn[Y_CPU_WORD]`, a constant index, which is a direct
  call written the long way.
- **Switch on the width but not the address type.** `YCPUWidth width : 1` has
  no value outside the enum, so its `default` compiles away and the switch is
  one bit test. `YCPUOperandAddress.type` is a whole byte under
  `-fshort-enums`, with 254 values outside the enum, so a switch on it emits a
  live third branch: **1.06% at `-O3`**. Narrowing it to a 1-bit field makes
  the switch free but every operand writes the field, a read-modify-write:
  **0.22% worse** (0.10% at `-O2`), with a larger core. The switch on the
  whole byte is 0.70% at `-O2`. Both were measured; do not "tidy" either. The
  distinction is bitfield width, not enum size.
- `YCPUOpcodeMetadata` is four byte fields then the handler pointer, 8 bytes
  with no padding, a 2,048-byte table read with single `ldrb`s, and no
  redundant `opcode` field.
- `-Os` loses 1.00% to the dispatch, recovered by `Y_ALWAYS_INLINE` on the six
  wrappers at a cost of 0.26%/0.06% at `-O3`/`-O2` - not taken.
- `YCPUReadImmediateOperand()` is the one wrapper left out of line, in flash.
  `Y_HOT` is 0.11% better at `-O3` and 0.35% worse at `-O2`; unmarked.

## Operand values

- **An operand value is a number, not a struct**: `YCPUOperandValue` is a
  `uint16_t` passed by value. As a width plus a union it was a struct in
  memory - **23.15% at `-O3`** and 9.72% at `-O2`, the largest result here,
  with a core 3,236 bytes smaller at `-O3` and 1,972 at `-O2`.
- **The mechanism is the ABI**: AAPCS returns a composite over four bytes
  through memory, and Thumb-1 GCC turns a small copy it cannot do in aligned
  words into a call to `memcpy`. Reading a register operand came out as
  `memset(10)`, `memcpy(4)` and `memcpy(10)` through veneers into flash -
  5.4% of `dos-boot`, found by an on-target profile. **When a structure on
  this path is suspect, look for calls to `__wrap_memcpy` before theorizing.**
- **A byte value has a zero high byte**, so widening is free. Every byte
  producer truncates; `YCPUReadRegisterOperandByte()` is the one that has to,
  and eight unit tests and the 8088 suite fail without its cast. **Sign
  extension takes the width explicitly**, `YCPUFromSignedOperandValue()` and
  `YCPUFromSignedOperand()`.
- An unreachable width arm returns `Y_CPU_UNREACHABLE_OPERAND_VALUE`,
  `0xFF` - open bus, and legal at either width where `0xFFFF` is not.
- **Sign extension is one test, byte or not**, not a switch with a return
  for the values outside the enum a width can hold. That return is a third
  path, and inlined into `IMUL` and `IDIV`, GCC carries it on to their next
  table lookup by width and warns that the index can be 2 (`-Warray-bounds`
  on every Pico build). Taking it out also measured **0.31% faster at
  `-O3`**, but not from sign extension, which only `SAR`, `NEG`, `IMUL` and
  `IDIV` use: the four shrank by 216 bytes, and GCC then stopped inlining
  `YVideoGetMode()` into `YPlatformWriteMemoryByte()`, both in SRAM, which
  made the latter 148 bytes smaller. An inlining side effect, which a later
  change can move again.
- **Never initialize an operand with a designated initializer** - `YCPUOperand
  operand = {.address = {...}}`: C zeroes the unnamed members, a `memset` of
  the whole struct. Assigning fields one at a time is **3.46% at `-O3`** from
  that one function.
- **`YCPUOperandAddress` is flat and four bytes**, so it returns in `r0`.
  Both arms of the old union were a register index and a small number, so
  they share fields: `register_index` (the register, or the segment for
  memory) and `offset` (0 or 8 within the register, or the effective
  address). Against master:

  | | `-O3` | `-O2` |
  | --- | --- | --- |
  | passing the address by pointer | +11.07% | +6.68% |
  | shrinking it to four bytes | **+15.67%** | **+22.67%** |
  | both together | +8.83% | +21.16% |

  **Making a hot struct fit the return register beats routing it through
  memory, and the two fight**: once it fits, an out-parameter forces a store.
- After this the core calls `memcpy`/`memset` nowhere - check with:
  ```sh
  arm-none-eabi-objdump -d build-pico/O3/yax86_pico_bench.elf |
      grep -cP 'bl\t2000....\s<____wrap_mem'
  ```
- `YCPUMemoryAddress.segment_register_index` is a `uint8_t`, since
  `YCPUApplySegmentOverride()` writes it through a pointer and an enum's
  width varies by target - four bytes in the native tests.
- **`YCPUOperand` (six bytes) goes back through a pointer**, the seven readers
  taking a `YCPUOperand*` last - and that pays only with `Y_ALWAYS_INLINE` on
  `YCPUReadOperandValue()`:

  | | `-O3` | `-O2` |
  | --- | --- | --- |
  | the out-parameter alone | **-5.46%** | -1.07% |
  | the inline mark alone | +0.11% | +1.98% |
  | both | **+0.97%** | **+4.15%** |

  Alone, the out-parameter shrank `YCPUReadRegisterOrMemoryOperand()` enough
  that GCC stopped inlining `YCPUReadOperandValue()`, which lives in flash -
  every operand read grew a veneer - at 94 bytes against 244, the smaller
  version was the slower. **Check what a changed function calls, not only
  its size.** The mark costs 608 bytes at `-O3` and 624 at `-O2`.

## What an instruction costs

- Cycles are a base per opcode, plus effective address time, plus four per
  byte moved over the 8-bit bus. The first two are fixed by the encoding and
  cached in `base_cycles`; the third is charged from the accesses that
  happen, so an instruction touching memory it has no reason to is billed.
- **A store resolves its destination with
  `YCPUGetRegisterOrMemoryOperandAddress()` and writes with
  `YCPUWriteOperandAddress()`**, never reading it first: a read makes `MOV
  [BX],AX` 23 cycles instead of 15. No architectural test sees this - only
  `cycles_test.cpp`, which asserts `cycles_this_tick`.
- **Any change to cycle costs moves the `dos-boot` invariant**: cheaper
  stores give the guest's timer loops more turns per PIT tick, so retired
  instructions go up as cycles go down.

## Bulk string runs

- **A `REP MOVS` or `REP STOS` entirely inside the direct data window runs as
  a loop over it**, not element by element through operand machinery and a
  function pointer. Worth **3.10% at `-O3`**. `REP SCAS`/`CMPS` read flags
  per element and stop on them, so there is no run to plan; `REP LODS` would
  do nothing one iteration does not.
- **The element path is the definition**: the bulk path must leave identical
  memory, registers, flags and cycles, and `string_bulk_test.cpp` runs each
  case both ways and compares all four. Each of eight deliberate breakages is
  caught by the test aimed at it.
- **A run is declined rather than made to work**: a zero count, an offset that
  would wrap its segment, or any byte outside the window. The segment wrap is
  tested separately, in a segment based part way up memory, because on most
  hosts it coincides with the window bound.
- **Overlap needs nothing special**: the loops visit elements in the element
  path's order, so a forward `REP MOVSB` from `SI` to `SI+1` propagates one
  byte as hardware does. The word loop reads both bytes before writing,
  which matters only at a one-byte overlap.
- **All four loops walk pointers, not indexes**: indexed, `gcc -O3` turns the
  byte `STOS` into a `memset()` call - the same speed, but a libc link
  dependency the core does not otherwise have. The copy loops are safe for a
  second reason: the step is a runtime value, so the compiler cannot prove
  the ranges do not overlap. Specializing on direction would hand it that
  proof, and a `memcpy()` may copy in any order.
- A synthesized `mem*` call is not the operand-path defect above: there the
  issue was speed, here libc dependence.
- Each bulk run sits beside the iteration it replaces; both inline at `-O3`.
- **`YCPUPlanBulkStringRun()` stays out of line in flash**: `Y_HOT` is 0.00%
  for 296 bytes of SRAM, and `Y_ALWAYS_INLINE` 0.73% slower, though it makes
  the core 216 bytes smaller. It runs once per
  repeat, not per element - hot by name, not by traffic.
- **A run reports one write per 4KB page to the decode cache**, which is all
  a generation change needs, and wraps the counter after 256 runs rather
  than 256 bytes. That moved the invariant without changing emulation:
  fewer flushes change where runs end, so the Enter answering DOS's date
  prompt lands at a different emulated time. With
  `Y_CPU_MAX_INSTRUCTIONS_PER_TICK` at 1 both sides give 2,328,015 /
  27,701,507. Reporting per byte restores the old invariant and costs 0.13%.

## The interrupt request hint

- **`YCPUConfig.interrupt_request_hint` is a `const bool*` read at every
  boundary with interrupts enabled**; while it reads false the acknowledge
  cycle - an indirect call that asks the PIC and is told nothing - is
  skipped. Worth **5.97% at `-O3`** and 4.17% at `-O2`.
- It points at `YPICState.has_unmasked_request`, `(irr & ~imr) != 0`.
  **Conservative, not exact**: it ignores priority and in-service, so it can
  be true when the PIC declines. **It may never be falsely false** - that
  strands an interrupt silently -
  `HintReadingFalseSuppressesTheAcknowledgeCycle` states the contract.
- `YPICUpdateUnmaskedRequest()` is the sole writer, called from all six sites
  that change `irr` or `imr`: `YPICInit()`, `YPICRaiseIRQ()`,
  `YPICLowerIRQ()`, ICW1, OCW1 and the acknowledge.
- **A pointer into the PIC's state, not a value handed over**, so a change is
  seen at the next instruction with nothing to synchronize.
- A dropped recompute leaves the flag conservatively true - a pure slowdown
  no behavior test catches. `interrupt_request_hint_test.cpp` covers it,
  comparing against `irr & ~imr` rather than literals. PIC tests set the mask
  through OCW1, as a guest does: assigning `imr` leaves the flag stale.
- A slave's request reaches the CPU through the master's cascade line, which
  `YPICRaiseIRQ()` raises by the ordinary path.
- A host that cannot promise the flag leaves it NULL and is asked every
  time, as `cpu_test.cpp`'s mocks and `CPUTestHelper` do.

## Runs of instructions

- **A tick from `YPlatformRun()` executes up to
  `Y_CPU_MAX_INSTRUCTIONS_PER_TICK` instructions back to back**, removing
  the per-tick round trip - `YPlatformTick()`'s deadline and stop checks and
  `YCPUTick()`'s prologue and epilogue. Worth **9.17% at `-O3`** and 9.74% at
  `-O2`.
- **Only the first instruction of a run is fetched**; the rest come from the
  decode cache, so a cold CPU runs one per tick whatever its budget, and the
  8088 suite stays a one-instruction test. A run continues through a branch
  into cached code.
- **`max_run_cycles` is the bound and the switch: zero runs exactly one
  instruction.** `YPlatformTick()` passes zero; `YPlatformRun()` passes the
  time to the next device deadline. It is an argument because it changes
  every tick.
- **The instruction cap only bounds how coarse a tick can be** when the next
  deadline is far; what it costs is `YPlatformRun()` overshooting by up to a
  run. It was 4, from straight-line runs averaging 3.65, but runs continue
  through taken branches into cached code. Measured in emulated MHz, which
  normalizes for each cap's own invariant: 2 gives 5.349, 4 5.554, 8 5.657, 16
  5.719, 32 5.746. **16 is where it stops paying** - 2.9% over 4, where 32
  adds 0.5% for twice the overshoot.
- **A run ends at whatever the end of a tick would have handled**: the
  budget, a self-raised interrupt (`has_pending_internal_interrupt` - `INT
  n`, `INTO`, divide error), a requested stop, `HLT`, TF, and the interrupt
  hint. Without a hint it ends the run rather than guessing.
- **A port access gets a tick to itself**: a run ends after one, since it can
  move the deadline, and does not continue into one, since a port handler
  calls `YPlatformSync()` and mid-run the clock lags the instructions already
  run - `IN AL, 0x40` two into a run reads a count two instructions stale.
  `IN` and `OUT` are 0xE4-0xE7 and 0xEC-0xEF, differing only in bit 3, so
  `(opcode & 0xF4) == 0xE4` matches exactly those. Two `& 0xF8` compares do
  not work: `0xE4 & 0xF8` is `0xE0`, so both arms are dead.
- **A host that needs to stop somewhere drives `YPlatformTick()`** and
  inspects between calls. The platform has no breakpoints or step mode.
- **Batching equals stepping**:
  `ABatchedRunIsIndistinguishableFromSteppingIt` compares tick count, retired
  count and registers exactly, with a program that accumulates PIT reads and
  puts a port read one instruction into a run.
- **`dos-boot` moved by 107 instructions and 1,266 cycles, none of it
  emulation**: with the cap at 1 the branch reproduces 2,328,015 /
  27,701,507. The overshoot moves where the harness's screen poll lands,
  and with it when the date prompt's Enter is pressed - the same effect as
  the poll interval itself, measured at 11,816 instructions.
- **Measure this loop against the base it lands on**: reporting the invalid
  instruction from a `Y_NOINLINE` helper was 10.7% at `-O2` on the first
  decode cache and a wash on the one that landed (0.12% better at `-O3`,
  0.25% worse at `-O2`), so the simpler locals stayed - the helper also
  relies on no handler touching CS or IP before rejecting. Hoisting
  `executed_instruction` out of the loop is neutral; GCC already does.

## Instruction counting

- The count is what the CPU ran; a halted tick retires nothing. It lives in
  the CPU because `YCPUTick()` knows which it did: 0.17% under callgrind,
  against 1.09% driven from `YPlatformTick()`.
- **Two `uint32_t` halves, not a `uint64_t`**, read by
  `YCPUInstructionsRetired()`. A 64-bit increment is eight instructions on a
  Cortex-M0+ in the hottest block; the low half is three, and the carry a
  branch not taken for 2^32 instructions. Worth **1.19% at `-O3`** for 16
  bytes. It must reach 64 bits: a running machine passes 2^32 in hours.
  `TheRetiredCountCarriesIntoItsHighHalf` starts just below the boundary.
- **Counting once per run does not pay**: 0.11%, and a constant budget 0.31%
  slower. A run averages 2.29 instructions (2,328,128 in 1,018,378 ticks), so
  moving work from per-instruction to per-tick has to clear that ratio.
