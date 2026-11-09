#include "floppy.h"

#include <stdbool.h>

#include "ff.h"
#include "pico/time.h"
#include "sdcard.h"
// Generated into the build directory - see the floppy image section of
// CMakeLists.txt.
#include "generated/dos_floppy_data.h"

enum {
  kSectorSize = kSdcardBlockSize,
  // The only format the floppy controller takes.
  kImageSize = 360 * 1024,
  kNoSector = -1,
  // What a read from beyond the image, or one that failed, returns: what an
  // undriven data bus reads.
  kOpenBusByte = 0xFF,
};

static const char kImagePath[] = "yax86/floppy_a.img";

static const char* const kSourceDescriptions[] = {
    [kFloppySourceSDCard] = "yax86/floppy_a.img on the SD card",
    [kFloppySourceNoCard] = "built in - no SD card",
    [kFloppySourceCardNotAnswering] = "built in - the SD card did not answer",
    [kFloppySourceNoFilesystem] = "built in - no FAT volume on the SD card",
    [kFloppySourceNoImage] = "built in - no yax86/floppy_a.img",
    [kFloppySourceWrongSize] = "built in - yax86/floppy_a.img is not 360KB",
};

static FATFS g_volume;
static FIL g_image;
static FloppyStats g_stats;

// The one sector of the image held in memory. The floppy controller moves a
// sector a byte at a time, in order, so one is enough to read or write each
// from the card once.
static uint8_t g_sector[kSectorSize];
static int32_t g_sector_index = kNoSector;
static bool g_sector_dirty;

FloppySource FloppyOpen(void) {
  FloppySource source = kFloppySourceSDCard;
  if (!SdcardIsInserted()) {
    source = kFloppySourceNoCard;
  } else {
    // Mounting at once, rather than at the first access, is what brings the
    // card up, and says whether it answered.
    const FRESULT mounted = f_mount(&g_volume, "", 1);
    if (mounted == FR_NOT_READY || mounted == FR_DISK_ERR) {
      source = kFloppySourceCardNotAnswering;
    } else if (mounted != FR_OK) {
      source = kFloppySourceNoFilesystem;
    } else if (f_open(&g_image, kImagePath, FA_READ | FA_WRITE) != FR_OK) {
      source = kFloppySourceNoImage;
    } else if (f_size(&g_image) != kImageSize) {
      f_close(&g_image);
      source = kFloppySourceWrongSize;
    }
  }
  g_stats.source = source;
  return source;
}

const char* FloppyDescribeSource(FloppySource source) {
  return kSourceDescriptions[source];
}

FloppyStats FloppyGetStats(void) { return g_stats; }

// ============================================================================
// The built-in floppy
// ============================================================================

static uint8_t BuiltInReadByte(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, uint32_t offset) {
  return offset < kDOSFloppyDataSize ? kDOSFloppyData[offset] : kOpenBusByte;
}

static void BuiltInWriteByte(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, Y_UNUSED uint32_t offset,
    Y_UNUSED uint8_t value) {}

// ============================================================================
// The image on the card
// ============================================================================

// Move the card's file position to a sector of the image.
static bool SeekSector(int32_t index) {
  return f_lseek(&g_image, (FSIZE_t)index * kSectorSize) == FR_OK;
}

static void WriteBackSector(void) {
  if (!g_sector_dirty) {
    return;
  }
  const uint64_t start_us = time_us_64();
  UINT written = 0;
  if (!SeekSector(g_sector_index) ||
      f_write(&g_image, g_sector, kSectorSize, &written) != FR_OK ||
      written != kSectorSize) {
    ++g_stats.errors;
  }
  ++g_stats.sector_writes;
  g_stats.write_us += time_us_64() - start_us;
  g_sector_dirty = false;
}

// Bring a sector of the image into memory, writing back the one it replaces
// if the guest changed it.
static void LoadSector(int32_t index) {
  if (index == g_sector_index) {
    return;
  }
  WriteBackSector();
  const uint64_t start_us = time_us_64();
  UINT read = 0;
  g_sector_index = index;
  if (!SeekSector(index) ||
      f_read(&g_image, g_sector, kSectorSize, &read) != FR_OK ||
      read != kSectorSize) {
    for (uint32_t i = 0; i < kSectorSize; ++i) {
      g_sector[i] = kOpenBusByte;
    }
    ++g_stats.errors;
  }
  ++g_stats.sector_reads;
  g_stats.read_us += time_us_64() - start_us;
}

static uint8_t CardReadByte(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, uint32_t offset) {
  if (offset >= kImageSize) {
    return kOpenBusByte;
  }
  LoadSector((int32_t)(offset / kSectorSize));
  return g_sector[offset % kSectorSize];
}

// A sector the guest writes goes to the card as soon as its last byte is in,
// so that nothing it has written is lost if the power goes.
static void CardWriteByte(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, uint32_t offset,
    uint8_t value) {
  if (offset >= kImageSize) {
    return;
  }
  LoadSector((int32_t)(offset / kSectorSize));
  g_sector[offset % kSectorSize] = value;
  g_sector_dirty = true;
  if (offset % kSectorSize == kSectorSize - 1) {
    WriteBackSector();
  }
}

void FloppyInsert(YFDCState* fdc) {
  if (g_stats.source == kFloppySourceSDCard) {
    fdc->config.read_image_byte = CardReadByte;
    fdc->config.write_image_byte = CardWriteByte;
  } else {
    fdc->config.read_image_byte = BuiltInReadByte;
    fdc->config.write_image_byte = BuiltInWriteByte;
  }
  YFDCInsertDisk(fdc, 0, &YFDCFormat360KB);
}
