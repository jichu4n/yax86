// The emulator session: one run of the emulator, from the VM's start until
// it powers off or resets. It is what the in-VM menu works on: the VM's
// configuration and the machine's floppy disk controller. Without a VM, the
// machine boots the floppy built into flash.
//
// emulator.c holds the session, and passes a pointer to it down to the
// in-VM menu's screens.
#ifndef YAX86_PICOCALC_EMULATOR_SESSION_H
#define YAX86_PICOCALC_EMULATOR_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "core/fdc.h"
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

#endif  // YAX86_PICOCALC_EMULATOR_SESSION_H
