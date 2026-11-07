#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

Y_PUBLIC void YPPIInit(YPPIState* ppi) {
  // Initially, keyboard clock is enabled (bit 6 = 1) and keyboard read is
  // enabled (bit 7 = 0).
  ppi->port_b = Y_PPI_PORT_B_KEYBOARD_CLOCK_LOW;
}

// Gets the number of floppy drives from the config, clamped to 1-4.
Y_FILE_PRIVATE inline uint8_t YPPIGetNumFloppyDrives(const YPPIConfig* config) {
  if (config->num_floppy_drives < 1) return 1;
  if (config->num_floppy_drives > 4) return 4;
  return config->num_floppy_drives;
}

Y_PUBLIC uint8_t YPPIReadPort(YPPIState* ppi, uint16_t port) {
  switch (port) {
    case Y_PPI_PORT_A:
      // Reading Port A gets the keyboard scancode.
      return ppi->port_a_latch;
    case Y_PPI_PORT_B:
      // Reading Port B returns its last written value.
      return ppi->port_b;
    case Y_PPI_PORT_C:
      if ((ppi->port_b & Y_PPI_PORT_B_DIP_SWITCH_SELECT) == 0) {
        // Read from SW1-4.
        uint8_t port_c = 0;
        // Bit 0: Floppy drive (IPL) installed
        port_c |= (ppi->config.num_floppy_drives > 0) & 0x01;
        // Bit 1: FPU installed
        port_c |= (ppi->config.fpu_installed << 1);
        // Bits 2-3: Memory size
        port_c |= ((ppi->config.memory_size & 0x03) << 2);
        // Bits 4-7 are for unsupported features (cassette, parity, etc.).
        return port_c;
      } else {
        // Read from SW5-8.
        uint8_t port_c = 0;
        // Bits 0-1: Video mode.
        port_c |= ppi->config.display_mode & 0x03;
        // Bits 2-3: Number of drives. Slightly confusingly, the encoding is
        // 1-based, i.e. 00=1 drive, 01=2 drives, etc.
        port_c |= (((YPPIGetNumFloppyDrives(&ppi->config) - 1) & 0x03) << 2);
        // Bits 4-7 are for unsupported features.
        return port_c;
      }
    default:
      // Invalid port.
      return 0xFF;
  }
}

Y_PUBLIC bool YPPIIsPCSpeakerEnabled(YPPIState* ppi) {
  return (ppi->port_b & Y_PPI_PORT_B_TIMER2_GATE) &&
         (ppi->port_b & Y_PPI_PORT_B_SPEAKER_DATA);
}

Y_FILE_PRIVATE inline uint8_t YPPIGetKeyboardControl(const YPPIState* ppi) {
  return (
      ppi->port_b &
      (Y_PPI_PORT_B_KEYBOARD_ENABLE_CLEAR | Y_PPI_PORT_B_KEYBOARD_CLOCK_LOW));
}

Y_PUBLIC void YPPIWritePort(YPPIState* ppi, uint16_t port, uint8_t value) {
  switch (port) {
    case Y_PPI_PORT_B: {
      // Save old states in order to check for changes after the write.
      bool old_speaker_enabled = YPPIIsPCSpeakerEnabled(ppi);
      uint8_t old_keyboard_control = YPPIGetKeyboardControl(ppi);

      ppi->port_b = value;

      // Bit 7: Keyboard enable/clear (0 = enable read, 1 = clear).
      if (value & Y_PPI_PORT_B_KEYBOARD_ENABLE_CLEAR) {
        ppi->port_a_latch = 0;
      }

      // Check for changes in PC speaker control bits and fire callback.
      bool speaker_enabled = YPPIIsPCSpeakerEnabled(ppi);
      if (old_speaker_enabled != speaker_enabled &&
          ppi->config.set_pc_speaker_frequency) {
        const uint32_t frequency = YPPIIsPCSpeakerEnabled(ppi)
                                       ? ppi->pc_speaker_frequency_from_pit
                                       : 0;
        ppi->config.set_pc_speaker_frequency(ppi->config.context, frequency);
      }

      // Check for changes in keyboard control bits and fire callback.
      uint8_t keyboard_control = YPPIGetKeyboardControl(ppi);
      if (old_keyboard_control != keyboard_control &&
          ppi->config.set_keyboard_control) {
        ppi->config.set_keyboard_control(
            ppi->config.context,
            (ppi->port_b & Y_PPI_PORT_B_KEYBOARD_ENABLE_CLEAR) != 0,
            (ppi->port_b & Y_PPI_PORT_B_KEYBOARD_CLOCK_LOW) != 0);
      }
      break;
    }
    case Y_PPI_PORT_CONTROL:
      // The BIOS always writes 0x99 (0b10011001) to set up the PPI. We can
      // ignore it since our emulation is hardcoded to behave accordingly.
      break;

    default:
      // Writes to Port A or C are ignored as they are inputs.
      break;
  }
}

Y_PUBLIC void YPPISetPCSpeakerFrequencyFromPIT(
    YPPIState* ppi, uint32_t frequency_hz) {
  uint32_t old_frequency = ppi->pc_speaker_frequency_from_pit;
  ppi->pc_speaker_frequency_from_pit = frequency_hz;
  // Invoke the callback only if the speaker is currently enabled and the
  // frequency has changed.
  if (YPPIIsPCSpeakerEnabled(ppi) && (frequency_hz != old_frequency) &&
      ppi->config.set_pc_speaker_frequency) {
    ppi->config.set_pc_speaker_frequency(ppi->config.context, frequency_hz);
  }
}

Y_PUBLIC void YPPISetScancode(YPPIState* ppi, uint8_t scancode) {
  ppi->port_a_latch = scancode;
}
