#include <gtest/gtest.h>

#include "pic.h"

namespace {

// Per the spec, bits for commands.
enum {
  kICW1_INIT = (1 << 4),   // 1 = initialization mode
  kICW1_SNGL = (1 << 1),   // 1 = single PIC, 0 = cascaded
  kOCW2_EOI = (1 << 5),    // End of Interrupt
  kICW2_BASE_XT = 0x08,    // Base for IBM PC/XT
  kICW2_BASE_AT_M = 0x08,  // Base for IBM PC/AT Master
  kICW2_BASE_AT_S = 0x70,  // Base for IBM PC/AT Slave
};

class IRQTest : public ::testing::Test {
 protected:
  void SetUpSinglePIC() {
    master_config_.sp = false;
    PICInit(&master_, &master_config_);

    // ICW1: single PIC, no ICW4 needed.
    PICWritePort(&master_, 0x20, kICW1_INIT | kICW1_SNGL);
    // ICW2: interrupt vector base 0x08.
    PICWritePort(&master_, 0x21, kICW2_BASE_XT);

    ASSERT_EQ(master_.init_state, kPICReady);
    master_.imr = 0x00;  // Unmask all interrupts for testing.
  }

  void SetUpCascadedPICs() {
    // Master PIC setup
    master_config_.sp = false;
    PICInit(&master_, &master_config_);
    PICWritePort(&master_, 0x20, kICW1_INIT);  // Cascaded, ICW4 not needed
    PICWritePort(&master_, 0x21, kICW2_BASE_AT_M);
    PICWritePort(&master_, 0x21, 1 << 2);  // Slave is on IRQ 2
    ASSERT_EQ(master_.init_state, kPICReady);

    // Slave PIC setup
    slave_config_.sp = true;
    PICInit(&slave_, &slave_config_);
    PICWritePort(&slave_, 0xA0, kICW1_INIT);  // Cascaded, ICW4 not needed
    PICWritePort(&slave_, 0xA1, kICW2_BASE_AT_S);
    PICWritePort(&slave_, 0xA1, 2);  // Slave ID is 2
    ASSERT_EQ(slave_.init_state, kPICReady);

    // Link them
    master_.cascade_pic = &slave_;
    slave_.cascade_pic = &master_;

    // Unmask all interrupts for testing.
    master_.imr = 0x00;
    slave_.imr = 0x00;
  }

  PICConfig master_config_ = {0};
  PICState master_ = {0};

  PICConfig slave_config_ = {0};
  PICState slave_ = {0};
};

TEST_F(IRQTest, SinglePIC_BasicIRQ) {
  SetUpSinglePIC();
  PICRaiseIRQ(&master_, 3);

  EXPECT_EQ(master_.irr, 1 << 3);
  EXPECT_EQ(PICGetPendingInterrupt(&master_), kICW2_BASE_XT + 3);
  EXPECT_EQ(master_.irr, 0);       // IRR bit should be cleared after ack.
  EXPECT_EQ(master_.isr, 1 << 3);  // ISR bit should be set.
}

TEST_F(IRQTest, SinglePIC_Priority) {
  SetUpSinglePIC();
  PICRaiseIRQ(&master_, 5);
  PICRaiseIRQ(&master_, 2);

  // IRQ 2 has higher priority (lower number) than IRQ 5.
  EXPECT_EQ(PICGetPendingInterrupt(&master_), kICW2_BASE_XT + 2);
  EXPECT_EQ(master_.isr, 1 << 2);
  EXPECT_EQ(master_.irr, 1 << 5);  // IRQ 5 should still be pending.
}

TEST_F(IRQTest, SinglePIC_Masking) {
  SetUpSinglePIC();
  PICRaiseIRQ(&master_, 4);

  // Mask IRQ 4.
  master_.imr = (1 << 4);
  EXPECT_EQ(PICGetPendingInterrupt(&master_), kPICNoPendingInterrupt);

  // Unmask IRQ 4.
  master_.imr = 0;
  EXPECT_EQ(PICGetPendingInterrupt(&master_), kICW2_BASE_XT + 4);
}

TEST_F(IRQTest, SinglePIC_InServicePriority) {
  SetUpSinglePIC();

  // Service IRQ 5.
  PICRaiseIRQ(&master_, 5);
  EXPECT_EQ(PICGetPendingInterrupt(&master_), kICW2_BASE_XT + 5);
  EXPECT_EQ(master_.isr, 1 << 5);

  // Raise a lower-priority interrupt (IRQ 7). It should not be serviced.
  PICRaiseIRQ(&master_, 7);
  EXPECT_EQ(PICGetPendingInterrupt(&master_), kPICNoPendingInterrupt);

  // Raise a higher-priority interrupt (IRQ 3). It should be serviced.
  PICRaiseIRQ(&master_, 3);
  EXPECT_EQ(PICGetPendingInterrupt(&master_), kICW2_BASE_XT + 3);
  EXPECT_EQ(master_.isr, (1 << 5) | (1 << 3));
}

TEST_F(IRQTest, SinglePIC_EOIInteraction) {
  SetUpSinglePIC();

  // Service IRQ 4.
  PICRaiseIRQ(&master_, 4);
  EXPECT_EQ(PICGetPendingInterrupt(&master_), kICW2_BASE_XT + 4);

  // Raise IRQ 5. It shouldn't be serviced yet.
  PICRaiseIRQ(&master_, 5);
  EXPECT_EQ(PICGetPendingInterrupt(&master_), kPICNoPendingInterrupt);

  // Issue an EOI for IRQ 4.
  PICWritePort(&master_, 0x20, kOCW2_EOI);

  // Now IRQ 5 should be serviced.
  EXPECT_EQ(PICGetPendingInterrupt(&master_), kICW2_BASE_XT + 5);
}

TEST_F(IRQTest, Cascaded_SlaveToMasterTrigger) {
  SetUpCascadedPICs();

  // Raise IRQ 11 (slave IRQ 3).
  PICRaiseIRQ(&slave_, 3);

  // This should be reflected in the slave's IRR.
  EXPECT_EQ(slave_.irr, 1 << 3);
  // This should raise the cascade line (IRQ 2) on the master.
  EXPECT_EQ(master_.irr, 1 << 2);
}

TEST_F(IRQTest, Cascaded_GetSlaveInterrupt) {
  SetUpCascadedPICs();

  // Raise IRQ 11 (slave IRQ 3).
  PICRaiseIRQ(&slave_, 3);

  // Ask the master for the pending interrupt.
  EXPECT_EQ(PICGetPendingInterrupt(&master_), kICW2_BASE_AT_S + 3);

  // Master's ISR should have the cascade bit set.
  EXPECT_EQ(master_.isr, 1 << 2);
  // Slave's ISR should have its interrupt bit set.
  EXPECT_EQ(slave_.isr, 1 << 3);
}

TEST_F(IRQTest, Cascaded_MasterVsSlavePriority) {
  SetUpCascadedPICs();

  PICRaiseIRQ(&master_, 4);  // Master IRQ 4
  PICRaiseIRQ(&slave_, 3);   // Slave IRQ 3 (overall IRQ 11)

  // The slave is on master's IRQ 2. Since 2 < 4, the slave's
  // interrupt has higher priority.
  EXPECT_EQ(PICGetPendingInterrupt(&master_), kICW2_BASE_AT_S + 3);
}

TEST_F(IRQTest, Cascaded_SlaveVsMasterPriority) {
  SetUpCascadedPICs();

  PICRaiseIRQ(&master_, 1);  // Master IRQ 1
  PICRaiseIRQ(&slave_, 3);   // Slave IRQ 3 (overall IRQ 11)

  // The slave is on master's IRQ 2. Since 1 < 2, the master's
  // own interrupt has higher priority.
  EXPECT_EQ(PICGetPendingInterrupt(&master_), kICW2_BASE_AT_M + 1);
}

TEST_F(IRQTest, Cascaded_LowerIRQ) {
  SetUpCascadedPICs();

  // Raise and lower an IRQ on the slave.
  PICRaiseIRQ(&slave_, 5);
  EXPECT_EQ(master_.irr, 1 << 2);  // Master cascade line is up.

  PICLowerIRQ(&slave_, 5);
  EXPECT_EQ(master_.irr, 0);  // Master cascade line is down.
}

}  // namespace
