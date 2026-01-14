#ifndef YAX86_IMPLEMENTATION
#include "fonts.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Default MDA state.
static const MDAState kDefaultMDAState = {
    .config = NULL,
    .registers =
        {
            0x61,
            0x50,
            0x52,
            0x0F,
            0x19,
            0x06,
            0x19,
            0x19,
            0x02,
            0x0D,
            0x0B,
            0x0C,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
        },
    .selected_register = 0,
    // high resolution mode, video enable, blink enable
    .control_port = 0x29,
    .status_port = 0x00,
};

static inline uint8_t ReadVRAMByte(MDAState* mda, uint32_t address) {
  if (mda->config && mda->config->read_vram_byte &&
      address < kMDAModeMetadata.vram_size) {
    return mda->config->read_vram_byte(mda, address);
  }
  return 0xFF;
}

static inline void WriteVRAMByte(
    MDAState* mda, uint32_t address, uint8_t value) {
  if (mda->config && mda->config->write_vram_byte &&
      address < kMDAModeMetadata.vram_size) {
    mda->config->write_vram_byte(mda, address, value);
  }
}

// Initialize MDA state with the provided configuration.
void MDAInit(MDAState* mda, MDAConfig* config) {
  *mda = kDefaultMDAState;
  mda->config = config;

  for (uint32_t i = 0; i < kMDAModeMetadata.vram_size; i += 2) {
    WriteVRAMByte(mda, i, ' ');
    WriteVRAMByte(mda, i + 1, 0x07 /* default attr */);
  }
}

uint8_t MDAReadVRAM(MDAState* mda, uint32_t address) {
  return ReadVRAMByte(mda, address);
}

void MDAWriteVRAM(MDAState* mda, uint32_t address, uint8_t value) {
  WriteVRAMByte(mda, address, value);
}

uint8_t MDAReadPort(MDAState* mda, uint16_t port) {
  switch (port) {
    case kMDAPortRegisterIndex:
      return mda->selected_register;
    case kMDAPortRegisterData:
      if (mda->selected_register < kMDANumRegisters) {
        return mda->registers[mda->selected_register];
      }
      return 0xFF;
    case kMDAPortControl:
      return mda->control_port;
    case kMDAPortStatus:
      return mda->status_port;
    default:
      return 0xFF;
  }
}

void MDAWritePort(MDAState* mda, uint16_t port, uint8_t value) {
  switch (port) {
    case kMDAPortRegisterIndex:
      mda->selected_register = value;
      break;
    case kMDAPortRegisterData:
      if (mda->selected_register < kMDANumRegisters) {
        mda->registers[mda->selected_register] = value;
      }
      break;
    case kMDAPortControl:
      mda->control_port = value;
      break;
    case kMDAPortStatus:
      mda->status_port = value;
      break;
    default:
      break;
  }
}

enum {
  // Position of underline in MDA text mode.
  kMDAUnderlinePosition = 12,
};

// Write a character to display in MDA text mode. For the attribute byte, we
// only support the officially documented combinations of values.
//
// Attribute byte structure:
//   - Bit 7: blink (0 = normal, 1 = blink)
//   - Bits 6-4: background
//   - Bit 3: intense foreground (0 = normal, 1 = intense)
//   - Bits 2-0: foreground
//
// Valid MDA character background and foreground attribute combinations:
//   - Normal: background = 000, foreground = 111
//   - Inverse video: background = 111, foreground = 000
//   - Invisible: background = 000, foreground = 000
//   - Underline: background = 000, foreground = 001
//
// Other combinations are undefined, but we will treat them as normal.
// TODO: Support blinking.
static void MDAWriteChar(MDAState* mda, TextPosition char_pos) {
  uint32_t char_address =
      (char_pos.row * kMDAModeMetadata.columns + char_pos.col) *
      2;  // Each character takes 2 bytes (char + attr).
  uint8_t char_value = ReadVRAMByte(mda, char_address);
  uint8_t attr_value = ReadVRAMByte(mda, char_address + 1);
  const uint16_t* char_bitmap = kFontMDA9x14Bitmap[char_value];

  const RGB* foreground;
  const RGB* background;
  bool underline = false;

  bool intense = ((attr_value >> 3) & 0x01) != 0;
  uint8_t background_attr = (attr_value >> 4) & 0x07;
  uint8_t foreground_attr = attr_value & 0x07;
  if (background_attr == 0x00 && foreground_attr == 0x07) {
    // Normal video mode.
    foreground =
        intense ? &mda->config->intense_foreground : &mda->config->foreground;
    background = &mda->config->background;
  } else if (background_attr == 0x07 && foreground_attr == 0x00) {
    // Inverse video mode.
    foreground = &mda->config->background;
    background = &mda->config->foreground;
  } else if (background_attr == 0x00 && foreground_attr == 0x00) {
    // Invisible mode.
    foreground = &mda->config->background;
    background = &mda->config->background;
  } else if (background_attr == 0x00 && foreground_attr == 0x01) {
    // Underline mode.
    underline = true;
    foreground =
        intense ? &mda->config->intense_foreground : &mda->config->foreground;
    background = &mda->config->background;
  } else {
    // Other combinations are treated as normal.
    foreground =
        intense ? &mda->config->intense_foreground : &mda->config->foreground;
    background = &mda->config->background;
  }

  const VideoModeMetadata* metadata = &kMDAModeMetadata;
  Position origin_pixel_pos = {
      .x = char_pos.col * metadata->char_width,
      .y = char_pos.row * metadata->char_height,
  };
  for (uint8_t y = 0; y < metadata->char_height; ++y) {
    uint16_t row_bitmap;
    // If underline, set entire underline row to foreground color.
    if (y == kMDAUnderlinePosition && underline) {
      row_bitmap = 0xFFFF;
    } else {
      row_bitmap = char_bitmap[y];
    }
    for (uint8_t x = 0; x < metadata->char_width; ++x) {
      Position pixel_pos = {
          .x = origin_pixel_pos.x + x,
          .y = origin_pixel_pos.y + y,
      };
      bool is_foreground =
          (row_bitmap & (1 << (metadata->char_width - 1 - x))) != 0;
      const RGB* pixel_rgb = is_foreground ? foreground : background;
      mda->config->write_pixel(mda, pixel_pos, *pixel_rgb);
    }
  }
}

// Render the current display. Invokes the write_pixel callback to do the actual
// pixel rendering.
void MDARender(MDAState* mda) {
  for (uint8_t row = 0; row < kMDAModeMetadata.rows; ++row) {
    for (uint8_t col = 0; col < kMDAModeMetadata.columns; ++col) {
      TextPosition char_pos = {.col = col, .row = row};
      MDAWriteChar(mda, char_pos);
    }
  }
}
