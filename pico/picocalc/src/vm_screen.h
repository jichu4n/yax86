// A VM's page, from Manage VMs: the images in its drives, over what can be
// done to it: Edit drives, Rename, Clone and Delete.
#ifndef YAX86_PICOCALC_VM_SCREEN_H
#define YAX86_PICOCALC_VM_SCREEN_H

#include <stdint.h>

#include "manage.h"
#include "screen.h"

// Show the page of the VM at index in manage's list, reading its vm.cfg for
// its drives. manage must outlive the screen; once a screen under it has
// changed the card, the page goes back to Manage VMs.
ScreenResult VMScreenOpen(ManageState* manage, uint8_t index);

#endif  // YAX86_PICOCALC_VM_SCREEN_H
