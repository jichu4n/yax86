// ==============================================================================
// YAX86 PIC MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_PIC_BUNDLE_H
#define YAX86_PIC_BUNDLE_H

#ifndef YAX86_UTIL_BUNDLE_H
#include "util.h"
#endif  // YAX86_UTIL_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/pic/public.h start
// ==============================================================================

#line 1 "./src/pic/public.h"
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

#ifndef YAX86_PIC_BUNDLE_H
#include "../util/log.h"
#endif  // YAX86_PIC_BUNDLE_H

enum {
  // Log module ID for the PIC.
  Y_PIC_LOG_MODULE_ID = 2,
};

// Log module for the PIC.
Y_PUBLIC_HEADER const YLogModule YPICLogModule = {
    .id = Y_PIC_LOG_MODULE_ID,
    .name = "pic",
};

// ============================================================================
// PIC state
// ============================================================================

// The mode of a PIC - single, master, or slave.
typedef enum YPICMode {
  // Single PIC on IBM PC and PC/XT
  Y_PIC_SINGLE = 0,
  // Master PIC on IBM PC/AT and PS/2
  Y_PIC_MASTER,
  // Slave PIC on IBM PC/AT and PS/2
  Y_PIC_SLAVE,
  // Number of PIC modes
  Y_PIC_NUM_MODES,
} YPICMode;

// Initialization state of a PIC.
typedef enum YPICInitState {
  // Uninitialized - waiting for ICW1.
  Y_PIC_EXPECT_ICW1 = 0,
  // ICW1 received - waiting for ICW2.
  Y_PIC_EXPECT_ICW2,
  // ICW2 received - waiting for ICW3 (if needed).
  Y_PIC_EXPECT_ICW3,
  // ICW3 received - waiting for ICW4 (if needed) or fully initialized.
  Y_PIC_EXPECT_ICW4,
  // Fully initialized.
  Y_PIC_READY,
} YPICInitState;

enum {
  // Indicates no pending interrupt. In normal operation, valid ranges of
  // interrupt vectors are 0x08-0x0F for a single PIC or master PIC, and
  // 0x70-0x77 for a slave PIC.
  Y_PIC_NO_PENDING_INTERRUPT = 0xFF,
};

struct YPICState;

// Caller-provided runtime configuration.
typedef struct YPICConfig {
  // Logger for this module. May be NULL.
  YLogger* logger;

  // State of the SP pin.
  // - Single PIC on IBM PC and PC/XT => false
  // - Master PIC on IBM PC/AT and PS/2 => false
  // - Slave PIC on IBM PC/AT and PS/2 => true
  bool sp;
} YPICConfig;

// The register to read on the next read from the data port.
typedef enum YPICReadRegister {
  Y_PIC_READ_IMR = 0,  // Default: read Interrupt Mask Register
  Y_PIC_READ_IRR = 1,  // Read Interrupt Request Register on next read
  Y_PIC_READ_ISR = 2,  // Read In-Service Register on next read
} YPICReadRegister;

// State of a single 8259 PIC chip.
typedef struct YPICState {
  // Pointer to caller-provided runtime configuration.
  YPICConfig config;

  // Initialization state.
  YPICInitState init_state;
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

  // Whether any requested interrupt is unmasked - that is, whether irr & ~imr
  // is non-zero. Recomputed by YPICUpdateUnmaskedRequest(), which is the sole
  // writer, wherever irr or imr changes.
  //
  // This exists so that a CPU can tell in a load whether asking for an
  // interrupt could produce one. It asks on every instruction it runs with
  // interrupts enabled, which is nearly all of them, and the answer is almost
  // always no.
  //
  // It is deliberately conservative rather than exact: it ignores priority and
  // the in-service register, so it can be true where YPICGetPendingInterrupt()
  // would still decline. That costs only a call that reports nothing. What it
  // may never be is falsely false, because a reader takes it as permission not
  // to ask at all.
  bool has_unmasked_request;

  // The register to read on the next read from the data port.
  YPICReadRegister read_register;

  // Pointer to master PIC if this is a slave, or to slave PIC if this is a
  // master. NULL if this is a single PIC.
  struct YPICState* cascade_pic;
} YPICState;

// ============================================================================
// PIC initialization
// ============================================================================

// Initialize a PIC with the provided configuration.
Y_PUBLIC void YPICInit(YPICState* pic);

// ============================================================================
// IRQ line control
// ============================================================================

// Raise an IRQ line (0-7) on this PIC. If this is a slave PIC, also raises
// the cascade IRQ on the master PIC.
Y_PUBLIC void YPICRaiseIRQ(YPICState* pic, uint8_t irq);

// Lower an IRQ line (0-7) on this PIC. If this is a slave PIC and no interrupts
// are pending, also lowers the cascade IRQ on the master PIC.
Y_PUBLIC void YPICLowerIRQ(YPICState* pic, uint8_t irq);

// ============================================================================
// I/O port interface
// ============================================================================

// Read from a PIC I/O port.
// For master PIC: port should be 0x20 (command) or 0x21 (data).
// For slave PIC: port should be 0xA0 (command) or 0xA1 (data).
Y_PUBLIC uint8_t YPICReadPort(YPICState* pic, uint16_t port);

// Write to a PIC I/O port.
// For master PIC: port should be 0x20 (command) or 0x21 (data).
// For slave PIC: port should be 0xA0 (command) or 0xA1 (data).
Y_PUBLIC void YPICWritePort(YPICState* pic, uint16_t port, uint8_t value);

// ============================================================================
// Interrupt handling
// ============================================================================

// Get the highest priority pending interrupt vector number from this PIC. If
// this is a master PIC, this will consider pending interrupts from the slave
// PIC as well. If no interrupts are pending, returns
// Y_PIC_NO_PENDING_INTERRUPT.
Y_PUBLIC uint8_t YPICGetPendingInterrupt(YPICState* pic);

#endif  // YAX86_PIC_PUBLIC_H


// ==============================================================================
// src/pic/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/pic/pic.c start
// ==============================================================================

#line 1 "./src/pic/pic.c"
#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

#define Y_PIC_ERROR(...) \
  Y_LOG(pic->config.logger, &YPICLogModule, Y_LOG_LEVEL_ERROR, __VA_ARGS__)
#define Y_PIC_WARN(...) \
  Y_LOG(pic->config.logger, &YPICLogModule, Y_LOG_LEVEL_WARN, __VA_ARGS__)
#define Y_PIC_INFO(...) \
  Y_LOG(pic->config.logger, &YPICLogModule, Y_LOG_LEVEL_INFO, __VA_ARGS__)
#define Y_PIC_DEBUG(...) \
  Y_LOG(pic->config.logger, &YPICLogModule, Y_LOG_LEVEL_DEBUG, __VA_ARGS__)

// ============================================================================
// Constants
// ============================================================================

enum {
  // ICW bits
  Y_PIC_ICW1_IC4 = (1 << 0),   // 1 = ICW4 needed
  Y_PIC_ICW1_SNGL = (1 << 1),  // 1 = single PIC, 0 = cascaded
  Y_PIC_ICW1_INIT = (1 << 4),  // 1 = initialization mode
  Y_PIC_ICW2_BASE = 0xF8,  // Upper 5 bits of ICW2 = the interrupt vector base

  // OCW bits
  Y_PIC_OCW_SELECT = (1 << 3),  // 1 = OCW3, 0 = OCW2
  Y_PIC_OCW2_EOI = (1 << 5),    // End of Interrupt
  Y_PIC_OCW2_SL = (1 << 6),     // Specific Level
  Y_PIC_OCW3_RR = (1 << 1),     // 1 = Read Register command
  Y_PIC_OCW3_RIS = (1 << 0),    // 1 = Read ISR, 0 = Read IRR

  // Master PIC cascade IRQ line
  Y_PIC_MASTER_CASCADE_IRQ = 2,
};

// The I/O port of a PIC.
typedef enum YPICPort {
  Y_PIC_PORT_COMMAND = 0,
  Y_PIC_PORT_DATA = 1,

  // Number of PIC ports.
  Y_PIC_NUM_PORTS,
  // Invalid port.
  Y_PIC_PORT_INVALID = -1,
} YPICPort;

// Map a PIC mode to its base I/O port.
Y_FILE_PRIVATE const uint16_t YPICBasePorts[Y_PIC_NUM_MODES] = {
    0x20,  // Y_PIC_SINGLE
    0x20,  // Y_PIC_MASTER
    0xA0,  // Y_PIC_SLAVE
};

// ============================================================================
// Helper functions
// ============================================================================

// Returns the mode of a PIC based on its ICWs.
Y_FILE_PRIVATE inline YPICMode YPICGetMode(YPICState* pic) {
  // If SNGL bit set in ICW1, we are single PIC.
  if (pic->icw1 & Y_PIC_ICW1_SNGL) {
    return Y_PIC_SINGLE;
  }

  // Otherwise, we are cascaded.
  // If SP pin is set, we are slave; otherwise, master.
  return pic->config.sp ? Y_PIC_SLAVE : Y_PIC_MASTER;
}

// Returns if the PIC is configured as a single PIC.
Y_FILE_PRIVATE inline bool YPICIsSingle(YPICState* pic) {
  return YPICGetMode(pic) == Y_PIC_SINGLE;
}

// Returns if the PIC is a master PIC.
Y_FILE_PRIVATE inline bool YPICIsMaster(YPICState* pic) {
  return YPICGetMode(pic) == Y_PIC_MASTER;
}

// Returns if the PIC is a slave PIC.
Y_FILE_PRIVATE inline bool YPICIsSlave(YPICState* pic) {
  return YPICGetMode(pic) == Y_PIC_SLAVE;
}

// Returns the I/O port corresponding to a given port number.
Y_FILE_PRIVATE inline YPICPort YPICGetPort(YPICState* pic, uint16_t port) {
  uint16_t port_offset = port - YPICBasePorts[YPICGetMode(pic)];
  if (port_offset >= Y_PIC_NUM_PORTS) {
    return Y_PIC_PORT_INVALID;
  }
  return (YPICPort)port_offset;
}

// Returns the IRQ number of the parent PIC connected to a slave PIC.
// Only valid if pic is a slave PIC.
Y_FILE_PRIVATE inline uint8_t YPICGetCascadeIRQ(YPICState* pic) {
  return pic->icw3 & 0x07;
}

// ============================================================================
// PIC initialization
// ============================================================================

// Recomputes YPICState.has_unmasked_request. This is its sole writer, and every
// path that changes irr or imr calls it, so the flag can never disagree with
// the registers it summarizes.
Y_FILE_PRIVATE inline void YPICUpdateUnmaskedRequest(YPICState* pic) {
  pic->has_unmasked_request = (pic->irr & ~pic->imr) != 0;
}

Y_PUBLIC void YPICInit(YPICState* pic) {
  // All interrupts masked by default.
  pic->imr = 0xFF;
  YPICUpdateUnmaskedRequest(pic);
}

// ============================================================================
// IRQ line control
// ============================================================================

Y_PUBLIC void YPICRaiseIRQ(YPICState* pic, uint8_t irq) {
  if (irq > 7) {
    Y_PIC_WARN("irq_out_of_range", Y_LOG_UINT("irq", irq));
    return;
  }
  Y_PIC_DEBUG(
      "irq_raised", Y_LOG_UINT("irq", irq), Y_LOG_HEX8("imr", pic->imr),
      Y_LOG_HEX8("isr", pic->isr));
  pic->irr |= (1 << irq);
  YPICUpdateUnmaskedRequest(pic);

  // If this is a slave PIC, also raise the cascade IRQ on the master.
  if (YPICIsSlave(pic) && pic->cascade_pic) {
    YPICRaiseIRQ(pic->cascade_pic, YPICGetCascadeIRQ(pic));
  }
}

Y_PUBLIC void YPICLowerIRQ(YPICState* pic, uint8_t irq) {
  if (irq > 7) {
    return;
  }
  pic->irr &= ~(1 << irq);
  YPICUpdateUnmaskedRequest(pic);

  // If this is a slave PIC and no interrupts are pending, lower the cascade
  // IRQ on the master.
  if (YPICIsSlave(pic) && pic->irr == 0 && pic->cascade_pic) {
    YPICLowerIRQ(pic->cascade_pic, YPICGetCascadeIRQ(pic));
  }
}

// ============================================================================
// I/O port interface
// ============================================================================

Y_PUBLIC uint8_t YPICReadPort(YPICState* pic, uint16_t port) {
  YPICPort pic_port = YPICGetPort(pic, port);
  switch (pic_port) {
    case Y_PIC_PORT_COMMAND:
      // Reading from the command port is not a defined operation.
      return 0x00;

    case Y_PIC_PORT_DATA: {
      uint8_t value;
      switch (pic->read_register) {
        case Y_PIC_READ_IRR:
          value = pic->irr;
          break;
        case Y_PIC_READ_ISR:
          value = pic->isr;
          break;
        default:
          value = pic->imr;
          break;
      }
      pic->read_register = Y_PIC_READ_IMR;
      return value;
    }

    default:
      // Invalid port.
      return 0x00;
  }
}

Y_PUBLIC void YPICWritePort(YPICState* pic, uint16_t port, uint8_t value) {
  YPICPort pic_port = YPICGetPort(pic, port);
  switch (pic_port) {
    case Y_PIC_PORT_COMMAND:
      if (value & Y_PIC_ICW1_INIT) {
        // This is ICW1, which starts the initialization sequence.
        pic->icw1 = value;
        pic->irr = 0x00;
        pic->isr = 0x00;
        // All interrupts masked by default.
        pic->imr = 0xFF;
        YPICUpdateUnmaskedRequest(pic);

        // The next write to the data port will be ICW2.
        pic->init_state = Y_PIC_EXPECT_ICW2;
      } else {
        // This is an OCW (Operational Command Word).
        if (value & Y_PIC_OCW_SELECT) {
          // This is OCW3.
          if (value & Y_PIC_OCW3_RR) {
            // This is a Read Register command.
            pic->read_register =
                value & Y_PIC_OCW3_RIS ? Y_PIC_READ_ISR : Y_PIC_READ_IRR;
          } else {
            // Other OCW3 commands (e.g. Special Mask Mode) are not
            // implemented.
          }
        } else {
          // This is OCW2.
          if (value & Y_PIC_OCW2_EOI) {
            if (value & Y_PIC_OCW2_SL) {
              // Specific EOI: clear specified ISR bit.
              uint8_t irq = value & 0x07;
              Y_PIC_DEBUG("specific_eoi", Y_LOG_UINT("irq", irq));
              pic->isr &= ~(1 << irq);
            } else {
              // Non-Specific EOI: clear highest priority ISR bit.
              for (uint8_t i = 0, isr_mask = 1; i < 8; ++i, isr_mask <<= 1) {
                if (pic->isr & isr_mask) {
                  Y_PIC_DEBUG("non_specific_eoi", Y_LOG_UINT("irq", i));
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

    case Y_PIC_PORT_DATA:
      switch (pic->init_state) {
        case Y_PIC_EXPECT_ICW2:
          // This is ICW2. It sets the interrupt vector base.
          // The PIC uses the upper 5 bits of this value.
          pic->icw2 = value;
          if (YPICIsSingle(pic)) {
            // Single mode -> no ICW3, ICW4 optional depending on ICW1.
            pic->init_state =
                pic->icw1 & Y_PIC_ICW1_IC4 ? Y_PIC_EXPECT_ICW4 : Y_PIC_READY;
          } else {
            // Cascaded mode. Expect ICW3 next.
            pic->init_state = Y_PIC_EXPECT_ICW3;
          }
          break;

        case Y_PIC_EXPECT_ICW3:
          // This is ICW3.
          // For master, it's a bitmask of slaves.
          // For slave, it's the 3-bit slave ID.
          pic->icw3 = value;
          // ICW4 is optional depending on ICW1.
          pic->init_state =
              pic->icw1 & Y_PIC_ICW1_IC4 ? Y_PIC_EXPECT_ICW4 : Y_PIC_READY;
          break;

        case Y_PIC_EXPECT_ICW4:
          // This is ICW4.
          pic->init_state = Y_PIC_READY;
          break;

        default:
          // This is an OCW1, which sets the IMR.
          pic->imr = value;
          YPICUpdateUnmaskedRequest(pic);
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

Y_PUBLIC Y_HOT uint8_t YPICGetPendingInterrupt(YPICState* pic) {
  // Find highest priority requested and unmasked interrupt.
  uint8_t irr = pic->irr & ~pic->imr;
  if (irr == 0) {
    return Y_PIC_NO_PENDING_INTERRUPT;
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
      return Y_PIC_NO_PENDING_INTERRUPT;
    }
  }

  // If this is the master PIC and the interrupt is from the slave, return the
  // slave PIC's interrupt vector.
  if (YPICIsMaster(pic) && pending_irq == Y_PIC_MASTER_CASCADE_IRQ &&
      pic->cascade_pic) {
    uint8_t slave_vector = YPICGetPendingInterrupt(pic->cascade_pic);
    if (slave_vector != Y_PIC_NO_PENDING_INTERRUPT) {
      pic->isr |= pending_irq_mask;
    }
    return slave_vector;
  }

  // This is a normal interrupt on this PIC (or it's a slave reporting up).
  pic->isr |= pending_irq_mask;
  pic->irr &= ~pending_irq_mask;
  YPICUpdateUnmaskedRequest(pic);

  return (pic->icw2 & Y_PIC_ICW2_BASE) + pending_irq;
}


// ==============================================================================
// src/pic/pic.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_PIC_BUNDLE_H

