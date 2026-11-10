#include "floppy.h"

#include "file_list.h"
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

const char kFloppyDefaultPath[] = "yax86/floppy_a.img";

static DiskImage g_images[kFloppyDrives];
// The paths of the images, which the images point to.
static char g_inserted_paths[kFloppyDrives][kFileListMaxPath + 1];
// Whether drive A holds the floppy built into flash.
static bool g_built_in;

void FloppyOpen(
    const char* const paths[kFloppyDrives], bool built_in_fallback,
    DiskImageStatus opened[kFloppyDrives]) {
  for (uint8_t drive = 0; drive < kFloppyDrives; ++drive) {
    DiskImage* image = &g_images[drive];
    if (paths[drive] == NULL || paths[drive][0] == '\0') {
      image->status = kDiskImageEjected;
    } else {
      FileListCopyPath(paths[drive], g_inserted_paths[drive]);
      DiskImageOpen(image, g_inserted_paths[drive], kImageSize);
    }
    opened[drive] = image->status;
  }
  g_built_in =
      built_in_fallback && g_images[kFloppyDriveA].status != kDiskImageOpen;
  for (uint8_t drive = 0; drive < kFloppyDrives; ++drive) {
    if (g_images[drive].status != kDiskImageOpen &&
        !(drive == kFloppyDriveA && g_built_in)) {
      g_images[drive].status = kDiskImageEjected;
    }
  }
}

const DiskImage* FloppyGetImage(FloppyDrive drive) { return &g_images[drive]; }

FloppyDrive FloppyDriveHolding(const char* path) {
  for (uint8_t drive = 0; drive < kFloppyDrives; ++drive) {
    const DiskImage* image = &g_images[drive];
    if (image->status == kDiskImageOpen &&
        FileListSamePath(image->path, path)) {
      return (FloppyDrive)drive;
    }
  }
  return kFloppyDrives;
}

static uint8_t ReadByte(
    Y_UNUSED void* context, uint8_t drive, uint32_t offset) {
  if (drive == kFloppyDriveA && g_built_in) {
    return offset < kDOSFloppyDataSize ? kDOSFloppyData[offset] : kOpenBusByte;
  }
  if (drive >= kFloppyDrives) {
    return kOpenBusByte;
  }
  return DiskImageReadByte(&g_images[drive], offset);
}

static void WriteByte(
    Y_UNUSED void* context, uint8_t drive, uint32_t offset, uint8_t value) {
  if ((drive == kFloppyDriveA && g_built_in) || drive >= kFloppyDrives) {
    return;
  }
  DiskImageWriteByte(&g_images[drive], offset, value);
}

void FloppyAttach(YFDCState* fdc) {
  fdc->config.read_image_byte = ReadByte;
  fdc->config.write_image_byte = WriteByte;
  for (uint8_t drive = 0; drive < kFloppyDrives; ++drive) {
    if (g_images[drive].status == kDiskImageOpen ||
        (drive == kFloppyDriveA && g_built_in)) {
      YFDCInsertDisk(fdc, drive, &YFDCFormat360KB);
    }
  }
}

void FloppyEject(YFDCState* fdc, FloppyDrive drive) {
  DiskImageClose(&g_images[drive]);
  if (drive == kFloppyDriveA) {
    g_built_in = false;
  }
  YFDCEjectDisk(fdc, drive);
}

DiskImageStatus FloppyInsertImage(
    YFDCState* fdc, FloppyDrive drive, const char* path) {
  FloppyEject(fdc, drive);
  char* inserted_path = g_inserted_paths[drive];
  FileListCopyPath(path, inserted_path);
  DiskImage* image = &g_images[drive];
  const DiskImageStatus status =
      DiskImageOpen(image, inserted_path, kImageSize);
  if (status != kDiskImageOpen) {
    // Nothing to fall back to: the drive stays empty.
    image->status = kDiskImageEjected;
    return status;
  }
  YFDCInsertDisk(fdc, drive, &YFDCFormat360KB);
  return status;
}
