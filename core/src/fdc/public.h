// Public interface for the Floppy Disk Controller (FDC) module.
#ifndef Y_FDC_PUBLIC_H
#define Y_FDC_PUBLIC_H

// This module emulates the NEC uPD765 Floppy Disk Controller. It handles I/O
// port communication and DMA transfers for floppy operations. Actual disk
// image access is delegated to the platform via callbacks.

#include <stdbool.h>
#include <stdint.h>

#ifndef Y_FDC_BUNDLE_H
#include "../util/log.h"
#include "../util/static_vector.h"
#endif  // Y_FDC_BUNDLE_H

enum {
  // Log module ID for the FDC.
  Y_FDC_LOG_MODULE_ID = 5,
};

// Log module for the FDC.
Y_PUBLIC_HEADER const YLogModule YFDCLogModule = {
    .id = Y_FDC_LOG_MODULE_ID,
    .name = "FDC",
};

// Floppy disk format configuration.
typedef struct YFDCDiskFormat {
  // Number of heads (1 or 2).
  uint8_t num_heads;
  // Number of tracks.
  uint8_t num_tracks;
  // Number of sectors per track.
  uint8_t num_sectors_per_track;
  // Size of each sector in bytes.
  uint16_t sector_size;
} YFDCDiskFormat;

// 5.25" 360KB double-sided double-density floppy disk format.
Y_PUBLIC_HEADER const YFDCDiskFormat YFDCFormat360KB = {
    .num_heads = 2,
    .num_tracks = 40,
    .num_sectors_per_track = 9,
    .sector_size = 512,
};

struct YFDCState;

enum {
  // Number of floppy drives supported by the FDC.
  Y_FDC_NUM_DRIVES = 4,
  // Maximum size of a command request.
  Y_FDC_COMMAND_BUFFER_SIZE = 9,
  // Maximum size of a command result.
  Y_FDC_RESULT_BUFFER_SIZE = 7,
};

// Command phases of the FDC.
typedef enum YFDCCommandPhase {
  // No command in progress.
  Y_FDC_PHASE_IDLE = 0,
  // Command has been issued, waiting for parameters.
  Y_FDC_PHASE_COMMAND,
  // Command parameters received, executing command.
  Y_FDC_PHASE_EXECUTION,
  // Command execution complete, sending result bytes.
  Y_FDC_PHASE_RESULT,
} YFDCCommandPhase;

// I/O ports for the FDC.
typedef enum YFDCPort {
  // Digital Output Register (write-only).
  Y_FDC_PORT_DOR = 0x3F2,
  // Main Status Register (read-only).
  Y_FDC_PORT_MSR = 0x3F4,
  // Data Register (read/write).
  Y_FDC_PORT_DATA = 0x3F5,
} YFDCPort;

// Flags for the Main Status Register (MSR).
enum {
  // Drive 0 is busy with a seek or recalibrate command.
  Y_FDC_MSR_DRIVE0_BUSY = 1 << 0,
  // Drive 1 is busy with a seek or recalibrate command.
  Y_FDC_MSR_DRIVE1_BUSY = 1 << 1,
  // Drive 2 is busy with a seek or recalibrate command.
  Y_FDC_MSR_DRIVE2_BUSY = 1 << 2,
  // Drive 3 is busy with a seek or recalibrate command.
  Y_FDC_MSR_DRIVE3_BUSY = 1 << 3,
  // A command is in progress.
  Y_FDC_MSR_BUSY = 1 << 4,
  // The FDC is in non-DMA mode.
  Y_FDC_MSR_NON_DMA_MODE = 1 << 5,
  // Indicates direction of data transfer. 0 = write to FDC, 1 = read from FDC.
  Y_FDC_MSR_DATA_DIRECTION = 1 << 6,
  // The Data Register is ready to send or receive data to/from the CPU.
  Y_FDC_MSR_REQUEST_FOR_MASTER = 1 << 7,
};

// Flags for the Digital Output Register (DOR).
enum {
  // Drive selection (0-3).
  Y_FDC_DOR_DRIVE_SELECT_MASK = 0x03,
  // Controller reset (0 = Reset active, 1 = Controller enabled).
  Y_FDC_DOR_RESET = 1 << 2,
  // DMA and Interrupt enable (1 = enabled).
  Y_FDC_DOR_INTERRUPT_ENABLE = 1 << 3,
  // Motor enable flags for drives 0-3.
  Y_FDC_DOR_MOTOR0_ENABLE = 1 << 4,
  Y_FDC_DOR_MOTOR1_ENABLE = 1 << 5,
  Y_FDC_DOR_MOTOR2_ENABLE = 1 << 6,
  Y_FDC_DOR_MOTOR3_ENABLE = 1 << 7,
};

// Flags for Status Register 0 (ST0).
enum {
  // Bits 7-6: Interrupt Code
  // 00 = Normal termination
  // 01 = Abnormal termination
  // 10 = Invalid command
  // 11 = Abnormal termination due to polling (Post-reset)
  Y_FDC_ST0_INTERRUPT_CODE_MASK = 0xC0,
  Y_FDC_ST0_NORMAL_TERMINATION = 0x00,
  Y_FDC_ST0_ABNORMAL_TERMINATION = 0x40,
  Y_FDC_ST0_INVALID_COMMAND = 0x80,
  Y_FDC_ST0_ABNORMAL_TERMINATION_POLLING = 0xC0,

  // Bit 5: Seek End
  Y_FDC_ST0_SEEK_END = 1 << 5,
  // Bit 4: Equipment Check
  Y_FDC_ST0_EQUIPMENT_CHECK = 1 << 4,
  // Bit 3: Not Ready
  Y_FDC_ST0_NOT_READY = 1 << 3,
  // Bit 2: Head Address
  Y_FDC_ST0_HEAD_ADDRESS = 1 << 2,
  // Bits 1-0: Drive Select
  Y_FDC_ST0_UNIT_SELECT_MASK = 0x03,
};

// Flags for Status Register 1 (ST1).
enum {
  // Bit 7: End of Cylinder
  Y_FDC_ST1_END_OF_CYLINDER = 1 << 7,
  // Bit 5: Data Error
  Y_FDC_ST1_DATA_ERROR = 1 << 5,
  // Bit 4: Overrun
  Y_FDC_ST1_OVERRUN = 1 << 4,
  // Bit 2: No Data
  Y_FDC_ST1_NO_DATA = 1 << 2,
  // Bit 1: Not Writable
  Y_FDC_ST1_NOT_WRITABLE = 1 << 1,
  // Bit 0: Missing Address Mark
  Y_FDC_ST1_MISSING_ADDRESS_MARK = 1 << 0,
};

// Flags for Status Register 2 (ST2).
enum {
  // Bit 6: Control Mark
  Y_FDC_ST2_CONTROL_MARK = 1 << 6,
  // Bit 5: Data Error in Data Field
  Y_FDC_ST2_DATA_ERROR_IN_DATA_FIELD = 1 << 5,
  // Bit 4: Wrong Cylinder
  Y_FDC_ST2_WRONG_CYLINDER = 1 << 4,
  // Bit 3: Scan Equal Hit
  Y_FDC_ST2_SCAN_EQUAL_HIT = 1 << 3,
  // Bit 2: Scan Not Satisfied
  Y_FDC_ST2_SCAN_NOT_SATISFIED = 1 << 2,
  // Bit 1: Bad Cylinder
  Y_FDC_ST2_BAD_CYLINDER = 1 << 1,
  // Bit 0: Missing Address Mark in Data Field
  Y_FDC_ST2_MISSING_ADDRESS_MARK_IN_DATA_FIELD = 1 << 0,
};

// State for a single floppy drive.
typedef struct YFDCDriveState {
  // Whether there is a disk inserted in the drive, i.e. whether an image is
  // mounted. The real hardware doesn't actually know this and will attempt to
  // access the disk and time out.
  bool present;
  // The format of the disk currently inserted in the drive.
  const YFDCDiskFormat* format;
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
} YFDCDriveState;

// Caller-provided runtime configuration for the FDC.
typedef struct YFDCConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger for this module. May be NULL.
  YLogger* logger;

  // Callback to raise an IRQ6 (FDC interrupt) to the CPU.
  void (*raise_irq6)(void* context);

  // Callback to signal the platform to execute a DMA cycle for Channel 2.
  // This represents the DREQ (DMA Request) signal.
  void (*request_dma)(void* context);

  // Callback to read a byte from a floppy image.
  uint8_t (*read_image_byte)(
      void* context,
      // 0 to Y_FDC_NUM_DRIVES-1
      uint8_t drive,
      // byte offset within the image
      uint32_t offset);

  // Callback to write a byte to a floppy image.
  void (*write_image_byte)(
      void* context,
      // 0 to Y_FDC_NUM_DRIVES-1
      uint8_t drive,
      // byte offset within the image
      uint32_t offset,
      // byte value to write
      uint8_t value);
} YFDCConfig;

Y_STATIC_VECTOR_TYPE(YFDCCommandBuffer, uint8_t, Y_FDC_COMMAND_BUFFER_SIZE)
Y_STATIC_VECTOR_TYPE(YFDCResultBuffer, uint8_t, Y_FDC_RESULT_BUFFER_SIZE)

struct YFDCCommandMetadata;

// State of the Floppy Disk Controller.
typedef struct YFDCState {
  // Pointer to the FDC configuration.
  YFDCConfig config;

  // Value of the Digital Output Register (DOR) from the last write to port
  // 0x3F2.
  uint8_t dor;

  // Per-drive state.
  YFDCDriveState drives[Y_FDC_NUM_DRIVES];

  // Current command phase.
  YFDCCommandPhase phase;

  // Command buffer to receive command and parameters from the CPU.
  YFDCCommandBuffer command_buffer;
  // Metadata for the command currently being processed.
  const struct YFDCCommandMetadata* current_command;
  // How many ticks the current command has been executing.
  uint32_t current_command_ticks;

  // Result buffer to send to the CPU.
  YFDCResultBuffer result_buffer;
  // Next index to read from result buffer.
  uint8_t next_result_byte_index;

  // State specific to the execution of a data transfer command
  // (Read/Write/Format).
  struct {
    // Current logical position on the disk during transfer.
    uint8_t cylinder;
    uint8_t head;
    uint8_t sector;

    // Command parameters governing the transfer.
    uint8_t sector_size_code;  // N
    uint8_t eot;               // End of Track sector number
    bool multi_track;          // MT bit set (read/write across heads)

    // Internal tracking.
    uint32_t current_offset;  // Current byte offset in the disk image.
    uint16_t
        sector_byte_index;  // Current byte index within the current sector.
    uint8_t data_register;  // Buffer for the byte currently being transferred.
    bool dma_request_active;  // DREQ is asserted, waiting for DMA access.
    bool tc_received;         // TC (Terminal Count) signal received from DMA.
  } transfer;
} YFDCState;

// Initializes the FDC to its power-on state.
Y_PUBLIC void YFDCInit(YFDCState* fdc);

// Handles reads from the FDC's I/O ports.
Y_PUBLIC uint8_t YFDCReadPort(YFDCState* fdc, uint16_t port);

// Handles writes to the FDC's I/O ports.
Y_PUBLIC void YFDCWritePort(YFDCState* fdc, uint16_t port, uint8_t value);

// Signals to the FDC that the DMA controller has reached the terminal count.
// This represents the TC signal.
Y_PUBLIC void YFDCHandleTC(YFDCState* fdc);

// Inserts a disk with the given format into the specified drive.
Y_PUBLIC void YFDCInsertDisk(
    YFDCState* fdc, uint8_t drive, const YFDCDiskFormat* format);

// Ejects the disk from the specified drive.
Y_PUBLIC void YFDCEjectDisk(YFDCState* fdc, uint8_t drive);

// Simulates a tick of the FDC, handling any timed operations.
Y_PUBLIC void YFDCTick(YFDCState* fdc);

#endif  // Y_FDC_PUBLIC_H
