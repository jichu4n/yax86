// The card's VMs and its settings, as last read: the one copy of them that
// the list, its pages and main.c all read. Reading and saving them is here;
// showing why one failed is left to the caller, as nothing here draws.
#ifndef YAX86_PICOCALC_VM_STORE_H
#define YAX86_PICOCALC_VM_STORE_H

#include <stdbool.h>
#include <stdint.h>

#include "font_4x10.h"
#include "vm_config.h"

// The VMs in yax86/vm, and the settings in yax86/yax86.cfg, as last read.
const VMList* VMStoreList(void);
const VMSettings* VMStoreSettings(void);

// Read the card's VMs and its settings afresh, and set *default_index to the
// index of the default VM in the list: boot_default's, or the first if it
// names none. Returns false, with the problem described, if the settings had
// one; they then hold what was read before it.
bool VMStoreRead(uint8_t* default_index, char problem[kVMProblemLength + 1]);

// Make the VM named name the one to start at the next power on, if it is not
// already. Returns false, with the problem described, if it could not be
// saved.
bool VMStoreSaveBootDefault(
    const char* name, char problem[kVMProblemLength + 1]);

// Make font the settings' font, saving it if it changed. Returns false, with
// the problem described, if it could not be saved; it is the font in the
// settings either way.
bool VMStoreSaveFont(Font4x10 font, char problem[kVMProblemLength + 1]);

#endif  // YAX86_PICOCALC_VM_STORE_H
