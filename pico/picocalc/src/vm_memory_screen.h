// Memory, from a VM's configuration: 128K, 256K, 384K, 512K or 640K, with
// "(paged)" on sizes above kPicoGuestSRAMSize.
#ifndef YAX86_PICOCALC_VM_MEMORY_SCREEN_H
#define YAX86_PICOCALC_VM_MEMORY_SCREEN_H

#include <stddef.h>
#include <stdint.h>

#include "screen.h"

enum {
  // Longest label VMMemoryFormat() produces ("640K (paged)", 12 characters),
  // with room for any uint16_t.
  kVMMemoryLabelLength = 23,
};

// Format memory_kb as "<N>K", or "<N>K (paged)" when it exceeds
// kPicoGuestSRAMSize on this board (regardless of whether the PSRAM is up).
void VMMemoryFormat(uint16_t memory_kb, char* buffer, size_t size);

// Show the memory sizes, with the preset matching or closest to *chosen
// selected. Choosing one writes it in KB to *chosen before the screen goes
// back. chosen must outlive the screen.
ScreenResult VMMemoryScreenOpen(uint16_t* chosen);

#endif  // YAX86_PICOCALC_VM_MEMORY_SCREEN_H
