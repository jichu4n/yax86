// filepath: /home/chuan/Projects/yax86/tests/call_ret_test.cpp
#include <gtest/gtest.h>

#include "../cpu.h"
#include "./test_helpers.h"

using namespace std;

class CallRetTest : public ::testing::Test {};

TEST_F(CallRetTest, DirectNearCall) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-direct-near-call-test",
      "call foo\n"
      "mov ax, 5555h\n"
      "foo:\n"
      "  mov ax, 1234h\n"
      "  ret\n");
  helper->cpu_.registers[kSS] = 0;
  helper->cpu_.registers[kSP] = helper->memory_size_ - 2;

  helper->cpu_.registers[kAX] = 0;
  EXPECT_EQ(helper->cpu_.registers[kIP], kCOMFileLoadOffset + 0x00);

  helper->ExecuteInstructions(1);  // call foo
  EXPECT_EQ(helper->cpu_.registers[kIP], kCOMFileLoadOffset + 0x06);

  helper->ExecuteInstructions(1);  // mov ax, 1234h
  EXPECT_EQ(helper->cpu_.registers[kIP], kCOMFileLoadOffset + 0x09);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1234);

  helper->ExecuteInstructions(1);  // ret
  EXPECT_EQ(helper->cpu_.registers[kIP], kCOMFileLoadOffset + 0x03);

  helper->ExecuteInstructions(1);  // mov ax, 5555h
  EXPECT_EQ(helper->cpu_.registers[kIP], kCOMFileLoadOffset + 0x06);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x5555);
}

TEST_F(CallRetTest, DirectFarCall) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-direct-far-call-test",
      "call 0:foo\n"
      "mov ax, 5555h\n"
      "foo:\n"
      "  mov ax, 1234h\n"
      "  retf\n");
  helper->cpu_.registers[kSS] = 0;
  helper->cpu_.registers[kSP] = helper->memory_size_ - 2;

  helper->cpu_.registers[kAX] = 0;
  EXPECT_EQ(helper->cpu_.registers[kIP], kCOMFileLoadOffset + 0x00);

  helper->ExecuteInstructions(1);  // call $$foo
  EXPECT_EQ(helper->cpu_.registers[kIP], kCOMFileLoadOffset + 0x08);

  helper->ExecuteInstructions(1);  // mov ax, 1234h
  EXPECT_EQ(helper->cpu_.registers[kIP], kCOMFileLoadOffset + 0x0b);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1234);

  helper->ExecuteInstructions(1);  // retf
  EXPECT_EQ(helper->cpu_.registers[kIP], kCOMFileLoadOffset + 0x05);

  helper->ExecuteInstructions(1);  // mov ax, 5555h
  EXPECT_EQ(helper->cpu_.registers[kIP], kCOMFileLoadOffset + 0x08);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x5555);
}
