#include <gtest/gtest.h>

#include "./test_helpers.h"
#include "cpu.h"

using namespace std;

class FlagsTest : public ::testing::Test {};

TEST_F(FlagsTest, PushPopFlag) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-push-pop-flag-test",
      "pushf\n"   // Push flags onto the stack
      "popf\n");  // Pop from the stack into flags
  helper->cpu_.registers[Y_CPU_SS] = 0;
  helper->cpu_.registers[Y_CPU_SP] = helper->memory_size_ - 2;
  const uint16_t saved_flags =
      Y_CPU_INITIAL_FLAGS | Y_CPU_CF | Y_CPU_ZF | Y_CPU_DF;
  helper->cpu_.flags = saved_flags;

  helper->ExecuteInstructions(1);
  helper->cpu_.flags = Y_CPU_INITIAL_FLAGS | Y_CPU_SF;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.flags, saved_flags);
}

// The bits of the flags register that are not flags cannot be changed, so a
// POPF of a word that has them the other way round still reads back the only
// way they can.
TEST_F(FlagsTest, PopFlagCannotChangeTheBitsThatAreNotFlags) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-pop-flag-reserved-test", "popf\n");
  helper->cpu_.registers[Y_CPU_SS] = 0;
  helper->cpu_.registers[Y_CPU_SP] = helper->memory_size_ - 2;
  // Every bit that is not a flag set the wrong way: bit 1 and bits 12 to 15
  // clear, bits 3 and 5 set.
  helper->memory_[helper->memory_size_ - 2] = Y_CPU_CF | 0x28;
  helper->memory_[helper->memory_size_ - 1] = 0x00;

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.flags, Y_CPU_INITIAL_FLAGS | Y_CPU_CF);
}

TEST_F(FlagsTest, LAHFAndSAHF) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-lahf-sahf-test",
      "lahf\n"
      "sahf\n");
  const uint16_t flags = Y_CPU_INITIAL_FLAGS | Y_CPU_CF | Y_CPU_PF | Y_CPU_AF;
  helper->cpu_.flags = flags;
  helper->cpu_.registers[Y_CPU_AX] = 0;
  helper->ExecuteInstructions(1);
  // AH takes the low byte of the flags register as it stands.
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_AX], (flags & 0x00FF) << 8);
  // Store back a low byte with the sign and zero flags set instead.
  helper->cpu_.registers[Y_CPU_AX] = (Y_CPU_SF | Y_CPU_ZF) << 8;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.flags, Y_CPU_INITIAL_FLAGS | Y_CPU_SF | Y_CPU_ZF);
}

TEST_F(FlagsTest, ClearCarryFlag) {
  auto helper = CPUTestHelper::CreateWithProgram("execute-clc-test", "clc\n");
  // Set initial flags: CF set, others according to Y_CPU_INITIAL_FLAGS
  helper->cpu_.flags = Y_CPU_INITIAL_FLAGS | Y_CPU_CF;
  helper->ExecuteInstructions(1);
  // Expect CF to be cleared, others remain Y_CPU_INITIAL_FLAGS
  EXPECT_EQ(helper->cpu_.flags, Y_CPU_INITIAL_FLAGS);
}

TEST_F(FlagsTest, SetCarryFlag) {
  auto helper = CPUTestHelper::CreateWithProgram("execute-stc-test", "stc\n");
  // Set initial flags: CF clear, others according to Y_CPU_INITIAL_FLAGS
  helper->cpu_.flags = Y_CPU_INITIAL_FLAGS;
  helper->ExecuteInstructions(1);
  // Expect CF to be set, others remain Y_CPU_INITIAL_FLAGS
  EXPECT_EQ(helper->cpu_.flags, Y_CPU_INITIAL_FLAGS | Y_CPU_CF);
}

TEST_F(FlagsTest, ClearInterruptFlag) {
  auto helper = CPUTestHelper::CreateWithProgram("execute-cli-test", "cli\n");
  // Set initial flags: IF set, others according to Y_CPU_INITIAL_FLAGS
  helper->cpu_.flags = Y_CPU_INITIAL_FLAGS | Y_CPU_IF;
  helper->ExecuteInstructions(1);
  // Expect IF to be cleared, others remain Y_CPU_INITIAL_FLAGS
  EXPECT_EQ(helper->cpu_.flags, Y_CPU_INITIAL_FLAGS);
}

TEST_F(FlagsTest, SetInterruptFlag) {
  auto helper = CPUTestHelper::CreateWithProgram("execute-sti-test", "sti\n");
  // Set initial flags: IF clear, others according to Y_CPU_INITIAL_FLAGS
  helper->cpu_.flags = Y_CPU_INITIAL_FLAGS;
  helper->ExecuteInstructions(1);
  // Expect IF to be set, others remain Y_CPU_INITIAL_FLAGS
  EXPECT_EQ(helper->cpu_.flags, Y_CPU_INITIAL_FLAGS | Y_CPU_IF);
}

TEST_F(FlagsTest, ClearDirectionFlag) {
  auto helper = CPUTestHelper::CreateWithProgram("execute-cld-test", "cld\n");
  // Set initial flags: DF set, others according to Y_CPU_INITIAL_FLAGS
  helper->cpu_.flags = Y_CPU_INITIAL_FLAGS | Y_CPU_DF;
  helper->ExecuteInstructions(1);
  // Expect DF to be cleared, others remain Y_CPU_INITIAL_FLAGS
  EXPECT_EQ(helper->cpu_.flags, Y_CPU_INITIAL_FLAGS);
}

TEST_F(FlagsTest, SetDirectionFlag) {
  auto helper = CPUTestHelper::CreateWithProgram("execute-std-test", "std\n");
  // Set initial flags: DF clear, others according to Y_CPU_INITIAL_FLAGS
  helper->cpu_.flags = Y_CPU_INITIAL_FLAGS;
  helper->ExecuteInstructions(1);
  // Expect DF to be set, others remain Y_CPU_INITIAL_FLAGS
  EXPECT_EQ(helper->cpu_.flags, Y_CPU_INITIAL_FLAGS | Y_CPU_DF);
}
