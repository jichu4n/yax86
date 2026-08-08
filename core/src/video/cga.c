#ifndef YAX86_IMPLEMENTATION
#include "fonts.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Default CGA state (80x25 text mode, mode 3).
static const CGAState kDefaultCGAState = {
    .config = NULL,
    .registers =
        {
            0x71,
            0x50,
            0x5A,
            0x0A,
            0x1F,
            0x06,
            0x19,
            0x1C,
            0x02,
            0x07,
            0x06,
            0x07,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
        },
    .selected_register = 0,
    // 80-column, video enable, blink enable
    .mode_control = 0x29,
    .color_select = 0x00,
    .status = 0x00,
};

static inline uint8_t CGAReadVRAMByte(CGAState* cga, uint32_t address) {
  if (cga->config && cga->config->read_vram_byte &&
      address < kCGAVRAMSize) {
    return cga->config->read_vram_byte(cga, address);
  }
  return 0xFF;
}

static inline void CGAWriteVRAMByte(
    CGAState* cga, uint32_t address, uint8_t value) {
  if (cga->config && cga->config->write_vram_byte &&
      address < kCGAVRAMSize) {
    cga->config->write_vram_byte(cga, address, value);
  }
}

// Initialize CGA state with the provided configuration.
void CGAInit(CGAState* cga, CGAConfig* config) {
  *cga = kDefaultCGAState;
  cga->config = config;

  for (uint32_t i = 0; i < kCGAVRAMSize; i += 2) {
    CGAWriteVRAMByte(cga, i, ' ');
    CGAWriteVRAMByte(cga, i + 1, 0x07 /* default attr */);
  }
}

uint8_t CGAReadVRAM(CGAState* cga, uint32_t address) {
  return CGAReadVRAMByte(cga, address);
}

void CGAWriteVRAM(CGAState* cga, uint32_t address, uint8_t value) {
  CGAWriteVRAMByte(cga, address, value);
}

uint8_t CGAReadPort(CGAState* cga, uint16_t port) {
  switch (port) {
    case kCGAPortRegisterIndex:
      return cga->selected_register;
    case kCGAPortRegisterData:
      if (cga->selected_register < kCGANumRegisters) {
        return cga->registers[cga->selected_register];
      }
      return 0xFF;
    case kCGAPortStatus:
      // Toggle display enable (bit 0) and vsync (bit 3) on each read to
      // simulate retrace timing. This prevents DOS programs from hanging
      // while polling for retrace.
      cga->status ^= (kCGAStatusDisplayEnable | kCGAStatusVSync);
      return cga->status;
    default:
      return 0xFF;
  }
}

void CGAWritePort(CGAState* cga, uint16_t port, uint8_t value) {
  switch (port) {
    case kCGAPortRegisterIndex:
      cga->selected_register = value;
      break;
    case kCGAPortRegisterData:
      if (cga->selected_register < kCGANumRegisters) {
        cga->registers[cga->selected_register] = value;
      }
      break;
    case kCGAPortModeControl:
      cga->mode_control = value;
      break;
    case kCGAPortColorSelect:
      cga->color_select = value;
      break;
    default:
      break;
  }
}

// Get the metadata for the currently active CGA video mode, derived from
// the mode control register.
const VideoModeMetadata* CGAGetCurrentModeMetadata(const CGAState* cga) {
  if (cga->mode_control & kCGAModeControlGraphics) {
    if (cga->mode_control & kCGAModeControlHiRes) {
      // Mode 6: 640x200 2-color
      return &kCGAModeMetadata[6];
    }
    if (cga->mode_control & kCGAModeControlBW) {
      // Mode 5: 320x200 4-color (no color burst)
      return &kCGAModeMetadata[5];
    }
    // Mode 4: 320x200 4-color
    return &kCGAModeMetadata[4];
  }
  if (cga->mode_control & kCGAModeControl80Column) {
    if (cga->mode_control & kCGAModeControlBW) {
      // Mode 2: 80x25 text (BW)
      return &kCGAModeMetadata[2];
    }
    // Mode 3: 80x25 text (color)
    return &kCGAModeMetadata[3];
  }
  if (cga->mode_control & kCGAModeControlBW) {
    // Mode 0: 40x25 text (BW)
    return &kCGAModeMetadata[0];
  }
  // Mode 1: 40x25 text (color)
  return &kCGAModeMetadata[1];
}

// Render a single character in CGA text mode.
static void CGARenderTextChar(
    CGAState* cga, const VideoModeMetadata* metadata,
    TextPosition char_pos) {
  // Compute VRAM address accounting for start address register (R12/R13).
  uint16_t start_address =
      ((uint16_t)cga->registers[kMDARegisterStartAddressH] << 8) |
      cga->registers[kMDARegisterStartAddressL];
  uint32_t char_address =
      ((start_address +
        char_pos.row * metadata->columns + char_pos.col) *
       2) %
      kCGAVRAMSize;
  uint8_t char_value = CGAReadVRAMByte(cga, char_address);
  uint8_t attr_value = CGAReadVRAMByte(cga, char_address + 1);
  const uint8_t* char_bitmap = kFontCGA8x8Bitmap[char_value];

  // Decode attribute byte.
  // Bits 0-2: foreground color
  // Bit 3: foreground intensity
  // Bits 4-6: background color
  // Bit 7: blink (if blink enabled) or background intensity
  uint8_t fg_index = attr_value & 0x0F;
  uint8_t bg_index;
  if (cga->mode_control & kCGAModeControlBlink) {
    // Blink mode: bit 7 controls blink, background is 3 bits (0-7).
    bg_index = (attr_value >> 4) & 0x07;
  } else {
    // No blink: bit 7 is background intensity, background is 4 bits (0-15).
    bg_index = (attr_value >> 4) & 0x0F;
  }

  const RGB* foreground = &kCGAPalette[fg_index];
  const RGB* background = &kCGAPalette[bg_index];

  Position origin_pixel_pos = {
      .x = (uint16_t)(char_pos.col * metadata->char_width),
      .y = (uint16_t)(char_pos.row * metadata->char_height),
  };
  for (uint8_t y = 0; y < metadata->char_height; ++y) {
    uint8_t row_bitmap = char_bitmap[y];
    for (uint8_t x = 0; x < metadata->char_width; ++x) {
      Position pixel_pos = {
          .x = (uint16_t)(origin_pixel_pos.x + x),
          .y = (uint16_t)(origin_pixel_pos.y + y),
      };
      bool is_foreground =
          (row_bitmap & (1 << (metadata->char_width - 1 - x))) != 0;
      const RGB* pixel_rgb = is_foreground ? foreground : background;
      cga->config->write_pixel(cga, pixel_pos, *pixel_rgb);
    }
  }
}

// Render CGA text mode (modes 0-3).
static void CGARenderText(CGAState* cga) {
  const VideoModeMetadata* metadata = CGAGetCurrentModeMetadata(cga);
  for (uint8_t row = 0; row < metadata->rows; ++row) {
    for (uint8_t col = 0; col < metadata->columns; ++col) {
      TextPosition char_pos = {.col = col, .row = row};
      CGARenderTextChar(cga, metadata, char_pos);
    }
  }
}

enum {
  // Offset to odd scanline bank in CGA graphics modes.
  kCGAGraphicsOddBankOffset = 0x2000,
  // Bytes per scanline in 320x200 mode (80 bytes = 320 pixels / 4 pixels per
  // byte).
  kCGAGraphicsBytesPerLine320 = 80,
  // Bytes per scanline in 640x200 mode (80 bytes = 640 pixels / 8 pixels per
  // byte).
  kCGAGraphicsBytesPerLine640 = 80,
};

// Get the 4-color palette for CGA 320x200 graphics modes.
// Palette is determined by color_select register bits 4 and 5.
static inline void CGAGetGraphicsPalette(
    const CGAState* cga, const RGB* palette_out[4]) {
  // Pixel value 0 is always the background/border color.
  uint8_t bg_color = cga->color_select & 0x0F;
  palette_out[0] = &kCGAPalette[bg_color];

  // Palette selection from color_select bits 4 and 5.
  bool intensity = (cga->color_select & 0x10) != 0;
  bool palette_select = (cga->color_select & 0x20) != 0;

  if (palette_select) {
    // Palette 0: Green, Red, Brown/Yellow
    palette_out[1] = &kCGAPalette[intensity ? 10 : 2];
    palette_out[2] = &kCGAPalette[intensity ? 12 : 4];
    palette_out[3] = &kCGAPalette[intensity ? 14 : 6];
  } else {
    // Palette 1: Cyan, Magenta, White
    palette_out[1] = &kCGAPalette[intensity ? 11 : 3];
    palette_out[2] = &kCGAPalette[intensity ? 13 : 5];
    palette_out[3] = &kCGAPalette[intensity ? 15 : 7];
  }
}

// Render CGA 320x200 4-color graphics mode (modes 4/5).
static void CGARenderGraphics320(CGAState* cga) {
  const RGB* palette[4];
  CGAGetGraphicsPalette(cga, palette);

  for (uint16_t y = 0; y < 200; ++y) {
    // Interlaced layout: even scanlines at bank 0, odd scanlines at bank 1.
    uint32_t bank_offset = (y & 1) ? kCGAGraphicsOddBankOffset : 0;
    uint32_t line_offset = (y >> 1) * kCGAGraphicsBytesPerLine320;

    for (uint16_t x = 0; x < 320; ++x) {
      uint32_t byte_address = bank_offset + line_offset + (x >> 2);
      uint8_t byte_value = CGAReadVRAMByte(cga, byte_address);
      // 2 bits per pixel, MSB first.
      uint8_t pixel_shift = (uint8_t)(6 - ((x & 3) * 2));
      uint8_t color_index = (byte_value >> pixel_shift) & 0x03;
      Position pos = {.x = x, .y = y};
      cga->config->write_pixel(cga, pos, *palette[color_index]);
    }
  }
}

// Render CGA 640x200 2-color graphics mode (mode 6).
static void CGARenderGraphics640(CGAState* cga) {
  // Foreground color from color_select bits 0-3.
  uint8_t fg_color = cga->color_select & 0x0F;
  const RGB* foreground = &kCGAPalette[fg_color];
  // Background is always black.
  const RGB* background = &kCGAPalette[0];

  for (uint16_t y = 0; y < 200; ++y) {
    uint32_t bank_offset = (y & 1) ? kCGAGraphicsOddBankOffset : 0;
    uint32_t line_offset = (y >> 1) * kCGAGraphicsBytesPerLine640;

    for (uint16_t x = 0; x < 640; ++x) {
      uint32_t byte_address = bank_offset + line_offset + (x >> 3);
      uint8_t byte_value = CGAReadVRAMByte(cga, byte_address);
      // 1 bit per pixel, MSB first.
      uint8_t pixel_bit = (uint8_t)(7 - (x & 7));
      bool is_set = (byte_value >> pixel_bit) & 0x01;
      Position pos = {.x = x, .y = y};
      cga->config->write_pixel(cga, pos, is_set ? *foreground : *background);
    }
  }
}

// Render the current display. Invokes the write_pixel callback to do the
// actual pixel rendering.
void CGARender(CGAState* cga) {
  if (!cga->config || !cga->config->write_pixel) {
    return;
  }

  // If video is disabled, don't render.
  if (!(cga->mode_control & kCGAModeControlVideoEnable)) {
    return;
  }

  if (cga->mode_control & kCGAModeControlGraphics) {
    if (cga->mode_control & kCGAModeControlHiRes) {
      CGARenderGraphics640(cga);
    } else {
      CGARenderGraphics320(cga);
    }
  } else {
    CGARenderText(cga);
  }
}
