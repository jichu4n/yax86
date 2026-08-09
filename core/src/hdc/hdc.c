#ifndef YAX86_IMPLEMENTATION
#include "hdc_option_rom_data.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

#define YAX86_HDC_LOG(level, ...) \
  YAX86_LOG(hdc->config->logger, &kLogModuleHDC, level, __VA_ARGS__)

#include <stddef.h>

// Status of a drive that is idle and has nothing to report.
enum {
  kHDCStatusIdle = kHDCStatusReady | kHDCStatusSeekComplete,
};

// Word offsets into the Identify Device block. The block is 256 words, most
// of which are reserved or describe capabilities no drive of this era has, so
// only the ones that are filled in are named here.
typedef enum HDCIdentifyWord {
  // General configuration. Bit 6 marks the device as a non-removable fixed
  // disk, which is the only kind this controller has.
  kHDCIdentifyWordGeneralConfiguration = 0,
  kHDCIdentifyWordNumCylinders = 1,
  kHDCIdentifyWordNumHeads = 3,
  kHDCIdentifyWordBytesPerTrack = 4,
  kHDCIdentifyWordBytesPerSector = 5,
  kHDCIdentifyWordNumSectorsPerTrack = 6,
  kHDCIdentifyWordSerialNumber = 10,
  kHDCIdentifyWordBufferType = 20,
  kHDCIdentifyWordBufferSize = 21,
  kHDCIdentifyWordNumECCBytes = 22,
  kHDCIdentifyWordFirmwareRevision = 23,
  kHDCIdentifyWordModelNumber = 27,
  kHDCIdentifyWordMaxSectorsPerTransfer = 47,
  kHDCIdentifyWordCapabilities = 49,
  kHDCIdentifyWordPIOTiming = 51,
  kHDCIdentifyWordFieldValidity = 53,
  kHDCIdentifyWordCurrentNumCylinders = 54,
  kHDCIdentifyWordCurrentNumHeads = 55,
  kHDCIdentifyWordCurrentNumSectorsPerTrack = 56,
  kHDCIdentifyWordCurrentCapacity = 57,
  kHDCIdentifyWordNumUserAddressableSectors = 60,
} HDCIdentifyWord;

// Lengths in words of the string fields of the Identify Device block.
enum {
  kHDCIdentifySerialNumberWords = 10,
  kHDCIdentifyFirmwareRevisionWords = 4,
  kHDCIdentifyModelNumberWords = 20,
};

enum {
  // Marks the device as a non-removable fixed disk.
  kHDCIdentifyGeneralConfigurationFixedDisk = 1 << 6,
  // The drive accepts logical block addresses as well as cylinder/head/sector
  // ones.
  kHDCIdentifyCapabilitiesLBASupported = 1 << 9,
  // Words 54 through 58 hold the current translated geometry.
  kHDCIdentifyFieldValidityCurrentGeometry = 1 << 0,
  // PIO data transfer cycle timing mode, in the high byte.
  kHDCIdentifyPIOTimingMode2 = 2 << 8,
  // Sectors moved per Read Multiple or Write Multiple. Zero is how ATA says
  // multiple mode is not supported, which matches Set Multiple Mode aborting.
  kHDCIdentifyMaxSectorsPerTransfer = 0,
  // A dual ported buffer with look-ahead, which is what an ATA drive of this
  // era reports.
  kHDCIdentifyBufferTypeDualPorted = 3,
  // Buffer size in 512-byte increments.
  kHDCIdentifyBufferSize = 1,
  // Bytes reserved for ECC on a long read or write.
  kHDCIdentifyNumECCBytes = 4,
};

static const char* const kHDCModelNumber = "YAX86 HARD DISK";
static const char* const kHDCSerialNumber = "YAX86-0000001";
static const char* const kHDCFirmwareRevision = "1.0";

// Returns the drive the drive/head register currently selects.
static HDCDriveState* HDCSelectedDrive(HDCState* hdc) {
  return &hdc->drives[(hdc->drive_head & kHDCDriveHeadDriveSelect) ? 1 : 0];
}

// Completes a command successfully.
static void HDCFinishCommand(HDCState* hdc) {
  hdc->error = 0;
  hdc->status = kHDCStatusIdle;
}

// Fails a command, leaving the given bits in the error register.
static void HDCFailCommand(HDCState* hdc, uint8_t error) {
  hdc->error = error;
  hdc->status = kHDCStatusIdle | kHDCStatusError;
  hdc->buffer_index = 0;
}

// Total number of sectors on a drive.
static uint32_t HDCDriveNumSectors(const HDCDriveState* drive) {
  return (uint32_t)drive->geometry.num_cylinders *
         (uint32_t)drive->geometry.num_heads *
         (uint32_t)drive->geometry.num_sectors_per_track;
}

// Translates the address in the task file to a sector number, and returns
// whether it addresses a sector that exists on the drive.
//
// The address is an LBA when the drive/head register says so, and a
// cylinder/head/sector triple otherwise. A CHS triple is translated against
// the geometry the guest asked for with Initialize Device Parameters rather
// than the physical geometry, which is what lets a guest address the drive
// with a geometry of its own choosing.
static bool HDCComputeSectorNumber(
    HDCState* hdc, const HDCDriveState* drive, uint32_t* sector_number) {
  uint32_t lba;
  if (hdc->drive_head & kHDCDriveHeadLBA) {
    lba = ((uint32_t)(hdc->drive_head & kHDCDriveHeadHeadMask) << 24) |
          ((uint32_t)hdc->cylinder_high << 16) |
          ((uint32_t)hdc->cylinder_low << 8) | (uint32_t)hdc->sector_number;
  } else {
    const uint32_t cylinder =
        ((uint32_t)hdc->cylinder_high << 8) | (uint32_t)hdc->cylinder_low;
    const uint32_t head = hdc->drive_head & kHDCDriveHeadHeadMask;
    const uint32_t sector = hdc->sector_number;
    // Sectors are numbered from one, and sector zero does not exist.
    if (sector == 0 || head >= drive->num_translated_heads ||
        sector > drive->num_translated_sectors_per_track) {
      return false;
    }
    lba = (cylinder * (uint32_t)drive->num_translated_heads + head) *
              (uint32_t)drive->num_translated_sectors_per_track +
          (sector - 1);
  }

  if (lba >= HDCDriveNumSectors(drive)) {
    return false;
  }
  *sector_number = lba;
  return true;
}

// Writes a 16-bit word into the Identify Device block, which is little endian.
static void HDCWriteIdentifyWord(
    HDCState* hdc, uint16_t word_index, uint16_t value) {
  hdc->sector_buffer[word_index * 2] = (uint8_t)(value & 0xFF);
  hdc->sector_buffer[word_index * 2 + 1] = (uint8_t)(value >> 8);
}

// Writes a space padded string into the Identify Device block. ATA strings
// are byte swapped within each word, so the first character of a pair lands in
// the high byte.
static void HDCWriteIdentifyString(
    HDCState* hdc, uint16_t word_index, const char* text, uint16_t num_words) {
  const char* next = text;
  for (uint16_t i = 0; i < num_words; ++i) {
    const char first = *next ? *next++ : ' ';
    const char second = *next ? *next++ : ' ';
    HDCWriteIdentifyWord(
        hdc, (uint16_t)(word_index + i),
        (uint16_t)(((uint16_t)(uint8_t)first << 8) | (uint8_t)second));
  }
}

// Fills the sector buffer with the drive's Identify Device block.
static void HDCBuildIdentifyBlock(HDCState* hdc, const HDCDriveState* drive) {
  for (uint16_t i = 0; i < kHDCSectorSize; ++i) {
    hdc->sector_buffer[i] = 0;
  }

  const uint32_t num_sectors = HDCDriveNumSectors(drive);

  HDCWriteIdentifyWord(
      hdc, kHDCIdentifyWordGeneralConfiguration,
      kHDCIdentifyGeneralConfigurationFixedDisk);
  HDCWriteIdentifyWord(
      hdc, kHDCIdentifyWordNumCylinders, drive->geometry.num_cylinders);
  HDCWriteIdentifyWord(
      hdc, kHDCIdentifyWordNumHeads, drive->geometry.num_heads);
  HDCWriteIdentifyWord(
      hdc, kHDCIdentifyWordBytesPerTrack,
      (uint16_t)(kHDCSectorSize * drive->geometry.num_sectors_per_track));
  HDCWriteIdentifyWord(hdc, kHDCIdentifyWordBytesPerSector, kHDCSectorSize);
  HDCWriteIdentifyWord(
      hdc, kHDCIdentifyWordNumSectorsPerTrack,
      drive->geometry.num_sectors_per_track);
  HDCWriteIdentifyString(
      hdc, kHDCIdentifyWordSerialNumber, kHDCSerialNumber,
      kHDCIdentifySerialNumberWords);
  HDCWriteIdentifyWord(
      hdc, kHDCIdentifyWordBufferType, kHDCIdentifyBufferTypeDualPorted);
  HDCWriteIdentifyWord(hdc, kHDCIdentifyWordBufferSize, kHDCIdentifyBufferSize);
  HDCWriteIdentifyWord(
      hdc, kHDCIdentifyWordNumECCBytes, kHDCIdentifyNumECCBytes);
  HDCWriteIdentifyString(
      hdc, kHDCIdentifyWordFirmwareRevision, kHDCFirmwareRevision,
      kHDCIdentifyFirmwareRevisionWords);
  HDCWriteIdentifyString(
      hdc, kHDCIdentifyWordModelNumber, kHDCModelNumber,
      kHDCIdentifyModelNumberWords);
  HDCWriteIdentifyWord(
      hdc, kHDCIdentifyWordMaxSectorsPerTransfer,
      kHDCIdentifyMaxSectorsPerTransfer);
  HDCWriteIdentifyWord(
      hdc, kHDCIdentifyWordCapabilities, kHDCIdentifyCapabilitiesLBASupported);
  HDCWriteIdentifyWord(
      hdc, kHDCIdentifyWordPIOTiming, kHDCIdentifyPIOTimingMode2);
  HDCWriteIdentifyWord(
      hdc, kHDCIdentifyWordFieldValidity,
      kHDCIdentifyFieldValidityCurrentGeometry);
  HDCWriteIdentifyWord(
      hdc, kHDCIdentifyWordCurrentNumCylinders, drive->geometry.num_cylinders);
  HDCWriteIdentifyWord(
      hdc, kHDCIdentifyWordCurrentNumHeads, drive->num_translated_heads);
  HDCWriteIdentifyWord(
      hdc, kHDCIdentifyWordCurrentNumSectorsPerTrack,
      drive->num_translated_sectors_per_track);
  HDCWriteIdentifyWord(
      hdc, kHDCIdentifyWordCurrentCapacity, (uint16_t)(num_sectors & 0xFFFF));
  HDCWriteIdentifyWord(
      hdc, kHDCIdentifyWordCurrentCapacity + 1, (uint16_t)(num_sectors >> 16));
  HDCWriteIdentifyWord(
      hdc, kHDCIdentifyWordNumUserAddressableSectors,
      (uint16_t)(num_sectors & 0xFFFF));
  HDCWriteIdentifyWord(
      hdc, kHDCIdentifyWordNumUserAddressableSectors + 1,
      (uint16_t)(num_sectors >> 16));
}

// Starts handing the sector buffer to the guest through the data register.
static void HDCStartBufferTransfer(HDCState* hdc) {
  hdc->buffer_index = 0;
  hdc->error = 0;
  hdc->status = kHDCStatusIdle | kHDCStatusDataRequest;
}

static void HDCHandleIdentifyDevice(HDCState* hdc, HDCDriveState* drive) {
  HDCBuildIdentifyBlock(hdc, drive);
  HDCStartBufferTransfer(hdc);
}

static void HDCHandleInitializeDeviceParameters(
    HDCState* hdc, HDCDriveState* drive) {
  const uint8_t num_heads = (hdc->drive_head & kHDCDriveHeadHeadMask) + 1;
  const uint8_t num_sectors_per_track = hdc->sector_count;
  if (num_sectors_per_track == 0) {
    HDCFailCommand(hdc, kHDCErrorAborted);
    return;
  }
  drive->num_translated_heads = num_heads;
  drive->num_translated_sectors_per_track = num_sectors_per_track;
  YAX86_HDC_LOG(
      kLogLevelDebug, "drive %u translated geometry set to %u heads, %u S/T",
      (hdc->drive_head & kHDCDriveHeadDriveSelect) ? 1 : 0, num_heads,
      num_sectors_per_track);
  HDCFinishCommand(hdc);
}

// Checks that the addressed sector exists without transferring it.
static void HDCHandleReadVerifySectors(HDCState* hdc, HDCDriveState* drive) {
  uint32_t sector_number = 0;
  if (!HDCComputeSectorNumber(hdc, drive, &sector_number)) {
    HDCFailCommand(hdc, kHDCErrorIDNotFound);
    return;
  }
  HDCFinishCommand(hdc);
}

static void HDCExecuteCommand(HDCState* hdc, uint8_t opcode) {
  HDCDriveState* drive = HDCSelectedDrive(hdc);
  if (!drive->present) {
    // An absent drive never answers, so the command simply goes unheard. The
    // status register is shared by both drives, and HDCReadPort already
    // reports an absent one as never ready, so writing to it here would do
    // nothing except strand the other drive at a status of zero.
    return;
  }

  YAX86_HDC_LOG(
      kLogLevelDebug, "drive %u command %02X, CHS %u/%u/%u, count %u",
      (hdc->drive_head & kHDCDriveHeadDriveSelect) ? 1 : 0, opcode,
      ((uint16_t)hdc->cylinder_high << 8) | hdc->cylinder_low,
      hdc->drive_head & kHDCDriveHeadHeadMask, hdc->sector_number,
      hdc->sector_count);

  // The low nibble of recalibrate and seek is a stepping rate, so the whole
  // range is one command.
  const uint8_t command = ((opcode & 0xF0) == kHDCCommandRecalibrate ||
                           (opcode & 0xF0) == kHDCCommandSeek)
                              ? (uint8_t)(opcode & 0xF0)
                              : opcode;

  switch (command) {
    case kHDCCommandRecalibrate:
    case kHDCCommandSeek:
      // Seek time is not modelled, so the heads are already where they need
      // to be.
      HDCFinishCommand(hdc);
      break;

    case kHDCCommandReadVerifySectors:
    case kHDCCommandReadVerifySectorsNoRetry:
      HDCHandleReadVerifySectors(hdc, drive);
      break;

    case kHDCCommandInitializeDeviceParameters:
      HDCHandleInitializeDeviceParameters(hdc, drive);
      break;

    case kHDCCommandIdentifyDevice:
      HDCHandleIdentifyDevice(hdc, drive);
      break;

    default:
      YAX86_HDC_LOG(kLogLevelWarn, "unsupported command %02X", opcode);
      HDCFailCommand(hdc, kHDCErrorAborted);
      break;
  }
}

// Hands the guest the next byte of the sector buffer.
static uint8_t HDCReadDataRegister(HDCState* hdc) {
  if (!(hdc->status & kHDCStatusDataRequest)) {
    YAX86_HDC_LOG(kLogLevelWarn, "data register read with no transfer active");
    return 0;
  }
  const uint8_t value = hdc->sector_buffer[hdc->buffer_index];
  ++hdc->buffer_index;
  if (hdc->buffer_index >= kHDCSectorSize) {
    hdc->buffer_index = 0;
    hdc->status = kHDCStatusIdle;
  }
  return value;
}

uint8_t HDCReadPort(HDCState* hdc, uint16_t port) {
  const uint8_t offset = (uint8_t)((port - kHDCPortBase) & (kHDCNumPorts - 1));
  switch (HDCPortOffsetToRegister(offset)) {
    // The card latches the high byte of each word, but the guest reads the two
    // halves in order, so a single stream of bytes serves both ports.
    case kHDCRegisterData:
    case kHDCRegisterDataHigh:
      return HDCReadDataRegister(hdc);
    case kHDCRegisterError:
      return hdc->error;
    case kHDCRegisterSectorCount:
      return hdc->sector_count;
    case kHDCRegisterSectorNumber:
      return hdc->sector_number;
    case kHDCRegisterCylinderLow:
      return hdc->cylinder_low;
    case kHDCRegisterCylinderHigh:
      return hdc->cylinder_high;
    case kHDCRegisterDriveHead:
      return hdc->drive_head;
    case kHDCRegisterStatus:
      // An empty drive slot leaves the bus undriven, which the guest reads as
      // a drive that is never ready.
      return HDCSelectedDrive(hdc)->present ? hdc->status : 0;
    default:
      return kHDCOpenBusValue;
  }
}

void HDCWritePort(HDCState* hdc, uint16_t port, uint8_t value) {
  const uint8_t offset = (uint8_t)((port - kHDCPortBase) & (kHDCNumPorts - 1));
  switch (HDCPortOffsetToRegister(offset)) {
    case kHDCRegisterData:
    case kHDCRegisterDataHigh:
      // Writing to a drive is not supported yet.
      break;
    case kHDCRegisterError:
      hdc->features = value;
      break;
    case kHDCRegisterSectorCount:
      hdc->sector_count = value;
      break;
    case kHDCRegisterSectorNumber:
      hdc->sector_number = value;
      break;
    case kHDCRegisterCylinderLow:
      hdc->cylinder_low = value;
      break;
    case kHDCRegisterCylinderHigh:
      hdc->cylinder_high = value;
      break;
    case kHDCRegisterDriveHead:
      hdc->drive_head = value;
      break;
    case kHDCRegisterStatus:
      HDCExecuteCommand(hdc, value);
      break;
    default:
      break;
  }
}

void HDCAttachDrive(
    HDCState* hdc, uint8_t drive, const HDCDriveGeometry* geometry) {
  if (drive >= kHDCNumDrives) {
    return;
  }
  HDCDriveState* drive_state = &hdc->drives[drive];
  drive_state->present = true;
  drive_state->geometry = *geometry;
  // Until the guest says otherwise, addresses are translated against the
  // drive's own geometry.
  drive_state->num_translated_heads = geometry->num_heads;
  drive_state->num_translated_sectors_per_track =
      geometry->num_sectors_per_track;
}

void HDCDetachDrive(HDCState* hdc, uint8_t drive) {
  if (drive >= kHDCNumDrives) {
    return;
  }
  static const HDCDriveState empty_drive_state = {0};
  hdc->drives[drive] = empty_drive_state;
}

void HDCInit(HDCState* hdc, HDCConfig* config) {
  static const HDCState zero_hdc_state = {0};
  *hdc = zero_hdc_state;

  hdc->config = config;
  hdc->status = kHDCStatusIdle;
}

uint32_t HDCGetOptionROMSize(void) { return kHDCOptionROMDataSize; }

uint8_t HDCReadOptionROMByte(uint32_t offset) {
  if (offset >= kHDCOptionROMDataSize) {
    return kHDCOpenBusValue;
  }
  return kHDCOptionROMData[offset];
}
