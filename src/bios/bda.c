#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// BDAFieldMetadata array, indexed by BDAField enum.
const BDAFieldMetadata BDAFieldMetadataTable[kBDANumFields] = {
    // 0x00: Base I/O address for serial ports.
    {.field = kBDASerialPortAddress, .offset = 0x00, .size = 8},
    // 0x08: Base I/O address for parallel ports.
    {.field = kBDAParallelPortAddress, .offset = 0x08, .size = 8},
    // 0x10: Equipment word.
    {.field = kBDAEquipmentWord, .offset = 0x10, .size = 2},
    // 0x12: POST status / Manufacturing test initialization flags
    {.field = kBDAPOSTStatus, .offset = 0x12, .size = 1},
    // 0x13: Base memory size in kilobytes (0-640)
    {.field = kBDAMemorySize, .offset = 0x13, .size = 2},
    // 0x15: Manufacturing test scratch pad
    {.field = kBDAManufacturingTest1, .offset = 0x15, .size = 1},
    // 0x16: Manufacturing test scratch pad / BIOS control flags
    {.field = kBDAManufacturingTest2, .offset = 0x16, .size = 1},
    // 0x17: Keyboard status flags 1
    {.field = kBDAKeyboardStatus1, .offset = 0x17, .size = 1},
    // 0x18: Keyboard status flags 2
    {.field = kBDAKeyboardStatus2, .offset = 0x18, .size = 1},
    // 0x19: Keyboard: Alt-nnn keypad workspace
    {.field = kBDAKeyboardAltNumpad, .offset = 0x19, .size = 1},
    // 0x1A: Keyboard: ptr to next character in keyboard buffer
    {.field = kBDAKeyboardBufferHead, .offset = 0x1A, .size = 2},
    // 0x1C: Keyboard: ptr to first free slot in keyboard buffer
    {.field = kBDAKeyboardBufferTail, .offset = 0x1C, .size = 2},
    // 0x1E: Keyboard circular buffer (16 words)
    {.field = kBDAKeyboardBuffer, .offset = 0x1E, .size = 32},
    // 0x3E: Diskette recalibrate status
    {.field = kBDADisketteRecalibrateStatus, .offset = 0x3E, .size = 1},
    // 0x3F: Diskette motor status
    {.field = kBDADisketteMotorStatus, .offset = 0x3F, .size = 1},
    // 0x40: Diskette motor turn-off time-out count
    {.field = kBDADisketteMotorTimeout, .offset = 0x40, .size = 1},
    // 0x41: Diskette last operation status
    {.field = kBDADisketteLastStatus, .offset = 0x41, .size = 1},
    // 0x42: Diskette/Fixed disk status/command bytes (7 bytes)
    {.field = kBDADisketteStatusCommand, .offset = 0x42, .size = 7},
    // 0x49: Video current mode
    {.field = kBDAVideoMode, .offset = 0x49, .size = 1},
    // 0x4A: Video columns on screen
    {.field = kBDAVideoColumns, .offset = 0x4A, .size = 2},
    // 0x4C: Video page (regen buffer) size in bytes
    {.field = kBDAVideoPageSize, .offset = 0x4C, .size = 2},
    // 0x4E: Video current page start address in regen buffer
    {.field = kBDAVideoPageOffset, .offset = 0x4E, .size = 2},
    // 0x50: Video cursor position (col, row) for eight pages
    {.field = kBDAVideoCursorPos, .offset = 0x50, .size = 16},
    // 0x60: Video cursor type, 6845 compatible
    {.field = kBDAVideoCursorType, .offset = 0x60, .size = 2},
    // 0x62: Video current page number
    {.field = kBDAVideoCurrentPage, .offset = 0x62, .size = 1},
    // 0x63: Video CRT controller base address
    {.field = kBDAVideoCRTBaseAddress, .offset = 0x63, .size = 2},
    // 0x65: Video current setting of mode select register
    {.field = kBDAVideoModeSelect, .offset = 0x65, .size = 1},
    // 0x66: Video current setting of CGA palette register
    {.field = kBDAVideoCGAPalette, .offset = 0x66, .size = 1},
    // 0x67: POST real mode re-entry point after certain resets
    {.field = kBDAPostReentryPoint, .offset = 0x67, .size = 4},
    // 0x6B: POST last unexpected interrupt
    {.field = kBDAPostLastInterrupt, .offset = 0x6B, .size = 1},
    // 0x6C: Timer ticks since midnight
    {.field = kBDATimerTicks, .offset = 0x6C, .size = 4},
    // 0x70: Timer overflow, non-zero if has counted past midnight
    {.field = kBDATimerOverflow, .offset = 0x70, .size = 1},
    // 0x71: Ctrl-Break flag
    {.field = kBDACtrlBreakFlag, .offset = 0x71, .size = 1},
    // 0x72: POST reset flag
    {.field = kBDAPostResetFlag, .offset = 0x72, .size = 2},
    // 0x74: Fixed disk last operation status
    {.field = kBDAFixedDiskStatus, .offset = 0x74, .size = 1},
    // 0x75: Fixed disk: number of fixed disk drives
    {.field = kBDAFixedDiskCount, .offset = 0x75, .size = 1},
    // 0x76: Fixed disk: control byte
    {.field = kBDAFixedDiskControl, .offset = 0x76, .size = 1},
    // 0x77: Fixed disk: I/O port offset
    {.field = kBDAFixedDiskPortOffset, .offset = 0x77, .size = 1},
    // 0x78: Parallel devices 1-3 time-out counters
    {.field = kBDAParallelTimeout, .offset = 0x78, .size = 4},
    // 0x7C: Serial devices 1-4 time-out counters
    {.field = kBDASerialTimeout, .offset = 0x7C, .size = 4},
    // 0x80: Keyboard buffer start offset
    {.field = kBDAKeyboardBufferStart, .offset = 0x80, .size = 2},
    // 0x82: Keyboard buffer end+1 offset
    {.field = kBDAKeyboardBufferEnd, .offset = 0x82, .size = 2},
    // 0x84: Video EGA/MCGA/VGA rows on screen minus one
    {.field = kBDAVideoRows, .offset = 0x84, .size = 1},
    // 0x85: Video EGA/MCGA/VGA character height in scan-lines
    {.field = kBDAVideoCharHeight, .offset = 0x85, .size = 2},
    // 0x87: Video EGA/VGA control
    {.field = kBDAVideoEGAControl, .offset = 0x87, .size = 1},
    // 0x88: Video EGA/VGA switches
    {.field = kBDAVideoEGASwitches, .offset = 0x88, .size = 1},
    // 0x89: Video MCGA/VGA mode-set option control
    {.field = kBDAVideoVGAControl, .offset = 0x89, .size = 1},
    // 0x8A: Video index into Display Combination Code table
    {.field = kBDAVideoDCCIndex, .offset = 0x8A, .size = 1},
    // 0x8B: Diskette media control
    {.field = kBDADisketteMediaControl, .offset = 0x8B, .size = 1},
    // 0x8C: Fixed disk controller status
    {.field = kBDAFixedDiskControllerStatus, .offset = 0x8C, .size = 1},
    // 0x8D: Fixed disk controller Error Status
    {.field = kBDAFixedDiskErrorStatus, .offset = 0x8D, .size = 1},
    // 0x8E: Fixed disk Interrupt Control
    {.field = kBDAFixedDiskInterruptControl, .offset = 0x8E, .size = 1},
    // 0x8F: Diskette controller information
    {.field = kBDADisketteControllerInfo, .offset = 0x8F, .size = 1},
    // 0x90: Diskette drive 0 media state
    {.field = kBDADisketteDrive0MediaState, .offset = 0x90, .size = 1},
    // 0x91: Diskette drive 1 media state
    {.field = kBDADisketteDrive1MediaState, .offset = 0x91, .size = 1},
    // 0x92: Diskette drive 0 media state at start of operation
    {.field = kBDADisketteDrive0StartState, .offset = 0x92, .size = 1},
    // 0x93: Diskette drive 1 media state at start of operation
    {.field = kBDADisketteDrive1StartState, .offset = 0x93, .size = 1},
    // 0x94: Diskette drive 0 current track number
    {.field = kBDADisketteDrive0Track, .offset = 0x94, .size = 1},
    // 0x95: Diskette drive 1 current track number
    {.field = kBDADisketteDrive1Track, .offset = 0x95, .size = 1},
    // 0x96: Keyboard status byte 3
    {.field = kBDAKeyboardStatus3, .offset = 0x96, .size = 1},
    // 0x97: Keyboard status byte 4
    {.field = kBDAKeyboardStatus4, .offset = 0x97, .size = 1},
    // 0x98: Timer2: ptr to user wait-complete flag
    {.field = kBDATimer2WaitFlagPtr, .offset = 0x98, .size = 4},
    // 0x9C: Timer2: user wait count in microseconds
    {.field = kBDATimer2WaitCount, .offset = 0x9C, .size = 4},
    // 0xA0: Timer2: Wait active flag
    {.field = kBDATimer2WaitActive, .offset = 0xA0, .size = 1},
    // 0xA1: Reserved for network adapters (7 bytes)
    {.field = kBDANetworkReserved, .offset = 0xA1, .size = 7},
    // 0xA8: Video: EGA/MCGA/VGA ptr to Video Save Pointer Table
    {.field = kBDAVideoSavePointerTable, .offset = 0xA8, .size = 4},
    // 0xAC: Reserved (4 bytes)
    {.field = kBDAReservedAC, .offset = 0xAC, .size = 4},
    // 0xB0: ptr to 3363 Optical disk driver or BIOS entry point
    {.field = kBDAOpticalDiskPtr, .offset = 0xB0, .size = 4},
    // 0xB4: Reserved (2 bytes)
    {.field = kBDAReservedB4, .offset = 0xB4, .size = 2},
    // 0xB6: Reserved for POST (3 bytes)
    {.field = kBDAReservedPost, .offset = 0xB6, .size = 3},
    // 0xB9: Unknown (7 bytes)
    {.field = kBDAUnknownB9, .offset = 0xB9, .size = 7},
    // 0xC0: Reserved (14 bytes)
    {.field = kBDAReservedC0, .offset = 0xC0, .size = 14},
    // 0xCE: Count of days since last boot
    {.field = kBDADaysSinceBoot, .offset = 0xCE, .size = 2},
    // 0xD0: Reserved (32 bytes)
    {.field = kBDAReservedD0, .offset = 0xD0, .size = 32},
    // 0xF0: Reserved for user (16 bytes)
    {.field = kBDAUserReserved, .offset = 0xF0, .size = 16},
};
