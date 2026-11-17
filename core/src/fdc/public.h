// Public interface for the Floppy Disk Controller (FDC) module.
#ifndef YAX86_FDC_PUBLIC_H
#define YAX86_FDC_PUBLIC_H

// This module emulates the NEC uPD765 Floppy Disk Controller. It handles I/O
// port communication and DMA transfers for floppy operations. Actual disk
// image access is delegated to the platform via callbacks.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef YAX86_FDC_BUNDLE_H
#include "../util/log.h"
#include "../util/static_vector.h"
#endif  // YAX86_FDC_BUNDLE_H

enum {
  // Log module ID for the FDC.
  Y_FDC_LOG_MODULE_ID = 5,
};

// Log module for the FDC.
Y_PUBLIC_HEADER const YLogModule YFDCLogModule = {
    .id = Y_FDC_LOG_MODULE_ID,
    .name = "fdc",
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

// The 5.25" 360KB double-sided double-density floppy disk format.
enum {
  Y_FDC_FORMAT_360KB_NUM_HEADS = 2,
  Y_FDC_FORMAT_360KB_NUM_TRACKS = 40,
  Y_FDC_FORMAT_360KB_NUM_SECTORS_PER_TRACK = 9,
  Y_FDC_FORMAT_360KB_SECTOR_SIZE = 512,
  // Size in bytes of an image of such a disk, so that a host can size a buffer
  // for one at compile time.
  Y_FDC_FORMAT_360KB_IMAGE_SIZE = Y_FDC_FORMAT_360KB_NUM_HEADS *
      Y_FDC_FORMAT_360KB_NUM_TRACKS * Y_FDC_FORMAT_360KB_NUM_SECTORS_PER_TRACK *
      Y_FDC_FORMAT_360KB_SECTOR_SIZE,
};
Y_PUBLIC_HEADER const YFDCDiskFormat YFDCFormat360KB = {
    .num_heads = Y_FDC_FORMAT_360KB_NUM_HEADS,
    .num_tracks = Y_FDC_FORMAT_360KB_NUM_TRACKS,
    .num_sectors_per_track = Y_FDC_FORMAT_360KB_NUM_SECTORS_PER_TRACK,
    .sector_size = Y_FDC_FORMAT_360KB_SECTOR_SIZE,
};

// The 3.5" 720KB double-sided double-density floppy disk format.
enum {
  Y_FDC_FORMAT_720KB_NUM_HEADS = 2,
  Y_FDC_FORMAT_720KB_NUM_TRACKS = 80,
  Y_FDC_FORMAT_720KB_NUM_SECTORS_PER_TRACK = 9,
  Y_FDC_FORMAT_720KB_SECTOR_SIZE = 512,
  // Size in bytes of an image of such a disk, so that a host can size a buffer
  // for one at compile time.
  Y_FDC_FORMAT_720KB_IMAGE_SIZE = Y_FDC_FORMAT_720KB_NUM_HEADS *
      Y_FDC_FORMAT_720KB_NUM_TRACKS * Y_FDC_FORMAT_720KB_NUM_SECTORS_PER_TRACK *
      Y_FDC_FORMAT_720KB_SECTOR_SIZE,
};
Y_PUBLIC_HEADER const YFDCDiskFormat YFDCFormat720KB = {
    .num_heads = Y_FDC_FORMAT_720KB_NUM_HEADS,
    .num_tracks = Y_FDC_FORMAT_720KB_NUM_TRACKS,
    .num_sectors_per_track = Y_FDC_FORMAT_720KB_NUM_SECTORS_PER_TRACK,
    .sector_size = Y_FDC_FORMAT_720KB_SECTOR_SIZE,
};

// The 5.25" 1.2MB double-sided high-density floppy disk format.
enum {
  Y_FDC_FORMAT_1200KB_NUM_HEADS = 2,
  Y_FDC_FORMAT_1200KB_NUM_TRACKS = 80,
  Y_FDC_FORMAT_1200KB_NUM_SECTORS_PER_TRACK = 15,
  Y_FDC_FORMAT_1200KB_SECTOR_SIZE = 512,
  // Size in bytes of an image of such a disk, so that a host can size a buffer
  // for one at compile time.
  Y_FDC_FORMAT_1200KB_IMAGE_SIZE = Y_FDC_FORMAT_1200KB_NUM_HEADS *
      Y_FDC_FORMAT_1200KB_NUM_TRACKS *
      Y_FDC_FORMAT_1200KB_NUM_SECTORS_PER_TRACK *
      Y_FDC_FORMAT_1200KB_SECTOR_SIZE,
};
Y_PUBLIC_HEADER const YFDCDiskFormat YFDCFormat1200KB = {
    .num_heads = Y_FDC_FORMAT_1200KB_NUM_HEADS,
    .num_tracks = Y_FDC_FORMAT_1200KB_NUM_TRACKS,
    .num_sectors_per_track = Y_FDC_FORMAT_1200KB_NUM_SECTORS_PER_TRACK,
    .sector_size = Y_FDC_FORMAT_1200KB_SECTOR_SIZE,
};

// The 3.5" 1.44MB double-sided high-density floppy disk format.
enum {
  Y_FDC_FORMAT_1440KB_NUM_HEADS = 2,
  Y_FDC_FORMAT_1440KB_NUM_TRACKS = 80,
  Y_FDC_FORMAT_1440KB_NUM_SECTORS_PER_TRACK = 18,
  Y_FDC_FORMAT_1440KB_SECTOR_SIZE = 512,
  // Size in bytes of an image of such a disk, so that a host can size a buffer
  // for one at compile time.
  Y_FDC_FORMAT_1440KB_IMAGE_SIZE = Y_FDC_FORMAT_1440KB_NUM_HEADS *
      Y_FDC_FORMAT_1440KB_NUM_TRACKS *
      Y_FDC_FORMAT_1440KB_NUM_SECTORS_PER_TRACK *
      Y_FDC_FORMAT_1440KB_SECTOR_SIZE,
};
Y_PUBLIC_HEADER const YFDCDiskFormat YFDCFormat1440KB = {
    .num_heads = Y_FDC_FORMAT_1440KB_NUM_HEADS,
    .num_tracks = Y_FDC_FORMAT_1440KB_NUM_TRACKS,
    .num_sectors_per_track = Y_FDC_FORMAT_1440KB_NUM_SECTORS_PER_TRACK,
    .sector_size = Y_FDC_FORMAT_1440KB_SECTOR_SIZE,
};

enum {
  // Number of floppy disk formats supported.
  Y_FDC_NUM_FORMATS = 4,
  // Largest image size in bytes among the supported floppy disk formats, so
  // that a host can size a buffer for any floppy image at compile time.
  Y_FDC_MAX_IMAGE_SIZE = Y_FDC_FORMAT_1440KB_IMAGE_SIZE,
};

// Returns the size in bytes of an image of format.
Y_PUBLIC_HEADER inline uint32_t YFDCFormatImageSize(
    const YFDCDiskFormat* format) {
  return (uint32_t)format->num_heads * format->num_tracks *
         format->num_sectors_per_track * format->sector_size;
}

// The supported floppy disk formats, in ascending order of image size.
Y_PUBLIC_HEADER const YFDCDiskFormat* const YFDCFormats[Y_FDC_NUM_FORMATS] = {
    &YFDCFormat360KB,
    &YFDCFormat720KB,
    &YFDCFormat1200KB,
    &YFDCFormat1440KB,
};

// Image sizes in bytes of the supported floppy disk formats, in ascending
// order.
Y_PUBLIC_HEADER const uint32_t YFDCFormatImageSizes[Y_FDC_NUM_FORMATS] = {
    Y_FDC_FORMAT_360KB_IMAGE_SIZE,
    Y_FDC_FORMAT_720KB_IMAGE_SIZE,
    Y_FDC_FORMAT_1200KB_IMAGE_SIZE,
    Y_FDC_FORMAT_1440KB_IMAGE_SIZE,
};

// Returns the floppy disk format for an image of image_size bytes, or NULL if
// image_size is not a supported floppy image size.
Y_PUBLIC_HEADER inline const YFDCDiskFormat* YFDCFindFormat(
    uint32_t image_size) {
  for (uint8_t i = 0; i < Y_FDC_NUM_FORMATS; ++i) {
    if (YFDCFormatImageSize(YFDCFormats[i]) == image_size) {
      return YFDCFormats[i];
    }
  }
  return NULL;
}

// Type of a floppy drive mechanism. Values 1 to 4 match the PC/AT CMOS and
// Multi-Floppy BIOS Extension drive type codes.
typedef enum YFDCDriveType {
  // Drive not configured (defaults to a 360KB drive for controller operations,
  // and marks a drive absent in the Multi-Floppy BIOS configuration block).
  Y_FDC_DRIVE_TYPE_NONE = 0,
  // 5.25" 360KB double-density drive (40 tracks).
  Y_FDC_DRIVE_TYPE_360KB = 1,
  // 5.25" 1.2MB high-density drive (80 tracks; double-steps 40-track media at
  // 300kbps).
  Y_FDC_DRIVE_TYPE_1200KB = 2,
  // 3.5" 720KB double-density drive (80 tracks).
  Y_FDC_DRIVE_TYPE_720KB = 3,
  // 3.5" 1.44MB high-density drive (80 tracks).
  Y_FDC_DRIVE_TYPE_1440KB = 4,
  Y_FDC_NUM_DRIVE_TYPES = 5,
} YFDCDriveType;

// Returns whether a drive of drive_type accepts a disk of format. An
// unconfigured drive (Y_FDC_DRIVE_TYPE_NONE) defaults to a 360KB drive.
Y_PUBLIC_HEADER inline bool YFDCDoesDriveTypeSupportFormat(
    YFDCDriveType drive_type, const YFDCDiskFormat* format) {
  if (format == NULL) {
    return false;
  }
  const uint32_t size = YFDCFormatImageSize(format);
  switch (drive_type) {
    case Y_FDC_DRIVE_TYPE_NONE:
    case Y_FDC_DRIVE_TYPE_360KB:
      return size == Y_FDC_FORMAT_360KB_IMAGE_SIZE;
    case Y_FDC_DRIVE_TYPE_720KB:
      return size == Y_FDC_FORMAT_720KB_IMAGE_SIZE;
    case Y_FDC_DRIVE_TYPE_1200KB:
      return size == Y_FDC_FORMAT_360KB_IMAGE_SIZE ||
             size == Y_FDC_FORMAT_1200KB_IMAGE_SIZE;
    case Y_FDC_DRIVE_TYPE_1440KB:
      return size == Y_FDC_FORMAT_720KB_IMAGE_SIZE ||
             size == Y_FDC_FORMAT_1440KB_IMAGE_SIZE;
    default:
      return false;
  }
}

// Data transfer rates, as written to bits 1-0 of the Configuration Control
// Register (CCR, port 0x3F7).
typedef enum YFDCDataRate {
  Y_FDC_DATA_RATE_500KBPS = 0,
  Y_FDC_DATA_RATE_300KBPS = 1,
  Y_FDC_DATA_RATE_250KBPS = 2,
  Y_FDC_DATA_RATE_1MBPS = 3,
} YFDCDataRate;

enum {
  // Start address of the floppy option ROM. Clear of the XTIDE ROM at
  // 0xC8000-0xCAFFF and aligned to the memory map's 4KB pages.
  Y_FDC_OPTION_ROM_START_ADDRESS = 0xCC000,
  // Offset and size in bytes of the configuration block at the end of the
  // 8KB Multi-Floppy BIOS Extension ROM (0x1FC0-0x1FFF).
  Y_FDC_OPTION_ROM_CONFIG_OFFSET = 0x1FC0,
  Y_FDC_OPTION_ROM_CONFIG_SIZE = 64,
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
  // Digital Input Register (read-only) and Configuration Control Register
  // (write-only).
  Y_FDC_PORT_DIR = 0x3F7,
  Y_FDC_PORT_CCR = 0x3F7,
} YFDCPort;

// Flags for the Digital Input Register (DIR, port 0x3F7 read) and
// Configuration Control Register (CCR, port 0x3F7 write).
enum {
  // Bit 7 of DIR: the selected drive's disk-change line (1 = disk changed).
  Y_FDC_DIR_DISK_CHANGED = 1 << 7,
  // Bits 6-0 of DIR are undriven by the floppy controller and read as 1.
  Y_FDC_DIR_UNDRIVEN_BITS = 0x7F,
  // Bits 1-0 of CCR: the data transfer rate (YFDCDataRate).
  Y_FDC_CCR_DATA_RATE_MASK = 0x03,
};

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
  bool is_present;
  // The format of the disk currently inserted in the drive.
  const YFDCDiskFormat* format;
  // The track the read/write head is currently on.
  uint8_t track;
  // The currently active head (0 or 1).
  uint8_t head;
  // Whether the drive is currently busy.
  bool is_busy;
  // Status Register 0 (ST0) for the last completed Seek or Recalibrate
  // operation on this drive.
  uint8_t st0;
  // Whether there is a pending interrupt from a completed Seek or Recalibrate
  // operation on this drive.
  bool has_pending_interrupt;
  // Whether the disk-change line is active for this drive. Set when a disk is
  // inserted or ejected, and cleared by a Seek or Recalibrate when a disk is
  // in the drive.
  bool is_disk_changed;
} YFDCDriveState;

// Caller-provided runtime configuration for the FDC.
typedef struct YFDCConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger for this module. May be NULL.
  YLogger* logger;

  // Type of each drive's mechanism. Y_FDC_DRIVE_TYPE_NONE (0), the default,
  // marks the drive absent in the Multi-Floppy BIOS configuration block and
  // defaults to a 360KB drive for controller operations.
  YFDCDriveType drive_types[Y_FDC_NUM_DRIVES];

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
  // Active data transfer rate, initialized to Y_FDC_DATA_RATE_250KBPS by
  // YFDCInit() and updated by writes to the Configuration Control Register
  // (port 0x3F7).
  YFDCDataRate data_rate;
  // The 64-byte configuration block served at offset 0x1FC0 of the Multi-Floppy
  // BIOS Extension option ROM, built once by YFDCInit() from
  // config.drive_types.
  uint8_t option_rom_config[Y_FDC_OPTION_ROM_CONFIG_SIZE];

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
    // Whether the command's MT bit is set, so that the transfer goes on from
    // head 0 to head 1 of the same cylinder.
    bool is_multi_track;

    // Internal tracking.
    uint32_t current_offset;  // Current byte offset in the disk image.
    // Current byte index within the current sector; for Format Track, the
    // ID bytes taken so far.
    uint16_t sector_byte_index;
    uint8_t data_register;  // Buffer for the byte currently being transferred.
    // Whether DREQ is asserted, so the controller is waiting for the DMA
    // controller to move a byte.
    bool is_dma_request_active;
    // Whether the DMA controller has signalled TC (Terminal Count), the end of
    // the transfer it was programmed for.
    bool was_tc_received;
  } transfer;
} YFDCState;

// Initializes the FDC to its power-on state.
Y_PUBLIC void YFDCInit(YFDCState* fdc);

// Returns the size of the option ROM in bytes.
Y_PUBLIC uint32_t YFDCGetOptionROMSize(void);

// Returns a pointer to the raw option ROM image, YFDCGetOptionROMSize() bytes
// of it.
Y_PUBLIC const uint8_t* YFDCGetOptionROMData(void);

// Reads a byte at offset into the option ROM, serving the 64-byte configuration
// block at 0x1FC0 from fdc->option_rom_config and the rest from the ROM image.
Y_PUBLIC uint8_t YFDCReadOptionROMByte(const YFDCState* fdc, uint32_t offset);

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

#endif  // YAX86_FDC_PUBLIC_H
