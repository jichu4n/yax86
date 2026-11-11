#include "usb_disk.h"

#include <string.h>

#include "pico/time.h"
#include "sdcard.h"
#include "tusb.h"

// A SCSI sense: what went wrong with the last command, for the host to ask.
enum {
  // NOT READY, MEDIUM NOT PRESENT: an empty card reader.
  kSenseNotReady = SCSI_SENSE_NOT_READY,
  kSenseMediumNotPresent = 0x3A,
  // UNIT ATTENTION, NOT READY TO READY CHANGE, MEDIUM MAY HAVE CHANGED: a card
  // just put in, which the host is to read afresh.
  kSenseUnitAttention = SCSI_SENSE_UNIT_ATTENTION,
  kSenseMediumChanged = 0x28,
  // ILLEGAL REQUEST, INVALID COMMAND OPERATION CODE, and LOGICAL BLOCK
  // ADDRESS OUT OF RANGE.
  kSenseIllegalRequest = SCSI_SENSE_ILLEGAL_REQUEST,
  kSenseInvalidCommand = 0x20,
  kSenseOutOfRange = 0x21,
  // MEDIUM ERROR, UNRECOVERED READ ERROR, and WRITE ERROR.
  kSenseMediumError = SCSI_SENSE_MEDIUM_ERROR,
  kSenseReadError = 0x11,
  kSenseWriteError = 0x0C,

  // SYNCHRONIZE CACHE (10), which a host sends before it lets go of a disk.
  kCommandSynchronizeCache10 = 0x35,
};

// What the reader says it is, space padded: 8, 16 and 4 characters.
static const char kVendor[] = "yax86   ";
static const char kProduct[] = "PicoCalc SD     ";
static const char kRevision[] = "1.0 ";

// Set from the main loop and read from the callbacks, which can interrupt it
// but not the other way around - so a change the main loop makes is whole by
// the time a callback sees it.
static volatile bool g_sharing;
// The image shared, or NULL for the whole card.
static DiskImage* volatile g_image;
// Whether the host is yet to be told the medium changed.
static volatile bool g_changed;
static volatile UsbDiskStats g_stats;

static void Share(DiskImage* image) {
  g_image = image;
  g_stats.seen = false;
  g_stats.ejected = false;
  g_stats.blocks_read = 0;
  g_stats.blocks_written = 0;
  g_stats.read_us = 0;
  g_stats.write_us = 0;
  g_stats.errors = 0;
  g_changed = true;
  g_sharing = true;
}

void UsbDiskShareCard(void) { Share(NULL); }

void UsbDiskShareImage(DiskImage* image) { Share(image); }

void UsbDiskStopSharing(void) { g_sharing = false; }

UsbDiskStats UsbDiskGetStats(void) {
  UsbDiskStats stats = {
      .sharing = g_sharing,
      .image_path = g_sharing && g_image != NULL ? g_image->path : NULL,
      .connected = tud_mounted(),
      .seen = g_stats.seen,
      .ejected = g_stats.ejected,
      .blocks_read = g_stats.blocks_read,
      .blocks_written = g_stats.blocks_written,
      .read_us = g_stats.read_us,
      .write_us = g_stats.write_us,
      .errors = g_stats.errors,
  };
  return stats;
}

// Whether the card is in the reader as the host sees it: shared, and not
// ejected by it.
static bool Present(void) { return g_sharing && !g_stats.ejected; }

// The blocks of what is shared.
static uint32_t BlockCount(void) {
  return g_image != NULL ? g_image->size / kSdcardBlockSize
                         : SdcardGetBlockCount();
}

// Move count blocks from lba between what is shared and data: the card's
// own, or an image's sectors, which are its blocks.
static bool ReadBlocks(uint32_t lba, uint8_t* data, uint32_t count) {
  if (g_image == NULL) {
    return SdcardReadBlocks(lba, data, count);
  }
  return DiskImageReadSectors(g_image, lba, data, count);
}

static bool WriteBlocks(uint32_t lba, const uint8_t* data, uint32_t count) {
  if (g_image == NULL) {
    return SdcardWriteBlocks(lba, data, count);
  }
  return DiskImageWriteSectors(g_image, lba, data, count);
}

void tud_msc_inquiry_cb(
    uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16],
    uint8_t product_rev[4]) {
  (void)lun;
  memcpy(vendor_id, kVendor, sizeof(kVendor) - 1);
  memcpy(product_id, kProduct, sizeof(kProduct) - 1);
  memcpy(product_rev, kRevision, sizeof(kRevision) - 1);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
  if (!Present()) {
    tud_msc_set_sense(lun, kSenseNotReady, kSenseMediumNotPresent, 0);
    return false;
  }
  if (g_changed) {
    g_changed = false;
    tud_msc_set_sense(lun, kSenseUnitAttention, kSenseMediumChanged, 0);
    return false;
  }
  g_stats.seen = true;
  return true;
}

void tud_msc_capacity_cb(
    uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
  (void)lun;
  *block_count = Present() ? BlockCount() : 0;
  *block_size = kSdcardBlockSize;
}

bool tud_msc_start_stop_cb(
    uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
  (void)lun;
  (void)power_condition;
  if (load_eject && !start && g_sharing) {
    g_stats.ejected = true;
  }
  return true;
}

// Check a transfer: a whole number of blocks, on the card, while it is in.
// Sets the sense and returns false if it cannot go ahead.
static bool CheckTransfer(
    uint8_t lun, uint32_t lba, uint32_t offset, uint32_t bufsize) {
  if (!Present()) {
    tud_msc_set_sense(lun, kSenseNotReady, kSenseMediumNotPresent, 0);
    return false;
  }
  const uint32_t count = bufsize / kSdcardBlockSize;
  if (offset != 0 || bufsize % kSdcardBlockSize != 0 || lba >= BlockCount() ||
      count > BlockCount() - lba) {
    tud_msc_set_sense(lun, kSenseIllegalRequest, kSenseOutOfRange, 0);
    ++g_stats.errors;
    return false;
  }
  return true;
}

int32_t tud_msc_read10_cb(
    uint8_t lun, uint32_t lba, uint32_t offset, void* buffer,
    uint32_t bufsize) {
  if (!CheckTransfer(lun, lba, offset, bufsize)) {
    return -1;
  }
  const uint32_t count = bufsize / kSdcardBlockSize;
  const uint64_t start_us = time_us_64();
  const bool read = ReadBlocks(lba, buffer, count);
  g_stats.read_us += time_us_64() - start_us;
  if (!read) {
    tud_msc_set_sense(lun, kSenseMediumError, kSenseReadError, 0);
    ++g_stats.errors;
    return -1;
  }
  g_stats.blocks_read += count;
  return (int32_t)bufsize;
}

int32_t tud_msc_write10_cb(
    uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer,
    uint32_t bufsize) {
  if (!CheckTransfer(lun, lba, offset, bufsize)) {
    return -1;
  }
  const uint32_t count = bufsize / kSdcardBlockSize;
  const uint64_t start_us = time_us_64();
  const bool written = WriteBlocks(lba, buffer, count);
  g_stats.write_us += time_us_64() - start_us;
  if (!written) {
    tud_msc_set_sense(lun, kSenseMediumError, kSenseWriteError, 0);
    ++g_stats.errors;
    return -1;
  }
  g_stats.blocks_written += count;
  return (int32_t)bufsize;
}

int32_t tud_msc_scsi_cb(
    uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize) {
  (void)buffer;
  (void)bufsize;
  switch (scsi_cmd[0]) {
    case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
      // Nothing to lock: the menu asks before taking the card back.
      return 0;
    case kCommandSynchronizeCache10:
      // Nothing to write back: each write has reached the card before its
      // callback returns.
      return 0;
    default:
      tud_msc_set_sense(lun, kSenseIllegalRequest, kSenseInvalidCommand, 0);
      return -1;
  }
}
