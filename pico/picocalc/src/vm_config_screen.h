// A VM's configuration, as its vm.cfg holds it: for a new VM, from the VM
// manager's New VM, or for an existing one, from its page's Configure. A
// new VM's name, typed in place, and what each drive holds, each chosen on
// a page of its own, then Create or Save.
#ifndef YAX86_PICOCALC_VM_CONFIG_SCREEN_H
#define YAX86_PICOCALC_VM_CONFIG_SCREEN_H

#include <stdint.h>

#include "screen.h"
#include "vm_manager.h"

// Configure a new VM: no name, floppy disk 1 empty, floppy disk 2 and
// hard disk 2 disabled, and a new disk image as hard disk 1. Create makes
// the VM and notes it in vm_manager before the screen goes back; a name or
// drives that will not do show why, and the form stays. vm_manager must outlive
// the screen.
ScreenResult VMConfigScreenOpenNew(VMManager* vm_manager);

// Configure the existing VM at index in vm_manager's list, starting from its
// drives as its vm.cfg has them. Save gives the VM the drives chosen, as for
// Create.
ScreenResult VMConfigScreenOpenExisting(VMManager* vm_manager, uint8_t index);

#endif  // YAX86_PICOCALC_VM_CONFIG_SCREEN_H
