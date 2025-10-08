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
  // 8237 DMA Controller
  kDMAPortChannel0Address = 0x00,
  kDMAPortChannel0Count = 0x01,
  kDMAPortChannel1Address = 0x02,
  kDMAPortChannel1Count = 0x03,
  kDMAPortChannel2Address = 0x04,
  kDMAPortChannel2Count = 0x05,
  kDMAPortChannel3Address = 0x06,
  kDMAPortChannel3Count = 0x07,
  kDMAPortCommandStatus = 0x08,
  kDMAPortRequest = 0x09,
  kDMAPortSingleMask = 0x0A,
  kDMAPortMode = 0x0B,
  kDMAPortFlipFlopReset = 0x0C,
  kDMAPortMasterReset = 0x0D,
  kDMAPortAllMask = 0x0F,

  // 74LS670 Page Registers
  kDMAPortPageChannel2 = 0x81,  // Floppy
  kDMAPortPageChannel3 = 0x82,  // Hard Drive
  kDMAPortPageChannel1 = 0x83,
  kDMAPortPageChannel0 = 0x87,
} DMAPort;

// Bit definitions for the Mode Register (Port 0x0B)
enum {
  kDMAModeSelectChannel0 = 0x00,
  kDMAModeSelectChannel1 = 0x01,
  kDMAModeSelectChannel2 = 0x02,
  kDMAModeSelectChannel3 = 0x03,
  kDMAModeTransferTypeVerify = 0x00,
  kDMAModeTransferTypeWrite = 0x04,  // Write to memory
  kDMAModeTransferTypeRead = 0x08,   // Read from memory
  kDMAModeAutoInitialize = 0x10,
  kDMAModeAddressDecrement = 0x20,
  kDMAModeDemand = 0x00,
  kDMAModeSingle = 0x40,
  kDMAModeBlock = 0x80,
  kDMAModeCascade = 0xC0,
};

enum {
  // Number of DMA channels in the controller.
  kDMANumChannels = 4,
};

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
  kDMARegisterLSB = 0,
  kDMARegisterMSB = 1,
} DMARegisterByte;

// State for the entire 8237 DMA controller.
typedef struct DMAState {
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
  DMARegisterByte read_register_byte;
} DMAState;

// Initializes the DMA state to its power-on default.
void DMAInit(DMAState* dma);

// Handles reads from the DMA's I/O ports.
uint8_t DMAReadPort(DMAState* dma, uint16_t port);

// Handles writes to the DMA's I/O ports.
void DMAWritePort(DMAState* dma, uint16_t port, uint8_t value);

#endif  // YAX86_DMA_PUBLIC_H

