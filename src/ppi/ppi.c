#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Port B bit definitions
enum {
  // Port B bit 0: Timer 2 gate control
  kPortBTimer2Gate = (1 << 0),
  // Port B bit 1: PC speaker enable / disable
  kPortBSpeakerData = (1 << 1),
  kPortBReadSwitches = (1 << 2),  // 0 = SW1, 1 = SW2
  kPortBCassetteMotor = (1 << 3),
  kPortBIOChannelCheck = (1 << 4),
  kPortBParityCheck = (1 << 5),
  kPortBKeyboardClock = (1 << 6),
  kPortBKeyboardDisable = (1 << 7),
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
  switch (port) {
    case kPPIPortB: {
      bool old_pc_speaker_enabled = PPIIsPCSpeakerEnabled(ppi);
      ppi->port_b = value;

      // Check for changes in control bits and fire callbacks.
      bool pc_speaker_enabled = PPIIsPCSpeakerEnabled(ppi);
      if (old_pc_speaker_enabled != pc_speaker_enabled && ppi->config &&
          ppi->config->set_pc_speaker_frequency) {
        uint32_t frequency =
            pc_speaker_enabled ? ppi->pc_speaker_frequency_from_pit : 0;
        ppi->config->set_pc_speaker_frequency(ppi->config->context, frequency);
      }
      break;
    }
    case kPPIPortControl:
      // The BIOS always writes 0x99 (0b10011001) to set up the PPI. We can
      // ignore it since our emulation is hardcoded to behave accordingly.
      break;

    default:
      // Writes to Port A or C are ignored as they are inputs.
      break;
  }
}

bool PPIIsPCSpeakerEnabled(PPIState* ppi) {
  return (ppi->port_b & kPortBTimer2Gate) && (ppi->port_b & kPortBSpeakerData);
}

void PPISetPCSpeakerFrequencyFromPIT(PPIState* ppi, uint32_t frequency_hz) {
  uint32_t old_frequency = ppi->pc_speaker_frequency_from_pit;
  ppi->pc_speaker_frequency_from_pit = frequency_hz;
  // Invoke the callback only if the speaker is currently enabled and the
  // frequency has changed.
  if (PPIIsPCSpeakerEnabled(ppi) && (frequency_hz != old_frequency) &&
      ppi->config && ppi->config->set_pc_speaker_frequency) {
    ppi->config->set_pc_speaker_frequency(ppi->config->context, frequency_hz);
  }
}

void PPISetScancode(PPIState* ppi, uint8_t scancode) {
  ppi->port_a_latch = scancode;
}
