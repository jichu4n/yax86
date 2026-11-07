#include <gtest/gtest.h>

#include "pic.h"

namespace {

// Per the spec, bits for commands.
enum {
  kTestICW1_INIT = (1 << 4),  // 1 = initialization mode
  kTestICW1_SNGL = (1 << 1),  // 1 = single PIC, 0 = cascaded
  kTestOCW2_EOI = (1 << 5),   // End of Interrupt
  kICW2_BASE_XT = 0x08,       // Base for IBM PC/XT
  kICW2_BASE_AT_M = 0x08,     // Base for IBM PC/AT Master
  kICW2_BASE_AT_S = 0x70,     // Base for IBM PC/AT Slave
};

class IRQTest : public ::testing::Test {
 protected:
  void SetUpSinglePIC() {
    master_.config.sp = false;
    YPICInit(&master_);

    // ICW1: single PIC, no ICW4 needed.
    YPICWritePort(&master_, 0x20, kTestICW1_INIT | kTestICW1_SNGL);
    // ICW2: interrupt vector base 0x08.
    YPICWritePort(&master_, 0x21, kICW2_BASE_XT);

    ASSERT_EQ(master_.init_state, Y_PIC_READY);
    // Unmask all interrupts for testing. Written as an OCW1 rather than
    // assigned, so that the mask goes through the path a guest uses and
    // has_unmasked_request is recomputed with it.
    YPICWritePort(&master_, 0x21, 0x00);
  }

  void SetUpCascadedPICs() {
    // Master PIC setup
    master_.config.sp = false;
    YPICInit(&master_);
    YPICWritePort(&master_, 0x20, kTestICW1_INIT);  // Cascaded, ICW4 not needed
    YPICWritePort(&master_, 0x21, kICW2_BASE_AT_M);
    YPICWritePort(&master_, 0x21, 1 << 2);  // Slave is on IRQ 2
    ASSERT_EQ(master_.init_state, Y_PIC_READY);

    // Slave PIC setup
    slave_.config.sp = true;
    YPICInit(&slave_);
    YPICWritePort(&slave_, 0xA0, kTestICW1_INIT);  // Cascaded, ICW4 not needed
    YPICWritePort(&slave_, 0xA1, kICW2_BASE_AT_S);
    YPICWritePort(&slave_, 0xA1, 2);  // Slave ID is 2
    ASSERT_EQ(slave_.init_state, Y_PIC_READY);

    // Link them
    master_.cascade_pic = &slave_;
    slave_.cascade_pic = &master_;

    // Unmask all interrupts for testing, through OCW1 as above.
    YPICWritePort(&master_, 0x21, 0x00);
    YPICWritePort(&slave_, 0xA1, 0x00);
  }
  YPICState master_ = {0};
  YPICState slave_ = {0};
};

TEST_F(IRQTest, SinglePIC_BasicIRQ) {
  SetUpSinglePIC();
  YPICRaiseIRQ(&master_, 3);

  EXPECT_EQ(master_.irr, 1 << 3);
  EXPECT_EQ(YPICGetPendingInterrupt(&master_), kICW2_BASE_XT + 3);
  EXPECT_EQ(master_.irr, 0);       // IRR bit should be cleared after ack.
  EXPECT_EQ(master_.isr, 1 << 3);  // ISR bit should be set.
}

TEST_F(IRQTest, SinglePIC_Priority) {
  SetUpSinglePIC();
  YPICRaiseIRQ(&master_, 5);
  YPICRaiseIRQ(&master_, 2);

  // IRQ 2 has higher priority (lower number) than IRQ 5.
  EXPECT_EQ(YPICGetPendingInterrupt(&master_), kICW2_BASE_XT + 2);
  EXPECT_EQ(master_.isr, 1 << 2);
  EXPECT_EQ(master_.irr, 1 << 5);  // IRQ 5 should still be pending.
}

TEST_F(IRQTest, SinglePIC_Masking) {
  SetUpSinglePIC();
  YPICRaiseIRQ(&master_, 4);

  // Mask IRQ 4.
  YPICWritePort(&master_, 0x21, 1 << 4);
  EXPECT_FALSE(master_.has_unmasked_request);
  EXPECT_EQ(YPICGetPendingInterrupt(&master_), Y_PIC_NO_PENDING_INTERRUPT);

  // Unmask IRQ 4.
  YPICWritePort(&master_, 0x21, 0x00);
  EXPECT_TRUE(master_.has_unmasked_request);
  EXPECT_EQ(YPICGetPendingInterrupt(&master_), kICW2_BASE_XT + 4);
}

TEST_F(IRQTest, SinglePIC_InServicePriority) {
  SetUpSinglePIC();

  // Service IRQ 5.
  YPICRaiseIRQ(&master_, 5);
  EXPECT_EQ(YPICGetPendingInterrupt(&master_), kICW2_BASE_XT + 5);
  EXPECT_EQ(master_.isr, 1 << 5);

  // Raise a lower-priority interrupt (IRQ 7). It should not be serviced.
  YPICRaiseIRQ(&master_, 7);
  EXPECT_EQ(YPICGetPendingInterrupt(&master_), Y_PIC_NO_PENDING_INTERRUPT);

  // Raise a higher-priority interrupt (IRQ 3). It should be serviced.
  YPICRaiseIRQ(&master_, 3);
  EXPECT_EQ(YPICGetPendingInterrupt(&master_), kICW2_BASE_XT + 3);
  EXPECT_EQ(master_.isr, (1 << 5) | (1 << 3));
}

TEST_F(IRQTest, SinglePIC_EOIInteraction) {
  SetUpSinglePIC();

  // Service IRQ 4.
  YPICRaiseIRQ(&master_, 4);
  EXPECT_EQ(YPICGetPendingInterrupt(&master_), kICW2_BASE_XT + 4);

  // Raise IRQ 5. It shouldn't be serviced yet.
  YPICRaiseIRQ(&master_, 5);
  EXPECT_EQ(YPICGetPendingInterrupt(&master_), Y_PIC_NO_PENDING_INTERRUPT);

  // Issue an EOI for IRQ 4.
  YPICWritePort(&master_, 0x20, kTestOCW2_EOI);

  // Now IRQ 5 should be serviced.
  EXPECT_EQ(YPICGetPendingInterrupt(&master_), kICW2_BASE_XT + 5);
}

TEST_F(IRQTest, Cascaded_SlaveToMasterTrigger) {
  SetUpCascadedPICs();

  // Raise IRQ 11 (slave IRQ 3).
  YPICRaiseIRQ(&slave_, 3);

  // This should be reflected in the slave's IRR.
  EXPECT_EQ(slave_.irr, 1 << 3);
  // This should raise the cascade line (IRQ 2) on the master.
  EXPECT_EQ(master_.irr, 1 << 2);
}

TEST_F(IRQTest, Cascaded_GetSlaveInterrupt) {
  SetUpCascadedPICs();

  // Raise IRQ 11 (slave IRQ 3).
  YPICRaiseIRQ(&slave_, 3);

  // Ask the master for the pending interrupt.
  EXPECT_EQ(YPICGetPendingInterrupt(&master_), kICW2_BASE_AT_S + 3);

  // Master's ISR should have the cascade bit set.
  EXPECT_EQ(master_.isr, 1 << 2);
  // Slave's ISR should have its interrupt bit set.
  EXPECT_EQ(slave_.isr, 1 << 3);
}

TEST_F(IRQTest, Cascaded_MasterVsSlavePriority) {
  SetUpCascadedPICs();

  YPICRaiseIRQ(&master_, 4);  // Master IRQ 4
  YPICRaiseIRQ(&slave_, 3);   // Slave IRQ 3 (overall IRQ 11)

  // The slave is on master's IRQ 2. Since 2 < 4, the slave's
  // interrupt has higher priority.
  EXPECT_EQ(YPICGetPendingInterrupt(&master_), kICW2_BASE_AT_S + 3);
}

TEST_F(IRQTest, Cascaded_SlaveVsMasterPriority) {
  SetUpCascadedPICs();

  YPICRaiseIRQ(&master_, 1);  // Master IRQ 1
  YPICRaiseIRQ(&slave_, 3);   // Slave IRQ 3 (overall IRQ 11)

  // The slave is on master's IRQ 2. Since 1 < 2, the master's
  // own interrupt has higher priority.
  EXPECT_EQ(YPICGetPendingInterrupt(&master_), kICW2_BASE_AT_M + 1);
}

TEST_F(IRQTest, Cascaded_LowerIRQ) {
  SetUpCascadedPICs();

  // Raise and lower an IRQ on the slave.
  YPICRaiseIRQ(&slave_, 5);
  EXPECT_EQ(master_.irr, 1 << 2);  // Master cascade line is up.

  YPICLowerIRQ(&slave_, 5);
  EXPECT_EQ(master_.irr, 0);  // Master cascade line is down.
}

}  // namespace
