// Tests for undocumented 8086/8088 opcodes and opcode aliases.
//
// The 8086/8088 has no invalid opcode exception - its decoder does not examine
// every bit of an opcode, so undefined encodings fall through to whatever
// partial match they hit. Every one of the 256 opcode bytes therefore does
// something. Behavior here is cross-checked against MartyPC's 8088 decode
// table, which was validated against real hardware.
//
// Assemblers will not emit these encodings, so the test programs are
// hand-assembled.

#include <gtest/gtest.h>

#include "./test_helpers.h"
#include "cpu.h"

using namespace std;

class UndocumentedOpcodesTest : public ::testing::Test {};

namespace {

// Builds a helper with hand-assembled machine code loaded at 0x100.
unique_ptr<CPUTestHelper> WithCode(const vector<uint8_t>& code) {
  auto helper = make_unique<CPUTestHelper>();
  helper->LoadCOM(code);
  return helper;
}

}  // namespace

// ============================================================================
// 0x60-0x6F - conditional jumps, aliases of 0x70-0x7F
// ============================================================================

// The alias must jump identically to its documented counterpart, and - just as
// importantly - must be decoded as two bytes. Sizing it as one byte would feed
// the rel8 operand back to the decoder as an opcode.
TEST_F(UndocumentedOpcodesTest, ConditionalJumpAliasesDecodeAsTwoBytes) {
  for (uint8_t opcode = 0x60; opcode <= 0x6F; ++opcode) {
    auto helper = WithCode({opcode, 0x10});
    Instruction instruction;
    ASSERT_EQ(
        CPUFetchNextInstruction(&helper->cpu_, &instruction), kFetchSuccess)
        << "opcode " << hex << static_cast<int>(opcode);
    EXPECT_EQ(instruction.size, 2)
        << "opcode " << hex << static_cast<int>(opcode);
    EXPECT_EQ(instruction.immediate_size, 1)
        << "opcode " << hex << static_cast<int>(opcode);
  }
}

TEST_F(UndocumentedOpcodesTest, ConditionalJumpAliasesMatchTheirCounterparts) {
  // Exercise each alias against 0x70-0x7F with both a taken and an untaken
  // condition, driven by a flags value that makes the outcome differ per
  // opcode.
  static const uint16_t kFlagValues[] = {
      0,
      kCF,
      kZF,
      kSF,
      kOF,
      kPF,
      static_cast<uint16_t>(kSF | kOF),
      static_cast<uint16_t>(kZF | kCF),
  };

  for (uint8_t low = 0; low <= 0x0F; ++low) {
    for (uint16_t flags : kFlagValues) {
      auto aliased = WithCode({static_cast<uint8_t>(0x60 + low), 0x10});
      auto documented = WithCode({static_cast<uint8_t>(0x70 + low), 0x10});
      aliased->cpu_.flags = flags;
      documented->cpu_.flags = flags;

      aliased->ExecuteInstructions(1);
      documented->ExecuteInstructions(1);

      EXPECT_EQ(aliased->cpu_.registers[kIP], documented->cpu_.registers[kIP])
          << "opcode 0x" << hex << static_cast<int>(0x60 + low) << " vs 0x"
          << static_cast<int>(0x70 + low) << " with flags 0x" << flags;
    }
  }
}

// ============================================================================
// 0x0F - POP CS
// ============================================================================

TEST_F(UndocumentedOpcodesTest, PopCS) {
  auto helper = WithCode({0x0F});
  helper->cpu_.registers[kSS] = 0;
  helper->cpu_.registers[kSP] = 0x0800;
  // Value to be popped into CS.
  helper->memory_[0x0800] = 0x34;
  helper->memory_[0x0801] = 0x12;

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kCS], 0x1234);
  EXPECT_EQ(helper->cpu_.registers[kSP], 0x0802);
}

// ============================================================================
// 0xC0/0xC1/0xC8/0xC9 - RET and RETF aliases
// ============================================================================

TEST_F(UndocumentedOpcodesTest, NearReturnAliases) {
  // 0xC1 is RET with no immediate.
  auto helper = WithCode({0xC1});
  helper->cpu_.registers[kSS] = 0;
  helper->cpu_.registers[kSP] = 0x0800;
  helper->memory_[0x0800] = 0xCD;
  helper->memory_[0x0801] = 0xAB;

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kIP], 0xABCD);
  EXPECT_EQ(helper->cpu_.registers[kSP], 0x0802);
}

TEST_F(UndocumentedOpcodesTest, NearReturnAndPopAliasDecodesImmediate) {
  // 0xC0 is RET imm16, so it must consume two immediate bytes.
  auto helper = WithCode({0xC0, 0x04, 0x00});
  helper->cpu_.registers[kSS] = 0;
  helper->cpu_.registers[kSP] = 0x0800;
  helper->memory_[0x0800] = 0xCD;
  helper->memory_[0x0801] = 0xAB;

  Instruction instruction;
  ASSERT_EQ(
      CPUFetchNextInstruction(&helper->cpu_, &instruction), kFetchSuccess);
  EXPECT_EQ(instruction.size, 3);

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kIP], 0xABCD);
  // Two bytes popped for the return address, plus the four requested.
  EXPECT_EQ(helper->cpu_.registers[kSP], 0x0806);
}

TEST_F(UndocumentedOpcodesTest, FarReturnAliases) {
  // 0xC9 is RETF with no immediate.
  auto helper = WithCode({0xC9});
  helper->cpu_.registers[kSS] = 0;
  helper->cpu_.registers[kSP] = 0x0800;
  helper->memory_[0x0800] = 0xCD;
  helper->memory_[0x0801] = 0xAB;
  helper->memory_[0x0802] = 0x00;
  helper->memory_[0x0803] = 0x00;

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kIP], 0xABCD);
  EXPECT_EQ(helper->cpu_.registers[kSP], 0x0804);
}

TEST_F(UndocumentedOpcodesTest, FarReturnAndPopAliasDecodesImmediate) {
  // 0xC8 is RETF imm16.
  auto helper = WithCode({0xC8, 0x02, 0x00});
  Instruction instruction;
  ASSERT_EQ(
      CPUFetchNextInstruction(&helper->cpu_, &instruction), kFetchSuccess);
  EXPECT_EQ(instruction.size, 3);
}

// ============================================================================
// 0xD6 - SALC
// ============================================================================

TEST_F(UndocumentedOpcodesTest, SetALFromCarry) {
  auto helper = WithCode({0xD6});
  helper->cpu_.registers[kAX] = 0x1234;
  CPUSetFlag(&helper->cpu_, kCF, true);

  helper->ExecuteInstructions(1);

  // AL becomes 0xFF, AH is untouched.
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x12FF);
  // SALC affects no flags.
  helper->CheckFlags({{kCF, true}});
}

TEST_F(UndocumentedOpcodesTest, SetALFromCarryWhenClear) {
  auto helper = WithCode({0xD6});
  helper->cpu_.registers[kAX] = 0x12AB;
  CPUSetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1200);
  helper->CheckFlags({{kCF, false}});
}

// ============================================================================
// 0xF1 - LOCK prefix alias
// ============================================================================

TEST_F(UndocumentedOpcodesTest, LockPrefixAliasIsConsumedAsAPrefix) {
  // 0xF1 prefixing NOP must decode as a single two-byte instruction.
  auto helper = WithCode({0xF1, 0x90});

  Instruction instruction;
  ASSERT_EQ(
      CPUFetchNextInstruction(&helper->cpu_, &instruction), kFetchSuccess);
  EXPECT_EQ(instruction.size, 2);
  EXPECT_EQ(instruction.prefix_size, 1);
  EXPECT_EQ(instruction.prefix[0], 0xF1);
  EXPECT_EQ(instruction.opcode, 0x90);
}

// ============================================================================
// Group REG field aliases
// ============================================================================

TEST_F(UndocumentedOpcodesTest, Group3Reg1IsTestAndDecodesImmediate) {
  // 0xF6 /1 aliases 0xF6 /0 (TEST r/m8, imm8), so it carries an immediate.
  // ModRM 0xC8 = mod 11, reg 001, rm 000 -> AL.
  auto helper = WithCode({0xF6, 0xC8, 0x0F});
  helper->cpu_.registers[kAX] = 0x00F0;

  Instruction instruction;
  ASSERT_EQ(
      CPUFetchNextInstruction(&helper->cpu_, &instruction), kFetchSuccess);
  EXPECT_EQ(instruction.size, 3);

  helper->ExecuteInstructions(1);

  // 0xF0 & 0x0F == 0, so ZF is set and AL is unchanged.
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x00F0);
  helper->CheckFlags({{kZF, true}, {kCF, false}, {kOF, false}});
}

TEST_F(UndocumentedOpcodesTest, Group5Reg7IsPush) {
  // 0xFF /7 aliases 0xFF /6 (PUSH r/m16).
  // ModRM 0xFB = mod 11, reg 111, rm 011 -> BX.
  auto helper = WithCode({0xFF, 0xFB});
  helper->cpu_.registers[kSS] = 0;
  helper->cpu_.registers[kSP] = 0x0800;
  helper->cpu_.registers[kBX] = 0x5678;

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kSP], 0x07FE);
  EXPECT_EQ(helper->memory_[0x07FE], 0x78);
  EXPECT_EQ(helper->memory_[0x07FF], 0x56);
}

TEST_F(UndocumentedOpcodesTest, PopRegisterOrMemoryIgnoresRegField) {
  // 0x8F with a non-zero REG field still pops. ModRM 0xCB = mod 11, reg 001,
  // rm 011 -> BX.
  auto helper = WithCode({0x8F, 0xCB});
  helper->cpu_.registers[kSS] = 0;
  helper->cpu_.registers[kSP] = 0x0800;
  helper->memory_[0x0800] = 0x21;
  helper->memory_[0x0801] = 0x43;

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kBX], 0x4321);
  EXPECT_EQ(helper->cpu_.registers[kSP], 0x0802);
}

// ============================================================================
// 0xD0-0xD3 /6 - SETMO and SETMOC
// ============================================================================

// REG 6 of the shift group is its own operation on the 8086/8088 rather than
// an alias of SAL: it sets every bit of the operand.
TEST_F(UndocumentedOpcodesTest, SetMinusOneByte) {
  // D0 /6 with mod=11, rm=011: SETMO BL
  auto helper = WithCode({0xD0, 0xF3});
  helper->cpu_.registers[kBX] = 0x1234;
  CPUSetFlag(&helper->cpu_, kCF, true);
  CPUSetFlag(&helper->cpu_, kOF, true);
  CPUSetFlag(&helper->cpu_, kAF, true);

  helper->ExecuteInstructions(1);

  // BL becomes 0xFF, BH is untouched.
  EXPECT_EQ(helper->cpu_.registers[kBX], 0x12FF);
  // Carry, overflow and auxiliary carry are cleared, and sign, zero and parity
  // follow the all-ones result.
  helper->CheckFlags(
      {{kCF, false},
       {kOF, false},
       {kAF, false},
       {kSF, true},
       {kZF, false},
       {kPF, true}});
}

TEST_F(UndocumentedOpcodesTest, SetMinusOneWord) {
  // D1 /6 with mod=11, rm=010: SETMO DX
  auto helper = WithCode({0xD1, 0xF2});
  helper->cpu_.registers[kDX] = 0x1234;

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kDX], 0xFFFF);
}

// The count still gates the operation, so the CL forms do nothing at all when
// CL is zero.
TEST_F(UndocumentedOpcodesTest, SetMinusOneConditionalByCL) {
  // D3 /6 with mod=11, rm=001: SETMOC CX, CL
  auto helper = WithCode({0xD3, 0xF1});
  helper->cpu_.registers[kCX] = 0x1234;

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kCX], 0xFFFF);
}

TEST_F(UndocumentedOpcodesTest, SetMinusOneConditionalIsNoOpWhenCountIsZero) {
  // D2 /6 with mod=11, rm=011: SETMOC BL, CL
  auto helper = WithCode({0xD2, 0xF3});
  helper->cpu_.registers[kBX] = 0x1234;
  helper->cpu_.registers[kCX] = 0x0000;
  CPUSetFlag(&helper->cpu_, kCF, true);

  helper->ExecuteInstructions(1);

  // Neither the operand nor the flags are touched.
  EXPECT_EQ(helper->cpu_.registers[kBX], 0x1234);
  helper->CheckFlags({{kCF, true}});
}
