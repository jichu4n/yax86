// yax86 on the ClockworkPi PicoCalc.
//
// This is the start of the runtime: it brings up the LCD and the keyboard
// controller, shows what it found, and boots MS-DOS 3.30 off the floppy image
// in flash to the command prompt. The guest's own screen is not drawn yet, and
// the keyboard is not read - DOS's date and time questions are answered by
// watching the screen, the way pico/bench does it.
//
// Everything is reported twice: on the LCD, so that the device says what it is
// doing with nothing attached, and over USB serial for pico/bench/run.js to
// capture. A report is printed whenever a host attaches, since USB CDC drops
// whatever is written before then.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "core/platform.h"
#include "core/video.h"
#include "hardware/clocks.h"
#include "lcd.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include "southbridge.h"
#include "text.h"
// Generated into the build directory - see the floppy image section of
// CMakeLists.txt.
#include "generated/dos_floppy_data.h"

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

  // How often the idle loop looks for a newly attached host.
  kIdlePollMs = 100,

  kMicrosecondsPerMillisecond = 1000,
  kMillisecondsPerSecond = 1000,
  kHzPerKHz = 1000,

  // Where the status screen's lines go, in 4x10 character cells.
  kBannerRow = 1,
  kURLRow = 2,
  kStatusFirstRow = 4,
  kStatusLabelColumn = 1,
  kStatusValueColumn = 13,
  kPaletteLabelRow = 27,
  kPaletteTop = 280,
  kPaletteHeight = 30,
  kPaletteSwatchWidth = kLcdWidth / Y_VIDEO_NUM_CGA_COLORS,
};

// The status screen's lines, in the order they appear.
typedef enum StatusLine {
  kStatusLineClock = 0,
  kStatusLineLCD,
  kStatusLineKeyboard,
  kStatusLineMachine,
  kStatusLineBoot,
} StatusLine;

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
  uint32_t lcd_spi_hz;
  bool southbridge_answered;
  uint8_t southbridge_version;
  BootStage boot_stage;
  // Emulated and host time at the point boot_stage was last changed.
  uint64_t boot_cycles;
  uint64_t boot_elapsed_us;
  uint64_t instructions;
} RuntimeStatus;

static uint8_t g_guest_ram[kGuestRAMSize];
static uint8_t g_vram[Y_VIDEO_CGA_VRAM_SIZE];
static YPlatformState g_platform;
// Held for the platform's lifetime, which is why it is not a local.
static YLoggerConfig g_logger_config;
static RuntimeStatus g_status;
static bool g_host_attached;

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

static void DrawBootStatus(void) {
  char value[kTextColumns + 1];
  FormatBootLine(value, sizeof(value));
  DrawStatusLine(kStatusLineBoot, "Boot", value);
}

static void DrawStatusScreen(void) {
  TextDrawString(
      kStatusLabelColumn, kBannerRow,
      "yax86 - an IBM PC/XT emulator, on the ClockworkPi PicoCalc",
      CGAColor(14), CGAColor(0));
  TextDrawString(
      kStatusLabelColumn, kURLRow, "https://github.com/jichu4n/yax86",
      CGAColor(8), CGAColor(0));

  char value[kTextColumns + 1];
  snprintf(
      value, sizeof(value), "%lu MHz",
      (unsigned long)(clock_get_hz(clk_sys) / (kHzPerKHz * kHzPerKHz)));
  DrawStatusLine(kStatusLineClock, "CPU clock", value);
  snprintf(
      value, sizeof(value), "ST7365P, 320x320 RGB565, SPI at %lu.%02lu MHz",
      (unsigned long)(g_status.lcd_spi_hz / (kHzPerKHz * kHzPerKHz)),
      (unsigned long)(g_status.lcd_spi_hz / (kHzPerKHz * 10) % 100));
  DrawStatusLine(kStatusLineLCD, "Display", value);
  char version[16];
  FormatSouthbridgeVersion(version, sizeof(version));
  snprintf(value, sizeof(value), "STM32 controller, firmware %s", version);
  DrawStatusLine(kStatusLineKeyboard, "Keyboard", value);
  snprintf(
      value, sizeof(value), "%uK RAM, CGA, MS-DOS 3.30 from flash",
      (unsigned)(kGuestRAMSize / 1024));
  DrawStatusLine(kStatusLineMachine, "Machine", value);
  DrawBootStatus();

  // The sixteen CGA colors, so that the panel's color order and inversion can
  // be checked by eye: black on the left, white on the right, and blue,
  // green, cyan, red, magenta and brown in that order in between.
  TextDrawString(
      kStatusLabelColumn, kPaletteLabelRow, "CGA palette, colors 0 to 15",
      CGAColor(7), CGAColor(0));
  for (uint8_t i = 0; i < Y_VIDEO_NUM_CGA_COLORS; ++i) {
    LcdFillRect(
        (uint16_t)(i * kPaletteSwatchWidth), kPaletteTop, kPaletteSwatchWidth,
        kPaletteHeight, CGAColor(i));
  }
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
  printf("%-14s %u\n", "lcd_spi_hz", (unsigned)g_status.lcd_spi_hz);
  char version[16];
  FormatSouthbridgeVersion(version, sizeof(version));
  printf("%-14s %s\n", "keyboard_fw", version);
  printf("%-14s %u\n", "guest_ram_kb", (unsigned)(kGuestRAMSize / 1024));
  printf("%-14s %s\n", "boot", boot);
  printf(
      "%-14s %llu\n", "instructions",
      (unsigned long long)g_status.instructions);
  printf("\n");
}

// Send a report if a host has attached since the last look. Cheap enough to
// call every batch: it reads a flag the USB stack keeps.
static void ReportIfHostAttached(void) {
  const bool attached = stdio_usb_connected();
  if (attached && !g_host_attached) {
    PrintReport();
  }
  g_host_attached = attached;
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
  if (!YPlatformInit(&g_platform)) {
    return false;
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

static void SetBootStage(BootStage stage, uint64_t cycles, uint64_t start_us) {
  g_status.boot_stage = stage;
  g_status.boot_cycles = cycles;
  g_status.boot_elapsed_us = time_us_64() - start_us;
  g_status.instructions = YCPUInstructionsRetired(&g_platform.cpu);
  DrawBootStatus();
  if (g_host_attached) {
    PrintReport();
  }
}

// Run the machine until DOS gives a prompt, or until it is clear it will not.
static void BootToPrompt(void) {
  const uint64_t max_cycles =
      (uint64_t)kBootMaxEmulatedSeconds * Y_PLATFORM_CPU_CYCLES_PER_SECOND;
  uint64_t cycles = 0;
  uint64_t next_poll_cycles = kBootPollCycles;
  // The platform's cycle counter is 32 bits, so the total is accumulated from
  // per-batch deltas, which unsigned subtraction keeps right across a wrap.
  uint32_t last_ticks = g_platform.ticks;
  const uint64_t start_us = time_us_64();

  while (cycles < max_cycles) {
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
    cycles += (uint32_t)(g_platform.ticks - last_ticks);
    last_ticks = g_platform.ticks;
    if (status != Y_PLATFORM_RUNNING && status != Y_PLATFORM_INVALID) {
      printf("machine stopped with status %d\n", (int)status);
      break;
    }
    ReportIfHostAttached();

    if (cycles < next_poll_cycles) {
      continue;
    }
    do {
      next_poll_cycles += kBootPollCycles;
    } while (next_poll_cycles <= cycles);
    const BootStage stage = AdvanceBoot(g_status.boot_stage);
    if (stage != g_status.boot_stage) {
      SetBootStage(stage, cycles, start_us);
      if (stage == kBootStagePrompt) {
        return;
      }
    }
  }
  SetBootStage(kBootStageFailed, cycles, start_us);
}

// ============================================================================
// Startup
// ============================================================================

int main(void) {
  stdio_init_all();

  g_logger_config.write_line = WriteLogLine;
  g_logger_config.enabled_modules = 0xFFFFFFFF;
  g_logger_config.min_level = Y_LOG_LEVEL_ERROR;

  g_status.lcd_spi_hz = LcdInit();
  SouthbridgeInit();
  g_status.southbridge_answered =
      SouthbridgeReadVersion(&g_status.southbridge_version);
  DrawStatusScreen();

  if (!InitMachine()) {
    g_status.boot_stage = kBootStageFailed;
    DrawBootStatus();
  } else {
    BootToPrompt();
  }

  // The machine is not run past the prompt yet: there is no keyboard to type
  // at it and no screen to show what it does. What is left is to tell any host
  // that attaches how the boot went, ending with the marker run.js waits for.
  // Starting from detached makes a host that watched the whole boot get the
  // final report too.
  g_host_attached = false;
  for (;;) {
    const bool attached = stdio_usb_connected();
    if (attached && !g_host_attached) {
      PrintReport();
      printf("done\n");
    }
    g_host_attached = attached;
    sleep_ms(kIdlePollMs);
  }
  return 0;
}
