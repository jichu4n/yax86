#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "../yax86.h"

using namespace std;

// COM file load offset.
const uint16_t kCOMFileLoadOffset = 0x100;
const char* kCOMFileLoadOffsetString = "100h";
// Test output directory for assembly files and machine code.
const char* kTestOutputDir = "./test_output/";

// Overload the << operator for EncodedInstruction to print its contents.
ostream& operator<<(ostream& os, const EncodedInstruction& instruction) {
  // Prefix
  if (instruction.prefix_size > 0) {
    os << "p[";
    for (size_t i = 0; i < instruction.prefix_size; ++i) {
      os << (i ? "," : "") << hex << setw(2) << setfill('0')
         << static_cast<int>(instruction.prefix[i]);
    }
    os << "] ";
  }
  // Opcode
  os << hex << setw(2) << setfill('0') << static_cast<int>(instruction.opcode);
  // ModRM
  if (instruction.has_mod_rm) {
    os << " m[" << static_cast<int>(instruction.mod_rm.mod) << ","
       << static_cast<int>(instruction.mod_rm.reg) << ","
       << static_cast<int>(instruction.mod_rm.rm) << "]";
  }
  // Displacement
  if (instruction.displacement_size > 0) {
    os << " d[";
    for (size_t i = 0; i < instruction.displacement_size; ++i) {
      os << (i ? "," : "") << hex << setw(2) << setfill('0')
         << static_cast<int>(instruction.displacement[i]);
    }
    os << "]";
  }
  // Immediate
  if (instruction.immediate_size > 0) {
    os << " i[";
    for (size_t i = 0; i < instruction.immediate_size; ++i) {
      os << (i ? "," : "") << hex << setw(2) << setfill('0')
         << static_cast<int>(instruction.immediate[i]);
    }
    os << "]";
  }

  return os;
}

// Test helper to assemble instructions using fasm and return the machine code.
vector<uint8_t> Assemble(const string& name, const string& asm_code) {
  cout << ">> Assembling " << name << ":" << endl << asm_code << endl;

  // Create a temporary file for the assembly code
  string asm_file_name = kTestOutputDir + name + ".asm";
  ofstream asm_file(asm_file_name);
  if (!asm_file) {
    throw runtime_error("Failed to create assembly file: " + asm_file_name);
  }
  asm_file << "org " << kCOMFileLoadOffsetString << endl
           << endl
           << asm_code << endl;
  asm_file.close();

  // Assemble the code using fasm to a COM file
  string com_file_name = kTestOutputDir + name + ".com";
  string command = "fasm " + asm_file_name + " " + com_file_name;
  if (system(command.c_str()) != 0) {
    throw runtime_error("Failed to run command: " + command);
  }

  // Read the COM file into memory
  ifstream com_file(com_file_name, ios::binary);
  if (!com_file) {
    throw runtime_error("Failed to read COM file: " + com_file_name);
  }
  vector<uint8_t> machine_code(
      (istreambuf_iterator<char>(com_file)), istreambuf_iterator<char>());
  com_file.close();

  // Use objdump to disassemble and print out the machine code
  string disasm_command =
      "objdump -D -b binary -m i8086 -M intel " + com_file_name;
  system(disasm_command.c_str());
  cout << endl;

  return machine_code;
}

// Helper function to test decoding a sequence of assembly instructions.
vector<EncodedInstruction> TestFetchInstructions(
    const string& name, const string& asm_code) {
  // Assemble the code and get the machine code
  vector<uint8_t> machine_code = Assemble(name, asm_code);

  // Set up memory.
  const size_t memory_size = kCOMFileLoadOffset + machine_code.size() + 0x100;
  unique_ptr<uint8_t[]> memory = make_unique<uint8_t[]>(memory_size);
  memcpy(
      memory.get() + kCOMFileLoadOffset, machine_code.data(),
      machine_code.size());

  // Set up memory and handlers.
  CPUConfig config;
  struct Context {
    uint8_t* memory;
    size_t memory_size;
  } context = {memory.get(), memory_size};
  config.context = &context;
  config.read_memory_byte = [](void* raw_context, uint16_t address) {
    Context* context = reinterpret_cast<Context*>(raw_context);
    if (address >= context->memory_size) {
      throw runtime_error("Memory read out of bounds");
    }
    return context->memory[address];
  };
  config.write_memory_byte = [](void* raw_context, uint16_t address,
                                uint8_t value) {
    Context* context = reinterpret_cast<Context*>(raw_context);
    if (address >= context->memory_size) {
      throw runtime_error("Memory write out of bounds");
    }
    context->memory[address] = value;
  };
  config.handle_interrupt = [](void* raw_context, uint8_t interrupt_number) {
    throw runtime_error(
        "Interrupt " + to_string(interrupt_number) + " not handled in test");
  };

  // Set up CPU state.
  CPUState cpu;
  InitCPU(&cpu);
  cpu.config = &config;
  cpu.registers[kCS] = 0;
  cpu.registers[kIP] = kCOMFileLoadOffset;

  // Fetch instructions until we reach the end of the machine code
  cout << ">> Reading encoded instructions:" << endl;
  vector<EncodedInstruction> instructions;
  while (cpu.registers[kIP] < kCOMFileLoadOffset + machine_code.size()) {
    EncodedInstruction instruction = FetchNextInstruction(&cpu);
    instructions.push_back(instruction);
    cout << "  " << instruction << endl;
    cpu.registers[kIP] += instruction.size;
  }

  return instructions;
}

// Test fixture for FetchNextInstruction tests
class FetchNextInstructionTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

// Test assembling a simple MOV instruction.
TEST_F(FetchNextInstructionTest, CanAssembleAndReadBackMachineCode) {
  auto machine_code = Assemble("assemble-test", "mov ax, 0x1234");
  EXPECT_EQ(machine_code, (vector<uint8_t>{0xb8, 0x34, 0x12}));
}

// Test fetching a single MOV instruction.
TEST_F(FetchNextInstructionTest, FetchSingleMOVInstruction) {
  auto instructions = TestFetchInstructions("fetch-test", "mov ax, 0x1234");
  ASSERT_EQ(instructions.size(), 1);
  EXPECT_EQ(instructions[0].opcode, 0xb8);  // MOV AX, imm16
  EXPECT_EQ(instructions[0].has_mod_rm, false);
  EXPECT_EQ(instructions[0].displacement_size, 0);
  EXPECT_EQ(instructions[0].immediate_size, 2);
  EXPECT_EQ(instructions[0].immediate[0], 0x34);
  EXPECT_EQ(instructions[0].immediate[1], 0x12);
}

// Test fetching a sequence of simple MOV instructions.
TEST_F(FetchNextInstructionTest, FetchMultipleMOVInstructions) {
  auto instructions = TestFetchInstructions(
      "fetch-multiple-test", "mov ax, 0x1234\nmov bx, 0x5678");
  ASSERT_EQ(instructions.size(), 2);

  EXPECT_EQ(instructions[0].opcode, 0xb8);  // MOV AX, imm16
  EXPECT_EQ(instructions[0].has_mod_rm, false);
  EXPECT_EQ(instructions[0].displacement_size, 0);
  EXPECT_EQ(instructions[0].immediate_size, 2);
  EXPECT_EQ(instructions[0].immediate[0], 0x34);
  EXPECT_EQ(instructions[0].immediate[1], 0x12);

  EXPECT_EQ(instructions[1].opcode, 0xbb);  // MOV BX, imm16
  EXPECT_EQ(instructions[1].has_mod_rm, false);
  EXPECT_EQ(instructions[1].displacement_size, 0);
  EXPECT_EQ(instructions[1].immediate_size, 2);
  EXPECT_EQ(instructions[1].immediate[0], 0x78);
  EXPECT_EQ(instructions[1].immediate[1], 0x56);
}

// Test fetching a variety of MOV instructions with different source /
// destinations and immediate sizes.
TEST_F(FetchNextInstructionTest, FetchMOVInstructions) {
  auto instructions = TestFetchInstructions(
      "fetch-mov-test",
      // MOV r16, imm16
      "mov ax, 0x1234\n"
      // MOV r8, imm8
      "mov bl, 0x56\n"
      // MOV r16, r16
      "mov cx, dx\n"
      // MOV r8, r8
      "mov dh, al\n"
      // MOV [r16], r16
      "mov [bx], ax\n"
      // MOV [r16+disp8], r8
      "mov [si+2], cl\n"
      // MOV r16, [r16+disp16]
      "mov bp, [di+0x1234]\n"
      // MOV [disp16], r16
      "mov [0x5678], dx\n"
      // MOV byte [r16], imm8
      "mov byte [bp], 0x12\n"
      // MOV word [r16+r16], imm16
      "mov word [bx+si], 0x3456\n"
      // MOV sreg, r16
      "mov es, ax\n"
      // MOV r16, sreg
      "mov bx, ds");

  ASSERT_EQ(instructions.size(), 12);
}

// Test fetching a sequence of instructions with prefixes.
TEST_F(FetchNextInstructionTest, FetchInstructionsWithPrefixes) {
  auto instructions = TestFetchInstructions(
      "fetch-prefixes-test",
      // REP prefix
      "rep movsb\n"
      // REPNE prefix
      "repne movsb\n"
      // LOCK prefix
      "lock add [bx], ax\n"
      // Multiple prefixes
      "rep lock mov ds, [bx]\n"
      // CS segment override prefix
      "cs mov ax, [bx]\n"
      // ES segment override prefix with REP
      "rep es mov ax, [bx]\n"
      // SS segment override prefix with REPNE
      "repne ss mov ax, [bx]\n"
      // DS segment override prefix with LOCK
      "lock ds mov ax, [bx]\n");

  ASSERT_EQ(instructions.size(), 8);
  // REP prefix
  EXPECT_EQ(instructions[0].prefix_size, 1);
  EXPECT_EQ(instructions[0].prefix[0], 0xf3);
  // REPNE prefix
  EXPECT_EQ(instructions[1].prefix_size, 1);
  EXPECT_EQ(instructions[1].prefix[0], 0xf2);
  // LOCK prefix
  EXPECT_EQ(instructions[2].prefix_size, 1);
  EXPECT_EQ(instructions[2].prefix[0], 0xf0);
  // Multiple prefixes
  EXPECT_EQ(instructions[3].prefix_size, 2);
  EXPECT_EQ(instructions[3].prefix[0], 0xf3);
  EXPECT_EQ(instructions[3].prefix[1], 0xf0);
  // CS segment override prefix
  EXPECT_EQ(instructions[4].prefix_size, 1);
  EXPECT_EQ(instructions[4].prefix[0], 0x2e);
  // CS segment override prefix with REP
  EXPECT_EQ(instructions[5].prefix_size, 2);
  EXPECT_EQ(instructions[5].prefix[0], 0xf3);
  EXPECT_EQ(instructions[5].prefix[1], 0x26);
  // SS segment override prefix with REPNE
  EXPECT_EQ(instructions[6].prefix_size, 2);
  EXPECT_EQ(instructions[6].prefix[0], 0xf2);
  EXPECT_EQ(instructions[6].prefix[1], 0x36);
  // DS segment override prefix with LOCK
  EXPECT_EQ(instructions[7].prefix_size, 2);
  EXPECT_EQ(instructions[7].prefix[0], 0xf0);
  EXPECT_EQ(instructions[7].prefix[1], 0x3e);
}

// Test fetching a sequence of instructions with 0, 1, and 2 displacement bytes.
TEST_F(FetchNextInstructionTest, FetchInstructionsWithDisplacement) {
  auto instructions = TestFetchInstructions(
      "fetch-displacement-test",
      // MOV r16, [r16+disp8]
      "mov ax, [bx+2]\n"
      // MOV r16, [r16+disp16]
      "mov bx, [si+0x1234]\n"
      // MOV [r16+disp8], r8
      "mov [di+3], cl\n"
      // MOV [r16+disp16], r16
      "mov [bp+0x5678], dx\n"
      // MOV [r16], r16
      "mov ax, [bx]\n");

  ASSERT_EQ(instructions.size(), 5);
  // MOV r16, [r16+disp8]
  EXPECT_EQ(instructions[0].displacement_size, 1);
  EXPECT_EQ(instructions[0].displacement[0], 2);
  // MOV r16, [r16+disp16]
  EXPECT_EQ(instructions[1].displacement_size, 2);
  EXPECT_EQ(instructions[1].displacement[0], 0x34);
  EXPECT_EQ(instructions[1].displacement[1], 0x12);
  // MOV [r16+disp8], r8
  EXPECT_EQ(instructions[2].displacement_size, 1);
  EXPECT_EQ(instructions[2].displacement[0], 3);
  // MOV [r16+disp16], r16
  EXPECT_EQ(instructions[3].displacement_size, 2);
  EXPECT_EQ(instructions[3].displacement[0], 0x78);
  EXPECT_EQ(instructions[3].displacement[1], 0x56);
  // MOV [r16], r16
  EXPECT_EQ(instructions[4].displacement_size, 0);
}

// Test 0xF6 and 0xF7 instructions with immediate data.
TEST_F(FetchNextInstructionTest, FetchF6F7Instructions) {
  auto instructions = TestFetchInstructions(
      "fetch-f6f7-test",
      // NOT r/m8
      "not bl\n"
      // MUL r/m16
      "mul cx\n"
      // TEST r/m8, imm8
      "test byte [bx], 0x01\n"
      // TEST r/m16, imm16
      "test word [si+0x1234], 0x0002\n");

  ASSERT_EQ(instructions.size(), 4);

  // NOT r/m8
  EXPECT_EQ(instructions[0].opcode, 0xf6);
  EXPECT_EQ(instructions[0].has_mod_rm, true);
  EXPECT_EQ(instructions[0].immediate_size, 0);
  // MUL r/m16
  EXPECT_EQ(instructions[1].opcode, 0xf7);
  EXPECT_EQ(instructions[1].has_mod_rm, true);
  EXPECT_EQ(instructions[1].immediate_size, 0);
  // TEST r/m8, imm8
  EXPECT_EQ(instructions[2].opcode, 0xf6);
  EXPECT_EQ(instructions[2].has_mod_rm, true);
  EXPECT_EQ(instructions[2].immediate_size, 1);
  EXPECT_EQ(instructions[2].immediate[0], 0x01);
  // TEST r/m16, imm16
  EXPECT_EQ(instructions[3].opcode, 0xf7);
  EXPECT_EQ(instructions[3].has_mod_rm, true);
  EXPECT_EQ(instructions[3].immediate_size, 2);
  EXPECT_EQ(instructions[3].immediate[0], 0x02);
  EXPECT_EQ(instructions[3].immediate[1], 0x00);
}

// Test fetching JMP and CALL instructions with different immediate sizes.
TEST_F(FetchNextInstructionTest, FetchJmpCallInstructions) {
  auto instructions = TestFetchInstructions(
      "fetch-jmp-call-test",
      // JMP rel16
      "jmp 0x1234\n"
      // CALL rel16
      "call 0x5678\n"
      // JMP ptr16:16
      "jmp 0x9abc:0xdef0\n"
      // CALL ptr16:16
      "call 0x1357:0x2468\n");

  ASSERT_EQ(instructions.size(), 4);

  // JMP rel16
  EXPECT_EQ(instructions[0].opcode, 0xe9);
  EXPECT_EQ(instructions[0].immediate_size, 2);
  EXPECT_EQ(instructions[0].immediate[0], 0x31);
  EXPECT_EQ(instructions[0].immediate[1], 0x11);

  // CALL rel16
  EXPECT_EQ(instructions[1].opcode, 0xe8);
  EXPECT_EQ(instructions[1].immediate_size, 2);
  EXPECT_EQ(instructions[1].immediate[0], 0x72);
  EXPECT_EQ(instructions[1].immediate[1], 0x55);

  // JMP ptr16:16
  EXPECT_EQ(instructions[2].opcode, 0xea);
  EXPECT_EQ(instructions[2].immediate_size, 4);
  EXPECT_EQ(instructions[2].immediate[0], 0xf0);
  EXPECT_EQ(instructions[2].immediate[1], 0xde);
  EXPECT_EQ(instructions[2].immediate[2], 0xbc);
  EXPECT_EQ(instructions[2].immediate[3], 0x9a);
  // CALL ptr16:16
  EXPECT_EQ(instructions[3].opcode, 0x9a);
  EXPECT_EQ(instructions[3].immediate_size, 4);
  EXPECT_EQ(instructions[3].immediate[0], 0x68);
  EXPECT_EQ(instructions[3].immediate[1], 0x24);
  EXPECT_EQ(instructions[3].immediate[2], 0x57);
  EXPECT_EQ(instructions[3].immediate[3], 0x13);
}