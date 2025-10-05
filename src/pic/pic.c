#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Constants
// ============================================================================

enum {
  // ICW bits
  kICW1_IC4 = (1 << 0),   // 1 = ICW4 needed
  kICW1_SNGL = (1 << 1),  // 1 = single PIC, 0 = cascaded
  kICW1_INIT = (1 << 4),  // 1 = initialization mode
  kICW2_BASE = 0xF8,      // Upper 5 bits of ICW2 = the interrupt vector base

  // OCW bits
  kOCW_SELECT = (1 << 3),  // 1 = OCW3, 0 = OCW2
  kOCW2_EOI = (1 << 5),    // End of Interrupt
  kOCW2_SL = (1 << 6),     // Specific Level
  kOCW3_RR = (1 << 1),     // 1 = Read Register command
  kOCW3_RIS = (1 << 0),    // 1 = Read ISR, 0 = Read IRR

  // Master PIC cascade IRQ line
  kMasterCascadeIRQ = 2,
};

// The I/O port of a PIC.
typedef enum PICPort {
  kPICPortCommand = 0,
  kPICPortData = 1,

  // Number of PIC ports.
  kNumPICPorts,
  // Invalid port.
  kPICPortInvalid = -1,
} PICPort;

// Map a PIC mode to its base I/O port.
static const uint16_t kPICBasePorts[kPICNumModes] = {
    0x20,  // kPICSingle
    0x20,  // kPICMaster
    0xA0,  // kPICSlave
};

// ============================================================================
// Helper functions
// ============================================================================

// Returns the mode of a PIC based on its ICWs.
static inline PICMode PICGetMode(PICState* pic) {
  // If SNGL bit set in ICW1, we are single PIC.
  if (pic->icw1 & kICW1_SNGL) {
    return kPICSingle;
  }

  // Otherwise, we are cascaded.
  // If SP pin is set, we are slave; otherwise, master.
  return pic->config->sp ? kPICSlave : kPICMaster;
}

// Returns if the PIC is configured as a single PIC.
static inline bool PICIsSingle(PICState* pic) {
  return PICGetMode(pic) == kPICSingle;
}

// Returns if the PIC is a master PIC.
static inline bool PICIsMaster(PICState* pic) {
  return PICGetMode(pic) == kPICMaster;
}

// Returns if the PIC is a slave PIC.
static inline bool PICIsSlave(PICState* pic) {
  return PICGetMode(pic) == kPICSlave;
}

// Returns the I/O port corresponding to a given port number.
static inline PICPort PICGetPort(PICState* pic, uint16_t port) {
  uint16_t port_offset = port - kPICBasePorts[PICGetMode(pic)];
  if (port_offset >= kNumPICPorts) {
    return kPICPortInvalid;
  }
  return (PICPort)port_offset;
}

// Returns the IRQ number of the parent PIC connected to a slave PIC.
// Only valid if pic is a slave PIC.
static inline uint8_t PICGetCascadeIRQ(PICState* pic) {
  return pic->icw3 & 0x07;
}

// ============================================================================
// PIC initialization
// ============================================================================

void PICInit(PICState* pic, PICConfig* config) {
  // Zero out the PIC state.
  static const PICState zero_pic_state = {0};
  *pic = zero_pic_state;
  pic->config = config;

  // All interrupts masked by default.
  pic->imr = 0xFF;
}

// ============================================================================
// IRQ line control
// ============================================================================

void PICRaiseIRQ(PICState* pic, uint8_t irq) {
  if (irq > 7) {
    return;
  }
  pic->irr |= (1 << irq);

  // If this is a slave PIC, also raise the cascade IRQ on the master.
  if (PICIsSlave(pic) && pic->cascade_pic) {
    PICRaiseIRQ(pic->cascade_pic, PICGetCascadeIRQ(pic));
  }
}

void PICLowerIRQ(PICState* pic, uint8_t irq) {
  if (irq > 7) {
    return;
  }
  pic->irr &= ~(1 << irq);

  // If this is a slave PIC and no interrupts are pending, lower the cascade
  // IRQ on the master.
  if (PICIsSlave(pic) && pic->irr == 0 && pic->cascade_pic) {
    PICLowerIRQ(pic->cascade_pic, PICGetCascadeIRQ(pic));
  }
}

// ============================================================================
// I/O port interface
// ============================================================================

uint8_t PICReadPort(PICState* pic, uint16_t port) {
  PICPort pic_port = PICGetPort(pic, port);
  switch (pic_port) {
    case kPICPortCommand:
      // Reading from the command port is not a defined operation.
      return 0x00;

    case kPICPortData: {
      uint8_t value;
      switch (pic->read_register) {
        case kPICReadIRR:
          value = pic->irr;
          break;
        case kPICReadISR:
          value = pic->isr;
          break;
        default:
          value = pic->imr;
          break;
      }
      pic->read_register = kPICReadIMR;
      return value;
    }

    default:
      // Invalid port.
      return 0x00;
  }
}

void PICWritePort(PICState* pic, uint16_t port, uint8_t value) {
  PICPort pic_port = PICGetPort(pic, port);
  switch (pic_port) {
    case kPICPortCommand:
      if (value & kICW1_INIT) {
        // This is ICW1, which starts the initialization sequence.
        pic->icw1 = value;
        pic->irr = 0x00;
        pic->isr = 0x00;
        // All interrupts masked by default.
        pic->imr = 0xFF;

        // The next write to the data port will be ICW2.
        pic->init_state = kPICExpectICW2;
      } else {
        // This is an OCW (Operational Command Word).
        if (value & kOCW_SELECT) {
          // This is OCW3.
          if (value & kOCW3_RR) {
            // This is a Read Register command.
            pic->read_register = value & kOCW3_RIS ? kPICReadISR : kPICReadIRR;
          } else {
            // Other OCW3 commands (e.g. Special Mask Mode) are not
            // implemented.
          }
        } else {
          // This is OCW2.
          if (value & kOCW2_EOI) {
            if (value & kOCW2_SL) {
              // Specific EOI: clear specified ISR bit.
              uint8_t irq = value & 0x07;
              pic->isr &= ~(1 << irq);
            } else {
              // Non-Specific EOI: clear highest priority ISR bit.
              for (uint8_t i = 0, isr_mask = 1; i < 8; ++i, isr_mask <<= 1) {
                if (pic->isr & isr_mask) {
                  pic->isr &= ~isr_mask;
                  break;
                }
              }
            }
          } else {
            // Other OCW2 commands (Rotate) are not implemented as they are not
            // used by MS-DOS or the IBM PC BIOS.
          }
        }
      }
      break;

    case kPICPortData:
      switch (pic->init_state) {
        case kPICExpectICW2:
          // This is ICW2. It sets the interrupt vector base.
          // The PIC uses the upper 5 bits of this value.
          pic->icw2 = value;
          if (PICIsSingle(pic)) {
            // Single mode -> no ICW3, ICW4 optional depending on ICW1.
            pic->init_state =
                pic->icw1 & kICW1_IC4 ? kPICExpectICW4 : kPICReady;
          } else {
            // Cascaded mode. Expect ICW3 next.
            pic->init_state = kPICExpectICW3;
          }
          break;

        case kPICExpectICW3:
          // This is ICW3.
          // For master, it's a bitmask of slaves.
          // For slave, it's the 3-bit slave ID.
          pic->icw3 = value;
          // ICW4 is optional depending on ICW1.
          pic->init_state = pic->icw1 & kICW1_IC4 ? kPICExpectICW4 : kPICReady;
          break;

        case kPICExpectICW4:
          // This is ICW4.
          pic->init_state = kPICReady;
          break;

        default:
          // This is an OCW1, which sets the IMR.
          pic->imr = value;
          break;
      }
      break;

    default:
      // Invalid port - ignore.
      break;
  }
}

// ============================================================================
// Interrupt handling
// ============================================================================

uint8_t PICGetPendingInterrupt(PICState* pic) {
  // Find highest priority requested and unmasked interrupt.
  uint8_t irr = pic->irr & ~pic->imr;
  if (irr == 0) {
    return kPICNoPendingInterrupt;
  }
  uint8_t pending_irq = 0, pending_irq_mask = 1;
  for (; pending_irq < 8; ++pending_irq, pending_irq_mask <<= 1) {
    if (irr & pending_irq_mask) {
      break;
    }
  }

  // If there is already an interrupt being serviced, the new pending interrupt
  // must have higher priority (lower IRQ number) to be serviced now.
  if (pic->isr > 0) {
    uint8_t in_service_irq = 0, in_service_irq_mask = 1;
    for (; in_service_irq < 8; ++in_service_irq, in_service_irq_mask <<= 1) {
      if (pic->isr & in_service_irq_mask) {
        break;
      }
    }
    if (pending_irq >= in_service_irq) {
      // New interrupt does not have higher priority than in-service interrupt.
      return kPICNoPendingInterrupt;
    }
  }

  // If this is the master PIC and the interrupt is from the slave, return the
  // slave PIC's interrupt vector.
  if (PICIsMaster(pic) && pending_irq == kMasterCascadeIRQ &&
      pic->cascade_pic) {
    uint8_t slave_vector = PICGetPendingInterrupt(pic->cascade_pic);
    if (slave_vector != kPICNoPendingInterrupt) {
      pic->isr |= pending_irq_mask;
    }
    return slave_vector;
  }

  // This is a normal interrupt on this PIC (or it's a slave reporting up).
  pic->isr |= pending_irq_mask;
  pic->irr &= ~pending_irq_mask;

  return (pic->icw2 & kICW2_BASE) + pending_irq;
}

