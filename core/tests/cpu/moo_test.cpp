// Runs the 8088 hardware-generated CPU test suite against the emulator.
//
//   https://github.com/SingleStepTests/8088
//
// Each test gives a starting register, flag and memory state plus the bytes of
// one instruction, and the state a real 8088 was left in afterwards. Only the
// architectural result is checked here - the per-cycle bus and prefetch queue
// records in the suite are for cycle-accurate emulators and are skipped.
//
// Test data is not in the repository. Fetch an opcode with, for example:
//
//   ./tools/download-cpu-tests.sh 8D
//
// Tests are skipped when no data has been downloaded.

#include <gtest/gtest.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "cpu.h"

namespace {

// Physical address space the tests address.
constexpr size_t kMemorySize = 1024 * 1024;

// Registers in the order the suite's register bitmask uses them, mapped onto
// this emulator's own ordering. The flags register is last and is not in
// RegisterIndex, so it gets a sentinel.
constexpr int kFlagsPseudoRegister = -1;
constexpr int kMooRegisterOrder[] = {
    kAX, kBX, kCX, kDX, kCS, kSS, kDS,
    kES, kSP, kBP, kSI, kDI, kIP, kFlagsPseudoRegister,
};
constexpr int kNumMooRegisters =
    sizeof(kMooRegisterOrder) / sizeof(kMooRegisterOrder[0]);

// A register or flags value from a test's initial or final state.
struct RegisterValues {
  // Indexed by position in kMooRegisterOrder.
  bool present[kNumMooRegisters] = {false};
  uint16_t value[kNumMooRegisters] = {0};
};

struct CPUStateSnapshot {
  RegisterValues registers;
  // Physical address to byte value.
  std::map<uint32_t, uint8_t> ram;
};

struct MooTest {
  // Whether this test was parsed into something checkable. A parse that
  // silently yielded nothing would make every test pass vacuously.
  //
  // Only the initial state is required. A final state records just the values
  // that changed, and legitimately holds none at all - a taken "jz -2" jumps
  // to itself and leaves every register as it was.
  bool is_parsed() const {
    if (bytes.empty()) {
      return false;
    }
    for (int i = 0; i < kNumMooRegisters; ++i) {
      if (initial.registers.present[i]) {
        return true;
      }
    }
    return false;
  }

  std::string name;
  std::vector<uint8_t> bytes;
  CPUStateSnapshot initial;
  // Only the values that changed are recorded.
  CPUStateSnapshot final_state;
};

// ============================================================================
// MOO binary format
// ============================================================================
//
// A file is a sequence of chunks, each a four character type, a 32-bit
// little-endian length, and that many bytes of payload. A "MOO " header chunk
// is followed by one "TEST" chunk per test, whose payload is itself a sequence
// of chunks.

uint32_t ReadU32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

uint16_t ReadU16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

// Walks the chunks in a buffer, calling visit(type, data, length) for each.
template <typename Visitor>
bool ForEachChunk(const uint8_t* data, size_t size, Visitor visit) {
  size_t pos = 0;
  while (pos + 8 <= size) {
    char type[5] = {0};
    memcpy(type, data + pos, 4);
    const uint32_t length = ReadU32(data + pos + 4);
    pos += 8;
    if (pos + length > size) {
      return false;
    }
    visit(std::string(type), data + pos, length);
    pos += length;
  }
  return true;
}

RegisterValues ParseRegisters(const uint8_t* data, uint32_t length) {
  RegisterValues registers;
  if (length < 2) {
    return registers;
  }
  const uint16_t bitmask = ReadU16(data);
  const uint8_t* p = data + 2;
  uint32_t remaining = length - 2;
  for (int i = 0; i < kNumMooRegisters; ++i) {
    if (!((bitmask >> i) & 1)) {
      continue;
    }
    if (remaining < 2) {
      break;
    }
    registers.present[i] = true;
    registers.value[i] = ReadU16(p);
    p += 2;
    remaining -= 2;
  }
  return registers;
}

std::map<uint32_t, uint8_t> ParseRam(const uint8_t* data, uint32_t length) {
  std::map<uint32_t, uint8_t> ram;
  if (length < 4) {
    return ram;
  }
  const uint32_t count = ReadU32(data);
  if (length < 4 + count * 5) {
    return ram;
  }
  const uint8_t* p = data + 4;
  for (uint32_t i = 0; i < count; ++i) {
    ram[ReadU32(p)] = p[4];
    p += 5;
  }
  return ram;
}

CPUStateSnapshot ParseCPUState(const uint8_t* data, uint32_t length) {
  CPUStateSnapshot state;
  ForEachChunk(
      data, length,
      [&state](const std::string& type, const uint8_t* p, uint32_t len) {
        if (type == "REGS") {
          state.registers = ParseRegisters(p, len);
        } else if (type == "RAM ") {
          state.ram = ParseRam(p, len);
        }
        // QUEU is the prefetch queue, which this emulator does not model.
      });
  return state;
}

// Reads every test out of a .MOO file. Returns false if the file is unusable.
bool LoadMooFile(const std::string& path, std::vector<MooTest>* tests) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  const std::vector<uint8_t> data(
      (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  return ForEachChunk(
      data.data(), data.size(),
      [tests](const std::string& type, const uint8_t* p, uint32_t len) {
        if (type != "TEST") {
          return;
        }
        // A TEST chunk's payload starts with a 32-bit index, then its
        // subchunks.
        if (len < 4) {
          return;
        }
        MooTest test;
        ForEachChunk(
            p + 4, len - 4,
            [&test](const std::string& sub, const uint8_t* q, uint32_t sublen) {
              if (sub == "NAME" && sublen >= 4) {
                const uint32_t n = ReadU32(q);
                if (sublen >= 4 + n) {
                  test.name.assign((const char*)q + 4, n);
                }
              } else if (sub == "BYTS" && sublen >= 4) {
                const uint32_t n = ReadU32(q);
                if (sublen >= 4 + n) {
                  test.bytes.assign(q + 4, q + 4 + n);
                }
              } else if (sub == "INIT") {
                test.initial = ParseCPUState(q, sublen);
              } else if (sub == "FINA") {
                test.final_state = ParseCPUState(q, sublen);
              }
              // CYCL and HASH are not used.
            });
        tests->push_back(test);
      });
}

// ============================================================================
// Running a test
// ============================================================================

uint8_t g_memory[kMemorySize];

uint8_t ReadMemoryByte(YAX86_UNUSED CPUState* cpu, uint32_t address) {
  return address < kMemorySize ? g_memory[address] : 0xFF;
}

void WriteMemoryByte(
    YAX86_UNUSED CPUState* cpu, uint32_t address, uint8_t value) {
  if (address < kMemorySize) {
    g_memory[address] = value;
  }
}

// Reads the flag masks emitted by tools/download-cpu-tests.sh. Bits clear in a
// mask are left undefined by the 8088 for that opcode, and are not compared.
std::map<std::string, uint16_t> LoadFlagMasks(const std::string& dir) {
  std::map<std::string, uint16_t> masks;
  std::ifstream file(dir + "/flags_masks.txt");
  std::string opcode;
  std::string mask;
  while (file >> opcode >> mask) {
    masks[opcode] = (uint16_t)strtoul(mask.c_str(), nullptr, 16);
  }
  return masks;
}

// Formats a register mismatch for the failure message.
const char* MooRegisterName(int index) {
  static const char* kNames[] = {"AX", "BX", "CX", "DX", "CS", "SS", "DS",
                                 "ES", "SP", "BP", "SI", "DI", "IP", "FLAGS"};
  return kNames[index];
}

std::string DescribeBytes(const std::vector<uint8_t>& bytes) {
  std::string out;
  char buffer[8];
  for (uint8_t byte : bytes) {
    snprintf(buffer, sizeof(buffer), "%02X ", byte);
    out += buffer;
  }
  return out;
}

// Runs one test, returning an empty string on success or a description of the
// first mismatch.
std::string RunMooTest(const MooTest& test, uint16_t flags_mask) {
  memset(g_memory, 0, sizeof(g_memory));

  CPUConfig config = {0};
  config.read_memory_byte = ReadMemoryByte;
  config.write_memory_byte = WriteMemoryByte;
  CPUState cpu;
  CPUInit(&cpu, &config);

  // The expected end state is the starting state with the recorded changes
  // applied, so that a register or byte the instruction should not have
  // touched is checked as well.
  RegisterValues expected = test.initial.registers;
  for (int i = 0; i < kNumMooRegisters; ++i) {
    if (test.final_state.registers.present[i]) {
      expected.present[i] = true;
      expected.value[i] = test.final_state.registers.value[i];
    }
  }
  std::map<uint32_t, uint8_t> expected_ram = test.initial.ram;
  for (const auto& entry : test.final_state.ram) {
    expected_ram[entry.first] = entry.second;
  }

  for (int i = 0; i < kNumMooRegisters; ++i) {
    if (!test.initial.registers.present[i]) {
      continue;
    }
    const int reg = kMooRegisterOrder[i];
    if (reg == kFlagsPseudoRegister) {
      cpu.flags = test.initial.registers.value[i];
    } else {
      cpu.registers[reg] = test.initial.registers.value[i];
    }
  }
  for (const auto& entry : test.initial.ram) {
    if (entry.first < kMemorySize) {
      g_memory[entry.first] = entry.second;
    }
  }

  CPUTick(&cpu);

  char message[512];
  for (int i = 0; i < kNumMooRegisters; ++i) {
    if (!expected.present[i]) {
      continue;
    }
    const int reg = kMooRegisterOrder[i];
    uint16_t actual_value;
    uint16_t expected_value = expected.value[i];
    if (reg == kFlagsPseudoRegister) {
      actual_value = cpu.flags & flags_mask;
      expected_value &= flags_mask;
    } else {
      actual_value = cpu.registers[reg];
    }
    if (actual_value != expected_value) {
      snprintf(
          message, sizeof(message),
          "%s: %s expected %04X, got %04X  [%s] (initial flags %04X, IP %04X "
          "-> %04X, CS %04X)",
          test.name.c_str(), MooRegisterName(i), expected_value, actual_value,
          DescribeBytes(test.bytes).c_str(), test.initial.registers.value[13],
          test.initial.registers.value[12], cpu.registers[kIP],
          cpu.registers[kCS]);
      return message;
    }
  }
  for (const auto& entry : expected_ram) {
    if (entry.first >= kMemorySize) {
      continue;
    }
    if (g_memory[entry.first] != entry.second) {
      snprintf(
          message, sizeof(message),
          "%s: memory %05X expected %02X, got %02X  [%s]", test.name.c_str(),
          entry.first, entry.second, g_memory[entry.first],
          DescribeBytes(test.bytes).c_str());
      return message;
    }
  }
  return "";
}

// Opcode name from a test data file path, e.g. ".../8D.MOO" -> "8D".
std::string OpcodeFromPath(const std::string& path) {
  const size_t slash = path.find_last_of('/');
  const std::string base =
      slash == std::string::npos ? path : path.substr(slash + 1);
  const size_t dot = base.rfind(".MOO");
  return dot == std::string::npos ? base : base.substr(0, dot);
}

class MooTestSuite : public ::testing::TestWithParam<std::string> {};

// There are no instances unless opcode data has been downloaded, which is the
// normal state of a fresh checkout and of CI.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(MooTestSuite);

TEST_P(MooTestSuite, MatchesHardware) {
  const std::string path = GetParam();
  const std::string opcode = OpcodeFromPath(path);

  std::vector<MooTest> tests;
  ASSERT_TRUE(LoadMooFile(path, &tests)) << "could not read " << path;
  ASSERT_FALSE(tests.empty()) << "no tests in " << path;
  // Guard against a parse that finds tests but no state to compare, which
  // would make every one of them pass without checking anything.
  for (const MooTest& test : tests) {
    ASSERT_TRUE(test.is_parsed())
        << "test '" << test.name << "' in " << path
        << " has no instruction bytes or initial register state - the file "
           "format has probably changed";
  }

  const auto masks = LoadFlagMasks(YAX86_CPU_TEST_DATA_DIR);
  const auto mask_entry = masks.find(opcode);
  const uint16_t flags_mask =
      mask_entry == masks.end() ? 0xFFFF : mask_entry->second;

  int failures = 0;
  std::string first_failure;
  for (const MooTest& test : tests) {
    const std::string failure = RunMooTest(test, flags_mask);
    if (!failure.empty()) {
      if (failures == 0) {
        first_failure = failure;
      }
      ++failures;
    }
  }

  EXPECT_EQ(failures, 0) << opcode << ": " << failures << " of " << tests.size()
                         << " tests failed. First: " << first_failure;
  if (failures == 0) {
    std::cout << "  opcode " << opcode << ": " << tests.size()
              << " tests passed" << std::endl;
  }
}

// Enumerates the opcode files that have been downloaded, if any.
std::vector<std::string> DownloadedTestFiles() {
  std::vector<std::string> paths;
  // Not using a directory scan, to keep this portable: the download script
  // records what it fetched.
  std::ifstream index(std::string(YAX86_CPU_TEST_DATA_DIR) + "/downloaded.txt");
  std::string line;
  while (std::getline(index, line)) {
    if (!line.empty()) {
      paths.push_back(std::string(YAX86_CPU_TEST_DATA_DIR) + "/" + line);
    }
  }
  return paths;
}

INSTANTIATE_TEST_SUITE_P(
    Opcodes, MooTestSuite, ::testing::ValuesIn(DownloadedTestFiles()),
    [](const ::testing::TestParamInfo<std::string>& info) {
      // Group opcodes are named like "D0.4", and a test name may only
      // contain alphanumerics and underscores.
      std::string name = "Opcode" + OpcodeFromPath(info.param);
      for (char& c : name) {
        if (!isalnum(static_cast<unsigned char>(c))) {
          c = '_';
        }
      }
      return name;
    });

}  // namespace
