#include "harddisk.h"

#include "file_list.h"

static DiskImage g_images[kHardDisks];
static char g_paths[kHardDisks][kFileListMaxPath + 1];

void HardDiskOpen(const char* const paths[kHardDisks]) {
  for (uint8_t drive = 0; drive < kHardDisks; ++drive) {
    DiskImage* image = &g_images[drive];
    if (paths[drive] == NULL || paths[drive][0] == '\0') {
      image->status = kDiskImageEjected;
      continue;
    }
    FileListCopyPath(paths[drive], g_paths[drive]);
    DiskImageOpen(image, g_paths[drive], Y_HDC_GEOMETRY_10MB_IMAGE_SIZE);
  }
}

const DiskImage* HardDiskGetImage(HardDiskDrive drive) {
  return &g_images[drive];
}

static uint8_t ReadByte(
    Y_UNUSED void* context, uint8_t drive, uint32_t offset) {
  if (drive >= kHardDisks) {
    return 0xFF;
  }
  return DiskImageReadByte(&g_images[drive], offset);
}

static void WriteByte(
    Y_UNUSED void* context, uint8_t drive, uint32_t offset, uint8_t value) {
  if (drive >= kHardDisks) {
    return;
  }
  DiskImageWriteByte(&g_images[drive], offset, value);
}

void HardDiskAttach(YHDCState* hdc) {
  hdc->config.read_image_byte = ReadByte;
  hdc->config.write_image_byte = WriteByte;
  for (uint8_t drive = 0; drive < kHardDisks; ++drive) {
    if (g_images[drive].status == kDiskImageOpen) {
      YHDCAttachDrive(hdc, drive, &YHDCGeometry10MB);
    }
  }
}
