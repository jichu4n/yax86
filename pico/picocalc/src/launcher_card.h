// What the launcher does with the card, at the start and from the list:
// read its settings, finding the default VM among its VMs, and take a VM to
// start, reading its vm.cfg and making it the default.
#ifndef YAX86_PICOCALC_LAUNCHER_CARD_H
#define YAX86_PICOCALC_LAUNCHER_CARD_H

#include <stdbool.h>
#include <stdint.h>

#include "settings.h"
#include "vm_config.h"
#include "vm_list.h"

// Read the card's settings into settings, use their font, and set
// *highlighted to the index of the default VM in vms, or 0 without one.
// Returns false, with the problem shown on the list, if the settings had
// one.
bool LauncherCardReadSettings(
    const VMList* vms, Settings* settings, uint8_t* highlighted);

// Read the vm.cfg of the VM at index in vms into *chosen, and make it the
// default in settings and on the card. Returns false, with its problem shown
// on the list, if it does not read.
bool LauncherCardTakeVM(
    const VMList* vms, Settings* settings, uint8_t index, VMConfig* chosen);

#endif  // YAX86_PICOCALC_LAUNCHER_CARD_H
