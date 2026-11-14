#include "emulator.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "core/platform.h"
#include "core/video.h"
#include "debug_command.h"
#include "display.h"
#include "emulator_menu_screen.h"
#include "emulator_session.h"
#include "emulator_status_bar.h"
#include "file_list.h"
#include "floppy.h"
#include "harddisk.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h"
#include "keymap.h"
#include "launcher.h"
#include "pager.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include "psram.h"
#include "runtime_log.h"
#include "screen.h"
#include "sdcard.h"
#include "southbridge.h"
#include "speaker.h"
#include "stack_usage.h"
#include "status_bar.h"
#include "status_screen.h"
#include "typing.h"
#include "usb_storage.h"
#include "vm_config.h"
#include "vm_list.h"
#include "yax86_pico.h"

// Whether the guest's screen is drawn. Building with it off gives an image
// identical but for that, which is what measures what drawing costs.
#ifndef YAX86_PICOCALC_DRAW_SCREEN
#define YAX86_PICOCALC_DRAW_SCREEN 1
#endif

enum {
  // The SRAM guest memory has: conventional memory itself without the
  // PSRAM, and the frames it is paged through with it - see yax86_pico.h.
  kGuestRAMSize = kPicoGuestSRAMSize,

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

  // How long the screen stays on after the boot is over or a key is pressed,
  // whichever was later, before it is blanked. A static image left showing for
  // hours can leave its mark on the panel. A key turns it on again.
  kScreenOnMs = 30 * 1000,
  // How often the status rows are composed again. Often enough that a drive's
  // light comes on while it is in use; core 1 is only handed them, and draws
  // only the rows that changed, when something has.
  kStatusUpdateMs = 100,
  // The battery level below which the status rows say it is low, and the one
  // it has to come back above before they say so again.
  kBatteryLowPercent = 10,
  kBatteryRecoveredPercent = 15,
  // How long a menu-paused loop sleeps between looks at the keyboard.
  kPausedPollMs = 10,
  // How often the loop looks for a newly attached host once the VM has
  // stopped.
  kStoppedPollMs = 100,

  // How far behind real time the VM may fall and still make it up by
  // running flat out. Past this the lag is dropped instead, so that a long
  // stall is not followed by a burst of emulation at full speed.
  kMaxLagMs = 100,

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

// How far the boot has got, for the status page and for the report a host
// is sent.
typedef struct RuntimeStatus {
  BootStage boot_stage;
  // Emulated and host time at the point boot_stage was last changed.
  uint64_t boot_cycles;
  uint64_t boot_elapsed_us;
  // Host time spent running the VM up to then, which is the elapsed time
  // less the time spent waiting for real time to catch up. It is what the
  // boot would have taken unpaced.
  uint64_t boot_busy_us;
  uint64_t instructions;
} RuntimeStatus;

// main.c's: what initializing the system found; guest RAM, lent to the
// launcher until the VM starts; and the card's VMs and settings.
static const InitStatus* g_init_status;
static uint8_t* g_guest_ram;
static const VMList* g_vms;
static const Settings* g_settings;
static uint8_t g_vram[Y_VIDEO_CGA_VRAM_SIZE];
static YPlatformState g_platform;
// The VM started, main.c's, if one was: without one drive A holds the
// built-in floppy and no other drive has a disk.
static VMConfig* g_vm;
static bool g_has_vm;
// The emulator session the in-VM menu works on: g_vm and the machine's
// floppy disk controller.
static EmulatorSession g_session;
static RuntimeStatus g_status;
static bool g_host_attached;
// Host time spent carrying out commands a host sent, which a boot that took
// them is that much slower for.
static uint64_t g_command_us;

// How far the VM has got in emulated time, and what pacing it against
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
  // Host time spent waiting for real time to catch up with the VM.
  uint64_t waited_us;
  // Real time the VM fell behind by and did not make up.
  uint64_t dropped_us;
  // Host time spent with the VM paused for the menu, and when the pause
  // under way, if any, began.
  uint64_t paused_us;
  uint64_t pause_start_us;
} RunClock;

static RunClock g_clock;

// Whether guest RAM is paged, and whether the VM is up, before which there is
// no guest RAM to read.
static bool g_paging;
static bool g_vm_up;

// ============================================================================
// Reporting
// ============================================================================

static bool BootIsOver(void) {
  return g_status.boot_stage == kBootStagePrompt ||
         g_status.boot_stage == kBootStageFailed;
}

// Host time since power on, and how much of it went to running the VM
// rather than waiting for real time to catch up with it.
static uint64_t ElapsedUs(void) { return time_us_64() - g_clock.start_us; }

// Host time the VM has been paused for, while the menu showed.
static uint64_t PausedUs(void) {
  return g_clock.paused_us +
         (ScreenIsOpen() ? time_us_64() - g_clock.pause_start_us : 0);
}

// How much guest RAM the VM has.
static uint32_t GuestRAMSize(void) {
  return g_paging ? Y_PLATFORM_MAX_CONVENTIONAL_MEMORY_SIZE : kGuestRAMSize;
}

// A byte of guest RAM, read through the memory map since it may be paged. 0
// before the VM is up, which is what guest RAM starts as.
static uint8_t GuestRAMByte(uint32_t address) {
  return g_vm_up ? YPlatformReadMemoryByte(&g_platform, address) : 0;
}

static uint64_t BusyUs(void) {
  return ElapsedUs() - g_clock.waited_us - PausedUs();
}

// The time of day the guest's BIOS keeps, which is what DOS reports, in
// milliseconds. It counts from POST, since the date and time questions are
// answered with Enter.
static uint64_t GuestClockMs(void) {
  uint64_t ticks = 0;
  for (uint32_t i = 0; i < 4; ++i) {
    ticks |= (uint64_t)GuestRAMByte(kBIOSTickCountAddress + i) << (8 * i);
  }
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
  InitFormatKeyboardVersion(g_init_status, version, sizeof(version));
  snprintf(
      buffer, size, "%lu MHz, LCD at %lu.%02lu MHz, keyboard %s, %lu keys, %uK",
      (unsigned long)(clock_get_hz(clk_sys) / (kHzPerKHz * kHzPerKHz)),
      (unsigned long)(g_init_status->lcd_spi_hz / (kHzPerKHz * kHzPerKHz)),
      (unsigned long)(g_init_status->lcd_spi_hz / (kHzPerKHz * 10) % 100),
      version, (unsigned long)SouthbridgeGetPollStats().keys,
      (unsigned)(GuestRAMSize() / 1024));
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
          drive == kVMDriveFD1 && !g_has_vm ? "built in" : "empty");
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

// Fill in the menu's status page: what the runtime found, and what running
// the VM has cost so far.
static void DescribeStatus(StatusScreenLine lines[kStatusScreenLines]) {
  static const char* const kLabels[kStatusScreenLines] = {
      "System", "Boot", "Speed", "Clock", "Display", "Disk",
  };
  for (uint8_t i = 0; i < kStatusScreenLines; ++i) {
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

// Say once that the battery is low, until it has charged back up.
static void WarnOfLowBattery(SouthbridgeBattery battery) {
  static bool warned;
  if (!battery.known) {
    return;
  }
  if (!warned && !battery.charging && battery.percent < kBatteryLowPercent) {
    EmulatorStatusBarPostMessage("Battery low");
    warned = true;
  } else if (battery.percent >= kBatteryRecoveredPercent) {
    warned = false;
  }
}

// Gather what the status rows show from the VM, and update them.
static void UpdateStatusRows(void) {
  EmulatorStatusBarInputs inputs = {0};
  inputs.drives[kFloppyDriveA] = FloppyGetImage(kFloppyDriveA);
  inputs.drives[kFloppyDriveB] = EmulatorSessionFloppyDrives(&g_session) > 1
                                     ? FloppyGetImage(kFloppyDriveB)
                                     : NULL;
  inputs.drives[kVMDriveHD1] = HardDiskGetImage(kHardDisk1);
  inputs.drives[kVMDriveHD2] = HardDiskGetImage(kHardDisk2);
  inputs.vm = g_has_vm ? g_vm->entry.name : NULL;
  inputs.keyboard_flags = GuestRAMByte(kBIOSKeyboardFlagsAddress);
  inputs.battery = SouthbridgeGetBattery();
  inputs.hint = ScreenIsOpen() ? ScreenHint() : NULL;
  WarnOfLowBattery(inputs.battery);
  EmulatorStatusBarUpdate(&inputs);
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

// A drive's group of a report.
static void ReplyDiskImage(
    const DebugCommand* command, const char* group, const DiskImage* image) {
  const DiskImageStats* stats = &image->stats;
  DEBUG_COMMAND_REPLY_GROUP(
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

// Emulator mode's groups of a report, after the system's: the VM's paging and
// boot, the display, the VM's drives and name, the menu, and the pacing. In
// parts, none inlined into another: each group's fields take stack until the
// function they are in returns, and the report whole would be the deepest call
// on core 0's stack. See StackUsagePaint().
//
// The VM's memory and boot, and the display.
static __noinline void ReportRunning(const DebugCommand* command) {
  const PagerStats pager = PagerGetStats();
  DEBUG_COMMAND_REPLY_GROUP(
      command, "paging", Y_LOG_UINT("guest_ram_kb", GuestRAMSize() / 1024),
      Y_LOG_BOOL("on", g_paging),
      Y_LOG_BOOL("off_after_fault", g_init_status->after_fault),
      Y_LOG_UINT("frames", pager.frames),
      Y_LOG_UINT("page_bytes", kPagerPageSize),
      Y_LOG_UINT("page_ins", pager.page_ins),
      Y_LOG_UINT("zero_fills", pager.zero_fills),
      Y_LOG_UINT("write_backs", pager.write_backs),
      Y_LOG_UINT("soft_faults", pager.soft_faults),
      Y_LOG_UINT("errors", pager.errors),
      Y_LOG_UINT("fault_us", pager.fault_us));
  DEBUG_COMMAND_REPLY_GROUP(command, "boot", BOOT_FIELDS());
  const DisplayStats display = DisplayGetStats();
  DEBUG_COMMAND_REPLY_GROUP(
      command, "display", Y_LOG_BOOL("draw_screen", YAX86_PICOCALC_DRAW_SCREEN),
      Y_LOG_STRING("font", Font4x10Name(g_settings->font)),
      Y_LOG_UINT("frames_drawn", display.frames_drawn),
      Y_LOG_UINT("pixels_drawn", display.pixels_drawn),
      Y_LOG_UINT("render_us", display.render_us),
      Y_LOG_UINT("frames_late", display.frames_late),
      Y_LOG_UINT("copy_us", display.copy_us));
}

// The VM's drives, the VM, the menu and the pacing.
static __noinline void ReportDrives(const DebugCommand* command) {
  ReplyDiskImage(command, "fd1", FloppyGetImage(kFloppyDriveA));
  ReplyDiskImage(command, "fd2", FloppyGetImage(kFloppyDriveB));
  ReplyDiskImage(command, "hd1", HardDiskGetImage(kHardDisk1));
  ReplyDiskImage(command, "hd2", HardDiskGetImage(kHardDisk2));
  DEBUG_COMMAND_REPLY_GROUP(
      command, "vm", Y_LOG_STRING("name", g_has_vm ? g_vm->entry.name : ""),
      Y_LOG_UINT("vms", g_vms->count));
  DEBUG_COMMAND_REPLY_GROUP(
      command, "menu", Y_LOG_BOOL("open", ScreenIsOpen()),
      Y_LOG_STRING("page", ScreenIsOpen() ? ScreenPageName() : ""));
  DEBUG_COMMAND_REPLY_GROUP(
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
}

static void Report(const DebugCommand* command) {
  DebugCommandReportSystem(command);
  ReportRunning(command);
  ReportDrives(command);
  DebugCommandReplyOk(command);
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
// The VM
// ============================================================================

static void SetPCSpeakerFrequency(
    Y_UNUSED YPlatformState* platform, uint32_t frequency_hz) {
  SpeakerSetFrequency(frequency_hz);
}

// Page all of guest RAM through the PSRAM if it came up, and the last start
// did not end in a hard fault: paging is the newest thing a fault could come
// from, and without it the VM has the SRAM alone - see yax86_pico.h.
static void ConfigureGuestRAM(YPlatformConfig* config) {
  g_paging = PsramGetStats().state == kPsramOk && !g_init_status->after_fault;
  if (!g_paging) {
    config->direct_memory_size = kGuestRAMSize;
    config->direct_memory = g_guest_ram;
    config->paged_memory = NULL;
    return;
  }
  config->direct_memory_size = 0;
  config->direct_memory = NULL;
  config->paged_memory = PagerInit(
      g_guest_ram, kGuestRAMSize, Y_PLATFORM_MAX_CONVENTIONAL_MEMORY_SIZE);
}

static bool InitVM(void) {
  YPlatformConfig* config = &g_platform.config;
  config->logger_config = &g_vm_log_config;
  ConfigureGuestRAM(config);
  config->vram = g_vram;
  config->video_adapter = Y_VIDEO_ADAPTER_CGA;
  config->floppy_drives = EmulatorSessionFloppyDrives(&g_session);
  config->video_text_font.glyphs = &(*Font4x10Glyphs(g_settings->font))[0][0];
  config->video_text_font.width = kFont4x10Width;
  config->video_text_font.height = kFont4x10Height;
  config->set_pc_speaker_frequency = SetPCSpeakerFrequency;
  if (!YPlatformInit(&g_platform)) {
    return false;
  }
  FloppyAttach(&g_platform.fdc);
  HardDiskAttach(&g_platform.hdc);
  g_vm_up = true;
  if (g_init_status->after_fault) {
    EmulatorStatusBarPostMessage("Paging off: the last start crashed");
  }
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
// reached. The VM has no clock to set the date and time from, and
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

// Run a batch of emulated cycles, and return how the VM stands after it.
static YPlatformRunStatus RunBatch(void) {
  // An invalid opcode is not a stop: the 8088 has no invalid opcode
  // exception, so a real one would carry on.
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

// Wait until real time has caught up with the emulated time the VM has
// reached, which is what keeps it from running faster than a real PC/XT.
//
// Only ever waits between batches, never changes them, so pacing cannot move
// anything the guest sees: the same batches run whether or not there is time
// to spare.
static void PaceToRealTime(void) {
  // The anchor moves a whole emulated second at a time, which keeps the
  // product below from overflowing however long the VM runs, and loses
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
static void RunScreen(const DebugCommand* command) {
  const YVideoState* video = &g_platform.video;
  const YVideoModeMetadata* mode = YVideoGetModeMetadata(video);
  if (mode->type != Y_VIDEO_MODE_TEXT || mode->columns > kMaxTextColumns) {
    DebugCommandReplyError(command, "not_text_mode");
    return;
  }
  const char* attrs = DebugCommandArgument(command, "attrs");
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
  DebugCommandReplyOk(command);
}

// Answer a command: report and screen here, as they depend on the mode, and
// any other with DebugCommandRunCommon().
static void RunCommand(const DebugCommand* command) {
  if (DebugCommandIs(command, "report")) {
    Report(command);
  } else if (DebugCommandIs(command, "screen")) {
    RunScreen(command);
  } else {
    DebugCommandRunCommon(command);
  }
}

// Answer the command the host has sent, if a whole one is in, adding the
// time it took to *run_us if given. Kept out of line, so that the command is
// off core 0's stack while the loop runs.
static __noinline void PollCommand(uint64_t* run_us) {
  DebugCommand command;
  if (!DebugCommandPoll(&command)) {
    return;
  }
  const uint64_t start_us = time_us_64();
  RunCommand(&command);
  if (run_us != NULL) {
    *run_us += time_us_64() - start_us;
  }
}

// ============================================================================
// The menu
// ============================================================================

// The size of a box over the guest's screen.
typedef struct BoxShown {
  // The box's size, in columns and rows.
  uint8_t columns;
  uint8_t rows;
  // Whether a box is showing: false while the guest's screen is drawn in
  // full, as it is when the menu opens.
  bool drawn;
} BoxShown;

// Which Shift keys are held, from the keyboard controller's events.
static bool g_left_shift_held;
static bool g_right_shift_held;
// The status page's lines, which the menu's status screen draws, and what
// the menu decided, which it writes as it closes.
static StatusScreenLine g_status_lines[kStatusScreenLines];
static EmulatorMenuExit g_exit;
// The box last sent to core 1, and whether one drawn has still to be sent.
static BoxShown g_box_shown;
static bool g_box_pending;

// Mark the guest's screen to be drawn in full, and send it to core 1 now
// rather than at the next frame, which never comes while the VM is paused.
// Returns whether core 1 took it. If it did not, the mark stays in the live
// state, and the next frame sent draws it all.
static bool RedrawGuest(void) {
  g_platform.video.dirty_state.status = Y_VIDEO_FULL_REDRAW;
  return !YAX86_PICOCALC_DRAW_SCREEN || DisplaySendFrame(&g_platform.video);
}

// The VM again from power on, with the disks it has now, without the list.
// Without a VM there is no card, and a plain reboot boots the built-in floppy
// again.
static void ResetVM(void) {
  SYSTEM_INFO("reset", Y_LOG_STRING("path", g_vm->entry.path));
  if (!g_has_vm) {
    watchdog_reboot(0, 0, 0);
    for (;;) {
      tight_loop_contents();
    }
  }
  LauncherLeaveStartNote(kStartIntentRestart, g_vm->entry.name);
}

// Back to the launcher menu, which waits for a key with the VM selected.
static void PowerOffVM(void) {
  SYSTEM_INFO("power_off", Y_LOG_STRING("path", g_vm->entry.path));
  LauncherLeaveStartNote(kStartIntentMenu, g_vm->entry.name);
}

// Real time goes on while the VM is paused and emulated time does not,
// so on resuming the two are made to agree again from now, rather than the
// VM being run flat out to catch up a pause it was meant to take.
static void ResumeFromMenu(void) {
  const uint64_t now_us = time_us_64();
  g_clock.paused_us += now_us - g_clock.pause_start_us;
  g_clock.anchor_us = now_us;
  g_clock.anchor_cycles = g_clock.cycles;
  RedrawGuest();
}

// Track the Shift keys, and return whether this event completes the hotkey
// that opens and closes the menu: both held, the second just pressed. Nothing
// in DOS holds both, and typing never does.
static bool TrackHotkey(SouthbridgeKeyEvent event) {
  bool* held = NULL;
  if (event.key == kKeyCodeLeftShift) {
    held = &g_left_shift_held;
  } else if (event.key == kKeyCodeRightShift) {
    held = &g_right_shift_held;
  } else {
    return false;
  }
  const bool was_held = *held;
  *held = event.state != kSouthbridgeKeyReleased;
  return *held && !was_held && g_left_shift_held && g_right_shift_held;
}

// Open the menu over the VM, which pauses it. The guest has seen both Shift
// keys go down, and while the menu has the keyboard it would not see them
// come up, so it is told now.
static void OpenMenu(bool running) {
  if (running) {
    TypeKey((SouthbridgeKeyEvent){kSouthbridgeKeyReleased, kKeyCodeLeftShift});
    TypeKey((SouthbridgeKeyEvent){kSouthbridgeKeyReleased, kKeyCodeRightShift});
  }
  g_clock.pause_start_us = time_us_64();
  // The guest's screen is under the first box in full, so nothing of a box
  // is showing yet.
  g_box_shown.drawn = false;
  g_box_pending = false;
  ScreenShow(EmulatorMenuScreenOpen(&g_session, g_status_lines, &g_exit));
  SYSTEM_INFO("menu_opened");
}

// The menu has closed: carry out what it decided, and otherwise resume the
// VM.
static void CloseMenu(void) {
  switch (g_exit) {
    case kEmulatorMenuExitReset:
      ResetVM();
      break;
    case kEmulatorMenuExitPowerOff:
      PowerOffVM();
      break;
    case kEmulatorMenuExitResume:
      break;
  }
  ResumeFromMenu();
  SYSTEM_INFO("menu_closed");
}

// A key is first looked at for the hotkey, whether the menu shows or not.
// Otherwise it goes to the menu while the menu shows, and to the guest while
// it does not. Kept out of line, so that the menu's locals are on core 0's
// stack only while a key is handled, not under the whole run loop.
static __noinline void HandleKey(SouthbridgeKeyEvent key, bool running) {
  const bool menu_was_open = ScreenIsOpen();
  const bool hotkey = TrackHotkey(key);
  if (!menu_was_open && running) {
    TypeKey(key);
  }
  if (hotkey && !menu_was_open) {
    OpenMenu(running);
    return;
  }
  if (hotkey) {
    ScreenHide();
  } else if (menu_was_open) {
    ScreenHandleKey(key);
  }
  if (menu_was_open && !ScreenIsOpen()) {
    CloseMenu();
  }
}

// Whether a box over the guest's screen is one of another size than the one
// shown, so that the guest's screen has to be drawn under it first.
static bool BoxResized(const DisplayBox* box) {
  return g_box_shown.drawn &&
         (box->columns != g_box_shown.columns || box->rows != g_box_shown.rows);
}

// Draw the screen showing, and send it to core 1. A box of another size than
// the last leaves part of the last showing, so the guest's screen is drawn
// in full under it first. If core 1 cannot take that yet, the box waits, and
// is sent on a later pass, drawn again if the screen has changed meanwhile.
// Kept out of line, as HandleKey() is.
static __noinline void DrawMenu(void) {
  DisplayBox* box = DisplayBeginBox();
  if (box == NULL) {
    return;
  }
  // The status page reads its lines as it draws, every second while it
  // shows. Filling them in costs nothing that matters while the VM is
  // paused.
  DescribeStatus(g_status_lines);
  if (ScreenDraw(box)) {
    g_box_pending = true;
  }
  if (!g_box_pending || (BoxResized(box) && !RedrawGuest())) {
    return;
  }
  DisplayEndBox();
  g_box_shown.columns = box->columns;
  g_box_shown.rows = box->rows;
  g_box_shown.drawn = true;
  g_box_pending = false;
}

// ============================================================================
// The main loop
// ============================================================================

// Run the VM, paced to real time, for as long as the board has power.
// The boot is watched on the way, and DOS's questions answered. While the
// menu is open, the VM is paused and the loop only looks after the menu,
// the keyboard and commands.
static void RunVM(bool vm_ready) {
  const uint64_t max_boot_cycles =
      (uint64_t)kBootMaxEmulatedSeconds * Y_PLATFORM_CPU_CYCLES_PER_SECOND;
  uint64_t next_poll_cycles = kBootPollCycles;
  uint64_t next_render_cycles = kCyclesPerRender;
  bool running = vm_ready;
  bool screen_on = true;
  absolute_time_t blank_at = at_the_end_of_time;
  absolute_time_t next_status_at = get_absolute_time();
  g_clock.last_ticks = g_platform.ticks;
  g_clock.start_us = time_us_64();
  g_clock.anchor_us = g_clock.start_us;

  for (;;) {
    AnnounceToNewHost();

    if (running && !ScreenIsOpen() && RunBatch() == Y_PLATFORM_HUNG) {
      // The CPU halted with nothing to wake it.
      SYSTEM_ERROR("vm_stopped");
      running = false;
    }

    PollCommand(&g_command_us);
    // Between batches, so the keyboard is never written while the VM
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

    if (screen_on && !ScreenIsOpen() && g_clock.cycles >= next_render_cycles) {
      do {
        next_render_cycles += kCyclesPerRender;
      } while (next_render_cycles <= g_clock.cycles);
      if (YAX86_PICOCALC_DRAW_SCREEN) {
        DisplaySendFrame(&g_platform.video);
      }
    }

    if (!BootIsOver() && !ScreenIsOpen()) {
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
    if (screen_on && ScreenIsOpen()) {
      DrawMenu();
    }
    if (screen_on && time_reached(blank_at)) {
      DisplayTurnOff();
      screen_on = false;
    }

    if (ScreenIsOpen()) {
      sleep_ms(kPausedPollMs);
    } else if (running) {
      PaceToRealTime();
    } else {
      sleep_ms(kStoppedPollMs);
    }
  }
}

// ============================================================================
// Starting the VM
// ============================================================================

// Open the drives' images: the VM's, or without one, none - drive A then
// has the built-in floppy. An image a VM names that does not open leaves its
// drive empty, which a message says - as does one named for two drives, which
// goes in the first: two drives writing one image would each overwrite the
// other's sectors.
static __noinline void OpenDrives(void) {
  const char* paths[kVMDrives] = {"", "", "", ""};
  char message[kTextColumns + 1];
  if (g_has_vm) {
    snprintf(message, sizeof(message), "Started %s", g_vm->entry.name);
    EmulatorStatusBarPostMessage(message);
    for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
      paths[drive] = g_vm->drives[drive];
      for (uint8_t earlier = 0; earlier < drive; ++earlier) {
        if (paths[drive][0] != '\0' &&
            FSSamePath(paths[drive], paths[earlier])) {
          char name[kDiskImageNameLength + 1];
          DiskImageName(paths[drive], name);
          snprintf(
              message, sizeof(message), "%s: %s is already in %s",
              kVMDriveLabels[drive], name, kVMDriveLabels[earlier]);
          EmulatorStatusBarPostMessage(message);
          paths[drive] = "";
        }
      }
    }
  }
  DiskImageStatus opened[kVMDrives];
  FloppyOpen(&paths[kVMDriveFD1], !g_has_vm, &opened[kVMDriveFD1]);
  HardDiskOpen(&paths[kVMDriveHD1]);
  opened[kVMDriveHD1] = HardDiskGetImage(kHardDisk1)->status;
  opened[kVMDriveHD2] = HardDiskGetImage(kHardDisk2)->status;
  if (!g_has_vm) {
    return;
  }
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    if (paths[drive][0] == '\0' || opened[drive] == kDiskImageOpen) {
      continue;
    }
    char name[kDiskImageNameLength + 1];
    DiskImageName(paths[drive], name);
    snprintf(
        message, sizeof(message), "%s: could not open %s: %s",
        kVMDriveLabels[drive], name, DiskImageDescribeStatus(opened[drive]));
    EmulatorStatusBarPostMessage(message);
    DISK_WARN(
        "open_failed", Y_LOG_UINT("drive", drive),
        Y_LOG_STRING("path", paths[drive]),
        Y_LOG_STRING("error", DiskImageDescribeStatus(opened[drive])));
  }
}

// ============================================================================
// Emulator mode
// ============================================================================

void EmulatorInit(
    const InitStatus* init_status, const VMList* vms, const Settings* settings,
    uint8_t* guest_ram) {
  g_init_status = init_status;
  g_vms = vms;
  g_settings = settings;
  g_guest_ram = guest_ram;
}

void EmulatorRun(VMConfig* vm) {
  g_vm = vm;
  g_has_vm = vm != NULL;
  g_session.config = vm;
  g_session.fdc = &g_platform.fdc;
  OpenDrives();
  RunVM(InitVM());
}
