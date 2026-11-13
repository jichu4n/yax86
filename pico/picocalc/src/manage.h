// What Manage VMs and the screens opened from it share: the card's VMs and
// settings, which the launcher holds; the RAM a copy goes through; and what
// a screen changed on the card, for Manage VMs to show once it is back on
// top. Manage VMs keeps one, and passes it down to each screen it opens, as
// each of those does to its own.
//
// Also what those screens have in common in carrying out a change: showing
// and logging a problem, and drawing a copy's progress as it goes.
#ifndef YAX86_PICOCALC_MANAGE_H
#define YAX86_PICOCALC_MANAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "fs.h"
#include "settings.h"
#include "vm_config.h"
#include "vm_list.h"

typedef struct ManageState {
  // The launcher's: the list is read again once the card changes, and the
  // settings follow a rename or delete of the default VM.
  VMList* vms;
  Settings* settings;
  // Guest RAM, lent before the VM starts, that a clone's files and blank
  // disks are copied through, and how long it is.
  uint8_t* copy_buffer;
  uint32_t copy_buffer_size;
  // Set by a screen that changed the card, before it goes back: the VM to
  // choose once the list is read again, the first if it is not there, and
  // whether to show its page too.
  bool changed;
  char chosen[kVMNameLength + 1];
  bool show_vm;
} ManageState;

// Note that the card changed, with name the VM to choose after, and with
// show_vm, to show its page too.
void ManageNoteChange(ManageState* manage, const char* name, bool show_vm);

// Show why something could not be done to vm, and log it as event.
void ManageShowProblem(
    const char* event, const VMEntry* vm, const char* problem);

// A transfer through the copy buffer that draws its progress as it goes: a
// page titled title, saying what with the KB copied under it. The page
// draws at once, as the loop that would draw it is waiting for the copy.
// title and what must outlive the copy.
FSTransfer ManageBeginCopy(
    const ManageState* manage, const char* title, const char* what);

// Zero the copy buffer after a copy, leaving it as the guest would find RAM
// at power on, whatever it held.
void ManageEndCopy(const ManageState* manage);

#endif  // YAX86_PICOCALC_MANAGE_H
