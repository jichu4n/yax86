#include <gtest/gtest.h>

#include "../yax86.h"
#include "./test_helpers.h"

using namespace std;

class AndOrXorTest : public ::testing::Test {};

TEST_F(AndOrXorTest, AND) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-and-test",
      "and ax, [bx]\n"      // Register & Memory (word)
      "and [bx], cx\n"      // Memory & Register (word)
      "and dx, cx\n"        // Register & Register (word)
      "and dh, [di+1]\n"    // Register & Memory (byte)
      "and [di-1], cl\n"    // Memory & Register (byte)
      "and al, 0AAh\n"      // AL & Immediate (byte)
      "and ax, 0AAAAh\n");  // AX & Immediate (word)
  helper->cpu_.registers[kDS] = 0;

  // Set various flags to verify they are properly affected by AND
  SetFlag(&helper->cpu_, kCF, true);
  SetFlag(&helper->cpu_, kZF, false);
  SetFlag(&helper->cpu_, kSF, false);
  SetFlag(&helper->cpu_, kPF, false);
  SetFlag(&helper->cpu_, kOF, true);
  SetFlag(&helper->cpu_, kAF, true);

  // Test 1: and ax, [bx] - Register & Memory (word)
  // ax = 0xFFFF, bx = 0x0400, memory[0x0400] = 0x1234
  // Result: ax = 0x1234 (0xFFFF & 0x1234 = 0x1234)
  helper->cpu_.registers[kAX] = 0xFFFF;
  helper->cpu_.registers[kBX] = 0x0400;
  helper->memory_[0x0400] = 0x34;  // LSB
  helper->memory_[0x0401] = 0x12;  // MSB
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1234);
  // Verify flags: CF and OF should be cleared, others depend on result
  helper->CheckFlags(
      {{kZF, false},
       {kSF, false},
       {kPF, false},
       {kCF, false},    // CF should be cleared by AND
       {kOF, false}});  // OF should be cleared by AND

  // Test 2: and [bx], cx - Memory & Register (word)
  // memory[0x0400] = 0x1234, cx = 0xF0F0
  // Result: memory[0x0400] = 0x1030 (0x1234 & 0xF0F0 = 0x1030)
  helper->cpu_.registers[kCX] = 0xF0F0;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0400], 0x30);  // LSB
  EXPECT_EQ(helper->memory_[0x0401], 0x10);  // MSB
  // Verify flags
  helper->CheckFlags(
      {{kZF, false}, {kSF, false}, {kPF, true}, {kCF, false}, {kOF, false}});

  // Test 3: and dx, cx - Register & Register (word)
  // dx = 0xAAAA, cx = 0xF0F0
  // Result: dx = 0xA0A0 (0xAAAA & 0xF0F0 = 0xA0A0)
  helper->cpu_.registers[kDX] = 0xAAAA;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0xA0A0);
  // Verify flags
  helper->CheckFlags(
      {{kZF, false},
       {kSF, true},  // Negative result (bit 15 is set)
       {kPF, true},
       {kCF, false},
       {kOF, false}});

  // Test 4: and dh, [di+1] - Register & Memory (byte)
  // dh = 0xA0 (from 0xA0A0), di+1 = 0x0501, memory[0x0501] = 0x5A
  // Result: dh = 0x00 (0xA0 & 0x5A = 0x00)
  helper->cpu_.registers[kDI] = 0x0500;
  helper->memory_[0x0501] = 0x5A;
  helper->ExecuteInstructions(1);
  EXPECT_EQ((helper->cpu_.registers[kDX] >> 8) & 0xFF, 0x00);  // High byte (DH)
  EXPECT_EQ(
      helper->cpu_.registers[kDX] & 0xFF, 0xA0);  // Low byte (DL) unchanged
  // Verify flags: ZF set since result is zero
  helper->CheckFlags(
      {{kZF, true},
       {kSF, false},
       {kPF, true},  // Even parity for 0x00
       {kCF, false},
       {kOF, false}});

  // Test 5: and [di-1], cl - Memory & Register (byte)
  // memory[0x04FF] = 0xCC, cl = 0xF0 (set it)
  // Result: memory[0x04FF] = 0xC0 (0xCC & 0xF0 = 0xC0)
  helper->memory_[0x04FF] = 0xCC;
  helper->cpu_.registers[kCX] = (helper->cpu_.registers[kCX] & 0xFF00) | 0xF0;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x04FF], 0xC0);
  // Verify flags
  helper->CheckFlags(
      {{kZF, false},
       {kSF, true},  // Negative result (bit 7 is set)
       {kPF, true},  // Odd parity
       {kCF, false},
       {kOF, false}});

  // Test 6: and al, 0AAh - AL & Immediate (byte)
  // al = 0x55
  // Result: al = 0x00 (0x55 & 0xAA = 0x00) - no bits in common
  helper->cpu_.registers[kAX] = (helper->cpu_.registers[kAX] & 0xFF00) | 0x55;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x00);
  // Verify flags: ZF set since result is zero
  helper->CheckFlags(
      {{kZF, true},
       {kSF, false},
       {kPF, true},  // Even parity for 0x00
       {kCF, false},
       {kOF, false}});

  // Test 7: and ax, 0AAAAh - AX & Immediate (word)
  // ax = 0x5555
  // Result: ax = 0x0000 (0x5555 & 0xAAAA = 0x0000) - no bits in common
  helper->cpu_.registers[kAX] = 0x5555;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0000);
  // Verify flags: ZF set since result is zero
  helper->CheckFlags(
      {{kZF, true},
       {kSF, false},
       {kPF, true},  // Even parity for 0x00
       {kCF, false},
       {kOF, false}});
}

TEST_F(AndOrXorTest, OR) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-or-test",
      "or ax, [bx]\n"      // Register | Memory (word)
      "or [bx], cx\n"      // Memory | Register (word)
      "or dx, cx\n"        // Register | Register (word)
      "or dh, [di+1]\n"    // Register | Memory (byte)
      "or [di-1], cl\n"    // Memory | Register (byte)
      "or al, 0AAh\n"      // AL | Immediate (byte)
      "or ax, 0AAAAh\n");  // AX | Immediate (word)
  helper->cpu_.registers[kDS] = 0;

  // Set various flags to verify they are properly affected by OR
  SetFlag(&helper->cpu_, kCF, true);
  SetFlag(&helper->cpu_, kZF, true);
  SetFlag(&helper->cpu_, kSF, false);
  SetFlag(&helper->cpu_, kPF, false);
  SetFlag(&helper->cpu_, kOF, true);
  SetFlag(&helper->cpu_, kAF, true);

  // Test 1: or ax, [bx] - Register | Memory (word)
  // ax = 0x1200, bx = 0x0400, memory[0x0400] = 0x0034
  // Result: ax = 0x1234 (0x1200 | 0x0034 = 0x1234)
  helper->cpu_.registers[kAX] = 0x1200;
  helper->cpu_.registers[kBX] = 0x0400;
  helper->memory_[0x0400] = 0x34;  // LSB
  helper->memory_[0x0401] = 0x00;  // MSB
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1234);
  // Verify flags: CF and OF should be cleared, others depend on result
  helper->CheckFlags(
      {{kZF, false},
       {kSF, false},
       {kPF, false},
       {kCF, false},    // CF should be cleared by OR
       {kOF, false}});  // OF should be cleared by OR

  // Test 2: or [bx], cx - Memory | Register (word)
  // memory[0x0400] = 0x1234, cx = 0xF000
  // Result: memory[0x0400] = 0xF234 (0x1234 | 0xF000 = 0xF234)
  helper->cpu_.registers[kCX] = 0xF000;
  helper->memory_[0x0400] = 0x34;  // LSB
  helper->memory_[0x0401] = 0x12;  // MSB
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0400], 0x34);  // LSB
  EXPECT_EQ(helper->memory_[0x0401], 0xF2);  // MSB
  // Verify flags
  helper->CheckFlags(
      {{kZF, false},
       {kSF, true},  // Negative result (bit 15 is set)
       {kPF, false},
       {kCF, false},
       {kOF, false}});

  // Test 3: or dx, cx - Register | Register (word)
  // dx = 0x0A0A, cx = 0xF000
  // Result: dx = 0xFA0A (0x0A0A | 0xF000 = 0xFA0A)
  helper->cpu_.registers[kDX] = 0x0A0A;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0xFA0A);
  // Verify flags
  helper->CheckFlags(
      {{kZF, false},
       {kSF, true},  // Negative result (bit 15 is set)
       {kPF, true},
       {kCF, false},
       {kOF, false}});

  // Test 4: or dh, [di+1] - Register | Memory (byte)
  // dh = 0xFA (from 0xFA0A), di+1 = 0x0501, memory[0x0501] = 0x05
  // Result: dh = 0xFF (0xFA | 0x05 = 0xFF)
  helper->cpu_.registers[kDI] = 0x0500;
  helper->memory_[0x0501] = 0x05;
  helper->ExecuteInstructions(1);
  EXPECT_EQ((helper->cpu_.registers[kDX] >> 8) & 0xFF, 0xFF);  // High byte (DH)
  EXPECT_EQ(
      helper->cpu_.registers[kDX] & 0xFF, 0x0A);  // Low byte (DL) unchanged
  // Verify flags
  helper->CheckFlags(
      {{kZF, false},
       {kSF, true},  // Negative result (bit 7 is set)
       {kPF, true},  // Even parity for 0xFF
       {kCF, false},
       {kOF, false}});

  // Test 5: or [di-1], cl - Memory | Register (byte)
  // memory[0x04FF] = 0x33, cl = 0x0C (set it)
  // Result: memory[0x04FF] = 0x3F (0x33 | 0x0C = 0x3F)
  helper->memory_[0x04FF] = 0x33;
  helper->cpu_.registers[kCX] = (helper->cpu_.registers[kCX] & 0xFF00) | 0x0C;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x04FF], 0x3F);
  // Verify flags
  helper->CheckFlags(
      {{kZF, false},
       {kSF, false},  // Positive result (bit 7 is clear)
       {kPF, true},   // Corrected: 0x3F (00111111) has 6 set bits (even)
       {kCF, false},
       {kOF, false}});

  // Test 6: or al, 0AAh - AL | Immediate (byte)
  // al = 0x55
  // Result: al = 0xFF (0x55 | 0xAA = 0xFF) - all bits set
  helper->cpu_.registers[kAX] = (helper->cpu_.registers[kAX] & 0xFF00) | 0x55;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0xFF);
  // Verify flags
  helper->CheckFlags(
      {{kZF, false},
       {kSF, true},  // Negative result (bit 7 is set)
       {kPF, true},  // Even parity for 0xFF
       {kCF, false},
       {kOF, false}});

  // Test 7: or ax, 0AAAAh - AX | Immediate (word)
  // ax = 0x5555
  // Result: ax = 0xFFFF (0x5555 | 0xAAAA = 0xFFFF) - all bits set
  helper->cpu_.registers[kAX] = 0x5555;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0xFFFF);
  // Verify flags
  helper->CheckFlags(
      {{kZF, false},
       {kSF, true},  // Negative result (bit 15 is set)
       {kPF, true},  // Even parity for least significant byte 0xFF
       {kCF, false},
       {kOF, false}});
}

TEST_F(AndOrXorTest, XOR) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-xor-test",
      "xor ax, [bx]\n"      // Register ^ Memory (word)
      "xor [bx], cx\n"      // Memory ^ Register (word)
      "xor dx, cx\n"        // Register ^ Register (word)
      "xor dh, [di+1]\n"    // Register ^ Memory (byte)
      "xor [di-1], cl\n"    // Memory ^ Register (byte)
      "xor al, 0AAh\n"      // AL ^ Immediate (byte)
      "xor ax, 0AAAAh\n");  // AX ^ Immediate (word)
  helper->cpu_.registers[kDS] = 0;

  // Set various flags to verify they are properly affected by XOR
  SetFlag(&helper->cpu_, kCF, true);
  SetFlag(&helper->cpu_, kZF, false);
  SetFlag(&helper->cpu_, kSF, false);
  SetFlag(&helper->cpu_, kPF, false);
  SetFlag(&helper->cpu_, kOF, true);
  SetFlag(&helper->cpu_, kAF, true);

  // Test 1: xor ax, [bx] - Register ^ Memory (word)
  // ax = 0x1200, bx = 0x0400, memory[0x0400] = 0x0034
  // Result: ax = 0x1234 (0x1200 ^ 0x0034 = 0x1234)
  helper->cpu_.registers[kAX] = 0x1200;
  helper->cpu_.registers[kBX] = 0x0400;
  helper->memory_[0x0400] = 0x34;  // LSB
  helper->memory_[0x0401] = 0x00;  // MSB
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1234);
  // Verify flags: CF and OF should be cleared, others depend on result
  helper->CheckFlags(
      {{kZF, false},
       {kSF, false},
       {kPF, false},
       {kCF, false},    // CF should be cleared by XOR
       {kOF, false}});  // OF should be cleared by XOR

  // Test 2: xor [bx], cx - Memory ^ Register (word)
  // memory[0x0400] = 0x1234, cx = 0xF000
  // Result: memory[0x0400] = 0xE234 (0x1234 ^ 0xF000 = 0xE234)
  helper->cpu_.registers[kCX] = 0xF000;
  helper->memory_[0x0400] = 0x34;  // LSB
  helper->memory_[0x0401] = 0x12;  // MSB
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0400], 0x34);  // LSB
  EXPECT_EQ(helper->memory_[0x0401], 0xE2);  // MSB
  // Verify flags
  helper->CheckFlags(
      {{kZF, false},
       {kSF, true},  // Negative result (bit 15 is set)
       {kPF, false},
       {kCF, false},
       {kOF, false}});

  // Test 3: xor dx, cx - Register ^ Register (word)
  // dx = 0x0A0A, cx = 0xF000
  // Result: dx = 0xFA0A (0x0A0A ^ 0xF000 = 0xFA0A)
  helper->cpu_.registers[kDX] = 0x0A0A;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0xFA0A);
  // Verify flags
  helper->CheckFlags(
      {{kZF, false},
       {kSF, true},  // Negative result (bit 15 is set)
       {kPF, true},
       {kCF, false},
       {kOF, false}});

  // Test 4: xor dh, [di+1] - Register ^ Memory (byte)
  // dh = 0xFA (from 0xFA0A), di+1 = 0x0501, memory[0x0501] = 0x55
  // Result: dh = 0xAF (0xFA ^ 0x55 = 0xAF)
  helper->cpu_.registers[kDI] = 0x0500;
  helper->memory_[0x0501] = 0x55;
  helper->ExecuteInstructions(1);
  EXPECT_EQ((helper->cpu_.registers[kDX] >> 8) & 0xFF, 0xAF);  // High byte (DH)
  EXPECT_EQ(
      helper->cpu_.registers[kDX] & 0xFF, 0x0A);  // Low byte (DL) unchanged
  // Verify flags
  helper->CheckFlags(
      {{kZF, false},
       {kSF, true},  // Negative result (bit 7 is set)
       {kPF, true},  // Odd parity for 0xAF
       {kCF, false},
       {kOF, false}});

  // Test 5: xor [di-1], cl - Memory ^ Register (byte)
  // memory[0x04FF] = 0x33, cl = 0x0C (set it)
  // Result: memory[0x04FF] = 0x3F (0x33 ^ 0x0C = 0x3F)
  helper->memory_[0x04FF] = 0x33;
  helper->cpu_.registers[kCX] = (helper->cpu_.registers[kCX] & 0xFF00) | 0x0C;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x04FF], 0x3F);
  // Verify flags
  helper->CheckFlags(
      {{kZF, false},
       {kSF, false},  // Positive result (bit 7 is clear)
       {kPF, true},   // Odd parity
       {kCF, false},
       {kOF, false}});

  // Test 6: xor al, 0AAh - AL ^ Immediate (byte)
  // al = 0x55
  // Result: al = 0xFF (0x55 ^ 0xAA = 0xFF) - every bit is different
  helper->cpu_.registers[kAX] = (helper->cpu_.registers[kAX] & 0xFF00) | 0x55;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0xFF);
  // Verify flags
  helper->CheckFlags(
      {{kZF, false},
       {kSF, true},  // Negative result (bit 7 is set)
       {kPF, true},  // Even parity for 0xFF
       {kCF, false},
       {kOF, false}});

  // Test 7: xor ax, 0AAAAh - AX ^ Immediate (word)
  // ax = 0x5555
  // Result: ax = 0xFFFF (0x5555 ^ 0xAAAA = 0xFFFF) - every bit is different
  helper->cpu_.registers[kAX] = 0x5555;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0xFFFF);
  // Verify flags
  helper->CheckFlags(
      {{kZF, false},
       {kSF, true},  // Negative result (bit 15 is set)
       {kPF, true},  // Even parity for least significant byte 0xFF
       {kCF, false},
       {kOF, false}});

  // Test XOR with same operands - should result in zero
  auto helper2 = CPUTestHelper::CreateWithProgram(
      "execute-xor-same-test",
      "xor ax, ax\n"    // XOR register with itself
      "xor cx, cx\n");  // XOR another register with itself

  // Set some values and flags
  helper2->cpu_.registers[kAX] = 0x1234;
  helper2->cpu_.registers[kCX] = 0xABCD;
  SetFlag(&helper2->cpu_, kCF, true);
  SetFlag(&helper2->cpu_, kSF, true);
  SetFlag(&helper2->cpu_, kOF, true);

  // Test: xor ax, ax - XOR register with itself
  // Result should be 0 and ZF should be set
  helper2->ExecuteInstructions(1);
  EXPECT_EQ(helper2->cpu_.registers[kAX], 0x0000);
  helper2->CheckFlags(
      {{kZF, true}, {kSF, false}, {kPF, true}, {kCF, false}, {kOF, false}});

  // Test: xor cx, cx - XOR another register with itself
  // Result should be 0 and ZF should be set
  helper2->ExecuteInstructions(1);
  EXPECT_EQ(helper2->cpu_.registers[kCX], 0x0000);
  helper2->CheckFlags(
      {{kZF, true}, {kSF, false}, {kPF, true}, {kCF, false}, {kOF, false}});
}

TEST_F(AndOrXorTest, TEST) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-test-test",
      "test ax, bx\n"        // Register & register (word)
      "test al, 0AAh\n"      // AL & Immediate (byte)
      "test ax, 0AAAAh\n");  // AX & Immediate (word)

  // Set various flags to verify they are properly affected by TEST

  // Test 1: test ax, bx
  // ax = 0x1200, bx = 0x0034
  // Result of 0x1200 & 0x0034 is 0x0000.
  // ZF = true, SF = false, PF = true (for 0x00 LSB), CF = false, OF = false
  SetFlag(&helper->cpu_, kCF, true);
  SetFlag(&helper->cpu_, kZF, false);
  SetFlag(&helper->cpu_, kSF, false);
  SetFlag(&helper->cpu_, kPF, false);
  SetFlag(&helper->cpu_, kOF, true);
  SetFlag(&helper->cpu_, kAF, true);
  helper->cpu_.registers[kAX] = 0x1200;
  helper->cpu_.registers[kBX] = 0x0034;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1200);  // AX unchanged
  helper->CheckFlags(
      {{kZF, true}, {kSF, false}, {kPF, true}, {kCF, false}, {kOF, false}});

  // Test 2: test al, 0AAh
  // al = 0x55 (from ax = 0x1255), immediate = 0xAA
  // Result of 0x55 & 0xAA is 0x00.
  // ZF = true, SF = false, PF = true, CF = false, OF = false
  SetFlag(&helper->cpu_, kCF, true);
  SetFlag(&helper->cpu_, kZF, true);
  SetFlag(&helper->cpu_, kSF, true);
  SetFlag(&helper->cpu_, kPF, false);
  SetFlag(&helper->cpu_, kOF, true);
  SetFlag(&helper->cpu_, kAF, true);
  helper->cpu_.registers[kAX] = 0x1255;  // Set AL to 0x55, AH to 0x12
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1255);  // AX unchanged
  helper->CheckFlags(
      {{kZF, true}, {kSF, false}, {kPF, true}, {kCF, false}, {kOF, false}});

  // Test 3: test ax, 0AAAAh
  // ax = 0x5555, immediate = 0xAAAA
  // Result of 0x5555 & 0xAAAA is 0x0000.
  // ZF = true, SF = false, PF = true, CF = false, OF = false
  SetFlag(&helper->cpu_, kCF, true);
  SetFlag(&helper->cpu_, kZF, false);
  SetFlag(&helper->cpu_, kSF, true);
  SetFlag(&helper->cpu_, kPF, false);
  SetFlag(&helper->cpu_, kOF, true);
  SetFlag(&helper->cpu_, kAF, true);
  helper->cpu_.registers[kAX] = 0x5555;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x5555);  // AX unchanged
  helper->CheckFlags(
      {{kZF, true}, {kSF, false}, {kPF, true}, {kCF, false}, {kOF, false}});
}