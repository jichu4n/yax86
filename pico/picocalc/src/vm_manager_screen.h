// The VM manager, from the list's Manage VMs row: the VMs in yax86/vm, a row
// each, then New VM. A VM's page renames, clones, deletes it and edits its
// drives, and New VM makes one.
#ifndef YAX86_PICOCALC_VM_MANAGER_SCREEN_H
#define YAX86_PICOCALC_VM_MANAGER_SCREEN_H

#include <stdint.h>

#include "screen.h"
#include "settings.h"
#include "vm_list.h"

// Show the VM manager for vms and settings, which must outlive it, and which
// the screens under it change as they change the card. A copy goes through
// copy_buffer, copy_buffer_size bytes of guest RAM.
ScreenResult VMManagerScreenOpen(
    VMList* vms, Settings* settings, uint8_t* copy_buffer,
    uint32_t copy_buffer_size);

#endif  // YAX86_PICOCALC_VM_MANAGER_SCREEN_H
