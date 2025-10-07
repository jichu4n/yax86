#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Port B bit definitions
enum {
  kPortBTimer2Gate = (1 << 0),
  kPortBSpeakerData = (1 << 1),
  kPortBReadSwitches = (1 << 2),  // 0 = SW1, 1 = SW2
  kPortBCassetteMotor = (1 << 3),
  kPortBIOChannelCheck = (1 << 4),
  kPortBParityCheck = (1 << 5),
  kPortBKeyboardClock = (1 << 6),
  kPortBKeyboardData = (1 << 7),
};

void PPIInit(PPIState* ppi, PPIConfig* config) {
  static const PPIState zero_ppi_state = {0};
  *ppi = zero_ppi_state;
  ppi->config = config;
}

uint8_t PPIReadPort(PPIState* ppi, uint16_t port) {
  switch (port) {
    case kPPIPortA:
      // Reading Port A gets the keyboard scancode.
      return ppi->port_a_latch;
    case kPPIPortB:
      // Reading Port B returns its last written value.
      return ppi->port_b;
    case kPPIPortC:
      // TODO: Implement reading DIP switches. For now, return 0xFF.
      return 0xFF;
    default:
      // Invalid port.
      return 0xFF;
  }
}

void PPIWritePort(PPIState* ppi, uint16_t port, uint8_t value) {
  if (!ppi->config) {
    return;
  }

  switch (port) {
    case kPPIPortB: {
      uint8_t old_port_b = ppi->port_b;
      ppi->port_b = value;

      // Check for changes in control bits and fire callbacks.
      // TODO

      break;
    }
    case kPPIPortControl:
      // The BIOS writes 0x99 to set up Mode 0. We don't need to do anything
      // since our emulation is hardcoded to this mode.
      break;

    default:
      // Writes to Port A or C are ignored as they are inputs.
      break;
  }
}

void PPISetScancode(PPIState* ppi, uint8_t scancode) {
  ppi->port_a_latch = scancode;
}
