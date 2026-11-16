// Memory, from a VM's configuration: 128K, 256K, 512K or 640K.
#ifndef YAX86_PICOCALC_VM_MEMORY_SCREEN_H
#define YAX86_PICOCALC_VM_MEMORY_SCREEN_H

#include <stdint.h>

#include "screen.h"

// Show the memory sizes, with the preset matching or closest to *chosen
// selected. Choosing one writes it in KB to *chosen before the screen goes
// back. chosen must outlive the screen.
ScreenResult VMMemoryScreenOpen(uint16_t* chosen);

#endif  // YAX86_PICOCALC_VM_MEMORY_SCREEN_H
