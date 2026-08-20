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
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
// Generated into the build directory rather than the source tree - see the
// floppy image section of CMakeLists.txt.
#include "generated/dos_floppy_data.h"

// Guest RAM in kilobytes, chosen at build time by the YAX86_PICO_GUEST_RAM_KB
// CMake option. Period-accurate XT sizes only: an IBM PC/XT shipped with 64K
// to 640K, and what fits alongside the SDK in 264KB of SRAM is the low end.
#ifndef YAX86_PICO_GUEST_RAM_KB
#define YAX86_PICO_GUEST_RAM_KB 128
#endif

// The optimization level the firmware was built at, for the result header.
// Supplied by CMake so that a pasted result says which build produced it.
#ifndef YAX86_PICO_OPT
#define YAX86_PICO_OPT "unknown"
#endif

enum {
  // Guest conventional memory, in bytes.
  kGuestRAMSize = YAX86_PICO_GUEST_RAM_KB * 1024,

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
  kBootPollCycles = kCPUCyclesPerSecond / 20,
  // How much of the machine's life to simulate before giving up on the boot.
  // Reaching the prompt takes under six emulated seconds, so anything close to
  // this bound means it went wrong rather than slowly.
  kBootMaxEmulatedSeconds = 30,

  // How long to wait between polls for the USB host to attach.
  kUSBConnectPollMs = 100,

  // Fixed-point scales for the result lines, which are formatted from integers
  // so that the firmware needs no floating point printf, and the number of
  // fraction digits each one prints.
  kHundredths = 100,
  kHundredthDigits = 2,
  kMicrosecondsPerSecond = 1000000,
  kMicrosecondDigits = 6,
};

// The guest's conventional memory and the CGA adapter's frame buffer. Static
// rather than allocated: this target has no heap worth the name, and these two
// arrays are most of what the machine costs in RAM.
static uint8_t g_guest_ram[kGuestRAMSize];
static uint8_t g_vram[kCGAVRAMSize];
static PlatformState g_platform;
// Held for the platform's lifetime, which is why it is not a local.
static PlatformConfig g_platform_config;
static LoggerConfig g_logger_config;

#ifdef YAX86_PICO_SYS_CLK_KHZ
// Deliberately not zeroed at startup - see where it is read.
static uint32_t __uninitialized_ram(g_overclock_marker);
#endif

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
  PlatformRunStatus status;
} BenchResult;

// Only errors are logged, and only outside a run: anything chattier would put
// host I/O in the measured path.
static void BenchWriteLogLine(
    YAX86_UNUSED void* context, const LogModule* module,
    YAX86_UNUSED LogLevel level, YAX86_UNUSED uint64_t tick,
    const char* message, YAX86_UNUSED size_t length) {
  printf("[%s] %s\n", module->name, message);
}

// Serve the boot floppy straight out of flash. There is nowhere to put a
// writable copy - 360KB is several times the SRAM left over - so guest writes
// are discarded, the same as the SDL runtime does for its hard disk. Nothing
// booting to a command prompt writes to the disk, and a run starts from the
// same image every time either way.
static uint8_t FloppyReadByte(
    YAX86_UNUSED void* context, YAX86_UNUSED uint8_t drive, uint32_t offset) {
  return offset < kDOSFloppyDataSize ? kDOSFloppyData[offset] : 0xFF;
}

static void FloppyWriteByte(
    YAX86_UNUSED void* context, YAX86_UNUSED uint8_t drive,
    YAX86_UNUSED uint32_t offset, YAX86_UNUSED uint8_t value) {}

// Bring the machine up as if it had just been switched on, with the boot
// floppy in drive A. Done afresh for each run so that two runs in a session
// give the same answer as one on its own.
static bool ResetMachine(void) {
  for (uint32_t i = 0; i < kGuestRAMSize; ++i) {
    g_guest_ram[i] = 0;
  }
  for (uint32_t i = 0; i < kCGAVRAMSize; ++i) {
    g_vram[i] = 0;
  }
  static const PlatformState kEmptyPlatform = {0};
  g_platform = kEmptyPlatform;

  static const PlatformConfig kEmptyConfig = {0};
  g_platform_config = kEmptyConfig;
  g_platform_config.logger_config = &g_logger_config;
  g_platform_config.physical_memory_size = kGuestRAMSize;
  g_platform_config.physical_memory = g_guest_ram;
  g_platform_config.vram = g_vram;
  g_platform_config.video_adapter = kVideoAdapterCGA;
  // enable_dos_idle_skip is left off. The run stops at the command prompt,
  // which is where DOS starts idling, so the skip would have nothing to skip -
  // and leaving it off keeps the workload a measurement of how fast guest
  // instructions execute rather than of how many of them are elided.
  if (!PlatformInit(&g_platform, &g_platform_config)) {
    return false;
  }
  g_platform.fdc_config.read_image_byte = FloppyReadByte;
  g_platform.fdc_config.write_image_byte = FloppyWriteByte;
  FDCInsertDisk(&g_platform.fdc, 0, &kFDCFormat360KB);
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
        KeyboardHandleKeyPress(&g_platform.keyboard, kEnterScancode);
        g_boot_prompt_state = kBootPromptWaitingForTime;
      }
      break;
    case kBootPromptWaitingForTime:
      if (ScreenContains("Enter new time")) {
        KeyboardHandleKeyPress(&g_platform.keyboard, kEnterScancode);
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
static void ProfilePrint(void) {
  if (g_profile_samples == 0) {
    return;
  }
  printf("profile_samples %u\n", (unsigned)g_profile_samples);
  printf("profile_bucket %u\n", (unsigned)(1u << kProfileSRAMBucketShift));
  printf("profile_elsewhere %u\n", (unsigned)g_profile_elsewhere);
  for (uint32_t i = 0; i < kProfileSRAMBuckets; ++i) {
    if (g_profile_sram[i] > 0) {
      printf(
          "p %08x %u\n",
          (unsigned)(kProfileSRAMBase + (i << kProfileSRAMBucketShift)),
          (unsigned)g_profile_sram[i]);
    }
  }
  for (uint32_t i = 0; i < kProfileFlashBuckets; ++i) {
    if (g_profile_flash[i] > 0) {
      printf(
          "p %08x %u\n",
          (unsigned)(kProfileFlashBase + (i << kProfileFlashBucketShift)),
          (unsigned)g_profile_flash[i]);
    }
  }
  printf("profile_end\n");
}
#endif  // YAX86_PICO_PROFILE

// ============================================================================
// The run
// ============================================================================

static BenchResult RunBenchmark(void) {
  BenchResult result = {0};
  if (!ResetMachine()) {
    result.status = kPlatformHung;
    return result;
  }
  g_boot_prompt_state = kBootPromptWaitingForDate;

  // A boot never ends by itself, so it is bounded by emulated time.
  const uint64_t max_cycles =
      (uint64_t)kBootMaxEmulatedSeconds * kCPUCyclesPerSecond;
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
    // Driven a batch at a time through PlatformRun() rather than an
    // instruction at a time, which is how an application would drive it. The
    // core counts retired instructions itself, so there is nothing left for a
    // caller to do per instruction.
    //
    // An invalid opcode is not a stop: the 8088 has no invalid opcode
    // exception, so a real machine would carry on. Anything else means the
    // machine can no longer make progress.
    const uint64_t retired_before = g_platform.cpu.instructions_retired;
    PlatformRunStatus status = kPlatformRunning;
    for (uint32_t remaining = kCyclesPerBatch; remaining > 0;) {
      const uint32_t batch_start = g_platform.ticks;
      status = PlatformRun(&g_platform, remaining);
      const uint32_t consumed = g_platform.ticks - batch_start;
      remaining -= consumed < remaining ? consumed : remaining;
      if (status != kPlatformInvalid) {
        break;
      }
    }
    result.instructions += g_platform.cpu.instructions_retired - retired_before;
    result.cycles += (uint32_t)(g_platform.ticks - last_ticks);
    last_ticks = g_platform.ticks;
    if (status != kPlatformRunning && status != kPlatformInvalid) {
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

// Print a value carried as an integer scaled by a power of ten, so that no
// floating point formatting is linked into the firmware.
static void PrintScaled(
    const char* label, uint64_t scaled, uint64_t scale, uint8_t digits) {
  printf(
      "%-13s %llu.%0*llu\n", label, (unsigned long long)(scaled / scale),
      (int)digits, (unsigned long long)(scaled % scale));
}

// A run of zero microseconds cannot have happened, but dividing by it would
// still be worse than reporting one.
static uint64_t ElapsedMicroseconds(const BenchResult* r) {
  return r->elapsed_us > 0 ? r->elapsed_us : 1;
}

static void PrintResult(const BenchResult* r) {
  const uint64_t us = ElapsedMicroseconds(r);
  printf("%-13s %s\n", "workload", "dos-boot");
  PrintScaled("seconds", us, kMicrosecondsPerSecond, kMicrosecondDigits);
  printf("%-13s %llu\n", "instructions", (unsigned long long)r->instructions);
  printf("%-13s %llu\n", "emulated_cyc", (unsigned long long)r->cycles);
  // Instructions per microsecond is millions of instructions per second, so
  // MIPS needs no unit conversion - only a scale for the decimal places.
  PrintScaled(
      "mips", r->instructions * kHundredths / us, kHundredths,
      kHundredthDigits);
  // Emulated cycles per microsecond is emulated megahertz: the clock rate the
  // guest would have had to run at for this much emulated time to have taken
  // this much host time. Against a real 8088's 4.77.
  PrintScaled(
      "emulated_mhz", r->cycles * kHundredths / us, kHundredths,
      kHundredthDigits);
  if (!r->completed) {
    printf(
        "warning: the DOS command prompt was never reached - the reported "
        "rate is still valid but the run was cut short (status %d)\n",
        (int)r->status);
  }
  printf("\n");
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
static void PaintStack(void) {
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
static void PrintStackUsage(void) {
  const uint32_t total =
      (uint32_t)((&__StackTop - &__StackBottom) * sizeof(uint32_t));
  const uint32_t* word = &__StackBottom;
  while (word < &__StackTop && *word == kStackPaintValue) {
    ++word;
  }
  const uint32_t peak = (uint32_t)((&__StackTop - word) * sizeof(uint32_t));
  printf(
      "%-13s %u of %u bytes%s\n", "stack_peak", (unsigned)peak, (unsigned)total,
      peak >= total ? " (OVERFLOWED - figure is a floor)" : "");
  printf("\n");
}

// ============================================================================
// Startup
// ============================================================================

static void PrintHeader(void) {
  printf("\n");
  printf("%-13s %s\n", "emulator", "yax86");
  printf("%-13s %s\n", "target", "rp2040");
  printf("%-13s %s\n", "opt", YAX86_PICO_OPT);
  printf("%-13s %u\n", "guest_ram_kb", (unsigned)YAX86_PICO_GUEST_RAM_KB);
  printf("%-13s %u\n", "sys_clk_khz", (unsigned)(clock_get_hz(clk_sys) / 1000));
  printf("%-13s %u\n", "profile", (unsigned)YAX86_PICO_PROFILE);
  printf("%-13s %u\n", "state_bytes", (unsigned)sizeof(PlatformState));
  printf("\n");
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
    printf("%-13s %s\n", "overclock", "skipped - last attempt hung");
    return;
  }
  printf(
      "%-13s requesting %u kHz\n", "overclock",
      (unsigned)YAX86_PICO_SYS_CLK_KHZ);
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
    printf("%-13s %s\n", "overclock", "PLL cannot produce that clock");
  }
  // Survived the change. A hang under load is still possible, so the watchdog
  // stays armed until the run is over, but the marker has done its job and
  // must not make the next boot skip a clock that works.
  g_overclock_marker = 0;
}
#endif

int main(void) {
  stdio_init_all();
  // Wait for the host to attach before printing anything. USB CDC drops
  // whatever is written before the port is opened, and the header says which
  // build produced the numbers that follow.
  while (!stdio_usb_connected()) {
    sleep_ms(kUSBConnectPollMs);
  }

  g_logger_config.write_line = BenchWriteLogLine;
  g_logger_config.enabled_modules = 0xFFFFFFFF;
  g_logger_config.min_level = kLogLevelError;

#if YAX86_PICO_PROFILE
  // Armed before the clock changes so that the alarm is programmed once, from
  // a timer that runs at 1MHz regardless of clk_sys.
  ProfileInit();
#endif
#ifdef YAX86_PICO_SYS_CLK_KHZ
  Overclock();
#endif

  PrintHeader();
  // Painted from main's frame, before anything deep runs, so that the whole of
  // what the run uses is measured.
  PaintStack();

  const BenchResult result = RunBenchmark();

  watchdog_disable();
  PrintResult(&result);
#if YAX86_PICO_PROFILE
  ProfilePrint();
#endif
  PrintStackUsage();
  printf("done\n");
  // Nothing left to do, and returning from main on this target resets the
  // chip, which would wipe the results off the terminal.
  for (;;) {
    sleep_ms(kUSBConnectPollMs);
  }
  return 0;
}
