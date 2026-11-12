// On-target benchmark harness for the yax86 core on the Raspberry Pi Pico.
//
// The RP2040 is the platform the emulator is meant to fit on, and it is not a
// small desktop: a Cortex-M0+ has no branch predictor, no speculation and no
// cache except the 16KB XIP window that code executes from flash through. A
// change measured on x86-64 says very little about how it lands here, which is
// what this exists to find out.
//
// There is one workload, dos-boot: power on the machine, let GLaBIOS POST,
// boot MS-DOS 3.30 off a floppy image in flash, and stop when the command
// prompt appears. It is the whole machine - CPU, PIT, PIC, video, FDC - rather
// than a CPU loop, and it is deterministic, so the emulated cycle count it
// reports is an invariant: if that number moves, the change altered behaviour
// and the times either side of it are not comparable.
//
// Guest memory is laid out as pico/picocalc lays it out, by whether the
// PSRAM answers at startup (yax86_pico.h): on a PicoCalc, 640KB paged through
// the PSRAM, and on a bare Pico, 128KB held directly in SRAM. The two boot
// differently, the BIOS testing and DOS sizing more memory, so each has its
// own invariant, and the header's paging field says which a run had.
//
// Timing discipline: host I/O on this target is expensive enough to swamp what
// is being measured - a blocking UART write costs orders of magnitude more
// than the guest instruction it would be reporting on. So nothing is printed
// from inside the run. Results accumulate in RAM and are printed once the run
// has finished. The one perturbation that cannot be removed is USB servicing,
// which the SDK drives from a timer interrupt; it costs the same in every
// build, so it does not affect the comparisons this harness exists to make.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "core/platform.h"
#include "core/video.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/vreg.h"
#include "hardware/watchdog.h"
#include "pager.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include "psram.h"
#include "yax86_pico.h"
// Generated into the build directory rather than the source tree - see the
// floppy image section of CMakeLists.txt.
#include "generated/dos_floppy_data.h"

// The optimization level the firmware was built at, for the result header.
// Supplied by CMake so that a pasted result says which build produced it.
#ifndef YAX86_PICO_OPT
#define YAX86_PICO_OPT "unknown"
#endif

enum {
  // The SRAM guest memory has: conventional memory itself without the
  // PSRAM, and the frames it is paged through with it.
  kGuestRAMSize = kPicoGuestSRAMSize,

  // Emulated cycles per pass of the run loop, about ten milliseconds of guest
  // time. Short enough that the watchdog is fed often and the screen is
  // polled promptly, long enough that neither costs anything measurable.
  kCyclesPerBatch = 48000,

  // How long the watchdog waits before deciding an overclock has hung. Long
  // enough that a batch always beats it, short enough that a hung board is
  // back within seconds.
  kWatchdogTimeoutMs = 4000,

  // Written to g_overclock_marker across the clock change, and cleared once it
  // is survived. Any value does, as long as uninitialized RAM is unlikely to
  // hold it.
  kOverclockAttemptMarker = 0x0C10C4ED,
  // The same for g_psram_marker, across bringing the PSRAM up.
  kPsramAttemptMarker = 0x95A3B007,

  // The CGA text screen, which the run watches to know how far DOS has got.
  // Character and attribute alternate, so the characters are the even bytes of
  // video memory.
  kTextColumns = 80,
  kTextRows = 25,
  kTextCells = kTextColumns * kTextRows,
  kBytesPerTextCell = 2,
  // The printable range of the character set. Everything else is a box drawing
  // or line drawing glyph as far as this is concerned, and is matched as a
  // space so that the screen reads the way it looks.
  kFirstPrintableCharacter = 0x20,
  kFirstNonPrintableCharacter = 0x7F,

  // Enter, which is what answers DOS's date and time questions.
  kEnterScancode = 0x1C,
  // Emulated cycles between screen polls, at 20 a second. Frequent enough to
  // answer DOS promptly and far too rare to affect the timing.
  kBootPollCycles = Y_PLATFORM_CPU_CYCLES_PER_SECOND / 20,
  // How much of the machine's life to simulate before giving up on the boot.
  // Reaching the prompt takes under six emulated seconds, so anything close to
  // this bound means it went wrong rather than slowly.
  kBootMaxEmulatedSeconds = 30,

  // How long to wait between polls for the USB host to attach.
  kUSBConnectPollMs = 100,
};

// The guest's memory in SRAM and the CGA adapter's frame buffer. Static
// rather than allocated: this target has no heap worth the name, and these two
// arrays are most of what the machine costs in RAM.
static uint8_t g_guest_ram[kGuestRAMSize];
static uint8_t g_vram[Y_VIDEO_CGA_VRAM_SIZE];
// Whether guest RAM is paged through the PSRAM, and how much there is. Set
// once the PSRAM has been brought up, before the first run.
static bool g_paging;
static uint32_t g_guest_ram_size = kGuestRAMSize;
static YPlatformState g_platform;
// What the machine logs through: errors only, since the core's warnings come
// in numbers during a boot and writing them out would be host I/O in the
// measured path. Held for the platform's lifetime, which is why it is not a
// local.
static YLoggerConfig g_machine_log_config;
// What the harness reports through, down to info, outside the run.
static YLoggerConfig g_bench_log_config;
static YLogger g_bench_logger;

static const YLogModule kBenchLogModule = {
    .id = Y_LOG_FIRST_HOST_MODULE_ID,
    .name = "bench",
};

#define BENCH_ERROR(...) \
  Y_LOG(&g_bench_logger, &kBenchLogModule, Y_LOG_LEVEL_ERROR, __VA_ARGS__)
#define BENCH_WARN(...) \
  Y_LOG(&g_bench_logger, &kBenchLogModule, Y_LOG_LEVEL_WARN, __VA_ARGS__)
#define BENCH_INFO(...) \
  Y_LOG(&g_bench_logger, &kBenchLogModule, Y_LOG_LEVEL_INFO, __VA_ARGS__)
#define BENCH_DEBUG(...) \
  Y_LOG(&g_bench_logger, &kBenchLogModule, Y_LOG_LEVEL_DEBUG, __VA_ARGS__)

#ifdef YAX86_PICO_SYS_CLK_KHZ
// Deliberately not zeroed at startup - see where it is read.
static uint32_t __uninitialized_ram(g_overclock_marker);
#endif
// The same for the PSRAM - see StartPsram().
static uint32_t __uninitialized_ram(g_psram_marker);

// What a run produced. Filled in during the run and printed after it, so that
// no host I/O happens while the guest is executing.
typedef struct BenchResult {
  // Retired guest instructions.
  uint64_t instructions;
  // Emulated CPU cycles at 4.77MHz.
  uint64_t cycles;
  // Host time the run took, in microseconds.
  uint64_t elapsed_us;
  // Whether the command prompt was reached.
  bool completed;
  // Why the run stopped, if it stopped before completing.
  YPlatformRunStatus status;
} BenchResult;

static void BenchWriteLogLine(
    Y_UNUSED void* context, Y_UNUSED YLogKind kind,
    Y_UNUSED const YLogModule* module, Y_UNUSED YLogLevel level,
    const char* line, Y_UNUSED size_t length) {
  printf("%s\n", line);
}

// Serve the boot floppy straight out of flash. There is nowhere to put a
// writable copy - 360KB is several times the SRAM left over - so guest writes
// are discarded, the same as the SDL runtime does for its hard disk. Nothing
// booting to a command prompt writes to the disk, and a run starts from the
// same image every time either way.
static uint8_t FloppyReadByte(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, uint32_t offset) {
  return offset < kDOSFloppyDataSize ? kDOSFloppyData[offset] : 0xFF;
}

static void FloppyWriteByte(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, Y_UNUSED uint32_t offset,
    Y_UNUSED uint8_t value) {}

// Bring the machine up as if it had just been switched on, with the boot
// floppy in drive A. Done afresh for each run so that two runs in a session
// give the same answer as one on its own.
static bool ResetMachine(void) {
  for (uint32_t i = 0; i < kGuestRAMSize; ++i) {
    g_guest_ram[i] = 0;
  }
  for (uint32_t i = 0; i < Y_VIDEO_CGA_VRAM_SIZE; ++i) {
    g_vram[i] = 0;
  }
  static const YPlatformState kEmptyPlatform = {0};
  g_platform = kEmptyPlatform;

  YPlatformConfig* config = &g_platform.config;
  config->logger_config = &g_machine_log_config;
  if (g_paging) {
    // Every page starts as zeros, as the SRAM does without paging.
    config->paged_memory = PagerInit(
        g_guest_ram, kGuestRAMSize, Y_PLATFORM_MAX_CONVENTIONAL_MEMORY_SIZE);
  } else {
    config->direct_memory_size = kGuestRAMSize;
    config->direct_memory = g_guest_ram;
  }
  config->vram = g_vram;
  config->video_adapter = Y_VIDEO_ADAPTER_CGA;
  // The DOS idle skip is left off. The run stops at the command prompt,
  // which is where DOS starts idling, so the skip would have nothing to skip -
  // and leaving it off keeps the workload a measurement of how fast guest
  // instructions execute rather than of how many of them are elided.
  if (!YPlatformInit(&g_platform)) {
    return false;
  }
  g_platform.fdc.config.read_image_byte = FloppyReadByte;
  g_platform.fdc.config.write_image_byte = FloppyWriteByte;
  YFDCInsertDisk(&g_platform.fdc, 0, &YFDCFormat360KB);
  return true;
}

// How far a boot has got. DOS asks for the date and then the time before it
// gives a prompt, and neither question answers itself.
typedef enum BootPromptState {
  kBootPromptWaitingForDate = 0,
  kBootPromptWaitingForTime,
  kBootPromptWaitingForCommand,
  kBootPromptReached,
} BootPromptState;

static BootPromptState g_boot_prompt_state = kBootPromptWaitingForDate;

// The character in a text screen cell, with anything unprintable read as a
// space so that the screen matches the way it looks.
static char ScreenCharacter(uint16_t cell) {
  const uint8_t value = g_vram[cell * kBytesPerTextCell];
  return (value >= kFirstPrintableCharacter &&
          value < kFirstNonPrintableCharacter)
             ? (char)value
             : ' ';
}

// Whether the text screen shows the given string. Matched cell by cell rather
// than through a copy of the screen, which would want two kilobytes of a four
// kilobyte stack.
static bool ScreenContains(const char* needle) {
  for (uint16_t start = 0; start < kTextCells; ++start) {
    uint16_t i = 0;
    while (needle[i] != '\0' && start + i < kTextCells &&
           ScreenCharacter((uint16_t)(start + i)) == needle[i]) {
      ++i;
    }
    if (needle[i] == '\0') {
      return true;
    }
  }
  return false;
}

// Answer whichever question DOS is asking, and report whether the command
// prompt has been reached.
static bool AdvanceBootToPrompt(void) {
  switch (g_boot_prompt_state) {
    case kBootPromptWaitingForDate:
      if (ScreenContains("Enter new date")) {
        YKeyboardHandleKeyPress(&g_platform.keyboard, kEnterScancode);
        g_boot_prompt_state = kBootPromptWaitingForTime;
      }
      break;
    case kBootPromptWaitingForTime:
      if (ScreenContains("Enter new time")) {
        YKeyboardHandleKeyPress(&g_platform.keyboard, kEnterScancode);
        g_boot_prompt_state = kBootPromptWaitingForCommand;
      }
      break;
    case kBootPromptWaitingForCommand:
      if (ScreenContains("A>")) {
        g_boot_prompt_state = kBootPromptReached;
      }
      break;
    case kBootPromptReached:
      break;
  }
  return g_boot_prompt_state == kBootPromptReached;
}

// ============================================================================
// Sampling profiler
// ============================================================================
//
// A timer interrupt records where the interrupted code was, and the counts are
// dumped after the run for a host to turn into symbol names. This exists
// because guessing at what a Cortex-M0+ spends its time on has a poor record.
//
// It costs a little over a percent, all of which lands in the reported time,
// so a build with it on is for finding out where the time goes and never for
// saying how much there was.

#ifndef YAX86_PICO_PROFILE
// Whether to sample. Off by default: the histogram costs 32KB of SRAM and the
// interrupt costs more than most of the differences being looked for.
#define YAX86_PICO_PROFILE 0
#endif

#if YAX86_PICO_PROFILE
enum {
  // 16-byte buckets over the low 64KB of SRAM, which is where the hot path
  // ends up once it is placed there. Coarser buckets could not be trusted: at
  // 128 bytes several small functions share a bucket and only the first is
  // named, which put samples on functions this workload cannot reach.
  kProfileSRAMBucketShift = 4,
  kProfileSRAMBase = 0x20000000u,
  kProfileSRAMBuckets = 4096,
  // Flash stays coarse, covering 512KB of the XIP window. Buckets there only
  // have to be fine enough to say which region the time went to.
  kProfileFlashBucketShift = 7,
  kProfileFlashBase = 0x10000000u,
  kProfileFlashBuckets = 4096,
  // 50kHz. Sixteen byte buckets need proportionally more samples to say
  // anything: at 10kHz a six second run left about twenty per bucket, which is
  // noise. The interrupt perturbs every bucket alike.
  kProfileIntervalUs = 20,
  kProfileAlarm = 1,
};

static uint32_t g_profile_sram[kProfileSRAMBuckets];
static uint32_t g_profile_flash[kProfileFlashBuckets];
static uint32_t g_profile_samples;
static uint32_t g_profile_elsewhere;
static bool g_profile_running;

// Called from the interrupt with the exception frame, whose seventh word is
// the PC the interrupt arrived at.
void __not_in_flash_func(ProfileSampleFromFrame)(const uint32_t* frame) {
  timer_hw->intr = 1u << kProfileAlarm;
  timer_hw->alarm[kProfileAlarm] = timer_hw->timerawl + kProfileIntervalUs;
  if (!g_profile_running) {
    return;
  }
  const uint32_t pc = frame[6];
  ++g_profile_samples;
  const uint32_t sram_index =
      (pc - kProfileSRAMBase) >> kProfileSRAMBucketShift;
  if (pc >= kProfileSRAMBase && sram_index < kProfileSRAMBuckets) {
    ++g_profile_sram[sram_index];
    return;
  }
  const uint32_t flash_index =
      (pc - kProfileFlashBase) >> kProfileFlashBucketShift;
  if (pc >= kProfileFlashBase && flash_index < kProfileFlashBuckets) {
    ++g_profile_flash[flash_index];
    return;
  }
  ++g_profile_elsewhere;
}

// The hardware pushes {r0-r3, r12, lr, pc, xpsr} on exception entry, and a C
// function's own prologue would move the stack pointer before it could be
// read. So the stack pointer is captured first, in assembly, and the exception
// return value in lr is preserved across the call.
__attribute__((naked)) static void ProfileIrqHandler(void) {
  __asm volatile(
      "mov r0, sp\n\t"
      "push {lr}\n\t"
      "bl ProfileSampleFromFrame\n\t"
      "pop {r0}\n\t"
      "bx r0\n\t");
}

static void ProfileInit(void) {
  irq_set_exclusive_handler(TIMER_IRQ_0 + kProfileAlarm, ProfileIrqHandler);
  irq_set_enabled(TIMER_IRQ_0 + kProfileAlarm, true);
  hw_set_bits(&timer_hw->inte, 1u << kProfileAlarm);
  timer_hw->alarm[kProfileAlarm] = timer_hw->timerawl + kProfileIntervalUs;
}

// Emitted as address and count so that a host can map them to symbols with
// nm; printing names here would mean carrying a symbol table on the board.
static Y_NOINLINE void ProfileLog(void) {
  if (g_profile_samples == 0) {
    return;
  }
  BENCH_INFO(
      "profile", Y_LOG_UINT("samples", g_profile_samples),
      Y_LOG_UINT("sram_bucket_bytes", 1u << kProfileSRAMBucketShift),
      Y_LOG_UINT("flash_bucket_bytes", 1u << kProfileFlashBucketShift),
      Y_LOG_UINT("elsewhere", g_profile_elsewhere));
  for (uint32_t i = 0; i < kProfileSRAMBuckets; ++i) {
    if (g_profile_sram[i] > 0) {
      BENCH_INFO(
          "profile_bucket",
          Y_LOG_HEX32(
              "address", kProfileSRAMBase + (i << kProfileSRAMBucketShift)),
          Y_LOG_UINT("samples", g_profile_sram[i]));
    }
  }
  for (uint32_t i = 0; i < kProfileFlashBuckets; ++i) {
    if (g_profile_flash[i] > 0) {
      BENCH_INFO(
          "profile_bucket",
          Y_LOG_HEX32(
              "address", kProfileFlashBase + (i << kProfileFlashBucketShift)),
          Y_LOG_UINT("samples", g_profile_flash[i]));
    }
  }
}
#endif  // YAX86_PICO_PROFILE

// ============================================================================
// The run
// ============================================================================

static BenchResult RunBenchmark(void) {
  BenchResult result = {0};
  if (!ResetMachine()) {
    result.status = Y_PLATFORM_HUNG;
    return result;
  }
  g_boot_prompt_state = kBootPromptWaitingForDate;

  // A boot never ends by itself, so it is bounded by emulated time.
  const uint64_t max_cycles =
      (uint64_t)kBootMaxEmulatedSeconds * Y_PLATFORM_CPU_CYCLES_PER_SECOND;
  uint64_t next_poll_cycles = kBootPollCycles;

  // The platform's own cycle counter is 32 bits and wraps after about fifteen
  // minutes of emulated time, so the total is accumulated from per-batch
  // deltas. Unsigned subtraction stays correct across the wrap.
  uint32_t last_ticks = g_platform.ticks;

#if YAX86_PICO_PROFILE
  g_profile_running = true;
#endif
  const uint64_t start_us = time_us_64();
  while (result.cycles < max_cycles) {
    // One register write per batch, so an overclock that survives the clock
    // change but falls over under load still recovers.
    watchdog_update();
    // Driven a batch at a time through YPlatformRun() rather than an
    // instruction at a time, which is how an application would drive it. The
    // core counts retired instructions itself, so there is nothing left for a
    // caller to do per instruction.
    //
    // An invalid opcode is not a stop: the 8088 has no invalid opcode
    // exception, so a real machine would carry on. Anything else means the
    // machine can no longer make progress.
    const uint64_t retired_before = YCPUInstructionsRetired(&g_platform.cpu);
    YPlatformRunStatus status = Y_PLATFORM_RUNNING;
    for (uint32_t remaining = kCyclesPerBatch; remaining > 0;) {
      const uint32_t batch_start = g_platform.ticks;
      status = YPlatformRun(&g_platform, remaining);
      const uint32_t consumed = g_platform.ticks - batch_start;
      remaining -= consumed < remaining ? consumed : remaining;
      if (status != Y_PLATFORM_INVALID) {
        break;
      }
    }
    result.instructions +=
        YCPUInstructionsRetired(&g_platform.cpu) - retired_before;
    result.cycles += (uint32_t)(g_platform.ticks - last_ticks);
    last_ticks = g_platform.ticks;
    if (status != Y_PLATFORM_RUNNING && status != Y_PLATFORM_INVALID) {
      result.status = status;
      break;
    }

    // Screen polls are scheduled from emulated time so that they cost the same
    // however fast the host is. A batch that crosses more than one deadline
    // only wants one poll.
    if (result.cycles < next_poll_cycles) {
      continue;
    }
    do {
      next_poll_cycles += kBootPollCycles;
    } while (next_poll_cycles <= result.cycles);
    if (AdvanceBootToPrompt()) {
      result.completed = true;
      break;
    }
  }
  result.elapsed_us = time_us_64() - start_us;
#if YAX86_PICO_PROFILE
  g_profile_running = false;
#endif
  result.cycles += (uint32_t)(g_platform.ticks - last_ticks);
  return result;
}

// ============================================================================
// Results
// ============================================================================

// The run's figures, as integers. Seconds, emulated MHz and MIPS are worked
// out from them by whatever reads the line - run.js does - so that no
// floating point formatting is linked into the firmware, and nothing is
// truncated on the way.
//
// This and the other logs after the run are kept out of line, as main() would
// otherwise take them in along with the run loop, and their fields would grow
// the code the run loop sits in.
static Y_NOINLINE void LogResult(const BenchResult* r) {
  BENCH_INFO(
      "result", Y_LOG_STRING("workload", "dos-boot"),
      Y_LOG_BOOL("completed", r->completed),
      Y_LOG_UINT("elapsed_us", r->elapsed_us),
      Y_LOG_UINT("instructions", r->instructions),
      Y_LOG_UINT("cycles", r->cycles));
  if (!r->completed) {
    // The rate is still valid, but the run was cut short.
    BENCH_WARN("prompt_not_reached", Y_LOG_INT("status", r->status));
  }
}

// The PSRAM's state, which says why a run did not page, and what paging did
// in the run. The pager's counts follow from what the guest touched, so they
// repeat run to run; fault_us is host time.
static Y_NOINLINE void LogPaging(void) {
  const PsramStats psram = PsramGetStats();
  BENCH_INFO(
      "psram", Y_LOG_STRING("state", PsramStateName(psram.state)),
      Y_LOG_UINT("read_512_us", psram.read_512_us),
      Y_LOG_UINT("write_512_us", psram.write_512_us),
      Y_LOG_UINT("timeouts", psram.timeouts));
  if (!g_paging) {
    return;
  }
  const PagerStats pager = PagerGetStats();
  BENCH_INFO(
      "paging", Y_LOG_UINT("frames", pager.frames),
      Y_LOG_UINT("page_bytes", kPagerPageSize),
      Y_LOG_UINT("page_ins", pager.page_ins),
      Y_LOG_UINT("zero_fills", pager.zero_fills),
      Y_LOG_UINT("write_backs", pager.write_backs),
      Y_LOG_UINT("soft_faults", pager.soft_faults),
      Y_LOG_UINT("errors", pager.errors),
      Y_LOG_UINT("fault_us", pager.fault_us));
}

// ============================================================================
// Stack high water mark
// ============================================================================
//
// The stack lives in its own 4KB scratch bank, a long way from guest memory -
// it would have to overflow by more than 100KB to reach it - so what this
// answers is not whether the two collide but how much headroom the interpreter
// actually leaves. That is the number a real Pico port needs.
//
// Filling the unused stack with a known value and afterwards looking for how
// far it was overwritten costs nothing while the run is going, which is what
// makes it safe to leave switched on.

// From the linker script. The bank runs from __StackBottom up to __StackTop
// and the stack grows down from the top.
extern uint32_t __StackBottom;
extern uint32_t __StackTop;

enum {
  // Written into unused stack words. Any value does, as long as the stack is
  // unlikely to hold it for real - and it has to fit in an int, so that this
  // can be an enum rather than a macro.
  kStackPaintValue = 0x5A5A5A5A,
  // Words below the painting frame's own locals that are left alone, so that
  // painting cannot scribble on the frame doing the painting.
  kStackPaintMarginWords = 64,
};

// Fill the unused part of the stack with kStackPaintValue. Must be called from
// a shallow frame - anything below the caller's frame is painted, so a deep
// caller would leave the interesting part unmeasured.
//
// Never inlined. Inlined into main(), marker lands inside main()'s frame
// rather than below all of it, and the paint overwrites whatever main() keeps
// below marker - a pointer read after the run, say, which then faults.
static Y_NOINLINE void PaintStack(void) {
  // Taking the address of a local puts it in this frame, which is as good a
  // marker for where the stack currently reaches as reading SP, and is
  // ordinary C rather than inline assembly.
  uint32_t marker = 0;
  // As an integer rather than a pointer: subtracting from &marker would form a
  // pointer outside the object it came from, which is undefined however
  // sensibly it behaves, and which -O3 warns about.
  const uintptr_t limit =
      (uintptr_t)&marker - kStackPaintMarginWords * sizeof(uint32_t);
  for (uint32_t* word = &__StackBottom; (uintptr_t)word < limit; ++word) {
    *word = kStackPaintValue;
  }
}

// Reports the high water mark since PaintStack(), and whether the paint
// survived at all - if it did not, the stack reached the bottom of its bank
// and the figure is a floor rather than a measurement. Interrupt frames count,
// which is what makes this the real peak rather than the emulator's share.
static Y_NOINLINE void LogStackUsage(void) {
  const uint32_t total =
      (uint32_t)((&__StackTop - &__StackBottom) * sizeof(uint32_t));
  const uint32_t* word = &__StackBottom;
  while (word < &__StackTop && *word == kStackPaintValue) {
    ++word;
  }
  const uint32_t peak = (uint32_t)((&__StackTop - word) * sizeof(uint32_t));
  // If the paint did not survive at all, the stack reached the bottom of its
  // bank and the peak is a floor rather than a measurement.
  BENCH_INFO(
      "stack_peak", Y_LOG_UINT("bytes", peak), Y_LOG_UINT("total", total),
      Y_LOG_BOOL("overflowed", peak >= total));
}

// ============================================================================
// Startup
// ============================================================================

static Y_NOINLINE void LogHeader(void) {
  BENCH_INFO(
      "header", Y_LOG_STRING("emulator", "yax86"),
      Y_LOG_STRING("target", "rp2040"), Y_LOG_STRING("opt", YAX86_PICO_OPT),
      Y_LOG_UINT("guest_ram_kb", g_guest_ram_size / 1024),
      Y_LOG_BOOL("paging", g_paging),
      Y_LOG_UINT("sys_clk_khz", clock_get_hz(clk_sys) / 1000),
      Y_LOG_BOOL("profile", YAX86_PICO_PROFILE),
      Y_LOG_UINT("state_bytes", sizeof(YPlatformState)));
}

#ifdef YAX86_PICO_SYS_CLK_KHZ
// Raise the system clock, after USB is up rather than before.
//
// A clock the chip cannot take hangs it, and a hang before USB enumerates
// leaves no way in except the BOOTSEL button - which is no good on a board
// being flashed remotely. So the request happens with the host already
// attached and watching, and a watchdog turns a hang into a reboot. The reboot
// lands here again, where the marker says not to try the same thing twice.
//
// watchdog_caused_reboot() alone is not evidence of a hang: picotool reboots
// the chip through the watchdog too, so it reads true after every flash. The
// marker is what distinguishes them. It lives in RAM that startup does not
// clear, so it survives a reset but not a power cycle, and it is only ever
// left set across the window where a hang is possible.
static void Overclock(void) {
  if (g_overclock_marker == kOverclockAttemptMarker) {
    g_overclock_marker = 0;
    BENCH_WARN("overclock_skipped_after_hang");
    return;
  }
  BENCH_INFO("overclock_requested", Y_LOG_UINT("khz", YAX86_PICO_SYS_CLK_KHZ));
  // Flushed before the clock moves, so the last thing seen is the attempt.
  sleep_ms(50);
  g_overclock_marker = kOverclockAttemptMarker;
  watchdog_enable(kWatchdogTimeoutMs, true);
  if (YAX86_PICO_SYS_CLK_KHZ > 250000) {
    vreg_set_voltage(VREG_VOLTAGE_1_30);
  } else if (YAX86_PICO_SYS_CLK_KHZ > 125000) {
    vreg_set_voltage(VREG_VOLTAGE_1_20);
  }
  busy_wait_us(10000);
  if (!set_sys_clock_khz(YAX86_PICO_SYS_CLK_KHZ, false)) {
    BENCH_ERROR("overclock_pll_cannot_produce_clock");
  }
  // Survived the change. A hang under load is still possible, so the watchdog
  // stays armed until the run is over, but the marker has done its job and
  // must not make the next boot skip a clock that works.
  g_overclock_marker = 0;
}
#endif

// Bring the PSRAM up, and page guest RAM through it if it works. After the
// clock change, since the PSRAM's serial clock is divided from it.
//
// Done under the watchdog with a marker, as Overclock() is, so that a chip
// that hangs the board costs one reboot, after which the bench runs without
// it. The watchdog is left as it was found: armed if Overclock() armed it.
static void StartPsram(void) {
  if (g_psram_marker == kPsramAttemptMarker) {
    g_psram_marker = 0;
    PsramInit(true);
    BENCH_WARN("psram_skipped_after_hang");
    return;
  }
  const bool watchdog_was_armed = watchdog_hw->ctrl & WATCHDOG_CTRL_ENABLE_BITS;
  g_psram_marker = kPsramAttemptMarker;
  watchdog_enable(kWatchdogTimeoutMs, true);
  g_paging = PsramInit(false);
  g_psram_marker = 0;
  if (!watchdog_was_armed) {
    watchdog_disable();
  }
  if (g_paging) {
    g_guest_ram_size = Y_PLATFORM_MAX_CONVENTIONAL_MEMORY_SIZE;
  }
}

int main(void) {
  stdio_init_all();
  // Wait for the host to attach before printing anything. USB CDC drops
  // whatever is written before the port is opened, and the header says which
  // build produced the numbers that follow.
  while (!stdio_usb_connected()) {
    sleep_ms(kUSBConnectPollMs);
  }

  g_machine_log_config.write_line = BenchWriteLogLine;
  g_machine_log_config.enabled_modules = 0xFFFFFFFF;
  g_machine_log_config.min_level = Y_LOG_LEVEL_ERROR;
  g_bench_log_config.write_line = BenchWriteLogLine;
  g_bench_log_config.enabled_modules = 0xFFFFFFFF;
  g_bench_log_config.min_level = Y_LOG_LEVEL_INFO;
  YLoggerInit(&g_bench_logger, &g_bench_log_config);

#if YAX86_PICO_PROFILE
  // Armed before the clock changes so that the alarm is programmed once, from
  // a timer that runs at 1MHz regardless of clk_sys.
  ProfileInit();
#endif
#ifdef YAX86_PICO_SYS_CLK_KHZ
  Overclock();
#endif
  StartPsram();

  LogHeader();
  // Painted from main's frame, before anything deep runs, so that the whole of
  // what the run uses is measured.
  PaintStack();

  const BenchResult result = RunBenchmark();

  watchdog_disable();
  LogResult(&result);
  LogPaging();
#if YAX86_PICO_PROFILE
  ProfileLog();
#endif
  LogStackUsage();
  BENCH_INFO("done");
  // Nothing left to do, and returning from main on this target resets the
  // chip, which would wipe the results off the terminal.
  for (;;) {
    sleep_ms(kUSBConnectPollMs);
  }
  return 0;
}
