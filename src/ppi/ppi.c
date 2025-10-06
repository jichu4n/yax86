#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Port B bit definitions
enum {
  kPortBTimer2Gate = (1 << 0),
  kPortBSpeakerData = (1 << 1),
  kPortBReadSwitches = (1 << 2), // 0 = SW1, 1 = SW2
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
    case kPPIPortC: {
      // Reading Port C gets the DIP switch state.
      // The bank of switches to read is determined by Port B, bit 2.
      if (!ppi->config || !ppi->config->read_dip_switches) {
        return 0xFF; // Return all high if not configured
      }
      int bank = (ppi->port_b & kPortBReadSwitches) ? 1 : 0;
      return ppi->config->read_dip_switches(ppi->config->context, bank);
    }
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

      // Bit 0: Timer 2 Gate
      if ((value & kPortBTimer2Gate) != (old_port_b & kPortBTimer2Gate)) {
        if (ppi->config->set_pit_gate_2) {
          ppi->config->set_pit_gate_2(ppi->config->context, value & kPortBTimer2Gate);
        }
      }

      // Bit 1: Speaker Data
      if ((value & kPortBSpeakerData) != (old_port_b & kPortBSpeakerData)) {
        if (ppi->config->set_speaker_data) {
          ppi->config->set_speaker_data(ppi->config->context, value & kPortBSpeakerData);
        }
      }

      // Bit 6: Keyboard Clock Inhibit
      if ((value & kPortBKeyboardClock) != (old_port_b & kPortBKeyboardClock)) {
        if (ppi->config->set_keyboard_inhibited) {
          ppi->config->set_keyboard_inhibited(ppi->config->context, !!(value & kPortBKeyboardClock));
        }
      }

      // Bit 7: Keyboard Data (Reset Pulse)
      // The BIOS pulses this bit (high then low) to reset the keyboard.
      // We trigger on the rising edge.
      if ((value & kPortBKeyboardData) && !(old_port_b & kPortBKeyboardData)) {
        if (ppi->config->reset_keyboard) {
          ppi->config->reset_keyboard(ppi->config->context);
        }
      }

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
