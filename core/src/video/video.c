#ifndef YAX86_IMPLEMENTATION
#include "internal.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Power-on 6845 register values for the IBM Monochrome Display.
static const uint8_t kDefaultMDARegisters[kNumCRTCRegisters] = {
    0x61, 0x50, 0x52, 0x0F, 0x19, 0x06, 0x19, 0x19, 0x02,
    0x0D, 0x0B, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

enum {
  // High resolution mode, video enable, blink enable.
  kMDADefaultControlRegister = 0x29,

  // Value returned for reads that decode to nothing.
  kVideoUnmappedPortValue = 0xFF,
};

// ============================================================================
// Video RAM
// ============================================================================

YAX86_PRIVATE uint8_t VideoReadVRAMByte(VideoState* video, uint32_t address) {
  if (!video->config || !video->config->read_vram_byte ||
      address >= kMDAModeMetadata.vram_size) {
    return kVideoUnmappedPortValue;
  }
  return video->config->read_vram_byte(video, address);
}

YAX86_PRIVATE void VideoWriteVRAMByte(
    VideoState* video, uint32_t address, uint8_t value) {
  if (!video->config || !video->config->write_vram_byte ||
      address >= kMDAModeMetadata.vram_size) {
    return;
  }
  video->config->write_vram_byte(video, address, value);
}

YAX86_PRIVATE void VideoWritePixel(
    VideoState* video, Position position, RGB rgb) {
  if (!video->config || !video->config->write_pixel) {
    return;
  }
  video->config->write_pixel(video, position, rgb);
}

uint8_t VideoReadVRAM(VideoState* video, uint32_t address) {
  return VideoReadVRAMByte(video, address);
}

void VideoWriteVRAM(VideoState* video, uint32_t address, uint8_t value) {
  VideoWriteVRAMByte(video, address, value);
}

// ============================================================================
// Initialization
// ============================================================================

void VideoInit(VideoState* video, VideoConfig* config) {
  static const VideoState kEmptyVideoState = {0};
  *video = kEmptyVideoState;
  video->config = config;

  for (uint8_t i = 0; i < kNumCRTCRegisters; ++i) {
    video->registers[i] = kDefaultMDARegisters[i];
  }
  video->control_register = kMDADefaultControlRegister;

  for (uint32_t i = 0; i < kMDAModeMetadata.vram_size; i += 2) {
    VideoWriteVRAMByte(video, i, ' ');
    VideoWriteVRAMByte(video, i + 1, 0x07 /* default attr */);
  }
}

// ============================================================================
// I/O ports
// ============================================================================

uint8_t VideoReadPort(VideoState* video, uint16_t port) {
  switch (port) {
    case kMDAPortRegisterIndex:
      return video->selected_register;
    case kMDAPortRegisterData:
      if (video->selected_register < kNumCRTCRegisters) {
        return video->registers[video->selected_register];
      }
      return kVideoUnmappedPortValue;
    case kMDAPortControl:
      return video->control_register;
    case kMDAPortStatus:
      return video->status_register;
    default:
      return kVideoUnmappedPortValue;
  }
}

void VideoWritePort(VideoState* video, uint16_t port, uint8_t value) {
  switch (port) {
    case kMDAPortRegisterIndex:
      video->selected_register = value;
      break;
    case kMDAPortRegisterData:
      if (video->selected_register < kNumCRTCRegisters) {
        video->registers[video->selected_register] = value;
      }
      break;
    case kMDAPortControl:
      video->control_register = value;
      break;
    case kMDAPortStatus:
      video->status_register = value;
      break;
    default:
      break;
  }
}

// ============================================================================
// Rendering
// ============================================================================

void VideoRender(VideoState* video) {
  if (!video->config || !video->config->write_pixel) {
    return;
  }

  MDARenderScreen(video);
}
