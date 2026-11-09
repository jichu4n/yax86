#include "harddisk.h"

#include <stdio.h>

#include "app_log.h"

// The mounted image, held in memory for the lifetime of the program. A blank
// disk is all zeroes, which is what an unpartitioned drive looks like.
static uint8_t g_hdd_image[Y_HDC_GEOMETRY_10MB_IMAGE_SIZE];

static uint8_t HardDiskReadByte(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, uint32_t offset) {
  return offset < Y_HDC_GEOMETRY_10MB_IMAGE_SIZE ? g_hdd_image[offset] : 0xFF;
}

static void HardDiskWriteByte(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, uint32_t offset,
    uint8_t value) {
  if (offset < Y_HDC_GEOMETRY_10MB_IMAGE_SIZE) {
    g_hdd_image[offset] = value;
  }
}

bool HardDiskAttach(YPlatformState* platform, const char* path) {
  if (path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
      APP_ERROR("hard_disk_open_failed", Y_LOG_STRING("path", path));
      return false;
    }
    const size_t bytes_read =
        fread(g_hdd_image, 1, Y_HDC_GEOMETRY_10MB_IMAGE_SIZE, file);
    // A well-formed image is exactly the size of the drive's geometry: a short
    // read means it is too small, and a successful extra byte means it is too
    // large.
    const bool is_expected_size =
        bytes_read == Y_HDC_GEOMETRY_10MB_IMAGE_SIZE && fgetc(file) == EOF;
    fclose(file);

    if (!is_expected_size) {
      // The 10MB drive is the only geometry there is.
      APP_ERROR(
          "hard_disk_wrong_size", Y_LOG_STRING("path", path),
          Y_LOG_UINT("expected_bytes", Y_HDC_GEOMETRY_10MB_IMAGE_SIZE));
      return false;
    }
  }

  platform->hdc.config.read_image_byte = HardDiskReadByte;
  platform->hdc.config.write_image_byte = HardDiskWriteByte;
  YHDCAttachDrive(&platform->hdc, 0, &YHDCGeometry10MB);
  return true;
}
