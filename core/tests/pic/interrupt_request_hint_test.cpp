#include <gtest/gtest.h>

#include "pic.h"

namespace {

enum {
  kTestICW1_INIT = (1 << 4),
  kTestICW1_SNGL = (1 << 1),
  kTestOCW2_EOI = (1 << 5),
  kICW2_BASE_XT = 0x08,

  kCommandPort = 0x20,
  kDataPort = 0x21,
};

// YPICState.has_unmasked_request is a cached derivation of irr & ~imr that a
// CPU reads instead of running an acknowledge cycle it almost never needs. A
// stale one is invisible in the registers and would strand an interrupt, so
// these check it against the registers after every operation that can change
// either.
class InterruptRequestHintTest : public ::testing::Test {
 protected:
  void SetUp() override {
    pic_.config.sp = false;
    YPICInit(&pic_);
    YPICWritePort(&pic_, kCommandPort, kTestICW1_INIT | kTestICW1_SNGL);
    YPICWritePort(&pic_, kDataPort, kICW2_BASE_XT);
    ASSERT_EQ(pic_.init_state, kPICReady);
  }

  // What the flag is a cache of. Every check below compares the two rather
  // than asserting a literal, so a case added later cannot pin the wrong one.
  void ExpectHintMatchesRegisters() {
    EXPECT_EQ(pic_.has_unmasked_request, (pic_.irr & ~pic_.imr) != 0)
        << "irr " << (int)pic_.irr << " imr " << (int)pic_.imr;
  }
  YPICState pic_ = {0};
};

// A PIC out of reset has everything masked, so nothing can be requested.
TEST_F(InterruptRequestHintTest, StartsClear) {
  YPICState fresh = {0};
  YPICInit(&fresh);
  EXPECT_FALSE(fresh.has_unmasked_request);
  EXPECT_EQ(fresh.imr, 0xFF);
}

TEST_F(InterruptRequestHintTest, RaisingAnUnmaskedIRQSetsIt) {
  YPICWritePort(&pic_, kDataPort, 0x00);
  ExpectHintMatchesRegisters();

  YPICRaiseIRQ(&pic_, 3);
  EXPECT_TRUE(pic_.has_unmasked_request);
  ExpectHintMatchesRegisters();
}

// The flag says a request exists, not that it would be taken - so a masked
// request leaves it false, and unmasking alone is enough to set it.
TEST_F(InterruptRequestHintTest, MaskingAndUnmaskingTrackTheRequest) {
  YPICWritePort(&pic_, kDataPort, 0xFF);
  YPICRaiseIRQ(&pic_, 4);
  EXPECT_FALSE(pic_.has_unmasked_request);
  ExpectHintMatchesRegisters();

  YPICWritePort(&pic_, kDataPort, (uint8_t)~(1 << 4));
  EXPECT_TRUE(pic_.has_unmasked_request);
  ExpectHintMatchesRegisters();

  YPICWritePort(&pic_, kDataPort, 0xFF);
  EXPECT_FALSE(pic_.has_unmasked_request);
  ExpectHintMatchesRegisters();
}

TEST_F(InterruptRequestHintTest, LoweringAnIRQClearsIt) {
  YPICWritePort(&pic_, kDataPort, 0x00);
  YPICRaiseIRQ(&pic_, 5);
  ASSERT_TRUE(pic_.has_unmasked_request);

  YPICLowerIRQ(&pic_, 5);
  EXPECT_FALSE(pic_.has_unmasked_request);
  ExpectHintMatchesRegisters();
}

// Acknowledging is what clears the request, so the flag has to fall with it -
// otherwise the CPU would keep running acknowledge cycles that report nothing.
TEST_F(InterruptRequestHintTest, AcknowledgingClearsIt) {
  YPICWritePort(&pic_, kDataPort, 0x00);
  YPICRaiseIRQ(&pic_, 3);

  EXPECT_EQ(YPICGetPendingInterrupt(&pic_), kICW2_BASE_XT + 3);
  EXPECT_FALSE(pic_.has_unmasked_request);
  ExpectHintMatchesRegisters();
}

// One of two requests acknowledged leaves the other outstanding, so the flag
// stays set even though the first is now in service.
TEST_F(InterruptRequestHintTest, StaysSetWhileAnotherRequestRemains) {
  YPICWritePort(&pic_, kDataPort, 0x00);
  YPICRaiseIRQ(&pic_, 2);
  YPICRaiseIRQ(&pic_, 5);

  EXPECT_EQ(YPICGetPendingInterrupt(&pic_), kICW2_BASE_XT + 2);
  EXPECT_TRUE(pic_.has_unmasked_request);
  ExpectHintMatchesRegisters();

  YPICWritePort(&pic_, kCommandPort, kTestOCW2_EOI);
  EXPECT_EQ(YPICGetPendingInterrupt(&pic_), kICW2_BASE_XT + 5);
  EXPECT_FALSE(pic_.has_unmasked_request);
  ExpectHintMatchesRegisters();
}

// A request the in-service register outranks is still a request. The flag is
// allowed to be true where YPICGetPendingInterrupt() declines - it may only
// never be falsely false - and this is the case where that happens.
TEST_F(InterruptRequestHintTest, SetWhileALowerPriorityRequestIsHeldOff) {
  YPICWritePort(&pic_, kDataPort, 0x00);
  YPICRaiseIRQ(&pic_, 1);
  ASSERT_EQ(YPICGetPendingInterrupt(&pic_), kICW2_BASE_XT + 1);

  YPICRaiseIRQ(&pic_, 6);
  EXPECT_EQ(YPICGetPendingInterrupt(&pic_), kPICNoPendingInterrupt);
  EXPECT_TRUE(pic_.has_unmasked_request);
  ExpectHintMatchesRegisters();
}

// Re-initializing clears the request register and masks everything, so a
// request outstanding across it must not survive in the flag.
TEST_F(InterruptRequestHintTest, ReinitializingClearsIt) {
  YPICWritePort(&pic_, kDataPort, 0x00);
  YPICRaiseIRQ(&pic_, 7);
  ASSERT_TRUE(pic_.has_unmasked_request);

  YPICWritePort(&pic_, kCommandPort, kTestICW1_INIT | kTestICW1_SNGL);
  EXPECT_FALSE(pic_.has_unmasked_request);
  ExpectHintMatchesRegisters();
}

// A slave's request reaches the CPU as the master's cascade line, and the CPU
// only ever reads the master's flag - so the master's has to rise with it.
TEST_F(InterruptRequestHintTest, ASlaveRequestSetsTheMastersFlag) {
  YPICState slave = {0};
  slave.config.sp = true;
  YPICInit(&slave);
  YPICWritePort(&slave, 0xA0, kTestICW1_INIT);
  YPICWritePort(&slave, 0xA1, 0x70);
  YPICWritePort(&slave, 0xA1, 2);
  ASSERT_EQ(slave.init_state, kPICReady);

  YPICWritePort(&pic_, kCommandPort, kTestICW1_INIT);
  YPICWritePort(&pic_, kDataPort, kICW2_BASE_XT);
  YPICWritePort(&pic_, kDataPort, 1 << 2);
  ASSERT_EQ(pic_.init_state, kPICReady);
  YPICWritePort(&pic_, kDataPort, 0x00);
  YPICWritePort(&slave, 0xA1, 0x00);
  pic_.cascade_pic = &slave;
  slave.cascade_pic = &pic_;

  YPICRaiseIRQ(&slave, 3);
  EXPECT_TRUE(slave.has_unmasked_request);
  EXPECT_TRUE(pic_.has_unmasked_request);

  YPICLowerIRQ(&slave, 3);
  EXPECT_FALSE(slave.has_unmasked_request);
  EXPECT_FALSE(pic_.has_unmasked_request);
}

}  // namespace
