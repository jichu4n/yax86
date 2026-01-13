#include "gtest/gtest.h"
#include "platform.h"

namespace {

// Mock disk image pattern.
static uint8_t MockImageRead(void* context, uint8_t drive, uint32_t offset) {
  return (uint8_t)(offset & 0xFF);
}

class PlatformFDCIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.physical_memory_size = 64 * 1024; // 64KB RAM
    config_.context = this;
    config_.read_physical_memory_byte = [](PlatformState* p, uint32_t addr) -> uint8_t {
        PlatformFDCIntegrationTest* test = static_cast<PlatformFDCIntegrationTest*>(p->config->context);
        if (addr < sizeof(test->ram_)) return test->ram_[addr];
        return 0xFF;
    };
    config_.write_physical_memory_byte = [](PlatformState* p, uint32_t addr, uint8_t val) {
        PlatformFDCIntegrationTest* test = static_cast<PlatformFDCIntegrationTest*>(p->config->context);
        if (addr < sizeof(test->ram_)) test->ram_[addr] = val;
    };

    // Initialize platform.
    ASSERT_TRUE(PlatformInit(&platform_, &config_));

    // Hook FDC image callback directly.
    platform_.fdc_config.read_image_byte = MockImageRead;
  }

  void WritePort(uint16_t port, uint8_t value) {
    WritePortByte(&platform_, port, value);
  }

  uint8_t ReadPort(uint16_t port) {
    return ReadPortByte(&platform_, port);
  }

  PlatformConfig config_ = {0};
  PlatformState platform_;
  uint8_t ram_[64 * 1024];
};

TEST_F(PlatformFDCIntegrationTest, ReadSectorViaDMA) {
  // 1. Reset FDC to known state.
  // Unmask IRQ 6 in PIC (Port 0x21). Default is masked.
  WritePort(0x21, 0xBF); // Clear bit 6.

  WritePort(0x3F2, 0x00); // Reset active
  WritePort(0x3F2, 0x0C); // Reset inactive, DMA/IRQ enabled
  
  // Clear Reset Interrupt (Sense Interrupt Status 4x).
  // Tick to trigger reset interrupt.
  FDCTick(&platform_.fdc);
  // Verify IRQ 6 pending in PIC.
  // PIC not initialized with ICWs, so base vector is 0. IRQ 6 -> Vector 6.
  EXPECT_EQ(PICGetPendingInterrupt(&platform_.pic), 6); 
  // Wait, PICInit default base is usually 0x08? GLaBIOS sets it later.
  // 8259 default init state?
  // Let's assume IRQ 6 is raised.
  // Clear it by SENSE INT loop.
  for (int i=0; i<4; ++i) {
    WritePort(0x3F5, 0x08);
    FDCTick(&platform_.fdc);
    ReadPort(0x3F5); // ST0
    ReadPort(0x3F5); // PCN
  }

  // 2. Configure DMA Channel 2 for Write (Peripheral -> Memory).
  // 8237 ports: 
  // 0x0B (Mode): Channel 2, Write Transfer, AutoInit=0, Inc=0, Single Mode
  // Mode: 01 (Single) | 00 (Inc) | 00 (Auto) | 01 (Write) | 10 (Ch2)
  // Mode Byte: 01 00 01 10 = 0x46
  WritePort(0x0B, 0x46); // Single Mode, Address Increment, No Auto, Write, Ch2
  
  // Clear Flip-Flop
  WritePort(0x0C, 0x00);
  
  // Address 0x1000.
  // Ch2 Address (0x04). LSB then MSB.
  WritePort(0x04, 0x00);
  WritePort(0x04, 0x10);
  // Page Register (0x81) for Ch2. 0x00.
  WritePort(0x81, 0x00);
  
  // Count 511 (0x1FF).
  // Ch2 Count (0x05). LSB then MSB.
  WritePort(0x05, 0xFF);
  WritePort(0x05, 0x01);
  
  // Unmask Channel 2 (0x0A). 0 = Clear mask (Enable).
  WritePort(0x0A, 0x02); // Clear mask bit 2.

  // Initialize target memory with a canary pattern.
  for (int i = 0; i < 512; ++i) {
    // We can't use WriteMemoryByte directly as it's not exposed in test class helper.
    // Use helper.
    // But 'WriteMemoryByte' is global in public.h? Yes.
    WriteMemoryByte(&platform_, 0x1000 + i, 0xCC);
  }

  // 3. Issue FDC Read Data Command.
  // Insert Disk.
  FDCInsertDisk(&platform_.fdc, 0, &kFDCFormat360KB);
  
  // Command: Read Data (0x06)
  WritePort(0x3F5, 0x06);
  WritePort(0x3F5, 0x00); // Drive 0, Head 0
  WritePort(0x3F5, 0x00); // C=0
  WritePort(0x3F5, 0x00); // H=0
  WritePort(0x3F5, 0x01); // R=1
  WritePort(0x3F5, 0x02); // N=2 (512b)
  WritePort(0x3F5, 0x09); // EOT=9
  WritePort(0x3F5, 0x2A); // GPL
  WritePort(0x3F5, 0xFF); // DTL

  // 4. Run Execution Loop.
  // Drive FDC ticks until IRQ 6 is raised again.
  // Limit iterations to avoid infinite loop.
  int ticks = 0;
  bool done = false;
  while (ticks < 2000) { // 512 bytes + overhead
    FDCTick(&platform_.fdc);
    ticks++;
    // Check if IRQ 6 is raised (Transfer Complete).
    // Note: FDC raises IRQ 6 at end of command.
    // PIC status check:
    if (platform_.pic.irr & (1 << 6)) {
      done = true;
      break;
    }
  }
  
  EXPECT_TRUE(done) << "Transfer timed out.";
  
  // 5. Verify Memory Content.
  // Address 0x1000 should contain data.
  // MockImageRead(0, 0) = 0x00 -> RAM[0x1000]
  // MockImageRead(0, 1) = 0x01 -> RAM[0x1001]
  // ...
  for (int i = 0; i < 512; ++i) {
    uint8_t val = ReadMemoryByte(&platform_, 0x1000 + i);
    EXPECT_EQ(val, (uint8_t)(i & 0xFF)) << "Mismatch at offset " << i;
  }
}

} // namespace
