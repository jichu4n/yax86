#include <gtest/gtest.h>

#include "./test_helpers.h"
#include "cpu.h"

using namespace std;

class CmpJmpTest : public ::testing::Test {};

TEST_F(CmpJmpTest, CMPJE) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-cmp-jmp-unsigned-test-je",
      "cmp ax, bx\n"
      "je b2\n"
      "b1: mov cx, 1\n"
      "b2: mov cx, 2\n");

  // Test 1: Should jump to b2
  helper->cpu_.registers[Y_CPU_AX] = 0x42;
  helper->cpu_.registers[Y_CPU_BX] = 0x42;
  helper->cpu_.registers[Y_CPU_CX] = 0;
  helper->ExecuteInstructions(3);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), true);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 2);

  // Test 2: Should not jump to b2
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_AX] = 0x42;
  helper->cpu_.registers[Y_CPU_BX] = 0x43;
  helper->cpu_.registers[Y_CPU_CX] = 0;
  helper->ExecuteInstructions(3);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), false);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 1);
}

TEST_F(CmpJmpTest, CMPJNE) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-cmp-jmp-unsigned-test-jne",
      "cmp ax, 1234h\n"
      "jne b2\n"
      "b1: mov cx, 1\n"
      "b2: mov cx, 2\n");

  // Test 1: Should not jump to b2
  helper->cpu_.registers[Y_CPU_AX] = 0x1234;
  helper->cpu_.registers[Y_CPU_CX] = 0;
  helper->ExecuteInstructions(3);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), true);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 1);

  // Test 2: Should jump to b2
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_AX] = 0x1235;
  helper->cpu_.registers[Y_CPU_CX] = 0;
  helper->ExecuteInstructions(3);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), false);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 2);
}

TEST_F(CmpJmpTest, CMPJB) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-cmp-jmp-unsigned-test-jb",
      "cmp al, 0x42\n"
      "jb b2\n"
      "b1: mov cx, 1\n"
      "b2: mov cx, 2\n");

  // Test 1: Should jump to b2
  helper->cpu_.registers[Y_CPU_AX] = 0x41;
  helper->cpu_.registers[Y_CPU_CX] = 0;
  helper->ExecuteInstructions(3);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_CF), true);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 2);

  // Test 2: Should not jump to b2
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_AX] = 0x42;
  helper->cpu_.registers[Y_CPU_CX] = 0;
  helper->ExecuteInstructions(3);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_CF), false);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 1);

  // Test 3: Should not jump to b2
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_AX] = 0x43;
  helper->cpu_.registers[Y_CPU_CX] = 0;
  helper->ExecuteInstructions(3);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_CF), false);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 1);
}

TEST_F(CmpJmpTest, CMPJA) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-cmp-jmp-unsigned-test-ja",
      "cmp al, 0x42\n"
      "ja b2\n"
      "b1: mov cx, 1\n"
      "b2: mov cx, 2\n");

  // Test 1: Should jump to b2
  helper->cpu_.registers[Y_CPU_AX] = 0x43;
  helper->cpu_.registers[Y_CPU_CX] = 0;
  helper->ExecuteInstructions(3);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_CF), false);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), false);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 2);

  // Test 2: Should not jump to b2
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_AX] = 0x42;
  helper->cpu_.registers[Y_CPU_CX] = 0;
  helper->ExecuteInstructions(3);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_CF), false);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), true);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 1);

  // Test 2: Should not jump to b2
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_AX] = 0x41;
  helper->cpu_.registers[Y_CPU_CX] = 0;
  helper->ExecuteInstructions(3);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_CF), true);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), false);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 1);
}

TEST_F(CmpJmpTest, CMPJBE) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-cmp-jmp-unsigned-test-jbe",
      "cmp al, 0x42\n"
      "jbe b2\n"
      "b1: mov cx, 1\n"
      "b2: mov cx, 2\n");

  // Test 1: Should jump to b2
  helper->cpu_.registers[Y_CPU_AX] = 0x41;
  helper->cpu_.registers[Y_CPU_CX] = 0;
  helper->ExecuteInstructions(3);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_CF), true);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), false);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 2);

  // Test 2: Should jump to b2
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_AX] = 0x42;
  helper->cpu_.registers[Y_CPU_CX] = 0;
  helper->ExecuteInstructions(3);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_CF), false);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), true);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 2);

  // Test 3: Should not jump to b2
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_AX] = 0x43;
  helper->cpu_.registers[Y_CPU_CX] = 0;
  helper->ExecuteInstructions(3);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_CF), false);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), false);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 1);
}

TEST_F(CmpJmpTest, CMPJG) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-cmp-jmp-unsigned-test-jg",
      "cmp al, 0F6h\n"  // -10
      "jg b2\n"
      "b1: mov cx, 1\n"
      "b2: mov cx, 2\n");

  // Test 1: Should jump to b2
  helper->cpu_.registers[Y_CPU_AX] = 0xFB;  // -5
  helper->cpu_.registers[Y_CPU_CX] = 0;
  helper->ExecuteInstructions(3);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_SF), false);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_OF), false);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), false);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 2);

  // Test 2: Should not jump to b2
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_AX] = 0xF6;  // -10
  helper->cpu_.registers[Y_CPU_CX] = 0;
  helper->ExecuteInstructions(3);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_SF), false);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_OF), false);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), true);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 1);

  // Test 3: Should not jump to b2
  helper->cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
  helper->cpu_.registers[Y_CPU_AX] = 0xEC;  // -20
  helper->cpu_.registers[Y_CPU_CX] = 0;
  helper->ExecuteInstructions(3);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_SF), true);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_OF), false);
  EXPECT_EQ(YCPUGetFlag(&helper->cpu_, Y_CPU_ZF), false);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 1);
}

TEST_F(CmpJmpTest, JumpShort) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-cmp-jmp-short-jmp-test",
      "jmp b2\n"
      "b1: mov cx, 1\n"
      "b2: mov cx, 2\n");
  helper->cpu_.registers[Y_CPU_CX] = 0;
  helper->ExecuteInstructions(1);  // jmp b2
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_IP], kCOMFileLoadOffset + 0x5);
  helper->ExecuteInstructions(1);  // mov cx, 2
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 2);
}

TEST_F(CmpJmpTest, JumpFar) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-cmp-jmp-far-jmp-test",
      "jmp 0000:b2\n"
      "b1: mov cx, 1\n"
      "b2: mov cx, 2\n");
  helper->cpu_.registers[Y_CPU_CX] = 0;
  helper->ExecuteInstructions(1);  // jmp b2
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_IP], kCOMFileLoadOffset + 0x8);
  helper->ExecuteInstructions(1);  // mov cx, 2
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_CX], 2);
}