// ==============================================================================
// YAX86 DMA MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_DMA_BUNDLE_H
#define YAX86_DMA_BUNDLE_H

#ifndef YAX86_UTIL_BUNDLE_H
#include "util.h"
#endif  // YAX86_UTIL_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/dma/public.h start
// ==============================================================================

#line 1 "./src/dma/public.h"
// Public interface for the DMA (Direct Memory Access) module.
#ifndef YAX86_DMA_PUBLIC_H
#define YAX86_DMA_PUBLIC_H

// This module emulates the Intel 8237 DMA controller used in the IBM PC/XT.
// The DMA controller allows peripherals to transfer data directly to and from
// memory without involving the CPU, which is critical for high-speed devices
// like disk drives.
//
// The standard channel assignments are:
// - Channel 0: DRAM Refresh
// - Channel 1: Unused / Expansion
// - Channel 2: Floppy Disk Controller
// - Channel 3: Hard Disk Controller
//
// Note that we do not support all features of the 8237, only those needed to
// support GLaBIOS and basic PC/XT peripherals. Specifically:
// - DRAM Refresh on Channel 0 is not implemented, as it is disabled in the
//   target GLaBIOS build for emulators.
// - Memory-to-memory transfers are not supported.
// - Cascade Mode for multiple DMA controllers is not supported.
// - Advanced transfer modes (Demand, Block) and priorities (Rotating) are not
//   supported. Only Single Cycle mode with Fixed Priority is implemented.

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_DMA_BUNDLE_H
#include "../util/log.h"
#endif  // YAX86_DMA_BUNDLE_H

enum {
  // Log module ID for the DMA.
  Y_DMA_LOG_MODULE_ID = 6,
};

// Log module for the DMA.
Y_PUBLIC_HEADER const YLogModule YDMALogModule = {
    .id = Y_DMA_LOG_MODULE_ID,
    .name = "dma",
};

// I/O ports for the 8237 DMA Controller and Page Registers.
typedef enum YDMAPort {
  // --- 8237 DMA Controller ---
  // Channel 0 base and current address
  Y_DMA_PORT_CHANNEL0_ADDRESS = 0x00,
  // Channel 0 base and current word count
  Y_DMA_PORT_CHANNEL0_COUNT = 0x01,
  // Channel 1 base and current address
  Y_DMA_PORT_CHANNEL1_ADDRESS = 0x02,
  // Channel 1 base and current word count
  Y_DMA_PORT_CHANNEL1_COUNT = 0x03,
  // Channel 2 base and current address
  Y_DMA_PORT_CHANNEL2_ADDRESS = 0x04,
  // Channel 2 base and current word count
  Y_DMA_PORT_CHANNEL2_COUNT = 0x05,
  // Channel 3 base and current address
  Y_DMA_PORT_CHANNEL3_ADDRESS = 0x06,
  // Channel 3 base and current word count
  Y_DMA_PORT_CHANNEL3_COUNT = 0x07,
  // Read: Status Register / Write: Command Register
  Y_DMA_PORT_COMMAND_STATUS = 0x08,
  // Write: Request Register
  Y_DMA_PORT_REQUEST = 0x09,
  // Write: Set/Clear a single channel's mask bit
  Y_DMA_PORT_SINGLE_MASK = 0x0A,
  // Write: Mode Register
  Y_DMA_PORT_MODE = 0x0B,
  // Write: Clear Byte Pointer Flip-Flop
  Y_DMA_PORT_FLIP_FLOP_RESET = 0x0C,
  // Write: Master Reset
  Y_DMA_PORT_MASTER_RESET = 0x0D,
  // Write: Mask Register (for all channels)
  Y_DMA_PORT_ALL_MASK = 0x0F,

  // --- 74LS670 Page Registers ---
  // Page register for Channel 2 (Floppy)
  Y_DMA_PORT_PAGE_CHANNEL2 = 0x81,
  // Page register for Channel 3 (Hard Drive)
  Y_DMA_PORT_PAGE_CHANNEL3 = 0x82,
  // Page register for Channel 1
  Y_DMA_PORT_PAGE_CHANNEL1 = 0x83,
  // Page register for Channel 0
  Y_DMA_PORT_PAGE_CHANNEL0 = 0x87,
} YDMAPort;

// Bit definitions for the Mode Register (Port 0x0B)
enum {
  // --- Channel Select (bits 0-1) ---
  // Select channel 0
  Y_DMA_MODE_SELECT_CHANNEL0 = 0x00,
  // Select channel 1
  Y_DMA_MODE_SELECT_CHANNEL1 = 0x01,
  // Select channel 2
  Y_DMA_MODE_SELECT_CHANNEL2 = 0x02,
  // Select channel 3
  Y_DMA_MODE_SELECT_CHANNEL3 = 0x03,

  // --- Transfer Type (bits 2-3) ---
  // Verify transfer (no data is moved)
  Y_DMA_MODE_TRANSFER_TYPE_VERIFY = 0x00,
  // Write to memory (device -> memory)
  Y_DMA_MODE_TRANSFER_TYPE_WRITE = 0x04,
  // Read from memory (memory -> device)
  Y_DMA_MODE_TRANSFER_TYPE_READ = 0x08,

  // --- Auto-initialization (bit 4) ---
  // If set, the channel reloads its base address and count after a transfer.
  Y_DMA_MODE_AUTO_INITIALIZE = 0x10,

  // --- Address Direction (bit 5) ---
  // If set, the memory address is decremented; otherwise, it is incremented.
  Y_DMA_MODE_ADDRESS_DECREMENT = 0x20,

  // --- Transfer Mode (bits 6-7) ---
  // Demand mode: transfer bytes until the DREQ line becomes inactive.
  Y_DMA_MODE_DEMAND = 0x00,
  // Single mode: transfer one byte for each DREQ signal.
  Y_DMA_MODE_SINGLE = 0x40,
  // Block mode: transfer an entire block of data in response to a single DREQ.
  Y_DMA_MODE_BLOCK = 0x80,
  // Cascade mode: used for chaining multiple DMA controllers (not supported).
  Y_DMA_MODE_CASCADE = 0xC0,
};

enum {
  // Number of DMA channels in the controller.
  Y_DMA_NUM_CHANNELS = 4,
};

// ============================================================================
// DMA state
// ============================================================================

struct YDMAState;

// Caller-provided runtime configuration for the DMA controller.
typedef struct YDMAConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Logger for this module. May be NULL.
  YLogger* logger;

  // Callback to read a byte from system memory.
  uint8_t (*read_memory_byte)(void* context, uint32_t address);
  // Callback to write a byte to system memory.
  void (*write_memory_byte)(void* context, uint32_t address, uint8_t value);

  // Callback to read a byte from a peripheral for a specific DMA channel.
  uint8_t (*read_device_byte)(void* context, uint8_t channel);
  // Callback to write a byte to a peripheral for a specific DMA channel.
  void (*write_device_byte)(void* context, uint8_t channel, uint8_t value);

  // Callback to notify the system that a channel has reached its terminal
  // count. This corresponds to the EOP (End of Process) signal on the 8237,
  // which is connected to the TC (Terminal Count) pin on devices like the FDC.
  void (*on_terminal_count)(void* context, uint8_t channel);
} YDMAConfig;

// State for a single DMA channel.
typedef struct YDMAChannelState {
  // Base address register, reloaded on auto-initialization.
  uint16_t base_address;
  // Current address register, updated during a transfer.
  uint16_t current_address;
  // Base count register, reloaded on auto-initialization.
  uint16_t base_count;
  // Current count register, updated during a transfer.
  uint16_t current_count;
  // Mode register for this channel.
  uint8_t mode;
  // High-order address bits from the page register.
  uint8_t page_register;
} YDMAChannelState;

// Which register byte to read/write next.
typedef enum YDMARegisterByte {
  // Read or write the lower byte next.
  Y_DMA_REGISTER_LSB = 0,
  // Read or write the upper byte next.
  Y_DMA_REGISTER_MSB = 1,
} YDMARegisterByte;

// State for the entire 8237 DMA controller.
typedef struct YDMAState {
  // Pointer to the DMA configuration.
  YDMAConfig config;

  // The four DMA channels.
  YDMAChannelState channels[Y_DMA_NUM_CHANNELS];

  // Command register for the controller.
  uint8_t command_register;
  // Status register (Terminal Count and Request flags).
  uint8_t status_register;
  // Software request register.
  uint8_t request_register;
  // Mask register for all four channels.
  uint8_t mask_register;

  // Internal byte flip-flop for 16-bit register access.
  YDMARegisterByte rw_byte;
} YDMAState;

// ============================================================================
// DMA interface
// ============================================================================

// Initializes the DMA state to its power-on default.
Y_PUBLIC void YDMAInit(YDMAState* dma);

// Handles reads from the DMA's I/O ports.
Y_PUBLIC uint8_t YDMAReadPort(YDMAState* dma, uint16_t port);

// Handles writes to the DMA's I/O ports.
Y_PUBLIC void YDMAWritePort(YDMAState* dma, uint16_t port, uint8_t value);

// Executes a single-byte transfer for the specified channel. This function
// should be called by the platform in response to a DREQ signal from a
// peripheral.
Y_PUBLIC void YDMATransferByte(YDMAState* dma, uint8_t channel_index);

#endif  // YAX86_DMA_PUBLIC_H


// ==============================================================================
// src/dma/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/dma/dma.c start
// ==============================================================================

#line 1 "./src/dma/dma.c"
#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

#define Y_DMA_ERROR(...) \
  Y_LOG(dma->config.logger, &YDMALogModule, Y_LOG_LEVEL_ERROR, __VA_ARGS__)
#define Y_DMA_WARN(...) \
  Y_LOG(dma->config.logger, &YDMALogModule, Y_LOG_LEVEL_WARN, __VA_ARGS__)
#define Y_DMA_INFO(...) \
  Y_LOG(dma->config.logger, &YDMALogModule, Y_LOG_LEVEL_INFO, __VA_ARGS__)
#define Y_DMA_DEBUG(...) \
  Y_LOG(dma->config.logger, &YDMALogModule, Y_LOG_LEVEL_DEBUG, __VA_ARGS__)

Y_PUBLIC void YDMAInit(YDMAState* dma) {
  // Mask all channels by default on power-on.
  dma->mask_register = 0x0F;
}

// Helper to read a 16-bit value byte-by-byte using the flip-flop.
Y_FILE_PRIVATE inline uint8_t YDMAReadRegisterByte(
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

Y_PUBLIC uint8_t YDMAReadPort(YDMAState* dma, uint16_t port) {
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
Y_FILE_PRIVATE inline void YDMAWriteRegisterByte(
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

Y_PUBLIC void YDMAWritePort(YDMAState* dma, uint16_t port, uint8_t value) {
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
        Y_DMA_DEBUG(
            "channel_enabled", Y_LOG_UINT("channel", channel_index),
            Y_LOG_HEX8("page", channel->page_register),
            Y_LOG_HEX16("address", channel->current_address),
            Y_LOG_HEX16("count", channel->current_count),
            Y_LOG_HEX8("mode", channel->mode));
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

Y_PUBLIC Y_HOT void YDMATransferByte(YDMAState* dma, uint8_t channel_index) {
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
    case Y_DMA_MODE_TRANSFER_TYPE_VERIFY:
    case Y_DMA_MODE_TRANSFER_TYPE_WRITE:
      // Device -> memory. A verify cycle acknowledges the device just the
      // same, so the device hands its byte over and moves on, but nothing is
      // written: the BIOS verifies sectors this way, and a floppy controller
      // left waiting for its byte to be taken never finishes the read.
      if (dma->config.read_device_byte) {
        const uint8_t data =
            dma->config.read_device_byte(dma->config.context, channel_index);
        if (transfer_type == Y_DMA_MODE_TRANSFER_TYPE_WRITE &&
            dma->config.write_memory_byte) {
          dma->config.write_memory_byte(dma->config.context, address, data);
        }
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


// ==============================================================================
// src/dma/dma.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_DMA_BUNDLE_H

