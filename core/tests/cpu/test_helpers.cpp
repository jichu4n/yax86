#include "./test_helpers.h"

#include <gtest/gtest.h>

#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace std;

// COM file load offset.
constexpr const char* kCOMFileLoadOffsetString = "100h";

// Overload the << operator for YCPUInstruction to print its contents.
ostream& operator<<(ostream& os, const YCPUInstruction& instruction) {
  // Prefixes, as what they selected rather than as the bytes that encoded
  // them. A LOCK prefix selects nothing and is not recorded, so it does not
  // appear here - only the instruction's size shows it was there.
  if (instruction.segment_override != Y_CPU_NO_SEGMENT_OVERRIDE ||
      instruction.repetition_prefix) {
    os << "p[";
    const char* separator = "";
    if (instruction.segment_override != Y_CPU_NO_SEGMENT_OVERRIDE) {
      static const char* const kSegmentNames[] = {"es", "cs", "ss", "ds"};
      os << kSegmentNames[instruction.segment_override - Y_CPU_ES];
      separator = ",";
    }
    if (instruction.repetition_prefix) {
      os << separator
         << (instruction.repetition_prefix == Y_CPU_PREFIX_REP ? "rep"
                                                               : "repnz");
    }
    os << "] ";
  }
  // Opcode
  os << hex << setw(2) << setfill('0') << static_cast<int>(instruction.opcode);
  // YCPUModRM
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

string GetFlagName(YCPUFlag flag) {
  switch (flag) {
    case Y_CPU_CF:
      return "CF";
    case Y_CPU_PF:
      return "PF";
    case Y_CPU_AF:
      return "AF";
    case Y_CPU_ZF:
      return "ZF";
    case Y_CPU_SF:
      return "SF";
    case Y_CPU_TF:
      return "TF";
    case Y_CPU_IF:
      return "IF";
    case Y_CPU_DF:
      return "DF";
    case Y_CPU_OF:
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

  // Assemble the code to a COM file
  string com_file_name = name + ".com";
  string command = "nasm -f bin " + asm_file_name + " -o " + com_file_name;
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

  // Disassemble and print out the machine code
  string disasm_command = "ndisasm -o 100h -b 16 " + com_file_name;
  system(disasm_command.c_str());
  cout << endl;

  return machine_code;
}

CPUTestHelper::CPUTestHelper(size_t memory_size)
    : memory_size_(memory_size),
      memory_(make_unique<uint8_t[]>(memory_size)),
      context_{this, memory_.get(), memory_size},
      cpu_{} {
  cpu_.config.context = &context_;
  cpu_.config.read_memory_byte = [](YCPUState* cpu, uint32_t address) {
    Context* context = reinterpret_cast<Context*>(cpu->config.context);
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
  cpu_.config.write_memory_byte = [](YCPUState* cpu, uint32_t address,
                                     uint8_t value) {
    Context* context = reinterpret_cast<Context*>(cpu->config.context);
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
  cpu_.config.handle_interrupt =
      [](YCPUState* cpu,
         uint8_t interrupt_number) -> YCPUInterruptHandlerResult {
    throw runtime_error(
        "Interrupt " + to_string(interrupt_number) + " not handled in test");
  };

  YCPUInit(&cpu_);
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
  cpu_.registers[Y_CPU_CS] = 0;
  cpu_.registers[Y_CPU_IP] = kCOMFileLoadOffset;
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
  uint16_t* ip = &cpu_.registers[Y_CPU_IP];

  cout << ">> Executing encoded instructions:" << endl;
  for (int i = 0; i < num_instructions; ++i) {
    YCPUInstruction instruction;
    auto fetch_status = YCPUFetchNextInstruction(&cpu_, &instruction);
    if (fetch_status != Y_CPU_FETCH_SUCCESS) {
      throw runtime_error(
          "Failed to fetch instruction at IP: " + to_string(*ip) +
          ", status: " + to_string(fetch_status));
    }
    cout << "[" << hex << setw(4) << setfill('0') << (*ip) << "]\t"
         << instruction << endl;
    *ip += instruction.size;
    auto execute_result = YCPUExecuteInstruction(&cpu_, &instruction);
    if (execute_result != Y_CPU_INSTRUCTION_EXECUTED) {
      cout << "Warning: Instruction execution returned result " << dec
           << execute_result << endl;
    }
  }
}

void CPUTestHelper::CheckFlags(const vector<pair<YCPUFlag, bool> >& flags) {
  for (const auto& it : flags) {
    EXPECT_EQ(YCPUGetFlag(&cpu_, it.first), it.second)
        << "Flag " << GetFlagName(it.first) << " expected to be "
        << (it.second ? "set" : "not set") << ", but was "
        << (YCPUGetFlag(&cpu_, it.first) ? "set" : "not set");
  }
}
