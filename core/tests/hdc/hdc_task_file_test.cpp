#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "hdc.h"

namespace {

// A geometry small enough to make out-of-range addresses easy to construct.
constexpr YHDCDriveGeometry kTestGeometry = {
    /*num_cylinders=*/10,
    /*num_heads=*/2,
    /*num_sectors_per_track=*/4,
};

class HDCTaskFileTest : public ::testing::Test {
 protected:
  void SetUp() override {
    hdc_.config.context = this;
    YHDCInit(&hdc_);
    YHDCAttachDrive(&hdc_, 0, &kTestGeometry);
  }

  // Reads and writes go through the physical port, so every test exercises the
  // card's address line swap rather than assuming it.
  uint8_t Read(YHDCRegister reg) {
    return YHDCReadPort(
        &hdc_, Y_HDC_PORT_BASE + YHDCPortOffsetToRegister((uint8_t)reg));
  }

  void Write(YHDCRegister reg, uint8_t value) {
    YHDCWritePort(
        &hdc_, Y_HDC_PORT_BASE + YHDCPortOffsetToRegister((uint8_t)reg), value);
  }

  void SelectDrive(uint8_t drive) {
    Write(
        Y_HDC_REGISTER_DRIVE_HEAD,
        drive == 0 ? 0 : (uint8_t)Y_HDC_DRIVE_HEAD_DRIVE_SELECT);
  }

  // Issues a command and returns the status register afterwards.
  uint8_t RunCommand(uint8_t opcode) {
    Write(Y_HDC_REGISTER_STATUS, opcode);
    return Read(Y_HDC_REGISTER_STATUS);
  }

  // Drains a 512-byte block out of the data register, alternating the low and
  // high byte ports the way the option ROM does.
  std::vector<uint8_t> DrainBuffer() {
    std::vector<uint8_t> block;
    for (int i = 0; i < Y_HDC_SECTOR_SIZE; ++i) {
      block.push_back(
          Read(i % 2 == 0 ? Y_HDC_REGISTER_DATA : Y_HDC_REGISTER_DATA_HIGH));
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
  YHDCState hdc_ = {0};
};

TEST_F(HDCTaskFileTest, PortOffsetToRegisterMatchesTheCardWiring) {
  // The values the option ROM was observed to drive: it selects a drive at
  // port offset 6 and polls status at port offset 0xE.
  EXPECT_EQ(YHDCPortOffsetToRegister(0x6), Y_HDC_REGISTER_DRIVE_HEAD);
  EXPECT_EQ(YHDCPortOffsetToRegister(0xE), Y_HDC_REGISTER_STATUS);
  EXPECT_EQ(YHDCPortOffsetToRegister(0x8), Y_HDC_REGISTER_ERROR);
  EXPECT_EQ(YHDCPortOffsetToRegister(0x0), Y_HDC_REGISTER_DATA);
  EXPECT_EQ(YHDCPortOffsetToRegister(0x1), Y_HDC_REGISTER_DATA_HIGH);
  EXPECT_EQ(YHDCPortOffsetToRegister(0x2), Y_HDC_REGISTER_SECTOR_COUNT);
  EXPECT_EQ(YHDCPortOffsetToRegister(0xA), Y_HDC_REGISTER_SECTOR_NUMBER);
  EXPECT_EQ(YHDCPortOffsetToRegister(0x4), Y_HDC_REGISTER_CYLINDER_LOW);
  EXPECT_EQ(YHDCPortOffsetToRegister(0xC), Y_HDC_REGISTER_CYLINDER_HIGH);
  EXPECT_EQ(YHDCPortOffsetToRegister(0x7), Y_HDC_REGISTER_DEVICE_CONTROL);

  // Swapping two address lines is its own inverse.
  for (uint8_t offset = 0; offset < Y_HDC_NUM_PORTS; ++offset) {
    EXPECT_EQ(
        YHDCPortOffsetToRegister(YHDCPortOffsetToRegister(offset)), offset);
  }
}

TEST_F(HDCTaskFileTest, AddressRegistersReadBackThroughSwappedPorts) {
  Write(Y_HDC_REGISTER_SECTOR_COUNT, 0x12);
  Write(Y_HDC_REGISTER_SECTOR_NUMBER, 0x34);
  Write(Y_HDC_REGISTER_CYLINDER_LOW, 0x56);
  Write(Y_HDC_REGISTER_CYLINDER_HIGH, 0x78);
  Write(Y_HDC_REGISTER_DRIVE_HEAD, 0x9A);

  EXPECT_EQ(Read(Y_HDC_REGISTER_SECTOR_COUNT), 0x12);
  EXPECT_EQ(Read(Y_HDC_REGISTER_SECTOR_NUMBER), 0x34);
  EXPECT_EQ(Read(Y_HDC_REGISTER_CYLINDER_LOW), 0x56);
  EXPECT_EQ(Read(Y_HDC_REGISTER_CYLINDER_HIGH), 0x78);
  EXPECT_EQ(Read(Y_HDC_REGISTER_DRIVE_HEAD), 0x9A);
}

TEST_F(HDCTaskFileTest, AttachedDriveReportsReady) {
  SelectDrive(0);
  EXPECT_TRUE(Read(Y_HDC_REGISTER_STATUS) & Y_HDC_STATUS_READY);
  EXPECT_FALSE(Read(Y_HDC_REGISTER_STATUS) & Y_HDC_STATUS_BUSY);
  EXPECT_FALSE(Read(Y_HDC_REGISTER_STATUS) & Y_HDC_STATUS_ERROR);
}

TEST_F(HDCTaskFileTest, EmptyDriveSlotNeverBecomesReady) {
  // This is how the option ROM concludes there is no slave: it selects the
  // drive, polls status, and gives up when the ready bit never appears.
  SelectDrive(1);
  EXPECT_EQ(Read(Y_HDC_REGISTER_STATUS), 0);

  RunCommand(Y_HDC_COMMAND_IDENTIFY_DEVICE);
  EXPECT_EQ(Read(Y_HDC_REGISTER_STATUS), 0);
}

TEST_F(HDCTaskFileTest, CommandsToAnEmptySlotLeaveTheOtherDriveAlone) {
  // The status register is shared by both drives, so a command aimed at the
  // absent slave must not disturb the master. A guest that polls for ready
  // before issuing a command would otherwise wait forever.
  SelectDrive(1);
  RunCommand(Y_HDC_COMMAND_IDENTIFY_DEVICE);

  SelectDrive(0);
  EXPECT_TRUE(Read(Y_HDC_REGISTER_STATUS) & Y_HDC_STATUS_READY);
  EXPECT_FALSE(Read(Y_HDC_REGISTER_STATUS) & Y_HDC_STATUS_ERROR);
}

TEST_F(HDCTaskFileTest, IdentifyDeviceReportsGeometry) {
  SelectDrive(0);
  const uint8_t status = RunCommand(Y_HDC_COMMAND_IDENTIFY_DEVICE);
  ASSERT_TRUE(status & Y_HDC_STATUS_DATA_REQUEST);
  ASSERT_FALSE(status & Y_HDC_STATUS_ERROR);

  const std::vector<uint8_t> block = DrainBuffer();

  // Fixed disk.
  EXPECT_EQ(WordAt(block, 0) & (1 << 6), 1 << 6);
  EXPECT_EQ(WordAt(block, 1), kTestGeometry.num_cylinders);
  EXPECT_EQ(WordAt(block, 3), kTestGeometry.num_heads);
  EXPECT_EQ(WordAt(block, 6), kTestGeometry.num_sectors_per_track);
  EXPECT_EQ(WordAt(block, 5), Y_HDC_SECTOR_SIZE);

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

TEST_F(HDCTaskFileTest, IdentifyDeviceReportsAGeometryThatIsFullyAddressable) {
  SelectDrive(0);
  const uint32_t num_sectors = (uint32_t)kTestGeometry.num_cylinders *
                               kTestGeometry.num_heads *
                               kTestGeometry.num_sectors_per_track;

  // Ask for a geometry with a different shape to the physical one. The
  // reported cylinder count has to follow it, or the drive would be
  // advertising sectors it does not have.
  Write(Y_HDC_REGISTER_SECTOR_COUNT, 5);
  Write(Y_HDC_REGISTER_DRIVE_HEAD, 3);  // 4 heads
  ASSERT_FALSE(
      RunCommand(Y_HDC_COMMAND_INITIALIZE_DEVICE_PARAMETERS) &
      Y_HDC_STATUS_ERROR);

  RunCommand(Y_HDC_COMMAND_IDENTIFY_DEVICE);
  const std::vector<uint8_t> block = DrainBuffer();

  const uint16_t cylinders = WordAt(block, 54);
  const uint16_t heads = WordAt(block, 55);
  const uint16_t sectors = WordAt(block, 56);
  EXPECT_EQ(heads, 4);
  EXPECT_EQ(sectors, 5);
  EXPECT_EQ(cylinders, num_sectors / (4 * 5));

  // ATA defines current capacity as the product of the three, and the drive
  // cannot claim more sectors than it has.
  const uint32_t current_capacity =
      (uint32_t)WordAt(block, 57) | ((uint32_t)WordAt(block, 58) << 16);
  EXPECT_EQ(current_capacity, (uint32_t)cylinders * heads * sectors);
  EXPECT_LE(current_capacity, num_sectors);

  // The last sector of the reported geometry really is readable, which is the
  // property all of the above exists to guarantee.
  Write(Y_HDC_REGISTER_SECTOR_COUNT, 1);
  Write(Y_HDC_REGISTER_CYLINDER_LOW, (uint8_t)((cylinders - 1) & 0xFF));
  Write(Y_HDC_REGISTER_CYLINDER_HIGH, (uint8_t)((cylinders - 1) >> 8));
  Write(Y_HDC_REGISTER_DRIVE_HEAD, (uint8_t)(heads - 1));
  Write(Y_HDC_REGISTER_SECTOR_NUMBER, (uint8_t)sectors);
  EXPECT_FALSE(
      RunCommand(Y_HDC_COMMAND_READ_VERIFY_SECTORS) & Y_HDC_STATUS_ERROR);

  // Total user addressable sectors stays the physical capacity, since that is
  // what LBA reaches regardless of the geometry the guest chose.
  EXPECT_EQ(
      (uint32_t)WordAt(block, 60) | ((uint32_t)WordAt(block, 61) << 16),
      num_sectors);
}

TEST_F(HDCTaskFileTest, IdentifyDeviceDoesNotAdvertiseMultipleMode) {
  SelectDrive(0);
  RunCommand(Y_HDC_COMMAND_IDENTIFY_DEVICE);
  const std::vector<uint8_t> block = DrainBuffer();

  // Zero is how ATA says Read Multiple and Write Multiple are unavailable.
  // Anything else would contradict Set Multiple Mode aborting.
  EXPECT_EQ(WordAt(block, 47) & 0xFF, 0);

  enum { kSetMultipleMode = 0xC6 };
  const uint8_t status = RunCommand(kSetMultipleMode);
  EXPECT_TRUE(status & Y_HDC_STATUS_ERROR);
  EXPECT_TRUE(Read(Y_HDC_REGISTER_ERROR) & Y_HDC_ERROR_ABORTED);
}

TEST_F(HDCTaskFileTest, IdentifyDeviceStringsAreByteSwapped) {
  SelectDrive(0);
  RunCommand(Y_HDC_COMMAND_IDENTIFY_DEVICE);
  const std::vector<uint8_t> block = DrainBuffer();

  // Space padded to the full field width, and readable rather than scrambled.
  EXPECT_EQ(
      StringAt(block, 27, 20), "YAX86 HARD DISK                         ");
  EXPECT_EQ(StringAt(block, 10, 10), "YAX86-0000001       ");
  EXPECT_EQ(StringAt(block, 23, 4), "1.0     ");
}

TEST_F(HDCTaskFileTest, DataRequestClearsWhenTheBlockIsDrained) {
  SelectDrive(0);
  RunCommand(Y_HDC_COMMAND_IDENTIFY_DEVICE);

  // A read of the low byte port takes a whole word off the card, so a block is
  // a sector's worth of words rather than of reads.
  const int num_words = Y_HDC_SECTOR_SIZE / 2;
  for (int i = 0; i < num_words - 1; ++i) {
    Read(Y_HDC_REGISTER_DATA);
    Read(Y_HDC_REGISTER_DATA_HIGH);
    ASSERT_TRUE(Read(Y_HDC_REGISTER_STATUS) & Y_HDC_STATUS_DATA_REQUEST)
        << "cleared early after " << (i + 1) << " words";
  }
  Read(Y_HDC_REGISTER_DATA);
  Read(Y_HDC_REGISTER_DATA_HIGH);
  EXPECT_FALSE(Read(Y_HDC_REGISTER_STATUS) & Y_HDC_STATUS_DATA_REQUEST);
}

TEST_F(HDCTaskFileTest, DataRegisterReadWithNoTransferReturnsZero) {
  SelectDrive(0);
  EXPECT_EQ(Read(Y_HDC_REGISTER_DATA), 0);
}

TEST_F(HDCTaskFileTest, InitializeDeviceParametersSetsTranslatedGeometry) {
  SelectDrive(0);
  // Address the drive as 1 head of 8 sectors rather than 2 heads of 4.
  Write(Y_HDC_REGISTER_SECTOR_COUNT, 8);
  Write(Y_HDC_REGISTER_DRIVE_HEAD, 0);  // heads - 1 == 0
  const uint8_t status = RunCommand(Y_HDC_COMMAND_INITIALIZE_DEVICE_PARAMETERS);
  EXPECT_FALSE(status & Y_HDC_STATUS_ERROR);

  RunCommand(Y_HDC_COMMAND_IDENTIFY_DEVICE);
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
        RunCommand((uint8_t)(Y_HDC_COMMAND_RECALIBRATE + step)),
        Y_HDC_STATUS_READY | Y_HDC_STATUS_SEEK_COMPLETE)
        << "recalibrate step rate " << (int)step;
    EXPECT_EQ(
        RunCommand((uint8_t)(Y_HDC_COMMAND_SEEK + step)),
        Y_HDC_STATUS_READY | Y_HDC_STATUS_SEEK_COMPLETE)
        << "seek step rate " << (int)step;
  }
}

TEST_F(HDCTaskFileTest, ReadVerifyAcceptsAnAddressableSector) {
  SelectDrive(0);
  Write(Y_HDC_REGISTER_SECTOR_COUNT, 1);
  Write(Y_HDC_REGISTER_CYLINDER_LOW, 9);
  Write(Y_HDC_REGISTER_CYLINDER_HIGH, 0);
  Write(Y_HDC_REGISTER_DRIVE_HEAD, 1);  // head 1
  Write(Y_HDC_REGISTER_SECTOR_NUMBER, 4);

  const uint8_t status = RunCommand(Y_HDC_COMMAND_READ_VERIFY_SECTORS);
  EXPECT_FALSE(status & Y_HDC_STATUS_ERROR);
  EXPECT_EQ(Read(Y_HDC_REGISTER_ERROR), 0);
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
    // A single sector, so only the address can be what fails.
    Write(Y_HDC_REGISTER_SECTOR_COUNT, 1);
    Write(Y_HDC_REGISTER_CYLINDER_LOW, address.cylinder_low);
    Write(Y_HDC_REGISTER_CYLINDER_HIGH, 0);
    Write(Y_HDC_REGISTER_DRIVE_HEAD, address.head);
    Write(Y_HDC_REGISTER_SECTOR_NUMBER, address.sector);

    const uint8_t status = RunCommand(Y_HDC_COMMAND_READ_VERIFY_SECTORS);
    EXPECT_TRUE(status & Y_HDC_STATUS_ERROR) << address.what;
    EXPECT_TRUE(Read(Y_HDC_REGISTER_ERROR) & Y_HDC_ERROR_ID_NOT_FOUND)
        << address.what;
  }
}

TEST_F(HDCTaskFileTest, ReadVerifyRejectsARunThatLeavesTheDrive) {
  SelectDrive(0);
  const uint32_t num_sectors = (uint32_t)kTestGeometry.num_cylinders *
                               kTestGeometry.num_heads *
                               kTestGeometry.num_sectors_per_track;

  // Start on the last sector of the drive and ask to verify two. The first
  // sector exists, so only a check that covers the whole run catches this.
  Write(Y_HDC_REGISTER_SECTOR_COUNT, 2);
  Write(Y_HDC_REGISTER_SECTOR_NUMBER, (uint8_t)((num_sectors - 1) & 0xFF));
  Write(
      Y_HDC_REGISTER_CYLINDER_LOW, (uint8_t)(((num_sectors - 1) >> 8) & 0xFF));
  Write(Y_HDC_REGISTER_CYLINDER_HIGH, 0);
  Write(Y_HDC_REGISTER_DRIVE_HEAD, Y_HDC_DRIVE_HEAD_LBA);

  EXPECT_TRUE(
      RunCommand(Y_HDC_COMMAND_READ_VERIFY_SECTORS) & Y_HDC_STATUS_ERROR);
  EXPECT_TRUE(Read(Y_HDC_REGISTER_ERROR) & Y_HDC_ERROR_ID_NOT_FOUND);

  // The same run one sector earlier ends exactly at the end of the drive.
  Write(Y_HDC_REGISTER_SECTOR_COUNT, 2);
  Write(Y_HDC_REGISTER_SECTOR_NUMBER, (uint8_t)((num_sectors - 2) & 0xFF));
  Write(
      Y_HDC_REGISTER_CYLINDER_LOW, (uint8_t)(((num_sectors - 2) >> 8) & 0xFF));
  EXPECT_FALSE(
      RunCommand(Y_HDC_COMMAND_READ_VERIFY_SECTORS) & Y_HDC_STATUS_ERROR);
}

TEST_F(HDCTaskFileTest, ReadingTheHighByteLatchDoesNotAdvanceTheTransfer) {
  SelectDrive(0);
  RunCommand(Y_HDC_COMMAND_IDENTIFY_DEVICE);

  // The low byte port runs the bus cycle and fetches a whole word; the high
  // byte port just hands back what the card latched. Reading the latch over
  // and over must therefore keep returning the same byte rather than eating
  // the transfer.
  const uint8_t low_byte = Read(Y_HDC_REGISTER_DATA);
  const uint8_t high_byte = Read(Y_HDC_REGISTER_DATA_HIGH);
  EXPECT_EQ(Read(Y_HDC_REGISTER_DATA_HIGH), high_byte);
  EXPECT_EQ(Read(Y_HDC_REGISTER_DATA_HIGH), high_byte);

  // Word 0 of the Identify block is the general configuration word, whose
  // bit 6 marks a fixed disk, so the two halves the guest just collected have
  // to be that word, in order.
  EXPECT_EQ((uint16_t)(low_byte | (high_byte << 8)), 1 << 6);

  // The next low byte read is still word 1, undisturbed by the extra latch
  // reads. Word 1 is the cylinder count.
  const uint8_t next_low = Read(Y_HDC_REGISTER_DATA);
  const uint8_t next_high = Read(Y_HDC_REGISTER_DATA_HIGH);
  EXPECT_EQ(
      (uint16_t)(next_low | (next_high << 8)), kTestGeometry.num_cylinders);
}

TEST_F(HDCTaskFileTest, ReadVerifyAcceptsLogicalBlockAddresses) {
  SelectDrive(0);
  Write(Y_HDC_REGISTER_SECTOR_COUNT, 1);
  const uint32_t last_sector = (uint32_t)kTestGeometry.num_cylinders *
                                   kTestGeometry.num_heads *
                                   kTestGeometry.num_sectors_per_track -
                               1;
  Write(Y_HDC_REGISTER_SECTOR_NUMBER, (uint8_t)(last_sector & 0xFF));
  Write(Y_HDC_REGISTER_CYLINDER_LOW, (uint8_t)((last_sector >> 8) & 0xFF));
  Write(Y_HDC_REGISTER_CYLINDER_HIGH, (uint8_t)((last_sector >> 16) & 0xFF));
  Write(
      Y_HDC_REGISTER_DRIVE_HEAD,
      (uint8_t)(Y_HDC_DRIVE_HEAD_LBA | ((last_sector >> 24) & 0x0F)));
  EXPECT_FALSE(
      RunCommand(Y_HDC_COMMAND_READ_VERIFY_SECTORS) & Y_HDC_STATUS_ERROR);

  // One past the last sector is off the drive.
  Write(Y_HDC_REGISTER_SECTOR_NUMBER, (uint8_t)((last_sector + 1) & 0xFF));
  Write(
      Y_HDC_REGISTER_CYLINDER_LOW, (uint8_t)(((last_sector + 1) >> 8) & 0xFF));
  EXPECT_TRUE(
      RunCommand(Y_HDC_COMMAND_READ_VERIFY_SECTORS) & Y_HDC_STATUS_ERROR);
  EXPECT_TRUE(Read(Y_HDC_REGISTER_ERROR) & Y_HDC_ERROR_ID_NOT_FOUND);
}

TEST_F(HDCTaskFileTest, UnsupportedCommandsAbort) {
  SelectDrive(0);
  // Read Multiple, which this controller does not implement, and 0xFF, which
  // is not a command at all.
  for (uint8_t opcode : {(uint8_t)0xC4, (uint8_t)0xFF}) {
    const uint8_t status = RunCommand(opcode);
    EXPECT_TRUE(status & Y_HDC_STATUS_ERROR) << "opcode " << (int)opcode;
    EXPECT_TRUE(Read(Y_HDC_REGISTER_ERROR) & Y_HDC_ERROR_ABORTED)
        << "opcode " << (int)opcode;
  }
}

TEST_F(HDCTaskFileTest, DetachedDriveStopsResponding) {
  SelectDrive(0);
  ASSERT_TRUE(Read(Y_HDC_REGISTER_STATUS) & Y_HDC_STATUS_READY);

  YHDCDetachDrive(&hdc_, 0);
  EXPECT_EQ(Read(Y_HDC_REGISTER_STATUS), 0);
}

TEST_F(HDCTaskFileTest, AlternateStatusReadsTheSameAsStatus) {
  SelectDrive(0);
  EXPECT_EQ(Read(Y_HDC_REGISTER_DEVICE_CONTROL), Read(Y_HDC_REGISTER_STATUS));

  // A failed command shows up in both, and reading either leaves the transfer
  // state alone - the point of the alternate register.
  Write(Y_HDC_REGISTER_SECTOR_COUNT, 1);
  Write(Y_HDC_REGISTER_SECTOR_NUMBER, 0);  // sector zero does not exist
  RunCommand(Y_HDC_COMMAND_READ_VERIFY_SECTORS);
  EXPECT_TRUE(Read(Y_HDC_REGISTER_DEVICE_CONTROL) & Y_HDC_STATUS_ERROR);
  EXPECT_EQ(Read(Y_HDC_REGISTER_DEVICE_CONTROL), Read(Y_HDC_REGISTER_STATUS));

  // An empty slot leaves it undriven, just as it does the status register.
  SelectDrive(1);
  EXPECT_EQ(Read(Y_HDC_REGISTER_DEVICE_CONTROL), 0);
}

TEST_F(HDCTaskFileTest, SoftwareResetAbandonsATransferInProgress) {
  SelectDrive(0);
  ASSERT_TRUE(
      RunCommand(Y_HDC_COMMAND_IDENTIFY_DEVICE) & Y_HDC_STATUS_DATA_REQUEST);
  Read(Y_HDC_REGISTER_DATA);

  Write(Y_HDC_REGISTER_DEVICE_CONTROL, Y_HDC_DEVICE_CONTROL_SOFTWARE_RESET);

  // The drive is back to idle with nothing left to hand over, rather than
  // still holding out half a block the guest has stopped collecting.
  const uint8_t status = Read(Y_HDC_REGISTER_STATUS);
  EXPECT_FALSE(status & Y_HDC_STATUS_DATA_REQUEST);
  EXPECT_TRUE(status & Y_HDC_STATUS_READY);
  EXPECT_FALSE(status & Y_HDC_STATUS_ERROR);
  EXPECT_EQ(Read(Y_HDC_REGISTER_ERROR), 0);

  // And it takes commands again, starting a fresh block from its first word.
  ASSERT_TRUE(
      RunCommand(Y_HDC_COMMAND_IDENTIFY_DEVICE) & Y_HDC_STATUS_DATA_REQUEST);
  const uint8_t low_byte = Read(Y_HDC_REGISTER_DATA);
  const uint8_t high_byte = Read(Y_HDC_REGISTER_DATA_HIGH);
  EXPECT_EQ((uint16_t)(low_byte | (high_byte << 8)), 1 << 6);
}

TEST_F(HDCTaskFileTest, DisablingInterruptsIsAcceptedAndIgnored) {
  SelectDrive(0);
  ASSERT_TRUE(
      RunCommand(Y_HDC_COMMAND_IDENTIFY_DEVICE) & Y_HDC_STATUS_DATA_REQUEST);

  // The card has no interrupt line, but a guest still writes this bit, and it
  // must not be mistaken for a reset.
  Write(Y_HDC_REGISTER_DEVICE_CONTROL, Y_HDC_DEVICE_CONTROL_NO_INTERRUPT);
  EXPECT_TRUE(Read(Y_HDC_REGISTER_STATUS) & Y_HDC_STATUS_DATA_REQUEST);
}

TEST_F(HDCTaskFileTest, UnmappedPortOffsetsReadAsOpenBus) {
  SelectDrive(0);
  // Offsets that no register decodes to. Offset 0x7 is not among them: it
  // reaches the device control register, which is ATA register 0xE.
  for (uint8_t offset : {0x3, 0x5, 0x9, 0xB, 0xD, 0xF}) {
    EXPECT_EQ(
        YHDCReadPort(&hdc_, Y_HDC_PORT_BASE + offset), Y_HDC_OPEN_BUS_VALUE)
        << "offset " << (int)offset;
  }
}

}  // namespace
