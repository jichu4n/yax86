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
