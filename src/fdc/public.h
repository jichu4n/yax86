// Public interface for the Floppy Disk Controller (FDC) module.
#ifndef YAX86_FDC_PUBLIC_H
#define YAX86_FDC_PUBLIC_H

// This module emulates the NEC uPD765 Floppy Disk Controller.
// It handles I/O port communication and DMA transfers for floppy
// operations. Actual disk image access is delegated to the platform
// via callbacks.

#include <stdbool.h>
#include <stdint.h>

struct FDCState;

// Caller-provided runtime configuration for the FDC.
typedef struct FDCConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Callback for reading a sector from the floppy image.
  bool (*read_sector)(
      void* context, int drive, int track, int head, int sector, void* buffer);

  // Callback for writing a sector to the floppy image.
  bool (*write_sector)(
      void* context, int drive, int track, int head, int sector,
      const void* buffer);
} FDCConfig;

// State of the Floppy Disk Controller.
typedef struct FDCState {
  // Pointer to the FDC configuration.
  FDCConfig* config;

  // Other FDC state will be added here.
} FDCState;

// Initializes the FDC to its power-on state.
void FDCInit(FDCState* fdc, FDCConfig* config);

// Handles reads from the FDC's I/O ports.
uint8_t FDCReadPort(FDCState* fdc, uint16_t port);

// Handles writes to the FDC's I/O ports.
void FDCWritePort(FDCState* fdc, uint16_t port, uint8_t value);

#endif  // YAX86_FDC_PUBLIC_H

