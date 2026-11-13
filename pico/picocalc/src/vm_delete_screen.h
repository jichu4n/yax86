// The question before deleting a VM, saying what goes with it, and which VMs
// lose a disk.
#ifndef YAX86_PICOCALC_VM_DELETE_SCREEN_H
#define YAX86_PICOCALC_VM_DELETE_SCREEN_H

#include <stdint.h>

#include "screen.h"
#include "vm_manager.h"

// Ask whether to delete the VM at index in vm_manager's list. Delete deletes
// it, noting the change in vm_manager, before the screen goes back; Cancel goes
// back alone. vm_manager must outlive the screen.
ScreenResult VMDeleteScreenOpen(VMManager* vm_manager, uint8_t index);

#endif  // YAX86_PICOCALC_VM_DELETE_SCREEN_H
