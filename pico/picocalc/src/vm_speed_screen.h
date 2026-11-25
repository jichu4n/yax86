// Choose a VM's speed mode: 4.77MHz or Max. Used both from the New VM /
// Configure form (to set the VM's default in vm.cfg) and from the in-VM menu
// (to switch the running VM's speed mode).
#ifndef YAX86_PICOCALC_VM_SPEED_SCREEN_H
#define YAX86_PICOCALC_VM_SPEED_SCREEN_H

#include "screen.h"
#include "vm_config.h"

// Open the speed chooser with *chosen selected. Selecting an item writes it
// back through chosen and returns to the caller.
ScreenResult VMSpeedScreenOpen(VMSpeed* chosen);

#endif  // YAX86_PICOCALC_VM_SPEED_SCREEN_H
