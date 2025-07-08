#ifndef YAX86_BIOS_INTERRUPTS_H
#define YAX86_BIOS_INTERRUPTS_H

// Signature of a BIOS interrupt handler, i.e. a function that handles
// an interrupt number.
typedef ExecuteStatus (*BIOSInterruptHandler)(
    BIOSState* bios, CPUState* cpu, uint8_t ah);

// Signature of a BIOS interrupt function handler, i.e. a function that handles
// a specific interrupt number and AH register value.
typedef ExecuteStatus (*BIOSInterruptFunctionHandler)(
    BIOSState* bios, CPUState* cpu);

enum {
  // Number of BIOS functions for interrupt 0x10.
  kNumBIOSInterrupt10Functions = 0x14,
  // Number of BIOS functions for interrupt 0x13.
  kNumBIOSInterrupt13Functions = 0x18,
  // Number of BIOS functions for interrupt 0x14.
  kNumBIOSInterrupt14Functions = 0x04,
  // Number of BIOS functions for interrupt 0x15.
  kNumBIOSInterrupt15Functions = 0x04,
  // Number of BIOS functions for interrupt 0x16.
  kNumBIOSInterrupt16Functions = 0x03,
  // Number of BIOS functions for interrupt 0x17.
  kNumBIOSInterrupt17Functions = 0x03,
  // Number of BIOS functions for interrupt 0x1A.
  kNumBIOSInterrupt1AFunctions = 0x08,
};

#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"

// BIOS interrupt 0x05 - Print screen
// The result of the print screen operation is reported in the status byte at
// address 0x500:
//   - 0x00: Print screen successful
//   - 0x01: Print screen in progress
//   - 0xFF: Print screen failed
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt05PrintScreen(BIOSState* bios, CPUState* cpu, uint8_t ah);
// BIOS interrupt 0x10 - Video I/O
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10VideoIO(BIOSState* bios, CPUState* cpu, uint8_t ah);
// BIOS interrupt 0x11 - Equipment determination
YAX86_PRIVATE ExecuteStatus HandleBIOSInterrupt11EquipmentDetermination(
    BIOSState* bios, CPUState* cpu, uint8_t ah);
// BIOS interrupt 0x12 - Memory size determination
YAX86_PRIVATE ExecuteStatus HandleBIOSInterrupt12MemorySizeDetermination(
    BIOSState* bios, CPUState* cpu, uint8_t ah);
// BIOS interrupt 0x13 - Disk I/O
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt13DiskIO(BIOSState* bios, CPUState* cpu, uint8_t ah);
// BIOS interrupt 0x14 - RS-232 Serial I/O
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt14SerialIO(BIOSState* bios, CPUState* cpu, uint8_t ah);
// BIOS interrupt 0x15 - Cassette Tape I/O
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt15CassetteTapeIO(BIOSState* bios, CPUState* cpu, uint8_t ah);
// BIOS interrupt 0x16 - Keyboard I/O
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt16KeyboardIO(BIOSState* bios, CPUState* cpu, uint8_t ah);
// BIOS interrupt 0x17 - Printer I/O
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt17PrinterIO(BIOSState* bios, CPUState* cpu, uint8_t ah);
// BIOS interrupt 0x18 - ROM BASIC
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt18ROMBASIC(BIOSState* bios, CPUState* cpu, uint8_t ah);
// BIOS interrupt 0x19 - Bootstrap Loader (reboot the system)
YAX86_PRIVATE ExecuteStatus HandleBIOSInterrupt19BootstrapLoader(
    BIOSState* bios, CPUState* cpu, uint8_t ah);
// BIOS interrupt 0x1A - Time-of-Day
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt1ATimeOfDay(BIOSState* bios, CPUState* cpu, uint8_t ah);

// No-op BIOS interrupt function handler.
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterruptNoOp(BIOSState* bios, CPUState* cpu);

// BIOS Interrupt 0x10, AH = 0x00 - Set video mode
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH00SetVideoMode(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x10, AH = 0x01 - Set cursor shape
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH01SetCursorShape(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x10, AH = 0x02 - Set cursor position
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH02SetCursorPosition(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x10, AH = 0x03 - Read cursor position
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH03ReadCursorPosition(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x10, AH = 0x04 - Read light pen position
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH04ReadLightPenPosition(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x10, AH = 0x05 - Set active display page
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH05SetActiveDisplayPage(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x10, AH = 0x06 - Scroll active page up
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH06ScrollActivePageUp(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x10, AH = 0x07 - Scroll active page down
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH07ScrollActivePageDown(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x10, AH = 0x08 - Read character and attribute
YAX86_PRIVATE ExecuteStatus HandleBIOSInterrupt10AH08ReadCharacterAndAttribute(
    BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x10, AH = 0x09 - Write character and attribute
YAX86_PRIVATE ExecuteStatus HandleBIOSInterrupt10AH09WriteCharacterAndAttribute(
    BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x10, AH = 0x0A - Write character
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH0AWriteCharacter(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x10, AH = 0x0B - Set color palette
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH0BSetColorPalette(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x10, AH = 0x0C - Write dot
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH0CWriteDot(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x10, AH = 0x0D - Read dot
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH0DReadDot(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x10, AH = 0x0E - Write character as teletype
YAX86_PRIVATE ExecuteStatus HandleBIOSInterrupt10AH0EWriteCharacterAsTeletype(
    BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x10, AH = 0x0F - Get current video mode
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH0FGetCurrentVideoMode(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x10, AH = 0x13 - Write string
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt10AH13WriteString(BIOSState* bios, CPUState* cpu);

// BIOS Interrupt 0x13, AH = 0x00 - Reset disk
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt13AH00ResetDisk(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x13, AH = 0x01 - Get disk status
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt13AH01GetDiskStatus(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x13, AH = 0x02 - Read disk sectors
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt13AH02ReadDiskSectors(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x13, AH = 0x03 - Write disk sectors
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt13AH03WriteDiskSectors(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x13, AH = 0x04 - Verify disk sectors
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt13AH04VerifyDiskSectors(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x13, AH = 0x05 - Format track
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt13AH05FormatTrack(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x13, AH = 0x08 - Get current drive parameters
YAX86_PRIVATE ExecuteStatus HandleBIOSInterrupt13AH08GetCurrentDriveParameters(
    BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x13, AH = 0x09 - Initialize drive pair characteristics
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt13AH09InitializeDrivePairCharacteristics(
    BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x13, AH = 0x0A - Read long
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt13AH0AReadLong(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x13, AH = 0x0B - Write long
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt13AH0BWriteLong(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x13, AH = 0x0C - Seek
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt13AH0CSeek(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x13, AH = 0x0D - Reset hard disk
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt13AH0DResetHardDisk(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x13, AH = 0x10 - Test for drive ready
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt13AH10TestForDriveReady(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x13, AH = 0x11 - Recalibrate drive
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt13AH11RecalibrateDrive(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x13, AH = 0x14 - Controller internal diagnostic
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt13AH14ControllerInternalDiagnostic(
    BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x13, AH = 0x15 - Get disk type
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt13AH15GetDiskType(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x13, AH = 0x16 - Disk change status
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt13AH16DiskChangeStatus(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x13, AH = 0x17 - Set disk type
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt13AH17SetDiskType(BIOSState* bios, CPUState* cpu);

// BIOS Interrupt 0x14, AH = 0x00 - Initialize serial port
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt14AH00InitializeSerialPort(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x14, AH = 0x01 - Send one character
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt14AH01SendOneCharacter(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x14, AH = 0x02 - Receive one character
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt05AH02ReceiveOneCharacter(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x14, AH = 0x03 - Get serial port status
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt14AH03GetSerialPortStatus(BIOSState* bios, CPUState* cpu);

// BIOS Interrupt 0x15, AH = 0x00 - Turn on cassette motor
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt15AH00TurnOnCassetteMotor(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x15, AH = 0x01 - Turn off cassette motor
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt15AH01TurnOffCassetteMotor(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x15, AH = 0x02 - Read blocks of data
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt15AH02ReadCassetteDataBlocks(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x15, AH = 0x03 - Write blocks of data
YAX86_PRIVATE ExecuteStatus HandleBIOSInterrupt15AH03WriteCassetteDataBlocks(
    BIOSState* bios, CPUState* cpu);

// BIOS Interrupt 0x16, AH = 0x00 - Read next keyboard character
YAX86_PRIVATE ExecuteStatus HandleBIOSInterrupt16AH00ReadNextKeyboardCharacter(
    BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x16, AH = 0x01 - Determine whether character is available
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt16AH01DetermineCharacterAvailable(
    BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x16, AH = 0x02 - Get current shift status
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt16AH02GetCurrentShiftStatus(BIOSState* bios, CPUState* cpu);

// BIOS Interrupt 0x17, AH = 0x00 - Print character
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt17AH00PrintCharacter(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x17, AH = 0x01 - Initialize printer port
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt17AH01InitializePrinterPort(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x17, AH = 0x02 - Get printer status
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt17AH02GetPrinterStatus(BIOSState* bios, CPUState* cpu);

// BIOS Interrupt 0x1A, AH = 0x00 - Read the current clock value
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt1AAH00ReadCurrentClockValue(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x1A, AH = 0x01 - Set the current clock value
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt05AH01SetCurrentClockValue(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x1A, AH = 0x02 - Read the time from the battery-powered clock
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt1AAH02ReadRealTimeClockTime(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x1A, AH = 0x03 - Set the time on the battery-powered clock
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt1AAH03SetRealTimeClockTime(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x1A, AH = 0x04 - Read the date from the battery-powered clock
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt1AAH04ReadRealTimeClockDate(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x1A, AH = 0x05 - Set the date on the battery-powered clock
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt1AAH05SetRealTimeClockDate(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x1A, AH = 0x06 - Set the alarm
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt1AAH06SetAlarm(BIOSState* bios, CPUState* cpu);
// BIOS Interrupt 0x1A, AH = 0x07 - Reset the alarm
YAX86_PRIVATE ExecuteStatus
HandleBIOSInterrupt1AAH07ResetAlarm(BIOSState* bios, CPUState* cpu);

// Function handlers for BIOS interrupt 0x10.
YAX86_PRIVATE BIOSInterruptFunctionHandler
    bios_interrupt_10_handlers[kNumBIOSInterrupt10Functions];
// Function handlers for BIOS interrupt 0x13.
YAX86_PRIVATE BIOSInterruptFunctionHandler
    bios_interrupt_13_handlers[kNumBIOSInterrupt13Functions];
// Function handlers for BIOS interrupt 0x14.
YAX86_PRIVATE BIOSInterruptFunctionHandler
    bios_interrupt_14_handlers[kNumBIOSInterrupt14Functions];
// Function handlers for BIOS interrupt 0x15.
YAX86_PRIVATE BIOSInterruptFunctionHandler
    bios_interrupt_15_handlers[kNumBIOSInterrupt15Functions];
// Function handlers for BIOS interrupt 0x16.
YAX86_PRIVATE BIOSInterruptFunctionHandler
    bios_interrupt_16_handlers[kNumBIOSInterrupt16Functions];
// Function handlers for BIOS interrupt 0x17.
YAX86_PRIVATE BIOSInterruptFunctionHandler
    bios_interrupt_17_handlers[kNumBIOSInterrupt17Functions];
// Function handlers for BIOS interrupt 0x1A.
YAX86_PRIVATE BIOSInterruptFunctionHandler
    bios_interrupt_1A_handlers[kNumBIOSInterrupt1AFunctions];

#endif  // YAX86_IMPLEMENTATION

#endif  // YAX86_CPU_INSTRUCTIONS_H
