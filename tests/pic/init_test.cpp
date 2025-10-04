#include <gtest/gtest.h>

#include "pic.h"

namespace {

class ICWTest : public ::testing::Test {
 protected:
  void SetUp() override { PICInit(&pic_, &config_); }

  PICConfig config_ = {0};
  PICState pic_ = {0};
};

TEST_F(ICWTest, InitialState) {
  // After PICInit, the PIC should be in the kPICExpectICW1 state.
  EXPECT_EQ(pic_.init_state, kPICExpectICW1);
  // All interrupts should be masked.
  EXPECT_EQ(pic_.imr, 0xFF);
}

TEST_F(ICWTest, SinglePIC) {
  config_.sp = false;

  // Write ICW1: single PIC, no ICW4.
  PICWritePort(&pic_, 0x20, 0x12);
  EXPECT_EQ(pic_.init_state, kPICExpectICW2);
  EXPECT_EQ(pic_.icw1, 0x12);
  EXPECT_EQ(pic_.imr, 0xFF);  // Should still be masked
  EXPECT_EQ(pic_.irr, 0x00);
  EXPECT_EQ(pic_.isr, 0x00);

  // Write ICW2: interrupt vector base 0x08.
  PICWritePort(&pic_, 0x21, 0x08);
  EXPECT_EQ(pic_.init_state, kPICReady);
  EXPECT_EQ(pic_.icw2, 0x08);
}

TEST_F(ICWTest, SinglePICWithICW4) {
  config_.sp = false;

  // Write ICW1: single PIC, ICW4 needed.
  PICWritePort(&pic_, 0x20, 0x13);
  EXPECT_EQ(pic_.init_state, kPICExpectICW2);
  EXPECT_EQ(pic_.icw1, 0x13);

  // Write ICW2: interrupt vector base 0x08.
  PICWritePort(&pic_, 0x21, 0x08);
  EXPECT_EQ(pic_.init_state, kPICExpectICW4);
  EXPECT_EQ(pic_.icw2, 0x08);

  // Write ICW4.
  PICWritePort(&pic_, 0x21, 0x01);  // 8086/88 mode
  EXPECT_EQ(pic_.init_state, kPICReady);
}

TEST_F(ICWTest, MasterPIC) {
  config_.sp = false;

  // Write ICW1: cascaded, no ICW4.
  PICWritePort(&pic_, 0x20, 0x10);
  EXPECT_EQ(pic_.init_state, kPICExpectICW2);
  EXPECT_EQ(pic_.icw1, 0x10);

  // Write ICW2: interrupt vector base 0x08.
  PICWritePort(&pic_, 0x21, 0x08);
  EXPECT_EQ(pic_.init_state, kPICExpectICW3);
  EXPECT_EQ(pic_.icw2, 0x08);

  // Write ICW3: slave is on IRQ 2.
  PICWritePort(&pic_, 0x21, 1 << 2);
  EXPECT_EQ(pic_.init_state, kPICReady);
  EXPECT_EQ(pic_.icw3, 1 << 2);
}

TEST_F(ICWTest, MasterPICWithICW4) {
  config_.sp = false;

  // Write ICW1: cascaded, ICW4 needed.
  PICWritePort(&pic_, 0x20, 0x11);
  EXPECT_EQ(pic_.init_state, kPICExpectICW2);
  EXPECT_EQ(pic_.icw1, 0x11);

  // Write ICW2: interrupt vector base 0x08.
  PICWritePort(&pic_, 0x21, 0x08);
  EXPECT_EQ(pic_.init_state, kPICExpectICW3);
  EXPECT_EQ(pic_.icw2, 0x08);

  // Write ICW3: slave is on IRQ 2.
  PICWritePort(&pic_, 0x21, 1 << 2);
  EXPECT_EQ(pic_.init_state, kPICExpectICW4);
  EXPECT_EQ(pic_.icw3, 1 << 2);

  // Write ICW4.
  PICWritePort(&pic_, 0x21, 0x01);  // 8086/88 mode
  EXPECT_EQ(pic_.init_state, kPICReady);
}

TEST_F(ICWTest, SlavePIC) {
  config_.sp = true;

  // Write ICW1: cascaded, no ICW4.
  PICWritePort(&pic_, 0xA0, 0x10);
  EXPECT_EQ(pic_.init_state, kPICExpectICW2);
  EXPECT_EQ(pic_.icw1, 0x10);

  // Write ICW2: interrupt vector base 0x70.
  PICWritePort(&pic_, 0xA1, 0x70);
  EXPECT_EQ(pic_.init_state, kPICExpectICW3);
  EXPECT_EQ(pic_.icw2, 0x70);

  // Write ICW3: slave ID is 2.
  PICWritePort(&pic_, 0xA1, 2);
  EXPECT_EQ(pic_.init_state, kPICReady);
  EXPECT_EQ(pic_.icw3, 2);
}

TEST_F(ICWTest, SlavePICWithICW4) {
  config_.sp = true;

  // Write ICW1: cascaded, ICW4 needed.
  PICWritePort(&pic_, 0xA0, 0x11);
  EXPECT_EQ(pic_.init_state, kPICExpectICW2);
  EXPECT_EQ(pic_.icw1, 0x11);

  // Write ICW2: interrupt vector base 0x70.
  PICWritePort(&pic_, 0xA1, 0x70);
  EXPECT_EQ(pic_.init_state, kPICExpectICW3);
  EXPECT_EQ(pic_.icw2, 0x70);

  // Write ICW3: slave ID is 2.
  PICWritePort(&pic_, 0xA1, 2);
  EXPECT_EQ(pic_.init_state, kPICExpectICW4);
  EXPECT_EQ(pic_.icw3, 2);

  // Write ICW4.
  PICWritePort(&pic_, 0xA1, 0x01);  // 8086/88 mode
  EXPECT_EQ(pic_.init_state, kPICReady);
}

}  // namespace
