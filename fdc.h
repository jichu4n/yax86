// ==============================================================================
// YAX86 FDC MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_FDC_BUNDLE_H
#define YAX86_FDC_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/util/static_vector.h start
// ==============================================================================

#line 1 "./src/util/static_vector.h"
// Static vector library.
//
// A static vector is a vector backed by a fixed-size array. It's essentially
// a vector, but whose underlying storage is statically allocated and does not
// rely on dynamic memory allocation.

#ifndef YAX86_UTIL_STATIC_VECTOR_H
#define YAX86_UTIL_STATIC_VECTOR_H

#include <stddef.h>
#include <stdint.h>

// Header structure at the beginning of a static vector.
typedef struct StaticVectorHeader {
  // Element size in bytes.
  size_t element_size;
  // Maximum number of elements the vector can hold.
  size_t max_length;
  // Number of elements currently in the vector.
  size_t length;
} StaticVectorHeader;

// Define a static vector type with an element type.
#define STATIC_VECTOR_TYPE(name, element_type, max_length_value)          \
  typedef struct name {                                                   \
    StaticVectorHeader header;                                            \
    element_type elements[max_length_value];                              \
  } name;                                                                 \
  static void name##Init(name* vector) __attribute__((unused));           \
  static void name##Init(name* vector) {                                  \
    static const StaticVectorHeader header = {                            \
        .element_size = sizeof(element_type),                             \
        .max_length = (max_length_value),                                 \
        .length = 0,                                                      \
    };                                                                    \
    vector->header = header;                                              \
  }                                                                       \
  static size_t name##Length(const name* vector) __attribute__((unused)); \
  static size_t name##Length(const name* vector) {                        \
    return vector->header.length;                                         \
  }                                                                       \
  static element_type* name##Get(name* vector, size_t index)              \
      __attribute__((unused));                                            \
  static element_type* name##Get(name* vector, size_t index) {            \
    if (index >= (max_length_value)) {                                    \
      return NULL;                                                        \
    }                                                                     \
    return &(vector->elements[index]);                                    \
  }                                                                       \
  static bool name##Append(name* vector, const element_type* element)     \
      __attribute__((unused));                                            \
  static bool name##Append(name* vector, const element_type* element) {   \
    if (vector->header.length >= (max_length_value)) {                    \
      return false;                                                       \
    }                                                                     \
    vector->elements[vector->header.length++] = *element;                 \
    return true;                                                          \
  }                                                                       \
  static bool name##Insert(                                               \
      name* vector, size_t index, const element_type* element)            \
      __attribute__((unused));                                            \
  static bool name##Insert(                                               \
      name* vector, size_t index, const element_type* element) {          \
    if (index > vector->header.length ||                                  \
        vector->header.length >= (max_length_value)) {                    \
      return false;                                                       \
    }                                                                     \
    for (size_t i = vector->header.length; i > index; --i) {              \
      vector->elements[i] = vector->elements[i - 1];                      \
    }                                                                     \
    vector->elements[index] = *element;                                   \
    ++vector->header.length;                                              \
    return true;                                                          \
  }                                                                       \
  static bool name##Remove(name* vector, size_t index)                    \
      __attribute__((unused));                                            \
  static bool name##Remove(name* vector, size_t index) {                  \
    if (index >= vector->header.length) {                                 \
      return false;                                                       \
    }                                                                     \
    for (size_t i = index; i < vector->header.length - 1; ++i) {          \
      vector->elements[i] = vector->elements[i + 1];                      \
    }                                                                     \
    --vector->header.length;                                              \
    return true;                                                          \
  }                                                                       \
  static void name##Clear(name* vector) __attribute__((unused));          \
  static void name##Clear(name* vector) { vector->header.length = 0; }

#endif  // YAX86_UTIL_STATIC_VECTOR_H


// ==============================================================================
// src/util/static_vector.h end
// ==============================================================================

// ==============================================================================
// src/fdc/public.h start
// ==============================================================================

#line 1 "./src/fdc/public.h"
// Public interface for the Floppy Disk Controller (FDC) module.
#ifndef YAX86_FDC_PUBLIC_H
#define YAX86_FDC_PUBLIC_H

// This module emulates the NEC uPD765 Floppy Disk Controller. It handles I/O
// port communication and DMA transfers for floppy operations. Actual disk
// image access is delegated to the platform via callbacks.

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_FDC_BUNDLE_H
#include "../util/static_vector.h"
#endif  // YAX86_FDC_BUNDLE_H

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
  // Maximum size of a command request.
  kFDCCommandBufferSize = 9,
  // Maximum size of a command result.
  kFDCResultBufferSize = 7,
};

// Command phases of the FDC.
typedef enum FDCCommandPhase {
  // No command in progress.
  kFDCPhaseIdle = 0,
  // Command has been issued, waiting for parameters.
  kFDCPhaseCommand,
  // Command parameters received, executing command.
  kFDCPhaseExecution,
  // Command execution complete, sending result bytes.
  kFDCPhaseResult,
} FDCCommandPhase;

// I/O ports for the FDC.
typedef enum FDCPort {
  // Digital Output Register (write-only).
  kFDCPortDOR = 0x3F2,
  // Main Status Register (read-only).
  kFDCPortMSR = 0x3F4,
  // Data Register (read/write).
  kFDCPortData = 0x3F5,
} FDCPort;

// Flags for the Main Status Register (MSR).
enum {
  // Drive 0 is busy with a seek or recalibrate command.
  kFDCMSRDrive0Busy = 1 << 0,
  // Drive 1 is busy with a seek or recalibrate command.
  kFDCMSRDrive1Busy = 1 << 1,
  // Drive 2 is busy with a seek or recalibrate command.
  kFDCMSRDrive2Busy = 1 << 2,
  // Drive 3 is busy with a seek or recalibrate command.
  kFDCMSRDrive3Busy = 1 << 3,
  // A command is in progress.
  kFDCMSRBusy = 1 << 4,
  // The FDC is in non-DMA mode.
  kFDCMSRNonDMAMode = 1 << 5,
  // Indicates direction of data transfer. 0 = write to FDC, 1 = read from FDC.
  kFDCMSRDataDirection = 1 << 6,
  // The Data Register is ready to send or receive data to/from the CPU.
  kFDCMSRRequestForMaster = 1 << 7,
};

// Flags for Status Register 0 (ST0).
enum {
  // Bits 7-6: Interrupt Code
  // 00 = Normal termination
  // 01 = Abnormal termination
  // 10 = Invalid command
  // 11 = Abnormal termination due to polling
  kFDCST0InterruptCodeMask = 0xC0,
  kFDCST0NormalTermination = 0x00,
  kFDCST0AbnormalTermination = 0x40,
  kFDCST0InvalidCommand = 0x80,

  // Bit 5: Seek End
  kFDCST0SeekEnd = 1 << 5,
  // Bit 4: Equipment Check
  kFDCST0EquipmentCheck = 1 << 4,
  // Bit 3: Not Ready
  kFDCST0NotReady = 1 << 3,
  // Bit 2: Head Address
  kFDCST0HeadAddress = 1 << 2,
  // Bits 1-0: Drive Select
  kFDCST0UnitSelectMask = 0x03,
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
  // Whether the drive is currently busy.
  bool busy;
  // Status Register 0 (ST0) for the last completed Seek or Recalibrate
  // operation on this drive.
  uint8_t st0;
  // Whether there is a pending interrupt from a completed Seek or Recalibrate
  // operation on this drive.
  bool has_pending_interrupt;
} FDCDriveState;

// Caller-provided runtime configuration for the FDC.
typedef struct FDCConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Callback to raise an IRQ6 (FDC interrupt) to the CPU.
  void (*raise_irq6)(void* context);

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

STATIC_VECTOR_TYPE(FDCCommandBuffer, uint8_t, kFDCCommandBufferSize)
STATIC_VECTOR_TYPE(FDCResultBuffer, uint8_t, kFDCResultBufferSize)

struct FDCCommandMetadata;

// State of the Floppy Disk Controller.
typedef struct FDCState {
  // Pointer to the FDC configuration.
  FDCConfig* config;

  // Per-drive state.
  FDCDriveState drives[kFDCNumDrives];

  // Current command phase.
  FDCCommandPhase phase;

  // Command buffer to receive command and parameters from the CPU.
  FDCCommandBuffer command_buffer;
  // Metadata for the command currently being processed.
  const struct FDCCommandMetadata* current_command;
  // How many ticks the current command has been executing.
  uint32_t current_command_ticks;

  // Result buffer to send to the CPU.
  FDCResultBuffer result_buffer;
  // Next index to read from result buffer.
  uint8_t next_result_byte_index;
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

// Simulates a tick of the FDC, handling any timed operations.
void FDCTick(FDCState* fdc);

#endif  // YAX86_FDC_PUBLIC_H



// ==============================================================================
// src/fdc/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/fdc/fdc.c start
// ==============================================================================

#line 1 "./src/fdc/fdc.c"
#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

#include <stddef.h>

enum {
  // Invalid offset.
  kFDCInvalidOffset = 0xFFFFFFF,
};

// FDC command opcodes.
typedef enum FDCCommand {
  // Read a Track
  kFDCCmdReadTrack = 0x02,
  // Specify
  kFDCCmdSpecify = 0x03,
  // Sense Drive Status
  kFDCCmdSenseDriveStatus = 0x04,
  // Write Data
  kFDCCmdWriteData = 0x05,
  // Read Data
  kFDCCmdReadData = 0x06,
  // Recalibrate
  kFDCCmdRecalibrate = 0x07,
  // Sense Interrupt Status
  kFDCCmdSenseInterruptStatus = 0x08,
  // Write Deleted Data
  kFDCCmdWriteDeletedData = 0x09,
  // Read ID
  kFDCCmdReadID = 0x0A,
  // Read Deleted Data
  kFDCCmdReadDeletedData = 0x0C,
  // Format a Track
  kFDCCmdFormatTrack = 0x0D,
  // Seek
  kFDCCmdSeek = 0x0F,
  // Scan Equal
  kFDCCmdScanEqual = 0x11,
  // Scan Low or Equal
  kFDCCmdScanLowOrEqual = 0x19,
  // Scan High or Equal
  kFDCCmdScanHighOrEqual = 0x1D,
} FDCCommand;

// Metadata for each FDC command.
typedef struct FDCCommandMetadata {
  // Base opcode of the command (lower 5 bits).
  uint8_t opcode;
  // Number of parameter bytes expected.
  uint8_t num_param_bytes;
  // Handler to execute the command.
  void (*handler)(FDCState* fdc);
} FDCCommandMetadata;

// Helper to raise an IRQ6 if the callback is set.
static inline void FDCRaiseIRQ6(FDCState* fdc) {
  if (fdc->config && fdc->config->raise_irq6) {
    fdc->config->raise_irq6(fdc->config->context);
  }
}

// Transition into execution phase.
static inline void FDCStartCommandExecution(FDCState* fdc) {
  fdc->phase = kFDCPhaseExecution;
  fdc->current_command_ticks = 0;
}

// Transition into result phase after command execution.
static inline void FDCFinishCommandExecution(FDCState* fdc) {
  if (FDCResultBufferLength(&fdc->result_buffer) == 0) {
    // No result bytes to send, go back to idle phase.
    fdc->phase = kFDCPhaseIdle;
  } else {
    // Has result bytes to send, go to result phase.
    fdc->phase = kFDCPhaseResult;
    fdc->next_result_byte_index = 0;
  }
}

// Handler for Recalibrate command.
static void FDCHandleRecalibrate(FDCState* fdc) {
  // Recalibrate command has one parameter byte: drive number (0-3).
  uint8_t drive_index = *FDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
  FDCDriveState* drive = &fdc->drives[drive_index];

  // On initial tick, start seeking.
  if (fdc->current_command_ticks == 0) {
    drive->busy = true;
    return;
  }

  // Seek is complete.
  drive->track = 0;
  drive->busy = false;

  // Set Status Register 0 (ST0)
  // Bits 7-6: Interrupt Code = 00 (Normal Termination)
  // Bit 5: Seek End = 1
  // Bits 1-0: Unit Select (Drive Index)
  drive->st0 = kFDCST0NormalTermination | kFDCST0SeekEnd | drive_index;

  // Set interrupt pending flag for this drive.
  drive->has_pending_interrupt = true;

  // Raise Interrupt (IRQ6).
  FDCRaiseIRQ6(fdc);

  FDCFinishCommandExecution(fdc);
}

// Handler for Sense Interrupt Status command.
static void FDCHandleSenseInterruptStatus(FDCState* fdc) {
  // Check for any pending interrupts.
  for (int i = 0; i < kFDCNumDrives; ++i) {
    FDCDriveState* drive = &fdc->drives[i];
    if (drive->has_pending_interrupt) {
      // Clear the pending interrupt flag.
      drive->has_pending_interrupt = false;

      // Result Byte 0: ST0 (Status Register 0)
      FDCResultBufferAppend(&fdc->result_buffer, &drive->st0);
      // Result Byte 1: PCN (Present Cylinder Number)
      FDCResultBufferAppend(&fdc->result_buffer, &drive->track);

      FDCFinishCommandExecution(fdc);
      return;
    }
  }

  // No pending interrupts found. This is treated as an invalid command.
  uint8_t st0 = kFDCST0InvalidCommand;
  FDCResultBufferAppend(&fdc->result_buffer, &st0);
  FDCFinishCommandExecution(fdc);
}

// List of supported FDC commands.
// The opcodes here represent the base 5-bit command.
static const FDCCommandMetadata kFDCCommandMetadataTable[] = {
    // Read a Track
    {.opcode = kFDCCmdReadTrack, .num_param_bytes = 8, .handler = NULL},
    // Specify
    {.opcode = kFDCCmdSpecify, .num_param_bytes = 2, .handler = NULL},
    // Sense Drive Status
    {.opcode = kFDCCmdSenseDriveStatus, .num_param_bytes = 1, .handler = NULL},
    // Write Data
    {.opcode = kFDCCmdWriteData, .num_param_bytes = 8, .handler = NULL},
    // Read Data
    {.opcode = kFDCCmdReadData, .num_param_bytes = 8, .handler = NULL},
    // Recalibrate
    {.opcode = kFDCCmdRecalibrate,
     .num_param_bytes = 1,
     .handler = FDCHandleRecalibrate},
    // Sense Interrupt Status
    {.opcode = kFDCCmdSenseInterruptStatus,
     .num_param_bytes = 0,
     .handler = FDCHandleSenseInterruptStatus},
    // Write Deleted Data
    {.opcode = kFDCCmdWriteDeletedData, .num_param_bytes = 8, .handler = NULL},
    // Read ID
    {.opcode = kFDCCmdReadID, .num_param_bytes = 1, .handler = NULL},
    // Read Deleted Data
    {.opcode = kFDCCmdReadDeletedData, .num_param_bytes = 8, .handler = NULL},
    // Format a Track
    {.opcode = kFDCCmdFormatTrack, .num_param_bytes = 5, .handler = NULL},
    // Seek
    {.opcode = kFDCCmdSeek, .num_param_bytes = 2, .handler = NULL},
    // Scan Equal
    {.opcode = kFDCCmdScanEqual, .num_param_bytes = 8, .handler = NULL},
    // Scan Low or Equal
    {.opcode = kFDCCmdScanLowOrEqual, .num_param_bytes = 8, .handler = NULL},
    // Scan High or Equal
    {.opcode = kFDCCmdScanHighOrEqual, .num_param_bytes = 8, .handler = NULL},
};

// Compute the byte offset within a disk image for the given address. Returns
// kFDCInvalidOffset if the address is out of range.
//
// In a raw image file, the data is laid out track by track, starting from the
// outermost track (Track 0). Within each track, it reads all the data from the
// first head (Head 0, the top side) and then all the data from the second head
// (Head 1, the bottom side) before moving to the next track.
// In other words, the layout is an array of
// [num_tracks][num_heads][num_sectors_per_track].
static inline uint32_t FDCComputeOffset(
    FDCDiskFormat format, uint8_t head, uint8_t track, uint8_t sector,
    uint16_t sector_offset) {
  if (head >= format.num_heads || track >= format.num_tracks || sector == 0 ||
      sector > format.num_sectors_per_track ||
      sector_offset >= format.sector_size) {
    return kFDCInvalidOffset;
  }

  uint32_t offset = 0;
  // Seek to start of the track
  offset += (uint32_t)track * format.num_heads * format.num_sectors_per_track *
            format.sector_size;
  // Seek to start of the head within the track
  offset += (uint32_t)head * format.num_sectors_per_track * format.sector_size;
  // Seek to start of the sector within the head
  offset += (uint32_t)(sector - 1) * format.sector_size;
  // Add byte offset within the sector
  offset += (uint32_t)sector_offset;

  return offset;
}

void FDCInit(FDCState* fdc, FDCConfig* config) {
  static const FDCState zero_fdc_state = {0};
  *fdc = zero_fdc_state;

  fdc->config = config;
}

uint8_t FDCReadPort(FDCState* fdc, uint16_t port) {
  switch (port) {
    case kFDCPortMSR: {  // Main Status Register (MSR)
      uint8_t msr = 0;
      switch (fdc->phase) {
        case kFDCPhaseIdle:
        case kFDCPhaseCommand:
          // FDC is ready to receive a command or parameter byte.
          msr = kFDCMSRRequestForMaster;
          break;
        case kFDCPhaseResult:
          // FDC has result bytes to send and is still busy with the command.
          msr = kFDCMSRRequestForMaster | kFDCMSRDataDirection | kFDCMSRBusy;
          break;
        case kFDCPhaseExecution:
          // FDC is busy executing a command.
          msr |= kFDCMSRBusy;
          break;
      }
      // Set drive busy flags.
      for (int i = 0; i < kFDCNumDrives; ++i) {
        if (fdc->drives[i].busy) {
          msr |= (1 << i);
        }
      }
      return msr;
    }
    case kFDCPortData: {  // Data Register
      if (fdc->phase != kFDCPhaseResult) {
        return 0xFF;  // Invalid read.
      }
      if (fdc->next_result_byte_index >=
          FDCResultBufferLength(&fdc->result_buffer)) {
        return 0xFF;  // All result bytes have been read.
      }
      uint8_t value =
          *FDCResultBufferGet(&fdc->result_buffer, fdc->next_result_byte_index);
      fdc->next_result_byte_index++;
      if (fdc->next_result_byte_index >=
          FDCResultBufferLength(&fdc->result_buffer)) {
        // Last result byte was read, reset to idle.
        fdc->phase = kFDCPhaseIdle;
        fdc->next_result_byte_index = 0;
        FDCResultBufferClear(&fdc->result_buffer);
      }
      return value;
    }
    default:
      // Per convention for reads from unused/invalid ports.
      return 0xFF;
  }
}

// Looks up command metadata by opcode. Returns NULL if not found. This is a
// linear search, but the command table is small enough that this is fine.
static const FDCCommandMetadata* FDCFindCommandMetadata(uint8_t opcode) {
  for (size_t i = 0;
       i < sizeof(kFDCCommandMetadataTable) / sizeof(FDCCommandMetadata); ++i) {
    if (kFDCCommandMetadataTable[i].opcode == opcode) {
      return &kFDCCommandMetadataTable[i];
    }
  }
  return NULL;
}

static void FDCHandleDataPortWrite(FDCState* fdc, uint8_t value) {
  switch (fdc->phase) {
    case kFDCPhaseIdle: {
      // This is the first byte of a new command.
      // Extract the opcode (lower 5 bits).
      uint8_t opcode = value & 0x1F;
      fdc->current_command = FDCFindCommandMetadata(opcode);

      if (!fdc->current_command) {
        // Invalid command. Set up the result phase with an error.
        FDCResultBufferClear(&fdc->result_buffer);
        uint8_t status = kFDCST0InvalidCommand;
        FDCResultBufferAppend(&fdc->result_buffer, &status);
        FDCFinishCommandExecution(fdc);
        return;
      }

      // Clear previous command and store the first byte.
      FDCCommandBufferClear(&fdc->command_buffer);
      FDCCommandBufferAppend(&fdc->command_buffer, &value);

      if (fdc->current_command->num_param_bytes == 0) {
        // Command has no parameters, move directly to execution.
        FDCStartCommandExecution(fdc);
      } else {
        // Wait for parameter bytes.
        fdc->phase = kFDCPhaseCommand;
      }
      break;
    }

    case kFDCPhaseCommand: {
      // This is a parameter byte for the current command.
      if (!fdc->current_command) {
        // Should not happen, but as a safeguard, reset to idle.
        fdc->phase = kFDCPhaseIdle;
        return;
      }

      // Store the parameter byte.
      FDCCommandBufferAppend(&fdc->command_buffer, &value);

      // Check if all parameters have been received.
      // Total bytes = 1 (command) + num_param_bytes.
      if (FDCCommandBufferLength(&fdc->command_buffer) >=
          (uint8_t)(fdc->current_command->num_param_bytes + 1)) {
        // All bytes received, move to execution phase.
        FDCStartCommandExecution(fdc);
      }
      break;
    }

    case kFDCPhaseExecution:
    case kFDCPhaseResult:
      // The FDC is busy. Ignore any writes to the data port.
      break;
  }
}

void FDCWritePort(FDCState* fdc, uint16_t port, uint8_t value) {
  switch (port) {
    case kFDCPortDOR:  // Digital Output Register
      // TODO: Handle motor control, drive selection, and FDC reset based on
      // 'value'.
      break;

    case kFDCPortData:  // Data Register
      // Logic will be based on the current FDC phase.
      FDCHandleDataPortWrite(fdc, value);
      break;

    default:
      // Ignore writes to other ports.
      break;
  }
}

void FDCInsertDisk(FDCState* fdc, uint8_t drive, const FDCDiskFormat* format) {
  if (drive >= kFDCNumDrives) {
    return;
  }
  FDCDriveState* drive_state = &fdc->drives[drive];
  drive_state->present = true;
  drive_state->format = format;
  drive_state->head = 0;
  drive_state->track = 0;
}

void FDCEjectDisk(FDCState* fdc, uint8_t drive) {
  if (drive >= kFDCNumDrives) {
    return;
  }
  FDCDriveState* drive_state = &fdc->drives[drive];
  drive_state->present = false;
  drive_state->format = NULL;
}

void FDCTick(FDCState* fdc) {
  if (fdc->phase != kFDCPhaseExecution) {
    return;
  }

  // Run the command handler if defined.
  if (fdc->current_command && fdc->current_command->handler) {
    fdc->current_command->handler(fdc);
    ++fdc->current_command_ticks;
  } else {
    // No handler defined, finish execution immediately.
    FDCResultBufferClear(&fdc->result_buffer);
    FDCFinishCommandExecution(fdc);
  }
}


// ==============================================================================
// src/fdc/fdc.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_FDC_BUNDLE_H

