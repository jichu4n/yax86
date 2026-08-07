// The 8088 drives a 20 bit address bus, so a segment and offset that together
// exceed 1MB wrap around to the bottom of the address space rather than
// addressing memory that does not exist.

#include <gtest/gtest.h>

#include <vector>

#include "cpu.h"

namespace {

// Addresses the CPU put on the bus, in order.
std::vector<uint32_t> g_read_addresses;
std::vector<uint32_t> g_write_addresses;
// Instruction bytes, served from wherever the CPU asks.
uint8_t g_code[8];

uint8_t RecordingReadMemoryByte(YAX86_UNUSED CPUState* cpu, uint32_t address) {
  g_read_addresses.push_back(address);
  // Serve the instruction from the wrapped address the test expects.
  const size_t index = g_read_addresses.size() - 1;
  return index < sizeof(g_code) ? g_code[index] : 0x90;
}

void RecordingWriteMemoryByte(
    YAX86_UNUSED CPUState* cpu, uint32_t address, YAX86_UNUSED uint8_t value) {
  g_write_addresses.push_back(address);
}

class AddressWrapTest : public ::testing::Test {
 protected:
  void SetUp() override {
    g_read_addresses.clear();
    g_write_addresses.clear();
    config_ = CPUConfig{};
    config_.read_memory_byte = RecordingReadMemoryByte;
    config_.write_memory_byte = RecordingWriteMemoryByte;
    CPUInit(&cpu_, &config_);
  }

  CPUConfig config_ = {0};
  CPUState cpu_;
};

TEST_F(AddressWrapTest, InstructionFetchWrapsAtTheTopOfMemory) {
  // NOP at FFFF:0010, which is linear 0x100000 and must wrap to 0x00000.
  g_code[0] = 0x90;
  cpu_.registers[kCS] = 0xFFFF;
  cpu_.registers[kIP] = 0x0010;

  CPUTick(&cpu_);

  ASSERT_FALSE(g_read_addresses.empty());
  EXPECT_EQ(g_read_addresses[0], 0x00000u);
}

TEST_F(AddressWrapTest, DataReadWrapsAtTheTopOfMemory) {
  // MOV AL, [0020h] with DS = FFFF, so the operand is at linear 0x100010.
  g_code[0] = 0xA0;
  g_code[1] = 0x20;
  g_code[2] = 0x00;
  cpu_.registers[kCS] = 0;
  cpu_.registers[kIP] = 0;
  cpu_.registers[kDS] = 0xFFFF;

  CPUTick(&cpu_);

  // The last read is the operand, after the three instruction bytes.
  ASSERT_GE(g_read_addresses.size(), 4u);
  EXPECT_EQ(g_read_addresses.back(), 0x00010u);
}

TEST_F(AddressWrapTest, DataWriteWrapsAtTheTopOfMemory) {
  // MOV [0020h], AL with DS = FFFF.
  g_code[0] = 0xA2;
  g_code[1] = 0x20;
  g_code[2] = 0x00;
  cpu_.registers[kCS] = 0;
  cpu_.registers[kIP] = 0;
  cpu_.registers[kDS] = 0xFFFF;

  CPUTick(&cpu_);

  ASSERT_EQ(g_write_addresses.size(), 1u);
  EXPECT_EQ(g_write_addresses[0], 0x00010u);
}

TEST_F(AddressWrapTest, WordAccessStraddlingTheTopOfMemoryWraps) {
  // MOV AX, [000Fh] with DS = FFFF puts the low byte at linear 0xFFFFF and the
  // high byte one past it, which wraps to 0x00000.
  g_code[0] = 0xA1;
  g_code[1] = 0x0F;
  g_code[2] = 0x00;
  cpu_.registers[kCS] = 0;
  cpu_.registers[kIP] = 0;
  cpu_.registers[kDS] = 0xFFFF;

  CPUTick(&cpu_);

  ASSERT_GE(g_read_addresses.size(), 5u);
  EXPECT_EQ(g_read_addresses[g_read_addresses.size() - 2], 0xFFFFFu);
  EXPECT_EQ(g_read_addresses.back(), 0x00000u);
}

}  // namespace
