#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "hdc.h"

namespace {

// A geometry small enough to make out-of-range addresses easy to construct.
constexpr HDCDriveGeometry kTestGeometry = {
    /*num_cylinders=*/10,
    /*num_heads=*/2,
    /*num_sectors_per_track=*/4,
};

class HDCTaskFileTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.context = this;
    HDCInit(&hdc_, &config_);
    HDCAttachDrive(&hdc_, 0, &kTestGeometry);
  }

  // Reads and writes go through the physical port, so every test exercises the
  // card's address line swap rather than assuming it.
  uint8_t Read(HDCRegister reg) {
    return HDCReadPort(
        &hdc_, kHDCPortBase + HDCPortOffsetToRegister((uint8_t)reg));
  }

  void Write(HDCRegister reg, uint8_t value) {
    HDCWritePort(
        &hdc_, kHDCPortBase + HDCPortOffsetToRegister((uint8_t)reg), value);
  }

  void SelectDrive(uint8_t drive) {
    Write(
        kHDCRegisterDriveHead,
        drive == 0 ? 0 : (uint8_t)kHDCDriveHeadDriveSelect);
  }

  // Issues a command and returns the status register afterwards.
  uint8_t RunCommand(uint8_t opcode) {
    Write(kHDCRegisterStatus, opcode);
    return Read(kHDCRegisterStatus);
  }

  // Drains a 512-byte block out of the data register, alternating the low and
  // high byte ports the way the option ROM does.
  std::vector<uint8_t> DrainBuffer() {
    std::vector<uint8_t> block;
    for (int i = 0; i < kHDCSectorSize; ++i) {
      block.push_back(
          Read(i % 2 == 0 ? kHDCRegisterData : kHDCRegisterDataHigh));
    }
    return block;
  }

  static uint16_t WordAt(const std::vector<uint8_t>& block, int word_index) {
    return (uint16_t)(block[word_index * 2] | (block[word_index * 2 + 1] << 8));
  }

  // ATA strings are byte swapped within each word.
  static std::string StringAt(
      const std::vector<uint8_t>& block, int word_index, int num_words) {
    std::string text;
    for (int i = 0; i < num_words; ++i) {
      const uint16_t word = WordAt(block, word_index + i);
      text.push_back((char)(word >> 8));
      text.push_back((char)(word & 0xFF));
    }
    return text;
  }

  HDCConfig config_ = {0};
  HDCState hdc_ = {0};
};

TEST_F(HDCTaskFileTest, PortOffsetToRegisterMatchesTheCardWiring) {
  // The values the option ROM was observed to drive: it selects a drive at
  // port offset 6 and polls status at port offset 0xE.
  EXPECT_EQ(HDCPortOffsetToRegister(0x6), kHDCRegisterDriveHead);
  EXPECT_EQ(HDCPortOffsetToRegister(0xE), kHDCRegisterStatus);
  EXPECT_EQ(HDCPortOffsetToRegister(0x8), kHDCRegisterError);
  EXPECT_EQ(HDCPortOffsetToRegister(0x0), kHDCRegisterData);
  EXPECT_EQ(HDCPortOffsetToRegister(0x1), kHDCRegisterDataHigh);
  EXPECT_EQ(HDCPortOffsetToRegister(0x2), kHDCRegisterSectorCount);
  EXPECT_EQ(HDCPortOffsetToRegister(0xA), kHDCRegisterSectorNumber);
  EXPECT_EQ(HDCPortOffsetToRegister(0x4), kHDCRegisterCylinderLow);
  EXPECT_EQ(HDCPortOffsetToRegister(0xC), kHDCRegisterCylinderHigh);

  // Swapping two address lines is its own inverse.
  for (uint8_t offset = 0; offset < kHDCNumPorts; ++offset) {
    EXPECT_EQ(HDCPortOffsetToRegister(HDCPortOffsetToRegister(offset)), offset);
  }
}

TEST_F(HDCTaskFileTest, AddressRegistersReadBackThroughSwappedPorts) {
  Write(kHDCRegisterSectorCount, 0x12);
  Write(kHDCRegisterSectorNumber, 0x34);
  Write(kHDCRegisterCylinderLow, 0x56);
  Write(kHDCRegisterCylinderHigh, 0x78);
  Write(kHDCRegisterDriveHead, 0x9A);

  EXPECT_EQ(Read(kHDCRegisterSectorCount), 0x12);
  EXPECT_EQ(Read(kHDCRegisterSectorNumber), 0x34);
  EXPECT_EQ(Read(kHDCRegisterCylinderLow), 0x56);
  EXPECT_EQ(Read(kHDCRegisterCylinderHigh), 0x78);
  EXPECT_EQ(Read(kHDCRegisterDriveHead), 0x9A);
}

TEST_F(HDCTaskFileTest, AttachedDriveReportsReady) {
  SelectDrive(0);
  EXPECT_TRUE(Read(kHDCRegisterStatus) & kHDCStatusReady);
  EXPECT_FALSE(Read(kHDCRegisterStatus) & kHDCStatusBusy);
  EXPECT_FALSE(Read(kHDCRegisterStatus) & kHDCStatusError);
}

TEST_F(HDCTaskFileTest, EmptyDriveSlotNeverBecomesReady) {
  // This is how the option ROM concludes there is no slave: it selects the
  // drive, polls status, and gives up when the ready bit never appears.
  SelectDrive(1);
  EXPECT_EQ(Read(kHDCRegisterStatus), 0);

  RunCommand(kHDCCommandIdentifyDevice);
  EXPECT_EQ(Read(kHDCRegisterStatus), 0);
}

TEST_F(HDCTaskFileTest, CommandsToAnEmptySlotLeaveTheOtherDriveAlone) {
  // The status register is shared by both drives, so a command aimed at the
  // absent slave must not disturb the master. A guest that polls for ready
  // before issuing a command would otherwise wait forever.
  SelectDrive(1);
  RunCommand(kHDCCommandIdentifyDevice);

  SelectDrive(0);
  EXPECT_TRUE(Read(kHDCRegisterStatus) & kHDCStatusReady);
  EXPECT_FALSE(Read(kHDCRegisterStatus) & kHDCStatusError);
}

TEST_F(HDCTaskFileTest, IdentifyDeviceReportsGeometry) {
  SelectDrive(0);
  const uint8_t status = RunCommand(kHDCCommandIdentifyDevice);
  ASSERT_TRUE(status & kHDCStatusDataRequest);
  ASSERT_FALSE(status & kHDCStatusError);

  const std::vector<uint8_t> block = DrainBuffer();

  // Fixed disk.
  EXPECT_EQ(WordAt(block, 0) & (1 << 6), 1 << 6);
  EXPECT_EQ(WordAt(block, 1), kTestGeometry.num_cylinders);
  EXPECT_EQ(WordAt(block, 3), kTestGeometry.num_heads);
  EXPECT_EQ(WordAt(block, 6), kTestGeometry.num_sectors_per_track);
  EXPECT_EQ(WordAt(block, 5), kHDCSectorSize);

  // LBA supported.
  EXPECT_EQ(WordAt(block, 49) & (1 << 9), 1 << 9);
  // Words 54-58 are valid.
  EXPECT_EQ(WordAt(block, 53) & 1, 1);
  EXPECT_EQ(WordAt(block, 54), kTestGeometry.num_cylinders);
  EXPECT_EQ(WordAt(block, 55), kTestGeometry.num_heads);
  EXPECT_EQ(WordAt(block, 56), kTestGeometry.num_sectors_per_track);

  const uint32_t num_sectors = (uint32_t)kTestGeometry.num_cylinders *
                               kTestGeometry.num_heads *
                               kTestGeometry.num_sectors_per_track;
  EXPECT_EQ(
      (uint32_t)WordAt(block, 60) | ((uint32_t)WordAt(block, 61) << 16),
      num_sectors);
  EXPECT_EQ(
      (uint32_t)WordAt(block, 57) | ((uint32_t)WordAt(block, 58) << 16),
      num_sectors);
}

TEST_F(HDCTaskFileTest, IdentifyDeviceDoesNotAdvertiseMultipleMode) {
  SelectDrive(0);
  RunCommand(kHDCCommandIdentifyDevice);
  const std::vector<uint8_t> block = DrainBuffer();

  // Zero is how ATA says Read Multiple and Write Multiple are unavailable.
  // Anything else would contradict Set Multiple Mode aborting.
  EXPECT_EQ(WordAt(block, 47) & 0xFF, 0);

  enum { kSetMultipleMode = 0xC6 };
  const uint8_t status = RunCommand(kSetMultipleMode);
  EXPECT_TRUE(status & kHDCStatusError);
  EXPECT_TRUE(Read(kHDCRegisterError) & kHDCErrorAborted);
}

TEST_F(HDCTaskFileTest, IdentifyDeviceStringsAreByteSwapped) {
  SelectDrive(0);
  RunCommand(kHDCCommandIdentifyDevice);
  const std::vector<uint8_t> block = DrainBuffer();

  // Space padded to the full field width, and readable rather than scrambled.
  EXPECT_EQ(
      StringAt(block, 27, 20), "YAX86 HARD DISK                         ");
  EXPECT_EQ(StringAt(block, 10, 10), "YAX86-0000001       ");
  EXPECT_EQ(StringAt(block, 23, 4), "1.0     ");
}

TEST_F(HDCTaskFileTest, DataRequestClearsWhenTheBlockIsDrained) {
  SelectDrive(0);
  RunCommand(kHDCCommandIdentifyDevice);

  for (int i = 0; i < kHDCSectorSize - 1; ++i) {
    Read(kHDCRegisterData);
    ASSERT_TRUE(Read(kHDCRegisterStatus) & kHDCStatusDataRequest)
        << "cleared early after " << (i + 1) << " bytes";
  }
  Read(kHDCRegisterData);
  EXPECT_FALSE(Read(kHDCRegisterStatus) & kHDCStatusDataRequest);
}

TEST_F(HDCTaskFileTest, DataRegisterReadWithNoTransferReturnsZero) {
  SelectDrive(0);
  EXPECT_EQ(Read(kHDCRegisterData), 0);
}

TEST_F(HDCTaskFileTest, InitializeDeviceParametersSetsTranslatedGeometry) {
  SelectDrive(0);
  // Address the drive as 1 head of 8 sectors rather than 2 heads of 4.
  Write(kHDCRegisterSectorCount, 8);
  Write(kHDCRegisterDriveHead, 0);  // heads - 1 == 0
  const uint8_t status = RunCommand(kHDCCommandInitializeDeviceParameters);
  EXPECT_FALSE(status & kHDCStatusError);

  RunCommand(kHDCCommandIdentifyDevice);
  const std::vector<uint8_t> block = DrainBuffer();
  // The physical geometry is unchanged, but the current one now reflects the
  // translation.
  EXPECT_EQ(WordAt(block, 3), kTestGeometry.num_heads);
  EXPECT_EQ(WordAt(block, 55), 1);
  EXPECT_EQ(WordAt(block, 56), 8);
}

TEST_F(HDCTaskFileTest, RecalibrateAndSeekSucceedAcrossTheirStepRates) {
  SelectDrive(0);
  for (uint8_t step = 0; step < 0x10; ++step) {
    EXPECT_EQ(
        RunCommand((uint8_t)(kHDCCommandRecalibrate + step)),
        kHDCStatusReady | kHDCStatusSeekComplete)
        << "recalibrate step rate " << (int)step;
    EXPECT_EQ(
        RunCommand((uint8_t)(kHDCCommandSeek + step)),
        kHDCStatusReady | kHDCStatusSeekComplete)
        << "seek step rate " << (int)step;
  }
}

TEST_F(HDCTaskFileTest, ReadVerifyAcceptsAnAddressableSector) {
  SelectDrive(0);
  Write(kHDCRegisterCylinderLow, 9);
  Write(kHDCRegisterCylinderHigh, 0);
  Write(kHDCRegisterDriveHead, 1);  // head 1
  Write(kHDCRegisterSectorNumber, 4);

  const uint8_t status = RunCommand(kHDCCommandReadVerifySectors);
  EXPECT_FALSE(status & kHDCStatusError);
  EXPECT_EQ(Read(kHDCRegisterError), 0);
}

TEST_F(HDCTaskFileTest, ReadVerifyRejectsAddressesOffTheDrive) {
  SelectDrive(0);
  struct Address {
    const char* what;
    uint8_t cylinder_low;
    uint8_t head;
    uint8_t sector;
  };
  const Address kBadAddresses[] = {
      {"cylinder past the end", 10, 0, 1},
      {"head past the end", 0, 2, 1},
      {"sector past the end", 0, 0, 5},
      {"sector zero", 0, 0, 0},
  };

  for (const Address& address : kBadAddresses) {
    Write(kHDCRegisterCylinderLow, address.cylinder_low);
    Write(kHDCRegisterCylinderHigh, 0);
    Write(kHDCRegisterDriveHead, address.head);
    Write(kHDCRegisterSectorNumber, address.sector);

    const uint8_t status = RunCommand(kHDCCommandReadVerifySectors);
    EXPECT_TRUE(status & kHDCStatusError) << address.what;
    EXPECT_TRUE(Read(kHDCRegisterError) & kHDCErrorIDNotFound) << address.what;
  }
}

TEST_F(HDCTaskFileTest, ReadVerifyAcceptsLogicalBlockAddresses) {
  SelectDrive(0);
  const uint32_t last_sector = (uint32_t)kTestGeometry.num_cylinders *
                                   kTestGeometry.num_heads *
                                   kTestGeometry.num_sectors_per_track -
                               1;
  Write(kHDCRegisterSectorNumber, (uint8_t)(last_sector & 0xFF));
  Write(kHDCRegisterCylinderLow, (uint8_t)((last_sector >> 8) & 0xFF));
  Write(kHDCRegisterCylinderHigh, (uint8_t)((last_sector >> 16) & 0xFF));
  Write(
      kHDCRegisterDriveHead,
      (uint8_t)(kHDCDriveHeadLBA | ((last_sector >> 24) & 0x0F)));
  EXPECT_FALSE(RunCommand(kHDCCommandReadVerifySectors) & kHDCStatusError);

  // One past the last sector is off the drive.
  Write(kHDCRegisterSectorNumber, (uint8_t)((last_sector + 1) & 0xFF));
  Write(kHDCRegisterCylinderLow, (uint8_t)(((last_sector + 1) >> 8) & 0xFF));
  EXPECT_TRUE(RunCommand(kHDCCommandReadVerifySectors) & kHDCStatusError);
  EXPECT_TRUE(Read(kHDCRegisterError) & kHDCErrorIDNotFound);
}

TEST_F(HDCTaskFileTest, UnsupportedCommandsAbort) {
  SelectDrive(0);
  // Read Multiple, which this controller does not implement, and 0xFF, which
  // is not a command at all.
  for (uint8_t opcode : {(uint8_t)0xC4, (uint8_t)0xFF}) {
    const uint8_t status = RunCommand(opcode);
    EXPECT_TRUE(status & kHDCStatusError) << "opcode " << (int)opcode;
    EXPECT_TRUE(Read(kHDCRegisterError) & kHDCErrorAborted)
        << "opcode " << (int)opcode;
  }
}

TEST_F(HDCTaskFileTest, DetachedDriveStopsResponding) {
  SelectDrive(0);
  ASSERT_TRUE(Read(kHDCRegisterStatus) & kHDCStatusReady);

  HDCDetachDrive(&hdc_, 0);
  EXPECT_EQ(Read(kHDCRegisterStatus), 0);
}

TEST_F(HDCTaskFileTest, UnmappedPortOffsetsReadAsOpenBus) {
  SelectDrive(0);
  // Offsets that no register decodes to.
  for (uint8_t offset : {0x3, 0x5, 0x7, 0x9, 0xB, 0xD, 0xF}) {
    EXPECT_EQ(HDCReadPort(&hdc_, kHDCPortBase + offset), kHDCOpenBusValue)
        << "offset " << (int)offset;
  }
}

}  // namespace
