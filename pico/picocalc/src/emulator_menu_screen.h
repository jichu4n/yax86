// The in-VM menu's main page: the first screen when both Shift keys open the
// menu over a running VM, which is paused while the menu shows.
//
// Its items each open a screen of their own over it:
//
//   Floppy disk 1  the drive's page, to change its disk
//   Floppy disk 2  the same, shown only when the machine has a second drive
//   Status         the status page
//   Reset          the question before resetting the VM
//   Power off      the question before powering it off, shown only while
//                  there is a list of VMs to go back to
//
// Esc and Backspace close the menu, as both Shift keys do. So does inserting
// a disk, on a drive's page. Back from another page, the item that opened it
// is selected.
//
// The main page only decides. When Reset or Power off is answered yes, the
// menu closes with that written to an EmulatorMenuExit, and the emulator
// carries it out, in emulator.c.
#ifndef YAX86_PICOCALC_EMULATOR_MENU_SCREEN_H
#define YAX86_PICOCALC_EMULATOR_MENU_SCREEN_H

#include "running_vm.h"
#include "screen.h"
#include "status_screen.h"

// What the emulator is to do once the menu closes.
typedef enum EmulatorMenuExit {
  // Carry on running the VM.
  kEmulatorMenuExitResume = 0,
  // Restart the VM from power on, with the disks it has now.
  kEmulatorMenuExitReset,
  // Power the VM off, and go back to the list of VMs.
  kEmulatorMenuExitPowerOff,
} EmulatorMenuExit;

// Open the in-VM menu's main page.
//
// vm is the running VM, whose floppy disks the drives' pages change.
// status_lines is the status page's lines, which the caller keeps current
// while the menu shows.
//
// *exit is set to kEmulatorMenuExitResume, and to what the emulator is to do
// once the menu closes. All of these must outlive the menu.
ScreenResult EmulatorMenuScreenOpen(
    RunningVM* vm, const StatusScreenLine status_lines[kStatusScreenLines],
    EmulatorMenuExit* exit);

#endif  // YAX86_PICOCALC_EMULATOR_MENU_SCREEN_H
