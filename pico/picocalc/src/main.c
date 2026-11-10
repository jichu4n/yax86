// yax86 on the ClockworkPi PicoCalc.
//
// The runtime raises the system clock, starts core 1 drawing on the LCD, brings
// up the keyboard controller, and boots MS-DOS 3.30 off the floppy in drive A -
// an image on the SD card, or the one built into flash without it - with the
// guest's text screen on the LCD as it goes. A hard disk image on the card is
// attached as well, if there is one. The machine is
// paced to real time and keeps running past the prompt, with the PicoCalc's
// keyboard as its own. DOS's date and time questions are still answered by
// watching the screen, the way pico/bench does it, which keeps the boot the
// dos-boot workload.
//
// Core 0, which this file runs on, runs the machine and never touches the
// LCD: core 1 draws everything on it, from copies core 0 hands over - see
// display.h. The guest's screen takes the top 250 rows of the panel - 80 by 25
// cells of the 4x10 font - and the runtime's status lines the rest. Everything
// on them is also reported over USB serial for pico/bench/run.js to capture. A
// report is printed whenever a host attaches, since USB CDC drops whatever is
// written before then.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "command.h"
#include "core/platform.h"
#include "core/video.h"
#include "display.h"
#include "file_list.h"
#include "floppy.h"
#include "harddisk.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/watchdog.h"
#include "keymap.h"
#include "machine_config.h"
#include "menu.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include "runtime_log.h"
#include "sdcard.h"
#include "southbridge.h"
#include "speaker.h"
#include "status_bar.h"
#include "typing.h"

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
  // The keyboard controller's codes for the Shift keys, which together open
  // the menu.
  kKeyLeftShift = 0xA2,
  kKeyRightShift = 0xA3,
  // Emulated cycles between looks at the screen, 20 a second.
  kBootPollCycles = Y_PLATFORM_CPU_CYCLES_PER_SECOND / 20,
  // How much emulated time a boot may take before it is taken to have gone
  // wrong. Reaching the prompt takes under six emulated seconds.
  kBootMaxEmulatedSeconds = 30,

  // How long the screen stays on after the boot is over or a key is pressed,
  // whichever was later, before it is blanked. A static image left showing for
  // hours can leave its mark on the panel. A key turns it on again.
  kScreenOnMs = 30 * 1000,
  // How often the status rows are composed again. Often enough that a drive's
  // light comes on while it is in use; core 1 is only handed them, and draws
  // only the rows that changed, when something has.
  kStatusUpdateMs = 100,
  // How long the window is that the machine's speed is judged over, for the
  // status rows' warning that it has fallen behind real time.
  kSlowWindowUs = 1000 * 1000,
  // The battery level below which the status rows say it is low, and the one
  // it has to come back above before they say so again.
  kBatteryLowPercent = 10,
  kBatteryRecoveredPercent = 15,
  // How long a menu-paused loop sleeps between looks at the keyboard.
  kPausedPollMs = 10,
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

  // The BIOS's keyboard flags, in the BIOS data area: its lock and insert
  // states.
  kBIOSKeyboardFlagsAddress = 0x417,

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
};

// What became of the request to raise the system clock.
typedef enum OverclockResult {
  kOverclockNotRequested = 0,
  kOverclockApplied,
  kOverclockPLLCannot,
  kOverclockSkippedAfterHang,
} OverclockResult;

// What came of the overclock, as the report gives it.
static const char* const kOverclockNames[] = {
    [kOverclockNotRequested] = "not_requested",
    [kOverclockApplied] = "applied",
    [kOverclockPLLCannot] = "pll_cannot_produce_clock",
    [kOverclockSkippedAfterHang] = "skipped_after_hang",
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

// The same, as a log gives them.
static const char* const kBootStageEvents[] = {
    [kBootStagePowerOn] = "power_on",
    [kBootStageAnsweringDate] = "date_prompt",
    [kBootStageAnsweringTime] = "time_prompt",
    [kBootStagePrompt] = "prompt",
    [kBootStageFailed] = "failed",
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
} RuntimeStatus;

static uint8_t g_guest_ram[kGuestRAMSize];
static uint8_t g_vram[Y_VIDEO_CGA_VRAM_SIZE];
static YPlatformState g_platform;
// The machines on the card, and the one started, if one was: without one the
// drives are as they were before machines, drive A `yax86/floppy_a.img` or
// the built-in floppy, and drive C `yax86/hdd.img`.
static MachineList g_machines;
static MachineConfig g_machine;
static bool g_has_machine;
static RuntimeStatus g_status;
static bool g_host_attached;
// Host time spent carrying out commands a host sent, which a boot that took
// them is that much slower for.
static uint64_t g_command_us;
// Status rows composed that core 1 has not been handed yet, because it was
// still drawing the last ones.
static DisplayStatus g_status_rows;
static bool g_status_rows_pending;

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
  // Host time spent with the machine paused for the menu, and when the pause
  // under way, if any, began.
  uint64_t paused_us;
  uint64_t pause_start_us;
} RunClock;

static RunClock g_clock;

#if YAX86_PICOCALC_SYS_CLK_KHZ
// Deliberately not zeroed at startup - see Overclock().
static uint32_t __uninitialized_ram(g_overclock_marker);
#endif

// ============================================================================
// Reporting
// ============================================================================

static bool BootIsOver(void) {
  return g_status.boot_stage == kBootStagePrompt ||
         g_status.boot_stage == kBootStageFailed;
}

// Host time since power on, and how much of it went to running the machine
// rather than waiting for real time to catch up with it.
static uint64_t ElapsedUs(void) { return time_us_64() - g_clock.start_us; }

static uint64_t PausedUs(void) {
  return g_clock.paused_us +
         (MenuIsOpen() ? time_us_64() - g_clock.pause_start_us : 0);
}

static uint64_t BusyUs(void) {
  return ElapsedUs() - g_clock.waited_us - PausedUs();
}

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
  static uint64_t window_idle_us;
  const uint64_t now_us = ElapsedUs();
  const uint64_t idle_now_us = g_clock.waited_us + PausedUs();
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
      buffer, size, "%lu MHz, LCD at %lu.%02lu MHz, keyboard %s, %lu keys, %uK",
      (unsigned long)(clock_get_hz(clk_sys) / (kHzPerKHz * kHzPerKHz)),
      (unsigned long)(g_status.lcd_spi_hz / (kHzPerKHz * kHzPerKHz)),
      (unsigned long)(g_status.lcd_spi_hz / (kHzPerKHz * 10) % 100), version,
      (unsigned long)SouthbridgeGetPollStats().keys,
      (unsigned)(kGuestRAMSize / 1024));
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

// How much of a buffer of size an snprintf() that returned written filled,
// leaving room for the terminator.
static size_t Filled(int written, size_t size) {
  if (written < 0) {
    return 0;
  }
  return (size_t)written < size ? (size_t)written : size - 1;
}

// One drive's part of the Disk line: what it has read and written from its
// image on the card, or what it has instead and why. Returns how much of
// buffer it filled.
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
  const DiskImage* const images[kMachineDrives] = {
      [kMachineDriveA] = FloppyGetImage(kFloppyDriveA),
      [kMachineDriveB] = FloppyGetImage(kFloppyDriveB),
      [kMachineDriveC] = HardDiskGetImage(kHardDiskC),
      [kMachineDriveD] = HardDiskGetImage(kHardDiskD),
  };
  size_t length = 0;
  uint64_t card_us = 0;
  bool any_open = false;
  for (uint8_t drive = 0; drive < kMachineDrives; ++drive) {
    const DiskImage* image = images[drive];
    const char label[] = {(char)('A' + drive), ':', '\0'};
    if (image->status == kDiskImageOpen) {
      any_open = true;
      length += FormatDrive(buffer + length, size - length, label, image, "");
    } else if (image->status != kDiskImageEjected) {
      // What drive A has instead, or why a configured image did not open.
      length += FormatDrive(
          buffer + length, size - length, label, image,
          drive == kMachineDriveA && !g_has_machine ? "built in" : "none");
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

// The menu's status page: what the runtime found, and what running the machine
// has cost so far.
static void DescribeStatus(MenuStatusLine lines[kMenuStatusLines]) {
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

// How fast the machine has run against real time over the last window, in
// percent, if it has fallen behind and dropped time in it; 0 if it has kept
// up.
static uint32_t SlowPercent(void) {
  static uint64_t window_start_us;
  static uint64_t window_start_cycles;
  static uint64_t window_start_dropped_us;
  static uint32_t percent;
  const uint64_t now_us = time_us_64();
  if (now_us - window_start_us < kSlowWindowUs) {
    return percent;
  }
  const uint64_t real_us = now_us - window_start_us;
  const uint64_t emulated_us = (g_clock.cycles - window_start_cycles) *
                               kMicrosecondsPerSecond /
                               Y_PLATFORM_CPU_CYCLES_PER_SECOND;
  const bool dropped = g_clock.dropped_us != window_start_dropped_us;
  percent = dropped && !MenuIsOpen() && window_start_us != 0
                ? (uint32_t)(emulated_us * 100 / real_us)
                : 0;
  window_start_us = now_us;
  window_start_cycles = g_clock.cycles;
  window_start_dropped_us = g_clock.dropped_us;
  return percent;
}

// Say once that the battery is low, until it has charged back up.
static void WarnOfLowBattery(SouthbridgeBattery battery) {
  static bool warned;
  if (!battery.known) {
    return;
  }
  if (!warned && !battery.charging && battery.percent < kBatteryLowPercent) {
    StatusBarPostMessage("Battery low");
    warned = true;
  } else if (battery.percent >= kBatteryRecoveredPercent) {
    warned = false;
  }
}

static void OfferStatusRows(const StatusBarState* state);

// Compose the status rows from the machine's state.
static void UpdateStatusRows(void) {
  StatusBarState state = {0};
  state.drives[kFloppyDriveA] = FloppyGetImage(kFloppyDriveA);
  state.drives[kFloppyDriveB] = FloppyGetImage(kFloppyDriveB);
  state.drives[kStatusBarDriveC] = HardDiskGetImage(kHardDiskC);
  state.drives[kStatusBarDriveD] = HardDiskGetImage(kHardDiskD);
  state.machine = g_has_machine ? g_machine.machine.name : NULL;
  state.running = true;
  state.keyboard_flags = g_guest_ram[kBIOSKeyboardFlagsAddress];
  state.guest_seconds = (uint32_t)(GuestClockMs() / kMillisecondsPerSecond);
  state.battery = SouthbridgeGetBattery();
  state.slow_percent = SlowPercent();
  state.hint = MenuIsOpen() ? MenuHint() : NULL;
  WarnOfLowBattery(state.battery);
  OfferStatusRows(&state);
}

// Compose the status rows from state, and hand them to core 1 if they
// changed, or are still waiting from the last time it was busy.
static void OfferStatusRows(const StatusBarState* state) {
  if (StatusBarCompose(state, time_us_64(), &g_status_rows)) {
    g_status_rows_pending = true;
  }
  if (!g_status_rows_pending) {
    return;
  }
  DisplayStatus* rows = DisplayBeginStatus();
  if (rows == NULL) {
    return;
  }
  *rows = g_status_rows;
  DisplayEndStatus();
  g_status_rows_pending = false;
}

// How far the boot has got, and what it took: the fields of a boot_stage
// log, and of the boot's group of a report.
#define BOOT_FIELDS()                                                   \
  Y_LOG_STRING("stage", kBootStageEvents[g_status.boot_stage]),         \
      Y_LOG_UINT("instructions", g_status.instructions),                \
      Y_LOG_UINT("cycles", g_status.boot_cycles),                       \
      Y_LOG_UINT(                                                       \
          "emulated_us", g_status.boot_cycles* kMicrosecondsPerSecond / \
                             Y_PLATFORM_CPU_CYCLES_PER_SECOND),         \
      Y_LOG_UINT("real_us", g_status.boot_elapsed_us),                  \
      Y_LOG_UINT("busy_us", g_status.boot_busy_us),                     \
      Y_LOG_UINT("speed_pm", BootSpeedPermille())

// A line of a reply to a report command: one group of its facts.
#define REPLY_GROUP(command, group, ...)                 \
  Y_LOG_REPLY(                                           \
      &g_runtime_logger, (command)->id, (command)->name, \
      Y_LOG_STRING("group", (group)), __VA_ARGS__)

// A drive's group of a report.
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

// The same facts as the status screen, as a group for each part of the
// runtime, in answer to a report command.
static void ReplyReport(const Command* command) {
  char version[16];
  FormatSouthbridgeVersion(version, sizeof(version));
  REPLY_GROUP(
      command, "system", Y_LOG_STRING("runtime", "yax86 picocalc"),
      Y_LOG_UINT("sys_clk_khz", clock_get_hz(clk_sys) / kHzPerKHz),
      Y_LOG_STRING("overclock", kOverclockNames[g_status.overclock]),
      Y_LOG_UINT("lcd_spi_hz", g_status.lcd_spi_hz),
      Y_LOG_UINT("sd_clock_hz", SdcardGetClockHz()),
      Y_LOG_STRING("keyboard_fw", version),
      Y_LOG_UINT("guest_ram_kb", kGuestRAMSize / 1024));
  REPLY_GROUP(command, "boot", BOOT_FIELDS());
  const DisplayStats display = DisplayGetStats();
  REPLY_GROUP(
      command, "display", Y_LOG_BOOL("draw_screen", YAX86_PICOCALC_DRAW_SCREEN),
      Y_LOG_UINT("frames_drawn", display.frames_drawn),
      Y_LOG_UINT("pixels_drawn", display.pixels_drawn),
      Y_LOG_UINT("render_us", display.render_us),
      Y_LOG_UINT("frames_late", display.frames_late),
      Y_LOG_UINT("copy_us", display.copy_us));
  ReplyDiskImage(command, "drive_a", FloppyGetImage(kFloppyDriveA));
  ReplyDiskImage(command, "drive_b", FloppyGetImage(kFloppyDriveB));
  ReplyDiskImage(command, "drive_c", HardDiskGetImage(kHardDiskC));
  ReplyDiskImage(command, "drive_d", HardDiskGetImage(kHardDiskD));
  REPLY_GROUP(
      command, "machine",
      Y_LOG_STRING("name", g_has_machine ? g_machine.machine.name : ""),
      Y_LOG_UINT("machines", g_machines.count));
  const SouthbridgePollStats keyboard = SouthbridgeGetPollStats();
  REPLY_GROUP(
      command, "keyboard", Y_LOG_UINT("reads", keyboard.reads),
      Y_LOG_UINT("keys", keyboard.keys),
      Y_LOG_UINT("stale_keys", keyboard.stale_keys),
      Y_LOG_UINT("aborts", keyboard.aborts),
      Y_LOG_UINT("timeouts", keyboard.timeouts));
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
          "emulated_us", g_clock.cycles * kMicrosecondsPerSecond /
                             Y_PLATFORM_CPU_CYCLES_PER_SECOND),
      Y_LOG_UINT("guest_clock_ms", GuestClockMs()),
      Y_LOG_UINT("waited_us", g_clock.waited_us),
      Y_LOG_UINT("dropped_us", g_clock.dropped_us),
      Y_LOG_UINT("command_us", g_command_us),
      Y_LOG_UINT("paused_us", PausedUs()),
      Y_LOG_BOOL("idle_skip", g_platform.is_dos_idle_skip_enabled));
  CommandReplyOk(command);
}

// Say the boot is over to a host that has attached since the last look, if
// it is: one that attached after the boot would otherwise not know. Cheap
// enough to call every batch: it reads a flag the USB stack keeps.
static void AnnounceToNewHost(void) {
  const bool attached = stdio_usb_connected();
  if (attached && !g_host_attached && BootIsOver()) {
    BOOT_INFO("done");
  }
  g_host_attached = attached;
}

// ============================================================================
// The machine
// ============================================================================

static void SetPCSpeakerFrequency(
    Y_UNUSED YPlatformState* platform, uint32_t frequency_hz) {
  SpeakerSetFrequency(frequency_hz);
}

static bool InitMachine(void) {
  YPlatformConfig* config = &g_platform.config;
  config->logger_config = &g_machine_log_config;
  config->physical_memory_size = kGuestRAMSize;
  config->physical_memory = g_guest_ram;
  config->vram = g_vram;
  config->video_adapter = Y_VIDEO_ADAPTER_CGA;
  config->floppy_drives = kFloppyDrives;
  config->video_text_font.glyphs = &kFont4x10[0][0];
  config->video_text_font.width = kFont4x10Width;
  config->video_text_font.height = kFont4x10Height;
  config->set_pc_speaker_frequency = SetPCSpeakerFrequency;
  if (!YPlatformInit(&g_platform)) {
    return false;
  }
  FloppyAttach(&g_platform.fdc);
  HardDiskAttach(&g_platform.hdc);
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
// reached. The machine has no clock to set the date and time from, and
// answering for the user keeps the boot the dos-boot workload, which is what
// the speed figures measure.
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
  BOOT_INFO("boot_stage", BOOT_FIELDS());
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

// Pass a key on to the guest, as the scancodes a PC/XT keyboard would send.
static void TypeKey(SouthbridgeKeyEvent event) {
  uint8_t scancodes[kKeymapMaxScancodes];
  const uint8_t count = KeymapTranslate(event, scancodes);
  for (uint8_t i = 0; i < count; ++i) {
    YKeyboardHandleKeyPress(&g_platform.keyboard, scancodes[i]);
  }
}

// ============================================================================
// Commands
// ============================================================================

enum {
  // CRT controller registers 12 to 15: where the screen starts in video
  // memory, and where the cursor is, each in character cells.
  kCRTCStartAddressHigh = 12,
  kCRTCStartAddressLow = 13,
  kCRTCCursorHigh = 14,
  kCRTCCursorLow = 15,
  kBytesPerTextCell = 2,
  kMaxTextColumns = 80,
};

// The guest's text screen: the mode, the cursor, then a line for each row of
// the screen with its characters as hex, as the guest's code page has them,
// and with attrs=true another with their attributes. Hex, because a row of
// box drawing characters escaped as text would not fit on a line.
static void RunScreen(const Command* command) {
  const YVideoState* video = &g_platform.video;
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
  // Where the cursor is on the screen, if it is on it at all.
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
      // The screen can start anywhere, and wraps at the end of video memory.
      const uint32_t cell = start + (uint32_t)row * mode->columns + column;
      const uint32_t offset = (cell * kBytesPerTextCell) % vram_size;
      characters[column] = g_vram[offset];
      attributes[column] = g_vram[(offset + 1) % vram_size];
    }
    // Two hex digits a byte, and room for the null snprintf() writes after
    // the last pair.
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

// Press, release or, by default, tap a key, by name. The key goes through the
// keymap as a key pressed on the PicoCalc does, a key event a batch, and the
// reply comes once it is queued. A host sending keys faster than they go is
// told the queue is full, and nothing is queued, so the key can be sent
// again.
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

static void RunCommand(const Command* command) {
  if (CommandIs(command, "ping")) {
    CommandReplyOk(command);
  } else if (CommandIs(command, "report")) {
    ReplyReport(command);
  } else if (CommandIs(command, "screen")) {
    RunScreen(command);
  } else if (CommandIs(command, "screenshot")) {
    // Core 1 owns the panel, so it reads it back and sends the reply.
    if (!DisplaySendScreenshot(command->id)) {
      CommandReplyError(command, "busy");
    }
  } else if (CommandIs(command, "key")) {
    RunKey(command);
  } else {
    CommandReplyError(command, "unknown_command");
  }
}

// ============================================================================
// The menu
// ============================================================================

// Mark the guest's screen to be drawn in full, and offer it to core 1 now
// rather than at the next frame, which while the machine is paused never
// comes. Returns whether core 1 took it. If it did not, the mark stays in the
// live state, and the next frame offered draws it all.
static bool RedrawGuest(void) {
  g_platform.video.dirty_state.status = Y_VIDEO_FULL_REDRAW;
  return !YAX86_PICOCALC_DRAW_SCREEN || DisplayOfferFrame(&g_platform.video);
}

// Restart the board, which restarts the machine from power on, the way the
// power button would - but without losing the keyboard controller's power, or
// the guest's disks, whose sectors are already on the card.
static void ResetMachine(void) {
  SYSTEM_INFO("reset");
  watchdog_reboot(0, 0, 0);
  for (;;) {
    tight_loop_contents();
  }
}

static const char* const kDriveNames[kFloppyDrives] = {
    [kFloppyDriveA] = "a",
    [kFloppyDriveB] = "b",
};

static char DriveLetter(FloppyDrive drive) { return (char)('A' + drive); }

// Take the disk out of a floppy drive, and say so.
static void EjectDrive(FloppyDrive drive) {
  FloppyEject(&g_platform.fdc, drive);
  char message[kTextColumns + 1];
  snprintf(
      message, sizeof(message), "Ejected the disk from drive %c",
      DriveLetter(drive));
  StatusBarPostMessage(message);
  DISK_INFO("ejected", Y_LOG_STRING("drive", kDriveNames[drive]));
}

// Put an image in a floppy drive, and say whether it went in. An image
// already in the other drive stays there: two drives writing one image would
// each overwrite the other's sectors.
static void InsertDrive(FloppyDrive drive, const char* path) {
  char name[kDiskImageNameLength + 1];
  DiskImageName(path, name);
  char message[kTextColumns + 1];
  const FloppyDrive holding = FloppyDriveHolding(path);
  if (holding != kFloppyDrives) {
    snprintf(
        message, sizeof(message), "%s is already in drive %c", name,
        DriveLetter(holding));
    StatusBarPostMessage(message);
    DISK_WARN(
        "insert_failed", Y_LOG_STRING("drive", kDriveNames[drive]),
        Y_LOG_STRING("path", path), Y_LOG_STRING("error", "in_other_drive"));
    return;
  }
  const DiskImageStatus status =
      FloppyInsertImage(&g_platform.fdc, drive, path);
  if (status == kDiskImageOpen) {
    snprintf(
        message, sizeof(message), "Inserted %s in drive %c", name,
        DriveLetter(drive));
    DISK_INFO(
        "inserted", Y_LOG_STRING("drive", kDriveNames[drive]),
        Y_LOG_STRING("path", path));
  } else {
    snprintf(
        message, sizeof(message), "Could not insert %s: %s", name,
        DiskImageDescribeStatus(status));
    DISK_WARN(
        "insert_failed", Y_LOG_STRING("drive", kDriveNames[drive]),
        Y_LOG_STRING("path", path),
        Y_LOG_STRING("error", DiskImageDescribeStatus(status)));
  }
  StatusBarPostMessage(message);
}

// Real time goes on while the machine is paused and emulated time does not,
// so on resuming the two are made to agree again from now, rather than the
// machine being run flat out to catch up a pause it was meant to take.
static void ResumeFromMenu(void) {
  const uint64_t now_us = time_us_64();
  g_clock.paused_us += now_us - g_clock.pause_start_us;
  g_clock.anchor_us = now_us;
  g_clock.anchor_cycles = g_clock.cycles;
  RedrawGuest();
}

// A key goes to the menu, which looks for the hotkey whether it is open or
// not, and on to the guest unless the menu has the keyboard.
static void HandleKey(SouthbridgeKeyEvent key, bool running) {
  const bool menu_was_open = MenuIsOpen();
  const MenuAction action = MenuHandleKey(key);
  if (!menu_was_open && running) {
    TypeKey(key);
  }
  if (action == kMenuActionOpened) {
    // The guest has seen both Shift keys go down, and while the menu has the
    // keyboard it would not see them come up, so it is told now.
    if (running) {
      TypeKey((SouthbridgeKeyEvent){kSouthbridgeKeyReleased, kKeyLeftShift});
      TypeKey((SouthbridgeKeyEvent){kSouthbridgeKeyReleased, kKeyRightShift});
    }
    g_clock.pause_start_us = time_us_64();
    SYSTEM_INFO("menu_opened");
  } else if (action == kMenuActionClosed) {
    ResumeFromMenu();
    SYSTEM_INFO("menu_closed");
  }
}

// ============================================================================
// The main loop
// ============================================================================

// Run the machine, paced to real time, for as long as the board has power.
// The boot is watched on the way, and DOS's questions answered. While the
// menu is open, the machine is paused and the loop only looks after the menu,
// the keyboard and commands.
static void RunMachine(bool machine_ready) {
  const uint64_t max_boot_cycles =
      (uint64_t)kBootMaxEmulatedSeconds * Y_PLATFORM_CPU_CYCLES_PER_SECOND;
  uint64_t next_poll_cycles = kBootPollCycles;
  uint64_t next_render_cycles = kCyclesPerRender;
  bool running = machine_ready;
  bool screen_on = true;
  absolute_time_t blank_at = at_the_end_of_time;
  absolute_time_t next_status_at = get_absolute_time();
  g_clock.last_ticks = g_platform.ticks;
  g_clock.start_us = time_us_64();
  g_clock.anchor_us = g_clock.start_us;

  for (;;) {
    AnnounceToNewHost();

    if (running && !MenuIsOpen() && RunBatch() == Y_PLATFORM_HUNG) {
      // The CPU halted with nothing to wake it.
      SYSTEM_ERROR("machine_stopped");
      running = false;
    }

    Command command;
    if (CommandPoll(&command)) {
      const uint64_t start_us = time_us_64();
      RunCommand(&command);
      g_command_us += time_us_64() - start_us;
    }
    // Between batches, so the keyboard is never written while the machine
    // runs, and a key a host typed only in a batch the keyboard had none, so
    // that the two never land in one. A key also keeps the screen on, and
    // turns it back on if it was blanked: its contents are kept, and what
    // changed meanwhile is drawn with the next frame.
    SouthbridgeKeyEvent key;
    if (SouthbridgePollKey(&key) || TypingNext(&key)) {
      HandleKey(key, running);
      if (!screen_on) {
        DisplayTurnOn();
        screen_on = true;
      }
      if (BootIsOver()) {
        blank_at = make_timeout_time_ms(kScreenOnMs);
      }
    }

    if (screen_on && !MenuIsOpen() && g_clock.cycles >= next_render_cycles) {
      do {
        next_render_cycles += kCyclesPerRender;
      } while (next_render_cycles <= g_clock.cycles);
      if (YAX86_PICOCALC_DRAW_SCREEN) {
        DisplayOfferFrame(&g_platform.video);
      }
    }

    if (!BootIsOver() && !MenuIsOpen()) {
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
        SetBootStage(stage);
      }
      if (BootIsOver()) {
        // DOS polls for a key at the prompt rather than halting, and the
        // idle skip is what keeps that from costing a real time loop's worth
        // of emulation. Not before now: DOS idles at the date and time
        // prompts too, and skipping there would move the dos-boot invariant.
        YPlatformSetDOSIdleSkip(&g_platform, true);
        blank_at = make_timeout_time_ms(kScreenOnMs);
        BOOT_INFO("done");
      }
    }

    if (screen_on && time_reached(next_status_at)) {
      next_status_at = make_timeout_time_ms(kStatusUpdateMs);
      UpdateStatusRows();
    }
    if (screen_on) {
      MenuDraw(time_us_64());
    }
    if (screen_on && time_reached(blank_at)) {
      DisplayTurnOff();
      screen_on = false;
    }

    if (MenuIsOpen()) {
      sleep_ms(kPausedPollMs);
    } else if (running) {
      PaceToRealTime();
    } else {
      sleep_ms(kStoppedPollMs);
    }
  }
}

// ============================================================================
// Choosing a machine
// ============================================================================

// The status rows while the list of machines shows: the list's keys, or why
// a machine did not start, and the battery. There are no drives yet.
static void UpdateChoosingRows(void) {
  StatusBarState state = {0};
  state.battery = SouthbridgeGetBattery();
  state.hint = MenuHint();
  state.problem = MenuProblem();
  OfferStatusRows(&state);
}

// Read the configuration of the machine at index in the list, to start it.
// Returns false if it could not be read, with why shown on the list.
static bool TakeMachine(uint8_t index) {
  const MachineEntry* machine = &g_machines.machines[index];
  char problem[kMachineProblemLength + 1];
  if (MachineConfigRead(machine->path, &g_machine, problem)) {
    g_has_machine = true;
    SYSTEM_INFO("machine_chosen", Y_LOG_STRING("path", machine->path));
    return true;
  }
  char message[kTextColumns + 1];
  snprintf(message, sizeof(message), "%s: %s", machine->name, problem);
  MenuSetProblem(message);
  SYSTEM_WARN(
      "machine_config_problem", Y_LOG_STRING("path", machine->path),
      Y_LOG_STRING("problem", problem));
  return false;
}

// The card's settings, and the index of the default machine in the list.
// Returns false, with the problem shown on the list, if the settings had one.
static bool ReadSettings(MachineSettings* settings, uint8_t* highlighted) {
  char problem[kMachineProblemLength + 1];
  bool read = MachineSettingsRead(settings, problem);
  *highlighted = 0;
  if (read && settings->boot_default[0] != '\0') {
    // A default outside the machines folder is listed too.
    const uint8_t index = MachineListFind(&g_machines, settings->boot_default);
    if (index < kMachineListMax) {
      *highlighted = index;
    } else {
      snprintf(
          problem, sizeof(problem), "too many machines to add boot_default");
      read = false;
    }
  }
  if (!read) {
    char message[kTextColumns + 1];
    snprintf(message, sizeof(message), "YAX86.CFG: %s", problem);
    MenuSetProblem(message);
    SYSTEM_WARN("settings_problem", Y_LOG_STRING("problem", problem));
  }
  return read;
}

// With machines on the card, show the list of them until one is chosen whose
// configuration reads, or until the default starts by itself, the countdown
// running until a key is pressed. Settings that ask for no wait start the
// default at once, without the list, unless there is a problem to show.
// Without any machines, the machine starts with the drives it had before
// machines.
static void ChooseMachine(void) {
  MachineListRead(&g_machines);
  MachineSettings settings;
  uint8_t highlighted = 0;
  const bool settings_read = ReadSettings(&settings, &highlighted);
  if (g_machines.count == 0) {
    return;
  }
  if (settings_read && settings.boot_delay == 0 && TakeMachine(highlighted)) {
    return;
  }
  MenuChooseMachines(&g_machines, highlighted);
  const absolute_time_t start_at =
      make_timeout_time_ms(settings.boot_delay * kMillisecondsPerSecond);
  bool counting = settings.boot_delay > 0;
  absolute_time_t next_status_at = get_absolute_time();
  for (;;) {
    Command command;
    if (CommandPoll(&command)) {
      RunCommand(&command);
    }
    bool chosen = false;
    SouthbridgeKeyEvent key;
    if (SouthbridgePollKey(&key) || TypingNext(&key)) {
      counting = false;
      MenuSetCountdown(0);
      chosen = MenuHandleKey(key) == kMenuActionMachineChosen;
    }
    if (counting) {
      const int64_t left_us =
          absolute_time_diff_us(get_absolute_time(), start_at);
      if (left_us <= 0) {
        counting = false;
        MenuSetCountdown(0);
        chosen = true;
      } else {
        MenuSetCountdown((uint8_t)((left_us + kMicrosecondsPerSecond - 1) /
                                   kMicrosecondsPerSecond));
      }
    }
    if (chosen && TakeMachine(MenuChosenMachine())) {
      MenuClose();
      return;
    }
    if (time_reached(next_status_at)) {
      next_status_at = make_timeout_time_ms(kStatusUpdateMs);
      UpdateChoosingRows();
    }
    MenuDraw(time_us_64());
    sleep_ms(kPausedPollMs);
  }
}

// Open the drives' images: the machine's, or without one, the ones before
// machines. An image a machine names that does not open leaves its drive
// empty, which a message says - as does one named for two drives, which goes
// in the first: two drives writing one image would each overwrite the
// other's sectors.
static void OpenDrives(void) {
  const char* paths[kMachineDrives] = {
      [kMachineDriveA] = kFloppyDefaultPath,
      [kMachineDriveC] = kHardDiskDefaultPath,
  };
  char message[kTextColumns + 1];
  if (g_has_machine) {
    snprintf(message, sizeof(message), "Started %s", g_machine.machine.name);
    StatusBarPostMessage(message);
    for (uint8_t drive = 0; drive < kMachineDrives; ++drive) {
      paths[drive] = g_machine.drives[drive];
      for (uint8_t earlier = 0; earlier < drive; ++earlier) {
        if (paths[drive][0] != '\0' &&
            FileListSamePath(paths[drive], paths[earlier])) {
          char name[kDiskImageNameLength + 1];
          DiskImageName(paths[drive], name);
          snprintf(
              message, sizeof(message), "Drive %c: %s is already in drive %c",
              (char)('A' + drive), name, (char)('A' + earlier));
          StatusBarPostMessage(message);
          paths[drive] = "";
        }
      }
    }
  }
  DiskImageStatus opened[kMachineDrives];
  FloppyOpen(&paths[kMachineDriveA], !g_has_machine, &opened[kMachineDriveA]);
  HardDiskOpen(&paths[kMachineDriveC]);
  opened[kMachineDriveC] = HardDiskGetImage(kHardDiskC)->status;
  opened[kMachineDriveD] = HardDiskGetImage(kHardDiskD)->status;
  if (!g_has_machine) {
    return;
  }
  for (uint8_t drive = 0; drive < kMachineDrives; ++drive) {
    if (paths[drive][0] == '\0' || opened[drive] == kDiskImageOpen) {
      continue;
    }
    char name[kDiskImageNameLength + 1];
    DiskImageName(paths[drive], name);
    snprintf(
        message, sizeof(message), "Drive %c: could not open %s: %s",
        (char)('A' + drive), name, DiskImageDescribeStatus(opened[drive]));
    StatusBarPostMessage(message);
    DISK_WARN(
        "open_failed", Y_LOG_UINT("drive", drive),
        Y_LOG_STRING("path", paths[drive]),
        Y_LOG_STRING("error", DiskImageDescribeStatus(opened[drive])));
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
  // divider when it is: the LCD's, on core 1, from the system clock, and the
  // keyboard controller's I2C from the peripheral clock, which the overclock
  // moves to the 48MHz USB PLL.
  g_status.overclock = Overclock();
#endif

  RuntimeLogInit();

  g_status.lcd_spi_hz = DisplayStart();
  SouthbridgeInit();
  SpeakerInit();
  g_status.southbridge_answered =
      SouthbridgeReadVersion(&g_status.southbridge_version);
  const MenuConfig menu_config = {
      .describe_status = DescribeStatus,
      .reset = ResetMachine,
      .redraw_guest = RedrawGuest,
      .drive = FloppyGetImage,
      .eject_drive = EjectDrive,
      .insert_drive = InsertDrive,
  };
  MenuInit(&menu_config);

  ChooseMachine();
  OpenDrives();
  RunMachine(InitMachine());
  return 0;
}
