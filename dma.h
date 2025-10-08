// ==============================================================================
// YAX86 DMA MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_DMA_BUNDLE_H
#define YAX86_DMA_BUNDLE_H

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

// I/O ports for the 8237 DMA Controller and Page Registers.
typedef enum DMAPort {
  // --- 8237 DMA Controller ---
  // Channel 0 base and current address
  kDMAPortChannel0Address = 0x00,
  // Channel 0 base and current word count
  kDMAPortChannel0Count = 0x01,
  // Channel 1 base and current address
  kDMAPortChannel1Address = 0x02,
  // Channel 1 base and current word count
  kDMAPortChannel1Count = 0x03,
  // Channel 2 base and current address
  kDMAPortChannel2Address = 0x04,
  // Channel 2 base and current word count
  kDMAPortChannel2Count = 0x05,
  // Channel 3 base and current address
  kDMAPortChannel3Address = 0x06,
  // Channel 3 base and current word count
  kDMAPortChannel3Count = 0x07,
  // Read: Status Register / Write: Command Register
  kDMAPortCommandStatus = 0x08,
  // Write: Request Register
  kDMAPortRequest = 0x09,
  // Write: Set/Clear a single channel's mask bit
  kDMAPortSingleMask = 0x0A,
  // Write: Mode Register
  kDMAPortMode = 0x0B,
  // Write: Clear Byte Pointer Flip-Flop
  kDMAPortFlipFlopReset = 0x0C,
  // Write: Master Reset
  kDMAPortMasterReset = 0x0D,
  // Write: Mask Register (for all channels)
  kDMAPortAllMask = 0x0F,

  // --- 74LS670 Page Registers ---
  // Page register for Channel 2 (Floppy)
  kDMAPortPageChannel2 = 0x81,
  // Page register for Channel 3 (Hard Drive)
  kDMAPortPageChannel3 = 0x82,
  // Page register for Channel 1
  kDMAPortPageChannel1 = 0x83,
  // Page register for Channel 0
  kDMAPortPageChannel0 = 0x87,
} DMAPort;

// Bit definitions for the Mode Register (Port 0x0B)
enum {
  // --- Channel Select (bits 0-1) ---
  // Select channel 0
  kDMAModeSelectChannel0 = 0x00,
  // Select channel 1
  kDMAModeSelectChannel1 = 0x01,
  // Select channel 2
  kDMAModeSelectChannel2 = 0x02,
  // Select channel 3
  kDMAModeSelectChannel3 = 0x03,

  // --- Transfer Type (bits 2-3) ---
  // Verify transfer (no data is moved)
  kDMAModeTransferTypeVerify = 0x00,
  // Write to memory (device -> memory)
  kDMAModeTransferTypeWrite = 0x04,
  // Read from memory (memory -> device)
  kDMAModeTransferTypeRead = 0x08,

  // --- Auto-initialization (bit 4) ---
  // If set, the channel reloads its base address and count after a transfer.
  kDMAModeAutoInitialize = 0x10,

  // --- Address Direction (bit 5) ---
  // If set, the memory address is decremented; otherwise, it is incremented.
  kDMAModeAddressDecrement = 0x20,

  // --- Transfer Mode (bits 6-7) ---
  // Demand mode: transfer bytes until the DREQ line becomes inactive.
  kDMAModeDemand = 0x00,
  // Single mode: transfer one byte for each DREQ signal.
  kDMAModeSingle = 0x40,
  // Block mode: transfer an entire block of data in response to a single DREQ.
  kDMAModeBlock = 0x80,
  // Cascade mode: used for chaining multiple DMA controllers (not supported).
  kDMAModeCascade = 0xC0,
};

enum {
  // Number of DMA channels in the controller.
  kDMANumChannels = 4,
};

// ============================================================================
// DMA state
// ============================================================================

struct DMAState;

// Caller-provided runtime configuration for the DMA controller.
typedef struct DMAConfig {
  // Custom data passed through to callbacks.
  void* context;

  // Callback to read a byte from system memory.
  uint8_t (*read_memory_byte)(void* context, uint32_t address);
  // Callback to write a byte to system memory.
  void (*write_memory_byte)(void* context, uint32_t address, uint8_t value);

  // Callback to read a byte from a peripheral for a specific DMA channel.
  uint8_t (*read_device_byte)(void* context, uint8_t channel);
  // Callback to write a byte to a peripheral for a specific DMA channel.
  void (*write_device_byte)(void* context, uint8_t channel, uint8_t value);
} DMAConfig;

// State for a single DMA channel.
typedef struct DMAChannelState {
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
} DMAChannelState;

// Which register byte to read/write next.
typedef enum DMARegisterByte {
  // Read or write the lower byte next.
  kDMARegisterLSB = 0,
  // Read or write the upper byte next.
  kDMARegisterMSB = 1,
} DMARegisterByte;

// State for the entire 8237 DMA controller.
typedef struct DMAState {
  // Pointer to the DMA configuration.
  DMAConfig* config;

  // The four DMA channels.
  DMAChannelState channels[kDMANumChannels];

  // Command register for the controller.
  uint8_t command_register;
  // Status register (Terminal Count and Request flags).
  uint8_t status_register;
  // Software request register.
  uint8_t request_register;
  // Mask register for all four channels.
  uint8_t mask_register;

  // Internal byte flip-flop for 16-bit register access.
  DMARegisterByte rw_byte;
} DMAState;

// ============================================================================
// DMA interface
// ============================================================================

// Initializes the DMA state to its power-on default.
void DMAInit(DMAState* dma, DMAConfig* config);

// Handles reads from the DMA's I/O ports.
uint8_t DMAReadPort(DMAState* dma, uint16_t port);

// Handles writes to the DMA's I/O ports.
void DMAWritePort(DMAState* dma, uint16_t port, uint8_t value);

// Executes a single-byte transfer for the specified channel. This function
// should be called by the platform in response to a DREQ signal from a
// peripheral.
void DMATransferByte(DMAState* dma, uint8_t channel_index);

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
    channel->current_address++;
  } else {
    channel->current_address--;
  }

  // Update count register and check for Terminal Count (TC)
  channel->current_count--;
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



// ==============================================================================
// src/dma/dma.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_DMA_BUNDLE_H

