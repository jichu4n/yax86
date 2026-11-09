#include "harddisk.h"

static const char kImagePath[] = "yax86/hdd.img";

static DiskImage g_image;

void HardDiskOpen(void) {
  DiskImageOpen(&g_image, kImagePath, Y_HDC_GEOMETRY_10MB_IMAGE_SIZE);
}

const DiskImage* HardDiskGetImage(void) { return &g_image; }

static uint8_t ReadByte(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, uint32_t offset) {
  return DiskImageReadByte(&g_image, offset);
}

static void WriteByte(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, uint32_t offset,
    uint8_t value) {
  DiskImageWriteByte(&g_image, offset, value);
}

void HardDiskAttach(YHDCState* hdc) {
  if (g_image.status != kDiskImageOpen) {
    return;
  }
  hdc->config.read_image_byte = ReadByte;
  hdc->config.write_image_byte = WriteByte;
  YHDCAttachDrive(hdc, 0, &YHDCGeometry10MB);
}
