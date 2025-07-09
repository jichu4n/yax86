#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "interrupts.h"
#include "public.h"
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
  TextSetCursorPositionForPage(bios, cursor_pos, bh);
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
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH04ReadLightPenPosition(BIOSState* bios, CPUState* cpu) {
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

// Function handlers for BIOS interrupt 0x10.
YAX86_PRIVATE BIOSInterruptFunctionHandler
    kBIOSInterrupt10Handlers[kNumBIOSInterrupt10Functions] = {
        HandleBIOSInterrupt10AH00SetVideoMode,
        HandleBIOSInterrupt10AH01SetCursorShape,
        HandleBIOSInterrupt10AH02SetCursorPosition,
        HandleBIOSInterrupt10AH03ReadCursorPosition,
        HandleBIOSInterrupt10AH04ReadLightPenPosition,
        HandleBIOSInterrupt10AH05SetActiveDisplayPage,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
};

// BIOS interrupt 0x10 - Video I/O
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10VideoIO(BIOSState* bios, CPUState* cpu, uint8_t ah) {
  return ExecuteBIOSInterruptFunctionHandler(
      kBIOSInterrupt10Handlers, kNumBIOSInterrupt10Functions, bios, cpu, ah);
}
