#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

void DMAInit(DMAState* dma, DMAConfig* config) {
  static const DMAState zero_dma_state = {0};
  *dma = zero_dma_state;

  dma->config = config;

  // Mask all channels by default on power-on.
  dma->mask_register = 0x0F;
}

// Helper to read a 16-bit value byte-by-byte using the flip-flop.
static inline uint8_t DMAReadRegisterByte(DMAState* dma, uint16_t value) {
  uint8_t byte;
  if (dma->rw_byte == kDMARegisterMSB) {
    byte = (value >> 8) & 0xFF;
    dma->rw_byte = kDMARegisterLSB;
  } else {
    byte = value & 0xFF;
    dma->rw_byte = kDMARegisterMSB;
  }
  return byte;
}

uint8_t DMAReadPort(DMAState* dma, uint16_t port) {
  switch (port) {
    // Channel Address and Count Registers (ports 0x00-0x07)
    case kDMAPortChannel0Address:
    case kDMAPortChannel0Count:
    case kDMAPortChannel1Address:
    case kDMAPortChannel1Count:
    case kDMAPortChannel2Address:
    case kDMAPortChannel2Count:
    case kDMAPortChannel3Address:
    case kDMAPortChannel3Count: {
      const int channel_index = port / 2;
      const bool is_count_register = port % 2;
      const DMAChannelState* channel = &dma->channels[channel_index];
      return DMAReadRegisterByte(
          dma, is_count_register ? channel->current_count
                                 : channel->current_address);
    }

    // Status Register (port 0x08)
    case kDMAPortCommandStatus: {
      uint8_t status = dma->status_register;
      dma->status_register = 0;  // Clear TC flags on read
      return status;
    }

    // All other ports are write-only or unused for reads.
    default:
      return 0xFF;
  }
}

// Helper to write a 16-bit value byte-by-byte using the flip-flop.
// Note: Writes update both the 'base' and 'current' registers.
static inline void DMAWriteRegisterByte(
    DMAState* dma, uint16_t* base_reg, uint16_t* current_reg, uint8_t value) {
  if (dma->rw_byte == kDMARegisterMSB) {
    // Second write sets the high byte.
    *base_reg = (*base_reg & 0x00FF) | ((uint16_t)value << 8);
    dma->rw_byte = kDMARegisterLSB;
  } else {
    // First write sets the low byte.
    *base_reg = (*base_reg & 0xFF00) | value;
    dma->rw_byte = kDMARegisterMSB;
  }
  // The 'current' register always mirrors the 'base' register after a write.
  *current_reg = *base_reg;
}

void DMAWritePort(DMAState* dma, uint16_t port, uint8_t value) {
  switch (port) {
    // Channel Address and Count Registers (ports 0x00-0x07)
    case kDMAPortChannel0Address:
    case kDMAPortChannel0Count:
    case kDMAPortChannel1Address:
    case kDMAPortChannel1Count:
    case kDMAPortChannel2Address:
    case kDMAPortChannel2Count:
    case kDMAPortChannel3Address:
    case kDMAPortChannel3Count: {
      const int channel_index = port / 2;
      const bool is_count_register = port % 2;
      DMAChannelState* channel = &dma->channels[channel_index];

      if (is_count_register) {
        DMAWriteRegisterByte(
            dma, &channel->base_count, &channel->current_count, value);
      } else {
        DMAWriteRegisterByte(
            dma, &channel->base_address, &channel->current_address, value);
      }
      break;
    }

    // Command Register (port 0x08)
    case kDMAPortCommandStatus:
      dma->command_register = value;
      break;

    // Request Register (port 0x09)
    case kDMAPortRequest:
      dma->request_register = value;
      break;

    // Single Mask Register (port 0x0A)
    case kDMAPortSingleMask: {
      const int channel_index = value & 0x03;
      const bool should_mask = (value >> 2) & 1;
      if (should_mask) {
        dma->mask_register |= (1 << channel_index);
      } else {
        dma->mask_register &= ~(1 << channel_index);
      }
      break;
    }

    // Mode Register (port 0x0B)
    case kDMAPortMode: {
      const int channel_index = value & 0x03;
      dma->channels[channel_index].mode = value;
      break;
    }

    // Clear Byte Pointer Flip-Flop (port 0x0C)
    case kDMAPortFlipFlopReset:
      dma->rw_byte = kDMARegisterLSB;
      break;

    // Master Reset (port 0x0D)
    case kDMAPortMasterReset:
      DMAInit(dma, dma->config);
      break;

    // Mask Register for all channels (port 0x0F)
    case kDMAPortAllMask:
      dma->mask_register = value & 0x0F;
      break;

    // Page Registers
    case kDMAPortPageChannel0:
      dma->channels[0].page_register = value;
      break;
    case kDMAPortPageChannel1:
      dma->channels[1].page_register = value;
      break;
    case kDMAPortPageChannel2:
      dma->channels[2].page_register = value;
      break;
    case kDMAPortPageChannel3:
      dma->channels[3].page_register = value;
      break;

    default:
      // Ignore writes to read-only or unused ports.
      break;
  }
}

void DMATransferByte(DMAState* dma, uint8_t channel_index) {
  if (channel_index >= kDMANumChannels) {
    return;
  }
  DMAChannelState* channel = &dma->channels[channel_index];

  // Check if controller is disabled (bit 2 of command register).
  if ((dma->command_register & 0x04) != 0) {
    return;
  }

  // If channel is masked, do nothing.
  if ((dma->mask_register & (1 << channel_index)) != 0) {
    return;
  }

  // Construct full 20-bit memory address
  const uint32_t address =
      ((uint32_t)channel->page_register << 16) | channel->current_address;

  // Perform transfer based on type (bits 2-3 of mode register)
  const uint8_t transfer_type = channel->mode & (0x03 << 2);
  switch (transfer_type) {
    case kDMAModeTransferTypeVerify:  // Verify - no actual transfer
      break;
    case kDMAModeTransferTypeWrite:  // Write to memory (device -> memory)
      if (dma->config->read_device_byte && dma->config->write_memory_byte) {
        const uint8_t data =
            dma->config->read_device_byte(dma->config->context, channel_index);
        dma->config->write_memory_byte(dma->config->context, address, data);
      }
      break;
    case kDMAModeTransferTypeRead:  // Read from memory (memory -> device)
      if (dma->config->read_memory_byte && dma->config->write_device_byte) {
        const uint8_t data =
            dma->config->read_memory_byte(dma->config->context, address);
        dma->config->write_device_byte(
            dma->config->context, channel_index, data);
      }
      break;
    default:
      // Invalid/reserved mode, do nothing
      break;
  }

  // Update address register
  if ((channel->mode & kDMAModeAddressDecrement) == 0) {
    ++channel->current_address;
  } else {
    --channel->current_address;
  }

  // Update count register and check for Terminal Count (TC)
  --channel->current_count;
  if (channel->current_count == 0xFFFF) {
    // Set TC bit in status register
    dma->status_register |= (1 << channel_index);

    // Handle auto-initialization or mask the channel
    if ((channel->mode & kDMAModeAutoInitialize) != 0) {
      channel->current_address = channel->base_address;
      channel->current_count = channel->base_count;
    } else {
      dma->mask_register |= (1 << channel_index);
    }
  }
}

