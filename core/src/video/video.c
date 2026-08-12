#ifndef YAX86_IMPLEMENTATION
#include "internal.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Power-on 6845 register values for the IBM Monochrome Display.
static const uint8_t kDefaultMDARegisters[kNumCRTCRegisters] = {
    0x61, 0x50, 0x52, 0x0F, 0x19, 0x06, 0x19, 0x19, 0x02,
    0x0D, 0x0B, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// Power-on 6845 register values for the CGA in 80x25 text mode, matching the
// values GLaBIOS programs for that mode.
static const uint8_t kDefaultCGARegisters[kNumCRTCRegisters] = {
    0x71, 0x50, 0x5A, 0x0A, 0x1F, 0x06, 0x19, 0x1C, 0x02,
    0x07, 0x06, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

enum {
  // Value of bits 6-5 of the cursor start register that disables the cursor.
  kCRTCCursorDisabled = 0x20,
  // Mask of bits 6-5 of the cursor start register.
  kCRTCCursorModeMask = 0x60,
  // Mask of the scan line select bits in the cursor start and end registers.
  kCRTCCursorScanLineMask = 0x1F,

  // The 6845 latches only the low five bits of the register index.
  kCRTCRegisterIndexMask = 0x1F,

  // Value returned for reads that decode to nothing.
  kVideoUnmappedPortValue = 0xFF,
};

const VideoAdapterMetadata* VideoGetAdapterMetadata(const VideoState* video) {
  return &kVideoAdapterMetadata[video->adapter];
}

// ============================================================================
// Video RAM
// ============================================================================

YAX86_PRIVATE uint8_t VideoReadVRAMByte(VideoState* video, uint32_t address) {
  if (!video->config || !video->config->read_vram_byte) {
    return kVideoUnmappedPortValue;
  }
  // VRAM is aliased throughout the adapter's window, so an address past the end
  // wraps around rather than reading nothing. The renderer relies on this when
  // the 6845 start address pushes it past the end of VRAM.
  //
  // Every adapter's VRAM size is a power of two, so the wrap is a mask rather
  // than a remainder. The size is only known at run time, so a remainder would
  // compile to a hardware divide in the middle of the render loop - and the
  // Cortex-M0+ this targets has no divide instruction at all.
  uint32_t vram_size = VideoGetAdapterMetadata(video)->vram_size;
  return video->config->read_vram_byte(video, address & (vram_size - 1));
}

YAX86_PRIVATE void VideoWriteVRAMByte(
    VideoState* video, uint32_t address, uint8_t value) {
  if (!video->config || !video->config->write_vram_byte) {
    return;
  }
  uint32_t vram_size = VideoGetAdapterMetadata(video)->vram_size;
  video->config->write_vram_byte(video, address & (vram_size - 1), value);
}

YAX86_PRIVATE void VideoWritePixel(
    VideoState* video, Position position, RGB rgb) {
  if (!video->config || !video->config->write_pixel) {
    return;
  }
  video->config->write_pixel(video, position, rgb);
}

uint8_t VideoReadVRAM(VideoState* video, uint32_t address) {
  if (address >= VideoGetAdapterMetadata(video)->vram_size) {
    return kVideoUnmappedPortValue;
  }
  return VideoReadVRAMByte(video, address);
}

void VideoWriteVRAM(VideoState* video, uint32_t address, uint8_t value) {
  if (address >= VideoGetAdapterMetadata(video)->vram_size) {
    return;
  }
  VideoWriteVRAMByte(video, address, value);
}

// ============================================================================
// Initialization
// ============================================================================

void VideoInit(VideoState* video, VideoConfig* config) {
  static const VideoState kEmptyVideoState = {0};
  *video = kEmptyVideoState;
  video->config = config;
  video->adapter = config && config->adapter == kVideoAdapterCGA
                       ? kVideoAdapterCGA
                       : kVideoAdapterMDA;

  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(video);
  const uint8_t* default_registers = video->adapter == kVideoAdapterCGA
                                         ? kDefaultCGARegisters
                                         : kDefaultMDARegisters;
  for (uint8_t i = 0; i < kNumCRTCRegisters; ++i) {
    video->registers[i] = default_registers[i];
  }
  video->control_register = adapter->default_control_register;

  for (uint32_t i = 0; i < adapter->vram_size; i += 2) {
    VideoWriteVRAMByte(video, i, ' ');
    VideoWriteVRAMByte(video, i + 1, 0x07 /* default attr */);
  }
}

// ============================================================================
// Video mode
// ============================================================================

VideoMode VideoGetMode(const VideoState* video) {
  if (video->adapter == kVideoAdapterMDA) {
    // The MDA has only one mode.
    return kVideoModeMDAText80x25;
  }
  // Derive the CGA mode from the mode control register, matching the values
  // the BIOS writes for each of its INT 10h modes.
  //
  // The bits are tested in the order a real card resolves them: the high
  // resolution bit wins over the graphics bits, and the graphics bit over the
  // high resolution graphics bit. The BIOS never writes a combination where
  // the order matters, but software that ORs a bit into a saved mode byte can,
  // and this is the order 86Box's renderer decodes them in.
  if (video->control_register & kVideoControlHighResolution) {
    return video->control_register & kVideoControlBlackAndWhite
               ? kVideoModeCGAText80x25Mono
               : kVideoModeCGAText80x25Color;
  }
  if (!(video->control_register & kVideoControlGraphics)) {
    return video->control_register & kVideoControlBlackAndWhite
               ? kVideoModeCGAText40x25Mono
               : kVideoModeCGAText40x25Color;
  }
  if (video->control_register & kVideoControlHighResolutionGraphics) {
    return kVideoModeCGAGraphics640x200;
  }
  return video->control_register & kVideoControlBlackAndWhite
             ? kVideoModeCGAGraphics320x200Alt
             : kVideoModeCGAGraphics320x200;
}

const VideoModeMetadata* VideoGetModeMetadata(const VideoState* video) {
  return &kVideoModeMetadata[VideoGetMode(video)];
}

// ============================================================================
// 6845 CRT controller
// ============================================================================

YAX86_PRIVATE uint16_t VideoGetStartAddress(const VideoState* video) {
  return (uint16_t)(video->registers[kCRTCRegisterStartAddressH] << 8) |
         video->registers[kCRTCRegisterStartAddressL];
}

YAX86_PRIVATE uint16_t VideoGetCursorAddress(const VideoState* video) {
  return (uint16_t)(video->registers[kCRTCRegisterCursorH] << 8) |
         video->registers[kCRTCRegisterCursorL];
}

YAX86_PRIVATE bool VideoIsCursorEnabled(const VideoState* video) {
  return (video->registers[kCRTCRegisterCursorStart] & kCRTCCursorModeMask) !=
         kCRTCCursorDisabled;
}

YAX86_PRIVATE uint8_t VideoGetCursorStartScanLine(const VideoState* video) {
  return video->registers[kCRTCRegisterCursorStart] & kCRTCCursorScanLineMask;
}

YAX86_PRIVATE uint8_t VideoGetCursorEndScanLine(const VideoState* video) {
  return video->registers[kCRTCRegisterCursorEnd] & kCRTCCursorScanLineMask;
}

YAX86_PRIVATE bool VideoIsCursorBlinkOn(const VideoState* video) {
  return (video->frames / kVideoFramesPerCursorBlinkPhase) % 2 == 0;
}

YAX86_PRIVATE bool VideoIsTextBlinkOn(const VideoState* video) {
  return (video->frames / kVideoFramesPerTextBlinkPhase) % 2 == 0;
}

// ============================================================================
// Retrace timing
// ============================================================================

// Advances a software model of the CRT beam position by the given number of
// CPU cycles. This is what status port reads (below) derive retrace timing
// from.
//
// The beam advances one scan line every kMDACyclesPerScanLine cycles, but
// callers report cycles in whatever amount the CPU just consumed, which
// rarely divides evenly. scan_line_cycles banks the remainder as credit
// toward the next scan line; the loop pays it off a scan line at a time,
// looping rather than branching once since a single call can be worth more
// than one scan line. scan_line wraps at the adapter's scan_lines_per_frame,
// incrementing frames, which VideoIsCursorBlinkOn() and VideoIsTextBlinkOn()
// use to derive their blink phases.
void VideoTick(VideoState* video, uint16_t cycles) {
  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(video);
  video->scan_line_cycles += cycles;
  while (video->scan_line_cycles >= adapter->cycles_per_scan_line) {
    video->scan_line_cycles -= adapter->cycles_per_scan_line;
    if (++video->scan_line >= adapter->scan_lines_per_frame) {
      video->scan_line = 0;
      ++video->frames;
    }
  }
}

// The value the status port reads back, computed from where the CRT beam
// currently is.
static uint8_t VideoGetStatus(const VideoState* video) {
  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(video);
  bool in_vertical_retrace = video->scan_line >= adapter->displayed_scan_lines;
  bool in_horizontal_retrace =
      video->scan_line_cycles >= adapter->display_cycles_per_scan_line;

  // No light pen is emulated, so its switch always reads as off.
  uint8_t status = kVideoStatusLightPenSwitchOff;
  if (in_vertical_retrace || in_horizontal_retrace) {
    status |= kVideoStatusDisplayDisabled;
  }
  if (in_vertical_retrace) {
    status |= kVideoStatusVerticalRetrace;
  }
  // Note that the unused high bits are deliberately left clear rather than
  // floating high as they do on a real card: GLaBIOS probes bit 7 of the MDA
  // status port to detect a Hercules adapter.
  return status;
}

// ============================================================================
// I/O ports
// ============================================================================

// What an I/O port in the adapter's range does.
typedef enum VideoPortFunction {
  // Port is not decoded by the adapter.
  kVideoPortNone = 0,
  // 6845 register index.
  kVideoPortRegisterIndex,
  // 6845 register data.
  kVideoPortRegisterData,
  // Mode control register.
  kVideoPortControl,
  // CGA color select register.
  kVideoPortColorSelect,
  // Status register.
  kVideoPortStatus,
} VideoPortFunction;

// Decode an I/O port within the adapter's range.
static VideoPortFunction VideoDecodePort(
    const VideoState* video, uint16_t port) {
  if (video->adapter == kVideoAdapterMDA) {
    switch (port) {
      case kMDAPortRegisterIndex:
        return kVideoPortRegisterIndex;
      case kMDAPortRegisterData:
        return kVideoPortRegisterData;
      case kMDAPortControl:
        return kVideoPortControl;
      case kMDAPortStatus:
        return kVideoPortStatus;
      default:
        return kVideoPortNone;
    }
  }

  // The CGA only decodes the low four address bits, so the 6845 index and data
  // registers are aliased across 3D0 to 3D7.
  uint16_t offset = port - kCGAPortStart;
  if (offset < 8) {
    return offset & 1 ? kVideoPortRegisterData : kVideoPortRegisterIndex;
  }
  switch (port) {
    case kCGAPortControl:
      return kVideoPortControl;
    case kCGAPortColorSelect:
      return kVideoPortColorSelect;
    case kCGAPortStatus:
      return kVideoPortStatus;
    default:
      // The light pen strobe ports respond but do nothing, as no light pen is
      // emulated.
      return kVideoPortNone;
  }
}

uint8_t VideoReadPort(VideoState* video, uint16_t port) {
  switch (VideoDecodePort(video, port)) {
    case kVideoPortRegisterIndex:
      return video->selected_register;
    case kVideoPortRegisterData:
      if (video->selected_register < kNumCRTCRegisters) {
        return video->registers[video->selected_register];
      }
      return kVideoUnmappedPortValue;
    case kVideoPortControl:
      return video->control_register;
    case kVideoPortColorSelect:
      return video->color_select_register;
    case kVideoPortStatus:
      return VideoGetStatus(video);
    default:
      return kVideoUnmappedPortValue;
  }
}

void VideoWritePort(VideoState* video, uint16_t port, uint8_t value) {
  switch (VideoDecodePort(video, port)) {
    case kVideoPortRegisterIndex:
      video->selected_register = value & kCRTCRegisterIndexMask;
      break;
    case kVideoPortRegisterData:
      if (video->selected_register < kNumCRTCRegisters) {
        video->registers[video->selected_register] = value;
      }
      break;
    case kVideoPortControl:
      video->control_register = value;
      break;
    case kVideoPortColorSelect:
      video->color_select_register = value;
      break;
    default:
      // The status port is read only.
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

  if (!(video->control_register & kVideoControlVideoEnable)) {
    // The video signal is disabled, so the display is blank. The BIOS leaves it
    // this way while it reprograms the 6845.
    const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(video);
    RGB blank = video->adapter == kVideoAdapterCGA
                    ? video->config->cga_palette[0]
                    : video->config->background;
    for (uint16_t y = 0; y < adapter->frame_buffer_height; ++y) {
      for (uint16_t x = 0; x < adapter->frame_buffer_width; ++x) {
        Position pixel_pos = {.x = x, .y = y};
        VideoWritePixel(video, pixel_pos, blank);
      }
    }
    return;
  }

  if (video->adapter == kVideoAdapterCGA) {
    CGARenderScreen(video);
  } else {
    MDARenderScreen(video);
  }
}
