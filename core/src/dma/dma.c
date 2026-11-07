#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

#define YAX86_DMA_LOG(level, ...) \
  YAX86_LOG(dma->config.logger, &YDMALogModule, level, __VA_ARGS__)

YAX86_PUBLIC void YDMAInit(YDMAState* dma) {
  // Mask all channels by default on power-on.
  dma->mask_register = 0x0F;
}

// Helper to read a 16-bit value byte-by-byte using the flip-flop.
YAX86_FILE_PRIVATE inline uint8_t YDMAReadRegisterByte(
    YDMAState* dma, uint16_t value) {
  uint8_t byte;
  if (dma->rw_byte == Y_DMA_REGISTER_MSB) {
    byte = (value >> 8) & 0xFF;
    dma->rw_byte = Y_DMA_REGISTER_LSB;
  } else {
    byte = value & 0xFF;
    dma->rw_byte = Y_DMA_REGISTER_MSB;
  }
  return byte;
}

YAX86_PUBLIC uint8_t YDMAReadPort(YDMAState* dma, uint16_t port) {
  switch (port) {
    // Channel Address and Count Registers (ports 0x00-0x07)
    case Y_DMA_PORT_CHANNEL0_ADDRESS:
    case Y_DMA_PORT_CHANNEL0_COUNT:
    case Y_DMA_PORT_CHANNEL1_ADDRESS:
    case Y_DMA_PORT_CHANNEL1_COUNT:
    case Y_DMA_PORT_CHANNEL2_ADDRESS:
    case Y_DMA_PORT_CHANNEL2_COUNT:
    case Y_DMA_PORT_CHANNEL3_ADDRESS:
    case Y_DMA_PORT_CHANNEL3_COUNT: {
      const int channel_index = port / 2;
      const bool is_count_register = port % 2;
      const YDMAChannelState* channel = &dma->channels[channel_index];
      return YDMAReadRegisterByte(
          dma, is_count_register ? channel->current_count
                                 : channel->current_address);
    }

    // Status Register (port 0x08)
    case Y_DMA_PORT_COMMAND_STATUS: {
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
YAX86_FILE_PRIVATE inline void YDMAWriteRegisterByte(
    YDMAState* dma, uint16_t* base_reg, uint16_t* current_reg, uint8_t value) {
  if (dma->rw_byte == Y_DMA_REGISTER_MSB) {
    // Second write sets the high byte.
    *base_reg = (*base_reg & 0x00FF) | ((uint16_t)value << 8);
    dma->rw_byte = Y_DMA_REGISTER_LSB;
  } else {
    // First write sets the low byte.
    *base_reg = (*base_reg & 0xFF00) | value;
    dma->rw_byte = Y_DMA_REGISTER_MSB;
  }
  // The 'current' register always mirrors the 'base' register after a write.
  *current_reg = *base_reg;
}

YAX86_PUBLIC void YDMAWritePort(YDMAState* dma, uint16_t port, uint8_t value) {
  switch (port) {
    // Channel Address and Count Registers (ports 0x00-0x07)
    case Y_DMA_PORT_CHANNEL0_ADDRESS:
    case Y_DMA_PORT_CHANNEL0_COUNT:
    case Y_DMA_PORT_CHANNEL1_ADDRESS:
    case Y_DMA_PORT_CHANNEL1_COUNT:
    case Y_DMA_PORT_CHANNEL2_ADDRESS:
    case Y_DMA_PORT_CHANNEL2_COUNT:
    case Y_DMA_PORT_CHANNEL3_ADDRESS:
    case Y_DMA_PORT_CHANNEL3_COUNT: {
      const int channel_index = port / 2;
      const bool is_count_register = port % 2;
      YDMAChannelState* channel = &dma->channels[channel_index];

      if (is_count_register) {
        YDMAWriteRegisterByte(
            dma, &channel->base_count, &channel->current_count, value);
      } else {
        YDMAWriteRegisterByte(
            dma, &channel->base_address, &channel->current_address, value);
      }
      break;
    }

    // Command Register (port 0x08)
    case Y_DMA_PORT_COMMAND_STATUS:
      dma->command_register = value;
      break;

    // Request Register (port 0x09)
    case Y_DMA_PORT_REQUEST:
      dma->request_register = value;
      break;

    // Single Mask Register (port 0x0A)
    case Y_DMA_PORT_SINGLE_MASK: {
      const int channel_index = value & 0x03;
      const bool should_mask = (value >> 2) & 1;
      if (should_mask) {
        dma->mask_register |= (1 << channel_index);
      } else {
        dma->mask_register &= ~(1 << channel_index);
        // Unmasking is the point at which a channel becomes live, so the
        // address, count and page registers are final here.
        const YDMAChannelState* channel = &dma->channels[channel_index];
        YAX86_DMA_LOG(
            Y_LOG_LEVEL_DEBUG,
            "channel %d enabled: address %02X:%04X count %04X mode %02X",
            channel_index, channel->page_register, channel->current_address,
            channel->current_count, channel->mode);
      }
      break;
    }

    // Mode Register (port 0x0B)
    case Y_DMA_PORT_MODE: {
      const int channel_index = value & 0x03;
      dma->channels[channel_index].mode = value;
      break;
    }

    // Clear Byte Pointer Flip-Flop (port 0x0C)
    case Y_DMA_PORT_FLIP_FLOP_RESET:
      dma->rw_byte = Y_DMA_REGISTER_LSB;
      break;

    // Master Reset (port 0x0D)
    case Y_DMA_PORT_MASTER_RESET:
      // A master reset returns the controller to its power-on state, so
      // unlike YDMAInit() this does have to clear what is there. The config is
      // what the host wired up and survives - a reset is the chip's, not the
      // machine's. Members left out of an initializer are zeroed, so naming
      // the one that stays is the whole of it.
      *dma = (YDMAState){.config = dma->config};
      YDMAInit(dma);
      break;

    // Mask Register for all channels (port 0x0F)
    case Y_DMA_PORT_ALL_MASK:
      dma->mask_register = value & 0x0F;
      break;

    // Page Registers
    case Y_DMA_PORT_PAGE_CHANNEL0:
      dma->channels[0].page_register = value;
      break;
    case Y_DMA_PORT_PAGE_CHANNEL1:
      dma->channels[1].page_register = value;
      break;
    case Y_DMA_PORT_PAGE_CHANNEL2:
      dma->channels[2].page_register = value;
      break;
    case Y_DMA_PORT_PAGE_CHANNEL3:
      dma->channels[3].page_register = value;
      break;

    default:
      // Ignore writes to read-only or unused ports.
      break;
  }
}

YAX86_PUBLIC YAX86_HOT void YDMATransferByte(
    YDMAState* dma, uint8_t channel_index) {
  if (channel_index >= Y_DMA_NUM_CHANNELS) {
    return;
  }
  YDMAChannelState* channel = &dma->channels[channel_index];

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
    case Y_DMA_MODE_TRANSFER_TYPE_VERIFY:  // Verify - no actual transfer
      break;
    case Y_DMA_MODE_TRANSFER_TYPE_WRITE:  // Write to memory (device -> memory)
      if (dma->config.read_device_byte && dma->config.write_memory_byte) {
        const uint8_t data =
            dma->config.read_device_byte(dma->config.context, channel_index);
        dma->config.write_memory_byte(dma->config.context, address, data);
      }
      break;
    case Y_DMA_MODE_TRANSFER_TYPE_READ:  // Read from memory (memory -> device)
      if (dma->config.read_memory_byte && dma->config.write_device_byte) {
        const uint8_t data =
            dma->config.read_memory_byte(dma->config.context, address);
        dma->config.write_device_byte(dma->config.context, channel_index, data);
      }
      break;
    default:
      // Invalid/reserved mode, do nothing
      break;
  }

  // Update address register
  if ((channel->mode & Y_DMA_MODE_ADDRESS_DECREMENT) == 0) {
    ++channel->current_address;
  } else {
    --channel->current_address;
  }

  // Update count register and check for Terminal Count (TC)
  --channel->current_count;
  if (channel->current_count == 0xFFFF) {
    // Set TC bit in status register
    dma->status_register |= (1 << channel_index);

    // Notify the system that TC has been reached.
    if (dma->config.on_terminal_count) {
      dma->config.on_terminal_count(dma->config.context, channel_index);
    }

    // Handle auto-initialization or mask the channel
    if ((channel->mode & Y_DMA_MODE_AUTO_INITIALIZE) != 0) {
      channel->current_address = channel->base_address;
      channel->current_count = channel->base_count;
    } else {
      dma->mask_register |= (1 << channel_index);
    }
  }
}
