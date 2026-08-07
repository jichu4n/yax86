#include "floppy.h"

#include <stdio.h>

enum {
  // Size of a 5.25" 360KB floppy image. That is the only format the FDC
  // emulates, so it is the only image size that can be mounted.
  kFloppyImageSize = 360 * 1024,
};

const char* const kDefaultFloppyImagePath =
#ifdef __EMSCRIPTEN__
    // Packaged into the Emscripten filesystem at build time.
    "/floppy_a.img";
#else
    "floppy_a.img";
#endif  // __EMSCRIPTEN__

// The mounted image, held in memory for the lifetime of the program.
static uint8_t g_floppy_image[kFloppyImageSize];

static uint8_t FloppyReadByte(
    YAX86_UNUSED void* context, YAX86_UNUSED uint8_t drive, uint32_t offset) {
  return offset < kFloppyImageSize ? g_floppy_image[offset] : 0xFF;
}

static void FloppyWriteByte(
    YAX86_UNUSED void* context, YAX86_UNUSED uint8_t drive, uint32_t offset,
    uint8_t value) {
  if (offset < kFloppyImageSize) {
    g_floppy_image[offset] = value;
  }
}

bool FloppyMount(PlatformState* platform, const char* path) {
  FILE* file = fopen(path, "rb");
  if (!file) {
    fprintf(stderr, "Could not open floppy image '%s'\n", path);
    return false;
  }
  const size_t bytes_read = fread(g_floppy_image, 1, kFloppyImageSize, file);
  // A well-formed image is exactly kFloppyImageSize bytes: a short read means
  // it is too small, and a successful extra byte means it is too large.
  const bool is_expected_size =
      bytes_read == kFloppyImageSize && fgetc(file) == EOF;
  fclose(file);

  if (!is_expected_size) {
    fprintf(
        stderr,
        "Floppy image '%s' is not a %d KB image, which is the only format "
        "supported\n",
        path, kFloppyImageSize / 1024);
    return false;
  }

  platform->fdc_config.read_image_byte = FloppyReadByte;
  platform->fdc_config.write_image_byte = FloppyWriteByte;
  FDCInsertDisk(&platform->fdc, 0, &kFDCFormat360KB);
  return true;
}
