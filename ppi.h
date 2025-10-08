// ==============================================================================
// YAX86 PPI MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_PPI_BUNDLE_H
#define YAX86_PPI_BUNDLE_H

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

// I/O ports exposed by the PPI.
typedef enum PPIPort {
  // Keyboard scancode
  kPPIPortA = 0x60,
  // System control
  kPPIPortB = 0x61,
  // DIP switches
  kPPIPortC = 0x62,
  // Control word
  kPPIPortControl = 0x63,
} PPIPort;

// Bit definitions for PPI Port B (kPPIPortB).
enum {
  // Bit 0: Timer 2 signal gate (0 = disable, 1 = enable).
  kPPIPortBTimer2Gate = (1 << 0),
  // Bit 1: PC speaker enable/disable.
  kPPIPortBSpeakerData = (1 << 1),
  // Bit 2: Turbo mode (0 = turbo, 1 = normal). Not supported.
  kPPIPortBTurboMode = (1 << 2),
  // Bit 3: DIP switch select (0 = SW1-4, 1 = SW5-8).
  kPPIPortBDipSwitchSelect = (1 << 3),
  // Bit 4: Memory parity check enable/disable. Not supported.
  kPPIPortBMemoryParityCheck = (1 << 4),
  // Bit 5: I/O channel check enable/disable. Not supported.
  kPPIPortBIoChannelCheck = (1 << 5),
  // Bit 6: Keyboard clock control (0 = hold low, 1 = enable).
  kPPIPortBKeyboardClockLow = (1 << 6),
  // Bit 7: Keyboard enable/clear (0 = enable read, 1 = clear).
  kPPIPortBKeyboardEnableClear = (1 << 7),
};

// Memory sizes defined by DIP switches, corresponding to Port A bits 2-3.
// Note that GLaBIOS in ARCH_TYPE_EMU mode does not actually make use of these
// and instead performs its own memory detection based on the video card type.
typedef enum PPIMemorySize {
  kPPIMemorySize64KB = 0,   // 00
  kPPIMemorySize128KB = 1,  // 01
  kPPIMemorySize192KB = 2,  // 10
  kPPIMemorySize256KB = 3,  // 11
} PPIMemorySize;

// Display mode at boot time, corresponding to Port A bits 4-5.
typedef enum PPIDisplayMode {
  kPPIDisplayEGA = 0,       // 00: EGA/VGA
  kPPIDisplayCGA40x25 = 1,  // 01: CGA 40x25
  kPPIDisplayCGA80x25 = 2,  // 10: CGA 80x25
  kPPIDisplayMDA = 3,       // 11: MDA 80x25
} PPIDisplayMode;

struct PPIState;

// Caller-provided runtime configuration for the PPI.
typedef struct PPIConfig {
  // Opaque context pointer, passed to all callbacks.
  void* context;

  // Number of floppy drives (1-4).
  uint8_t num_floppy_drives;

  // Memory size setting from DIP switches.
  PPIMemorySize memory_size;

  // Display mode setting from DIP switches.
  PPIDisplayMode display_mode;

  // Whether FPU is installed.
  bool fpu_installed;

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
} PPIConfig;

// State of the PPI.
typedef struct PPIState {
  // Pointer to the PPI configuration.
  PPIConfig* config;

  // Port A: Keyboard scancode latch.
  uint8_t port_a_latch;

  // Port B: System control register.
  uint8_t port_b;

  // Current frequency of the PC speaker generated by the PIT, in Hz.
  uint32_t pc_speaker_frequency_from_pit;
} PPIState;

// Initializes the PPI to its power-on state.
void PPIInit(PPIState* ppi, PPIConfig* config);

// Handles reads from the PPI's I/O ports (0x60-0x62).
uint8_t PPIReadPort(PPIState* ppi, uint16_t port);

// Handles writes to the PPI's I/O ports (0x61, 0x63).
void PPIWritePort(PPIState* ppi, uint16_t port, uint8_t value);

// Returns whether the PC speaker is currently enabled. This is determined by
// bit 0 and 1 of Port B.
bool PPIIsPCSpeakerEnabled(PPIState* ppi);

// Sets the PC speaker frequency from the 8253 timer channel 2 output. This
// should be wired up to the callback from the PIT emulation module.
void PPISetPCSpeakerFrequencyFromPIT(PPIState* ppi, uint32_t frequency_hz);

// Sets the scancode byte that will be returned when the CPU reads from Port A.
// This function should be called by the keyboard emulation module.
void PPISetScancode(PPIState* ppi, uint8_t scancode);

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

void PPIInit(PPIState* ppi, PPIConfig* config) {
  static const PPIState zero_ppi_state = {0};
  *ppi = zero_ppi_state;
  ppi->config = config;
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


// ==============================================================================
// src/ppi/ppi.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_PPI_BUNDLE_H

