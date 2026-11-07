#ifndef YAX86_IMPLEMENTATION
#include "hdc_option_rom_data.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

#define Y_HDC_LOG(level, ...) \
  Y_LOG(hdc->config.logger, &YHDCLogModule, level, __VA_ARGS__)

#include <stddef.h>

// Status of a drive that is idle and has nothing to report.
enum {
  Y_HDC_STATUS_IDLE = Y_HDC_STATUS_READY | Y_HDC_STATUS_SEEK_COMPLETE,
};

// Word offsets into the Identify Device block. The block is 256 words, most
// of which are reserved or describe capabilities no drive of this era has, so
// only the ones that are filled in are named here.
typedef enum YHDCIdentifyWord {
  // General configuration. Bit 6 marks the device as a non-removable fixed
  // disk, which is the only kind this controller has.
  Y_HDC_IDENTIFY_WORD_GENERAL_CONFIGURATION = 0,
  Y_HDC_IDENTIFY_WORD_NUM_CYLINDERS = 1,
  Y_HDC_IDENTIFY_WORD_NUM_HEADS = 3,
  Y_HDC_IDENTIFY_WORD_BYTES_PER_TRACK = 4,
  Y_HDC_IDENTIFY_WORD_BYTES_PER_SECTOR = 5,
  Y_HDC_IDENTIFY_WORD_NUM_SECTORS_PER_TRACK = 6,
  Y_HDC_IDENTIFY_WORD_SERIAL_NUMBER = 10,
  Y_HDC_IDENTIFY_WORD_BUFFER_TYPE = 20,
  Y_HDC_IDENTIFY_WORD_BUFFER_SIZE = 21,
  Y_HDC_IDENTIFY_WORD_NUM_ECC_BYTES = 22,
  Y_HDC_IDENTIFY_WORD_FIRMWARE_REVISION = 23,
  Y_HDC_IDENTIFY_WORD_MODEL_NUMBER = 27,
  Y_HDC_IDENTIFY_WORD_MAX_SECTORS_PER_TRANSFER = 47,
  Y_HDC_IDENTIFY_WORD_CAPABILITIES = 49,
  Y_HDC_IDENTIFY_WORD_PIO_TIMING = 51,
  Y_HDC_IDENTIFY_WORD_FIELD_VALIDITY = 53,
  Y_HDC_IDENTIFY_WORD_CURRENT_NUM_CYLINDERS = 54,
  Y_HDC_IDENTIFY_WORD_CURRENT_NUM_HEADS = 55,
  Y_HDC_IDENTIFY_WORD_CURRENT_NUM_SECTORS_PER_TRACK = 56,
  Y_HDC_IDENTIFY_WORD_CURRENT_CAPACITY = 57,
  Y_HDC_IDENTIFY_WORD_NUM_USER_ADDRESSABLE_SECTORS = 60,
} YHDCIdentifyWord;

// Lengths in words of the string fields of the Identify Device block.
enum {
  Y_HDC_IDENTIFY_SERIAL_NUMBER_WORDS = 10,
  Y_HDC_IDENTIFY_FIRMWARE_REVISION_WORDS = 4,
  Y_HDC_IDENTIFY_MODEL_NUMBER_WORDS = 20,
};

enum {
  // Marks the device as a non-removable fixed disk.
  Y_HDC_IDENTIFY_GENERAL_CONFIGURATION_FIXED_DISK = 1 << 6,
  // The drive accepts logical block addresses as well as cylinder/head/sector
  // ones.
  Y_HDC_IDENTIFY_CAPABILITIES_LBA_SUPPORTED = 1 << 9,
  // Words 54 through 58 hold the current translated geometry.
  Y_HDC_IDENTIFY_FIELD_VALIDITY_CURRENT_GEOMETRY = 1 << 0,
  // PIO data transfer cycle timing mode, in the high byte.
  Y_HDC_IDENTIFY_PIO_TIMING_MODE2 = 2 << 8,
  // Sectors moved per Read Multiple or Write Multiple. Zero is how ATA says
  // multiple mode is not supported, which matches Set Multiple Mode aborting.
  Y_HDC_IDENTIFY_MAX_SECTORS_PER_TRANSFER = 0,
  // A dual ported buffer with look-ahead, which is what an ATA drive of this
  // era reports.
  Y_HDC_IDENTIFY_BUFFER_TYPE_DUAL_PORTED = 3,
  // Buffer size in 512-byte increments.
  Y_HDC_IDENTIFY_BUFFER_SIZE = 1,
  // Bytes reserved for ECC on a long read or write.
  Y_HDC_IDENTIFY_NUM_ECC_BYTES = 4,
};

Y_FILE_PRIVATE const char* const YHDCModelNumber = "YAX86 HARD DISK";
Y_FILE_PRIVATE const char* const YHDCSerialNumber = "YAX86-0000001";
Y_FILE_PRIVATE const char* const YHDCFirmwareRevision = "1.0";

// Returns the drive the drive/head register currently selects.
Y_FILE_PRIVATE YHDCDriveState* YHDCSelectedDrive(YHDCState* hdc) {
  return &hdc->drives
              [(hdc->drive_head & Y_HDC_DRIVE_HEAD_DRIVE_SELECT) ? 1 : 0];
}

// Completes a command successfully.
Y_FILE_PRIVATE void YHDCFinishCommand(YHDCState* hdc) {
  hdc->error = 0;
  hdc->status = Y_HDC_STATUS_IDLE;
}

// Fails a command, leaving the given bits in the error register.
Y_FILE_PRIVATE void YHDCFailCommand(YHDCState* hdc, uint8_t error) {
  hdc->error = error;
  hdc->status = Y_HDC_STATUS_IDLE | Y_HDC_STATUS_ERROR;
  hdc->transfer = Y_HDC_TRANSFER_NONE;
  hdc->transfer_byte_index = 0;
}

// Total number of sectors on a drive.
Y_FILE_PRIVATE uint32_t YHDCDriveNumSectors(const YHDCDriveState* drive) {
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
Y_FILE_PRIVATE bool YHDCComputeSectorNumber(
    YHDCState* hdc, const YHDCDriveState* drive, uint32_t* sector_number) {
  uint32_t lba;
  if (hdc->drive_head & Y_HDC_DRIVE_HEAD_LBA) {
    lba = ((uint32_t)(hdc->drive_head & Y_HDC_DRIVE_HEAD_HEAD_MASK) << 24) |
          ((uint32_t)hdc->cylinder_high << 16) |
          ((uint32_t)hdc->cylinder_low << 8) | (uint32_t)hdc->sector_number;
  } else {
    const uint32_t cylinder =
        ((uint32_t)hdc->cylinder_high << 8) | (uint32_t)hdc->cylinder_low;
    const uint32_t head = hdc->drive_head & Y_HDC_DRIVE_HEAD_HEAD_MASK;
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

  if (lba >= YHDCDriveNumSectors(drive)) {
    return false;
  }
  *sector_number = lba;
  return true;
}

// Writes a 16-bit word into the Identify Device block, which is little endian.
Y_FILE_PRIVATE void YHDCWriteIdentifyWord(
    YHDCState* hdc, uint16_t word_index, uint16_t value) {
  hdc->sector_buffer[word_index * 2] = (uint8_t)(value & 0xFF);
  hdc->sector_buffer[word_index * 2 + 1] = (uint8_t)(value >> 8);
}

// Writes a space padded string into the Identify Device block. ATA strings
// are byte swapped within each word, so the first character of a pair lands in
// the high byte.
Y_FILE_PRIVATE void YHDCWriteIdentifyString(
    YHDCState* hdc, uint16_t word_index, const char* text, uint16_t num_words) {
  const char* next = text;
  for (uint16_t i = 0; i < num_words; ++i) {
    const char first = *next ? *next++ : ' ';
    const char second = *next ? *next++ : ' ';
    YHDCWriteIdentifyWord(
        hdc, (uint16_t)(word_index + i),
        (uint16_t)(((uint16_t)(uint8_t)first << 8) | (uint8_t)second));
  }
}

// Fills the sector buffer with the drive's Identify Device block.
Y_FILE_PRIVATE void YHDCBuildIdentifyBlock(
    YHDCState* hdc, const YHDCDriveState* drive) {
  for (uint16_t i = 0; i < Y_HDC_SECTOR_SIZE; ++i) {
    hdc->sector_buffer[i] = 0;
  }

  const uint32_t num_sectors = YHDCDriveNumSectors(drive);

  // Cylinder count in the geometry the guest asked for, which is however many
  // whole cylinders of that shape fit on the drive. Reporting the physical
  // count instead would describe a drive larger than this one whenever the
  // guest picks a different head or sector count, and addresses in the part
  // that does not exist would come back as ID not found. Deriving it means
  // every address the reported geometry allows is one the drive really has:
  // the largest is num_translated_cylinders * heads * sectors - 1, which is at
  // most num_sectors - 1 by construction.
  const uint32_t num_sectors_per_cylinder =
      (uint32_t)drive->num_translated_heads *
      (uint32_t)drive->num_translated_sectors_per_track;
  const uint16_t num_translated_cylinders =
      (uint16_t)(num_sectors / num_sectors_per_cylinder);
  // Capacity in that same geometry, which ATA defines as the product of the
  // three current geometry words. It is at most the physical capacity, and
  // less when the head and sector counts leave a partial cylinder over.
  const uint32_t num_translated_sectors =
      (uint32_t)num_translated_cylinders * num_sectors_per_cylinder;

  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_GENERAL_CONFIGURATION,
      Y_HDC_IDENTIFY_GENERAL_CONFIGURATION_FIXED_DISK);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_NUM_CYLINDERS, drive->geometry.num_cylinders);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_NUM_HEADS, drive->geometry.num_heads);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_BYTES_PER_TRACK,
      (uint16_t)(Y_HDC_SECTOR_SIZE * drive->geometry.num_sectors_per_track));
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_BYTES_PER_SECTOR, Y_HDC_SECTOR_SIZE);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_NUM_SECTORS_PER_TRACK,
      drive->geometry.num_sectors_per_track);
  YHDCWriteIdentifyString(
      hdc, Y_HDC_IDENTIFY_WORD_SERIAL_NUMBER, YHDCSerialNumber,
      Y_HDC_IDENTIFY_SERIAL_NUMBER_WORDS);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_BUFFER_TYPE,
      Y_HDC_IDENTIFY_BUFFER_TYPE_DUAL_PORTED);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_BUFFER_SIZE, Y_HDC_IDENTIFY_BUFFER_SIZE);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_NUM_ECC_BYTES, Y_HDC_IDENTIFY_NUM_ECC_BYTES);
  YHDCWriteIdentifyString(
      hdc, Y_HDC_IDENTIFY_WORD_FIRMWARE_REVISION, YHDCFirmwareRevision,
      Y_HDC_IDENTIFY_FIRMWARE_REVISION_WORDS);
  YHDCWriteIdentifyString(
      hdc, Y_HDC_IDENTIFY_WORD_MODEL_NUMBER, YHDCModelNumber,
      Y_HDC_IDENTIFY_MODEL_NUMBER_WORDS);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_MAX_SECTORS_PER_TRANSFER,
      Y_HDC_IDENTIFY_MAX_SECTORS_PER_TRANSFER);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_CAPABILITIES,
      Y_HDC_IDENTIFY_CAPABILITIES_LBA_SUPPORTED);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_PIO_TIMING, Y_HDC_IDENTIFY_PIO_TIMING_MODE2);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_FIELD_VALIDITY,
      Y_HDC_IDENTIFY_FIELD_VALIDITY_CURRENT_GEOMETRY);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_CURRENT_NUM_CYLINDERS, num_translated_cylinders);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_CURRENT_NUM_HEADS, drive->num_translated_heads);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_CURRENT_NUM_SECTORS_PER_TRACK,
      drive->num_translated_sectors_per_track);
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_CURRENT_CAPACITY,
      (uint16_t)(num_translated_sectors & 0xFFFF));
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_CURRENT_CAPACITY + 1,
      (uint16_t)(num_translated_sectors >> 16));
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_NUM_USER_ADDRESSABLE_SECTORS,
      (uint16_t)(num_sectors & 0xFFFF));
  YHDCWriteIdentifyWord(
      hdc, Y_HDC_IDENTIFY_WORD_NUM_USER_ADDRESSABLE_SECTORS + 1,
      (uint16_t)(num_sectors >> 16));
}

// Opens a transfer through the data register.
Y_FILE_PRIVATE void YHDCStartTransfer(
    YHDCState* hdc, YHDCTransfer transfer, uint32_t offset,
    uint16_t num_sectors) {
  hdc->transfer = transfer;
  hdc->transfer_byte_index = 0;
  hdc->transfer_offset = offset;
  hdc->transfer_drive =
      (hdc->drive_head & Y_HDC_DRIVE_HEAD_DRIVE_SELECT) ? 1 : 0;
  hdc->transfer_sectors_remaining = num_sectors;
  hdc->error = 0;
  hdc->status = Y_HDC_STATUS_IDLE | Y_HDC_STATUS_DATA_REQUEST;
}

// Advances past the byte just transferred, moving on to the next sector or
// ending the transfer as needed.
Y_FILE_PRIVATE void YHDCAdvanceTransfer(YHDCState* hdc) {
  ++hdc->transfer_byte_index;
  if (hdc->transfer_byte_index < Y_HDC_SECTOR_SIZE) {
    return;
  }
  hdc->transfer_byte_index = 0;

  if (hdc->transfer == Y_HDC_TRANSFER_IDENTIFY) {
    // A single block, with no sector count behind it.
    hdc->transfer = Y_HDC_TRANSFER_NONE;
    hdc->status = Y_HDC_STATUS_IDLE;
    return;
  }

  hdc->transfer_offset += Y_HDC_SECTOR_SIZE;
  --hdc->transfer_sectors_remaining;
  // The guest reads the sector count register back to see how much of the
  // transfer got through, so it counts down as sectors complete.
  hdc->sector_count = (uint8_t)hdc->transfer_sectors_remaining;
  if (hdc->transfer_sectors_remaining == 0) {
    hdc->transfer = Y_HDC_TRANSFER_NONE;
    hdc->status = Y_HDC_STATUS_IDLE;
  }
}

Y_FILE_PRIVATE void YHDCHandleIdentifyDevice(
    YHDCState* hdc, YHDCDriveState* drive) {
  YHDCBuildIdentifyBlock(hdc, drive);
  YHDCStartTransfer(hdc, Y_HDC_TRANSFER_IDENTIFY, 0, 1);
}

// Works out the run of sectors a command addresses, and returns whether all of
// it lies on the drive. The whole run has to fit, not just its first sector: a
// byte callback has no way to refuse an address, so a command that would walk
// off the end has to fail before it moves anything rather than part way
// through.
Y_FILE_PRIVATE bool YHDCComputeSectorRange(
    YHDCState* hdc, const YHDCDriveState* drive, uint32_t* sector_number,
    uint16_t* num_sectors) {
  if (!YHDCComputeSectorNumber(hdc, drive, sector_number)) {
    return false;
  }
  // A sector count of zero asks for the full 256 sectors.
  *num_sectors = hdc->sector_count == 0 ? 256 : (uint16_t)hdc->sector_count;
  return *sector_number + *num_sectors <= YHDCDriveNumSectors(drive);
}

// Sets up a read or write of one or more sectors starting at the address in
// the task file. A sector count of zero means 256 sectors, which is how ATA
// encodes the largest transfer a single command can make.
Y_FILE_PRIVATE void YHDCHandleReadWriteSectors(
    YHDCState* hdc, YHDCDriveState* drive, YHDCTransfer transfer) {
  uint32_t sector_number = 0;
  uint16_t num_sectors = 0;
  if (!YHDCComputeSectorRange(hdc, drive, &sector_number, &num_sectors)) {
    YHDCFailCommand(hdc, Y_HDC_ERROR_ID_NOT_FOUND);
    return;
  }
  YHDCStartTransfer(
      hdc, transfer, sector_number * Y_HDC_SECTOR_SIZE, num_sectors);
}

Y_FILE_PRIVATE void YHDCHandleInitializeDeviceParameters(
    YHDCState* hdc, YHDCDriveState* drive) {
  const uint8_t num_heads = (hdc->drive_head & Y_HDC_DRIVE_HEAD_HEAD_MASK) + 1;
  const uint8_t num_sectors_per_track = hdc->sector_count;
  if (num_sectors_per_track == 0) {
    YHDCFailCommand(hdc, Y_HDC_ERROR_ABORTED);
    return;
  }
  drive->num_translated_heads = num_heads;
  drive->num_translated_sectors_per_track = num_sectors_per_track;
  Y_HDC_LOG(
      Y_LOG_LEVEL_DEBUG, "drive %u translated geometry set to %u heads, %u S/T",
      (hdc->drive_head & Y_HDC_DRIVE_HEAD_DRIVE_SELECT) ? 1 : 0, num_heads,
      num_sectors_per_track);
  YHDCFinishCommand(hdc);
}

// Checks that the addressed sectors exist without transferring them. The
// sector count counts here just as it does for a real transfer, so a verify
// that runs off the end of the drive fails rather than reporting success.
Y_FILE_PRIVATE void YHDCHandleReadVerifySectors(
    YHDCState* hdc, YHDCDriveState* drive) {
  uint32_t sector_number = 0;
  uint16_t num_sectors = 0;
  if (!YHDCComputeSectorRange(hdc, drive, &sector_number, &num_sectors)) {
    YHDCFailCommand(hdc, Y_HDC_ERROR_ID_NOT_FOUND);
    return;
  }
  YHDCFinishCommand(hdc);
}

Y_FILE_PRIVATE void YHDCExecuteCommand(YHDCState* hdc, uint8_t opcode) {
  YHDCDriveState* drive = YHDCSelectedDrive(hdc);
  if (!drive->present) {
    // An absent drive never answers, so the command simply goes unheard. The
    // status register is shared by both drives, and YHDCReadPort already
    // reports an absent one as never ready, so writing to it here would do
    // nothing except strand the other drive at a status of zero.
    return;
  }

  Y_HDC_LOG(
      Y_LOG_LEVEL_DEBUG, "drive %u command %02X, CHS %u/%u/%u, count %u",
      (hdc->drive_head & Y_HDC_DRIVE_HEAD_DRIVE_SELECT) ? 1 : 0, opcode,
      ((uint16_t)hdc->cylinder_high << 8) | hdc->cylinder_low,
      hdc->drive_head & Y_HDC_DRIVE_HEAD_HEAD_MASK, hdc->sector_number,
      hdc->sector_count);

  // The low nibble of recalibrate and seek is a stepping rate, so the whole
  // range is one command.
  const uint8_t command = ((opcode & 0xF0) == Y_HDC_COMMAND_RECALIBRATE ||
                           (opcode & 0xF0) == Y_HDC_COMMAND_SEEK)
                              ? (uint8_t)(opcode & 0xF0)
                              : opcode;

  switch (command) {
    case Y_HDC_COMMAND_RECALIBRATE:
    case Y_HDC_COMMAND_SEEK:
      // Seek time is not modelled, so the heads are already where they need
      // to be.
      YHDCFinishCommand(hdc);
      break;

    case Y_HDC_COMMAND_READ_VERIFY_SECTORS:
    case Y_HDC_COMMAND_READ_VERIFY_SECTORS_NO_RETRY:
      YHDCHandleReadVerifySectors(hdc, drive);
      break;

    case Y_HDC_COMMAND_READ_SECTORS:
    case Y_HDC_COMMAND_READ_SECTORS_NO_RETRY:
      YHDCHandleReadWriteSectors(hdc, drive, Y_HDC_TRANSFER_READ);
      break;

    case Y_HDC_COMMAND_WRITE_SECTORS:
    case Y_HDC_COMMAND_WRITE_SECTORS_NO_RETRY:
      YHDCHandleReadWriteSectors(hdc, drive, Y_HDC_TRANSFER_WRITE);
      break;

    case Y_HDC_COMMAND_INITIALIZE_DEVICE_PARAMETERS:
      YHDCHandleInitializeDeviceParameters(hdc, drive);
      break;

    case Y_HDC_COMMAND_IDENTIFY_DEVICE:
      YHDCHandleIdentifyDevice(hdc, drive);
      break;

    default:
      Y_HDC_LOG(Y_LOG_LEVEL_WARN, "unsupported command %02X", opcode);
      YHDCFailCommand(hdc, Y_HDC_ERROR_ABORTED);
      break;
  }
}

// Returns the byte at the current transfer position, without advancing.
Y_FILE_PRIVATE uint8_t YHDCReadTransferByte(YHDCState* hdc) {
  switch (hdc->transfer) {
    case Y_HDC_TRANSFER_IDENTIFY:
      return hdc->sector_buffer[hdc->transfer_byte_index];
    case Y_HDC_TRANSFER_READ:
      if (hdc->config.read_image_byte) {
        return hdc->config.read_image_byte(
            hdc->config.context, hdc->transfer_drive,
            hdc->transfer_offset + hdc->transfer_byte_index);
      }
      return 0;
    default:
      return 0;
  }
}

// Hands the guest the low byte of the next word, latching the high byte.
//
// The drive's data port is 16 bits wide and the card is on an 8-bit bus, so a
// read of the low byte port is what runs the bus cycle: the whole word comes
// off the drive at once, the low half goes to the guest, and the card holds
// the high half in its latch for the guest to collect from the other port.
// Reading the two ports is therefore not the same thing, and the card consumes
// a word per low byte read rather than a byte per read of either port.
Y_FILE_PRIVATE uint8_t YHDCReadDataRegister(YHDCState* hdc) {
  if (!(hdc->status & Y_HDC_STATUS_DATA_REQUEST)) {
    Y_HDC_LOG(Y_LOG_LEVEL_WARN, "data register read with no transfer active");
    return 0;
  }
  if (hdc->transfer == Y_HDC_TRANSFER_WRITE) {
    Y_HDC_LOG(Y_LOG_LEVEL_WARN, "data register read during a write");
    return 0;
  }

  // Sectors are a whole number of words, so a word never straddles a sector
  // boundary and both halves come from the same sector.
  const uint8_t low_byte = YHDCReadTransferByte(hdc);
  YHDCAdvanceTransfer(hdc);
  hdc->data_high_latch = YHDCReadTransferByte(hdc);
  YHDCAdvanceTransfer(hdc);
  return low_byte;
}

// Hands back the high byte of the word the last low byte read fetched.
//
// This port is wired to the card's latch and runs no bus cycle, so reading it
// twice returns the same value and reading it without having read the low byte
// port first returns whatever the previous word left behind. Nothing here
// touches the transfer, which is what keeps a guest that reads the ports out
// of order from sliding the transfer out of step.
Y_FILE_PRIVATE uint8_t YHDCReadDataHighRegister(YHDCState* hdc) {
  return hdc->data_high_latch;
}

// Writes one byte of the drive's image.
Y_FILE_PRIVATE void YHDCWriteImageByte(YHDCState* hdc, uint8_t value) {
  if (hdc->config.write_image_byte) {
    hdc->config.write_image_byte(
        hdc->config.context, hdc->transfer_drive,
        hdc->transfer_offset + hdc->transfer_byte_index, value);
  }
  YHDCAdvanceTransfer(hdc);
}

// Takes the next byte of a write from the guest.
//
// Writes are not the mirror image of reads. A read of the low byte port is
// what runs the bus cycle, so the drive hands over the low byte and the card
// latches the high one for the guest to collect afterwards - low byte first.
// A write is the other way round: the card cannot start the cycle until it has
// the whole word, so the guest writes the high byte to the latch first and the
// write of the low byte is what commits the pair. Streaming bytes in the order
// they arrive would put every word on the disk back to front.
Y_FILE_PRIVATE void YHDCWriteDataRegister(
    YHDCState* hdc, bool is_high_byte, uint8_t value) {
  if (!(hdc->status & Y_HDC_STATUS_DATA_REQUEST) ||
      hdc->transfer != Y_HDC_TRANSFER_WRITE) {
    Y_HDC_LOG(Y_LOG_LEVEL_WARN, "data register written with no write active");
    return;
  }

  if (is_high_byte) {
    hdc->data_high_latch = value;
    return;
  }

  // A word never straddles a sector boundary, so the two halves always land in
  // the same sector.
  YHDCWriteImageByte(hdc, value);
  YHDCWriteImageByte(hdc, hdc->data_high_latch);
}

// What the status and alternate status registers read back as. An empty drive
// slot leaves the bus undriven, which the guest reads as a drive that is never
// ready.
Y_FILE_PRIVATE uint8_t YHDCReadStatusRegister(YHDCState* hdc) {
  return YHDCSelectedDrive(hdc)->present ? hdc->status : 0;
}

// Handles a write to the device control register.
Y_FILE_PRIVATE void YHDCWriteDeviceControlRegister(
    YHDCState* hdc, uint8_t value) {
  // A software reset abandons whatever the drive was doing, which is the point
  // of it: it is how a guest recovers a transfer it cannot finish. Only the
  // assertion matters, not the release - the reset completes inside this port
  // write like every other command here, so there is no window in which the
  // guest could catch the drive busy. The device signature and diagnostic code
  // a reset leaves in the task file are not modelled, since nothing reads
  // them without an ATAPI device to tell apart.
  if (value & Y_HDC_DEVICE_CONTROL_SOFTWARE_RESET) {
    hdc->transfer = Y_HDC_TRANSFER_NONE;
    hdc->transfer_byte_index = 0;
    hdc->transfer_sectors_remaining = 0;
    hdc->data_high_latch = 0;
    hdc->error = 0;
    hdc->status = Y_HDC_STATUS_IDLE;
  }
}

Y_PUBLIC uint8_t YHDCReadPort(YHDCState* hdc, uint16_t port) {
  const uint8_t offset =
      (uint8_t)((port - Y_HDC_PORT_BASE) & (Y_HDC_NUM_PORTS - 1));
  switch (YHDCPortOffsetToRegister(offset)) {
    case Y_HDC_REGISTER_DATA:
      return YHDCReadDataRegister(hdc);
    case Y_HDC_REGISTER_DATA_HIGH:
      return YHDCReadDataHighRegister(hdc);
    case Y_HDC_REGISTER_ERROR:
      return hdc->error;
    case Y_HDC_REGISTER_SECTOR_COUNT:
      return hdc->sector_count;
    case Y_HDC_REGISTER_SECTOR_NUMBER:
      return hdc->sector_number;
    case Y_HDC_REGISTER_CYLINDER_LOW:
      return hdc->cylinder_low;
    case Y_HDC_REGISTER_CYLINDER_HIGH:
      return hdc->cylinder_high;
    case Y_HDC_REGISTER_DRIVE_HEAD:
      return hdc->drive_head;
    case Y_HDC_REGISTER_STATUS:
    case Y_HDC_REGISTER_DEVICE_CONTROL:
      return YHDCReadStatusRegister(hdc);
    default:
      return Y_HDC_OPEN_BUS_VALUE;
  }
}

Y_PUBLIC void YHDCWritePort(YHDCState* hdc, uint16_t port, uint8_t value) {
  const uint8_t offset =
      (uint8_t)((port - Y_HDC_PORT_BASE) & (Y_HDC_NUM_PORTS - 1));
  switch (YHDCPortOffsetToRegister(offset)) {
    case Y_HDC_REGISTER_DATA:
      YHDCWriteDataRegister(hdc, /*is_high_byte=*/false, value);
      break;
    case Y_HDC_REGISTER_DATA_HIGH:
      YHDCWriteDataRegister(hdc, /*is_high_byte=*/true, value);
      break;
    case Y_HDC_REGISTER_ERROR:
      hdc->features = value;
      break;
    case Y_HDC_REGISTER_SECTOR_COUNT:
      hdc->sector_count = value;
      break;
    case Y_HDC_REGISTER_SECTOR_NUMBER:
      hdc->sector_number = value;
      break;
    case Y_HDC_REGISTER_CYLINDER_LOW:
      hdc->cylinder_low = value;
      break;
    case Y_HDC_REGISTER_CYLINDER_HIGH:
      hdc->cylinder_high = value;
      break;
    case Y_HDC_REGISTER_DRIVE_HEAD:
      hdc->drive_head = value;
      break;
    case Y_HDC_REGISTER_STATUS:
      YHDCExecuteCommand(hdc, value);
      break;
    case Y_HDC_REGISTER_DEVICE_CONTROL:
      YHDCWriteDeviceControlRegister(hdc, value);
      break;
    default:
      break;
  }
}

Y_PUBLIC void YHDCAttachDrive(
    YHDCState* hdc, uint8_t drive, const YHDCDriveGeometry* geometry) {
  if (drive >= Y_HDC_NUM_DRIVES) {
    return;
  }
  YHDCDriveState* drive_state = &hdc->drives[drive];
  drive_state->present = true;
  drive_state->geometry = *geometry;
  // Until the guest says otherwise, addresses are translated against the
  // drive's own geometry.
  drive_state->num_translated_heads = geometry->num_heads;
  drive_state->num_translated_sectors_per_track =
      geometry->num_sectors_per_track;
}

Y_PUBLIC void YHDCDetachDrive(YHDCState* hdc, uint8_t drive) {
  if (drive >= Y_HDC_NUM_DRIVES) {
    return;
  }
  static const YHDCDriveState empty_drive_state = {0};
  hdc->drives[drive] = empty_drive_state;
}

Y_PUBLIC void YHDCInit(YHDCState* hdc) { hdc->status = Y_HDC_STATUS_IDLE; }

Y_PUBLIC uint32_t YHDCGetOptionROMSize(void) {
  return Y_HDC_OPTION_ROM_DATA_SIZE;
}

Y_PUBLIC const uint8_t* YHDCGetOptionROMData(void) { return YHDCOptionROMData; }
