// A floppy drive's mechanism type, from a drive's configuration: 360KB,
// 720KB, 1.2MB or 1.44MB, or Disabled for a drive that can be left out.
#ifndef YAX86_PICOCALC_VM_FLOPPY_TYPE_SCREEN_H
#define YAX86_PICOCALC_VM_FLOPPY_TYPE_SCREEN_H

#include <stdbool.h>

#include "core/fdc.h"
#include "screen.h"

// Show the floppy drive types, with *chosen selected. Choosing one writes it
// to *chosen before the screen goes back. With is_present, for a drive that
// can be left out, Disabled comes first, selected while *is_present is false;
// choosing it clears *is_present, and choosing a type sets it. chosen and
// is_present must outlive the screen.
ScreenResult VMFloppyTypeScreenOpen(YFDCDriveType* chosen, bool* is_present);

#endif  // YAX86_PICOCALC_VM_FLOPPY_TYPE_SCREEN_H
