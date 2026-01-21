#include <gtest/gtest.h>

#include "./test_helpers.h"
#include "cpu.h"

using namespace std;

class MovXchgXlatTest : public ::testing::Test {};

TEST_F(MovXchgXlatTest, MOVRegisterAndMemory) {
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
  CPUSetFlag(&helper->cpu_, kCF, true);
  CPUSetFlag(&helper->cpu_, kZF, true);
  CPUSetFlag(&helper->cpu_, kSF, true);
  CPUSetFlag(&helper->cpu_, kPF, true);
  CPUSetFlag(&helper->cpu_, kOF, true);
  CPUSetFlag(&helper->cpu_, kAF, true);

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

TEST_F(MovXchgXlatTest, MOVSegmentRegister) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-mov-segment-test",
      "mov ds, ax\n"                // Move register to segment register
      "mov ax, ds\n"                // Move segment register to register
      "mov es, [bx]\n"              // Move memory to segment register
      "mov [bx], ss\n");            // Move segment register to memory
  helper->cpu_.registers[kDS] = 0;  // Initial DS value

  // Set various flags to verify MOV instructions don't affect them
  CPUSetFlag(&helper->cpu_, kCF, true);
  CPUSetFlag(&helper->cpu_, kZF, true);
  CPUSetFlag(&helper->cpu_, kSF, true);
  CPUSetFlag(&helper->cpu_, kPF, true);
  CPUSetFlag(&helper->cpu_, kOF, true);
  CPUSetFlag(&helper->cpu_, kAF, true);

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

TEST_F(MovXchgXlatTest, MOVImmediateToRegister) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-mov-immediate-test",
      "mov al, 42h\n"       // Move immediate to 8-bit low register
      "mov ch, 0AAh\n"      // Move immediate to 8-bit high register
      "mov dx, 1234h\n"     // Move immediate to 16-bit register
      "mov si, 0ABCDh\n"    // Move immediate to index register
      "mov bp, 0FFFFh\n");  // Move immediate to base pointer
  helper->cpu_.registers[kDS] = 0;

  // Set various flags to verify MOV instructions don't affect them
  CPUSetFlag(&helper->cpu_, kCF, true);
  CPUSetFlag(&helper->cpu_, kZF, true);
  CPUSetFlag(&helper->cpu_, kSF, true);
  CPUSetFlag(&helper->cpu_, kPF, true);
  CPUSetFlag(&helper->cpu_, kOF, true);
  CPUSetFlag(&helper->cpu_, kAF, true);

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

TEST_F(MovXchgXlatTest, MOVMemoryOffsetAndALOrAX) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-mov-memory-offset-test",
      "mov al, [0500h]\n"    // Load a byte from direct memory address to AL
      "mov [0600h], al\n"    // Store AL to direct memory address
      "mov ax, [0700h]\n"    // Load a word from direct memory address to AX
      "mov [0800h], ax\n");  // Store AX to direct memory address

  // Set various flags to verify MOV instructions don't affect them
  CPUSetFlag(&helper->cpu_, kCF, true);
  CPUSetFlag(&helper->cpu_, kZF, true);
  CPUSetFlag(&helper->cpu_, kSF, true);
  CPUSetFlag(&helper->cpu_, kPF, true);
  CPUSetFlag(&helper->cpu_, kOF, true);
  CPUSetFlag(&helper->cpu_, kAF, true);

  // Test with DS = 0 (direct physical address = offset)
  helper->cpu_.registers[kDS] = 0;

  // Test 1: mov al, [0500h] - Load a byte from memory to AL
  helper->memory_[0x0500] = 0x42;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x42);  // AL
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 2: mov [0600h], al - Store AL to memory address
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0600], 0x42);
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 3: mov ax, [0700h] - Load a word from memory to AX
  helper->memory_[0x0700] = 0x34;  // LSB
  helper->memory_[0x0701] = 0x12;  // MSB
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

  // Test 4: mov [0800h], ax - Store AX to memory address
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x34);  // LSB
  EXPECT_EQ(helper->memory_[0x0801], 0x12);  // MSB
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Now test with DS != 0 (segment:offset addressing)
  helper = CPUTestHelper::CreateWithProgram(
      "execute-mov-memory-offset-segment-test",
      "mov al, [0050h]\n"    // Load a byte from memory offset to AL
      "mov [0060h], al\n"    // Store AL to memory offset
      "mov ax, [0070h]\n"    // Load a word from memory offset to AX
      "mov [0080h], ax\n");  // Store AX to memory offset

  helper->cpu_.registers[kDS] =
      0x80;  // DS = 0x80, so physical addr = (0x80 << 4) + offset

  // Test 5: mov al, [0050h] with DS=0x80 - Physical address = 0x0850
  helper->memory_[0x0850] = 0xAA;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0xAA);  // AL

  // Test 6: mov [0060h], al with DS=0x80 - Physical address = 0x0860
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0860], 0xAA);

  // Test 7: mov ax, [0070h] with DS=0x80 - Physical address = 0x0870
  helper->memory_[0x0870] = 0xCD;  // LSB
  helper->memory_[0x0871] = 0xAB;  // MSB
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0xABCD);

  // Test 8: mov [0080h], ax with DS=0x80 - Physical address = 0x0880
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0880], 0xCD);  // LSB
  EXPECT_EQ(helper->memory_[0x0881], 0xAB);  // MSB
}

TEST_F(MovXchgXlatTest, MOVImmediateToRegisterOrMemory) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-mov-immediate-to-reg-mem-test",
      "mov byte [bx], 42h\n"      // Move immediate byte to memory
      "mov word [bx+2], 1234h\n"  // Move immediate word to memory
      "mov byte [si], 0AAh\n"  // Move immediate byte to another memory location
      "mov word [di], 0ABCDh\n"  // Move immediate word to another memory
                                 // location
      "mov cl, 55h\n"      // Move immediate to register (already tested, for
                           // comparison)
      "mov dx, 5678h\n");  // Move immediate to register (already tested, for
                           // comparison)
  helper->cpu_.registers[kDS] = 0;

  // Set various flags to verify MOV instructions don't affect them
  CPUSetFlag(&helper->cpu_, kCF, true);
  CPUSetFlag(&helper->cpu_, kZF, true);
  CPUSetFlag(&helper->cpu_, kSF, true);
  CPUSetFlag(&helper->cpu_, kPF, true);
  CPUSetFlag(&helper->cpu_, kOF, true);
  CPUSetFlag(&helper->cpu_, kAF, true);

  // Setup memory addresses
  helper->cpu_.registers[kBX] = 0x0400;
  helper->cpu_.registers[kSI] = 0x0500;
  helper->cpu_.registers[kDI] = 0x0600;

  // Test 1: mov byte ptr [bx], 42h - Move immediate byte to memory
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0400], 0x42);
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 2: mov word ptr [bx+2], 1234h - Move immediate word to memory with
  // displacement
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0402], 0x34);  // LSB
  EXPECT_EQ(helper->memory_[0x0403], 0x12);  // MSB
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 3: mov byte ptr [si], 0AAh - Move immediate byte to memory via SI
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0500], 0xAA);
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 4: mov word ptr [di], 0ABCDh - Move immediate word to memory via DI
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0600], 0xCD);  // LSB
  EXPECT_EQ(helper->memory_[0x0601], 0xAB);  // MSB
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 5: mov cl, 55h - Move immediate byte to register (opcode 0xB1)
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kCX] & 0xFF, 0x55);  // CL
  // Verify flags are still set after MOV instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 6: mov dx, 5678h - Move immediate word to register (opcode 0xBA)
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

  // Test with DS != 0 (segment:offset addressing)
  helper = CPUTestHelper::CreateWithProgram(
      "execute-mov-immediate-to-mem-segment-test",
      "mov byte [0050h], 42h\n"      // Move immediate byte to memory offset
      "mov word [0060h], 1234h\n");  // Move immediate word to memory offset

  helper->cpu_.registers[kDS] =
      0x80;  // DS = 0x80, so physical addr = 0x80 << 4 + offset

  // Test 9: mov byte ptr [0050h], 42h - Move immediate byte to memory offset
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0850], 0x42);

  // Test 10: mov word ptr [0060h], 1234h - Move immediate word to memory offset
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0860], 0x34);  // LSB
  EXPECT_EQ(helper->memory_[0x0861], 0x12);  // MSB
}

TEST_F(MovXchgXlatTest, XCHGRegister) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-xchg-register-test",
      "xchg ax, ax\n"    // NOP (special case)
      "xchg ax, bx\n"    // Exchange AX with BX
      "xchg cx, dx\n"    // Exchange CX with DX
      "xchg sp, bp\n"    // Exchange SP with BP
      "xchg si, di\n"    // Exchange SI with DI
      "xchg ax, di\n");  // Exchange AX with DI
  helper->cpu_.registers[kDS] = 0;

  // Set various flags to verify XCHG instructions don't affect them
  CPUSetFlag(&helper->cpu_, kCF, true);
  CPUSetFlag(&helper->cpu_, kZF, true);
  CPUSetFlag(&helper->cpu_, kSF, true);
  CPUSetFlag(&helper->cpu_, kPF, true);
  CPUSetFlag(&helper->cpu_, kOF, true);
  CPUSetFlag(&helper->cpu_, kAF, true);

  // Test 1: xchg ax, ax - NOP operation
  // Set up: AX contains 0x1234
  helper->cpu_.registers[kAX] = 0x1234;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1234);  // No change
  // Verify flags are still set after XCHG instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 2: xchg ax, bx - Exchange AX with BX
  // Set up: AX=0x1234, BX=0x5678
  helper->cpu_.registers[kAX] = 0x1234;
  helper->cpu_.registers[kBX] = 0x5678;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x5678);
  EXPECT_EQ(helper->cpu_.registers[kBX], 0x1234);
  // Verify flags are still set after XCHG instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 3: xchg cx, dx - Exchange CX with DX
  // Set up: CX=0xABCD, DX=0xEF01
  helper->cpu_.registers[kCX] = 0xABCD;
  helper->cpu_.registers[kDX] = 0xEF01;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0xEF01);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0xABCD);
  // Verify flags are still set after XCHG instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 4: xchg sp, bp - Exchange SP with BP
  // Set up: SP=0x2000, BP=0x3000
  helper->cpu_.registers[kSP] = 0x2000;
  helper->cpu_.registers[kBP] = 0x3000;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kSP], 0x3000);
  EXPECT_EQ(helper->cpu_.registers[kBP], 0x2000);
  // Verify flags are still set after XCHG instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 5: xchg si, di - Exchange SI with DI
  // Set up: SI=0x4000, DI=0x5000
  helper->cpu_.registers[kSI] = 0x4000;
  helper->cpu_.registers[kDI] = 0x5000;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x5000);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x4000);
  // Verify flags are still set after XCHG instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 6: xchg ax, di - Exchange AX with DI
  // Set up: AX=0xAABB, DI=0x4000
  helper->cpu_.registers[kAX] = 0xAABB;
  helper->cpu_.registers[kDI] = 0x4000;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x4000);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0xAABB);
  // Verify flags are still set after XCHG instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});
}

TEST_F(MovXchgXlatTest, XCHGRegisterAndMemory) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-xchg-register-memory-test",
      "xchg al, [bx]\n"      // Exchange AL with byte in memory
      "xchg ch, [bx+1]\n"    // Exchange CH with byte in memory (with
                             // displacement)
      "xchg dx, [si]\n"      // Exchange DX with word in memory
      "xchg bp, [di+2]\n");  // Exchange BP with word in memory (with
                             // displacement)
  helper->cpu_.registers[kDS] = 0;

  // Set various flags to verify XCHG instructions don't affect them
  CPUSetFlag(&helper->cpu_, kCF, true);
  CPUSetFlag(&helper->cpu_, kZF, true);
  CPUSetFlag(&helper->cpu_, kSF, true);
  CPUSetFlag(&helper->cpu_, kPF, true);
  CPUSetFlag(&helper->cpu_, kOF, true);
  CPUSetFlag(&helper->cpu_, kAF, true);

  // Test 1: xchg al, [bx] - Exchange AL with byte in memory
  // Set up: AL=0x42, memory at BX=0x0400 contains 0x78
  helper->cpu_.registers[kAX] = 0x1142;  // AL = 0x42, AH = 0x11
  helper->cpu_.registers[kBX] = 0x0400;
  helper->memory_[0x0400] = 0x78;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX] & 0xFF, 0x78);  // AL now has memory value
  EXPECT_EQ((helper->cpu_.registers[kAX] >> 8) & 0xFF, 0x11);  // AH unchanged
  EXPECT_EQ(helper->memory_[0x0400], 0x42);  // Memory now has AL's value
  // Verify flags are still set after XCHG instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 2: xchg ch, [bx+1] - Exchange CH with byte in memory (with
  // displacement) Set up: CX=0x5500 (CH=0x55), memory at BX+1=0x0401 contains
  // 0xAA
  helper->cpu_.registers[kCX] = 0x5500;  // CH = 0x55, CL = 0x00
  helper->memory_[0x0401] = 0xAA;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      (helper->cpu_.registers[kCX] >> 8) & 0xFF,
      0xAA);  // CH now has memory value
  EXPECT_EQ(helper->cpu_.registers[kCX] & 0xFF, 0x00);  // CL unchanged
  EXPECT_EQ(helper->memory_[0x0401], 0x55);  // Memory now has CH's value
  // Verify flags are still set after XCHG instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 3: xchg dx, [si] - Exchange DX with word in memory
  // Set up: DX=0x1234, memory at SI=0x0500 contains 0x5678
  helper->cpu_.registers[kDX] = 0x1234;
  helper->cpu_.registers[kSI] = 0x0500;
  helper->memory_[0x0500] = 0x78;  // LSB
  helper->memory_[0x0501] = 0x56;  // MSB
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x5678);  // DX now has memory value
  EXPECT_EQ(helper->memory_[0x0500], 0x34);        // LSB of memory
  EXPECT_EQ(helper->memory_[0x0501], 0x12);        // MSB of memory
  // Verify flags are still set after XCHG instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test 4: xchg bp, [di+2] - Exchange BP with word in memory (with
  // displacement) Set up: BP=0xABCD, memory at DI+2=0x0602 contains 0xEF01
  helper->cpu_.registers[kBP] = 0xABCD;
  helper->cpu_.registers[kDI] = 0x0600;
  helper->memory_[0x0602] = 0x01;  // LSB
  helper->memory_[0x0603] = 0xEF;  // MSB
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kBP], 0xEF01);  // BP now has memory value
  EXPECT_EQ(helper->memory_[0x0602], 0xCD);        // LSB of memory
  EXPECT_EQ(helper->memory_[0x0603], 0xAB);        // MSB of memory
  // Verify flags are still set after XCHG instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});
}

TEST_F(MovXchgXlatTest, XLAT) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-xlat-test",
      "xlatb\n");                   // XLATB is an alias for XLAT
  helper->cpu_.registers[kDS] = 0;  // Assume DS is 0 for direct addressing

  // Set various flags to verify XLAT instructions don't affect them
  CPUSetFlag(&helper->cpu_, kCF, true);
  CPUSetFlag(&helper->cpu_, kZF, true);
  CPUSetFlag(&helper->cpu_, kSF, true);
  CPUSetFlag(&helper->cpu_, kPF, true);
  CPUSetFlag(&helper->cpu_, kOF, true);
  CPUSetFlag(&helper->cpu_, kAF, true);

  // Test XLAT: AL should be replaced by the value at [DS:BX+AL]
  // Set up: BX = 0x0700 (table base), AL = 0x05 (index)
  // Memory at [0x0700 + 0x05] = 0x0705 contains 0xAB
  helper->cpu_.registers[kBX] = 0x0700;
  helper->cpu_.registers[kAX] =
      0xCC05;  // AL = 0x05, AH = 0xCC (to check AH is unchanged)
  helper->memory_[0x0705] = 0xAB;

  helper->ExecuteInstructions(1);

  // Verify AL is updated
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0xAB);  // AL should be 0xAB
  // Verify AH is unchanged
  EXPECT_EQ(
      (helper->cpu_.registers[kAX] >> 8) & 0xFF,
      0xCC);  // AH should still be 0xCC

  // Verify flags are still set after XLAT instruction
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true}});

  // Test with a different index and value
  // Set up: BX = 0x0800, AL = 0x0A
  // Memory at [0x0800 + 0x0A] = 0x080A contains 0x42
  helper = CPUTestHelper::CreateWithProgram("execute-xlat-test-2", "xlat");
  helper->cpu_.registers[kDS] = 0;
  CPUSetFlag(&helper->cpu_, kCF, false);  // Change some flags for variety
  CPUSetFlag(&helper->cpu_, kZF, false);
  CPUSetFlag(&helper->cpu_, kSF, false);
  CPUSetFlag(&helper->cpu_, kPF, false);
  CPUSetFlag(&helper->cpu_, kOF, false);
  CPUSetFlag(&helper->cpu_, kAF, false);

  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0xDD0A;  // AL = 0x0A, AH = 0xDD
  helper->memory_[0x080A] = 0x42;

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x42);  // AL should be 0x42
  EXPECT_EQ(
      (helper->cpu_.registers[kAX] >> 8) & 0xFF,
      0xDD);  // AH should still be 0xDD

  helper->CheckFlags(
      {{kCF, false},
       {kZF, false},
       {kSF, false},
       {kPF, false},
       {kOF, false},
       {kAF, false}});
}