// Status reporting and host commands over USB for the PicoCalc runtime.
//
// Formats the 6-line Status page in the menu and replies to commands sent by a
// host over USB serial (report, screen, screenshot, key, ping).
#ifndef YAX86_PICOCALC_REPORT_H
#define YAX86_PICOCALC_REPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "command.h"
#include "core/platform.h"
#include "menu.h"
#include "vm_config.h"

// What became of the request to raise the system clock.
typedef enum OverclockResult {
  kOverclockNotRequested = 0,
  kOverclockApplied,
  kOverclockPLLCannot,
  kOverclockSkippedAfterHang,
} OverclockResult;

// How far the boot has got. DOS asks for the date and then the time before it
// gives a prompt.
typedef enum BootStage {
  kBootStagePowerOn = 0,
  kBootStageAnsweringDate,
  kBootStageAnsweringTime,
  kBootStagePrompt,
  kBootStageFailed,
} BootStage;

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
  // Host time spent running the VM up to then, which is the elapsed time
  // less the time spent waiting for real time to catch up. It is what the
  // boot would have taken unpaced.
  uint64_t boot_busy_us;
  uint64_t instructions;
} RuntimeStatus;

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

typedef struct ReportConfig {
  const YPlatformState* platform;
  // Reads a byte of guest RAM. Guest RAM may be paged, so it is read through
  // the memory map, and before the VM is up it reads as 0.
  uint8_t (*read_guest_ram_fn)(uint32_t address);
  // How much guest RAM the VM has, in bytes, which depends on whether it is
  // paged.
  uint32_t (*guest_ram_size_fn)(void);
  // Whether guest RAM is paged, and whether paging is off because the last
  // start ended in a hard fault.
  const bool* paging;
  const bool* paging_off_after_fault;
  const uint8_t* vram;
  const RuntimeStatus* status;
  const RunClock* clock;
  const VMList* vms;
  const VMSettings* settings;
  const VMConfig* vm;
  const bool* has_vm;
} ReportConfig;

// Initialize the reporting module with pointers to the runtime state.
void ReportInit(const ReportConfig* config);

// Paint the stacks of both cores to measure high-water mark usage.
void ReportPaintStacks(void);

// Format the 6 lines of the menu's status page.
void ReportDescribeStatus(MenuStatusLine lines[kMenuStatusLines]);

// Announce completion to a newly attached USB host if boot has finished.
void ReportAnnounceToNewHost(void);

// Log the current boot stage.
void ReportLogBootStage(void);

// Execute a command received from a host over USB.
void ReportRunCommand(const Command* command);

// Total host time spent executing commands.
uint64_t ReportCommandUs(void);

#endif  // YAX86_PICOCALC_REPORT_H
