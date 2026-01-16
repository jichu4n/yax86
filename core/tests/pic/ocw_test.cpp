#include "pic.h"

#include <gtest/gtest.h>

namespace {

// Per the spec, bits for OCW commands.
enum {
  // ICW bits
  kICW1_IC4 = (1 << 0),   // 1 = ICW4 needed
  kICW1_SNGL = (1 << 1),  // 1 = single PIC, 0 = cascaded
  kICW1_INIT = (1 << 4),  // 1 = initialization mode

  // OCW bits
  kOCW_SELECT = (1 << 3),  // 1 = OCW3, 0 = OCW2
  kOCW2_EOI = (1 << 5),    // End of Interrupt
  kOCW2_SL = (1 << 6),     // Specific Level
  kOCW3_RR = (1 << 1),     // 1 = Read Register command
  kOCW3_RIS = (1 << 0),    // 1 = Read ISR, 0 = Read IRR
};

class OCWTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Initialize the PIC to a known-ready state (single PIC mode).
    config_.sp = false;
    PICInit(&pic_, &config_);

    // ICW1: single PIC, no ICW4 needed.
    PICWritePort(&pic_, 0x20, kICW1_INIT | kICW1_SNGL);
    // ICW2: interrupt vector base 0x08.
    PICWritePort(&pic_, 0x21, 0x08);

    ASSERT_EQ(pic_.init_state, kPICReady);
  }

  PICConfig config_ = {0};
  PICState pic_ = {0};
};

TEST_F(OCWTest, OCW1_SetIMR) {
  // OCW1 is a write to the data port when the PIC is ready.
  // This should update the Interrupt Mask Register (IMR).
  const uint8_t new_imr = 0b10101010;
  PICWritePort(&pic_, 0x21, new_imr);
  EXPECT_EQ(pic_.imr, new_imr);

  const uint8_t newer_imr = 0b01010101;
  PICWritePort(&pic_, 0x21, newer_imr);
  EXPECT_EQ(pic_.imr, newer_imr);
}

TEST_F(OCWTest, OCW2_NonSpecificEOI) {
  // Set some bits in the In-Service Register.
  pic_.isr = (1 << 2) | (1 << 5) | (1 << 7);

  // Send a non-specific EOI. This should clear the highest-priority
  // (lowest index) ISR bit, which is bit 2.
  PICWritePort(&pic_, 0x20, kOCW2_EOI);
  EXPECT_EQ(pic_.isr, (1 << 5) | (1 << 7));

  // Send another non-specific EOI. This should clear bit 5.
  PICWritePort(&pic_, 0x20, kOCW2_EOI);
  EXPECT_EQ(pic_.isr, (1 << 7));

  // Send a final non-specific EOI. This should clear bit 7.
  PICWritePort(&pic_, 0x20, kOCW2_EOI);
  EXPECT_EQ(pic_.isr, 0x00);
}

TEST_F(OCWTest, OCW2_SpecificEOI) {
  // Set some bits in the In-Service Register.
  pic_.isr = (1 << 2) | (1 << 5) | (1 << 7);

  // Send a specific EOI for IRQ 5.
  PICWritePort(&pic_, 0x20, kOCW2_EOI | kOCW2_SL | 5);
  EXPECT_EQ(pic_.isr, (1 << 2) | (1 << 7));

  // Send a specific EOI for IRQ 7.
  PICWritePort(&pic_, 0x20, kOCW2_EOI | kOCW2_SL | 7);
  EXPECT_EQ(pic_.isr, (1 << 2));

  // Send a specific EOI for IRQ 2.
  PICWritePort(&pic_, 0x20, kOCW2_EOI | kOCW2_SL | 2);
  EXPECT_EQ(pic_.isr, 0x00);
}

TEST_F(OCWTest, OCW3_ReadIRR) {
  // Set IRR and IMR to distinct values.
  pic_.irr = 0xAB;
  pic_.imr = 0xCD;

  // Send OCW3 to select reading the IRR.
  PICWritePort(&pic_, 0x20, kOCW_SELECT | kOCW3_RR);
  EXPECT_EQ(PICReadPort(&pic_, 0x21), 0xAB);

  // The next read should revert to reading the IMR.
  EXPECT_EQ(PICReadPort(&pic_, 0x21), 0xCD);
}

TEST_F(OCWTest, OCW3_ReadISR) {
  // Set ISR and IMR to distinct values.
  pic_.isr = 0xEF;
  pic_.imr = 0x98;

  // Send OCW3 to select reading the ISR.
  PICWritePort(&pic_, 0x20, kOCW_SELECT | kOCW3_RR | kOCW3_RIS);
  EXPECT_EQ(PICReadPort(&pic_, 0x21), 0xEF);

  // The next read should revert to reading the IMR.
  EXPECT_EQ(PICReadPort(&pic_, 0x21), 0x98);
}

}  // namespace
