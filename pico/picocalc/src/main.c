// yax86 on the ClockworkPi PicoCalc.
//
// The runtime raises the system clock, starts core 1 drawing on the LCD, brings
// up the keyboard controller, and starts a VM chosen from those on the SD
// card, with its drives' images - or with none on the card, boots MS-DOS 3.30
// off the floppy built into flash - with the guest's text screen on the LCD
// as it goes. The VM is
// paced to real time and keeps running past the prompt, with the PicoCalc's
// keyboard as its own. DOS's date and time questions are still answered by
// watching the screen, the way pico/bench does it, which keeps the boot the
// dos-boot workload.
//
// Core 0, which this file runs on, runs the VM and never touches the
// LCD: core 1 draws everything on it, from copies core 0 hands over - see
// display.h. The guest's screen takes the top 250 rows of the panel - 80 by 25
// cells of the 4x10 font - and the runtime's status lines the rest. Everything
// on them is also reported over USB serial for pico/bench/run.js to capture. A
// report is printed whenever a host attaches, since USB CDC drops whatever is
// written before then.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "command.h"
#include "core/platform.h"
#include "core/video.h"
#include "display.h"
#include "floppy.h"
#include "harddisk.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/watchdog.h"
#include "keymap.h"
#include "launcher.h"
#include "menu.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include "report.h"
#include "runtime_log.h"
#include "sdcard.h"
#include "southbridge.h"
#include "speaker.h"
#include "status_bar.h"
#include "typing.h"
#include "vm_config.h"
#include "vm_folder.h"

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
  kBytesPerKB = 1024,
  kMillisecondsPerSecond = 1000,
  kMicrosecondsPerSecond = 1000 * 1000,
  kMaxLagUs = kMaxLagMs * kMicrosecondsPerMillisecond,
  kHzPerKHz = 1000,
};

static uint8_t g_guest_ram[kGuestRAMSize];
static uint8_t g_vram[Y_VIDEO_CGA_VRAM_SIZE];
static YPlatformState g_platform;
// The VMs on the card, the card's settings, and the VM started, if one was:
// without one drive A holds the built-in floppy and no other drive has a disk.
static VMList g_vms;
static VMSettings g_settings;
static VMConfig g_vm;
static bool g_has_vm;
static RuntimeStatus g_status;
// Status rows composed that core 1 has not been handed yet, because it was
// still drawing the last ones.
static DisplayStatus g_status_rows;
static bool g_status_rows_pending;
static RunClock g_clock;

#if YAX86_PICOCALC_SYS_CLK_KHZ
// Deliberately not zeroed at startup - see Overclock().
static uint32_t __uninitialized_ram(g_overclock_marker);
#endif



static bool BootIsOver(void) {
  return g_status.boot_stage == kBootStagePrompt ||
         g_status.boot_stage == kBootStageFailed;
}

// Host time since power on, and how much of it went to running the VM
// rather than waiting for real time to catch up with it.
static uint64_t ElapsedUs(void) { return time_us_64() - g_clock.start_us; }

static uint64_t PausedUs(void) {
  return g_clock.paused_us +
         (MenuIsOpen() ? time_us_64() - g_clock.pause_start_us : 0);
}

static uint64_t BusyUs(void) {
  return ElapsedUs() - g_clock.waited_us - PausedUs();
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

// Compose the status rows from the VM's state.
static uint8_t FloppyDrives(void);

static void UpdateStatusRows(void) {
  StatusBarState state = {0};
  state.drives[kFloppyDriveA] = FloppyGetImage(kFloppyDriveA);
  state.drives[kFloppyDriveB] =
      FloppyDrives() > 1 ? FloppyGetImage(kFloppyDriveB) : NULL;
  state.drives[kStatusBarDriveHD1] = HardDiskGetImage(kHardDisk1);
  state.drives[kStatusBarDriveHD2] = HardDiskGetImage(kHardDisk2);
  state.vm = g_has_vm ? g_vm.entry.name : NULL;
  state.running = true;
  state.keyboard_flags = g_guest_ram[kBIOSKeyboardFlagsAddress];
  state.battery = SouthbridgeGetBattery();
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



// ============================================================================
// The VM
// ============================================================================

static void SetPCSpeakerFrequency(
    Y_UNUSED YPlatformState* platform, uint32_t frequency_hz) {
  SpeakerSetFrequency(frequency_hz);
}

// The floppy drives the machine has: two, or one for a VM whose vm.cfg does
// not name floppy disk 2 - the BIOS's switches then report one, and DOS's B:
// is drive A by another name. Without a VM, the built-in floppy's machine
// has two, the one dos-boot measures.
static uint8_t FloppyDrives(void) {
  return g_has_vm && !g_vm.present[kVMDriveFD2] ? 1 : kFloppyDrives;
}

static bool InitVM(void) {
  YPlatformConfig* config = &g_platform.config;
  config->logger_config = &g_vm_log_config;
  config->physical_memory_size = kGuestRAMSize;
  config->physical_memory = g_guest_ram;
  config->vram = g_vram;
  config->video_adapter = Y_VIDEO_ADAPTER_CGA;
  config->floppy_drives = FloppyDrives();
  config->video_text_font.glyphs = &(*Font4x10Glyphs(g_settings.font))[0][0];
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
  ReportLogBootStage();
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
// The menu
// ============================================================================

// Mark the guest's screen to be drawn in full, and offer it to core 1 now
// rather than at the next frame, which while the VM is paused never
// comes. Returns whether core 1 took it. If it did not, the mark stays in the
// live state, and the next frame offered draws it all.
static bool RedrawGuest(void) {
  g_platform.video.dirty_state.status = Y_VIDEO_FULL_REDRAW;
  return !YAX86_PICOCALC_DRAW_SCREEN || DisplayOfferFrame(&g_platform.video);
}

// The VM again from power on, with the disks it has now, without the list.
// Without a VM there is no card, and a plain reboot boots the built-in floppy
// again.
static void ResetVM(void) {
  SYSTEM_INFO("reset", Y_LOG_STRING("path", g_vm.entry.path));
  if (!g_has_vm) {
    watchdog_reboot(0, 0, 0);
    for (;;) {
      tight_loop_contents();
    }
  }
  LauncherLeaveStartNote(kStartIntentRestart, g_vm.entry.name);
}

// Back to the list, which waits for a key with the VM left highlighted.
static void PowerOffVM(void) {
  SYSTEM_INFO("power_off", Y_LOG_STRING("path", g_vm.entry.path));
  LauncherLeaveStartNote(kStartIntentList, g_vm.entry.name);
}

// Whether there is a list to power off to: there is one only with a card, and
// a VM is started only from it.
static bool CanPowerOff(void) { return g_has_vm; }

static const char* const kDriveNames[kFloppyDrives] = {
    [kFloppyDriveA] = "fd1",
    [kFloppyDriveB] = "fd2",
};

// Take the disk out of a floppy drive, and say so.
// Save what a floppy drive now holds to the VM's `vm.cfg`, so the next start
// has it. Without a VM - the built-in floppy - there is nothing to save to.
static void SaveDrive(FloppyDrive drive) {
  if (!g_has_vm) {
    return;
  }
  const DiskImage* image = FloppyGetImage(drive);
  const char* path = image->status == kDiskImageOpen ? image->path : "";
  char problem[kVMProblemLength + 1];
  if (!VMConfigSaveDrive(&g_vm, kVMDriveFD1 + drive, path, problem)) {
    char message[kTextColumns + 1];
    snprintf(
        message, sizeof(message), "Could not save %s: %.50s",
        kVMDriveLabels[drive], problem);
    StatusBarPostMessage(message);
    SYSTEM_WARN(
        "drive_not_saved", Y_LOG_STRING("drive", kDriveNames[drive]),
        Y_LOG_STRING("problem", problem));
    return;
  }
  FSCopyPath(path, g_vm.drives[kVMDriveFD1 + drive]);
  SYSTEM_INFO(
      "drive_saved", Y_LOG_STRING("drive", kDriveNames[drive]),
      Y_LOG_STRING("path", path));
}

static void EjectDrive(FloppyDrive drive) {
  FloppyEject(&g_platform.fdc, drive);
  char message[kTextColumns + 1];
  snprintf(
      message, sizeof(message), "Ejected the disk from %s",
      kVMDriveLabels[drive]);
  StatusBarPostMessage(message);
  DISK_INFO("ejected", Y_LOG_STRING("drive", kDriveNames[drive]));
  SaveDrive(drive);
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
        message, sizeof(message), "%s is already in %s", name,
        kVMDriveLabels[holding]);
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
        message, sizeof(message), "Inserted %s in %s", name,
        kVMDriveLabels[drive]);
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
  // The drive is empty after an insert that failed.
  SaveDrive(drive);
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
    ReportAnnounceToNewHost();

    if (running && !MenuIsOpen() && RunBatch() == Y_PLATFORM_HUNG) {
      // The CPU halted with nothing to wake it.
      SYSTEM_ERROR("vm_stopped");
      running = false;
    }

    Command command;
    if (CommandPoll(&command)) {
      ReportRunCommand(&command);
    }
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

// Open the drives' images: the VM's, or without one, none - drive A then
// has the built-in floppy. An image a VM names that does not open leaves its
// drive empty, which a message says - as does one named for two drives, which
// goes in the first: two drives writing one image would each overwrite the
// other's sectors.
static __noinline void OpenDrives(void) {
  const char* paths[kVMDrives] = {"", "", "", ""};
  char message[kTextColumns + 1];
  if (g_has_vm) {
    snprintf(message, sizeof(message), "Started %s", g_vm.entry.name);
    StatusBarPostMessage(message);
    for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
      paths[drive] = g_vm.drives[drive];
      for (uint8_t earlier = 0; earlier < drive; ++earlier) {
        if (paths[drive][0] != '\0' &&
            FSSamePath(paths[drive], paths[earlier])) {
          char name[kDiskImageNameLength + 1];
          DiskImageName(paths[drive], name);
          snprintf(
              message, sizeof(message), "%s: %s is already in %s",
              kVMDriveLabels[drive], name, kVMDriveLabels[earlier]);
          StatusBarPostMessage(message);
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
  // First, before anything has used more of the stack than main's frame, or
  // core 1 has started.
  ReportPaintStacks();
  stdio_init_all();
#if YAX86_PICOCALC_SYS_CLK_KHZ
  // Before anything else is brought up, since each works out its clock
  // divider when it is: the LCD's, on core 1, from the system clock, and the
  // keyboard controller's I2C from the peripheral clock, which the overclock
  // moves to the 48MHz USB PLL.
  g_status.overclock = Overclock();
#endif

  RuntimeLogInit();
  const ReportConfig report_config = {
      .platform = &g_platform,
      .guest_ram = g_guest_ram,
      .guest_ram_kb = (uint16_t)(kGuestRAMSize / 1024),
      .vram = g_vram,
      .status = &g_status,
      .clock = &g_clock,
      .vms = &g_vms,
      .settings = &g_settings,
      .vm = &g_vm,
      .has_vm = &g_has_vm,
  };
  ReportInit(&report_config);

  g_status.lcd_spi_hz = DisplayStart();
  SouthbridgeInit();
  SpeakerInit();
  g_status.southbridge_answered =
      SouthbridgeReadVersion(&g_status.southbridge_version);
  const MenuConfig menu_config = {
      .describe_status = ReportDescribeStatus,
      .reset = ResetVM,
      .floppy_drives = FloppyDrives,
      .power_off = PowerOffVM,
      .can_power_off = CanPowerOff,
      .redraw_guest = RedrawGuest,
      .drive = FloppyGetImage,
      .eject_drive = EjectDrive,
      .insert_drive = InsertDrive,
  };
  MenuInit(&menu_config);

  const LauncherConfig launcher_config = {
      .vms = &g_vms,
      .settings = &g_settings,
      .vm = &g_vm,
      .has_vm = &g_has_vm,
      .guest_ram = g_guest_ram,
      .guest_ram_size = sizeof(g_guest_ram),
  };
  LauncherRun(&launcher_config);
  OpenDrives();
  RunVM(InitVM());
  return 0;
}
