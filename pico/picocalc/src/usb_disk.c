#include "usb_disk.h"

#include "tusb.h"

// A SCSI sense: what went wrong with the last command, for the host to ask.
enum {
  // NOT READY, MEDIUM NOT PRESENT: an empty card reader.
  kSenseNotReady = SCSI_SENSE_NOT_READY,
  kSenseMediumNotPresent = 0x3A,
  // ILLEGAL REQUEST, INVALID COMMAND OPERATION CODE.
  kSenseIllegalRequest = SCSI_SENSE_ILLEGAL_REQUEST,
  kSenseInvalidCommand = 0x20,

  kBlockSize = 512,
};

// What the reader says it is, space padded: 8, 16 and 4 characters.
static const char kVendor[] = "yax86   ";
static const char kProduct[] = "PicoCalc SD     ";
static const char kRevision[] = "1.0 ";

static void CopyPadded(uint8_t* to, const char* from, uint8_t length) {
  for (uint8_t i = 0; i < length; ++i) {
    to[i] = (uint8_t)from[i];
  }
}

void tud_msc_inquiry_cb(
    uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16],
    uint8_t product_rev[4]) {
  (void)lun;
  CopyPadded(vendor_id, kVendor, sizeof(kVendor) - 1);
  CopyPadded(product_id, kProduct, sizeof(kProduct) - 1);
  CopyPadded(product_rev, kRevision, sizeof(kRevision) - 1);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
  tud_msc_set_sense(lun, kSenseNotReady, kSenseMediumNotPresent, 0);
  return false;
}

void tud_msc_capacity_cb(
    uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
  (void)lun;
  *block_count = 0;
  *block_size = kBlockSize;
}

int32_t tud_msc_read10_cb(
    uint8_t lun, uint32_t lba, uint32_t offset, void* buffer,
    uint32_t bufsize) {
  (void)lba;
  (void)offset;
  (void)buffer;
  (void)bufsize;
  tud_msc_set_sense(lun, kSenseNotReady, kSenseMediumNotPresent, 0);
  return -1;
}

int32_t tud_msc_write10_cb(
    uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer,
    uint32_t bufsize) {
  (void)lba;
  (void)offset;
  (void)buffer;
  (void)bufsize;
  tud_msc_set_sense(lun, kSenseNotReady, kSenseMediumNotPresent, 0);
  return -1;
}

int32_t tud_msc_scsi_cb(
    uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize) {
  (void)buffer;
  (void)bufsize;
  if (scsi_cmd[0] == SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL) {
    // Nothing to lock: the reader holds nothing the host could lose.
    return 0;
  }
  tud_msc_set_sense(lun, kSenseIllegalRequest, kSenseInvalidCommand, 0);
  return -1;
}
