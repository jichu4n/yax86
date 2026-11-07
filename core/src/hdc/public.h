// Public interface for the Hard Disk Controller (HDC) module.
#ifndef YAX86_HDC_PUBLIC_H
#define YAX86_HDC_PUBLIC_H

// This module emulates an XT-IDE rev 2 hard disk controller - an 8-bit ATA
// task file operated in PIO mode, with no DMA and no interrupt line.
//
// The controller card also carries an option ROM, which is where the guest's
// INT 13h support for drives 80h and up actually comes from: GLaBIOS itself
// implements INT 13h for floppies only and rejects drive numbers above 3. Its
// POST scans C800:0000 and up for option ROMs and calls into whatever it
// finds, so the ROM this module maps is what makes a hard disk visible to DOS.
//
// The ROM is the XTIDE Universal BIOS, compiled into the library the same way
// the system BIOS is. Reading it from a file at run time would not work on
// targets like the Raspberry Pi Pico, which have no file system.

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_HDC_BUNDLE_H
#include "../util/log.h"
#endif  // YAX86_HDC_BUNDLE_H

enum {
  // Log module ID for the HDC.
  Y_HDC_LOG_MODULE_ID = 9,
};

// Log module for the HDC.
YAX86_PUBLIC_HEADER const YLogModule YHDCLogModule = {
    .id = Y_HDC_LOG_MODULE_ID,
    .name = "HDC",
};

enum {
  // Start address of the option ROM. GLaBIOS scans for option ROMs starting
  // here, on 2KB boundaries.
  Y_HDC_OPTION_ROM_START_ADDRESS = 0xC8000,
};

// Value returned for reads that no device answers.
enum {
  Y_HDC_OPEN_BUS_VALUE = 0xFF,
};

// I/O ports for the HDC.
enum {
  // Base I/O port of the task file. This is the XT-IDE's stock configuration,
  // which the option ROM reports at POST as "Master at 300h".
  Y_HDC_PORT_BASE = 0x300,
  // Number of I/O ports the card decodes.
  Y_HDC_NUM_PORTS = 16,
};

// Task file registers, given as offsets from Y_HDC_PORT_BASE after the card's
// address line swap has been undone. See YHDCPortOffsetToRegister().
typedef enum YHDCRegister {
  // Data register, low byte of each 16-bit word.
  Y_HDC_REGISTER_DATA = 0x0,
  // Error on read, features on write.
  Y_HDC_REGISTER_ERROR = 0x1,
  Y_HDC_REGISTER_SECTOR_COUNT = 0x2,
  // Sector number, or bits 7-0 of an LBA.
  Y_HDC_REGISTER_SECTOR_NUMBER = 0x3,
  // Cylinder low, or bits 15-8 of an LBA.
  Y_HDC_REGISTER_CYLINDER_LOW = 0x4,
  // Cylinder high, or bits 23-16 of an LBA.
  Y_HDC_REGISTER_CYLINDER_HIGH = 0x5,
  // Drive select and head, or bits 27-24 of an LBA.
  Y_HDC_REGISTER_DRIVE_HEAD = 0x6,
  // Status on read, command on write.
  Y_HDC_REGISTER_STATUS = 0x7,
  // Data register, high byte of each 16-bit word. Not an ATA register: the
  // drive's data port is 16 bits wide and the card is on an 8-bit bus, so the
  // card latches the high byte here.
  Y_HDC_REGISTER_DATA_HIGH = 0x8,
  // Alternate status on read, device control on write. The alternate status
  // register reads exactly like the status register; a guest polls it when it
  // wants the status without the side effects a status read has on a drive
  // that raises interrupts.
  Y_HDC_REGISTER_DEVICE_CONTROL = 0xE,
} YHDCRegister;

// Bits of the status register.
enum {
  // An error occurred; the error register says which.
  Y_HDC_STATUS_ERROR = 1 << 0,
  Y_HDC_STATUS_INDEX = 1 << 1,
  Y_HDC_STATUS_CORRECTED_DATA = 1 << 2,
  // The drive is ready to transfer a byte through the data register.
  Y_HDC_STATUS_DATA_REQUEST = 1 << 3,
  Y_HDC_STATUS_SEEK_COMPLETE = 1 << 4,
  Y_HDC_STATUS_DEVICE_FAULT = 1 << 5,
  // The drive is spun up and ready to accept a command.
  Y_HDC_STATUS_READY = 1 << 6,
  // The drive owns the task file and the other bits are not valid.
  Y_HDC_STATUS_BUSY = 1 << 7,
};

// Bits of the error register.
enum {
  Y_HDC_ERROR_ADDRESS_MARK_NOT_FOUND = 1 << 0,
  Y_HDC_ERROR_TRACK0_NOT_FOUND = 1 << 1,
  // The command was not recognized, or is not valid right now.
  Y_HDC_ERROR_ABORTED = 1 << 2,
  // The requested sector does not exist on this drive.
  Y_HDC_ERROR_ID_NOT_FOUND = 1 << 4,
  Y_HDC_ERROR_UNCORRECTABLE = 1 << 6,
  Y_HDC_ERROR_BAD_BLOCK = 1 << 7,
};

// Bits of the device control register.
enum {
  // Stops the drive raising interrupts. This controller has no interrupt line
  // in the first place, so it is accepted and ignored.
  Y_HDC_DEVICE_CONTROL_NO_INTERRUPT = 1 << 1,
  // Software reset, which the guest asserts and then releases.
  Y_HDC_DEVICE_CONTROL_SOFTWARE_RESET = 1 << 2,
};

// Bits of the drive/head register.
enum {
  // Head number, or bits 27-24 of an LBA.
  Y_HDC_DRIVE_HEAD_HEAD_MASK = 0x0F,
  // Selects drive 1 when set, drive 0 when clear.
  Y_HDC_DRIVE_HEAD_DRIVE_SELECT = 1 << 4,
  // The address registers hold an LBA rather than a cylinder/head/sector.
  Y_HDC_DRIVE_HEAD_LBA = 1 << 6,
};

// ATA commands.
typedef enum YHDCCommand {
  // Seek to cylinder 0. The low nibble is a step rate that has no meaning
  // here, so 0x10 through 0x1F are all this command.
  Y_HDC_COMMAND_RECALIBRATE = 0x10,
  Y_HDC_COMMAND_READ_SECTORS = 0x20,
  Y_HDC_COMMAND_READ_SECTORS_NO_RETRY = 0x21,
  Y_HDC_COMMAND_WRITE_SECTORS = 0x30,
  Y_HDC_COMMAND_WRITE_SECTORS_NO_RETRY = 0x31,
  Y_HDC_COMMAND_READ_VERIFY_SECTORS = 0x40,
  Y_HDC_COMMAND_READ_VERIFY_SECTORS_NO_RETRY = 0x41,
  // Seek to the addressed cylinder. As with recalibrate, the low nibble is a
  // step rate, so 0x70 through 0x7F are all this command.
  Y_HDC_COMMAND_SEEK = 0x70,
  // Sets the head and sector counts used to translate a cylinder/head/sector
  // address into a sector number.
  Y_HDC_COMMAND_INITIALIZE_DEVICE_PARAMETERS = 0x91,
  // Returns a 512-byte block describing the drive.
  Y_HDC_COMMAND_IDENTIFY_DEVICE = 0xEC,
} YHDCCommand;

enum {
  // Number of drives the controller supports: a master and a slave.
  Y_HDC_NUM_DRIVES = 2,
  // Bytes per sector. ATA drives of this era have no other sector size, and
  // the option ROM assumes this one.
  Y_HDC_SECTOR_SIZE = 512,
};

// Physical geometry of an attached drive.
typedef struct YHDCDriveGeometry {
  uint16_t num_cylinders;
  uint8_t num_heads;
  uint8_t num_sectors_per_track;
} YHDCDriveGeometry;

// Geometry of the 10MB fixed disk the IBM PC/XT shipped with. Era-accurate,
// comfortably under the 32MB per-partition ceiling of MS-DOS 3.3, and small
// enough for a host to keep an image of one in memory.
enum {
  Y_HDC_GEOMETRY_10MB_NUM_CYLINDERS = 306,
  Y_HDC_GEOMETRY_10MB_NUM_HEADS = 4,
  Y_HDC_GEOMETRY_10MB_NUM_SECTORS_PER_TRACK = 17,
  // Size in bytes of an image of such a disk, so that a host can size a buffer
  // for one at compile time.
  Y_HDC_GEOMETRY_10MB_IMAGE_SIZE = Y_HDC_GEOMETRY_10MB_NUM_CYLINDERS *
      Y_HDC_GEOMETRY_10MB_NUM_HEADS *
      Y_HDC_GEOMETRY_10MB_NUM_SECTORS_PER_TRACK * Y_HDC_SECTOR_SIZE,
};
YAX86_PUBLIC_HEADER const YHDCDriveGeometry YHDCGeometry10MB = {
    .num_cylinders = Y_HDC_GEOMETRY_10MB_NUM_CYLINDERS,
    .num_heads = Y_HDC_GEOMETRY_10MB_NUM_HEADS,
    .num_sectors_per_track = Y_HDC_GEOMETRY_10MB_NUM_SECTORS_PER_TRACK,
};

// What the data register is currently moving.
typedef enum YHDCTransfer {
  // Nothing; a read returns zero and a write is discarded.
  Y_HDC_TRANSFER_NONE = 0,
  // The Identify Device block, out of the sector buffer.
  Y_HDC_TRANSFER_IDENTIFY,
  // Sectors from the drive's image to the guest.
  Y_HDC_TRANSFER_READ,
  // Sectors from the guest to the drive's image.
  Y_HDC_TRANSFER_WRITE,
} YHDCTransfer;

// State of a single drive.
typedef struct YHDCDriveState {
  // Whether a drive is attached.
  bool present;

  // Physical geometry of the drive.
  YHDCDriveGeometry geometry;

  // Head and sector counts the guest asked for with Initialize Device
  // Parameters, which is what cylinder/head/sector addresses are translated
  // against. Initialized to the physical geometry, since a guest that never
  // issues the command expects the geometry the drive reported.
  uint8_t num_translated_heads;
  uint8_t num_translated_sectors_per_track;
} YHDCDriveState;

// Caller-provided runtime configuration for the HDC.
typedef struct YHDCConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger, or NULL to disable logging.
  YLogger* logger;

  // Callbacks to read and write a byte of a drive's image, where offset is a
  // byte offset from the start of the image. The controller range checks every
  // address against the drive's geometry before starting a transfer, so these
  // are only ever called with an offset that lies within the drive.
  //
  // A drive with no callbacks reads back as zeroes and discards writes.
  uint8_t (*read_image_byte)(void* context, uint8_t drive, uint32_t offset);
  void (*write_image_byte)(
      void* context, uint8_t drive, uint32_t offset, uint8_t value);
} YHDCConfig;

// State of the HDC.
typedef struct YHDCState {
  // Pointer to caller-provided runtime configuration.
  YHDCConfig config;

  // Attached drives.
  YHDCDriveState drives[Y_HDC_NUM_DRIVES];

  // Task file registers.
  uint8_t error;
  uint8_t features;
  uint8_t sector_count;
  uint8_t sector_number;
  uint8_t cylinder_low;
  uint8_t cylinder_high;
  uint8_t drive_head;
  uint8_t status;

  // Buffer holding the Identify Device block. Sector data is streamed straight
  // through the image callbacks rather than staged here.
  uint8_t sector_buffer[Y_HDC_SECTOR_SIZE];

  // What the data register is currently moving, if anything.
  YHDCTransfer transfer;
  // Index of the next byte within the sector being transferred. Only
  // meaningful while Y_HDC_STATUS_DATA_REQUEST is set.
  uint16_t transfer_byte_index;
  // Byte offset into the image of the sector being transferred.
  uint32_t transfer_offset;
  // Drive the transfer is against, which is latched when the command starts so
  // that it cannot change underneath a transfer in progress.
  uint8_t transfer_drive;
  // Sectors still to transfer, including the one in progress.
  uint16_t transfer_sectors_remaining;
  // The card's latch for the high byte of a word, which serves both
  // directions: a read of the low byte port fills it, and a write of the high
  // byte port loads it for the following write of the low byte to commit. See
  // YHDCReadDataRegister() and YHDCWriteDataRegister().
  uint8_t data_high_latch;
} YHDCState;

// Initializes the HDC to its power-on state.
YAX86_PUBLIC void YHDCInit(YHDCState* hdc);

// Returns the size of the option ROM in bytes.
YAX86_PUBLIC uint32_t YHDCGetOptionROMSize(void);

// Returns a pointer to the option ROM image, YHDCGetOptionROMSize() bytes of
// it. The image is a constant array compiled into the library, so the platform
// maps it directly rather than reading it a byte at a time through a callback.
YAX86_PUBLIC const uint8_t* YHDCGetOptionROMData(void);

// Handles reads from the HDC's I/O ports.
YAX86_PUBLIC uint8_t YHDCReadPort(YHDCState* hdc, uint16_t port);

// Handles writes to the HDC's I/O ports.
YAX86_PUBLIC void YHDCWritePort(YHDCState* hdc, uint16_t port, uint8_t value);

// Attaches a drive with the given geometry. Drive 0 is the master and drive 1
// is the slave.
YAX86_PUBLIC void YHDCAttachDrive(
    YHDCState* hdc, uint8_t drive, const YHDCDriveGeometry* geometry);

// Detaches the drive in the given slot.
YAX86_PUBLIC void YHDCDetachDrive(YHDCState* hdc, uint8_t drive);

// Maps an I/O port offset from Y_HDC_PORT_BASE to the task file register it
// reaches. An XT-IDE rev 2 card crosses address lines A0 and A3, so a register
// is reached at the port offset with bits 0 and 3 swapped - the status
// register, ATA register 7, is read at port offset 0xE. The mapping is its own
// inverse.
YAX86_PUBLIC_HEADER inline uint8_t YHDCPortOffsetToRegister(uint8_t offset) {
  return (uint8_t)((offset & ~0x09) | ((offset & 0x01) << 3) |
                   ((offset >> 3) & 0x01));
}

#endif  // YAX86_HDC_PUBLIC_H
