// The form for a new VM, from Manage VMs, or for a VM's drives, from its
// page: a new VM's name, typed in place, and what each drive holds, each
// chosen on a page of its own, then Create or Save.
#ifndef YAX86_PICOCALC_VM_FORM_SCREEN_H
#define YAX86_PICOCALC_VM_FORM_SCREEN_H

#include <stdint.h>

#include "manage.h"
#include "screen.h"

// Start a new VM's form: no name, floppy disk 1 empty, floppy disk 2 and
// hard disk 2 disabled, and a new disk image as hard disk 1. Create makes
// the VM and notes it in manage before the screen goes back; a name or
// drives that will not do show why, and the form stays. manage must outlive
// the screen.
ScreenResult VMFormScreenOpenNew(ManageState* manage);

// Start the form on the drives of the VM at index in manage's list, as its
// vm.cfg has them. Save gives the VM the drives chosen, as for Create.
ScreenResult VMFormScreenOpenEdit(ManageState* manage, uint8_t index);

#endif  // YAX86_PICOCALC_VM_FORM_SCREEN_H
