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
  helper->cpu_.registers[Y_CPU_CX] = 5;
  helper->cpu_.registers[Y_CPU_AX] = 0;
  helper->ExecuteInstructions(
      2);  // loop + mov ax, 2 (if jump) or loop + mov ax,1 (if no jump)
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 4);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_AX], 2);

  // Test 2: CX = 1, should not jump (CX becomes 0)
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_CX] = 1;
  helper->cpu_.registers[Y_CPU_AX] = 0;
  helper->ExecuteInstructions(2);  // loop + mov ax, 1
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 0);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_AX], 1);

  // Test 3: CX = 0, should jump (CX wraps to 0xFFFF)
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_CX] = 0;
  helper->cpu_.registers[Y_CPU_AX] = 0;
  helper->ExecuteInstructions(2);  // loop + mov ax, 2
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 0xFFFF);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_AX], 2);
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
  helper->cpu_.registers[Y_CPU_CX] = 5;
  YCPUSetFlag(&helper->cpu_, Y_CPU_ZF, true);
  helper->cpu_.registers[Y_CPU_AX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 4);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_AX], 2);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), true);  // ZF Unchanged

  // Test 2: CX = 1, ZF = 1. Should not jump (CX becomes 0).
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_CX] = 1;
  YCPUSetFlag(&helper->cpu_, Y_CPU_ZF, true);
  helper->cpu_.registers[Y_CPU_AX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 0);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_AX], 1);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), true);

  // Test 3: CX > 1, ZF = 0. Should not jump.
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_CX] = 5;
  YCPUSetFlag(&helper->cpu_, Y_CPU_ZF, false);
  helper->cpu_.registers[Y_CPU_AX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 4);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_AX], 1);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), false);

  // Test 4: CX = 0, ZF = 1. Should jump (CX wraps, ZF=1).
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_CX] = 0;
  YCPUSetFlag(&helper->cpu_, Y_CPU_ZF, true);
  helper->cpu_.registers[Y_CPU_AX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 0xFFFF);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_AX], 2);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), true);

  // Test 5: CX = 0, ZF = 0. Should not jump (ZF=0).
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_CX] = 0;
  YCPUSetFlag(&helper->cpu_, Y_CPU_ZF, false);
  helper->cpu_.registers[Y_CPU_AX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 0xFFFF);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_AX], 1);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), false);
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
  helper->cpu_.registers[Y_CPU_CX] = 5;
  YCPUSetFlag(&helper->cpu_, Y_CPU_ZF, false);
  helper->cpu_.registers[Y_CPU_AX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 4);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_AX], 2);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), false);  // ZF Unchanged

  // Test 2: CX = 1, ZF = 0. Should not jump (CX becomes 0).
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_CX] = 1;
  YCPUSetFlag(&helper->cpu_, Y_CPU_ZF, false);
  helper->cpu_.registers[Y_CPU_AX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 0);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_AX], 1);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), false);

  // Test 3: CX > 1, ZF = 1. Should not jump.
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_CX] = 5;
  YCPUSetFlag(&helper->cpu_, Y_CPU_ZF, true);
  helper->cpu_.registers[Y_CPU_AX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 4);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_AX], 1);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), true);

  // Test 4: CX = 0, ZF = 0. Should jump (CX wraps, ZF=0).
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_CX] = 0;
  YCPUSetFlag(&helper->cpu_, Y_CPU_ZF, false);
  helper->cpu_.registers[Y_CPU_AX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 0xFFFF);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_AX], 2);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), false);

  // Test 5: CX = 0, ZF = 1. Should not jump (ZF=1).
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_CX] = 0;
  YCPUSetFlag(&helper->cpu_, Y_CPU_ZF, true);
  helper->cpu_.registers[Y_CPU_AX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 0xFFFF);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_AX], 1);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), true);
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
  YCPUSetFlag(&helper->cpu_, Y_CPU_ZF, true);
  helper->cpu_.registers[Y_CPU_CX] = 0;
  helper->cpu_.registers[Y_CPU_AX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 0);  // JCXZ does not change CX
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_AX], 2);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), true);

  // Test 2: CX > 0 (e.g., 1). Should not jump.
  YCPUSetFlag(&helper->cpu_, Y_CPU_ZF, true);
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_CX] = 1;
  helper->cpu_.registers[Y_CPU_AX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 1);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_AX], 1);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), true);

  // Test 3: CX > 0 (e.g., 0xFFFF). Should not jump.
  YCPUSetFlag(&helper->cpu_, Y_CPU_ZF, false);
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_CX] = 0xFFFF;
  helper->cpu_.registers[Y_CPU_AX] = 0;
  helper->ExecuteInstructions(2);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 0xFFFF);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_AX], 1);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), false);
}
