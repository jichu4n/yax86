#include <gtest/gtest.h>

#include "../yax86.h"
#include "./test_helpers.h"

using namespace std;

class MovTest : public ::testing::Test {};

TEST_F(MovTest, MOVRegisterAndMemory) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-mov-test",
      "mov ax, [bx]\n"    // Load a word from memory into AX
      "mov [bx], cx\n"    // Store CX into memory
      "mov dx, cx\n"      // Register to register (word)
      "mov dh, [di+1]\n"  // Load a byte from memory into high register
      "mov [di-1], cl\n"  // Store low register byte into memory
      "mov al, ch\n");    // Register to register (byte)
  helper->cpu_.registers[kDS] = 0;

  // Set various flags to verify MOV instructions don't affect them
  SetFlag(&helper->cpu_, kCF, true);
  SetFlag(&helper->cpu_, kZF, true);
  SetFlag(&helper->cpu_, kSF, true);
  SetFlag(&helper->cpu_, kPF, true);
  SetFlag(&helper->cpu_, kOF, true);
  SetFlag(&helper->cpu_, kAF, true);

  // Test 1: mov ax, [bx] - Load word from memory into AX
  // Set up: BX points to memory location 0x0400, memory contains 0x1234
  helper->cpu_.registers[kBX] = 0x0400;
  helper->memory_[0x0400] = 0x34;  // LSB
  helper->memory_[0x0401] = 0x12;  // MSB
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1234);
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 2: mov [bx], cx - Store CX into memory
  // Set up: CX contains 0x5678
  helper->cpu_.registers[kCX] = 0x5678;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0400], 0x78);  // LSB
  EXPECT_EQ(helper->memory_[0x0401], 0x56);  // MSB
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 3: mov dx, cx - Register to register (word)
  // CX still contains 0x5678
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x5678);
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 4: mov dh, [di+1] - Load a byte from memory into high register
  // Set up: DI points to 0x0500, memory at 0x0501 contains 0xAB
  helper->cpu_.registers[kDI] = 0x0500;
  helper->memory_[0x0501] = 0xAB;
  helper->ExecuteInstructions(1);
  EXPECT_EQ((helper->cpu_.registers[kDX] >> 8) & 0xFF, 0xAB);  // High byte (DH)
  EXPECT_EQ(
      helper->cpu_.registers[kDX] & 0xFF, 0x78);  // Low byte (DL) unchanged
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 5: mov [di-1], cl - Store low register byte into memory
  // CL (low byte of CX) contains 0x78
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x04FF], 0x78);
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 6: mov al, ch - Register to register (byte)
  // CH (high byte of CX) contains 0x56
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x56);  // Low byte (AL)
  EXPECT_EQ(
      (helper->cpu_.registers[kAX] >> 8) & 0xFF,
      0x12);  // High byte (AH) unchanged
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});
}

TEST_F(MovTest, MOVSegmentRegister) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-mov-segment-test",
      "mov ds, ax\n"                // Move register to segment register
      "mov ax, ds\n"                // Move segment register to register
      "mov es, [bx]\n"              // Move memory to segment register
      "mov [bx], ss\n");            // Move segment register to memory
  helper->cpu_.registers[kDS] = 0;  // Initial DS value

  // Set various flags to verify MOV instructions don't affect them
  SetFlag(&helper->cpu_, kCF, true);
  SetFlag(&helper->cpu_, kZF, true);
  SetFlag(&helper->cpu_, kSF, true);
  SetFlag(&helper->cpu_, kPF, true);
  SetFlag(&helper->cpu_, kOF, true);
  SetFlag(&helper->cpu_, kAF, true);

  // Test 1: mov ds, ax - Move register to segment register
  // Set up: AX contains 0x1234
  helper->cpu_.registers[kAX] = 0x1234;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kDS], 0x1234);
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});
  // Reset DS for next test
  helper->cpu_.registers[kDS] = 0;

  // Test 2: mov ax, ds - Move segment register to register
  // DS contains 0x1234 from previous operation
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0);
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 3: mov es, [bx] - Move memory to segment register
  // Set up: BX points to memory location 0x0500, memory contains 0x5678
  helper->cpu_.registers[kBX] = 0x0500;
  helper->memory_[0x0500] = 0x78;  // LSB
  helper->memory_[0x0501] = 0x56;  // MSB
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kES], 0x5678);
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 4: mov [bx], ss - Move segment register to memory
  // Set up: SS contains 0xABCD
  helper->cpu_.registers[kSS] = 0xABCD;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0500], 0xCD);  // LSB
  EXPECT_EQ(helper->memory_[0x0501], 0xAB);  // MSB
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});
}

TEST_F(MovTest, MOVImmediateToRegister) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-mov-immediate-test",
      "mov al, 42h\n"       // Move immediate to 8-bit low register
      "mov ch, 0AAh\n"      // Move immediate to 8-bit high register
      "mov dx, 1234h\n"     // Move immediate to 16-bit register
      "mov si, 0ABCDh\n"    // Move immediate to index register
      "mov bp, 0FFFFh\n");  // Move immediate to base pointer
  helper->cpu_.registers[kDS] = 0;

  // Set various flags to verify MOV instructions don't affect them
  SetFlag(&helper->cpu_, kCF, true);
  SetFlag(&helper->cpu_, kZF, true);
  SetFlag(&helper->cpu_, kSF, true);
  SetFlag(&helper->cpu_, kPF, true);
  SetFlag(&helper->cpu_, kOF, true);
  SetFlag(&helper->cpu_, kAF, true);

  // Test 1: mov al, 42h - Move immediate to 8-bit low register
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x42);
  // High byte (AH) should be unchanged
  EXPECT_EQ((helper->cpu_.registers[kAX] >> 8) & 0xFF, 0x00);
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 2: mov ch, 0AAh - Move immediate to 8-bit high register
  helper->ExecuteInstructions(1);
  EXPECT_EQ((helper->cpu_.registers[kCX] >> 8) & 0xFF, 0xAA);
  // Low byte (CL) should be unchanged
  EXPECT_EQ(helper->cpu_.registers[kCX] & 0xFF, 0x00);
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 3: mov dx, 1234h - Move immediate to 16-bit register
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x1234);
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 4: mov si, 0ABCDh - Move immediate to index register
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kSI], 0xABCD);
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 5: mov bp, 0FFFFh - Move immediate to base pointer
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kBP], 0xFFFF);
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});
}