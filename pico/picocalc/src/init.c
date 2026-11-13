#include "init.h"

#include <stdio.h>

#include "display.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include "psram.h"
#include "runtime_log.h"
#include "southbridge.h"
#include "speaker.h"

// The system clock in kHz, or 0 for the stock 125MHz. Set by CMakeLists.txt.
#ifndef YAX86_PICOCALC_SYS_CLK_KHZ
#define YAX86_PICOCALC_SYS_CLK_KHZ 0
#endif

enum {
  // How long the watchdog waits, across the clock change, before deciding the
  // chip has hung and rebooting it.
  kWatchdogTimeoutMs = 4000,
  // Written to g_overclock_marker across the clock change, and cleared once it
  // is survived. Any value does, as long as uninitialized RAM is unlikely to
  // hold it.
  kOverclockAttemptMarker = 0x0C10C4ED,
  // Written to g_psram_marker across bringing the PSRAM up, and cleared once
  // it is survived. See StartPsram().
  kPsramAttemptMarker = 0x95A3B007,
  // Written to g_fault_marker by the hard fault handler, for the start after
  // the reboot it makes. See isr_hardfault().
  kFaultMarker = 0xFA017ED0,
  // How long the core voltage is given to settle before the clock is raised.
  kVoltageSettleUs = 10 * 1000,
};

#if YAX86_PICOCALC_SYS_CLK_KHZ
// Deliberately not zeroed at startup. See Overclock().
static uint32_t __uninitialized_ram(g_overclock_marker);
#endif
// The same for the PSRAM. See StartPsram().
static uint32_t __uninitialized_ram(g_psram_marker);
// The same for a hard fault. See isr_hardfault().
static uint32_t __uninitialized_ram(g_fault_marker);

static const char* const kOverclockNames[] = {
    [kOverclockNotRequested] = "not_requested",
    [kOverclockApplied] = "applied",
    [kOverclockPLLCannot] = "pll_cannot_produce_clock",
    [kOverclockSkippedAfterHang] = "skipped_after_hang",
};

const char* InitOverclockName(OverclockResult overclock) {
  return kOverclockNames[overclock];
}

void InitFormatKeyboardVersion(
    const InitStatus* status, char* buffer, size_t size) {
  if (!status->southbridge_answered) {
    snprintf(buffer, size, "not answering");
  } else if (status->southbridge_version == 0) {
    snprintf(buffer, size, "older than v1.6");
  } else {
    snprintf(
        buffer, size, "v%X.%X", status->southbridge_version >> 4,
        status->southbridge_version & 0xF);
  }
}

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

// Bring the PSRAM up with the watchdog armed, as the overclock is raised: the
// driver bounds every wait, so a hang here would be something it did not
// foresee, and the reboot it turns into starts without the PSRAM. The marker
// is the overclock's kind, and the watchdog disarmed after for the same
// reason. See Overclock().
static void StartPsram(void) {
  if (g_psram_marker == kPsramAttemptMarker) {
    g_psram_marker = 0;
    PsramInit(true);
    return;
  }
  g_psram_marker = kPsramAttemptMarker;
  watchdog_enable(kWatchdogTimeoutMs, true);
  PsramInit(false);
  watchdog_disable();
  g_psram_marker = 0;
}

// A hard fault, on either core, reboots the board rather than leaving it hung
// with nothing on USB, and the start after it runs without paging: InitStatus
// says so, and emulator.c's ConfigureGuestRAM() leaves paging off. Replaces
// the SDK's handler, a breakpoint, which halts.
void isr_hardfault(void) {
  g_fault_marker = kFaultMarker;
  watchdog_reboot(0, 0, 0);
  for (;;) {
  }
}

// Kept out of line, so that what only initializing needs is off core 0's
// stack while the modes run.
__noinline void InitSystem(InitStatus* status) {
  // Before anything can fault again.
  status->after_fault = g_fault_marker == kFaultMarker;
  g_fault_marker = 0;
  stdio_init_all();
#if YAX86_PICOCALC_SYS_CLK_KHZ
  // Before anything else is brought up, since each works out its clock
  // divider when it is: the LCD's, on core 1, from the system clock, and the
  // keyboard controller's I2C from the peripheral clock, which the overclock
  // moves to the 48MHz USB PLL.
  status->overclock = Overclock();
#endif
  // After the clock, which it divides for its own.
  StartPsram();

  RuntimeLogInit();

  status->lcd_spi_hz = DisplayStart();
  SouthbridgeInit();
  SpeakerInit();
  status->southbridge_answered =
      SouthbridgeReadVersion(&status->southbridge_version);
}
