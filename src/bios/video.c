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
        // .type = kVideoTextMode,
        .type = kVideoModeUnsupported,
        .vram_address = 0xB8000,
        .vram_size = 16 * 1024,
        .width = 320,
        .height = 200,
        .num_pages = 8,
        .columns = 40,
        .rows = 25,
        .char_width = kCGACharWidth,
        .char_height = kCGACharHeight,
    },
    // CGA text mode 0x01: Text, 40×25, 16 colors, 320x200, 8x8
    {
        .mode = kVideoTextModeCGA01,
        // .type = kVideoTextMode,
        .type = kVideoModeUnsupported,
        .vram_address = 0xB8000,
        .vram_size = 16 * 1024,
        .width = 320,
        .height = 200,
        .num_pages = 8,
        .columns = 40,
        .rows = 25,
        .char_width = kCGACharWidth,
        .char_height = kCGACharHeight,
    },
    // CGA text mode 0x02: Text, 80×25, grayscale, 640x200, 8x8
    {
        .mode = kVideoTextModeCGA02,
        // .type = kVideoTextMode,
        .type = kVideoModeUnsupported,
        .vram_address = 0xB8000,
        .vram_size = 16 * 1024,
        .width = 640,
        .height = 200,
        .num_pages = 4,
        .columns = 80,
        .rows = 25,
        .char_width = kCGACharWidth,
        .char_height = kCGACharHeight,
    },
    // CGA text mode 0x03: Text, 80×25, 16 colors, 640x200, 8x8
    {
        .mode = kVideoTextModeCGA03,
        // .type = kVideoTextMode,
        .type = kVideoModeUnsupported,
        .vram_address = 0xB8000,
        .vram_size = 16 * 1024,
        .width = 640,
        .height = 200,
        .num_pages = 4,
        .columns = 80,
        .rows = 25,
        .char_width = kCGACharWidth,
        .char_height = kCGACharHeight,
    },
    // CGA graphics mode 0x04: Graphics, 4 colors, 320×200
    {
        .mode = kVideoGraphicsModeCGA04,
        // .type = kVideoGraphicsMode,
        .type = kVideoModeUnsupported,
        .vram_address = 0xB8000,
        .vram_size = 16 * 1024,
        .width = 320,
        .height = 200,
        .num_pages = 1,
    },
    // CGA graphics mode 0x05: Graphics, grayscale, 320×200
    {
        .mode = kVideoGraphicsModeCGA05,
        // .type = kVideoGraphicsMode,
        .type = kVideoModeUnsupported,
        .vram_address = 0xB8000,
        .vram_size = 16 * 1024,
        .width = 320,
        .height = 200,
        .num_pages = 1,
    },
    // CGA graphics mode 0x06: Graphics, monochrome, 640×200
    {
        .mode = kVideoGraphicsModeCGA06,
        // .type = kVideoGraphicsMode,
        .type = kVideoModeUnsupported,
        .vram_address = 0xB8000,
        .vram_size = 16 * 1024,
        .width = 640,
        .height = 200,
        .num_pages = 1,
    },
    // MDA text mode 0x07: Text, 80×25, monochrome, 720x350, 9x14
    {
        .mode = kVideoTextModeMDA07,
        .type = kVideoTextMode,
        .vram_address = 0xB0000,
        .vram_size = 4 * 1024,
        .width = 720,
        .height = 350,
        .num_pages = 1,
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

// Get default cursor start and end rows for a mode.
YAX86_PRIVATE void GetDefaultCursorShape(
    const VideoModeMetadata* metadata, uint8_t* start_row, uint8_t* end_row) {
  // Default cursor type is two scan lines at bottom of the character cell.
  *start_row = metadata->char_height - 2;
  *end_row = metadata->char_height - 1;
}

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
    uint8_t cursor_start_row, cursor_end_row;
    GetDefaultCursorShape(metadata, &cursor_start_row, &cursor_end_row);
    TextSetCursorShape(bios, cursor_start_row, cursor_end_row);
    // Set cursor position to (0, 0) for all pages.
    for (uint8_t i = 0; i < 8; ++i) {
      WriteMemoryWord(bios, kBDAAddress + kBDAVideoCursorPos + i * 2, 0);
    }

    // Clear screen.
    TextClearScreen(bios);
  }

  return true;
}

// Text mode - clear a region of the screen to a specific attribute byte.
extern bool TextClearRegion(
    struct BIOSState* bios, uint8_t page, TextPosition top_left,
    TextPosition bottom_right, uint8_t attr) {
  if (bottom_right.row < top_left.row || bottom_right.col < top_left.col) {
    return false;
  }
  for (uint8_t row = top_left.row; row <= bottom_right.row; ++row) {
    TextPosition row_start_pos = {
        .col = top_left.col,
        .row = row,
    };
    uint32_t offset = TextGetCharOffset(bios, page, row_start_pos);
    if (offset == kInvalidMemoryOffset) {
      return false;
    }
    for (uint8_t col = top_left.col; col <= bottom_right.col;
         ++col, offset += 2) {
      WriteVRAMByte(bios, offset, ' ');
      WriteVRAMByte(bios, offset + 1, attr);
    }
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
  if (!metadata) {
    return 0;
  }
  uint8_t page = ReadMemoryByte(bios, kBDAAddress + kBDAVideoCurrentPage);
  if (page >= metadata->num_pages) {
    // If the page is out of bounds, default to the first page.
    page = 0;
  }
  return page;
}

// Text mode - set current page.
bool TextSetCurrentPage(struct BIOSState* bios, uint8_t page) {
  const VideoModeMetadata* metadata = GetCurrentVideoModeMetadata(bios);
  if (!metadata || page >= metadata->num_pages) {
    return false;
  }
  WriteMemoryByte(bios, kBDAAddress + kBDAVideoCurrentPage, page);
  return true;
}

// Text mode - get the memory address of a page.
uint32_t TextGetPageOffset(struct BIOSState* bios, uint8_t page) {
  const TextPosition position = {
      .col = 0,
      .row = 0,
  };
  return TextGetCharOffset(bios, page, position);
}

// Text mode - get the memory offset of a character relevant to VRAM start
// address.
uint32_t TextGetCharOffset(
    struct BIOSState* bios, uint8_t page, TextPosition position) {
  const VideoModeMetadata* metadata = GetCurrentVideoModeMetadata(bios);
  if (!metadata || page >= metadata->num_pages ||
      position.col >= metadata->columns || position.row >= metadata->rows) {
    return kInvalidMemoryOffset;
  }
  return (page * metadata->columns * metadata->rows +
          position.row * metadata->columns + position.col) *
         2;
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
  if (position.col >= metadata->columns) {
    position.col = metadata->columns - 1;
  }
  if (position.row >= metadata->rows) {
    position.row = metadata->rows - 1;
  }
  return position;
}

// Text mode - set cursor position for a specific page.
bool TextSetCursorPositionForPage(
    struct BIOSState* bios, uint8_t page, TextPosition position) {
  const VideoModeMetadata* metadata = GetCurrentVideoModeMetadata(bios);
  if (!metadata) {
    return false;
  }
  if (page >= metadata->num_pages || position.col >= metadata->columns ||
      position.row >= metadata->rows) {
    return false;
  }
  uint32_t cursor_address = kBDAAddress + kBDAVideoCursorPos + page * 2;
  WriteMemoryByte(bios, cursor_address, position.col);
  WriteMemoryByte(bios, cursor_address + 1, position.row);
  return true;
}

// Text mode - get cursor position in current page.
TextPosition TextGetCursorPosition(struct BIOSState* bios) {
  return TextGetCursorPositionForPage(bios, TextGetCurrentPage(bios));
}

// Text mode - set cursor start and end rows.
bool TextSetCursorShape(
    struct BIOSState* bios, uint8_t start_row, uint8_t end_row) {
  const VideoModeMetadata* metadata = GetCurrentVideoModeMetadata(bios);
  if (!metadata || metadata->type != kVideoTextMode) {
    return false;
  }
  if (start_row >= metadata->char_height || end_row >= metadata->char_height) {
    return false;
  }
  WriteMemoryByte(bios, kBDAAddress + kBDAVideoCursorStartRow, start_row);
  WriteMemoryByte(bios, kBDAAddress + kBDAVideoCursorEndRow, end_row);
  return true;
}

// Text mode - get cursor start and end rows.
bool TextGetCursorShape(
    struct BIOSState* bios, uint8_t* start_row, uint8_t* end_row) {
  const VideoModeMetadata* metadata = GetCurrentVideoModeMetadata(bios);
  if (!metadata || metadata->type != kVideoTextMode) {
    return false;
  }
  uint8_t start_row_value =
      ReadMemoryByte(bios, kBDAAddress + kBDAVideoCursorStartRow);
  uint8_t end_row_value =
      ReadMemoryByte(bios, kBDAAddress + kBDAVideoCursorEndRow);
  if (start_row_value >= metadata->char_height ||
      end_row_value >= metadata->char_height) {
    return false;
  }
  *start_row = start_row_value;
  *end_row = end_row_value;
  return true;
}

// Text mode - common logic to scroll up or down a region.
static bool TextScrollCommon(
    struct BIOSState* bios, uint8_t page, TextPosition top_left,
    TextPosition bottom_right, uint8_t lines, uint8_t attr, bool scroll_up) {
  if (bottom_right.row <= top_left.row || bottom_right.col <= top_left.col) {
    return false;
  }

  // Do nothing if scrolling by zero lines.
  if (lines == 0) {
    return true;
  }
  // Clear screen if scrolling by more lines than the height of the region.
  const uint8_t height = bottom_right.row - top_left.row + 1;
  if (lines >= height) {
    return TextClearRegion(bios, page, top_left, bottom_right, attr);
  }

  // Copy each line upwards or downwards by the specified number of lines.
  for (uint8_t row = top_left.row; row <= bottom_right.row - lines; ++row) {
    TextPosition src_row_pos = {
        .col = top_left.col,
    };
    TextPosition dest_row_pos = {
        .col = top_left.col,
    };
    if (scroll_up) {
      src_row_pos.row = row + lines;
      dest_row_pos.row = row;
    } else {
      src_row_pos.row = row;
      dest_row_pos.row = row + lines;
    }
    uint32_t src_offset = TextGetCharOffset(bios, page, src_row_pos);
    uint32_t dest_offset = TextGetCharOffset(bios, page, dest_row_pos);
    if (src_offset == kInvalidMemoryOffset ||
        dest_offset == kInvalidMemoryOffset) {
      return false;
    }
    for (uint8_t col = top_left.col; col <= bottom_right.col;
         ++col, src_offset += 2, dest_offset += 2) {
      WriteVRAMByte(bios, dest_offset, ReadVRAMByte(bios, src_offset));
      WriteVRAMByte(bios, dest_offset + 1, ReadVRAMByte(bios, src_offset + 1));
    }
  }

  // Clear the top / bottom lines newly scrolled in.
  TextPosition clear_top_left = {
      .col = top_left.col,
  };
  TextPosition clear_bottom_right = {
      .col = bottom_right.col,
  };
  if (scroll_up) {
    clear_top_left.row = bottom_right.row - lines + 1;
    clear_bottom_right.row = bottom_right.row;
  } else {
    clear_top_left.row = top_left.row;
    clear_bottom_right.row = top_left.row + lines - 1;
  }
  return TextClearRegion(bios, page, clear_top_left, clear_bottom_right, attr);
}

// Text mode - scroll up a region.
bool TextScrollUp(
    struct BIOSState* bios, uint8_t page, TextPosition top_left,
    TextPosition bottom_right, uint8_t lines, uint8_t attr) {
  return TextScrollCommon(
      bios, page, top_left, bottom_right, lines, attr, true);
}

// Text mode - scroll down a region.
bool TextScrollDown(
    struct BIOSState* bios, uint8_t page, TextPosition top_left,
    TextPosition bottom_right, uint8_t lines, uint8_t attr) {
  return TextScrollCommon(
      bios, page, top_left, bottom_right, lines, attr, false);
}

// Text mode - scroll up entire page.
extern bool TextScrollUpPage(
    struct BIOSState* bios, uint8_t page, uint8_t lines, uint8_t attr) {
  const VideoModeMetadata* metadata = GetCurrentVideoModeMetadata(bios);
  if (!metadata) {
    return false;
  }
  TextPosition top_left = {
      .col = 0,
      .row = 0,
  };
  TextPosition bottom_right = {
      .col = metadata->columns - 1,
      .row = metadata->rows - 1,
  };
  return TextScrollUp(bios, page, top_left, bottom_right, lines, attr);
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

// Write a character to display in MDA text mode. For the attribute byte, we
// follow the description in the book Programmer's Reference Manual for IBM
// Personal Computers.
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
YAX86_PRIVATE void WriteCharMDA(
    BIOSState* bios, const VideoModeMetadata* metadata, uint8_t page,
    TextPosition char_pos) {
  uint32_t char_address = TextGetCharOffset(bios, page, char_pos);
  if (char_address == kInvalidMemoryOffset) {
    return;
  }
  uint8_t char_value = ReadVRAMByte(bios, char_address);
  uint8_t attr_value = ReadVRAMByte(bios, char_address + 1);
  const uint16_t* char_bitmap = kFontMDA9x14Bitmap[char_value];

  const RGB* foreground;
  const RGB* background;
  bool underline = false;

  bool intense = ((attr_value >> 3) & 0x01) != 0;
  uint8_t background_attr = (attr_value >> 4) & 0x07;
  uint8_t foreground_attr = attr_value & 0x07;
  if (background_attr == 0x00 && foreground_attr == 0x07) {
    // Normal video mode.
    foreground = intense ? &bios->config->mda_config.intense_foreground
                         : &bios->config->mda_config.foreground;
    background = &bios->config->mda_config.background;
  } else if (background_attr == 0x07 && foreground_attr == 0x00) {
    // Inverse video mode.
    foreground = &bios->config->mda_config.background;
    background = &bios->config->mda_config.foreground;
  } else if (background_attr == 0x00 && foreground_attr == 0x00) {
    // Invisible mode.
    foreground = &bios->config->mda_config.background;
    background = &bios->config->mda_config.background;
  } else if (background_attr == 0x00 && foreground_attr == 0x01) {
    // Underline mode.
    underline = true;
    foreground = &bios->config->mda_config.foreground;
    background = &bios->config->mda_config.background;
  } else {
    // Other combinations are treated as normal.
    foreground = intense ? &bios->config->mda_config.intense_foreground
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
