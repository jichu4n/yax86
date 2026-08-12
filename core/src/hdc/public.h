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
  kLogModuleIDHDC = 9,
};

// Log module for the HDC.
static const LogModule kLogModuleHDC = {
    .id = kLogModuleIDHDC,
    .name = "HDC",
};

// Memory region types.
enum {
  // Option ROM memory map entry type.
  kMemoryMapEntryHDCOptionROM = 0x0C,

  // Start address of the option ROM. GLaBIOS scans for option ROMs starting
  // here, on 2KB boundaries.
  kHDCOptionROMStartAddress = 0xC8000,
};

// Value returned for reads that no device answers.
enum {
  kHDCOpenBusValue = 0xFF,
};

// I/O ports for the HDC.
enum {
  // Base I/O port of the task file. This is the XT-IDE's stock configuration,
  // which the option ROM reports at POST as "Master at 300h".
  kHDCPortBase = 0x300,
  // Number of I/O ports the card decodes.
  kHDCNumPorts = 16,
};

// Task file registers, given as offsets from kHDCPortBase after the card's
// address line swap has been undone. See HDCPortOffsetToRegister().
typedef enum HDCRegister {
  // Data register, low byte of each 16-bit word.
  kHDCRegisterData = 0x0,
  // Error on read, features on write.
  kHDCRegisterError = 0x1,
  kHDCRegisterSectorCount = 0x2,
  // Sector number, or bits 7-0 of an LBA.
  kHDCRegisterSectorNumber = 0x3,
  // Cylinder low, or bits 15-8 of an LBA.
  kHDCRegisterCylinderLow = 0x4,
  // Cylinder high, or bits 23-16 of an LBA.
  kHDCRegisterCylinderHigh = 0x5,
  // Drive select and head, or bits 27-24 of an LBA.
  kHDCRegisterDriveHead = 0x6,
  // Status on read, command on write.
  kHDCRegisterStatus = 0x7,
  // Data register, high byte of each 16-bit word. Not an ATA register: the
  // drive's data port is 16 bits wide and the card is on an 8-bit bus, so the
  // card latches the high byte here.
  kHDCRegisterDataHigh = 0x8,
  // Alternate status on read, device control on write. The alternate status
  // register reads exactly like the status register; a guest polls it when it
  // wants the status without the side effects a status read has on a drive
  // that raises interrupts.
  kHDCRegisterDeviceControl = 0xE,
} HDCRegister;

// Bits of the status register.
enum {
  // An error occurred; the error register says which.
  kHDCStatusError = 1 << 0,
  kHDCStatusIndex = 1 << 1,
  kHDCStatusCorrectedData = 1 << 2,
  // The drive is ready to transfer a byte through the data register.
  kHDCStatusDataRequest = 1 << 3,
  kHDCStatusSeekComplete = 1 << 4,
  kHDCStatusDeviceFault = 1 << 5,
  // The drive is spun up and ready to accept a command.
  kHDCStatusReady = 1 << 6,
  // The drive owns the task file and the other bits are not valid.
  kHDCStatusBusy = 1 << 7,
};

// Bits of the error register.
enum {
  kHDCErrorAddressMarkNotFound = 1 << 0,
  kHDCErrorTrack0NotFound = 1 << 1,
  // The command was not recognized, or is not valid right now.
  kHDCErrorAborted = 1 << 2,
  // The requested sector does not exist on this drive.
  kHDCErrorIDNotFound = 1 << 4,
  kHDCErrorUncorrectable = 1 << 6,
  kHDCErrorBadBlock = 1 << 7,
};

// Bits of the device control register.
enum {
  // Stops the drive raising interrupts. This controller has no interrupt line
  // in the first place, so it is accepted and ignored.
  kHDCDeviceControlNoInterrupt = 1 << 1,
  // Software reset, which the guest asserts and then releases.
  kHDCDeviceControlSoftwareReset = 1 << 2,
};

// Bits of the drive/head register.
enum {
  // Head number, or bits 27-24 of an LBA.
  kHDCDriveHeadHeadMask = 0x0F,
  // Selects drive 1 when set, drive 0 when clear.
  kHDCDriveHeadDriveSelect = 1 << 4,
  // The address registers hold an LBA rather than a cylinder/head/sector.
  kHDCDriveHeadLBA = 1 << 6,
};

// ATA commands.
typedef enum HDCCommand {
  // Seek to cylinder 0. The low nibble is a step rate that has no meaning
  // here, so 0x10 through 0x1F are all this command.
  kHDCCommandRecalibrate = 0x10,
  kHDCCommandReadSectors = 0x20,
  kHDCCommandReadSectorsNoRetry = 0x21,
  kHDCCommandWriteSectors = 0x30,
  kHDCCommandWriteSectorsNoRetry = 0x31,
  kHDCCommandReadVerifySectors = 0x40,
  kHDCCommandReadVerifySectorsNoRetry = 0x41,
  // Seek to the addressed cylinder. As with recalibrate, the low nibble is a
  // step rate, so 0x70 through 0x7F are all this command.
  kHDCCommandSeek = 0x70,
  // Sets the head and sector counts used to translate a cylinder/head/sector
  // address into a sector number.
  kHDCCommandInitializeDeviceParameters = 0x91,
  // Returns a 512-byte block describing the drive.
  kHDCCommandIdentifyDevice = 0xEC,
} HDCCommand;

enum {
  // Number of drives the controller supports: a master and a slave.
  kHDCNumDrives = 2,
  // Bytes per sector. ATA drives of this era have no other sector size, and
  // the option ROM assumes this one.
  kHDCSectorSize = 512,
};

// Physical geometry of an attached drive.
typedef struct HDCDriveGeometry {
  uint16_t num_cylinders;
  uint8_t num_heads;
  uint8_t num_sectors_per_track;
} HDCDriveGeometry;

// Geometry of the 10MB fixed disk the IBM PC/XT shipped with. Era-accurate,
// comfortably under the 32MB per-partition ceiling of MS-DOS 3.3, and small
// enough for a host to keep an image of one in memory.
enum {
  kHDCGeometry10MBNumCylinders = 306,
  kHDCGeometry10MBNumHeads = 4,
  kHDCGeometry10MBNumSectorsPerTrack = 17,
  // Size in bytes of an image of such a disk, so that a host can size a buffer
  // for one at compile time.
  kHDCGeometry10MBImageSize =
      kHDCGeometry10MBNumCylinders * kHDCGeometry10MBNumHeads *
      kHDCGeometry10MBNumSectorsPerTrack * kHDCSectorSize,
};
static const HDCDriveGeometry kHDCGeometry10MB = {
    .num_cylinders = kHDCGeometry10MBNumCylinders,
    .num_heads = kHDCGeometry10MBNumHeads,
    .num_sectors_per_track = kHDCGeometry10MBNumSectorsPerTrack,
};

// What the data register is currently moving.
typedef enum HDCTransfer {
  // Nothing; a read returns zero and a write is discarded.
  kHDCTransferNone = 0,
  // The Identify Device block, out of the sector buffer.
  kHDCTransferIdentify,
  // Sectors from the drive's image to the guest.
  kHDCTransferRead,
  // Sectors from the guest to the drive's image.
  kHDCTransferWrite,
} HDCTransfer;

// State of a single drive.
typedef struct HDCDriveState {
  // Whether a drive is attached.
  bool present;

  // Physical geometry of the drive.
  HDCDriveGeometry geometry;

  // Head and sector counts the guest asked for with Initialize Device
  // Parameters, which is what cylinder/head/sector addresses are translated
  // against. Initialized to the physical geometry, since a guest that never
  // issues the command expects the geometry the drive reported.
  uint8_t num_translated_heads;
  uint8_t num_translated_sectors_per_track;
} HDCDriveState;

// Caller-provided runtime configuration for the HDC.
typedef struct HDCConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger, or NULL to disable logging.
  Logger* logger;

  // Callbacks to read and write a byte of a drive's image, where offset is a
  // byte offset from the start of the image. The controller range checks every
  // address against the drive's geometry before starting a transfer, so these
  // are only ever called with an offset that lies within the drive.
  //
  // A drive with no callbacks reads back as zeroes and discards writes.
  uint8_t (*read_image_byte)(void* context, uint8_t drive, uint32_t offset);
  void (*write_image_byte)(
      void* context, uint8_t drive, uint32_t offset, uint8_t value);
} HDCConfig;

// State of the HDC.
typedef struct HDCState {
  // Pointer to caller-provided runtime configuration.
  HDCConfig* config;

  // Attached drives.
  HDCDriveState drives[kHDCNumDrives];

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
  uint8_t sector_buffer[kHDCSectorSize];

  // What the data register is currently moving, if anything.
  HDCTransfer transfer;
  // Index of the next byte within the sector being transferred. Only
  // meaningful while kHDCStatusDataRequest is set.
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
  // HDCReadDataRegister() and HDCWriteDataRegister().
  uint8_t data_high_latch;
} HDCState;

// Initializes the HDC to its power-on state.
void HDCInit(HDCState* hdc, HDCConfig* config);

// Returns the size of the option ROM in bytes.
uint32_t HDCGetOptionROMSize(void);

// Returns a pointer to the option ROM image, HDCGetOptionROMSize() bytes of
// it. The image is a constant array compiled into the library, so the platform
// maps it directly rather than reading it a byte at a time through a callback.
const uint8_t* HDCGetOptionROMData(void);

// Reads a byte from the option ROM, where offset is relative to
// kHDCOptionROMStartAddress.
uint8_t HDCReadOptionROMByte(uint32_t offset);

// Handles reads from the HDC's I/O ports.
uint8_t HDCReadPort(HDCState* hdc, uint16_t port);

// Handles writes to the HDC's I/O ports.
void HDCWritePort(HDCState* hdc, uint16_t port, uint8_t value);

// Attaches a drive with the given geometry. Drive 0 is the master and drive 1
// is the slave.
void HDCAttachDrive(
    HDCState* hdc, uint8_t drive, const HDCDriveGeometry* geometry);

// Detaches the drive in the given slot.
void HDCDetachDrive(HDCState* hdc, uint8_t drive);

// Maps an I/O port offset from kHDCPortBase to the task file register it
// reaches. An XT-IDE rev 2 card crosses address lines A0 and A3, so a register
// is reached at the port offset with bits 0 and 3 swapped - the status
// register, ATA register 7, is read at port offset 0xE. The mapping is its own
// inverse.
static inline uint8_t HDCPortOffsetToRegister(uint8_t offset) {
  return (uint8_t)((offset & ~0x09) | ((offset & 0x01) << 3) |
                   ((offset >> 3) & 0x01));
}

#endif  // YAX86_HDC_PUBLIC_H
