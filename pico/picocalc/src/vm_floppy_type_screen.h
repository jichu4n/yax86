// A floppy drive's mechanism type, from a drive's configuration: 360KB,
// 720KB, 1.2MB or 1.44MB.
#ifndef YAX86_PICOCALC_VM_FLOPPY_TYPE_SCREEN_H
#define YAX86_PICOCALC_VM_FLOPPY_TYPE_SCREEN_H

#include "core/fdc.h"
#include "screen.h"

// Show the floppy drive types, with *chosen selected. Choosing one writes it
// to *chosen before the screen goes back. chosen must outlive the screen.
ScreenResult VMFloppyTypeScreenOpen(YFDCDriveType* chosen);

#endif  // YAX86_PICOCALC_VM_FLOPPY_TYPE_SCREEN_H
