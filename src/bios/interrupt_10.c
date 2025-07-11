#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "interrupts.h"
#include "public.h"
#include "video.h"
#endif  // YAX86_IMPLEMENTATION

// BIOS Interrupt 0x10, AH = 0x00 - Set video mode
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH00SetVideoMode(BIOSState* bios, CPUState* cpu) {
  uint8_t al = cpu->registers[kAX] & 0xFF;  // Video mode
  SwitchVideoMode(bios, al);
  return kExecuteSuccess;
}

// BIOS Interrupt 0x10, AH = 0x01 - Set cursor shape
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH01SetCursorShape(BIOSState* bios, CPUState* cpu) {
  uint8_t ch = (cpu->registers[kCX] >> 8) & 0xFF;  // Cursor starting row
  uint8_t cl = cpu->registers[kCX] & 0xFF;         // Cursor ending row
  TextSetCursorShape(bios, ch, cl);
  return kExecuteSuccess;
}

// BIOS Interrupt 0x10, AH = 0x02 - Set cursor position
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH02SetCursorPosition(BIOSState* bios, CPUState* cpu) {
  uint8_t dh = (cpu->registers[kDX] >> 8) & 0xFF;  // Cursor row
  uint8_t dl = cpu->registers[kDX] & 0xFF;         // Cursor column
  uint8_t bh = cpu->registers[kBX] & 0xFF;         // Page number
  TextPosition cursor_pos = {
      .col = dl,
      .row = dh,
  };
  TextSetCursorPositionForPage(bios, bh, cursor_pos);
  return kExecuteSuccess;
}

// BIOS Interrupt 0x10, AH = 0x03 - Read cursor position
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH03ReadCursorPosition(BIOSState* bios, CPUState* cpu) {
  uint8_t bh = cpu->registers[kBX] & 0xFF;  // Page number
  const VideoModeMetadata* metadata = GetCurrentVideoModeMetadata(bios);
  if (!metadata) {
    return kExecuteSuccess;
  }
  TextPosition cursor_pos = TextGetCursorPositionForPage(bios, bh);
  uint8_t cursor_start_row, cursor_end_row;
  if (!TextGetCursorShape(bios, &cursor_start_row, &cursor_end_row)) {
    GetDefaultCursorShape(metadata, &cursor_start_row, &cursor_end_row);
  }

  uint8_t dh = cursor_pos.row;
  uint8_t dl = cursor_pos.col;
  cpu->registers[kDX] = (dh << 8) | dl;

  uint8_t ch = cursor_start_row;
  uint8_t cl = cursor_end_row;
  cpu->registers[kCX] = (ch << 8) | cl;

  return kExecuteSuccess;
}

// BIOS Interrupt 0x10, AH = 0x04 - Read light pen position
YAX86_PRIVATE ExecuteStatus HandleBIOSInterrupt10AH04ReadLightPenPosition(
    __attribute__((unused)) BIOSState* bios, CPUState* cpu) {
  // This is a placeholder implementation. Actual light pen support would
  // require hardware interaction. We set AH to 0x00 to indicate no light pen.
  cpu->registers[kAX] &= 0x00FF;
  return kExecuteSuccess;
}

// BIOS Interrupt 0x10, AH = 0x05 - Set active display page
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH05SetActiveDisplayPage(BIOSState* bios, CPUState* cpu) {
  uint8_t al = cpu->registers[kAX] & 0xFF;  // Page number
  TextSetCurrentPage(bios, al);
  return kExecuteSuccess;
}

// Common logic for scrolling a region in text mode.
static ExecuteStatus ScrollActivePageUpOrDown(
    BIOSState* bios, CPUState* cpu, bool scroll_up) {
  uint8_t al = cpu->registers[kAX] & 0xFF;         // Number of lines to scroll
  uint8_t ch = (cpu->registers[kCX] >> 8) & 0xFF;  // Starting row
  uint8_t cl = cpu->registers[kCX] & 0xFF;         // Starting column
  uint8_t dh = (cpu->registers[kDX] >> 8) & 0xFF;  // Ending row
  uint8_t dl = cpu->registers[kDX] & 0xFF;         // Ending column
  uint8_t bh =
      (cpu->registers[kBX] >> 8) & 0xFF;  // Text attribute for blank lines
  uint8_t page = TextGetCurrentPage(bios);
  TextPosition top_left = {.col = cl, .row = ch};
  TextPosition bottom_right = {.col = dl, .row = dh};
  if (al == 0) {
    // AL = 0 means clear the region.
    TextClearRegion(bios, page, top_left, bottom_right, bh);
  } else {
    if (scroll_up) {
      TextScrollUp(bios, page, top_left, bottom_right, al, bh);
    } else {
      TextScrollDown(bios, page, top_left, bottom_right, al, bh);
    }
  }
  return kExecuteSuccess;
}

// BIOS Interrupt 0x10, AH = 0x06 - Scroll active page up
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH06ScrollActivePageUp(BIOSState* bios, CPUState* cpu) {
  return ScrollActivePageUpOrDown(bios, cpu, true);
}

// BIOS Interrupt 0x10, AH = 0x07 - Scroll active page down
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH06ScrollActivePageDown(BIOSState* bios, CPUState* cpu) {
  return ScrollActivePageUpOrDown(bios, cpu, false);
}

// BIOS Interrupt 0x10, AH = 0x08 - Read character and attribute
YAX86_PRIVATE ExecuteStatus HandleBIOSInterrupt10AH08ReadCharacterAndAttribute(
    BIOSState* bios, CPUState* cpu) {
  uint8_t bh = (cpu->registers[kBX] >> 8) & 0xFF;  // Page number
  TextPosition cursor_pos = TextGetCursorPositionForPage(bios, bh);
  uint32_t offset = TextGetCharOffset(bios, bh, cursor_pos);
  uint8_t al = ReadVRAMByte(bios, offset);      // Character
  uint8_t ah = ReadVRAMByte(bios, offset + 1);  // Attribute byte
  cpu->registers[kAX] = (ah << 8) | al;
  return kExecuteSuccess;
}

// Common logic for writing a character and optional attribute in text mode.
YAX86_PRIVATE ExecuteStatus TextWriteCharacterAndOptionalAttribute(
    BIOSState* bios, CPUState* cpu, bool write_attribute) {
  uint8_t bh = (cpu->registers[kBX] >> 8) & 0xFF;  // Page number
  uint8_t al = cpu->registers[kAX] & 0xFF;         // Character
  uint8_t bl = cpu->registers[kBX] & 0xFF;         // Attribute byte
  uint16_t cx = cpu->registers[kCX];               // Number of times to repeat

  // On original hardware, if CX is 0, the character is written continuously.
  // However, this is not particularly useful and modern BIOS implementations
  // treat CX = 0 as a no-op.
  if (cx == 0) {
    return kExecuteSuccess;
  }

  const VideoModeMetadata* metadata = GetCurrentVideoModeMetadata(bios);
  if (!metadata) {
    return kExecuteSuccess;
  }
  TextPosition cursor_pos = TextGetCursorPositionForPage(bios, bh);
  uint32_t offset = TextGetCharOffset(bios, bh, cursor_pos);
  if (offset == kInvalidMemoryOffset) {
    return kExecuteSuccess;
  }
  TextPosition bottom_right_pos = {
      .col = metadata->columns - 1,
      .row = metadata->rows - 1,
  };
  uint32_t max_offset = TextGetCharOffset(bios, bh, bottom_right_pos);
  for (uint16_t i = 0; i < cx && offset <= max_offset; ++i, offset += 2) {
    WriteVRAMByte(bios, offset, al);
    if (write_attribute) {
      WriteVRAMByte(bios, offset + 1, bl);
    }
  }
  return kExecuteSuccess;
}

// BIOS Interrupt 0x10, AH = 0x09 - Write character and attribute
YAX86_PRIVATE ExecuteStatus HandleBIOSInterrupt10AH09WriteCharacterAndAttribute(
    BIOSState* bios, CPUState* cpu) {
  return TextWriteCharacterAndOptionalAttribute(bios, cpu, true);
}

// BIOS Interrupt 0x10, AH = 0x0A - Write character
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH0AWriteCharacter(BIOSState* bios, CPUState* cpu) {
  return TextWriteCharacterAndOptionalAttribute(bios, cpu, false);
}

// BIOS Interrupt 0x10, AH = 0x0B - Set color palette
YAX86_PRIVATE ExecuteStatus HandleBIOSInterrupt10AH0BSetColorPalette(
    YAX86_UNUSED BIOSState* bios, YAX86_UNUSED CPUState* cpu) {
  return kExecuteSuccess;
}

// BIOS Interrupt 0x10, AH = 0x0C - Write dot
YAX86_PRIVATE ExecuteStatus HandleBIOSInterrupt10AH0CWriteDot(
    YAX86_UNUSED BIOSState* bios, YAX86_UNUSED CPUState* cpu) {
  return kExecuteSuccess;
}

// BIOS Interrupt 0x10, AH = 0x0D - Read dot
YAX86_PRIVATE ExecuteStatus HandleBIOSInterrupt10AH0DReadDot(
    YAX86_UNUSED BIOSState* bios, YAX86_UNUSED CPUState* cpu) {
  return kExecuteSuccess;
}

// Common logic for writing a character as teletype in text mode.
YAX86_PRIVATE ExecuteStatus TextWriteCharacterAsTeletype(
    BIOSState* bios, uint8_t page, uint8_t char_value, bool write_attr_value,
    uint8_t attr_value) {
  if (char_value == '\a') {
    // Bell character - not implemented
    return kExecuteSuccess;
  }
  const VideoModeMetadata* metadata = GetCurrentVideoModeMetadata(bios);
  if (!metadata) {
    return kExecuteSuccess;
  }
  TextPosition cursor_pos = TextGetCursorPositionForPage(bios, page);

  switch (char_value) {
    case '\b': {
      // Backspace - move cursor left, but stop at the start of the line.
      if (cursor_pos.col > 0) {
        cursor_pos.col--;
      }
      TextSetCursorPositionForPage(bios, page, cursor_pos);
      break;
    }
    case '\n': {
      // Line feed - move cursor to the next line or scroll up.
      if (cursor_pos.row < metadata->rows - 1) {
        cursor_pos.row++;
        TextSetCursorPositionForPage(bios, page, cursor_pos);
      } else {
        // If at the bottom of the screen, scroll up.
        TextScrollUpPage(bios, page, 1, 0x07);
      }
      break;
    }
    case '\r': {
      // Carriage return - move cursor to the start of the line.
      cursor_pos.col = 0;
      TextSetCursorPositionForPage(bios, page, cursor_pos);
      break;
    }
    default: {
      // Write character.
      uint32_t offset = TextGetCharOffset(bios, page, cursor_pos);
      if (offset == kInvalidMemoryOffset) {
        return kExecuteSuccess;
      }
      WriteVRAMByte(bios, offset, char_value);
      if (write_attr_value) {
        WriteVRAMByte(bios, offset + 1, attr_value);
      }

      // Move cursor.
      if (cursor_pos.col < metadata->columns - 1) {
        // If not at the end of the line, move cursor right.
        ++cursor_pos.col;
      } else if (cursor_pos.row < metadata->rows - 1) {
        // If at the end of the line, move to the next line.
        ++cursor_pos.row;
        cursor_pos.col = 0;
      } else {
        // If at the bottom right of the screen, scroll up.
        TextScrollUpPage(bios, page, 1, 0x07);
        cursor_pos.col = 0;
      }
      TextSetCursorPositionForPage(bios, page, cursor_pos);
      break;
    }
  }
  return kExecuteSuccess;
}

// BIOS Interrupt 0x10, AH = 0x0E - Write character as teletype
YAX86_PRIVATE ExecuteStatus HandleBIOSInterrupt10AH0EWriteCharacterAsTeletype(
    BIOSState* bios, CPUState* cpu) {
  uint8_t al = cpu->registers[kAX] & 0xFF;         // Character to write
  uint8_t bh = (cpu->registers[kBX] >> 8) & 0xFF;  // Page number
  return TextWriteCharacterAsTeletype(
      bios, bh, al, /* has_attr_value */ false, 0);
}

// BIOS Interrupt 0x10, AH = 0x0F - Get current video mode
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH0FGetCurrentVideoMode(BIOSState* bios, CPUState* cpu) {
  const VideoModeMetadata* metadata = GetCurrentVideoModeMetadata(bios);
  if (!metadata) {
    return kExecuteSuccess;
  }
  // Return the current video mode in AL.
  uint8_t al = (uint8_t)metadata->mode;
  uint8_t ah = metadata->width;
  uint8_t bh = TextGetCurrentPage(bios);
  uint8_t bl = cpu->registers[kBX] & 0xFF;
  cpu->registers[kAX] = (ah << 8) | al;
  cpu->registers[kBX] = (bh << 8) | bl;
  return kExecuteSuccess;
}

// BIOS Interrupt 0x10, AH = 0x13 - Write string
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH13WriteString(BIOSState* bios, CPUState* cpu) {
  // AL indicates the operation:
  //   0 - Write string with BL value as attribute, but keep original cursor
  //       position. Array in memory stores text to be written.
  //   1 - Write string with BL value as attribute, and move cursor to the end
  //       of the string. Array in memory stores text to be written.
  //   2 - Array in memory contains character byte + attribute byte. Keep
  //       original cursor position.
  //   3 - Array in memory contains character byte + attribute byte. Move cursor
  //       to the end of the string.
  uint8_t al = cpu->registers[kAX] & 0xFF;
  if (al > 3) {
    return kExecuteSuccess;
  }
  uint8_t bh = (cpu->registers[kBX] >> 8) & 0xFF;  // Page number
  uint8_t bl = cpu->registers[kBX] & 0xFF;  // Attribute byte if AL = 0 or 1
  uint16_t cx = cpu->registers[kCX];        // Number of characters to write
  uint8_t dh = (cpu->registers[kDX] >> 8) & 0xFF;  // Starting row
  uint8_t dl = cpu->registers[kDX] & 0xFF;         // Starting column
  uint16_t es = cpu->registers[kES];  // Segment address of the string
  uint16_t bp = cpu->registers[kBP];  // Offset address of the string
  uint32_t string_address = (((uint32_t)es) << 4) | bp;
  TextPosition orig_cursor_pos = TextGetCursorPosition(bios);

  TextPosition cursor_pos = {
      .col = dl,
      .row = dh,
  };
  TextSetCursorPositionForPage(bios, bh, cursor_pos);

  if (al <= 1) {
    for (uint16_t i = 0; i < cx; ++i, ++string_address) {
      uint8_t char_value = ReadMemoryByte(bios, string_address);
      ExecuteStatus status = TextWriteCharacterAsTeletype(
          bios, bh, char_value, /* has_attr_value */ true, bl);
      if (status != kExecuteSuccess) {
        return status;
      }
    }
  } else {
    for (uint16_t i = 0; i < cx; ++i, string_address += 2) {
      uint8_t char_value = ReadMemoryByte(bios, string_address);
      uint8_t attr_value = ReadMemoryByte(bios, string_address + 1);
      ExecuteStatus status = TextWriteCharacterAsTeletype(
          bios, bh, char_value, /* has_attr_value */ true, attr_value);
      if (status != kExecuteSuccess) {
        return status;
      }
    }
  }

  // Restore cursor position.
  if (al == 0 || al == 2) {
    TextSetCursorPositionForPage(bios, bh, orig_cursor_pos);
  }

  return kExecuteSuccess;
}

// Function handlers for BIOS interrupt 0x10.
YAX86_PRIVATE BIOSInterruptFunctionHandler
    kBIOSInterrupt10Handlers[kNumBIOSInterrupt10Functions] = {
        HandleBIOSInterrupt10AH00SetVideoMode,
        HandleBIOSInterrupt10AH01SetCursorShape,
        HandleBIOSInterrupt10AH02SetCursorPosition,
        HandleBIOSInterrupt10AH03ReadCursorPosition,
        HandleBIOSInterrupt10AH04ReadLightPenPosition,
        HandleBIOSInterrupt10AH05SetActiveDisplayPage,
        HandleBIOSInterrupt10AH06ScrollActivePageUp,
        HandleBIOSInterrupt10AH06ScrollActivePageDown,
        HandleBIOSInterrupt10AH08ReadCharacterAndAttribute,
        HandleBIOSInterrupt10AH09WriteCharacterAndAttribute,
        HandleBIOSInterrupt10AH0AWriteCharacter,
        HandleBIOSInterrupt10AH0BSetColorPalette,
        HandleBIOSInterrupt10AH0CWriteDot,
        HandleBIOSInterrupt10AH0DReadDot,
        HandleBIOSInterrupt10AH0EWriteCharacterAsTeletype,
        HandleBIOSInterrupt10AH0FGetCurrentVideoMode,
        0,
        0,
        0,
        HandleBIOSInterrupt10AH13WriteString,
};

// BIOS interrupt 0x10 - Video I/O
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10VideoIO(BIOSState* bios, CPUState* cpu, uint8_t ah) {
  return ExecuteBIOSInterruptFunctionHandler(
      kBIOSInterrupt10Handlers, kNumBIOSInterrupt10Functions, bios, cpu, ah);
}
