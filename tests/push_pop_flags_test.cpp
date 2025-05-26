#include <gtest/gtest.h>

#include "../yax86.h"
#include "./test_helpers.h"

using namespace std;

class PushPopFlagsTest : public ::testing::Test {};

TEST_F(PushPopFlagsTest, PushPopRegisters) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-push-pop-test",
      "push ax\n"   // Push AX onto the stack
      "push cx\n"   // Push CX onto the stack
      "pop dx\n"    // Pop from the stack into BX
      "pop bx\n");  // Pop from the stack into DX
  helper->cpu_.registers[kSS] = 0;
  helper->cpu_.registers[kSP] = helper->memory_size_ - 2;
  // Set up: AX=0x1234, CX=0x5678
  helper->cpu_.registers[kAX] = 0x1234;
  helper->cpu_.registers[kCX] = 0x5678;
  helper->cpu_.registers[kBX] = 0;
  helper->cpu_.registers[kDX] = 0;

  helper->ExecuteInstructions(4);
  EXPECT_EQ(helper->cpu_.registers[kBX], 0x1234);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x5678);
}

TEST_F(PushPopFlagsTest, PushPopSegmentRegisters) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-push-pop-segment-test",
      "push ds\n"   // Push DS onto the stack
      "push es\n"   // Push ES onto the stack
      "pop ds\n"    // Pop from the stack into SS
      "pop es\n");  // Pop from the stack into CS
  helper->cpu_.registers[kSS] = 0;
  helper->cpu_.registers[kSP] = helper->memory_size_ - 2;
  // Set up: DS=0x1234, ES=0x5678
  helper->cpu_.registers[kDS] = 0x1234;
  helper->cpu_.registers[kES] = 0x5678;

  helper->ExecuteInstructions(4);
  EXPECT_EQ(helper->cpu_.registers[kDS], 0x5678);
  EXPECT_EQ(helper->cpu_.registers[kES], 0x1234);
}

TEST_F(PushPopFlagsTest, PushPopFlag) {
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

TEST_F(PushPopFlagsTest, LAHFAndSAHF) {
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