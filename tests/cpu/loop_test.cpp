// filepath: /home/chuan/Projects/yax86/tests/loop_test.cpp
#include <gtest/gtest.h>

#include "./test_helpers.h"
#include "cpu.h"

using namespace std;

class LoopTest : public ::testing::Test {};

TEST_F(LoopTest, LOOP) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-loop-test",
      "loop target_label\n"
      "mov ax, 1\n"      // Should not be reached if jump is taken
      "jmp end_label\n"  // Skip setting ax to 2
      "target_label: mov ax, 2\n"
      "end_label: nop\n");

  // Test 1: CX > 1, should jump
  helper->cpu_.registers[kCX] = 5;
  helper->cpu_.registers[kAX] = 0;
  helper->ExecuteInstructions(
      2);  // loop + mov ax, 2 (if jump) or loop + mov ax,1 (if no jump)
  EXPECT_EQ(helper->cpu_.registers[kCX], 4);
  EXPECT_EQ(helper->cpu_.registers[kAX], 2);

  // Test 2: CX = 1, should not jump (CX becomes 0)
  helper->cpu_.registers[kIP] = kCOMFileLoadOffset;
  helper->cpu_.registers[kCX] = 1;
  helper->cpu_.registers[kAX] = 0;
  helper->ExecuteInstructions(2);  // loop + mov ax, 1
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);
  EXPECT_EQ(helper->cpu_.registers[kAX], 1);

  // Test 3: CX = 0, should jump (CX wraps to 0xFFFF)
  helper->cpu_.registers[kIP] = kCOMFileLoadOffset;
  helper->cpu_.registers[kCX] = 0;
  helper->cpu_.registers[kAX] = 0;
  helper->ExecuteInstructions(2);  // loop + mov ax, 2
  EXPECT_EQ(helper->cpu_.registers[kCX], 0xFFFF);
  EXPECT_EQ(helper->cpu_.registers[kAX], 2);
}

TEST_F(LoopTest, LOOPE) {  // Also tests LOOPZ
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-loope-test",
      "loope target_label\n"
      "mov ax, 1\n"
      "jmp end_label\n"
      "target_label: mov ax, 2\n"
      "end_label: nop\n");

  // Test 1: CX > 1, ZF = 1. Should jump.
  helper->cpu_.registers[kCX] = 5;
  SetFlag(&helper->cpu_, kZF, true);
  helper->cpu_.registers[kAX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[kCX], 4);
  EXPECT_EQ(helper->cpu_.registers[kAX], 2);
  EXPECT_EQ(GetFlag(&helper->cpu_, kZF), true);  // ZF Unchanged

  // Test 2: CX = 1, ZF = 1. Should not jump (CX becomes 0).
  helper->cpu_.registers[kIP] = kCOMFileLoadOffset;
  helper->cpu_.registers[kCX] = 1;
  SetFlag(&helper->cpu_, kZF, true);
  helper->cpu_.registers[kAX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);
  EXPECT_EQ(helper->cpu_.registers[kAX], 1);
  EXPECT_EQ(GetFlag(&helper->cpu_, kZF), true);

  // Test 3: CX > 1, ZF = 0. Should not jump.
  helper->cpu_.registers[kIP] = kCOMFileLoadOffset;
  helper->cpu_.registers[kCX] = 5;
  SetFlag(&helper->cpu_, kZF, false);
  helper->cpu_.registers[kAX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[kCX], 4);
  EXPECT_EQ(helper->cpu_.registers[kAX], 1);
  EXPECT_EQ(GetFlag(&helper->cpu_, kZF), false);

  // Test 4: CX = 0, ZF = 1. Should jump (CX wraps, ZF=1).
  helper->cpu_.registers[kIP] = kCOMFileLoadOffset;
  helper->cpu_.registers[kCX] = 0;
  SetFlag(&helper->cpu_, kZF, true);
  helper->cpu_.registers[kAX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0xFFFF);
  EXPECT_EQ(helper->cpu_.registers[kAX], 2);
  EXPECT_EQ(GetFlag(&helper->cpu_, kZF), true);

  // Test 5: CX = 0, ZF = 0. Should not jump (ZF=0).
  helper->cpu_.registers[kIP] = kCOMFileLoadOffset;
  helper->cpu_.registers[kCX] = 0;
  SetFlag(&helper->cpu_, kZF, false);
  helper->cpu_.registers[kAX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0xFFFF);
  EXPECT_EQ(helper->cpu_.registers[kAX], 1);
  EXPECT_EQ(GetFlag(&helper->cpu_, kZF), false);
}

TEST_F(LoopTest, LOOPNE) {  // Also tests LOOPNZ
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-loopne-test",
      "loopne target_label\n"
      "mov ax, 1\n"
      "jmp end_label\n"
      "target_label: mov ax, 2\n"
      "end_label: nop\n");

  // Test 1: CX > 1, ZF = 0. Should jump.
  helper->cpu_.registers[kCX] = 5;
  SetFlag(&helper->cpu_, kZF, false);
  helper->cpu_.registers[kAX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[kCX], 4);
  EXPECT_EQ(helper->cpu_.registers[kAX], 2);
  EXPECT_EQ(GetFlag(&helper->cpu_, kZF), false);  // ZF Unchanged

  // Test 2: CX = 1, ZF = 0. Should not jump (CX becomes 0).
  helper->cpu_.registers[kIP] = kCOMFileLoadOffset;
  helper->cpu_.registers[kCX] = 1;
  SetFlag(&helper->cpu_, kZF, false);
  helper->cpu_.registers[kAX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);
  EXPECT_EQ(helper->cpu_.registers[kAX], 1);
  EXPECT_EQ(GetFlag(&helper->cpu_, kZF), false);

  // Test 3: CX > 1, ZF = 1. Should not jump.
  helper->cpu_.registers[kIP] = kCOMFileLoadOffset;
  helper->cpu_.registers[kCX] = 5;
  SetFlag(&helper->cpu_, kZF, true);
  helper->cpu_.registers[kAX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[kCX], 4);
  EXPECT_EQ(helper->cpu_.registers[kAX], 1);
  EXPECT_EQ(GetFlag(&helper->cpu_, kZF), true);

  // Test 4: CX = 0, ZF = 0. Should jump (CX wraps, ZF=0).
  helper->cpu_.registers[kIP] = kCOMFileLoadOffset;
  helper->cpu_.registers[kCX] = 0;
  SetFlag(&helper->cpu_, kZF, false);
  helper->cpu_.registers[kAX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0xFFFF);
  EXPECT_EQ(helper->cpu_.registers[kAX], 2);
  EXPECT_EQ(GetFlag(&helper->cpu_, kZF), false);

  // Test 5: CX = 0, ZF = 1. Should not jump (ZF=1).
  helper->cpu_.registers[kIP] = kCOMFileLoadOffset;
  helper->cpu_.registers[kCX] = 0;
  SetFlag(&helper->cpu_, kZF, true);
  helper->cpu_.registers[kAX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0xFFFF);
  EXPECT_EQ(helper->cpu_.registers[kAX], 1);
  EXPECT_EQ(GetFlag(&helper->cpu_, kZF), true);
}

TEST_F(LoopTest, JCXZ) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-jcxz-test",
      "jcxz target_label\n"
      "mov ax, 1\n"
      "jmp end_label\n"
      "target_label: mov ax, 2\n"
      "end_label: nop\n");

  // Test 1: CX = 0. Should jump.
  SetFlag(&helper->cpu_, kZF, true);
  helper->cpu_.registers[kCX] = 0;
  helper->cpu_.registers[kAX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);  // JCXZ does not change CX
  EXPECT_EQ(helper->cpu_.registers[kAX], 2);
  EXPECT_EQ(GetFlag(&helper->cpu_, kZF), true);

  // Test 2: CX > 0 (e.g., 1). Should not jump.
  SetFlag(&helper->cpu_, kZF, true);
  helper->cpu_.registers[kIP] = kCOMFileLoadOffset;
  helper->cpu_.registers[kCX] = 1;
  helper->cpu_.registers[kAX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[kCX], 1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 1);
  EXPECT_EQ(GetFlag(&helper->cpu_, kZF), true);

  // Test 3: CX > 0 (e.g., 0xFFFF). Should not jump.
  SetFlag(&helper->cpu_, kZF, false);
  helper->cpu_.registers[kIP] = kCOMFileLoadOffset;
  helper->cpu_.registers[kCX] = 0xFFFF;
  helper->cpu_.registers[kAX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0xFFFF);
  EXPECT_EQ(helper->cpu_.registers[kAX], 1);
  EXPECT_EQ(GetFlag(&helper->cpu_, kZF), false);
}
