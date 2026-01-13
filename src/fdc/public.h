// Public interface for the Floppy Disk Controller (FDC) module.
#ifndef YAX86_FDC_PUBLIC_H
#define YAX86_FDC_PUBLIC_H

// This module emulates the NEC uPD765 Floppy Disk Controller. It handles I/O
// port communication and DMA transfers for floppy operations. Actual disk
// image access is delegated to the platform via callbacks.

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_FDC_BUNDLE_H
#include "../util/static_vector.h"
#endif  // YAX86_FDC_BUNDLE_H

// Floppy disk format configuration.
typedef struct FDCDiskFormat {
  // Number of heads (1 or 2).
  uint8_t num_heads;
  // Number of tracks.
  uint8_t num_tracks;
  // Number of sectors per track.
  uint8_t num_sectors_per_track;
  // Size of each sector in bytes.
  uint16_t sector_size;
} FDCDiskFormat;

// 5.25" 360KB double-sided double-density floppy disk format.
static const FDCDiskFormat kFDCFormat360KB = {
    .num_heads = 2,
    .num_tracks = 40,
    .num_sectors_per_track = 9,
    .sector_size = 512,
};

struct FDCState;

enum {
  // Number of floppy drives supported by the FDC.
  kFDCNumDrives = 4,
  // Maximum size of a command request.
  kFDCCommandBufferSize = 9,
  // Maximum size of a command result.
  kFDCResultBufferSize = 7,
};

// Command phases of the FDC.
typedef enum FDCCommandPhase {
  // No command in progress.
  kFDCPhaseIdle = 0,
  // Command has been issued, waiting for parameters.
  kFDCPhaseCommand,
  // Command parameters received, executing command.
  kFDCPhaseExecution,
  // Command execution complete, sending result bytes.
  kFDCPhaseResult,
} FDCCommandPhase;

// I/O ports for the FDC.
typedef enum FDCPort {
  // Digital Output Register (write-only).
  kFDCPortDOR = 0x3F2,
  // Main Status Register (read-only).
  kFDCPortMSR = 0x3F4,
  // Data Register (read/write).
  kFDCPortData = 0x3F5,
} FDCPort;

// Flags for the Main Status Register (MSR).
enum {
  // Drive 0 is busy with a seek or recalibrate command.
  kFDCMSRDrive0Busy = 1 << 0,
  // Drive 1 is busy with a seek or recalibrate command.
  kFDCMSRDrive1Busy = 1 << 1,
  // Drive 2 is busy with a seek or recalibrate command.
  kFDCMSRDrive2Busy = 1 << 2,
  // Drive 3 is busy with a seek or recalibrate command.
  kFDCMSRDrive3Busy = 1 << 3,
  // A command is in progress.
  kFDCMSRBusy = 1 << 4,
  // The FDC is in non-DMA mode.
  kFDCMSRNonDMAMode = 1 << 5,
  // Indicates direction of data transfer. 0 = write to FDC, 1 = read from FDC.
  kFDCMSRDataDirection = 1 << 6,
  // The Data Register is ready to send or receive data to/from the CPU.
  kFDCMSRRequestForMaster = 1 << 7,
};

// Flags for the Digital Output Register (DOR).
enum {
  // Drive selection (0-3).
  kFDCDORDriveSelectMask = 0x03,
  // Controller reset (0 = Reset active, 1 = Controller enabled).
  kFDCDORReset = 1 << 2,
  // DMA and Interrupt enable (1 = enabled).
  kFDCDORInterruptEnable = 1 << 3,
  // Motor enable flags for drives 0-3.
  kFDCDORMotor0Enable = 1 << 4,
  kFDCDORMotor1Enable = 1 << 5,
  kFDCDORMotor2Enable = 1 << 6,
  kFDCDORMotor3Enable = 1 << 7,
};

// Flags for Status Register 0 (ST0).
enum {
  // Bits 7-6: Interrupt Code
  // 00 = Normal termination
  // 01 = Abnormal termination
  // 10 = Invalid command
  // 11 = Abnormal termination due to polling (Post-reset)
  kFDCST0InterruptCodeMask = 0xC0,
  kFDCST0NormalTermination = 0x00,
  kFDCST0AbnormalTermination = 0x40,
  kFDCST0InvalidCommand = 0x80,
  kFDCST0AbnormalTerminationPolling = 0xC0,

  // Bit 5: Seek End
  kFDCST0SeekEnd = 1 << 5,
  // Bit 4: Equipment Check
  kFDCST0EquipmentCheck = 1 << 4,
  // Bit 3: Not Ready
  kFDCST0NotReady = 1 << 3,
  // Bit 2: Head Address
  kFDCST0HeadAddress = 1 << 2,
  // Bits 1-0: Drive Select
  kFDCST0UnitSelectMask = 0x03,
};

// State for a single floppy drive.
typedef struct FDCDriveState {
  // Whether there is a disk inserted in the drive, i.e. whether an image is
  // mounted. The real hardware doesn't actually know this and will attempt to
  // access the disk and time out.
  bool present;
  // The format of the disk currently inserted in the drive.
  const FDCDiskFormat* format;
  // The track the read/write head is currently on.
  uint8_t track;
  // The currently active head (0 or 1).
  uint8_t head;
  // Whether the drive is currently busy.
  bool busy;
  // Status Register 0 (ST0) for the last completed Seek or Recalibrate
  // operation on this drive.
  uint8_t st0;
  // Whether there is a pending interrupt from a completed Seek or Recalibrate
  // operation on this drive.
  bool has_pending_interrupt;
} FDCDriveState;

// Caller-provided runtime configuration for the FDC.
typedef struct FDCConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Callback to raise an IRQ6 (FDC interrupt) to the CPU.
  void (*raise_irq6)(void* context);

  // Callback to read a byte from a floppy image.
  uint8_t (*read_image_byte)(
      void* context,
      // 0 to kFDCNumDrives-1
      uint8_t drive,
      // byte offset within the image
      uint32_t offset);

  // Callback to write a byte to a floppy image.
  void (*write_image_byte)(
      void* context,
      // 0 to kFDCNumDrives-1
      uint8_t drive,
      // byte offset within the image
      uint32_t offset,
      // byte value to write
      uint8_t value);
} FDCConfig;

STATIC_VECTOR_TYPE(FDCCommandBuffer, uint8_t, kFDCCommandBufferSize)
STATIC_VECTOR_TYPE(FDCResultBuffer, uint8_t, kFDCResultBufferSize)

struct FDCCommandMetadata;

// State of the Floppy Disk Controller.
typedef struct FDCState {
  // Pointer to the FDC configuration.
  FDCConfig* config;

  // Value of the Digital Output Register (DOR) from the last write to port 0x3F2.
  uint8_t dor;

  // Per-drive state.
  FDCDriveState drives[kFDCNumDrives];

  // Current command phase.
  FDCCommandPhase phase;

  // Command buffer to receive command and parameters from the CPU.
  FDCCommandBuffer command_buffer;
  // Metadata for the command currently being processed.
  const struct FDCCommandMetadata* current_command;
  // How many ticks the current command has been executing.
  uint32_t current_command_ticks;

  // Result buffer to send to the CPU.
  FDCResultBuffer result_buffer;
  // Next index to read from result buffer.
  uint8_t next_result_byte_index;
} FDCState;

// Initializes the FDC to its power-on state.
void FDCInit(FDCState* fdc, FDCConfig* config);

// Handles reads from the FDC's I/O ports.
uint8_t FDCReadPort(FDCState* fdc, uint16_t port);

// Handles writes to the FDC's I/O ports.
void FDCWritePort(FDCState* fdc, uint16_t port, uint8_t value);

// Inserts a disk with the given format into the specified drive.
void FDCInsertDisk(FDCState* fdc, uint8_t drive, const FDCDiskFormat* format);

// Ejects the disk from the specified drive.
void FDCEjectDisk(FDCState* fdc, uint8_t drive);

// Simulates a tick of the FDC, handling any timed operations.
void FDCTick(FDCState* fdc);

#endif  // YAX86_FDC_PUBLIC_H

