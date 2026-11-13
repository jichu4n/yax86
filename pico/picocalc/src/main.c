// yax86 on the ClockworkPi PicoCalc.
//
// main.c brings the board up: the system clock, the PSRAM, the LCD on core
// 1, the keyboard controller and the speaker. Then it runs the two modes,
// one after the other. The launcher loads a VM from those on the SD card, and
// the emulator runs it; without a card there is no VM, and the emulator runs
// the floppy built into flash.
//
// main.c holds what both modes use: guest RAM, which the launcher borrows to
// copy files until the VM starts; the card's VMs and settings; and the VM
// to start.

#include <stdbool.h>
#include <stdint.h>

#include "board.h"
#include "display.h"
#include "emulator.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/watchdog.h"
#include "launcher.h"
#include "pico/stdlib.h"
#include "psram.h"
#include "runtime_log.h"
#include "settings.h"
#include "southbridge.h"
#include "speaker.h"
#include "stacks.h"
#include "vm_config.h"
#include "vm_list.h"
#include "yax86_pico.h"

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
  // it is survived - see StartPsram().
  kPsramAttemptMarker = 0x95A3B007,
  // Written to g_fault_marker by the hard fault handler, for the start after
  // the reboot it makes - see isr_hardfault().
  kFaultMarker = 0xFA017ED0,
  // How long the core voltage is given to settle before the clock is raised.
  kVoltageSettleUs = 10 * 1000,
};

// Guest RAM: conventional memory itself without the PSRAM, and the frames it
// is paged through with it - see yax86_pico.h.
static uint8_t g_guest_ram[kPicoGuestSRAMSize];
// What bringing the board up found.
static BoardStatus g_board;
// The card's VMs and settings, which the launcher reads and its screens
// change; and the VM the launcher loaded to start, if it loaded one.
static VMList g_vms;
static Settings g_settings;
static VMConfig g_vm;

#if YAX86_PICOCALC_SYS_CLK_KHZ
// Deliberately not zeroed at startup - see Overclock().
static uint32_t __uninitialized_ram(g_overclock_marker);
#endif
// The same for the PSRAM - see StartPsram().
static uint32_t __uninitialized_ram(g_psram_marker);
// The same for a hard fault - see isr_hardfault().
static uint32_t __uninitialized_ram(g_fault_marker);

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
// reason - see Overclock().
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
// with nothing on USB, and the start after it runs without paging - see
// ConfigureGuestRAM(). Replaces the SDK's handler, a breakpoint, which halts.
void isr_hardfault(void) {
  g_fault_marker = kFaultMarker;
  watchdog_reboot(0, 0, 0);
  for (;;) {
  }
}

// Bring the board up. Kept out of line, so that what only bringing it up
// needs is off core 0's stack while the modes run.
static __noinline void BringUp(void) {
  // Before anything can fault again.
  g_board.after_fault = g_fault_marker == kFaultMarker;
  g_fault_marker = 0;
  // First, before anything has used more of the stack than main's and this
  // frame, or core 1 has started.
  StacksPaint();
  stdio_init_all();
#if YAX86_PICOCALC_SYS_CLK_KHZ
  // Before anything else is brought up, since each works out its clock
  // divider when it is: the LCD's, on core 1, from the system clock, and the
  // keyboard controller's I2C from the peripheral clock, which the overclock
  // moves to the 48MHz USB PLL.
  g_board.overclock = Overclock();
#endif
  // After the clock, which it divides for its own.
  StartPsram();

  RuntimeLogInit();

  g_board.lcd_spi_hz = DisplayStart();
  SouthbridgeInit();
  SpeakerInit();
  g_board.southbridge_answered =
      SouthbridgeReadVersion(&g_board.southbridge_version);
}

int main(void) {
  BringUp();
  // Before the launcher, whose commands the emulator answers too.
  EmulatorInit(&g_board, &g_vms, &g_settings, g_guest_ram);
  // Guest RAM is lent to the launcher until a VM starts, for copies.
  const bool has_vm =
      LauncherRun(&g_vms, &g_settings, g_guest_ram, &kEmulatorCommands, &g_vm);
  EmulatorRun(has_vm ? &g_vm : NULL);
  return 0;
}
