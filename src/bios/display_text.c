#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
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
