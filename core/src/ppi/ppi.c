#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

void PPIInit(PPIState* ppi, PPIConfig* config) {
  static const PPIState zero_ppi_state = {0};
  *ppi = zero_ppi_state;
  ppi->config = config;
  // Initially, keyboard clock is enabled (bit 6 = 1) and keyboard read is 
  // enabled (bit 7 = 0).
  ppi->port_b = kPPIPortBKeyboardClockLow;
}

// Gets the number of floppy drives from the config, clamped to 1-4.
static inline uint8_t GetNumFloppyDrives(const PPIConfig* config) {
  if (config->num_floppy_drives < 1) return 1;
  if (config->num_floppy_drives > 4) return 4;
  return config->num_floppy_drives;
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
      if ((ppi->port_b & kPPIPortBDipSwitchSelect) == 0) {
        // Read from SW1-4.
        uint8_t port_c = 0;
        // Bit 0: Floppy drive (IPL) installed
        port_c |= (ppi->config->num_floppy_drives > 0) & 0x01;
        // Bit 1: FPU installed
        port_c |= (ppi->config->fpu_installed << 1);
        // Bits 2-3: Memory size
        port_c |= ((ppi->config->memory_size & 0x03) << 2);
        // Bits 4-7 are for unsupported features (cassette, parity, etc.).
        return port_c;
      } else {
        // Read from SW5-8.
        uint8_t port_c = 0;
        // Bits 0-1: Video mode.
        port_c |= ppi->config->display_mode & 0x03;
        // Bits 2-3: Number of drives. Slightly confusingly, the encoding is
        // 1-based, i.e. 00=1 drive, 01=2 drives, etc.
        port_c |= (((GetNumFloppyDrives(ppi->config) - 1) & 0x03) << 2);
        // Bits 4-7 are for unsupported features.
        return port_c;
      }
    default:
      // Invalid port.
      return 0xFF;
  }
}

bool PPIIsPCSpeakerEnabled(PPIState* ppi) {
  return (ppi->port_b & kPPIPortBTimer2Gate) &&
         (ppi->port_b & kPPIPortBSpeakerData);
}

static inline uint8_t PPIGetKeyboardControl(const PPIState* ppi) {
  return (
      ppi->port_b & (kPPIPortBKeyboardEnableClear | kPPIPortBKeyboardClockLow));
}

void PPIWritePort(PPIState* ppi, uint16_t port, uint8_t value) {
  switch (port) {
    case kPPIPortB: {
      // Save old states in order to check for changes after the write.
      bool old_speaker_enabled = PPIIsPCSpeakerEnabled(ppi);
      uint8_t old_keyboard_control = PPIGetKeyboardControl(ppi);

      ppi->port_b = value;

      // Bit 7: Keyboard enable/clear (0 = enable read, 1 = clear).
      if (value & kPPIPortBKeyboardEnableClear) {
        ppi->port_a_latch = 0;
      }

      // Check for changes in PC speaker control bits and fire callback.
      bool speaker_enabled = PPIIsPCSpeakerEnabled(ppi);
      if (old_speaker_enabled != speaker_enabled && ppi->config &&
          ppi->config->set_pc_speaker_frequency) {
        const uint32_t frequency =
            PPIIsPCSpeakerEnabled(ppi) ? ppi->pc_speaker_frequency_from_pit : 0;
        ppi->config->set_pc_speaker_frequency(ppi->config->context, frequency);
      }

      // Check for changes in keyboard control bits and fire callback.
      uint8_t keyboard_control = PPIGetKeyboardControl(ppi);
      if (old_keyboard_control != keyboard_control && ppi->config &&
          ppi->config->set_keyboard_control) {
        ppi->config->set_keyboard_control(
            ppi->config->context,
            (ppi->port_b & kPPIPortBKeyboardEnableClear) != 0,
            (ppi->port_b & kPPIPortBKeyboardClockLow) != 0);
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
