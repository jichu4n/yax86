// The running VM: the VM emulator mode runs, as the in-VM menu's screens see
// and change it. Without a VM, the machine boots the floppy built into
// flash.
//
// The menu changes the VM's floppy disks through this. Inserting or ejecting
// a disk says so on the status rows, and saves what the drive now holds to
// the VM's vm.cfg, so that the next start has it.
//
// emulator.c holds the RunningVM, and passes a pointer to it down to the
// menu's screens.
#ifndef YAX86_PICOCALC_RUNNING_VM_H
#define YAX86_PICOCALC_RUNNING_VM_H

#include <stdbool.h>
#include <stdint.h>

#include "core/fdc.h"
#include "floppy.h"
#include "vm_config.h"

typedef struct RunningVM {
  // The VM's configuration, which the launcher read from its vm.cfg, or NULL
  // without a VM.
  VMConfig* config;
  // The machine's floppy disk controller, which is told of a disk inserted
  // or ejected.
  YFDCState* fdc;
} RunningVM;

// The floppy drives the machine has: two, or one for a VM whose vm.cfg does
// not name floppy disk 2. With one, the BIOS's switches report one, and
// DOS's B: is drive A by another name. Without a VM, the machine has two,
// the machine pico/bench's dos-boot workload measures.
uint8_t RunningVMFloppyDrives(const RunningVM* vm);

// Whether there is a list of VMs to power off to. There is one only with a
// card, and a VM is started only from it.
bool RunningVMCanPowerOff(const RunningVM* vm);

// Take the disk out of a floppy drive, say so, and save the empty drive.
void RunningVMEjectFloppy(RunningVM* vm, FloppyDrive drive);

// Put the image at path, a path from the card's root, in a floppy drive,
// say whether it went in, and save what the drive now holds. An image
// already in the other drive stays there: two drives writing one image
// would each overwrite the other's sectors.
void RunningVMInsertFloppy(RunningVM* vm, FloppyDrive drive, const char* path);

#endif  // YAX86_PICOCALC_RUNNING_VM_H
