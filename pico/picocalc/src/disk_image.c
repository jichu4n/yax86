#include "disk_image.h"

#include <ctype.h>
#include <string.h>

#include "pico/time.h"

enum {
  kSectorSize = kSdcardBlockSize,
  kNoSector = -1,
  // What a read from beyond the image, or one that failed, returns: what an
  // undriven data bus reads.
  kOpenBusByte = 0xFF,
};

static const char* const kStatusDescriptions[] = {
    [kDiskImageOpen] = "open",
    [kDiskImageNoCard] = "no SD card",
    [kDiskImageCardNotAnswering] = "SD card did not answer",
    [kDiskImageNoFilesystem] = "no FAT volume",
    [kDiskImageNotFound] = "no image",
    [kDiskImageWrongSize] = "image is the wrong size",
    [kDiskImageEjected] = "ejected",
};

static FATFS g_volume;
static bool g_did_try_mount;
static DiskImageStatus g_mount_status;

void DiskImageUnmountCard(void) {
  if (g_did_try_mount && g_mount_status == kDiskImageOpen) {
    f_mount(NULL, "", 0);
  }
  g_did_try_mount = false;
}

DiskImageStatus DiskImageMountCard(void) {
  if (g_did_try_mount) {
    return g_mount_status;
  }
  g_did_try_mount = true;
  g_mount_status = kDiskImageOpen;
  if (!SdcardIsInserted()) {
    g_mount_status = kDiskImageNoCard;
  } else {
    // Mounting at once, rather than at the first access, is what brings the
    // card up, and says whether it answered.
    const FRESULT mounted = f_mount(&g_volume, "", 1);
    if (mounted == FR_NOT_READY || mounted == FR_DISK_ERR) {
      g_mount_status = kDiskImageCardNotAnswering;
    } else if (mounted != FR_OK) {
      g_mount_status = kDiskImageNoFilesystem;
    }
  }
  return g_mount_status;
}

// Have FatFs map where the image's clusters are, so that a seek is a lookup.
static bool MapClusters(DiskImage* image) {
  image->link_map[0] = kDiskImageNumLinkMapEntries;
  image->file.cltbl = image->link_map;
  if (f_lseek(&image->file, CREATE_LINKMAP) == FR_OK) {
    return true;
  }
  image->file.cltbl = NULL;
  return false;
}

static DiskImageStatus Open(DiskImage* image, const char* path, uint32_t size) {
  const DiskImageStatus mounted = DiskImageMountCard();
  if (mounted != kDiskImageOpen) {
    return mounted;
  }
  if (f_open(&image->file, path, FA_READ | FA_WRITE) != FR_OK) {
    return kDiskImageNotFound;
  }
  if (f_size(&image->file) != size) {
    f_close(&image->file);
    return kDiskImageWrongSize;
  }
  image->size = size;
  image->sector_index = kNoSector;
  image->uses_fast_seek = MapClusters(image);
  return kDiskImageOpen;
}

DiskImageStatus DiskImageOpen(
    DiskImage* image, const char* path, uint32_t size) {
  image->path = path;
  image->status = Open(image, path, size);
  return image->status;
}

const char* DiskImageDescribeStatus(DiskImageStatus status) {
  return kStatusDescriptions[status];
}

void DiskImageName(const char* path, char name[kDiskImageNameLength + 1]) {
  const char* slash = strrchr(path, '/');
  const char* start = slash != NULL ? slash + 1 : path;
  const size_t length = strlen(start);
  const size_t kept =
      length > kDiskImageNameLength ? kDiskImageNameLength - 1 : length;
  for (size_t i = 0; i < kept; ++i) {
    name[i] = (char)toupper((unsigned char)start[i]);
  }
  if (length > kDiskImageNameLength) {
    name[kept] = '~';
    name[kept + 1] = '\0';
  } else {
    name[kept] = '\0';
  }
}

// Move the card's file position to a sector of the image.
static bool SeekSector(DiskImage* image, int32_t index) {
  return f_lseek(&image->file, (FSIZE_t)index * kSectorSize) == FR_OK;
}

static void WriteBackSector(DiskImage* image) {
  if (!image->is_sector_dirty) {
    return;
  }
  const uint64_t start_us = time_us_64();
  UINT written = 0;
  if (!SeekSector(image, image->sector_index) ||
      f_write(&image->file, image->sector, kSectorSize, &written) != FR_OK ||
      written != kSectorSize) {
    ++image->stats.errors;
  }
  ++image->stats.sector_writes;
  image->stats.write_us += time_us_64() - start_us;
  image->is_sector_dirty = false;
}

// Bring a sector of the image into memory, writing back the one it replaces
// if the guest changed it.
static void LoadSector(DiskImage* image, int32_t index) {
  if (index == image->sector_index) {
    return;
  }
  WriteBackSector(image);
  const uint64_t start_us = time_us_64();
  UINT read = 0;
  image->sector_index = index;
  if (!SeekSector(image, index) ||
      f_read(&image->file, image->sector, kSectorSize, &read) != FR_OK ||
      read != kSectorSize) {
    for (uint32_t i = 0; i < kSectorSize; ++i) {
      image->sector[i] = kOpenBusByte;
    }
    ++image->stats.errors;
  }
  ++image->stats.sector_reads;
  image->stats.read_us += time_us_64() - start_us;
}

uint8_t DiskImageReadByte(DiskImage* image, uint32_t offset) {
  if (image->status != kDiskImageOpen || offset >= image->size) {
    return kOpenBusByte;
  }
  LoadSector(image, (int32_t)(offset / kSectorSize));
  return image->sector[offset % kSectorSize];
}

void DiskImageWriteByte(DiskImage* image, uint32_t offset, uint8_t value) {
  if (image->status != kDiskImageOpen || offset >= image->size) {
    return;
  }
  LoadSector(image, (int32_t)(offset / kSectorSize));
  image->sector[offset % kSectorSize] = value;
  image->is_sector_dirty = true;
  if (offset % kSectorSize == kSectorSize - 1) {
    WriteBackSector(image);
  }
}

bool DiskImageReadSectors(
    DiskImage* image, uint32_t index, uint8_t* data, uint32_t num_sectors) {
  if (image->status != kDiskImageOpen) {
    return false;
  }
  // The held sector may have bytes the card does not.
  WriteBackSector(image);
  const uint64_t start_us = time_us_64();
  const UINT length = num_sectors * kSectorSize;
  UINT read = 0;
  const bool is_ok = SeekSector(image, (int32_t)index) &&
                     f_read(&image->file, data, length, &read) == FR_OK &&
                     read == length;
  if (!is_ok) {
    ++image->stats.errors;
  }
  image->stats.sector_reads += num_sectors;
  image->stats.read_us += time_us_64() - start_us;
  return is_ok;
}

bool DiskImageWriteSectors(
    DiskImage* image, uint32_t index, const uint8_t* data,
    uint32_t num_sectors) {
  if (image->status != kDiskImageOpen) {
    return false;
  }
  WriteBackSector(image);
  // The held sector may be one of these, which would leave it stale.
  image->sector_index = kNoSector;
  const uint64_t start_us = time_us_64();
  const UINT length = num_sectors * kSectorSize;
  UINT written = 0;
  const bool is_ok = SeekSector(image, (int32_t)index) &&
                     f_write(&image->file, data, length, &written) == FR_OK &&
                     written == length;
  if (!is_ok) {
    ++image->stats.errors;
  }
  image->stats.sector_writes += num_sectors;
  image->stats.write_us += time_us_64() - start_us;
  return is_ok;
}

void DiskImageClose(DiskImage* image) {
  if (image->status == kDiskImageOpen) {
    WriteBackSector(image);
    f_close(&image->file);
  }
  image->status = kDiskImageEjected;
  image->is_sector_dirty = false;
}
