#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

YAX86_PRIVATE void InitDisplayText(BIOSState* bios) {
  // TODO: Set display to text mode.

  // Initialize the text mode framebuffer to a blank state.
  for (int i = 0; i < kTextModeFramebufferSize; i += 2) {
    bios->text_framebuffer[i] = 0;
    bios->text_framebuffer[i + 1] = 0x07;
  }
}

uint8_t ReadDisplayTextByte(BIOSState* bios, uint32_t address) {
  if (address >= kTextModeFramebufferSize) {
    return 0xFF;  // Out of bounds, return garbage data.
  }
  return bios->text_framebuffer[address];
}

void WriteDisplayTextByte(BIOSState* bios, uint32_t address, uint8_t value) {
  if (address >= kTextModeFramebufferSize) {
    return;
  }
  bios->text_framebuffer[address] = value;
}
