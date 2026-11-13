// yax86 on the ClockworkPi PicoCalc.
//
// main() starts the system and then runs the two modes, one after the other.
// It paints the stacks for stack_usage.c to measure, and has init.c
// initialize the system: the clock, the PSRAM, the LCD on core 1, the
// keyboard controller and the speaker. Then launcher mode loads a VM from
// those on the SD card, and emulator mode runs it. Without a card there is no
// VM, and the emulator runs the floppy built into flash.
//
// main.c holds what both modes use: what initializing the system found;
// guest RAM, which the launcher borrows to copy files until the VM starts;
// the card's VMs and settings; and the VM to start.

#include <stdbool.h>
#include <stdint.h>

#include "debug_command.h"
#include "emulator.h"
#include "init.h"
#include "launcher.h"
#include "settings.h"
#include "stack_usage.h"
#include "vm_config.h"
#include "vm_list.h"
#include "yax86_pico.h"

// What initializing the system found.
static InitStatus g_init_status;
// Guest RAM: conventional memory itself when there is no PSRAM, and the
// frames conventional memory is paged through when there is. yax86_pico.h
// describes both.
static uint8_t g_guest_ram[kPicoGuestSRAMSize];
// The card's VMs and settings, which the launcher reads and its screens
// change.
static VMList g_vms;
static Settings g_settings;
// The VM the launcher loaded to start, if it loaded one.
static VMConfig g_vm;

int main(void) {
  // First, before anything has used more of the stack than main's frame, or
  // core 1 has started.
  StackUsagePaint();
  InitSystem(&g_init_status);
  DebugCommandInit(&g_init_status);
  EmulatorInit(&g_init_status, &g_vms, &g_settings, g_guest_ram);
  // Guest RAM is lent to the launcher until a VM starts, for copies.
  const bool has_vm = LauncherRun(&g_vms, &g_settings, g_guest_ram, &g_vm);
  EmulatorRun(has_vm ? &g_vm : NULL);
  return 0;
}
