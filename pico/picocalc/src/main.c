// yax86 on the ClockworkPi PicoCalc.
//
// The runtime raises the system clock, brings up the LCD and the keyboard
// controller, and boots MS-DOS 3.30 off the floppy image in flash, drawing the
// guest's text screen on the LCD as it goes. The machine is paced to real
// time and keeps running past the prompt. The keyboard is not read yet, so
// DOS's date and time questions are answered by watching the screen, the way
// pico/bench does it.
//
// The guest's screen takes the top 250 rows of the panel - 80 by 25 cells of
// the 4x10 font - and the runtime's status lines the rest. Everything on them
// is also reported over USB serial for pico/bench/run.js to capture. A report
// is printed whenever a host attaches, since USB CDC drops whatever is written
// before then.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "core/platform.h"
#include "core/video.h"
#include "font_4x10.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/watchdog.h"
#include "lcd.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include "southbridge.h"
#include "text.h"
// Generated into the build directory - see the floppy image section of
// CMakeLists.txt.
#include "generated/dos_floppy_data.h"

// Whether the guest's screen is drawn. Building with it off gives an image
// identical but for that, which is what measures what drawing costs.
#ifndef YAX86_PICOCALC_DRAW_SCREEN
#define YAX86_PICOCALC_DRAW_SCREEN 1
#endif

// The system clock in kHz, or 0 for the stock 125MHz. Set by CMakeLists.txt.
#ifndef YAX86_PICOCALC_SYS_CLK_KHZ
#define YAX86_PICOCALC_SYS_CLK_KHZ 0
#endif

enum {
  // Guest conventional memory. The same 128K pico/bench measures with.
  kGuestRAMSize = 128 * 1024,

  // Emulated cycles per pass of the run loop, about ten milliseconds of guest
  // time - the same batch pico/bench runs.
  kCyclesPerBatch = 48000,

  // The CGA text screen, which the boot is watched through. Character and
  // attribute alternate, so the characters are the even bytes.
  kScreenColumns = 80,
  kScreenRows = 25,
  kScreenCells = kScreenColumns * kScreenRows,
  kBytesPerScreenCell = 2,
  kFirstPrintableCharacter = 0x20,
  kFirstNonPrintableCharacter = 0x7F,

  // Enter, which answers DOS's date and time questions.
  kEnterScancode = 0x1C,
  // Emulated cycles between looks at the screen, 20 a second.
  kBootPollCycles = Y_PLATFORM_CPU_CYCLES_PER_SECOND / 20,
  // How much emulated time a boot may take before it is taken to have gone
  // wrong. Reaching the prompt takes under six emulated seconds.
  kBootMaxEmulatedSeconds = 30,

  // How long the screen stays on once the boot is over, before it is blanked.
  // Nothing the guest does changes it after the boot but the cursor, and a
  // static image left showing for hours can leave its mark on the panel.
  kScreenOnAfterBootMs = 30 * 1000,
  // How often the status lines are brought up to date once the boot is over.
  kStatusUpdateMs = 1000,
  // How often the loop looks for a newly attached host once the machine has
  // stopped.
  kStoppedPollMs = 100,

  // How far behind real time the machine may fall and still make it up by
  // running flat out. Past this the lag is dropped instead, so that a long
  // stall is not followed by a burst of emulation at full speed.
  kMaxLagMs = 100,

  // How long the watchdog waits, across the clock change, before deciding the
  // chip has hung and rebooting it.
  kWatchdogTimeoutMs = 4000,
  // Written to g_overclock_marker across the clock change, and cleared once it
  // is survived. Any value does, as long as uninitialized RAM is unlikely to
  // hold it.
  kOverclockAttemptMarker = 0x0C10C4ED,
  // How long the core voltage is given to settle before the clock is raised.
  kVoltageSettleUs = 10 * 1000,

  // The BIOS's count of timer ticks since midnight, in the BIOS data area,
  // which is the time of day DOS reports. A tick is 65,536 counts of the
  // PIT's 1,193,182Hz input clock, about 18.2 a second.
  kBIOSTickCountAddress = 0x46C,
  kPITInputHz = 1193182,
  kPITCountsPerBIOSTick = 65536,

  // Emulated cycles between renders: once a frame of the CGA's 60Hz display.
  // Drawing more often shows nothing new, since a frame is the most often the
  // guest's screen can change as it would be seen.
  kCyclesPerRender = Y_PLATFORM_CPU_CYCLES_PER_SECOND / 60,

  kMicrosecondsPerMillisecond = 1000,
  kMillisecondsPerSecond = 1000,
  kMicrosecondsPerSecond = 1000 * 1000,
  kMaxLagUs = kMaxLagMs * kMicrosecondsPerMillisecond,
  kHzPerKHz = 1000,

  // The guest's screen: 80 by 25 cells of the 4x10 font, at the top of the
  // panel.
  kGuestScreenTop = 0,
  kGuestScreenWidth = kScreenColumns * kFont4x10Width,
  kGuestScreenHeight = kScreenRows * kFont4x10Height,

  // The status lines below it, in 4x10 character cells. Row 25 is left blank
  // to separate them from the guest's screen.
  kBannerRow = kScreenRows + 1,
  kStatusFirstRow = kBannerRow + 1,
  kStatusLabelColumn = 1,
  kStatusValueColumn = 10,
};

// The status lines, in the order they appear.
typedef enum StatusLine {
  kStatusLineSystem = 0,
  kStatusLineBoot,
  kStatusLineSpeed,
  kStatusLineClock,
  kStatusLineDisplay,
} StatusLine;

// What became of the request to raise the system clock.
typedef enum OverclockResult {
  kOverclockNotRequested = 0,
  kOverclockApplied,
  kOverclockPLLCannot,
  kOverclockSkippedAfterHang,
} OverclockResult;

static const char* const kOverclockNames[] = {
    [kOverclockNotRequested] = "not requested",
    [kOverclockApplied] = "applied",
    [kOverclockPLLCannot] = "PLL cannot produce that clock",
    [kOverclockSkippedAfterHang] = "skipped - the last attempt hung",
};

// How far the boot has got. DOS asks for the date and then the time before it
// gives a prompt.
typedef enum BootStage {
  kBootStagePowerOn = 0,
  kBootStageAnsweringDate,
  kBootStageAnsweringTime,
  kBootStagePrompt,
  kBootStageFailed,
} BootStage;

static const char* const kBootStageNames[] = {
    [kBootStagePowerOn] = "running POST and loading DOS",
    [kBootStageAnsweringDate] = "answered the date prompt",
    [kBootStageAnsweringTime] = "answered the time prompt",
    [kBootStagePrompt] = "reached the A> prompt",
    [kBootStageFailed] = "did not reach the prompt",
};

// What the runtime found and how far it has got, for the LCD and for the
// report a host is sent.
typedef struct RuntimeStatus {
  OverclockResult overclock;
  uint32_t lcd_spi_hz;
  bool southbridge_answered;
  uint8_t southbridge_version;
  BootStage boot_stage;
  // Emulated and host time at the point boot_stage was last changed.
  uint64_t boot_cycles;
  uint64_t boot_elapsed_us;
  // Host time spent running the machine up to then, which is the elapsed time
  // less the time spent waiting for real time to catch up. It is what the
  // boot would have taken unpaced.
  uint64_t boot_busy_us;
  uint64_t instructions;
  // What drawing the guest's screen has cost: renders that drew something,
  // the pixels they drew, and the host time spent in YVideoRender().
  uint32_t frames_drawn;
  uint64_t pixels_drawn;
  uint64_t render_us;
} RuntimeStatus;

static uint8_t g_guest_ram[kGuestRAMSize];
static uint8_t g_vram[Y_VIDEO_CGA_VRAM_SIZE];
static YPlatformState g_platform;
// Held for the platform's lifetime, which is why it is not a local.
static YLoggerConfig g_logger_config;
static RuntimeStatus g_status;
static bool g_host_attached;
// Whether the region being rendered lies on the guest's part of the panel.
// Graphics modes render 640 pixels across, which is not drawn yet.
static bool g_region_on_panel;
static uint16_t g_pixel_batch[Y_VIDEO_PIXEL_BATCH_SIZE];

// How far the machine has got in emulated time, and what pacing it against
// real time has cost.
typedef struct RunClock {
  // Emulated cycles since power on. The platform's own counter is 32 bits, so
  // this is accumulated from per-batch deltas, which unsigned subtraction
  // keeps right across a wrap.
  uint64_t cycles;
  uint32_t last_ticks;
  // Host time at power on.
  uint64_t start_us;
  // A point at which emulated and real time agreed. Emulated time from there
  // on is due to take as long in real time.
  uint64_t anchor_us;
  uint64_t anchor_cycles;
  // Host time spent waiting for real time to catch up with the machine.
  uint64_t waited_us;
  // Real time the machine fell behind by and did not make up.
  uint64_t dropped_us;
} RunClock;

static RunClock g_clock;

#if YAX86_PICOCALC_SYS_CLK_KHZ
// Deliberately not zeroed at startup - see Overclock().
static uint32_t __uninitialized_ram(g_overclock_marker);
#endif

// ============================================================================
// Reporting
// ============================================================================

static void WriteLogLine(
    Y_UNUSED void* context, const YLogModule* module, Y_UNUSED YLogLevel level,
    Y_UNUSED uint64_t tick, const char* message, Y_UNUSED size_t length) {
  printf("[%s] %s\n", module->name, message);
}

static uint16_t CGAColor(uint8_t index) {
  const YVideoRGB rgb = YVideoDefaultConfig.cga_palette[index];
  return LcdRGB565(rgb.r, rgb.g, rgb.b);
}

static bool BootIsOver(void) {
  return g_status.boot_stage == kBootStagePrompt ||
         g_status.boot_stage == kBootStageFailed;
}

// Host time since power on, and how much of it went to running the machine
// rather than waiting for real time to catch up with it.
static uint64_t ElapsedUs(void) { return time_us_64() - g_clock.start_us; }

static uint64_t BusyUs(void) { return ElapsedUs() - g_clock.waited_us; }

// The time of day the guest's BIOS keeps, which is what DOS reports, in
// milliseconds. It counts from POST, since the date and time questions are
// answered with Enter.
static uint64_t GuestClockMs(void) {
  const uint8_t* const count = &g_guest_ram[kBIOSTickCountAddress];
  const uint64_t ticks = (uint64_t)count[0] | (uint64_t)count[1] << 8 |
                         (uint64_t)count[2] << 16 | (uint64_t)count[3] << 24;
  return ticks * kPITCountsPerBIOSTick * kMillisecondsPerSecond / kPITInputHz;
}

// How fast the boot ran, as tenths of a percent of a real PC/XT's speed.
// Taken from the host time spent running it rather than the time it took,
// which pacing holds to real time.
static uint32_t BootSpeedPermille(void) {
  if (g_status.boot_busy_us == 0) {
    return 0;
  }
  return (uint32_t)(g_status.boot_cycles * 1000 * kMicrosecondsPerSecond /
                    Y_PLATFORM_CPU_CYCLES_PER_SECOND / g_status.boot_busy_us);
}

static void FormatSouthbridgeVersion(char* buffer, size_t size) {
  if (!g_status.southbridge_answered) {
    snprintf(buffer, size, "not answering");
  } else if (g_status.southbridge_version == 0) {
    snprintf(buffer, size, "older than v1.6");
  } else {
    snprintf(
        buffer, size, "v%X.%X", g_status.southbridge_version >> 4,
        g_status.southbridge_version & 0xF);
  }
}

static void FormatBootLine(char* buffer, size_t size) {
  const uint64_t emulated_ms = g_status.boot_cycles * kMillisecondsPerSecond /
                               Y_PLATFORM_CPU_CYCLES_PER_SECOND;
  const uint64_t host_ms =
      g_status.boot_elapsed_us / kMicrosecondsPerMillisecond;
  if (g_status.boot_stage == kBootStagePowerOn) {
    snprintf(buffer, size, "%s", kBootStageNames[g_status.boot_stage]);
    return;
  }
  snprintf(
      buffer, size, "%s at %llu.%03llus emulated, %llu.%03llus real",
      kBootStageNames[g_status.boot_stage],
      (unsigned long long)(emulated_ms / kMillisecondsPerSecond),
      (unsigned long long)(emulated_ms % kMillisecondsPerSecond),
      (unsigned long long)(host_ms / kMillisecondsPerSecond),
      (unsigned long long)(host_ms % kMillisecondsPerSecond));
}

static void FormatSpeedLine(char* buffer, size_t size) {
  const uint32_t permille = BootSpeedPermille();
  if (permille == 0) {
    snprintf(buffer, size, "measured once DOS asks for the date");
    return;
  }
  snprintf(
      buffer, size, "%lu.%lu%% of a real PC/XT, so %s",
      (unsigned long)(permille / 10), (unsigned long)(permille % 10),
      permille >= 1000 ? "paced to real time" : "slower than real time");
}

// The guest's clock against the real one, and how busy the host has been
// since the last time the line was drawn.
static void FormatClockLine(char* buffer, size_t size) {
  static uint64_t window_start_us;
  static uint64_t window_waited_us;
  const uint64_t now_us = ElapsedUs();
  const uint64_t window_us = now_us - window_start_us;
  const uint64_t waited_us = g_clock.waited_us - window_waited_us;
  const uint32_t busy_percent =
      window_us > 0 && waited_us <= window_us
          ? (uint32_t)((window_us - waited_us) * 100 / window_us)
          : 0;
  window_start_us = now_us;
  window_waited_us = g_clock.waited_us;

  const uint64_t guest_ds = GuestClockMs() / 100;
  const uint64_t real_ds = now_us / (100 * kMicrosecondsPerMillisecond);
  snprintf(
      buffer, size,
      "guest %llu.%llus, real %llu.%llus since power on, %lu%% busy",
      (unsigned long long)(guest_ds / 10), (unsigned long long)(guest_ds % 10),
      (unsigned long long)(real_ds / 10), (unsigned long long)(real_ds % 10),
      (unsigned long)busy_percent);
}

static void FormatDisplayLine(char* buffer, size_t size) {
  if (!YAX86_PICOCALC_DRAW_SCREEN) {
    snprintf(buffer, size, "not drawn in this build");
    return;
  }
  const uint64_t render_ms = g_status.render_us / kMicrosecondsPerMillisecond;
  const uint64_t busy_us = BusyUs() > 0 ? BusyUs() : 1;
  snprintf(
      buffer, size, "%lu frames, %llu.%03llus drawing, %llu%% of the work",
      (unsigned long)g_status.frames_drawn,
      (unsigned long long)(render_ms / kMillisecondsPerSecond),
      (unsigned long long)(render_ms % kMillisecondsPerSecond),
      (unsigned long long)(g_status.render_us * 100 / busy_us));
}

// Draw one status line, padded out to the right edge so that a shorter value
// covers a longer one.
static void DrawStatusLine(
    StatusLine line, const char* label, const char* value) {
  const uint8_t row = (uint8_t)(kStatusFirstRow + line);
  char padded[kTextColumns + 1];
  snprintf(
      padded, sizeof(padded), "%-*s", kTextColumns - kStatusValueColumn, value);
  TextDrawString(kStatusLabelColumn, row, label, CGAColor(7), CGAColor(0));
  TextDrawString(kStatusValueColumn, row, padded, CGAColor(15), CGAColor(0));
}

// Everything on the status screen that changes as the machine runs.
static void DrawRunStatus(void) {
  char value[kTextColumns + 1];
  FormatBootLine(value, sizeof(value));
  DrawStatusLine(kStatusLineBoot, "Boot", value);
  FormatSpeedLine(value, sizeof(value));
  DrawStatusLine(kStatusLineSpeed, "Speed", value);
  FormatClockLine(value, sizeof(value));
  DrawStatusLine(kStatusLineClock, "Clock", value);
  FormatDisplayLine(value, sizeof(value));
  DrawStatusLine(kStatusLineDisplay, "Display", value);
}

static void DrawStatusScreen(void) {
  TextDrawString(
      kStatusLabelColumn, kBannerRow,
      "yax86 - an IBM PC/XT emulator, on the ClockworkPi PicoCalc",
      CGAColor(14), CGAColor(0));

  char version[16];
  FormatSouthbridgeVersion(version, sizeof(version));
  char value[kTextColumns + 1];
  snprintf(
      value, sizeof(value), "%lu MHz, LCD at %lu.%02lu MHz, keyboard %s, %uK",
      (unsigned long)(clock_get_hz(clk_sys) / (kHzPerKHz * kHzPerKHz)),
      (unsigned long)(g_status.lcd_spi_hz / (kHzPerKHz * kHzPerKHz)),
      (unsigned long)(g_status.lcd_spi_hz / (kHzPerKHz * 10) % 100), version,
      (unsigned)(kGuestRAMSize / 1024));
  DrawStatusLine(kStatusLineSystem, "System", value);
  DrawRunStatus();
}

// The same facts as the status screen, as key and value lines.
static void PrintReport(void) {
  char boot[kTextColumns + 1];
  FormatBootLine(boot, sizeof(boot));
  printf("\n");
  printf("%-14s %s\n", "runtime", "yax86 picocalc");
  printf(
      "%-14s %u\n", "sys_clk_khz",
      (unsigned)(clock_get_hz(clk_sys) / kHzPerKHz));
  printf("%-14s %s\n", "overclock", kOverclockNames[g_status.overclock]);
  printf("%-14s %u\n", "lcd_spi_hz", (unsigned)g_status.lcd_spi_hz);
  char version[16];
  FormatSouthbridgeVersion(version, sizeof(version));
  printf("%-14s %s\n", "keyboard_fw", version);
  printf("%-14s %u\n", "guest_ram_kb", (unsigned)(kGuestRAMSize / 1024));
  printf("%-14s %s\n", "boot", boot);
  printf(
      "%-14s %llu\n", "instructions",
      (unsigned long long)g_status.instructions);
  printf(
      "%-14s %llu\n", "boot_cycles", (unsigned long long)g_status.boot_cycles);
  printf(
      "%-14s %llu\n", "boot_us", (unsigned long long)g_status.boot_elapsed_us);
  printf(
      "%-14s %llu\n", "boot_busy_us",
      (unsigned long long)g_status.boot_busy_us);
  printf("%-14s %lu\n", "boot_speed_pm", (unsigned long)BootSpeedPermille());
  printf("%-14s %d\n", "draw_screen", YAX86_PICOCALC_DRAW_SCREEN);
  printf("%-14s %lu\n", "frames_drawn", (unsigned long)g_status.frames_drawn);
  printf(
      "%-14s %llu\n", "pixels_drawn",
      (unsigned long long)g_status.pixels_drawn);
  printf("%-14s %llu\n", "render_us", (unsigned long long)g_status.render_us);
  printf("%-14s %llu\n", "real_us", (unsigned long long)ElapsedUs());
  printf(
      "%-14s %llu\n", "emulated_us",
      (unsigned long long)(g_clock.cycles * kMicrosecondsPerSecond /
                           Y_PLATFORM_CPU_CYCLES_PER_SECOND));
  printf("%-14s %llu\n", "guest_clock_ms", (unsigned long long)GuestClockMs());
  printf("%-14s %llu\n", "waited_us", (unsigned long long)g_clock.waited_us);
  printf("%-14s %llu\n", "dropped_us", (unsigned long long)g_clock.dropped_us);
  printf("%-14s %d\n", "idle_skip", (int)g_platform.is_dos_idle_skip_enabled);
  printf("\n");
}

// Send a report if a host has attached since the last look, ending with the
// marker run.js waits for once the boot is over. Cheap enough to call every
// batch: it reads a flag the USB stack keeps.
static void ReportIfHostAttached(void) {
  const bool attached = stdio_usb_connected();
  if (attached && !g_host_attached) {
    PrintReport();
    if (BootIsOver()) {
      printf("done\n");
    }
  }
  g_host_attached = attached;
}

// ============================================================================
// The guest's screen
// ============================================================================

// The core renders only what changed since the last render, as rectangles,
// and the panel keeps its own frame memory, so a region goes straight to the
// panel as a window of its own.
static void BeginRenderRegion(
    Y_UNUSED YVideoState* video, YVideoRegion region) {
  g_region_on_panel = region.origin.x + region.width <= kGuestScreenWidth &&
                      region.origin.y + region.height <= kGuestScreenHeight;
  if (g_region_on_panel) {
    LcdBeginWrite(
        region.origin.x, (uint16_t)(kGuestScreenTop + region.origin.y),
        region.width, region.height);
  }
}

static void WriteGuestPixels(
    Y_UNUSED YVideoState* video, Y_UNUSED YVideoPosition origin,
    const YVideoRGB* pixels, uint8_t count) {
  if (!g_region_on_panel) {
    return;
  }
  for (uint8_t i = 0; i < count; ++i) {
    g_pixel_batch[i] = LcdRGB565(pixels[i].r, pixels[i].g, pixels[i].b);
  }
  LcdWritePixels(g_pixel_batch, count);
  g_status.pixels_drawn += count;
}

static void EndRenderRegion(Y_UNUSED YVideoState* video) {
  if (g_region_on_panel) {
    LcdEndWrite();
  }
}

// Bring the panel up to date with the guest's screen, timing it.
static void DrawGuestScreen(void) {
  const uint64_t pixels_before = g_status.pixels_drawn;
  const uint64_t start_us = time_us_64();
  YVideoRender(&g_platform.video);
  g_status.render_us += time_us_64() - start_us;
  if (g_status.pixels_drawn != pixels_before) {
    ++g_status.frames_drawn;
  }
}

// ============================================================================
// The machine
// ============================================================================

// Serve the boot floppy straight out of flash. Guest writes are discarded:
// 360KB is several times the SRAM left over, and booting to a prompt writes
// nothing.
static uint8_t FloppyReadByte(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, uint32_t offset) {
  return offset < kDOSFloppyDataSize ? kDOSFloppyData[offset] : 0xFF;
}

static void FloppyWriteByte(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, Y_UNUSED uint32_t offset,
    Y_UNUSED uint8_t value) {}

static bool InitMachine(void) {
  YPlatformConfig* config = &g_platform.config;
  config->logger_config = &g_logger_config;
  config->physical_memory_size = kGuestRAMSize;
  config->physical_memory = g_guest_ram;
  config->vram = g_vram;
  config->video_adapter = Y_VIDEO_ADAPTER_CGA;
  config->video_text_font.glyphs = &kFont4x10[0][0];
  config->video_text_font.width = kFont4x10Width;
  config->video_text_font.height = kFont4x10Height;
  if (!YPlatformInit(&g_platform)) {
    return false;
  }
  if (YAX86_PICOCALC_DRAW_SCREEN) {
    g_platform.video.config.begin_render_region = BeginRenderRegion;
    g_platform.video.config.write_pixels = WriteGuestPixels;
    g_platform.video.config.end_render_region = EndRenderRegion;
  }
  g_platform.fdc.config.read_image_byte = FloppyReadByte;
  g_platform.fdc.config.write_image_byte = FloppyWriteByte;
  YFDCInsertDisk(&g_platform.fdc, 0, &YFDCFormat360KB);
  return true;
}

// The character in a text screen cell, with anything unprintable read as a
// space so that the screen matches the way it looks.
static char ScreenCharacter(uint16_t cell) {
  const uint8_t value = g_vram[cell * kBytesPerScreenCell];
  return (value >= kFirstPrintableCharacter &&
          value < kFirstNonPrintableCharacter)
             ? (char)value
             : ' ';
}

static bool ScreenContains(const char* needle) {
  for (uint16_t start = 0; start < kScreenCells; ++start) {
    uint16_t i = 0;
    while (needle[i] != '\0' && start + i < kScreenCells &&
           ScreenCharacter((uint16_t)(start + i)) == needle[i]) {
      ++i;
    }
    if (needle[i] == '\0') {
      return true;
    }
  }
  return false;
}

// Answer whichever question DOS is asking, and return the stage the boot has
// reached. Stands in for the keyboard until the runtime reads one.
static BootStage AdvanceBoot(BootStage stage) {
  switch (stage) {
    case kBootStagePowerOn:
      if (ScreenContains("Enter new date")) {
        YKeyboardHandleKeyPress(&g_platform.keyboard, kEnterScancode);
        return kBootStageAnsweringDate;
      }
      break;
    case kBootStageAnsweringDate:
      if (ScreenContains("Enter new time")) {
        YKeyboardHandleKeyPress(&g_platform.keyboard, kEnterScancode);
        return kBootStageAnsweringTime;
      }
      break;
    case kBootStageAnsweringTime:
      if (ScreenContains("A>")) {
        return kBootStagePrompt;
      }
      break;
    case kBootStagePrompt:
    case kBootStageFailed:
      break;
  }
  return stage;
}

static void SetBootStage(BootStage stage) {
  g_status.boot_stage = stage;
  g_status.boot_cycles = g_clock.cycles;
  g_status.boot_elapsed_us = ElapsedUs();
  g_status.boot_busy_us = BusyUs();
  g_status.instructions = YCPUInstructionsRetired(&g_platform.cpu);
  DrawRunStatus();
  if (g_host_attached) {
    PrintReport();
  }
}

// Run a batch of emulated cycles, and return how the machine stands after it.
static YPlatformRunStatus RunBatch(void) {
  // An invalid opcode is not a stop: the 8088 has no invalid opcode
  // exception, so a real machine would carry on.
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
  g_clock.cycles += (uint32_t)(g_platform.ticks - g_clock.last_ticks);
  g_clock.last_ticks = g_platform.ticks;
  return status;
}

// Wait until real time has caught up with the emulated time the machine has
// reached, which is what keeps it from running faster than a real PC/XT.
//
// Only ever waits between batches, never changes them, so pacing cannot move
// anything the guest sees: the same batches run whether or not there is time
// to spare.
static void PaceToRealTime(void) {
  // The anchor moves a whole emulated second at a time, which keeps the
  // product below from overflowing however long the machine runs, and loses
  // nothing to rounding.
  while (g_clock.cycles - g_clock.anchor_cycles >=
         Y_PLATFORM_CPU_CYCLES_PER_SECOND) {
    g_clock.anchor_cycles += Y_PLATFORM_CPU_CYCLES_PER_SECOND;
    g_clock.anchor_us += kMicrosecondsPerSecond;
  }
  const uint64_t due_us =
      g_clock.anchor_us + (g_clock.cycles - g_clock.anchor_cycles) *
                              kMicrosecondsPerSecond /
                              Y_PLATFORM_CPU_CYCLES_PER_SECOND;
  const uint64_t now_us = time_us_64();
  if (now_us < due_us) {
    sleep_until(from_us_since_boot(due_us));
    g_clock.waited_us += time_us_64() - now_us;
  } else if (now_us - due_us > kMaxLagUs) {
    g_clock.dropped_us += now_us - due_us;
    g_clock.anchor_us = now_us;
    g_clock.anchor_cycles = g_clock.cycles;
  }
}

// Run the machine, paced to real time, for as long as the board has power.
// The boot is watched on the way, and DOS's questions answered.
static void RunMachine(bool machine_ready) {
  const uint64_t max_boot_cycles =
      (uint64_t)kBootMaxEmulatedSeconds * Y_PLATFORM_CPU_CYCLES_PER_SECOND;
  uint64_t next_poll_cycles = kBootPollCycles;
  uint64_t next_render_cycles = kCyclesPerRender;
  bool running = machine_ready;
  bool screen_on = true;
  absolute_time_t blank_at = at_the_end_of_time;
  absolute_time_t next_status_at = at_the_end_of_time;
  g_clock.last_ticks = g_platform.ticks;
  g_clock.start_us = time_us_64();
  g_clock.anchor_us = g_clock.start_us;

  for (;;) {
    ReportIfHostAttached();

    if (running && RunBatch() == Y_PLATFORM_HUNG) {
      printf("machine stopped: the CPU halted with nothing to wake it\n");
      running = false;
    }

    if (screen_on && g_clock.cycles >= next_render_cycles) {
      do {
        next_render_cycles += kCyclesPerRender;
      } while (next_render_cycles <= g_clock.cycles);
      DrawGuestScreen();
    }

    if (!BootIsOver()) {
      BootStage stage = g_status.boot_stage;
      if (!running || g_clock.cycles >= max_boot_cycles) {
        stage = kBootStageFailed;
      } else if (g_clock.cycles >= next_poll_cycles) {
        do {
          next_poll_cycles += kBootPollCycles;
        } while (next_poll_cycles <= g_clock.cycles);
        stage = AdvanceBoot(stage);
      }
      if (stage != g_status.boot_stage) {
        // Drawn first, so that the screen shows what the stage was decided
        // from and the time recorded for it includes drawing that.
        DrawGuestScreen();
        SetBootStage(stage);
      }
      if (BootIsOver()) {
        // DOS polls for a key at the prompt rather than halting, and the
        // idle skip is what keeps that from costing a real time loop's worth
        // of emulation. Not before now: DOS idles at the date and time
        // prompts too, and skipping there would move the dos-boot invariant.
        YPlatformSetDOSIdleSkip(&g_platform, true);
        blank_at = make_timeout_time_ms(kScreenOnAfterBootMs);
        next_status_at = make_timeout_time_ms(kStatusUpdateMs);
        // So that a host that watched the boot gets the final report too.
        g_host_attached = false;
      }
    }

    if (screen_on && time_reached(next_status_at)) {
      next_status_at = delayed_by_ms(next_status_at, kStatusUpdateMs);
      DrawRunStatus();
    }
    if (screen_on && time_reached(blank_at)) {
      LcdSetDisplayOn(false);
      screen_on = false;
    }

    if (running) {
      PaceToRealTime();
    } else {
      sleep_ms(kStoppedPollMs);
    }
  }
}

// ============================================================================
// Startup
// ============================================================================

#if YAX86_PICOCALC_SYS_CLK_KHZ
// Raise the system clock, after USB is up and with the watchdog armed, as
// pico/bench does. A clock the chip cannot take hangs it, and the watchdog
// turns that into a reboot rather than a board with no way in but the BOOTSEL
// button, which on a PicoCalc is inside the case. The reboot lands here again,
// where the marker says not to try the same clock twice.
//
// watchdog_caused_reboot() alone is not evidence of a hang: picotool reboots
// the chip through the watchdog too. The marker lives in RAM that startup does
// not clear, so it survives a reset but not a power cycle, and it is only ever
// left set across the window where a hang is possible.
//
// The watchdog is disarmed again once the change is survived, and must not be
// left running and fed. The SDK carries out picotool's reboot request with
// watchdog_reboot(), which only shortens the watchdog's countdown, so feeding
// it would put the reboot off indefinitely. A hang after this is a power
// button press away from recovery, since the keyboard controller cuts power.
static OverclockResult Overclock(void) {
  if (g_overclock_marker == kOverclockAttemptMarker) {
    g_overclock_marker = 0;
    return kOverclockSkippedAfterHang;
  }
  g_overclock_marker = kOverclockAttemptMarker;
  watchdog_enable(kWatchdogTimeoutMs, true);
  if (YAX86_PICOCALC_SYS_CLK_KHZ > 250000) {
    vreg_set_voltage(VREG_VOLTAGE_1_30);
  } else if (YAX86_PICOCALC_SYS_CLK_KHZ > 125000) {
    vreg_set_voltage(VREG_VOLTAGE_1_20);
  }
  busy_wait_us(kVoltageSettleUs);
  const bool applied = set_sys_clock_khz(YAX86_PICOCALC_SYS_CLK_KHZ, false);
  watchdog_disable();
  g_overclock_marker = 0;
  return applied ? kOverclockApplied : kOverclockPLLCannot;
}
#endif

int main(void) {
  stdio_init_all();
#if YAX86_PICOCALC_SYS_CLK_KHZ
  // Before anything else is brought up, since each works out its clock
  // divider when it is: the LCD's from the system clock, and the keyboard
  // controller's I2C from the peripheral clock, which the overclock moves to
  // the 48MHz USB PLL.
  g_status.overclock = Overclock();
#endif

  g_logger_config.write_line = WriteLogLine;
  g_logger_config.enabled_modules = 0xFFFFFFFF;
  g_logger_config.min_level = Y_LOG_LEVEL_ERROR;

  g_status.lcd_spi_hz = LcdInit();
  SouthbridgeInit();
  g_status.southbridge_answered =
      SouthbridgeReadVersion(&g_status.southbridge_version);
  DrawStatusScreen();

  RunMachine(InitMachine());
  return 0;
}
