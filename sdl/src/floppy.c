#include "floppy.h"

#include <stdio.h>
#include <strings.h>

#include "app_log.h"

const char* const kDefaultFloppyImagePath =
#ifdef __EMSCRIPTEN__
    // Packaged into the Emscripten filesystem at build time.
    "/floppy_a.img";
#else
    "floppy_a.img";
#endif  // __EMSCRIPTEN__

// The mounted image, held in memory for the lifetime of the program, sized for
// the largest format the FDC supports, and the size of the image currently
// mounted.
static uint8_t g_floppy_image[Y_FDC_MAX_IMAGE_SIZE];
static uint32_t g_floppy_image_size;

static uint8_t FloppyReadByte(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, uint32_t offset) {
  return offset < g_floppy_image_size ? g_floppy_image[offset] : 0xFF;
}

static void FloppyWriteByte(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, uint32_t offset,
    uint8_t value) {
  if (offset < g_floppy_image_size) {
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
      fread(g_floppy_image, 1, Y_FDC_MAX_IMAGE_SIZE, file);
  // A well-formed image is an exact supported format size: a short read means
  // it is smaller or between formats, and an extra byte past the buffer means
  // it is larger than the largest format.
  const bool is_complete = !ferror(file) && fgetc(file) == EOF;
  fclose(file);

  const YFDCDiskFormat* format =
      is_complete ? YFDCFindFormat((uint32_t)bytes_read) : NULL;
  if (!format) {
    char expected[64];
    size_t pos = 0;
    for (uint8_t i = 0; i < Y_FDC_NUM_FORMATS && pos < sizeof(expected); ++i) {
      const int n = snprintf(
          expected + pos, sizeof(expected) - pos, "%s%lu", i > 0 ? "," : "",
          (unsigned long)YFDCFormatImageSizes[i]);
      if (n > 0) {
        pos += (size_t)n;
      }
    }
    APP_ERROR(
        "floppy_wrong_size", Y_LOG_STRING("path", path),
        Y_LOG_STRING("expected_bytes", expected));
    return false;
  }
  if (!YFDCDoesDriveTypeSupportFormat(
          platform->fdc.config.drive_types[0], format)) {
    APP_ERROR(
        "floppy_incompatible_drive_type", Y_LOG_STRING("path", path),
        Y_LOG_UINT("bytes", (uint32_t)bytes_read));
    return false;
  }

  g_floppy_image_size = (uint32_t)bytes_read;
  platform->fdc.config.read_image_byte = FloppyReadByte;
  platform->fdc.config.write_image_byte = FloppyWriteByte;
  YFDCInsertDisk(&platform->fdc, 0, format);
  return true;
}

bool FloppyParseDriveType(const char* value, YFDCDriveType* drive_type) {
  if (strcasecmp(value, "360k") == 0 || strcasecmp(value, "360kb") == 0) {
    *drive_type = Y_FDC_DRIVE_TYPE_360KB;
    return true;
  }
  if (strcasecmp(value, "720k") == 0 || strcasecmp(value, "720kb") == 0) {
    *drive_type = Y_FDC_DRIVE_TYPE_720KB;
    return true;
  }
  if (strcasecmp(value, "1.2m") == 0 || strcasecmp(value, "1.2mb") == 0) {
    *drive_type = Y_FDC_DRIVE_TYPE_1200KB;
    return true;
  }
  if (strcasecmp(value, "1.44m") == 0 || strcasecmp(value, "1.44mb") == 0) {
    *drive_type = Y_FDC_DRIVE_TYPE_1440KB;
    return true;
  }
  return false;
}
