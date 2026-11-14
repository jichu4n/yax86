// ==============================================================================
// YAX86 PPI MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_PPI_BUNDLE_H
#define YAX86_PPI_BUNDLE_H

#ifndef YAX86_UTIL_BUNDLE_H
#include "util.h"
#endif  // YAX86_UTIL_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/ppi/public.h start
// ==============================================================================

#line 1 "./src/ppi/public.h"
// Public interface for the PPI (Programmable Peripheral Interface) module.
#ifndef YAX86_PPI_PUBLIC_H
#define YAX86_PPI_PUBLIC_H

// This module emulates the Intel 8255 PPI chip as used in the IBM PC and
// PC/XT. Note that we don't implement all features of the 8255, only those
// needed to support GLaBIOS in ARCH_TYPE_EMU mode. Specifically:
//
// - Simplified PC speaker control
//     - Only on/off and frequency from PIT channel 2
//     - No real-time mirroring of PIT channel 2 on port C pin 5
// - No memory or I/O parity checking
// - No cassette support
//
// Reference tables from GLaBIOS source code:
//
// ----------------------------------------------------------------------------
//  5160/Standard: 8255 PPI Channel B (Port 61h) Flags
// ----------------------------------------------------------------------------
//  84218421
//  7 	    |	PBKB	0=enable keyboard read, 1=clear
//   6      |	PBKC	0=hold keyboard clock low, 1=enable clock
//    5     |	PBIO	0=enable i/o check, 1=disable
//     4    |	PBPC	0=enable memory parity check, 1=disable
//      3   |	PBSW	0=read SW1-4, 1=read SW-5-8
//       2  |	PBTB	0=turbo, 1=normal
//        1 |	PBSP	0=turn off speaker, 1=turn on
//         0|	PBST	0=turn off timer 2, 1=turn on
// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------
//  5160: 8255 PPI Channel C (Port 62h) Flags When PPI B PBSW = 0
// ----------------------------------------------------------------------------
//  84218421
//  7 	    |	PCPE	0=no parity error, 1=memory parity error
//   6      |	PCIE	0=no i/o channel error, 1=i/o channel error
//    5     |	PCT2	timer 2 output / cassette data output
//     4    |	PCCI	cassette data input
//      32  |	PCMB	SW 3,4: MB RAM (00=64K, 01=128K, 10=192K, 11=256K)
//        1 |	PCFP	SW 2: 0=no FPU, 1=FPU installed
//         0|	PCFD	SW 1: Floppy drive (IPL) installed
// ----------------------------------------------------------------------------
//
// ----------------------------------------------------------------------------
//  8255 PPI Channel C (Port 62h) Flags When PPI B PBSW = 1
// ----------------------------------------------------------------------------
//  84218421
//  7 	    |	PC2PE	0=no parity error, 1 r/w memory parity check error
//   6      |	PC2IE	0=no i/o channel error, 1 i/o channel check error
//    5     |	PC2T2	timer 2 output
//     4    |	PC2CI	cassette data input
//      32  |	PCDRV	SW 7,8: # of drives (00=1, 01=2, 10=3, 11=4)
//        10|	PCVID	SW 5,6: video Mode (00=ROM, 01=CG40, 10=CG80, 11=MDA)
// ----------------------------------------------------------------------------

#include <stdbool.h>
#include <stdint.h>

#ifndef YAX86_PPI_BUNDLE_H
#include "../util/log.h"
#endif  // YAX86_PPI_BUNDLE_H

enum {
  // Log module ID for the PPI.
  Y_PPI_LOG_MODULE_ID = 4,
};

// Log module for the PPI.
Y_PUBLIC_HEADER const YLogModule YPPILogModule = {
    .id = Y_PPI_LOG_MODULE_ID,
    .name = "ppi",
};

// I/O ports exposed by the PPI.
typedef enum YPPIPort {
  // Keyboard scancode
  Y_PPI_PORT_A = 0x60,
  // System control
  Y_PPI_PORT_B = 0x61,
  // DIP switches
  Y_PPI_PORT_C = 0x62,
  // Control word
  Y_PPI_PORT_CONTROL = 0x63,
} YPPIPort;

// Bit definitions for PPI Port B (Y_PPI_PORT_B).
enum {
  // Bit 0: Timer 2 signal gate (0 = disable, 1 = enable).
  Y_PPI_PORT_B_TIMER2_GATE = (1 << 0),
  // Bit 1: PC speaker enable/disable.
  Y_PPI_PORT_B_SPEAKER_DATA = (1 << 1),
  // Bit 2: Turbo mode (0 = turbo, 1 = normal). Not supported.
  Y_PPI_PORT_B_TURBO_MODE = (1 << 2),
  // Bit 3: DIP switch select (0 = SW1-4, 1 = SW5-8).
  Y_PPI_PORT_B_DIP_SWITCH_SELECT = (1 << 3),
  // Bit 4: Memory parity check enable/disable. Not supported.
  Y_PPI_PORT_B_MEMORY_PARITY_CHECK = (1 << 4),
  // Bit 5: I/O channel check enable/disable. Not supported.
  Y_PPI_PORT_B_IO_CHANNEL_CHECK = (1 << 5),
  // Bit 6: Keyboard clock control (0 = hold low, 1 = enable).
  Y_PPI_PORT_B_KEYBOARD_CLOCK_LOW = (1 << 6),
  // Bit 7: Keyboard enable/clear (0 = enable read, 1 = clear).
  Y_PPI_PORT_B_KEYBOARD_ENABLE_CLEAR = (1 << 7),
};

// Memory sizes defined by DIP switches, corresponding to Port A bits 2-3.
// Note that GLaBIOS in ARCH_TYPE_EMU mode does not actually make use of these
// and instead performs its own memory detection based on the video card type.
typedef enum YPPIMemorySize {
  // 00 = 64KB
  Y_PPI_MEMORY_SIZE_64KB = 0,
  // 01 = 128KB
  Y_PPI_MEMORY_SIZE_128KB = 1,
  // 10 = 192KB
  Y_PPI_MEMORY_SIZE_192KB = 2,
  // 11 = 256KB
  Y_PPI_MEMORY_SIZE_256KB = 3,
} YPPIMemorySize;

// Display mode at boot time, corresponding to Port A bits 4-5.
typedef enum YPPIDisplayMode {
  // 00 = EGA/VGA (ROM)
  Y_PPI_DISPLAY_EGA = 0,
  // 01 = CGA 40x25
  Y_PPI_DISPLAY_CGA_40X25 = 1,
  // 10 = CGA 80x25
  Y_PPI_DISPLAY_CGA_80X25 = 2,
  // 11 = MDA 80x25
  Y_PPI_DISPLAY_MDA = 3,
} YPPIDisplayMode;

struct YPPIState;

// Caller-provided runtime configuration for the PPI.
typedef struct YPPIConfig {
  // Opaque context pointer, passed to all callbacks.
  void* context;

  // Logger for this module. May be NULL.
  YLogger* logger;

  // Number of floppy drives (1-4).
  uint8_t num_floppy_drives;

  // Memory size setting from DIP switches.
  YPPIMemorySize memory_size;

  // Display mode setting from DIP switches.
  YPPIDisplayMode display_mode;

  // Whether FPU is installed.
  bool is_fpu_installed;

  // Callback to control PC speaker.If frequency_hz is 0, the speaker should
  // be turned off. Otherwise, it should be set to the specified frequency.
  void (*set_pc_speaker_frequency)(void* context, uint32_t frequency_hz);

  // Callback when keyboard control bits are modified (bits 6 and 7 of Port B).
  void (*set_keyboard_control)(
      void* context,
      // Port B bit 7
      bool keyboard_enable_clear,
      // Port B bit 6
      bool keyboard_clock_low);
} YPPIConfig;

// State of the PPI.
typedef struct YPPIState {
  // Pointer to the PPI configuration.
  YPPIConfig config;

  // Port A: Keyboard scancode latch.
  uint8_t port_a_latch;

  // Port B: System control register.
  uint8_t port_b;

  // Current frequency of the PC speaker generated by the PIT, in Hz.
  uint32_t pc_speaker_frequency_from_pit;
} YPPIState;

// Initializes the PPI to its power-on state.
Y_PUBLIC void YPPIInit(YPPIState* ppi);

// Handles reads from the PPI's I/O ports (0x60-0x62).
Y_PUBLIC uint8_t YPPIReadPort(YPPIState* ppi, uint16_t port);

// Handles writes to the PPI's I/O ports (0x61, 0x63).
Y_PUBLIC void YPPIWritePort(YPPIState* ppi, uint16_t port, uint8_t value);

// Returns whether the PC speaker is currently enabled. This is determined by
// bit 0 and 1 of Port B.
Y_PUBLIC bool YPPIIsPCSpeakerEnabled(YPPIState* ppi);

// Sets the PC speaker frequency from the 8253 timer channel 2 output. This
// should be wired up to the callback from the PIT emulation module.
Y_PUBLIC void YPPISetPCSpeakerFrequencyFromPIT(
    YPPIState* ppi, uint32_t frequency_hz);

// Sets the scancode byte that will be returned when the CPU reads from Port A.
// This function should be called by the keyboard emulation module.
Y_PUBLIC void YPPISetScancode(YPPIState* ppi, uint8_t scancode);

#endif  // YAX86_PPI_PUBLIC_H


// ==============================================================================
// src/ppi/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/ppi/ppi.c start
// ==============================================================================

#line 1 "./src/ppi/ppi.c"
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
        port_c |= (ppi->config.is_fpu_installed << 1);
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
      bool was_speaker_enabled = YPPIIsPCSpeakerEnabled(ppi);
      uint8_t old_keyboard_control = YPPIGetKeyboardControl(ppi);

      ppi->port_b = value;

      // Bit 7: Keyboard enable/clear (0 = enable read, 1 = clear).
      if (value & Y_PPI_PORT_B_KEYBOARD_ENABLE_CLEAR) {
        ppi->port_a_latch = 0;
      }

      // Check for changes in PC speaker control bits and fire callback.
      bool is_speaker_enabled = YPPIIsPCSpeakerEnabled(ppi);
      if (was_speaker_enabled != is_speaker_enabled &&
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


// ==============================================================================
// src/ppi/ppi.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_PPI_BUNDLE_H

