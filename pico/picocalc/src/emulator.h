// Emulator mode: running a VM once the launcher has loaded one. The VM starts
// with its drives' images. Without a card there is no VM, and the machine
// boots the floppy built into flash. It runs paced to real time for as long
// as the board has power, with the PicoCalc's keyboard as its own. DOS's
// date and time questions are answered by watching the screen, the way
// pico/bench does it, which keeps the boot pico/bench's dos-boot workload.
//
// It runs on core 0 and never touches the LCD. Core 1 draws everything on
// it, from copies core 0 sends: see display.h. The guest's screen takes the
// top 250 rows of the panel, 80 by 25 cells of the 4x10 font, and the status
// rows the rest. Everything on them is also reported over USB serial, for
// pico/bench/run.js to capture. A report is printed whenever a host
// attaches, since USB CDC drops whatever is written before then.
//
// Both Shift keys held together open the in-VM menu over the guest's screen,
// and pause the VM. The menu is screens, through screen.h, starting with
// emulator_menu_screen.h's main page. While it shows, the loop hands it the
// keys and sends what it draws to core 1, drawing the guest's screen under
// a box of a new size first. When it closes, the loop carries out what it
// decided, a reset or a power off, or resumes the VM.
//
// Its loop answers two debug commands itself: report, with the system's
// groups and then the VM's, and screen, with the guest's text screen. The
// others are common to every mode.
#ifndef YAX86_PICOCALC_EMULATOR_H
#define YAX86_PICOCALC_EMULATOR_H

#include <stdint.h>

#include "init.h"
#include "settings.h"
#include "vm_config.h"
#include "vm_list.h"

// Set the emulator up, before launcher mode: with what initializing the
// system found, the card's VMs and settings as the launcher reads them, and
// guest_ram, kPicoGuestSRAMSize bytes, which the launcher borrows until the
// VM starts. Each must outlive the emulator.
void EmulatorInit(
    const InitStatus* init_status, const VMList* vms, Settings* settings,
    uint8_t* guest_ram);

// Run vm, main.c's, whose floppy drives' images are saved to it as they
// change; or with NULL, the built-in floppy. Never returns.
void EmulatorRun(VMConfig* vm);

#endif  // YAX86_PICOCALC_EMULATOR_H
