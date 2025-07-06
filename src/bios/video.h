#ifndef YAX86_BIOS_VIDEO_H
#define YAX86_BIOS_VIDEO_H

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"

// Initialize the display in text mode.
extern void InitVideo(BIOSState* bios);

// Read a byte from video RAM.
extern uint8_t ReadVRAMByte(BIOSState* bios, uint32_t address);
// Write a byte to video RAM.
extern void WriteVRAMByte(BIOSState* bios, uint32_t address, uint8_t value);

// Table of video mode metadata, indexed by VideoMode enum values.
extern const VideoModeMetadata kVideoModeMetadataTable[kNumVideoModes];

#endif  // YAX86_IMPLEMENTATION

#endif  // YAX86_BIOS_VIDEO_H
