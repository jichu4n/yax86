#ifndef YAX86_BIOS_DISPLAY_TEXT_H
#define YAX86_BIOS_DISPLAY_TEXT_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"

// Initialize the display in text mode.
extern void InitTextMode(BIOSState* bios);

// Read a byte from the text framebuffer.
extern uint8_t ReadTextModeFramebufferByte(BIOSState* bios, uint32_t address);
// Write a byte to the text framebuffer.
extern void WriteTextModeFramebufferByte(
    BIOSState* bios, uint32_t address, uint8_t value);

#endif  // YAX86_IMPLEMENTATION

#endif  // YAX86_BIOS_DISPLAY_TEXT_H
