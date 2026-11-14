// The emulator session: one run of the emulator, from the VM's start until
// it powers off or resets. It is how the in-VM menu's screens see and change
// the emulator: the VM's configuration, the machine's floppy disk
// controller, and changing the disks in its floppy drives. Without a VM, the
// machine boots the floppy built into flash.
//
// Inserting or ejecting a disk says so on the status rows, and saves what
// the drive now holds to the VM's vm.cfg, so that the next start has it.
//
// emulator.c holds the session, and passes a pointer to it down to the
// in-VM menu's screens.
#ifndef YAX86_PICOCALC_EMULATOR_SESSION_H
#define YAX86_PICOCALC_EMULATOR_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "core/fdc.h"
#include "floppy.h"
#include "vm_config.h"

typedef struct EmulatorSession {
  // The VM's configuration, which the launcher read from its vm.cfg, or NULL
  // without a VM.
  VMConfig* config;
  // The machine's floppy disk controller, which is told of a disk inserted
  // or ejected.
  YFDCState* fdc;
} EmulatorSession;

// The floppy drives the machine has: two, or one for a VM whose vm.cfg does
// not name floppy disk 2. With one, the BIOS's switches report one, and
// DOS's B: is drive A by another name. Without a VM, the machine has two,
// the machine pico/bench's dos-boot workload measures.
uint8_t EmulatorSessionFloppyDrives(const EmulatorSession* session);

// Whether there is a list of VMs to power off to. There is one only with a
// card, and a VM is started only from it.
bool EmulatorSessionCanPowerOff(const EmulatorSession* session);

// Take the disk out of a floppy drive, say so, and save the empty drive.
void EmulatorSessionEjectFloppy(EmulatorSession* session, FloppyDrive drive);

// Put the image at path, a path from the card's root, in a floppy drive,
// say whether it went in, and save what the drive now holds. An image
// already in the other drive stays there: two drives writing one image
// would each overwrite the other's sectors.
void EmulatorSessionInsertFloppy(
    EmulatorSession* session, FloppyDrive drive, const char* path);

#endif  // YAX86_PICOCALC_EMULATOR_SESSION_H
