#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

#define Y_FDC_ERROR(...) \
  Y_LOG(fdc->config.logger, &YFDCLogModule, Y_LOG_LEVEL_ERROR, __VA_ARGS__)
#define Y_FDC_WARN(...) \
  Y_LOG(fdc->config.logger, &YFDCLogModule, Y_LOG_LEVEL_WARN, __VA_ARGS__)
#define Y_FDC_INFO(...) \
  Y_LOG(fdc->config.logger, &YFDCLogModule, Y_LOG_LEVEL_INFO, __VA_ARGS__)
#define Y_FDC_DEBUG(...) \
  Y_LOG(fdc->config.logger, &YFDCLogModule, Y_LOG_LEVEL_DEBUG, __VA_ARGS__)

#include <stddef.h>

enum {
  // Invalid offset.
  Y_FDC_INVALID_OFFSET = 0xFFFFFFF,
};

// FDC command opcodes.
typedef enum YFDCCommand {
  // Read a Track
  Y_FDC_CMD_READ_TRACK = 0x02,
  // Specify
  Y_FDC_CMD_SPECIFY = 0x03,
  // Sense Drive Status
  Y_FDC_CMD_SENSE_DRIVE_STATUS = 0x04,
  // Write Data
  Y_FDC_CMD_WRITE_DATA = 0x05,
  // Read Data
  Y_FDC_CMD_READ_DATA = 0x06,
  // Recalibrate
  Y_FDC_CMD_RECALIBRATE = 0x07,
  // Sense Interrupt Status
  Y_FDC_CMD_SENSE_INTERRUPT_STATUS = 0x08,
  // Write Deleted Data
  Y_FDC_CMD_WRITE_DELETED_DATA = 0x09,
  // Read ID
  Y_FDC_CMD_READ_ID = 0x0A,
  // Read Deleted Data
  Y_FDC_CMD_READ_DELETED_DATA = 0x0C,
  // Format a Track
  Y_FDC_CMD_FORMAT_TRACK = 0x0D,
  // Seek
  Y_FDC_CMD_SEEK = 0x0F,
  // Scan Equal
  Y_FDC_CMD_SCAN_EQUAL = 0x11,
  // Scan Low or Equal
  Y_FDC_CMD_SCAN_LOW_OR_EQUAL = 0x19,
  // Scan High or Equal
  Y_FDC_CMD_SCAN_HIGH_OR_EQUAL = 0x1D,
} YFDCCommand;

// Metadata for each FDC command.
typedef struct YFDCCommandMetadata {
  // Base opcode of the command (lower 5 bits).
  uint8_t opcode;
  // Number of parameter bytes expected.
  uint8_t num_param_bytes;
  // Handler to execute the command.
  void (*handler)(YFDCState* fdc);
} YFDCCommandMetadata;

// Helper to raise an IRQ6 if the callback is set and interrupts are enabled in
// DOR.
Y_FILE_PRIVATE inline void YFDCRaiseIRQ6(YFDCState* fdc) {
  if (fdc->config.raise_irq6 && (fdc->dor & Y_FDC_DOR_INTERRUPT_ENABLE)) {
    fdc->config.raise_irq6(fdc->config.context);
  }
}

// Transition into execution phase.
Y_FILE_PRIVATE inline void YFDCStartCommandExecution(YFDCState* fdc) {
  Y_FDC_DEBUG(
      "command_started",
      Y_LOG_HEX8(
          "command", fdc->current_command ? fdc->current_command->opcode : 0),
      Y_LOG_UINT(
          "parameter_bytes",
          YFDCCommandBufferLength(&fdc->command_buffer) - 1));
  fdc->phase = Y_FDC_PHASE_EXECUTION;
  fdc->current_command_ticks = 0;
}

// Transition into result phase after command execution.
Y_FILE_PRIVATE inline void YFDCFinishCommandExecution(YFDCState* fdc) {
  Y_FDC_DEBUG(
      "command_finished",
      Y_LOG_UINT("result_bytes", YFDCResultBufferLength(&fdc->result_buffer)),
      Y_LOG_HEX8(
          "st0", YFDCResultBufferLength(&fdc->result_buffer) > 0
                     ? *YFDCResultBufferGet(&fdc->result_buffer, 0)
                     : 0));
  if (YFDCResultBufferLength(&fdc->result_buffer) == 0) {
    // No result bytes to send, go back to idle phase.
    fdc->phase = Y_FDC_PHASE_IDLE;
  } else {
    // Has result bytes to send, go to result phase.
    fdc->phase = Y_FDC_PHASE_RESULT;
    fdc->next_result_byte_index = 0;
  }
}

// Helper to perform a seek operation (for Seek and Recalibrate).
Y_FILE_PRIVATE void YFDCPerformSeek(
    YFDCState* fdc, uint8_t drive_index, uint8_t target_track) {
  YFDCDriveState* drive = &fdc->drives[drive_index];

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
  drive->st0 = Y_FDC_ST0_NORMAL_TERMINATION | Y_FDC_ST0_SEEK_END | drive_index;

  // Set interrupt pending flag for this drive.
  drive->has_pending_interrupt = true;

  // Raise Interrupt (IRQ6).
  YFDCRaiseIRQ6(fdc);

  YFDCFinishCommandExecution(fdc);
}

// Compute the byte offset within a disk image for the given address. Returns
// Y_FDC_INVALID_OFFSET if the address is out of range.
//
// In a raw image file, the data is laid out track by track, starting from the
// outermost track (Track 0). Within each track, it reads all the data from the
// first head (Head 0, the top side) and then all the data from the second head
// (Head 1, the bottom side) before moving to the next track.
// In other words, the layout is an array of
// [num_tracks][num_heads][num_sectors_per_track].
Y_FILE_PRIVATE inline uint32_t YFDCComputeOffset(
    YFDCDiskFormat format, uint8_t head, uint8_t track, uint8_t sector,
    uint16_t sector_offset) {
  if (head >= format.num_heads || track >= format.num_tracks || sector == 0 ||
      sector > format.num_sectors_per_track ||
      sector_offset >= format.sector_size) {
    return Y_FDC_INVALID_OFFSET;
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
Y_FILE_PRIVATE void YFDCFinishReadWrite(
    YFDCState* fdc, uint8_t st0, uint8_t st1, uint8_t st2) {
  YFDCResultBufferAppend(&fdc->result_buffer, &st0);
  YFDCResultBufferAppend(&fdc->result_buffer, &st1);
  YFDCResultBufferAppend(&fdc->result_buffer, &st2);
  YFDCResultBufferAppend(&fdc->result_buffer, &fdc->transfer.cylinder);
  YFDCResultBufferAppend(&fdc->result_buffer, &fdc->transfer.head);
  YFDCResultBufferAppend(&fdc->result_buffer, &fdc->transfer.sector);
  YFDCResultBufferAppend(&fdc->result_buffer, &fdc->transfer.sector_size_code);

  YFDCRaiseIRQ6(fdc);
  YFDCFinishCommandExecution(fdc);
}

// Handler for Write Data command.
Y_FILE_PRIVATE void YFDCHandleWriteData(YFDCState* fdc) {
  if (fdc->current_command_ticks == 0) {
    // Initialization.
    uint8_t cmd_byte = *YFDCCommandBufferGet(&fdc->command_buffer, 0);
    fdc->transfer.multi_track = (cmd_byte & 0x80) != 0;

    uint8_t drive_head = *YFDCCommandBufferGet(&fdc->command_buffer, 1);
    uint8_t drive_index = drive_head & 0x03;
    uint8_t head_address = (drive_head >> 2) & 0x01;

    fdc->transfer.cylinder = *YFDCCommandBufferGet(&fdc->command_buffer, 2);
    fdc->transfer.head = *YFDCCommandBufferGet(&fdc->command_buffer, 3);
    fdc->transfer.sector = *YFDCCommandBufferGet(&fdc->command_buffer, 4);
    fdc->transfer.sector_size_code =
        *YFDCCommandBufferGet(&fdc->command_buffer, 5);
    fdc->transfer.eot = *YFDCCommandBufferGet(&fdc->command_buffer, 6);
    // GPL and DTL are ignored.

    YFDCDriveState* drive = &fdc->drives[drive_index];
    if (!drive->present) {
      // Drive not ready.
      YFDCFinishReadWrite(
          fdc,
          Y_FDC_ST0_ABNORMAL_TERMINATION | Y_FDC_ST0_NOT_READY |
              head_address << 2 | drive_index,
          0, 0);
      return;
    }

    // Calculate offset for the first byte.
    fdc->transfer.current_offset = YFDCComputeOffset(
        *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
        fdc->transfer.sector, 0);

    if (fdc->transfer.current_offset == Y_FDC_INVALID_OFFSET) {
      YFDCFinishReadWrite(
          fdc, Y_FDC_ST0_ABNORMAL_TERMINATION | head_address << 2 | drive_index,
          Y_FDC_ST1_NO_DATA, 0);
      return;
    }

    fdc->transfer.sector_byte_index = 0;
    // For Write, we need to request the first byte immediately.
    fdc->transfer.dma_request_active = true;
    fdc->transfer.tc_received = false;
    if (fdc->config.request_dma) {
      fdc->config.request_dma(fdc->config.context);
    }
    return;
  }

  // Execution Loop.

  // Check for Terminal Count (TC).
  if (fdc->transfer.tc_received) {
    // Transfer complete.
    uint8_t drive_index = *YFDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
    uint8_t head_address = (fdc->transfer.head & 0x01);
    YFDCFinishReadWrite(
        fdc, Y_FDC_ST0_NORMAL_TERMINATION | head_address << 2 | drive_index, 0,
        0);
    return;
  }

  // If DREQ is active, wait for the system to service it (write byte to us).
  if (fdc->transfer.dma_request_active) {
    return;
  }

  // Data has arrived in data_register. Write it to image.
  uint8_t drive_index = *YFDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
  if (fdc->config.write_image_byte) {
    fdc->config.write_image_byte(
        fdc->config.context, drive_index, fdc->transfer.current_offset,
        fdc->transfer.data_register);
  }

  // Advance pointers.
  fdc->transfer.current_offset++;
  fdc->transfer.sector_byte_index++;

  // Calculate sector size.
  uint16_t sector_size;
  uint8_t dtl = *YFDCCommandBufferGet(&fdc->command_buffer, 8);
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
        YFDCDriveState* drive = &fdc->drives[drive_index];
        fdc->transfer.current_offset = YFDCComputeOffset(
            *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
            fdc->transfer.sector, 0);

        if (fdc->transfer.current_offset == Y_FDC_INVALID_OFFSET) {
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
      YFDCDriveState* drive = &fdc->drives[drive_index];
      fdc->transfer.current_offset = YFDCComputeOffset(
          *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
          fdc->transfer.sector, 0);

      if (fdc->transfer.current_offset == Y_FDC_INVALID_OFFSET) {
        fdc->transfer.tc_received = true;
        return;
      }
    }
  }

  // Request next byte via DMA.
  fdc->transfer.dma_request_active = true;
  if (fdc->config.request_dma) {
    fdc->config.request_dma(fdc->config.context);
  }
}

// Handler for Read Data command.
Y_FILE_PRIVATE Y_HOT void YFDCHandleReadData(YFDCState* fdc) {
  if (fdc->current_command_ticks == 0) {
    // Initialization.
    uint8_t cmd_byte = *YFDCCommandBufferGet(&fdc->command_buffer, 0);
    fdc->transfer.multi_track = (cmd_byte & 0x80) != 0;

    uint8_t drive_head = *YFDCCommandBufferGet(&fdc->command_buffer, 1);
    uint8_t drive_index = drive_head & 0x03;
    uint8_t head_address = (drive_head >> 2) & 0x01;

    fdc->transfer.cylinder = *YFDCCommandBufferGet(&fdc->command_buffer, 2);
    fdc->transfer.head = *YFDCCommandBufferGet(&fdc->command_buffer, 3);
    fdc->transfer.sector = *YFDCCommandBufferGet(&fdc->command_buffer, 4);
    fdc->transfer.sector_size_code =
        *YFDCCommandBufferGet(&fdc->command_buffer, 5);
    fdc->transfer.eot = *YFDCCommandBufferGet(&fdc->command_buffer, 6);
    // GPL and DTL are ignored as we don't simulate gap timings or use DTL for
    // init.

    YFDCDriveState* drive = &fdc->drives[drive_index];
    if (!drive->present) {
      // Drive not ready.
      YFDCFinishReadWrite(
          fdc,
          Y_FDC_ST0_ABNORMAL_TERMINATION | Y_FDC_ST0_NOT_READY |
              head_address << 2 | drive_index,
          0, 0);
      return;
    }

    // Calculate offset for the first byte.
    fdc->transfer.current_offset = YFDCComputeOffset(
        *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
        fdc->transfer.sector, 0);

    if (fdc->transfer.current_offset == Y_FDC_INVALID_OFFSET) {
      // Invalid sector/track.
      // If the sector is physically out of bounds for the format, report No
      // Data (Sector Not Found).
      YFDCFinishReadWrite(
          fdc, Y_FDC_ST0_ABNORMAL_TERMINATION | head_address << 2 | drive_index,
          Y_FDC_ST1_NO_DATA, 0);
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
    uint8_t drive_index = *YFDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
    uint8_t head_address = (fdc->transfer.head & 0x01);
    YFDCFinishReadWrite(
        fdc, Y_FDC_ST0_NORMAL_TERMINATION | head_address << 2 | drive_index, 0,
        0);
    return;
  }

  // If DREQ is active, wait for the system to service it.
  if (fdc->transfer.dma_request_active) {
    return;
  }

  // Read next byte.
  uint8_t drive_index = *YFDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
  if (fdc->config.read_image_byte) {
    fdc->transfer.data_register = fdc->config.read_image_byte(
        fdc->config.context, drive_index, fdc->transfer.current_offset);
  } else {
    fdc->transfer.data_register = 0;
  }

  // Advance pointers.
  fdc->transfer.current_offset++;
  fdc->transfer.sector_byte_index++;

  // Request DMA transfer.
  fdc->transfer.dma_request_active = true;
  if (fdc->config.request_dma) {
    fdc->config.request_dma(fdc->config.context);
  }

  // Calculate sector size again for boundary check.
  uint16_t sector_size;
  uint8_t dtl = *YFDCCommandBufferGet(&fdc->command_buffer, 8);
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
        YFDCDriveState* drive = &fdc->drives[drive_index];
        fdc->transfer.current_offset = YFDCComputeOffset(
            *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
            fdc->transfer.sector, 0);

        if (fdc->transfer.current_offset == Y_FDC_INVALID_OFFSET) {
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
      YFDCDriveState* drive = &fdc->drives[drive_index];
      fdc->transfer.current_offset = YFDCComputeOffset(
          *drive->format, fdc->transfer.head, fdc->transfer.cylinder,
          fdc->transfer.sector, 0);

      if (fdc->transfer.current_offset == Y_FDC_INVALID_OFFSET) {
        // Should not happen if EOT is correct, but if we ran off the end
        // of the image despite EOT, terminate.
        fdc->transfer.tc_received = true;
      }
    }
  }
}

// Handler for Recalibrate command.
Y_FILE_PRIVATE void YFDCHandleRecalibrate(YFDCState* fdc) {
  // Recalibrate command has one parameter byte: drive number (0-3).
  uint8_t drive_index = *YFDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
  YFDCPerformSeek(fdc, drive_index, 0);
}

// Handler for Seek command.
Y_FILE_PRIVATE void YFDCHandleSeek(YFDCState* fdc) {
  // Seek command parameters:
  // Byte 1: Drive number (0-3) and Head address (ignored for seek).
  // Byte 2: New Cylinder Number (NCN).
  uint8_t drive_index = *YFDCCommandBufferGet(&fdc->command_buffer, 1) & 0x03;
  uint8_t target_track = *YFDCCommandBufferGet(&fdc->command_buffer, 2);
  YFDCPerformSeek(fdc, drive_index, target_track);
}

// Handler for Specify command.
Y_FILE_PRIVATE void YFDCHandleSpecify(YFDCState* fdc) {
  // We don't currently support changing timings or non-DMA mode, so we just
  // ignore the parameters.
  YFDCFinishCommandExecution(fdc);
}

// Handler for Sense Interrupt Status command.
Y_FILE_PRIVATE void YFDCHandleSenseInterruptStatus(YFDCState* fdc) {
  // Check for any pending interrupts.
  for (int i = 0; i < Y_FDC_NUM_DRIVES; ++i) {
    YFDCDriveState* drive = &fdc->drives[i];
    if (drive->has_pending_interrupt) {
      // Clear the pending interrupt flag.
      drive->has_pending_interrupt = false;

      // Result Byte 0: ST0 (Status Register 0)
      YFDCResultBufferAppend(&fdc->result_buffer, &drive->st0);
      // Result Byte 1: PCN (Present Cylinder Number)
      YFDCResultBufferAppend(&fdc->result_buffer, &drive->track);

      YFDCFinishCommandExecution(fdc);
      return;
    }
  }

  // No pending interrupts found. This is treated as an invalid command.
  uint8_t st0 = Y_FDC_ST0_INVALID_COMMAND;
  YFDCResultBufferAppend(&fdc->result_buffer, &st0);
  YFDCFinishCommandExecution(fdc);
}

// List of supported FDC commands.
// The opcodes here represent the base 5-bit command.
Y_FILE_PRIVATE const YFDCCommandMetadata YFDCCommandMetadataTable[] = {
    // Read a Track
    {.opcode = Y_FDC_CMD_READ_TRACK, .num_param_bytes = 8, .handler = NULL},
    // Specify
    {.opcode = Y_FDC_CMD_SPECIFY,
     .num_param_bytes = 2,
     .handler = YFDCHandleSpecify},
    // Sense Drive Status
    {.opcode = Y_FDC_CMD_SENSE_DRIVE_STATUS,
     .num_param_bytes = 1,
     .handler = NULL},
    // Write Data
    {.opcode = Y_FDC_CMD_WRITE_DATA,
     .num_param_bytes = 8,
     .handler = YFDCHandleWriteData},
    // Read Data
    {.opcode = Y_FDC_CMD_READ_DATA,
     .num_param_bytes = 8,
     .handler = YFDCHandleReadData},
    // Recalibrate
    {.opcode = Y_FDC_CMD_RECALIBRATE,
     .num_param_bytes = 1,
     .handler = YFDCHandleRecalibrate},
    // Sense Interrupt Status
    {.opcode = Y_FDC_CMD_SENSE_INTERRUPT_STATUS,
     .num_param_bytes = 0,
     .handler = YFDCHandleSenseInterruptStatus},
    // Write Deleted Data
    {.opcode = Y_FDC_CMD_WRITE_DELETED_DATA,
     .num_param_bytes = 8,
     .handler = NULL},
    // Read ID
    {.opcode = Y_FDC_CMD_READ_ID, .num_param_bytes = 1, .handler = NULL},
    // Read Deleted Data
    {.opcode = Y_FDC_CMD_READ_DELETED_DATA,
     .num_param_bytes = 8,
     .handler = NULL},
    // Format a Track
    {.opcode = Y_FDC_CMD_FORMAT_TRACK, .num_param_bytes = 5, .handler = NULL},
    // Seek
    {.opcode = Y_FDC_CMD_SEEK, .num_param_bytes = 2, .handler = YFDCHandleSeek},
    // Scan Equal
    {.opcode = Y_FDC_CMD_SCAN_EQUAL, .num_param_bytes = 8, .handler = NULL},
    // Scan Low or Equal
    {.opcode = Y_FDC_CMD_SCAN_LOW_OR_EQUAL,
     .num_param_bytes = 8,
     .handler = NULL},
    // Scan High or Equal
    {.opcode = Y_FDC_CMD_SCAN_HIGH_OR_EQUAL,
     .num_param_bytes = 8,
     .handler = NULL},
};

// Nothing to do: an FDC powers on with every field at zero, and the caller has
// already zeroed the state. Kept so that every module is brought up the same
// way, and so that a non-zero default acquired later has somewhere to go.
Y_PUBLIC void YFDCInit(Y_UNUSED YFDCState* fdc) {}

// Looks up command metadata by opcode. Returns NULL if not found. This is a
// linear search, but the command table is small enough that this is fine.
Y_FILE_PRIVATE const YFDCCommandMetadata* YFDCFindCommandMetadata(
    uint8_t opcode) {
  for (size_t i = 0;
       i < sizeof(YFDCCommandMetadataTable) / sizeof(YFDCCommandMetadata);
       ++i) {
    if (YFDCCommandMetadataTable[i].opcode == opcode) {
      return &YFDCCommandMetadataTable[i];
    }
  }
  return NULL;
}

Y_FILE_PRIVATE uint8_t YFDCReadMSRPort(YFDCState* fdc) {
  uint8_t msr = 0;
  switch (fdc->phase) {
    case Y_FDC_PHASE_IDLE:
    case Y_FDC_PHASE_COMMAND:
      // FDC is ready to receive a command or parameter byte.
      msr = Y_FDC_MSR_REQUEST_FOR_MASTER;
      break;
    case Y_FDC_PHASE_RESULT:
      // FDC has result bytes to send and is still busy with the command.
      msr = Y_FDC_MSR_REQUEST_FOR_MASTER | Y_FDC_MSR_DATA_DIRECTION |
            Y_FDC_MSR_BUSY;
      break;
    case Y_FDC_PHASE_EXECUTION:
      // FDC is busy executing a command.
      msr |= Y_FDC_MSR_BUSY;
      break;
  }
  // Set drive busy flags.
  for (int i = 0; i < Y_FDC_NUM_DRIVES; ++i) {
    if (fdc->drives[i].busy) {
      msr |= (1 << i);
    }
  }
  return msr;
}

Y_FILE_PRIVATE uint8_t YFDCReadDataPort(YFDCState* fdc) {
  switch (fdc->phase) {
    case Y_FDC_PHASE_EXECUTION:
      // DMA or Polling read during execution.
      fdc->transfer.dma_request_active = false;
      return fdc->transfer.data_register;

    case Y_FDC_PHASE_RESULT: {
      if (fdc->next_result_byte_index >=
          YFDCResultBufferLength(&fdc->result_buffer)) {
        return 0xFF;  // All result bytes have been read.
      }
      uint8_t value = *YFDCResultBufferGet(
          &fdc->result_buffer, fdc->next_result_byte_index);
      fdc->next_result_byte_index++;
      if (fdc->next_result_byte_index >=
          YFDCResultBufferLength(&fdc->result_buffer)) {
        // Last result byte was read, reset to idle.
        fdc->phase = Y_FDC_PHASE_IDLE;
        fdc->next_result_byte_index = 0;
        YFDCResultBufferClear(&fdc->result_buffer);
      }
      return value;
    }

    default:
      return 0xFF;  // Invalid read.
  }
}

Y_PUBLIC uint8_t YFDCReadPort(YFDCState* fdc, uint16_t port) {
  switch (port) {
    case Y_FDC_PORT_MSR:  // Main Status Register (MSR)
      return YFDCReadMSRPort(fdc);
    case Y_FDC_PORT_DATA:  // Data Register
      return YFDCReadDataPort(fdc);
    default:
      // Per convention for reads from unused/invalid ports.
      return 0xFF;
  }
}

Y_FILE_PRIVATE void YFDCWriteDORPort(YFDCState* fdc, uint8_t value) {
  uint8_t old_dor = fdc->dor;
  fdc->dor = value;

  bool old_reset_bit = (old_dor & Y_FDC_DOR_RESET) != 0;
  bool new_reset_bit = (value & Y_FDC_DOR_RESET) != 0;

  if (!new_reset_bit && old_reset_bit) {
    // Entering reset state (1 -> 0).
    fdc->phase = Y_FDC_PHASE_IDLE;
    YFDCCommandBufferClear(&fdc->command_buffer);
    YFDCResultBufferClear(&fdc->result_buffer);
    for (int i = 0; i < Y_FDC_NUM_DRIVES; ++i) {
      fdc->drives[i].busy = false;
      fdc->drives[i].has_pending_interrupt = false;
    }
  } else if (new_reset_bit && !old_reset_bit) {
    // Exiting reset state (0 -> 1).
    // The FDC generates an interrupt and sets up status for Sense
    // Interrupt Status for all drives.
    for (int i = 0; i < Y_FDC_NUM_DRIVES; ++i) {
      fdc->drives[i].has_pending_interrupt = true;
      fdc->drives[i].st0 = Y_FDC_ST0_ABNORMAL_TERMINATION_POLLING | (uint8_t)i;
    }
    YFDCRaiseIRQ6(fdc);
  }
}

Y_FILE_PRIVATE void YFDCWriteDataPort(YFDCState* fdc, uint8_t value) {
  switch (fdc->phase) {
    case Y_FDC_PHASE_IDLE: {
      // This is the first byte of a new command.
      // Extract the opcode (lower 5 bits).
      uint8_t opcode = value & 0x1F;
      fdc->current_command = YFDCFindCommandMetadata(opcode);

      if (!fdc->current_command) {
        Y_FDC_WARN("invalid_command", Y_LOG_HEX8("command", opcode));
        // Invalid command. Set up the result phase with an error.
        YFDCResultBufferClear(&fdc->result_buffer);
        uint8_t status = Y_FDC_ST0_INVALID_COMMAND;
        YFDCResultBufferAppend(&fdc->result_buffer, &status);
        YFDCFinishCommandExecution(fdc);
        return;
      }

      // Clear previous command and store the first byte.
      YFDCCommandBufferClear(&fdc->command_buffer);
      YFDCCommandBufferAppend(&fdc->command_buffer, &value);

      if (fdc->current_command->num_param_bytes == 0) {
        // Command has no parameters, move directly to execution.
        YFDCStartCommandExecution(fdc);
      } else {
        // Wait for parameter bytes.
        fdc->phase = Y_FDC_PHASE_COMMAND;
      }
      break;
    }

    case Y_FDC_PHASE_COMMAND: {
      // This is a parameter byte for the current command.
      if (!fdc->current_command) {
        // Should not happen, but as a safeguard, reset to idle.
        fdc->phase = Y_FDC_PHASE_IDLE;
        return;
      }

      // Store the parameter byte.
      YFDCCommandBufferAppend(&fdc->command_buffer, &value);

      // Check if all parameters have been received.
      // Total bytes = 1 (command) + num_param_bytes.
      if (YFDCCommandBufferLength(&fdc->command_buffer) >=
          (uint8_t)(fdc->current_command->num_param_bytes + 1)) {
        // All bytes received, move to execution phase.
        YFDCStartCommandExecution(fdc);
      }
      break;
    }

    case Y_FDC_PHASE_EXECUTION:
      // DMA or Polling write during execution.
      fdc->transfer.data_register = value;
      fdc->transfer.dma_request_active = false;
      break;

    case Y_FDC_PHASE_RESULT:
      // The FDC is busy. Ignore any writes to the data port.
      break;
  }
}

Y_PUBLIC void YFDCWritePort(YFDCState* fdc, uint16_t port, uint8_t value) {
  switch (port) {
    case Y_FDC_PORT_DOR:  // Digital Output Register
      YFDCWriteDORPort(fdc, value);
      break;

    case Y_FDC_PORT_DATA:  // Data Register
      // Logic will be based on the current FDC phase.
      YFDCWriteDataPort(fdc, value);
      break;

    default:
      // Ignore writes to other ports.
      break;
  }
}

Y_PUBLIC void YFDCHandleTC(YFDCState* fdc) { fdc->transfer.tc_received = true; }

Y_PUBLIC void YFDCInsertDisk(
    YFDCState* fdc, uint8_t drive, const YFDCDiskFormat* format) {
  if (drive >= Y_FDC_NUM_DRIVES) {
    return;
  }
  YFDCDriveState* drive_state = &fdc->drives[drive];
  drive_state->present = true;
  drive_state->format = format;
  drive_state->head = 0;
  drive_state->track = 0;
}

Y_PUBLIC void YFDCEjectDisk(YFDCState* fdc, uint8_t drive) {
  if (drive >= Y_FDC_NUM_DRIVES) {
    return;
  }
  YFDCDriveState* drive_state = &fdc->drives[drive];
  drive_state->present = false;
  drive_state->format = NULL;
}

Y_PUBLIC void YFDCTick(YFDCState* fdc) {
  if (fdc->phase != Y_FDC_PHASE_EXECUTION) {
    return;
  }

  // Run the command handler if defined.
  if (fdc->current_command && fdc->current_command->handler) {
    fdc->current_command->handler(fdc);
    ++fdc->current_command_ticks;
  } else {
    // No handler defined, finish execution immediately.
    YFDCResultBufferClear(&fdc->result_buffer);
    YFDCFinishCommandExecution(fdc);
  }
}
