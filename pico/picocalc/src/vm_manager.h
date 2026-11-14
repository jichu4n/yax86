// The VM manager, which the list's Manage VMs row opens: what its screens
// share, the card's VMs and settings, which the launcher holds, the RAM a
// copy goes through, and what a screen changed on the card, for the VM
// manager's screen to show once it is back on top. That screen keeps one,
// and passes it down to each screen it opens, as each of those does to its
// own.
//
// Also what those screens have in common in carrying out a change: showing
// and logging a problem, and drawing a copy's progress as it goes.
#ifndef YAX86_PICOCALC_VM_MANAGER_H
#define YAX86_PICOCALC_VM_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "fs.h"
#include "settings.h"
#include "vm_config.h"
#include "vm_list.h"

typedef struct VMManager {
  // The launcher's: the list is read again once the card changes, and the
  // settings follow a rename or delete of the default VM.
  VMList* vms;
  Settings* settings;
  // Guest RAM, lent before the VM starts, that a clone's files and blank
  // disks are copied through, and how long it is.
  uint8_t* copy_buffer;
  uint32_t copy_buffer_size;
  // Set by a screen that changed the card, before it goes back: the VM to
  // select once the list is read again, the first if it is not there, and
  // whether to show its screen too.
  bool changed;
  char to_select[kVMNameLength + 1];
  bool show_vm;
} VMManager;

// Note that the card changed, with name the VM to select after, and with
// show_vm, to show its screen too.
void VMManagerNoteChange(VMManager* vm_manager, const char* name, bool show_vm);

// Show why something could not be done to vm, and log it as event.
void VMManagerShowProblem(
    const char* event, const VMEntry* vm, const char* problem);

// A transfer through the copy buffer that draws its progress as it goes: a
// page titled title, saying what with the KB copied under it. The page
// draws at once, as the loop that would draw it is waiting for the copy.
// title and what must outlive the copy.
FSTransfer VMManagerBeginCopy(
    const VMManager* vm_manager, const char* title, const char* what);

// Zero the copy buffer after a copy, leaving it as the guest would find RAM
// at power on, whatever it held.
void VMManagerEndCopy(const VMManager* vm_manager);

#endif  // YAX86_PICOCALC_VM_MANAGER_H
