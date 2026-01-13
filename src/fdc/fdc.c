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

// Helper to raise an IRQ6 if the callback is set and interrupts are enabled in
// DOR.
static inline void FDCRaiseIRQ6(FDCState* fdc) {
  if (fdc->config && fdc->config->raise_irq6 &&
      (fdc->dor & kFDCDORInterruptEnable)) {
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

// Helper to perform a seek operation (for Seek and Recalibrate).
static void FDCPerformSeek(
    FDCState* fdc, uint8_t drive_index, uint8_t target_track) {
  FDCDriveState* drive = &fdc->drives[drive_index];

  // On initial tick, start seeking.
  if (fdc->current_command_ticks == 0) {
    drive->busy = true;
    return;
  }

  // On 2nd tick, seek is complete.
  drive->track = target_track;
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

// Helper to finish a read/write command.
static void FDCFinishReadWrite(
    FDCState* fdc, uint8_t st0, uint8_t st1, uint8_t st2) {
  FDCResultBufferAppend(&fdc->result_buffer, &st0);
  FDCResultBufferAppend(&fdc->result_buffer, &st1);
  FDCResultBufferAppend(&fdc->result_buffer, &st2);
  FDCResultBufferAppend(&fdc->result_buffer, &fdc->transfer.cylinder);
  FDCResultBufferAppend(&fdc->result_buffer, &fdc->transfer.head);
  FDCResultBufferAppend(&fdc->result_buffer, &fdc->transfer.sector);
  FDCResultBufferAppend(&fdc->result_buffer, &fdc->transfer.sector_size_code);

  FDCRaiseIRQ6(fdc);
  FDCFinishCommandExecution(fdc);
}

// Handler for Write Data command.
static void FDCHandleWriteData(FDCState* fdc) {
  if (fdc->current_command_ticks == 0) {
    // Initialization.
    uint8_t cmd_byte = *FDCCommandBufferGet(&fdc->command_buffer, 0);
    fdc->transfer.multi_track = (cmd_byte & 0x80) != 0;

    uint8_t drive_head = *FDCCommandBufferGet(&fdc->command_buffer, 1);
    uint8_t drive_index = drive_head & 0x03;
    uint8_t head_address = (drive_head >> 2) & 0x01;

    fdc->transfer.cylinder = *FDCCommandBufferGet(&fdc->command_buffer, 2);
    fdc->transfer.head = *FDCCommandBufferGet(&fdc->command_buffer, 3);
    fdc->transfer.sector = *FDCCommandBufferGet(&fdc->command_buffer, 4);
    fdc->transfer.sector_size_code =
        *FDCCommandBufferGet(&fdc->command_buffer, 5);
    fdc->transfer.eot = *FDCCommandBufferGet(&fdc->command_buffer, 6);
    // GPL and DTL are ignored.

    FDCDriveState* drive = &fdc->drives[drive_index];
    if (!drive->present) {
      // Drive not ready.
      FDCFinishReadWrite(
          fdc,
          kFDCST0AbnormalTermination | kFDCST0NotReady | head_address << 2 |
              drive_index,
          0, 0);
      return;
    }

    // Calculate offset for the first byte.
    fdc->transfer.current_offset = FDCComputeOffset(
        *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
        fdc->transfer.sector, 0);

    if (fdc->transfer.current_offset == kFDCInvalidOffset) {
      FDCFinishReadWrite(
          fdc, kFDCST0AbnormalTermination | head_address << 2 | drive_index,
          kFDCST1NoData, 0);
      return;
    }

    fdc->transfer.sector_byte_index = 0;
    // For Write, we need to request the first byte immediately.
    fdc->transfer.dma_request_active = true;
    fdc->transfer.tc_received = false;
    if (fdc->config && fdc->config->request_dma) {
      fdc->config->request_dma(fdc->config->context);
    }
    return;
  }

  // Execution Loop.

  // Check for Terminal Count (TC).
  if (fdc->transfer.tc_received) {
    // Transfer complete.
    uint8_t drive_index = *FDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
    uint8_t head_address = (fdc->transfer.head & 0x01);
    FDCFinishReadWrite(
        fdc, kFDCST0NormalTermination | head_address << 2 | drive_index, 0, 0);
    return;
  }

  // If DREQ is active, wait for the system to service it (write byte to us).
  if (fdc->transfer.dma_request_active) {
    return;
  }

  // Data has arrived in data_register. Write it to image.
  uint8_t drive_index = *FDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
  if (fdc->config && fdc->config->write_image_byte) {
    fdc->config->write_image_byte(
        fdc->config->context, drive_index, fdc->transfer.current_offset,
        fdc->transfer.data_register);
  }

  // Advance pointers.
  fdc->transfer.current_offset++;
  fdc->transfer.sector_byte_index++;

  // Calculate sector size.
  uint16_t sector_size;
  uint8_t dtl = *FDCCommandBufferGet(&fdc->command_buffer, 8);
  if (fdc->transfer.sector_size_code == 0) {
    sector_size = dtl;
  } else {
    sector_size = 128 << fdc->transfer.sector_size_code;
  }

  // Check for sector boundary.
  if (fdc->transfer.sector_byte_index >= sector_size) {
    // Sector done.
    if (fdc->transfer.sector >= fdc->transfer.eot) {
      // End of Track reached.
      if (fdc->transfer.multi_track && (fdc->transfer.head & 1) == 0) {
        // Multi-Track rollover.
        fdc->transfer.head ^= 1;
        fdc->transfer.sector = 1;
        fdc->transfer.sector_byte_index = 0;

        // Recompute offset.
        FDCDriveState* drive = &fdc->drives[drive_index];
        fdc->transfer.current_offset = FDCComputeOffset(
            *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
            fdc->transfer.sector, 0);

        if (fdc->transfer.current_offset == kFDCInvalidOffset) {
          fdc->transfer.tc_received = true;
          return;  // Stop here, don't request next byte.
        }
      } else {
        // Terminate.
        fdc->transfer.sector++;
        fdc->transfer.tc_received = true;
        return;  // Stop here.
      }
    } else {
      // Next sector.
      fdc->transfer.sector++;
      fdc->transfer.sector_byte_index = 0;
      FDCDriveState* drive = &fdc->drives[drive_index];
      fdc->transfer.current_offset = FDCComputeOffset(
          *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
          fdc->transfer.sector, 0);

      if (fdc->transfer.current_offset == kFDCInvalidOffset) {
        fdc->transfer.tc_received = true;
        return;
      }
    }
  }

  // Request next byte via DMA.
  fdc->transfer.dma_request_active = true;
  if (fdc->config && fdc->config->request_dma) {
    fdc->config->request_dma(fdc->config->context);
  }
}

// Handler for Read Data command.
static void FDCHandleReadData(FDCState* fdc) {
  if (fdc->current_command_ticks == 0) {
    // Initialization.
    uint8_t cmd_byte = *FDCCommandBufferGet(&fdc->command_buffer, 0);
    fdc->transfer.multi_track = (cmd_byte & 0x80) != 0;

    uint8_t drive_head = *FDCCommandBufferGet(&fdc->command_buffer, 1);
    uint8_t drive_index = drive_head & 0x03;
    uint8_t head_address = (drive_head >> 2) & 0x01;

    fdc->transfer.cylinder = *FDCCommandBufferGet(&fdc->command_buffer, 2);
    fdc->transfer.head = *FDCCommandBufferGet(&fdc->command_buffer, 3);
    fdc->transfer.sector = *FDCCommandBufferGet(&fdc->command_buffer, 4);
    fdc->transfer.sector_size_code =
        *FDCCommandBufferGet(&fdc->command_buffer, 5);
    fdc->transfer.eot = *FDCCommandBufferGet(&fdc->command_buffer, 6);
    // GPL and DTL are ignored as we don't simulate gap timings or use DTL for
    // init.

    FDCDriveState* drive = &fdc->drives[drive_index];
    if (!drive->present) {
      // Drive not ready.
      FDCFinishReadWrite(
          fdc,
          kFDCST0AbnormalTermination | kFDCST0NotReady | head_address << 2 |
              drive_index,
          0, 0);
      return;
    }

    // Calculate offset for the first byte.
    fdc->transfer.current_offset = FDCComputeOffset(
        *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
        fdc->transfer.sector, 0);

    if (fdc->transfer.current_offset == kFDCInvalidOffset) {
      // Invalid sector/track.
      // If the sector is physically out of bounds for the format, report No
      // Data (Sector Not Found).
      FDCFinishReadWrite(
          fdc, kFDCST0AbnormalTermination | head_address << 2 | drive_index,
          kFDCST1NoData, 0);
      return;
    }

    fdc->transfer.sector_byte_index = 0;
    fdc->transfer.dma_request_active = false;
    fdc->transfer.tc_received = false;
    return;
  }

  // Execution Loop.

  // Check for Terminal Count (TC).
  if (fdc->transfer.tc_received) {
    // Transfer complete.
    uint8_t drive_index = *FDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
    uint8_t head_address = (fdc->transfer.head & 0x01);
    FDCFinishReadWrite(
        fdc, kFDCST0NormalTermination | head_address << 2 | drive_index, 0, 0);
    return;
  }

  // If DREQ is active, wait for the system to service it.
  if (fdc->transfer.dma_request_active) {
    return;
  }

  // Read next byte.
  uint8_t drive_index = *FDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
  if (fdc->config && fdc->config->read_image_byte) {
    fdc->transfer.data_register = fdc->config->read_image_byte(
        fdc->config->context, drive_index, fdc->transfer.current_offset);
  } else {
    fdc->transfer.data_register = 0;
  }

  // Advance pointers.
  fdc->transfer.current_offset++;
  fdc->transfer.sector_byte_index++;

  // Request DMA transfer.
  fdc->transfer.dma_request_active = true;
  if (fdc->config && fdc->config->request_dma) {
    fdc->config->request_dma(fdc->config->context);
  }

  // Calculate sector size again for boundary check.
  uint16_t sector_size;
  uint8_t dtl = *FDCCommandBufferGet(&fdc->command_buffer, 8);
  if (fdc->transfer.sector_size_code == 0) {
    sector_size = dtl;
  } else {
    sector_size = 128 << fdc->transfer.sector_size_code;
  }

  // Check for sector boundary.
  if (fdc->transfer.sector_byte_index >= sector_size) {
    // Sector done.
    if (fdc->transfer.sector >= fdc->transfer.eot) {
      // End of Track reached.
      if (fdc->transfer.multi_track && (fdc->transfer.head & 1) == 0) {
        // Multi-Track rollover: Side 0 -> Side 1.
        fdc->transfer.head ^= 1;   // Flip to Head 1
        fdc->transfer.sector = 1;  // Reset to Sector 1
        fdc->transfer.sector_byte_index = 0;

        // Recompute offset for new head/sector.
        FDCDriveState* drive = &fdc->drives[drive_index];
        fdc->transfer.current_offset = FDCComputeOffset(
            *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
            fdc->transfer.sector, 0);

        if (fdc->transfer.current_offset == kFDCInvalidOffset) {
          fdc->transfer.tc_received = true;
        }
      } else {
        // Standard termination (MT=0 or already on Head 1).
        // Increment sector so result phase reports the *next* logical sector.
        fdc->transfer.sector++;
        fdc->transfer.tc_received = true;
      }
    } else {
      // Move to next sector.
      fdc->transfer.sector++;
      fdc->transfer.sector_byte_index = 0;
      // Recompute offset for new sector.
      FDCDriveState* drive = &fdc->drives[drive_index];
      fdc->transfer.current_offset = FDCComputeOffset(
          *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
          fdc->transfer.sector, 0);

      if (fdc->transfer.current_offset == kFDCInvalidOffset) {
        // Should not happen if EOT is correct, but if we ran off the end
        // of the image despite EOT, terminate.
        fdc->transfer.tc_received = true;
      }
    }
  }
}

// Handler for Recalibrate command.
static void FDCHandleRecalibrate(FDCState* fdc) {
  // Recalibrate command has one parameter byte: drive number (0-3).
  uint8_t drive_index = *FDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
  FDCPerformSeek(fdc, drive_index, 0);
}

// Handler for Seek command.
static void FDCHandleSeek(FDCState* fdc) {
  // Seek command parameters:
  // Byte 1: Drive number (0-3) and Head address (ignored for seek).
  // Byte 2: New Cylinder Number (NCN).
  uint8_t drive_index = *FDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
  uint8_t target_track = *FDCCommandBufferGet(&fdc->command_buffer, 2);
  FDCPerformSeek(fdc, drive_index, target_track);
}

// Handler for Specify command.
static void FDCHandleSpecify(FDCState* fdc) {
  // We don't currently support changing timings or non-DMA mode, so we just
  // ignore the parameters.
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
    {.opcode = kFDCCmdSpecify,
     .num_param_bytes = 2,
     .handler = FDCHandleSpecify},
    // Sense Drive Status
    {.opcode = kFDCCmdSenseDriveStatus, .num_param_bytes = 1, .handler = NULL},
    // Write Data
    {.opcode = kFDCCmdWriteData,
     .num_param_bytes = 8,
     .handler = FDCHandleWriteData},
    // Read Data
    {.opcode = kFDCCmdReadData,
     .num_param_bytes = 8,
     .handler = FDCHandleReadData},
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
    {.opcode = kFDCCmdSeek, .num_param_bytes = 2, .handler = FDCHandleSeek},
    // Scan Equal
    {.opcode = kFDCCmdScanEqual, .num_param_bytes = 8, .handler = NULL},
    // Scan Low or Equal
    {.opcode = kFDCCmdScanLowOrEqual, .num_param_bytes = 8, .handler = NULL},
    // Scan High or Equal
    {.opcode = kFDCCmdScanHighOrEqual, .num_param_bytes = 8, .handler = NULL},
};

void FDCInit(FDCState* fdc, FDCConfig* config) {
  static const FDCState zero_fdc_state = {0};
  *fdc = zero_fdc_state;

  fdc->config = config;
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

static uint8_t FDCReadMSRPort(FDCState* fdc) {
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

static uint8_t FDCReadDataPort(FDCState* fdc) {
  switch (fdc->phase) {
    case kFDCPhaseExecution:
      // DMA or Polling read during execution.
      fdc->transfer.dma_request_active = false;
      return fdc->transfer.data_register;

    case kFDCPhaseResult: {
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
      return 0xFF;  // Invalid read.
  }
}

uint8_t FDCReadPort(FDCState* fdc, uint16_t port) {
  switch (port) {
    case kFDCPortMSR:  // Main Status Register (MSR)
      return FDCReadMSRPort(fdc);
    case kFDCPortData:  // Data Register
      return FDCReadDataPort(fdc);
    default:
      // Per convention for reads from unused/invalid ports.
      return 0xFF;
  }
}

static void FDCWriteDORPort(FDCState* fdc, uint8_t value) {
  uint8_t old_dor = fdc->dor;
  fdc->dor = value;

  bool old_reset_bit = (old_dor & kFDCDORReset) != 0;
  bool new_reset_bit = (value & kFDCDORReset) != 0;

  if (!new_reset_bit && old_reset_bit) {
    // Entering reset state (1 -> 0).
    fdc->phase = kFDCPhaseIdle;
    FDCCommandBufferClear(&fdc->command_buffer);
    FDCResultBufferClear(&fdc->result_buffer);
    for (int i = 0; i < kFDCNumDrives; ++i) {
      fdc->drives[i].busy = false;
      fdc->drives[i].has_pending_interrupt = false;
    }
  } else if (new_reset_bit && !old_reset_bit) {
    // Exiting reset state (0 -> 1).
    // The FDC generates an interrupt and sets up status for Sense
    // Interrupt Status for all drives.
    for (int i = 0; i < kFDCNumDrives; ++i) {
      fdc->drives[i].has_pending_interrupt = true;
      fdc->drives[i].st0 = kFDCST0AbnormalTerminationPolling | (uint8_t)i;
    }
    FDCRaiseIRQ6(fdc);
  }
}

static void FDCWriteDataPort(FDCState* fdc, uint8_t value) {
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
      // DMA or Polling write during execution.
      fdc->transfer.data_register = value;
      fdc->transfer.dma_request_active = false;
      break;

    case kFDCPhaseResult:
      // The FDC is busy. Ignore any writes to the data port.
      break;
  }
}

void FDCWritePort(FDCState* fdc, uint16_t port, uint8_t value) {
  switch (port) {
    case kFDCPortDOR:  // Digital Output Register
      FDCWriteDORPort(fdc, value);
      break;

    case kFDCPortData:  // Data Register
      // Logic will be based on the current FDC phase.
      FDCWriteDataPort(fdc, value);
      break;

    default:
      // Ignore writes to other ports.
      break;
  }
}

void FDCHandleTC(FDCState* fdc) { fdc->transfer.tc_received = true; }

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
