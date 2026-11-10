#include "floppy.h"

// Generated into the build directory - see the floppy image section of
// CMakeLists.txt.
#include "generated/dos_floppy_data.h"

enum {
  // The only format the floppy controller takes.
  kImageSize = 360 * 1024,
  // What a read from beyond the built-in floppy returns: what an undriven
  // data bus reads.
  kOpenBusByte = 0xFF,
};

static const char kImagePath[] = "yax86/floppy_a.img";

static DiskImage g_image;
// The path of an image inserted from the menu, which the image points to.
static char g_inserted_path[kFloppyMaxPath + 1];

void FloppyOpen(void) { DiskImageOpen(&g_image, kImagePath, kImageSize); }

const DiskImage* FloppyGetImage(void) { return &g_image; }

static uint8_t BuiltInReadByte(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, uint32_t offset) {
  return offset < kDOSFloppyDataSize ? kDOSFloppyData[offset] : kOpenBusByte;
}

static void BuiltInWriteByte(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, Y_UNUSED uint32_t offset,
    Y_UNUSED uint8_t value) {}

static uint8_t CardReadByte(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, uint32_t offset) {
  return DiskImageReadByte(&g_image, offset);
}

static void CardWriteByte(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, uint32_t offset,
    uint8_t value) {
  DiskImageWriteByte(&g_image, offset, value);
}

void FloppyInsert(YFDCState* fdc) {
  if (g_image.status == kDiskImageOpen) {
    fdc->config.read_image_byte = CardReadByte;
    fdc->config.write_image_byte = CardWriteByte;
  } else {
    fdc->config.read_image_byte = BuiltInReadByte;
    fdc->config.write_image_byte = BuiltInWriteByte;
  }
  YFDCInsertDisk(fdc, 0, &YFDCFormat360KB);
}

void FloppyEject(YFDCState* fdc) {
  DiskImageClose(&g_image);
  YFDCEjectDisk(fdc, 0);
}

DiskImageStatus FloppyInsertImage(YFDCState* fdc, const char* path) {
  FloppyEject(fdc);
  uint16_t length = 0;
  while (path[length] != '\0' && length < kFloppyMaxPath) {
    g_inserted_path[length] = path[length];
    ++length;
  }
  g_inserted_path[length] = '\0';
  const DiskImageStatus status =
      DiskImageOpen(&g_image, g_inserted_path, kImageSize);
  if (status != kDiskImageOpen) {
    // Nothing to fall back to: the drive stays empty.
    g_image.status = kDiskImageEjected;
    return status;
  }
  FloppyInsert(fdc);
  return status;
}
