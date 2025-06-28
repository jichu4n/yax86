#include "./test_helpers.h"

#include <gtest/gtest.h>

#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace std;

// COM file load offset.
constexpr const char* kCOMFileLoadOffsetString = "100h";

// Overload the << operator for Instruction to print its contents.
ostream& operator<<(ostream& os, const Instruction& instruction) {
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

string GetFlagName(Flag flag) {
  switch (flag) {
    case kCF:
      return "CF";
    case kPF:
      return "PF";
    case kAF:
      return "AF";
    case kZF:
      return "ZF";
    case kSF:
      return "SF";
    case kTF:
      return "TF";
    case kIF:
      return "IF";
    case kDF:
      return "DF";
    case kOF:
      return "OF";
    default:
      return "Unknown flag";
  }
}

vector<uint8_t> Assemble(const string& name, const string& asm_code) {
  cout << ">> Assembling " << name << ":" << endl << asm_code << endl;

  // Create a temporary file for the assembly code
  string asm_file_name = name + ".asm";
  ofstream asm_file(asm_file_name);
  if (!asm_file) {
    throw runtime_error("Failed to create assembly file: " + asm_file_name);
  }
  asm_file << "org " << kCOMFileLoadOffsetString << endl
           << endl
           << asm_code << endl;
  asm_file.close();

  // Assemble the code using fasm to a COM file
  string com_file_name = name + ".com";
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

CPUTestHelper::CPUTestHelper(size_t memory_size)
    : memory_size_(memory_size),
      memory_(make_unique<uint8_t[]>(memory_size)),
      context_{this, memory_.get(), memory_size} {
  InitCPU(&cpu_);

  cpu_.config = &config_;
  config_.context = &context_;
  config_.read_memory_byte = [](CPUState* cpu, uint16_t address) {
    Context* context = reinterpret_cast<Context*>(cpu->config->context);
    if (address >= context->memory_size) {
      ostringstream oss;
      oss << "Memory read out of bounds: 0x" << hex << address
          << ", memory size: 0x" << hex << context->memory_size;
      throw runtime_error(oss.str());
    }
    if (context->self->enable_debug_memory_access_) {
      cout << "--- READ " << hex << setw(4) << setfill('0') << address << " => "
           << hex << setw(2) << setfill('0')
           << static_cast<uint32_t>(context->memory[address]) << endl;
    }
    return context->memory[address];
  };
  config_.write_memory_byte = [](CPUState* cpu, uint16_t address,
                                 uint8_t value) {
    Context* context = reinterpret_cast<Context*>(cpu->config->context);
    if (address >= context->memory_size) {
      ostringstream oss;
      oss << "Memory write out of bounds: 0x" << hex << address
          << ", memory size: 0x" << hex << context->memory_size;
      throw runtime_error(oss.str());
    }
    if (context->self->enable_debug_memory_access_) {
      cout << "--- WRITE " << hex << setw(4) << setfill('0') << address
           << " <= " << hex << setw(2) << setfill('0')
           << static_cast<uint32_t>(value) << endl;
    }
    context->memory[address] = value;
  };
  config_.handle_interrupt = [](CPUState* cpu,
                                uint8_t interrupt_number) -> ExecuteStatus {
    throw runtime_error(
        "Interrupt " + to_string(interrupt_number) + " not handled in test");
  };
}

CPUTestHelper::~CPUTestHelper() {}

void CPUTestHelper::Load(const vector<uint8_t>& data, uint16_t offset) {
  if (offset + data.size() > memory_size_) {
    throw runtime_error("Data size exceeds memory size");
  }
  memcpy(memory_.get() + offset, data.data(), data.size());
}

void CPUTestHelper::LoadCOM(const vector<uint8_t>& code) {
  Load(code, kCOMFileLoadOffset);
  cpu_.registers[kCS] = 0;
  cpu_.registers[kIP] = kCOMFileLoadOffset;
}

size_t CPUTestHelper::AssembleAndLoadProgram(
    const string& name, const string& asm_code) {
  auto machine_code = Assemble(name, asm_code);
  LoadCOM(machine_code);
  return machine_code.size();
}

unique_ptr<CPUTestHelper> CPUTestHelper::CreateWithProgram(
    const string& name, const string& asm_code, size_t memory_size) {
  auto cpu_test_helper = make_unique<CPUTestHelper>(memory_size);
  cpu_test_helper->AssembleAndLoadProgram(name, asm_code);
  return cpu_test_helper;
}

void CPUTestHelper::ExecuteInstructions(int num_instructions) {
  uint16_t* ip = &cpu_.registers[kIP];

  cout << ">> Executing encoded instructions:" << endl;
  for (int i = 0; i < num_instructions; ++i) {
    Instruction instruction;
    auto fetch_status = FetchNextInstruction(&cpu_, &instruction);
    if (fetch_status != kFetchSuccess) {
      throw runtime_error(
          "Failed to fetch instruction at IP: " + to_string(*ip) +
          ", status: " + to_string(fetch_status));
    }
    cout << "[" << hex << setw(4) << setfill('0') << (*ip) << "]\t"
         << instruction << endl;
    *ip += instruction.size;
    auto execute_status = ExecuteInstruction(&cpu_, &instruction);
    if (execute_status != kExecuteSuccess) {
      cout << "Warning: Instruction execution returned status " << dec
           << execute_status << endl;
    }
  }
}

void CPUTestHelper::CheckFlags(const vector<pair<Flag, bool> >& flags) {
  for (const auto& it : flags) {
    EXPECT_EQ(GetFlag(&cpu_, it.first), it.second)
        << "Flag " << GetFlagName(it.first) << " expected to be "
        << (it.second ? "set" : "not set") << ", but was "
        << (GetFlag(&cpu_, it.first) ? "set" : "not set");
  }
}
