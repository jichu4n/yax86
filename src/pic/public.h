// Public interface for the PIC (Programmable Interrupt Controller) module.
#ifndef YAX86_PIC_PUBLIC_H
#define YAX86_PIC_PUBLIC_H

// This module emulates the Intel 8259 PIC(s) on the IBM PC series. There are
// two possible configurations:
//
// 1. Single PIC - IBM PC and PC/XT
//    The system has a single PIC at I/O ports 0x20/0x21, handling IRQs 0-7,
//    connected to the CPU.
//
// 2. Cascaded PICs - IBM PC/AT and PS/2
//    The system has a master PIC at I/O ports 0x20/0x21 handling IRQs 0-7,
//    and a slave PIC at I/O ports 0xA0/0xA1 handling IRQs 8-15. The slave PIC
//    is connected to the master's IRQ2 line. Only the master PIC is directly
//    connected to the CPU.
//
// Note that we do not support all features of the 8259 PIC, such as auto EOI,
// rotating priorities, etc., as they are not used by MS-DOS or the IBM PC
// BIOS.

#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// PIC state
// ============================================================================

// The mode of a PIC - single, master, or slave.
typedef enum PICMode {
  // Single PIC on IBM PC and PC/XT
  kPICSingle = 0,
  // Master PIC on IBM PC/AT and PS/2
  kPICMaster,
  // Slave PIC on IBM PC/AT and PS/2
  kPICSlave,
  // Number of PIC modes
  kNumPICModes,
} PICMode;

// Initialization state of a PIC.
typedef enum PICInitState {
  // Uninitialized - waiting for ICW1.
  kPICExpectICW1 = 0,
  // ICW1 received - waiting for ICW2.
  kPICExpectICW2,
  // ICW2 received - waiting for ICW3 (if needed).
  kPICExpectICW3,
  // ICW3 received - waiting for ICW4 (if needed) or fully initialized.
  kPICExpectICW4,
  // Fully initialized.
  kPICReady,
} PICInitState;

enum {
  // Indicates no pending interrupt. In normal operation, valid ranges of
  // interrupt vectors are 0x08-0x0F for a single PIC or master PIC, and
  // 0x70-0x77 for a slave PIC.
  kNoPendingInterrupt = 0xFF,
};

struct PICState;

// Caller-provided runtime configuration.
typedef struct PICConfig {
  // State of the SP pin.
  // - Single PIC on IBM PC and PC/XT => false
  // - Master PIC on IBM PC/AT and PS/2 => false
  // - Slave PIC on IBM PC/AT and PS/2 => true
  bool sp;
} PICConfig;

// The register to read on the next read from the data port.
typedef enum PICReadRegister {
  kPICReadIMR = 0,  // Default: read Interrupt Mask Register
  kPICReadIRR = 1,  // Read Interrupt Request Register on next read
  kPICReadISR = 2,  // Read In-Service Register on next read
} PICReadRegister;

// State of a single 8259 PIC chip.
typedef struct PICState {
  // Pointer to caller-provided runtime configuration.
  PICConfig* config;

  // Initialization state.
  PICInitState init_state;
  // Received initialization words.
  uint8_t icw1;
  uint8_t icw2;
  uint8_t icw3;
  // We don't store ICW4 as its extra features are not used by MS-DOS or the
  // IBM PC BIOS.

  // Interrupt Request Register - pending interrupts. Bit i is set if IRQ i is
  // pending.
  uint8_t irr;
  // In-Service Register - interrupts currently being serviced. Bit i is set if
  // IRQ i is being serviced.
  uint8_t isr;
  // Interrupt Mask Register - masked interrupts. Bit i is set if IRQ i is
  // masked.
  uint8_t imr;

  // The register to read on the next read from the data port.
  PICReadRegister read_register;

  // Pointer to master PIC if this is a slave, or to slave PIC if this is a
  // master. NULL if this is a single PIC.
  struct PICState* cascade_pic;
} PICState;

// ============================================================================
// PIC initialization
// ============================================================================

// Initialize a PIC with the provided configuration.
void PICInit(PICState* pic, PICConfig* config);

// ============================================================================
// IRQ line control
// ============================================================================

// Raise an IRQ line (0-7) on this PIC. If this is a slave PIC, also raises
// the cascade IRQ on the master PIC.
void PICRaiseIRQ(PICState* pic, uint8_t irq);

// Lower an IRQ line (0-7) on this PIC. If this is a slave PIC and no interrupts
// are pending, also lowers the cascade IRQ on the master PIC.
void PICLowerIRQ(PICState* pic, uint8_t irq);

// ============================================================================
// I/O port interface
// ============================================================================

// Read from a PIC I/O port.
// For master PIC: port should be 0x20 (command) or 0x21 (data).
// For slave PIC: port should be 0xA0 (command) or 0xA1 (data).
uint8_t PICReadPort(PICState* pic, uint16_t port);

// Write to a PIC I/O port.
// For master PIC: port should be 0x20 (command) or 0x21 (data).
// For slave PIC: port should be 0xA0 (command) or 0xA1 (data).
void PICWritePort(PICState* pic, uint16_t port, uint8_t value);

// ============================================================================
// Interrupt handling
// ============================================================================

// Get the highest priority pending interrupt vector number from this PIC. If
// this is a master PIC, this will consider pending interrupts from the slave
// PIC as well. If no interrupts are pending, returns kNoPendingInterrupt.
uint8_t PICGetPendingInterrupt(PICState* pic);

#endif  // YAX86_PIC_PUBLIC_H
