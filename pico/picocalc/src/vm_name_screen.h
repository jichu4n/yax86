// The name to rename a VM to, or to clone it as, typed in a field: a
// character a name can have adds to it, Backspace takes the last off, Enter
// renames or clones, and Esc goes back.
#ifndef YAX86_PICOCALC_VM_NAME_SCREEN_H
#define YAX86_PICOCALC_VM_NAME_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "manage.h"
#include "screen.h"

// Type a name for the VM at index in manage's list: to rename it, starting
// from its name, or with cloning, to clone it, starting from its name with
// _2 at the end. A name that will not do shows why and stays to be typed
// again; a rename or clone done, or failed, is noted in manage before the
// screen goes back. manage must outlive the screen.
ScreenResult VMNameScreenOpen(ManageState* manage, uint8_t index, bool cloning);

#endif  // YAX86_PICOCALC_VM_NAME_SCREEN_H
