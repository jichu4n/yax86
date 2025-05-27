#include <gtest/gtest.h>

#include "../yax86.h"
#include "./test_helpers.h"

using namespace std;

class LeaLesLdsTest : public ::testing::Test {};

TEST_F(LeaLesLdsTest, LEA) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-lea-test",
      "lea ax, [bx+si]\n"      // Effective address from BX+SI
      "lea cx, [bp+di+10h]\n"  // Effective address with 8-bit displacement
      "lea dx, [0200h]\n"      // Effective address from direct address
      "lea sp, [bx+0100h]\n"   // Effective address with 16-bit displacement
      "lea bp, [si-5]\n");     // Effective address with negative 8-bit
                               // displacement (0FBh)
  helper->cpu_.registers[kDS] =
      0;  // LEA calculates offset, DS doesn't affect result unless overridden

  // Set various flags to verify LEA instructions don't affect them
  SetFlag(&helper->cpu_, kCF, true);
  SetFlag(&helper->cpu_, kZF, true);
  SetFlag(&helper->cpu_, kSF, true);
  SetFlag(&helper->cpu_, kPF, true);
  SetFlag(&helper->cpu_, kOF, true);
  SetFlag(&helper->cpu_, kAF, true);

  // Test 1: lea ax, [bx+si]
  // Set up: BX=0x1000, SI=0x0200. Expected AX = 0x1000 + 0x0200 = 0x1200
  helper->cpu_.registers[kBX] = 0x1000;
  helper->cpu_.registers[kSI] = 0x0200;
  // Put some data in memory to ensure LEA doesn't read it
  helper->memory_[0x1200] = 0xAA;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1200);
  EXPECT_EQ(helper->memory_[0x1200], 0xAA);  // Memory should be unchanged
  // Verify flags are still set after LEA instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 2: lea cx, [bp+di+10h]
  // Set up: BP=0x2000, DI=0x0300. Expected CX = 0x2000 + 0x0300 + 0x0010 =
  // 0x2310
  helper->cpu_.registers[kBP] = 0x2000;
  helper->cpu_.registers[kDI] = 0x0300;
  helper->memory_[0x2310] = 0xBB;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0x2310);
  EXPECT_EQ(helper->memory_[0x2310], 0xBB);  // Memory should be unchanged
  // Verify flags are still set
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 3: lea dx, [0200h]
  // Expected DX = 0x0200. Note: DS is 0, so segment override is not tested
  // here. For LEA, [0200h] means the offset 0200h itself, not DS:0200h.
  helper->memory_[0x0200] = 0xCC;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x0200);
  EXPECT_EQ(helper->memory_[0x0200], 0xCC);  // Memory should be unchanged
  // Verify flags are still set
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 4: lea sp, [bx+0100h]
  // Set up: BX=0x1000 (from Test 1). Expected SP = 0x1000 + 0x0100 = 0x1100
  // SI is still 0x0200 from Test 1, but not used here.
  helper->memory_[0x1100] = 0xDD;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kSP], 0x1100);
  EXPECT_EQ(helper->memory_[0x1100], 0xDD);  // Memory should be unchanged
  // Verify flags are still set
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 5: lea bp, [si-5] (equivalent to lea bp, [si+0FBh] for 8-bit
  // displacement) Set up: SI=0x0200 (from Test 1). Expected BP = 0x0200 - 5 =
  // 0x01FB BX is still 0x1000, BP is 0x2000 (will be overwritten), DI is 0x0300
  helper->memory_[0x01FB] = 0xEE;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kBP], 0x01FB);
  EXPECT_EQ(helper->memory_[0x01FB], 0xEE);  // Memory should be unchanged
  // Verify flags are still set
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});
}

TEST_F(LeaLesLdsTest, LES) {
  auto helper =
      CPUTestHelper::CreateWithProgram("execute-les-test", "les di, [bx]\n");
  helper->cpu_.registers[kDS] = 0;  // Use DS=0 for memory addressing

  // Set various flags to verify LES instructions don't affect them
  SetFlag(&helper->cpu_, kCF, true);
  SetFlag(&helper->cpu_, kZF, true);
  SetFlag(&helper->cpu_, kSF, true);
  SetFlag(&helper->cpu_, kPF, true);
  SetFlag(&helper->cpu_, kOF, true);
  SetFlag(&helper->cpu_, kAF, true);

  // Test: les di, [bx]
  // Set up: BX=0x0400. Memory at [0x0400] should contain a 32-bit pointer.
  // Offset part: 0xABCD at [0x0400]
  // Segment part: 0x1234 at [0x0402]
  helper->cpu_.registers[kBX] = 0x0400;
  helper->memory_[0x0400] = 0xCD;  // Low byte of offset
  helper->memory_[0x0401] = 0xAB;  // High byte of offset
  helper->memory_[0x0402] = 0x34;  // Low byte of segment
  helper->memory_[0x0403] = 0x12;  // High byte of segment

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kDI], 0xABCD);
  EXPECT_EQ(helper->cpu_.registers[kES], 0x1234);

  // Verify flags are still set after LES instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});
}

TEST_F(LeaLesLdsTest, LDS) {
  auto helper =
      CPUTestHelper::CreateWithProgram("execute-lds-test", "lds si, [0200h]\n");
  helper->cpu_.registers[kDS] = 0;  // Use DS=0 for memory addressing

  // Set various flags to verify LDS instructions don't affect them
  SetFlag(&helper->cpu_, kCF, true);
  SetFlag(&helper->cpu_, kZF, true);
  SetFlag(&helper->cpu_, kSF, true);
  SetFlag(&helper->cpu_, kPF, true);
  SetFlag(&helper->cpu_, kOF, true);
  SetFlag(&helper->cpu_, kAF, true);

  // Test: lds si, [0200h]
  // Memory at [0x0200] should contain a 32-bit pointer.
  // Offset part: 0x5678 at [0x0200]
  // Segment part: 0x9ABC at [0x0202]
  helper->memory_[0x0200] = 0x78;  // Low byte of offset
  helper->memory_[0x0201] = 0x56;  // High byte of offset
  helper->memory_[0x0202] = 0xBC;  // Low byte of segment
  helper->memory_[0x0203] = 0x9A;  // High byte of segment

  // Clear SI and DS before test to ensure they are loaded by LDS
  helper->cpu_.registers[kSI] = 0;
  // Note: LDS will change DS. The helper's DS is set to 0 for locating the
  // pointer itself.
  // The original DS value before LDS is not relevant for the instruction's
  // core logic being tested (loading DS from memory).

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kSI], 0x5678);
  EXPECT_EQ(helper->cpu_.registers[kDS], 0x9ABC);

  // Verify flags are still set after LDS instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});
}
