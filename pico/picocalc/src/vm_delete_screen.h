// The question before deleting a VM, saying what goes with it, and which VMs
// lose a disk.
#ifndef YAX86_PICOCALC_VM_DELETE_SCREEN_H
#define YAX86_PICOCALC_VM_DELETE_SCREEN_H

#include <stdint.h>

#include "manage.h"
#include "screen.h"

// Ask whether to delete the VM at index in manage's list. Delete deletes it,
// noting the change in manage, before the screen goes back; Cancel goes back
// alone. manage must outlive the screen.
ScreenResult VMDeleteScreenOpen(ManageState* manage, uint8_t index);

#endif  // YAX86_PICOCALC_VM_DELETE_SCREEN_H
