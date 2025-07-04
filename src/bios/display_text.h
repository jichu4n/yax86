#ifndef YAX86_BIOS_DISPLAY_TEXT_H
#define YAX86_BIOS_DISPLAY_TEXT_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"

// Initialize the display.
void InitDisplayText(BIOSState* bios);

// Read a byte from the display text buffer.
uint8_t ReadDisplayTextByte(BIOSState* bios, uint32_t address);
// Write a byte to the display text buffer.
void WriteDisplayTextByte(BIOSState* bios, uint32_t address, uint8_t value);

#endif  // YAX86_IMPLEMENTATION

#endif  // YAX86_BIOS_DISPLAY_TEXT_H
