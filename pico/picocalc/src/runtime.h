// What the runtime, main.c, shares with the modules it hands its state to:
// whether the screen is drawn, how far the boot has got, and the clock it
// paces the VM by.
#ifndef YAX86_PICOCALC_RUNTIME_H
#define YAX86_PICOCALC_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

// Whether the guest's screen is drawn. Building with it off gives an image
// identical but for that, which is what measures what drawing costs. Set by
// CMakeLists.txt.
#ifndef YAX86_PICOCALC_DRAW_SCREEN
#define YAX86_PICOCALC_DRAW_SCREEN 1
#endif

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

// Whether the boot is over, one way or the other: at the prompt, or given up.
bool RuntimeBootIsOver(void);

// Host time since power on, and how much of it went to running the VM
// rather than waiting for real time to catch up with it.
uint64_t RuntimeElapsedUs(void);

// Host time spent with the VM paused for the menu, including a pause under
// way.
uint64_t RuntimePausedUs(void);

// Host time spent carrying out commands a host sent, which a boot that took
// them is that much slower for.
uint64_t RuntimeCommandUs(void);

#endif  // YAX86_PICOCALC_RUNTIME_H
