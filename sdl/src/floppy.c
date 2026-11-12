#include "floppy.h"

#include <stdio.h>

#include "app_log.h"

const char* const kDefaultFloppyImagePath =
#ifdef __EMSCRIPTEN__
    // Packaged into the Emscripten filesystem at build time.
    "/floppy_a.img";
#else
    "floppy_a.img";
#endif  // __EMSCRIPTEN__

// The mounted image, held in memory for the lifetime of the program. 360KB
// is the only format the FDC emulates, so the only image size that can be
// mounted.
static uint8_t g_floppy_image[Y_FDC_FORMAT_360KB_IMAGE_SIZE];

static uint8_t FloppyReadByte(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, uint32_t offset) {
  return offset < Y_FDC_FORMAT_360KB_IMAGE_SIZE ? g_floppy_image[offset] : 0xFF;
}

static void FloppyWriteByte(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, uint32_t offset,
    uint8_t value) {
  if (offset < Y_FDC_FORMAT_360KB_IMAGE_SIZE) {
    g_floppy_image[offset] = value;
  }
}

bool FloppyMount(YPlatformState* platform, const char* path) {
  FILE* file = fopen(path, "rb");
  if (!file) {
    APP_ERROR("floppy_open_failed", Y_LOG_STRING("path", path));
    return false;
  }
  const size_t bytes_read =
      fread(g_floppy_image, 1, Y_FDC_FORMAT_360KB_IMAGE_SIZE, file);
  // A well-formed image is exactly Y_FDC_FORMAT_360KB_IMAGE_SIZE bytes: a short
  // read means it is too small, and a successful extra byte means it is too
  // large.
  const bool is_expected_size =
      bytes_read == Y_FDC_FORMAT_360KB_IMAGE_SIZE && fgetc(file) == EOF;
  fclose(file);

  if (!is_expected_size) {
    // 360KB is the only format there is.
    APP_ERROR(
        "floppy_wrong_size", Y_LOG_STRING("path", path),
        Y_LOG_UINT("expected_bytes", Y_FDC_FORMAT_360KB_IMAGE_SIZE));
    return false;
  }

  platform->fdc.config.read_image_byte = FloppyReadByte;
  platform->fdc.config.write_image_byte = FloppyWriteByte;
  YFDCInsertDisk(&platform->fdc, 0, &YFDCFormat360KB);
  return true;
}
