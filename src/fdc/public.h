// Public interface for the Floppy Disk Controller (FDC) module.
#ifndef YAX86_FDC_PUBLIC_H
#define YAX86_FDC_PUBLIC_H

// This module emulates the NEC uPD765 Floppy Disk Controller. It handles I/O
// port communication and DMA transfers for floppy operations. Actual disk
// image access is delegated to the platform via callbacks.

#include <stdbool.h>
#include <stdint.h>

// Floppy disk format configuration.
typedef struct FDCDiskFormat {
  // Number of heads (1 or 2).
  uint8_t num_heads;
  // Number of tracks.
  uint8_t num_tracks;
  // Number of sectors per track.
  uint8_t num_sectors_per_track;
  // Size of each sector in bytes.
  uint16_t sector_size;
} FDCDiskFormat;

// 5.25" 360KB double-sided double-density floppy disk format.
static const FDCDiskFormat kFDCFormat360KB = {
    .num_heads = 2,
    .num_tracks = 40,
    .num_sectors_per_track = 9,
    .sector_size = 512,
};

struct FDCState;

enum {
  // Number of floppy drives supported by the FDC.
  kFDCNumDrives = 4,
};

// State for a single floppy drive.
typedef struct FDCDriveState {
  // Whether there is a disk inserted in the drive, i.e. whether an image is
  // mounted. The real hardware doesn't actually know this and will attempt to
  // access the disk and time out.
  bool present;
  // The format of the disk currently inserted in the drive.
  const FDCDiskFormat* format;
  // The track the read/write head is currently on.
  uint8_t track;
  // The currently active head (0 or 1).
  uint8_t head;
} FDCDriveState;

// Caller-provided runtime configuration for the FDC.
typedef struct FDCConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Callback to read a byte from a floppy image.
  uint8_t (*read_image_byte)(
      void* context,
      // 0 to kFDCNumDrives-1
      uint8_t drive,
      // byte offset within the image
      uint32_t offset);

  // Callback to write a byte to a floppy image.
  void (*write_image_byte)(
      void* context,
      // 0 to kFDCNumDrives-1
      uint8_t drive,
      // byte offset within the image
      uint32_t offset,
      // byte value to write
      uint8_t value);
} FDCConfig;

// State of the Floppy Disk Controller.
typedef struct FDCState {
  // Pointer to the FDC configuration.
  FDCConfig* config;

  // Per-drive state.
  FDCDriveState drives[kFDCNumDrives];

  // Other FDC state will be added here.
} FDCState;

// Initializes the FDC to its power-on state.
void FDCInit(FDCState* fdc, FDCConfig* config);

// Handles reads from the FDC's I/O ports.
uint8_t FDCReadPort(FDCState* fdc, uint16_t port);

// Handles writes to the FDC's I/O ports.
void FDCWritePort(FDCState* fdc, uint16_t port, uint8_t value);

// Inserts a disk with the given format into the specified drive.
void FDCInsertDisk(FDCState* fdc, uint8_t drive, const FDCDiskFormat* format);

// Ejects the disk from the specified drive.
void FDCEjectDisk(FDCState* fdc, uint8_t drive);

#endif  // YAX86_FDC_PUBLIC_H

