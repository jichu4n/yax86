#include "harddisk.h"

#include <stdio.h>

// The mounted image, held in memory for the lifetime of the program. A blank
// disk is all zeroes, which is what an unpartitioned drive looks like.
static uint8_t g_hdd_image[kHDCGeometry10MBImageSize];

static uint8_t HardDiskReadByte(
    YAX86_UNUSED void* context, YAX86_UNUSED uint8_t drive, uint32_t offset) {
  return offset < kHDCGeometry10MBImageSize ? g_hdd_image[offset] : 0xFF;
}

static void HardDiskWriteByte(
    YAX86_UNUSED void* context, YAX86_UNUSED uint8_t drive, uint32_t offset,
    uint8_t value) {
  if (offset < kHDCGeometry10MBImageSize) {
    g_hdd_image[offset] = value;
  }
}

bool HardDiskAttach(PlatformState* platform, const char* path) {
  if (path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
      fprintf(stderr, "Could not open hard disk image '%s'\n", path);
      return false;
    }
    const size_t bytes_read =
        fread(g_hdd_image, 1, kHDCGeometry10MBImageSize, file);
    // A well-formed image is exactly the size of the drive's geometry: a short
    // read means it is too small, and a successful extra byte means it is too
    // large.
    const bool is_expected_size =
        bytes_read == kHDCGeometry10MBImageSize && fgetc(file) == EOF;
    fclose(file);

    if (!is_expected_size) {
      fprintf(
          stderr,
          "Hard disk image '%s' is not a %d MB image, which is the only "
          "geometry supported\n",
          path, kHDCGeometry10MBImageSize / (1024 * 1024));
      return false;
    }
  }

  platform->hdc_config.read_image_byte = HardDiskReadByte;
  platform->hdc_config.write_image_byte = HardDiskWriteByte;
  HDCAttachDrive(&platform->hdc, 0, &kHDCGeometry10MB);
  return true;
}
