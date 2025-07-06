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
  helper->cpu_.registers[kSS] = 0;
  helper->cpu_.registers[kSP] = helper->memory_size_ - 2;
  // Set up: Flags=0x1234
  helper->cpu_.flags = 0x1234;

  helper->ExecuteInstructions(1);
  helper->cpu_.flags = 0x5678;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.flags, 0x1234);
}

TEST_F(FlagsTest, LAHFAndSAHF) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-lahf-sahf-test",
      "lahf\n"
      "sahf\n");
  helper->cpu_.flags = 0x1234;
  helper->cpu_.registers[kAX] = 0;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x3400);
  helper->cpu_.registers[kAX] = 0x5678;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.flags, 0x1256);
}

TEST_F(FlagsTest, ClearCarryFlag) {
  auto helper = CPUTestHelper::CreateWithProgram("execute-clc-test", "clc\n");
  // Set initial flags: CF set, others according to kInitialFlags
  helper->cpu_.flags = kInitialFlags | kCF;
  helper->ExecuteInstructions(1);
  // Expect CF to be cleared, others remain kInitialFlags
  EXPECT_EQ(helper->cpu_.flags, kInitialFlags);
}

TEST_F(FlagsTest, SetCarryFlag) {
  auto helper = CPUTestHelper::CreateWithProgram("execute-stc-test", "stc\n");
  // Set initial flags: CF clear, others according to kInitialFlags
  helper->cpu_.flags = kInitialFlags;
  helper->ExecuteInstructions(1);
  // Expect CF to be set, others remain kInitialFlags
  EXPECT_EQ(helper->cpu_.flags, kInitialFlags | kCF);
}

TEST_F(FlagsTest, ClearInterruptFlag) {
  auto helper = CPUTestHelper::CreateWithProgram("execute-cli-test", "cli\n");
  // Set initial flags: IF set, others according to kInitialFlags
  helper->cpu_.flags = kInitialFlags | kIF;
  helper->ExecuteInstructions(1);
  // Expect IF to be cleared, others remain kInitialFlags
  EXPECT_EQ(helper->cpu_.flags, kInitialFlags);
}

TEST_F(FlagsTest, SetInterruptFlag) {
  auto helper = CPUTestHelper::CreateWithProgram("execute-sti-test", "sti\n");
  // Set initial flags: IF clear, others according to kInitialFlags
  helper->cpu_.flags = kInitialFlags;
  helper->ExecuteInstructions(1);
  // Expect IF to be set, others remain kInitialFlags
  EXPECT_EQ(helper->cpu_.flags, kInitialFlags | kIF);
}

TEST_F(FlagsTest, ClearDirectionFlag) {
  auto helper = CPUTestHelper::CreateWithProgram("execute-cld-test", "cld\n");
  // Set initial flags: DF set, others according to kInitialFlags
  helper->cpu_.flags = kInitialFlags | kDF;
  helper->ExecuteInstructions(1);
  // Expect DF to be cleared, others remain kInitialFlags
  EXPECT_EQ(helper->cpu_.flags, kInitialFlags);
}

TEST_F(FlagsTest, SetDirectionFlag) {
  auto helper = CPUTestHelper::CreateWithProgram("execute-std-test", "std\n");
  // Set initial flags: DF clear, others according to kInitialFlags
  helper->cpu_.flags = kInitialFlags;
  helper->ExecuteInstructions(1);
  // Expect DF to be set, others remain kInitialFlags
  EXPECT_EQ(helper->cpu_.flags, kInitialFlags | kDF);
}
