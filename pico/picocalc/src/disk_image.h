// A disk image file on the SD card, which a drive reads and writes a byte at
// a time.
//
// The card is mounted the first time an image is opened, and stays mounted.
// What the guest writes goes to the card a sector at a time, as soon as the
// last byte of the sector is in, so nothing it has written waits in memory to
// be lost when the power goes.
#ifndef YAX86_PICOCALC_DISK_IMAGE_H
#define YAX86_PICOCALC_DISK_IMAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "ff.h"
#include "sdcard.h"

// Whether an image is open, and if not, why.
typedef enum DiskImageStatus {
  kDiskImageOpen = 0,
  kDiskImageNoCard,
  kDiskImageCardNotAnswering,
  kDiskImageNoFilesystem,
  kDiskImageNotFound,
  kDiskImageWrongSize,
  // Closed with DiskImageClose(): a drive with no disk in it.
  kDiskImageEjected,
} DiskImageStatus;

// What reading and writing an image has cost, for the report.
typedef struct DiskImageStats {
  // Sectors read from and written to the card, reads and writes that failed,
  // and the time they took, which the machine waits for.
  uint32_t sector_reads;
  uint32_t sector_writes;
  uint32_t errors;
  uint64_t read_us;
  uint64_t write_us;
} DiskImageStats;

enum {
  // The size of an image's map of where its clusters are on the card: two
  // entries for each run of clusters that follow each other, and two more.
  // 64 covers an image in up to 31 pieces, and a file copied onto a card is
  // usually in one.
  kDiskImageLinkMapEntries = 64,
  // The longest name DiskImageName() gives: an 8.3 name, which is all the
  // card's file system keeps.
  kDiskImageNameLength = 12,
};

typedef struct DiskImage {
  DiskImageStatus status;
  // Where the image is on the card, whether or not it opened. The caller's
  // string, which must outlive the image.
  const char* path;
  // Whether seeking looks the sector up in link_map. Without it, FatFs walks
  // the cluster chain from the start of the file to seek backwards, which an
  // image that is too fragmented for the map falls back to.
  bool fast_seek;
  DiskImageStats stats;

  FIL file;
  uint32_t size;
  DWORD link_map[kDiskImageLinkMapEntries];

  // The one sector of the image held in memory. A drive moves a sector a byte
  // at a time, in order, so one is enough to read or write each from the card
  // once.
  uint8_t sector[kSdcardBlockSize];
  int32_t sector_index;
  bool sector_dirty;
} DiskImage;

// Open the image at path on the card, which must be exactly size bytes, and
// return whether it opened. The result is also kept in image->status.
DiskImageStatus DiskImageOpen(
    DiskImage* image, const char* path, uint32_t size);

// A few words on why an image did not open, for the status page.
const char* DiskImageDescribeStatus(DiskImageStatus status);

// Close an image, writing back the sector the guest was part way through
// writing, if any, and leave it ejected.
void DiskImageClose(DiskImage* image);

// Mount the card's volume, the first time an image or this asks, and return
// whether it is mounted: kDiskImageOpen if it is, and why not if it is not.
DiskImageStatus DiskImageMountCard(void);

// An image's file name, without its folders, in capitals, as DOS shows a
// name. A name too long for kDiskImageNameLength keeps its first characters
// and ends in ~, as DOS shortens a long name.
void DiskImageName(const char* path, char name[kDiskImageNameLength + 1]);

// Read or write a byte of an open image. A read from beyond the image, or
// from a sector that could not be read, returns 0xFF; a write beyond it is
// dropped.
uint8_t DiskImageReadByte(DiskImage* image, uint32_t offset);
void DiskImageWriteByte(DiskImage* image, uint32_t offset, uint8_t value);

#endif  // YAX86_PICOCALC_DISK_IMAGE_H
