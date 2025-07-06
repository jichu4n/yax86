#ifndef YAX86_IMPLEMENTATION
#include "video.h"

#include "../util/common.h"
#include "fonts.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

const VideoModeMetadata kVideoModeMetadataTable[kNumVideoModes] = {
    // CGA text mode 0x00: Text, 40×25, grayscale, 320x200, 8x8
    {
        .mode = kVideoTextModeCGA00,
        .type = kVideoTextMode,
        .vram_address = 0xB8000,
        .vram_size = 16 * 1024,
        .width = 320,
        .height = 200,
        .columns = 40,
        .rows = 25,
        .char_width = kCGACharWidth,
        .char_height = kCGACharHeight,
    },
    // CGA text mode 0x01: Text, 40×25, 16 colors, 320x200, 8x8
    {
        .mode = kVideoTextModeCGA01,
        .type = kVideoTextMode,
        .vram_address = 0xB8000,
        .vram_size = 16 * 1024,
        .width = 320,
        .height = 200,
        .columns = 40,
        .rows = 25,
        .char_width = kCGACharWidth,
        .char_height = kCGACharHeight,
    },
    // CGA text mode 0x02: Text, 80×25, grayscale, 640x200, 8x8
    {
        .mode = kVideoTextModeCGA02,
        .type = kVideoTextMode,
        .vram_address = 0xB8000,
        .vram_size = 16 * 1024,
        .width = 640,
        .height = 200,
        .columns = 80,
        .rows = 25,
        .char_width = kCGACharWidth,
        .char_height = kCGACharHeight,
    },
    // CGA text mode 0x03: Text, 80×25, 16 colors, 640x200, 8x8
    {
        .mode = kVideoTextModeCGA03,
        .type = kVideoTextMode,
        .vram_address = 0xB8000,
        .vram_size = 16 * 1024,
        .width = 640,
        .height = 200,
        .columns = 80,
        .rows = 25,
        .char_width = kCGACharWidth,
        .char_height = kCGACharHeight,
    },
    // CGA graphics mode 0x04: Graphics, 4 colors, 320×200
    {
        .mode = kVideoGraphicsModeCGA04,
        .type = kVideoGraphicsMode,
        .vram_address = 0xB8000,
        .vram_size = 16 * 1024,
        .width = 320,
        .height = 200,
    },
    // CGA graphics mode 0x05: Graphics, grayscale, 320×200
    {
        .mode = kVideoGraphicsModeCGA05,
        .type = kVideoGraphicsMode,
        .vram_address = 0xB8000,
        .vram_size = 16 * 1024,
        .width = 320,
        .height = 200,
    },
    // CGA graphics mode 0x06: Graphics, monochrome, 640×200
    {
        .mode = kVideoGraphicsModeCGA06,
        .type = kVideoGraphicsMode,
        .vram_address = 0xB8000,
        .vram_size = 16 * 1024,
        .width = 640,
        .height = 200,
    },
    // MDA text mode 0x07: Text, 80×25, monochrome, 720x350, 9x14
    {
        .mode = kVideoTextModeMDA07,
        .type = kVideoTextMode,
        .vram_address = 0xB0000,
        .vram_size = 4 * 1024,
        .width = 720,
        .height = 350,
        .columns = 80,
        .rows = 25,
        .char_width = kMDACharWidth,
        .char_height = kMDACharHeight,
    },
};

enum {
  // Position of underline in MDA text mode.
  kMDAUnderlinePosition = 12,
};

// Check if video mode is valid and supported.
bool IsSupportedVideoMode(uint8_t mode) {
  if (mode >= kNumVideoModes) {
    return false;
  }
  return kVideoModeMetadataTable[mode].type != kVideoModeUnsupported;
}

// Get current video mode. Returns kInvalidVideoMode if the video mode in the
// BIOS Data Area (BDA) is invalid.
VideoMode GetCurrentVideoMode(struct BIOSState* bios) {
  uint8_t mode = ReadMemoryByte(bios, kBDAAddress + kBDAVideoMode);
  return IsSupportedVideoMode(mode) ? (VideoMode)mode : kInvalidVideoMode;
}

// Get current video mode metadata, or NULL if the video mode in the BIOS Data
// Area (BDA) is invalid.
const VideoModeMetadata* GetCurrentVideoModeMetadata(struct BIOSState* bios) {
  VideoMode mode = GetCurrentVideoMode(bios);
  return IsSupportedVideoMode(mode) ? &kVideoModeMetadataTable[mode] : NULL;
}

YAX86_PRIVATE uint8_t ReadVRAMByte(BIOSState* bios, uint32_t address) {
  const VideoModeMetadata* metadata = GetCurrentVideoModeMetadata(bios);
  if (!metadata || address >= metadata->vram_size) {
    return 0xFF;
  }
  return bios->config->read_vram_byte(bios, address);
}

YAX86_PRIVATE void WriteVRAMByte(
    BIOSState* bios, uint32_t address, uint8_t value) {
  const VideoModeMetadata* metadata = GetCurrentVideoModeMetadata(bios);
  if (!metadata || address >= metadata->vram_size) {
    return;
  }
  bios->config->write_vram_byte(bios, address, value);
}

// Switch video mode.
bool SwitchVideoMode(struct BIOSState* bios, VideoMode mode) {
  if (!IsSupportedVideoMode(mode)) {
    return false;
  }

  // Update the video mode in the BIOS Data Area (BDA).
  WriteMemoryByte(bios, kBDAAddress + kBDAVideoMode, (uint8_t)mode);
  // Update memory map.
  const VideoModeMetadata* metadata = &kVideoModeMetadataTable[mode];
  MemoryRegion vram_region = {
      .region_type = kMemoryRegionVideo,
      .start = metadata->vram_address,
      .size = metadata->vram_size,
      .read_memory_byte = ReadVRAMByte,
      .write_memory_byte = WriteVRAMByte,
  };
  MemoryRegion* existing_vram_region =
      GetMemoryRegionByType(bios, kMemoryRegionVideo);
  if (existing_vram_region) {
    *existing_vram_region = vram_region;
  } else {
    MemoryRegionsAppend(&bios->memory_regions, &vram_region);
  }

  if (metadata->type == kVideoTextMode) {
    // Update text mode metadata in the BDA.
    WriteMemoryByte(bios, kBDAAddress + kBDAVideoColumns, metadata->columns);
    WriteMemoryByte(bios, kBDAAddress + kBDAVideoRows, metadata->rows - 1);
    WriteMemoryByte(
        bios, kBDAAddress + kBDAVideoCharHeight, metadata->char_height);

    // Update page state.
    // One page is 2 bytes per character (char + attr).
    WriteMemoryWord(
        bios, kBDAAddress + kBDAVideoPageSize,
        metadata->columns * metadata->rows * 2);
    WriteMemoryByte(bios, kBDAAddress + kBDAVideoPageOffset, 0);
    WriteMemoryByte(bios, kBDAAddress + kBDAVideoCurrentPage, 0);

    // Update cursor state.
    // Default cursor type is two scan lines at bottom of the character cell.
    uint16_t default_cursor =
        (metadata->char_height - 2) << 8 | (metadata->char_height - 1);
    WriteMemoryWord(bios, kBDAAddress + kBDAVideoCursorType, default_cursor);
    // Set cursor position to (0, 0) for all pages.
    for (uint8_t i = 0; i < 8; ++i) {
      WriteMemoryWord(bios, kBDAAddress + kBDAVideoCursorPos + i * 2, 0);
    }

    // Clear screen.
    TextClearScreen(bios);
  }

  return true;
}

// Text mode - clear screen.
void TextClearScreen(struct BIOSState* bios) {
  const VideoModeMetadata* metadata = GetCurrentVideoModeMetadata(bios);
  if (!metadata || metadata->type != kVideoTextMode) {
    return;
  }
  for (uint32_t i = 0; i < metadata->vram_size; i += 2) {
    WriteVRAMByte(bios, i, ' ');
    // All text modes use 0x07 as the default attribute byte.
    WriteVRAMByte(bios, i + 1, 0x07);
  }
}

// Text mode - get current page.
uint8_t TextGetCurrentPage(struct BIOSState* bios) {
  const VideoModeMetadata* metadata = GetCurrentVideoModeMetadata(bios);
  if (!metadata || metadata->type != kVideoTextMode) {
    return 0;
  }
  return ReadMemoryByte(bios, kBDAAddress + kBDAVideoCurrentPage);
}

// Text mode - get cursor position on a page.
TextPosition TextGetCursorPositionForPage(
    struct BIOSState* bios, uint8_t page) {
  static const TextPosition kInvalidTextPosition = {
      .col = 0,
      .row = 0,
  };
  if (page >= 8) {
    return kInvalidTextPosition;
  }
  const VideoModeMetadata* metadata = GetCurrentVideoModeMetadata(bios);
  if (!metadata || metadata->type != kVideoTextMode) {
    return kInvalidTextPosition;
  }

  uint32_t cursor_address = kBDAAddress + kBDAVideoCursorPos + page * 2;
  TextPosition position = {
      .col = ReadMemoryByte(bios, cursor_address),
      .row = ReadMemoryByte(bios, cursor_address + 1),
  };
  return position;
}

// Text mode - get cursor position in current page.
TextPosition TextGetCursorPosition(struct BIOSState* bios) {
  return TextGetCursorPositionForPage(bios, TextGetCurrentPage(bios));
}

YAX86_PRIVATE void InitVideo(BIOSState* bios) {
  // Set initial video mode in BDA equipment list word, bits 4-5.
  //   00b - EGA, VGA, or other (use other BIOS data area locations)
  //   01b - 40×25 color (CGA)
  //   10b - 80×25 color (CGA)
  //   11b - 80×25 monochrome (MDA)
  uint16_t equipment_word =
      ReadMemoryWord(bios, kBDAAddress + kBDAEquipmentWord);
  equipment_word |= (0x03 << 4);  // MDA
  WriteMemoryWord(bios, kBDAAddress + kBDAEquipmentWord, equipment_word);

  SwitchVideoMode(bios, kVideoTextModeMDA07);
}

// Write a character to display in MDA text mode.
// TODO: Support blinking.
YAX86_PRIVATE void WriteCharMDA(
    BIOSState* bios, const VideoModeMetadata* metadata, uint8_t page,
    TextPosition char_pos) {
  if (char_pos.col >= metadata->columns || char_pos.row >= metadata->rows) {
    return;
  }

  uint32_t char_address = (page * metadata->rows * metadata->columns +
                           char_pos.row * metadata->columns + char_pos.col) *
                          2;
  uint8_t char_value = ReadVRAMByte(bios, char_address);
  uint8_t attr_value = ReadVRAMByte(bios, char_address + 1);
  const uint16_t* char_bitmap = kFontMDA9x14Bitmap[char_value];

  const RGB* foreground;
  const RGB* background;
  if ((attr_value & kMDABackground) == 0x70) {
    // Reverse video mode.
    foreground = &bios->config->mda_config.background;
    background = &bios->config->mda_config.foreground;
  } else {
    // Normal video mode.
    foreground = (attr_value & kMDAIntenseForeground)
                     ? &bios->config->mda_config.intense_foreground
                     : &bios->config->mda_config.foreground;
    background = &bios->config->mda_config.background;
  }

  Position origin_pixel_pos = {
      .x = char_pos.col * metadata->char_width,
      .y = char_pos.row * metadata->char_height,
  };
  for (uint8_t y = 0; y < metadata->char_height; ++y) {
    uint16_t row_bitmap;
    // If underline, set entire underline row to foreground color.
    if (y == kMDAUnderlinePosition && (attr_value & kMDAForeground) == 0x01) {
      row_bitmap = 0xFFFF;
    } else {
      row_bitmap = char_bitmap[y];
    }
    for (uint8_t x = 0; x < metadata->char_width; ++x) {
      Position pixel_pos = {
          .x = origin_pixel_pos.x + x,
          .y = origin_pixel_pos.y + y,
      };
      const RGB* pixel_rgb =
          (row_bitmap & (1 << (metadata->char_width - 1 - x))) ? foreground
                                                               : background;
      bios->config->write_pixel(bios, pixel_pos, *pixel_rgb);
    }
  }
}

// Handler to write a character to the display in text mode.
typedef void (*WriteCharHandler)(
    struct BIOSState* bios, const VideoModeMetadata* metadata, uint8_t page,
    TextPosition char_pos);
// Table of handlers for writing characters in different text modes, indexed by
// VideoMode enum values.
const WriteCharHandler kWriteCharHandlers[] = {
    // CGA text mode 0x00: 40×25, grayscale, 320x200, 8x8
    0,
    // CGA text mode 0x01: 40×25, 16 colors, 320x200, 8x8
    0,
    // CGA text mode 0x02: 80×25, grayscale, 640x200, 8x8
    0,
    // CGA text mode 0x03: 80×25, 16 colors, 640x200, 8x8
    0,
    // CGA graphics mode 0x04: 4 colors, 320×200
    0,
    // CGA graphics mode 0x05: grayscale, 320×200
    0,
    // CGA graphics mode 0x06: monochrome, 640×200
    0,
    // MDA text mode 0x07: 80×25, monochrome, 720x350, 9x14
    WriteCharMDA,
};

// Render the current page in the emulated video RAM to the real display.
// Invokes the write_pixel callback to do the actual pixel rendering.
bool RenderCurrentVideoPage(struct BIOSState* bios) {
  const VideoModeMetadata* metadata = GetCurrentVideoModeMetadata(bios);
  if (!metadata) {
    return false;
  }
  switch (metadata->type) {
    case kVideoTextMode: {
      WriteCharHandler handler = kWriteCharHandlers[metadata->mode];
      if (!handler) {
        return false;
      }
      uint8_t page = TextGetCurrentPage(bios);
      for (uint8_t row = 0; row < metadata->rows; ++row) {
        for (uint8_t col = 0; col < metadata->columns; ++col) {
          TextPosition char_pos = {.col = col, .row = row};
          handler(bios, metadata, page, char_pos);
        }
      }
      return true;
    }
    case kVideoGraphicsMode: {
      return false;
    }
    default:
      return false;
  }
}
