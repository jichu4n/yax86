#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

void DMAInit(DMAState* dma) {
  static const DMAState zero_dma_state = {0};
  *dma = zero_dma_state;

  // Mask all channels by default on power-on.
  dma->mask_register = 0x0F;
}

// Helper to read a 16-bit value byte-by-byte using the flip-flop.
static inline uint8_t DMAReadRegisterByte(DMAState* dma, uint16_t value) {
  uint8_t byte;
  if (dma->read_register_byte == kDMARegisterMSB) {
    byte = (value >> 8) & 0xFF;
    dma->read_register_byte = kDMARegisterLSB;
  } else {
    byte = value & 0xFF;
    dma->read_register_byte = kDMARegisterMSB;
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
    case kDMAPortCommandStatus:
      return dma->status_register;

    // All other ports are write-only or unused for reads.
    default:
      return 0xFF;
  }
}

void DMAWritePort(DMAState* dma, uint16_t port, uint8_t value) {
  // Ignore all writes for now.
  (void)dma;
  (void)port;
  (void)value;
}

