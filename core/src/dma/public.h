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
YAX86_PUBLIC_HEADER const YLogModule YDMALogModule = {
    .id = Y_DMA_LOG_MODULE_ID,
    .name = "DMA",
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
YAX86_PUBLIC void YDMAInit(YDMAState* dma);

// Handles reads from the DMA's I/O ports.
YAX86_PUBLIC uint8_t YDMAReadPort(YDMAState* dma, uint16_t port);

// Handles writes to the DMA's I/O ports.
YAX86_PUBLIC void YDMAWritePort(YDMAState* dma, uint16_t port, uint8_t value);

// Executes a single-byte transfer for the specified channel. This function
// should be called by the platform in response to a DREQ signal from a
// peripheral.
YAX86_PUBLIC void YDMATransferByte(YDMAState* dma, uint8_t channel_index);

#endif  // YAX86_DMA_PUBLIC_H
