#ifndef YAX86_IMPLEMENTATION
#include "display_text_mode.h"

#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

YAX86_PRIVATE uint8_t
ReadTextModeFramebufferByte(BIOSState* bios, uint32_t address) {
  if (address >= kTextModeFramebufferSize) {
    return 0xFF;
  }
  return bios->text_mode_framebuffer[address];
}

YAX86_PRIVATE void WriteTextModeFramebufferByte(
    BIOSState* bios, uint32_t address, uint8_t value) {
  if (address >= kTextModeFramebufferSize) {
    return;
  }
  bios->text_mode_framebuffer[address] = value;
}

YAX86_PRIVATE void InitTextMode(BIOSState* bios) {
  // TODO: Set display to text mode.
  MemoryRegion text_mode_framebuffer = {
      .region = kMemoryRegionTextModeFramebuffer,
      .start = kTextModeFramebufferAddress,
      .size = kTextModeFramebufferSize,
      .read_memory_byte = ReadTextModeFramebufferByte,
      .write_memory_byte = WriteTextModeFramebufferByte,
  };
  MemoryRegionsAppend(&bios->memory_regions, &text_mode_framebuffer);

  // Initialize the framebuffer to a blank state.
  for (int i = 0; i < kTextModeFramebufferSize; i += 2) {
    bios->text_mode_framebuffer[i] = ' ';
    bios->text_mode_framebuffer[i + 1] = 0x07;
  }
}