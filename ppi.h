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
// PC/XT.
//
// It is configured by the BIOS in Mode 0 with the following port setup:
// - Port A (0x60): Input - Used for keyboard scancode data.
// - Port B (0x61): Output - Used for various system control functions.
// - Port C (0x62): Input - Used for reading DIP switch settings.
// - Control Word (0x63): Write-only register to configure the PPI.
//
// Note that we don't implement all features of the 8255, only those needed for
// the IBM PC/XT functionality. For example, we don't support modes other than
// Mode 0, and we don't implement all bits of Port B.

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

struct PPIState;

// Caller-provided runtime configuration for the PPI.
typedef struct PPIConfig {
  // Opaque context pointer, passed to all callbacks.
  void* context;

  // --- Keyboard Interface Callbacks ---

  // Callback to change the keyboard's inhibited state. When 'inhibited' is
  // true, the keyboard should be blocked from sending data. This corresponds to
  // port B, bit 6
  void (*set_keyboard_inhibited)(void* context, bool inhibited);

  // Callback to signal a reset to the keyboard controller. This corresponds to
  // port B, bit 7.
  void (*reset_keyboard)(void* context);

  // --- PIT Interface Callbacks ---

  // Callback to set the gate for PIT channel 2.
  void (*set_pit_gate_2)(void* context, bool level);

  // --- Speaker Interface Callbacks ---

  // Callback to set the speaker data level.
  void (*set_speaker_data)(void* context, bool level);

  // --- DIP Switch Interface ---

  // Callback to read the DIP switch bank.
  // bank = 0 for low switches (SW1), 1 for high switches (SW2)
  uint8_t (*read_dip_switches)(void* context, int bank);
} PPIConfig;

// State of the PPI.
typedef struct PPIState {
  // Pointer to the PPI configuration.
  PPIConfig* config;

  // Port A: Keyboard scancode latch.
  uint8_t port_a_latch;

  // Port B: System control register.
  uint8_t port_b;

} PPIState;

// Initializes the PPI to its power-on state.
void PPIInit(PPIState* ppi, PPIConfig* config);

// Handles reads from the PPI's I/O ports (0x60-0x62).
uint8_t PPIReadPort(PPIState* ppi, uint16_t port);

// Handles writes to the PPI's I/O ports (0x61, 0x63).
void PPIWritePort(PPIState* ppi, uint16_t port, uint8_t value);

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


// ==============================================================================
// src/ppi/ppi.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_PPI_BUNDLE_H

