#include "report.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "command.h"
#include "core/video.h"
#include "display.h"
#include "floppy.h"
#include "harddisk.h"
#include "hardware/clocks.h"
#include "menu.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include "runtime_log.h"
#include "sdcard.h"
#include "southbridge.h"
#include "speaker.h"
#include "typing.h"
#include "usb_disk.h"

#ifndef YAX86_PICOCALC_DRAW_SCREEN
#define YAX86_PICOCALC_DRAW_SCREEN 1
#endif

enum {
  // What the stacks are painted with. Any value does, as long as a stack is
  // unlikely to hold it.
  kStackPaint = 0x5AC4DEAD,
  // What painting core 0's stack leaves of it below the painter's own frame.
  kStackPaintMargin = 64,

  // The BIOS's count of timer ticks since midnight, in the BIOS data area,
  // which is the time of day DOS reports. A tick is 65,536 counts of the
  // PIT's 1,193,182Hz input clock, about 18.2 a second.
  kBIOSTickCountAddress = 0x46C,
  kPITInputHz = 1193182,
  kPITCountsPerBIOSTick = 65536,

  kMicrosecondsPerMillisecond = 1000,
  kMillisecondsPerSecond = 1000,
  kMicrosecondsPerSecond = 1000 * 1000,
  kHzPerKHz = 1000,

  // CRT controller registers 12 to 15: where the screen starts in video
  // memory, and where the cursor is, each in character cells.
  kCRTCStartAddressHigh = 12,
  kCRTCStartAddressLow = 13,
  kCRTCCursorHigh = 14,
  kCRTCCursorLow = 15,
  kBytesPerTextCell = 2,
  kMaxTextColumns = 80,
};

// What came of the overclock, as the report gives it.
static const char* const kOverclockNames[] = {
    [kOverclockNotRequested] = "not_requested",
    [kOverclockApplied] = "applied",
    [kOverclockPLLCannot] = "pll_cannot_produce_clock",
    [kOverclockSkippedAfterHang] = "skipped_after_hang",
};

static const char* const kBootStageNames[] = {
    [kBootStagePowerOn] = "running POST and loading DOS",
    [kBootStageAnsweringDate] = "answered the date prompt",
    [kBootStageAnsweringTime] = "answered the time prompt",
    [kBootStagePrompt] = "reached the A> prompt",
    [kBootStageFailed] = "did not reach the prompt",
};

// The same, as a log gives them.
static const char* const kBootStageEvents[] = {
    [kBootStagePowerOn] = "power_on",
    [kBootStageAnsweringDate] = "date_prompt",
    [kBootStageAnsweringTime] = "time_prompt",
    [kBootStagePrompt] = "prompt",
    [kBootStageFailed] = "failed",
};

extern uint32_t __StackBottom;
extern uint32_t __StackTop;
extern uint32_t __StackOneBottom;
extern uint32_t __StackOneTop;

static ReportConfig g_config;
static bool g_host_attached;
static uint64_t g_command_us;

void ReportInit(const ReportConfig* config) { g_config = *config; }

uint64_t ReportCommandUs(void) { return g_command_us; }

// Paint core 0's stack from its bottom to a little below this function's
// frame, and core 1's, before core 1 starts on it, whole.
__noinline void ReportPaintStacks(void) {
  volatile uint32_t here = 0;
  uint32_t* const end = (uint32_t*)((uintptr_t)&here - kStackPaintMargin) - 1;
  for (uint32_t* word = &__StackBottom; word < end; ++word) {
    *word = kStackPaint;
  }
  for (uint32_t* word = &__StackOneBottom; word < &__StackOneTop; ++word) {
    *word = kStackPaint;
  }
}

// The most of a stack that has been used: from its top down to the lowest
// word no longer as painted.
static uint32_t StackUsed(const uint32_t* bottom, const uint32_t* top) {
  const uint32_t* word = bottom;
  while (word < top && *word == kStackPaint) {
    ++word;
  }
  return (uint32_t)((uintptr_t)top - (uintptr_t)word);
}

static bool BootIsOver(void) {
  return g_config.status->boot_stage == kBootStagePrompt ||
         g_config.status->boot_stage == kBootStageFailed;
}

static uint64_t ElapsedUs(void) {
  return time_us_64() - g_config.clock->start_us;
}

static uint64_t PausedUs(void) {
  return g_config.clock->paused_us +
         (MenuIsOpen() ? time_us_64() - g_config.clock->pause_start_us : 0);
}

static uint64_t GuestClockMs(void) {
  const uint8_t* const count = &g_config.guest_ram[kBIOSTickCountAddress];
  const uint64_t ticks = (uint64_t)count[0] | (uint64_t)count[1] << 8 |
                         (uint64_t)count[2] << 16 | (uint64_t)count[3] << 24;
  return ticks * kPITCountsPerBIOSTick * kMillisecondsPerSecond / kPITInputHz;
}

static uint32_t BootSpeedPermille(void) {
  if (g_config.status->boot_busy_us == 0) {
    return 0;
  }
  return (uint32_t)(g_config.status->boot_cycles * 1000 *
                    kMicrosecondsPerSecond / Y_PLATFORM_CPU_CYCLES_PER_SECOND /
                    g_config.status->boot_busy_us);
}

static void FormatSouthbridgeVersion(char* buffer, size_t size) {
  if (!g_config.status->southbridge_answered) {
    snprintf(buffer, size, "not answering");
  } else if (g_config.status->southbridge_version == 0) {
    snprintf(buffer, size, "older than v1.6");
  } else {
    snprintf(
        buffer, size, "v%X.%X", g_config.status->southbridge_version >> 4,
        g_config.status->southbridge_version & 0xF);
  }
}

static void FormatBootLine(char* buffer, size_t size) {
  const uint64_t emulated_ms = g_config.status->boot_cycles *
                               kMillisecondsPerSecond /
                               Y_PLATFORM_CPU_CYCLES_PER_SECOND;
  const uint64_t host_ms =
      g_config.status->boot_elapsed_us / kMicrosecondsPerMillisecond;
  if (g_config.status->boot_stage == kBootStagePowerOn) {
    snprintf(buffer, size, "%s", kBootStageNames[g_config.status->boot_stage]);
    return;
  }
  snprintf(
      buffer, size, "%s at %llu.%03llus emulated, %llu.%03llus real",
      kBootStageNames[g_config.status->boot_stage],
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

static void FormatClockLine(char* buffer, size_t size) {
  static uint64_t window_start_us;
  static uint64_t window_idle_us;
  const uint64_t now_us = ElapsedUs();
  const uint64_t idle_now_us = g_config.clock->waited_us + PausedUs();
  const uint64_t window_us = now_us - window_start_us;
  const uint64_t waited_us = idle_now_us - window_idle_us;
  const uint32_t busy_percent =
      window_us > 0 && waited_us <= window_us
          ? (uint32_t)((window_us - waited_us) * 100 / window_us)
          : 0;
  window_start_us = now_us;
  window_idle_us = idle_now_us;

  const uint64_t guest_ds = GuestClockMs() / 100;
  const uint64_t real_ds = now_us / (100 * kMicrosecondsPerMillisecond);
  snprintf(
      buffer, size,
      "guest %llu.%llus, real %llu.%llus since power on, %lu%% busy",
      (unsigned long long)(guest_ds / 10), (unsigned long long)(guest_ds % 10),
      (unsigned long long)(real_ds / 10), (unsigned long long)(real_ds % 10),
      (unsigned long)busy_percent);
}

static void FormatSystemLine(char* buffer, size_t size) {
  char version[16];
  FormatSouthbridgeVersion(version, sizeof(version));
  snprintf(
      buffer, size,
      "%lu MHz, LCD at %lu.%02lu MHz, keyboard %.10s, %lu keys, %uK",
      (unsigned long)(clock_get_hz(clk_sys) / (kHzPerKHz * kHzPerKHz)),
      (unsigned long)(g_config.status->lcd_spi_hz / (kHzPerKHz * kHzPerKHz)),
      (unsigned long)(g_config.status->lcd_spi_hz / (kHzPerKHz * 10) % 100),
      version, (unsigned long)SouthbridgeGetPollStats().keys,
      (unsigned)(g_config.guest_ram_kb % 10000));
}

static void FormatDisplayLine(char* buffer, size_t size) {
  if (!YAX86_PICOCALC_DRAW_SCREEN) {
    snprintf(buffer, size, "not drawn in this build");
    return;
  }
  const DisplayStats stats = DisplayGetStats();
  const uint64_t render_ms = stats.render_us / kMicrosecondsPerMillisecond;
  snprintf(
      buffer, size, "%lu frames, %llu.%03llus drawing on core 1, %lu late",
      (unsigned long)stats.frames_drawn,
      (unsigned long long)(render_ms / kMillisecondsPerSecond),
      (unsigned long long)(render_ms % kMillisecondsPerSecond),
      (unsigned long)stats.frames_late);
}

static size_t Filled(int written, size_t size) {
  if (written < 0) {
    return 0;
  }
  return (size_t)written < size ? (size_t)written : size - 1;
}

static size_t FormatDrive(
    char* buffer, size_t size, const char* drive, const DiskImage* image,
    const char* instead) {
  if (image->status != kDiskImageOpen) {
    return Filled(
        snprintf(
            buffer, size, "%s %s, %s  ", drive, instead,
            DiskImageDescribeStatus(image->status)),
        size);
  }
  return Filled(
      snprintf(
          buffer, size, "%s %lu read, %lu written  ", drive,
          (unsigned long)image->stats.sector_reads,
          (unsigned long)image->stats.sector_writes),
      size);
}

static void FormatDiskLine(char* buffer, size_t size) {
  const DiskImage* const images[kVMDrives] = {
      [kVMDriveFD1] = FloppyGetImage(kFloppyDriveA),
      [kVMDriveFD2] = FloppyGetImage(kFloppyDriveB),
      [kVMDriveHD1] = HardDiskGetImage(kHardDisk1),
      [kVMDriveHD2] = HardDiskGetImage(kHardDisk2),
  };
  size_t length = 0;
  uint64_t card_us = 0;
  bool any_open = false;
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    const DiskImage* image = images[drive];
    char label[8];
    snprintf(label, sizeof(label), "%s:", kVMDriveLabels[drive]);
    if (image->status == kDiskImageOpen) {
      any_open = true;
      length += FormatDrive(buffer + length, size - length, label, image, "");
    } else if (image->status != kDiskImageEjected) {
      // What drive A has instead, or why a configured image did not open.
      length += FormatDrive(
          buffer + length, size - length, label, image,
          drive == kVMDriveFD1 && !*g_config.has_vm ? "built in" : "empty");
    }
    card_us += image->stats.read_us + image->stats.write_us;
  }
  if (!any_open) {
    return;
  }
  const uint64_t card_ms = card_us / kMicrosecondsPerMillisecond;
  snprintf(
      buffer + length, size - length, "%llu.%03llus",
      (unsigned long long)(card_ms / kMillisecondsPerSecond),
      (unsigned long long)(card_ms % kMillisecondsPerSecond));
}

void ReportDescribeStatus(MenuStatusLine lines[kMenuStatusLines]) {
  static const char* const kLabels[kMenuStatusLines] = {
      "System", "Boot", "Speed", "Clock", "Display", "Disk",
  };
  for (uint8_t i = 0; i < kMenuStatusLines; ++i) {
    lines[i].label = kLabels[i];
  }
  const size_t size = sizeof(lines[0].value);
  FormatSystemLine(lines[0].value, size);
  FormatBootLine(lines[1].value, size);
  FormatSpeedLine(lines[2].value, size);
  FormatClockLine(lines[3].value, size);
  FormatDisplayLine(lines[4].value, size);
  FormatDiskLine(lines[5].value, size);
}

#define BOOT_FIELDS()                                                    \
  Y_LOG_STRING("stage", kBootStageEvents[g_config.status->boot_stage]),  \
      Y_LOG_UINT("instructions", g_config.status->instructions),         \
      Y_LOG_UINT("cycles", g_config.status->boot_cycles),                \
      Y_LOG_UINT(                                                        \
          "emulated_us", g_config.status->boot_cycles*                   \
                             kMicrosecondsPerSecond /                    \
                             Y_PLATFORM_CPU_CYCLES_PER_SECOND),          \
      Y_LOG_UINT("real_us", g_config.status->boot_elapsed_us),           \
      Y_LOG_UINT("busy_us", g_config.status->boot_busy_us),              \
      Y_LOG_UINT("speed_pm", BootSpeedPermille())

void ReportLogBootStage(void) { BOOT_INFO("boot_stage", BOOT_FIELDS()); }

#define REPLY_GROUP(command, group, ...)                 \
  Y_LOG_REPLY(                                           \
      &g_runtime_logger, (command)->id, (command)->name, \
      Y_LOG_STRING("group", (group)), __VA_ARGS__)

static void ReplyDiskImage(
    const Command* command, const char* group, const DiskImage* image) {
  const DiskImageStats* stats = &image->stats;
  REPLY_GROUP(
      command, group,
      Y_LOG_STRING("image", DiskImageDescribeStatus(image->status)),
      Y_LOG_STRING("path", image->path != NULL ? image->path : ""),
      Y_LOG_BOOL("fast_seek", image->fast_seek),
      Y_LOG_UINT("reads", stats->sector_reads),
      Y_LOG_UINT("writes", stats->sector_writes),
      Y_LOG_UINT("errors", stats->errors),
      Y_LOG_UINT("read_us", stats->read_us),
      Y_LOG_UINT("write_us", stats->write_us));
}

static __noinline void ReplyReportBoard(const Command* command) {
  char version[16];
  FormatSouthbridgeVersion(version, sizeof(version));
  REPLY_GROUP(
      command, "system", Y_LOG_STRING("runtime", "yax86 picocalc"),
      Y_LOG_UINT("sys_clk_khz", clock_get_hz(clk_sys) / kHzPerKHz),
      Y_LOG_STRING("overclock", kOverclockNames[g_config.status->overclock]),
      Y_LOG_UINT("lcd_spi_hz", g_config.status->lcd_spi_hz),
      Y_LOG_UINT("sd_clock_hz", SdcardGetClockHz()),
      Y_LOG_UINT("sd_blocks", SdcardGetBlockCount()),
      Y_LOG_STRING("keyboard_fw", version),
      Y_LOG_UINT("guest_ram_kb", g_config.guest_ram_kb));
  REPLY_GROUP(
      command, "stack",
      Y_LOG_UINT("size", (uintptr_t)&__StackTop - (uintptr_t)&__StackBottom),
      Y_LOG_UINT("core0_used", StackUsed(&__StackBottom, &__StackTop)),
      Y_LOG_UINT("core1_used", StackUsed(&__StackOneBottom, &__StackOneTop)));
  REPLY_GROUP(command, "boot", BOOT_FIELDS());
  const DisplayStats display = DisplayGetStats();
  REPLY_GROUP(
      command, "display", Y_LOG_BOOL("draw_screen", YAX86_PICOCALC_DRAW_SCREEN),
      Y_LOG_STRING("font", Font4x10Name(g_config.settings->font)),
      Y_LOG_UINT("frames_drawn", display.frames_drawn),
      Y_LOG_UINT("pixels_drawn", display.pixels_drawn),
      Y_LOG_UINT("render_us", display.render_us),
      Y_LOG_UINT("frames_late", display.frames_late),
      Y_LOG_UINT("copy_us", display.copy_us));
}

static __noinline void ReplyReportDevices(const Command* command) {
  ReplyDiskImage(command, "fd1", FloppyGetImage(kFloppyDriveA));
  ReplyDiskImage(command, "fd2", FloppyGetImage(kFloppyDriveB));
  ReplyDiskImage(command, "hd1", HardDiskGetImage(kHardDisk1));
  ReplyDiskImage(command, "hd2", HardDiskGetImage(kHardDisk2));
  REPLY_GROUP(
      command, "vm",
      Y_LOG_STRING("name", *g_config.has_vm ? g_config.vm->entry.name : ""),
      Y_LOG_UINT("vms", g_config.vms->count));
  const UsbDiskStats usb = UsbDiskGetStats();
  REPLY_GROUP(
      command, "usb", Y_LOG_BOOL("sharing", usb.sharing),
      Y_LOG_STRING("image", usb.image_path != NULL ? usb.image_path : ""),
      Y_LOG_BOOL("connected", usb.connected), Y_LOG_BOOL("seen", usb.seen),
      Y_LOG_BOOL("ejected", usb.ejected),
      Y_LOG_UINT("blocks_read", usb.blocks_read),
      Y_LOG_UINT("blocks_written", usb.blocks_written),
      Y_LOG_UINT("read_us", usb.read_us), Y_LOG_UINT("write_us", usb.write_us),
      Y_LOG_UINT("errors", usb.errors));
  const SouthbridgePollStats keyboard = SouthbridgeGetPollStats();
  REPLY_GROUP(
      command, "keyboard", Y_LOG_UINT("reads", keyboard.reads),
      Y_LOG_UINT("keys", keyboard.keys),
      Y_LOG_UINT("stale_keys", keyboard.stale_keys),
      Y_LOG_UINT("aborts", keyboard.aborts),
      Y_LOG_UINT("timeouts", keyboard.timeouts));
}

static __noinline void ReplyReportRunning(const Command* command) {
  const SpeakerStats speaker = SpeakerGetStats();
  REPLY_GROUP(
      command, "speaker", Y_LOG_UINT("frequency_hz", speaker.frequency_hz),
      Y_LOG_UINT("output_hz", speaker.output_hz),
      Y_LOG_UINT("changes", speaker.changes));
  REPLY_GROUP(
      command, "menu", Y_LOG_BOOL("open", MenuIsOpen()),
      Y_LOG_STRING("page", MenuPageName()));
  REPLY_GROUP(
      command, "pacing", Y_LOG_UINT("real_us", ElapsedUs()),
      Y_LOG_UINT(
          "emulated_us", g_config.clock->cycles * kMicrosecondsPerSecond /
                             Y_PLATFORM_CPU_CYCLES_PER_SECOND),
      Y_LOG_UINT("guest_clock_ms", GuestClockMs()),
      Y_LOG_UINT("waited_us", g_config.clock->waited_us),
      Y_LOG_UINT("dropped_us", g_config.clock->dropped_us),
      Y_LOG_UINT("command_us", g_command_us),
      Y_LOG_UINT("paused_us", PausedUs()),
      Y_LOG_BOOL("idle_skip", g_config.platform->is_dos_idle_skip_enabled));
}

static void ReplyReport(const Command* command) {
  ReplyReportBoard(command);
  ReplyReportDevices(command);
  ReplyReportRunning(command);
  CommandReplyOk(command);
}

void ReportAnnounceToNewHost(void) {
  const bool attached = stdio_usb_connected();
  if (attached && !g_host_attached && BootIsOver()) {
    BOOT_INFO("done");
  }
  g_host_attached = attached;
}

static void RunScreen(const Command* command) {
  const YVideoState* video = &g_config.platform->video;
  const YVideoModeMetadata* mode = YVideoGetModeMetadata(video);
  if (mode->type != Y_VIDEO_MODE_TEXT || mode->columns > kMaxTextColumns) {
    CommandReplyError(command, "not_text_mode");
    return;
  }
  const char* attrs = CommandArgument(command, "attrs");
  const bool with_attributes = attrs != NULL && attrs[0] == 't';
  const uint8_t* registers = video->registers;
  const uint32_t start = ((uint32_t)registers[kCRTCStartAddressHigh] << 8) |
                         registers[kCRTCStartAddressLow];
  const uint32_t cursor =
      ((uint32_t)registers[kCRTCCursorHigh] << 8) | registers[kCRTCCursorLow];
  const uint32_t cells = (uint32_t)mode->columns * mode->rows;
  const uint32_t vram_size = YVideoGetAdapterMetadata(video)->vram_size;
  const uint32_t cursor_cell = cursor - start;
  if (cursor >= start && cursor_cell < cells) {
    Y_LOG_REPLY(
        &g_runtime_logger, command->id, command->name,
        Y_LOG_HEX8("mode", mode->mode), Y_LOG_UINT("columns", mode->columns),
        Y_LOG_UINT("rows", mode->rows),
        Y_LOG_UINT("cursor_row", cursor_cell / mode->columns),
        Y_LOG_UINT("cursor_column", cursor_cell % mode->columns));
  } else {
    Y_LOG_REPLY(
        &g_runtime_logger, command->id, command->name,
        Y_LOG_HEX8("mode", mode->mode), Y_LOG_UINT("columns", mode->columns),
        Y_LOG_UINT("rows", mode->rows));
  }
  for (uint8_t row = 0; row < mode->rows; ++row) {
    uint8_t characters[kMaxTextColumns];
    uint8_t attributes[kMaxTextColumns];
    for (uint8_t column = 0; column < mode->columns; ++column) {
      const uint32_t cell = start + (uint32_t)row * mode->columns + column;
      const uint32_t offset = (cell * kBytesPerTextCell) % vram_size;
      characters[column] = g_config.vram[offset];
      attributes[column] = g_config.vram[(offset + 1) % vram_size];
    }
    char hex[kMaxTextColumns * 2 + 1];
    for (uint8_t column = 0; column < mode->columns; ++column) {
      snprintf(&hex[column * 2], 3, "%02X", characters[column]);
    }
    Y_LOG_REPLY(
        &g_runtime_logger, command->id, command->name, Y_LOG_UINT("row", row),
        Y_LOG_STRING("chars", hex));
    if (with_attributes) {
      for (uint8_t column = 0; column < mode->columns; ++column) {
        snprintf(&hex[column * 2], 3, "%02X", attributes[column]);
      }
      Y_LOG_REPLY(
          &g_runtime_logger, command->id, command->name, Y_LOG_UINT("row", row),
          Y_LOG_STRING("attrs", hex));
    }
  }
  CommandReplyOk(command);
}

static void RunKey(const Command* command) {
  const char* name = CommandArgument(command, "name");
  const char* state = CommandArgument(command, "state");
  TypingAction action = kTypingTap;
  if (state != NULL && state[0] == 'p') {
    action = kTypingPress;
  } else if (state != NULL && state[0] == 'r') {
    action = kTypingRelease;
  }
  uint8_t code = 0;
  if (name == NULL) {
    CommandReplyError(command, "missing_name");
  } else if (!TypingFindKey(name, &code)) {
    CommandReplyError(command, "unknown_key");
  } else if (!TypingQueueKey(code, action)) {
    CommandReplyError(command, "queue_full");
  } else {
    CommandReplyOk(command);
  }
}

void ReportRunCommand(const Command* command) {
  const uint64_t start_us = time_us_64();
  if (CommandIs(command, "ping")) {
    CommandReplyOk(command);
  } else if (CommandIs(command, "report")) {
    ReplyReport(command);
  } else if (CommandIs(command, "screen")) {
    RunScreen(command);
  } else if (CommandIs(command, "screenshot")) {
    if (!DisplaySendScreenshot(command->id)) {
      CommandReplyError(command, "busy");
    }
  } else if (CommandIs(command, "key")) {
    RunKey(command);
  } else {
    CommandReplyError(command, "unknown_command");
  }
  g_command_us += time_us_64() - start_us;
}
