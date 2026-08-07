#include <gtest/gtest.h>

#include "./test_helpers.h"
#include "cpu.h"

namespace {

// The full 20-bit address space, so that an address past the top of memory
// wraps around inside it rather than being rejected as out of bounds.
constexpr size_t kFullMemorySize = 0x100000;

class AddressWrapTest : public ::testing::Test {};

// A segment base plus an offset can sum past the top of the 20-bit address
// bus. The carry is dropped, so the access lands at the bottom of memory.
TEST_F(AddressWrapTest, ReadPastTopOfMemoryWrapsToBottom) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "address-wrap-read-test", "mov ax, [bx]\n", kFullMemorySize);
  // 0xFFFF0 + 0x0010 is 0x100000, which wraps to 0x00000.
  helper->cpu_.registers[kDS] = 0xFFFF;
  helper->cpu_.registers[kBX] = 0x0010;
  helper->memory_[0x00000] = 0xCD;
  helper->memory_[0x00001] = 0xAB;

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0xABCD);
}

TEST_F(AddressWrapTest, WritePastTopOfMemoryWrapsToBottom) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "address-wrap-write-test", "mov [bx], ax\n", kFullMemorySize);
  helper->cpu_.registers[kDS] = 0xFFFF;
  helper->cpu_.registers[kBX] = 0x0010;
  helper->cpu_.registers[kAX] = 0xABCD;

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x00000], 0xCD);
  EXPECT_EQ(helper->memory_[0x00001], 0xAB);
}

// The offset is 16 bits wide and wraps within its segment, so the high byte of
// a word at offset 0xFFFF comes from offset 0 of the same segment rather than
// from the paragraph above it.
TEST_F(AddressWrapTest, ReadWordAtEndOfSegmentWrapsWithinSegment) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "address-wrap-read-word-test", "mov ax, [bx]\n", kFullMemorySize);
  helper->cpu_.registers[kDS] = 0x1000;
  helper->cpu_.registers[kBX] = 0xFFFF;
  // Low byte at 0x1000:0xFFFF, high byte back at 0x1000:0x0000.
  helper->memory_[0x1FFFF] = 0xCD;
  helper->memory_[0x10000] = 0xAB;
  // A byte at the address that would be read if the offset did not wrap.
  helper->memory_[0x20000] = 0x11;

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0xABCD);
}

TEST_F(AddressWrapTest, WriteWordAtEndOfSegmentWrapsWithinSegment) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "address-wrap-write-word-test", "mov [bx], ax\n", kFullMemorySize);
  helper->cpu_.registers[kDS] = 0x1000;
  helper->cpu_.registers[kBX] = 0xFFFF;
  helper->cpu_.registers[kAX] = 0xABCD;

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x1FFFF], 0xCD);
  EXPECT_EQ(helper->memory_[0x10000], 0xAB);
  EXPECT_EQ(helper->memory_[0x20000], 0x00);
}

}  // namespace
