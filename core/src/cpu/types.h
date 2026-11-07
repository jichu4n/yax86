#ifndef Y_CPU_TYPES_H
#define Y_CPU_TYPES_H

#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Data width helpers.

// Data widths supported by the 8086 CPU.
typedef enum YCPUWidth {
  Y_CPU_BYTE = 0,
  Y_CPU_WORD,
} YCPUWidth;

enum {
  // Number of data width types.
  Y_CPU_NUM_WIDTHS = Y_CPU_WORD + 1,
};

enum {
  // How many bytes a segment addresses. An offset is 16 bits and wraps within
  // its segment where a linear address does not, so this is where anything
  // walking memory through an offset has to stop.
  Y_CPU_SEGMENT_SIZE = 0x10000,
};

// Bitmask to extract the sign bit of a value.
Y_MODULE_PRIVATE_HEADER const uint32_t YCPUSignBit[Y_CPU_NUM_WIDTHS] = {
    1 << 7,   // Y_CPU_BYTE
    1 << 15,  // Y_CPU_WORD
};

// Maximum unsigned value for each data width.
Y_MODULE_PRIVATE_HEADER const uint32_t YCPUMaxValue[Y_CPU_NUM_WIDTHS] = {
    0xFF,   // Y_CPU_BYTE
    0xFFFF  // Y_CPU_WORD
};

// Maximum signed value for each data width.
Y_MODULE_PRIVATE_HEADER const int32_t YCPUMaxSignedValue[Y_CPU_NUM_WIDTHS] = {
    0x7F,   // Y_CPU_BYTE
    0x7FFF  // Y_CPU_WORD
};

// Minimum signed value for each data width.
Y_MODULE_PRIVATE_HEADER const int32_t YCPUMinSignedValue[Y_CPU_NUM_WIDTHS] = {
    -0x80,   // Y_CPU_BYTE
    -0x8000  // Y_CPU_WORD
};

// Number of bytes in each data width.
Y_MODULE_PRIVATE_HEADER const uint8_t YCPUNumBytes[Y_CPU_NUM_WIDTHS] = {
    1,  // Y_CPU_BYTE
    2,  // Y_CPU_WORD
};

// Number of bits in each data width.
Y_MODULE_PRIVATE_HEADER const uint8_t YCPUNumBits[Y_CPU_NUM_WIDTHS] = {
    8,   // Y_CPU_BYTE
    16,  // Y_CPU_WORD
};

// Operand types.

// The address of a register operand.
typedef struct YCPURegisterAddress {
  // Register index. A uint8_t so the layout does not depend on
  // -fshort-enums.
  uint8_t register_index;
  // Byte offset within the register; only relevant for byte-sized operands.
  // 0 for low byte (AL, CL, DL, BL), 8 for high byte (AH, CH, DH, BH).
  uint8_t byte_offset;
} YCPURegisterAddress;

// The address of a memory operand.
typedef struct YCPUMemoryAddress {
  // Segment register. A uint8_t because YCPUApplySegmentOverride() writes it
  // through a pointer, and an enum's width is the target's business - one byte
  // under -fshort-enums and four otherwise.
  uint8_t segment_register_index;
  // Effective address offset.
  uint16_t offset;
} YCPUMemoryAddress;

// Whether the operand is a register or memory operand.
typedef enum YCPUOperandAddressType {
  Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER = 0,
  Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY,
} YCPUOperandAddressType;

enum {
  // Number of operand address types.
  Y_CPU_NUM_OPERAND_ADDRESS_TYPES = Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY + 1,
};

// Where an operand lives.
//
// Four bytes, which is the largest a composite AAPCS returns in a register
// rather than through memory.
typedef struct YCPUOperandAddress {
  // Y_CPU_OPERAND_ADDRESS_TYPE_REGISTER or Y_CPU_OPERAND_ADDRESS_TYPE_MEMORY. A
  // uint8_t rather than the enum to ensure the four byte layout.
  uint8_t type;
  // The register the operand is in, or the segment register it is addressed
  // through.
  uint8_t register_index;
  // The byte within that register - 0 for AL, 8 for AH - or the effective
  // address offset within that segment.
  uint16_t offset;
} YCPUOperandAddress;

// The value of an operand.
//
// A number rather than a struct, because a struct here is a struct on the
// stack: an operand value is produced and consumed on the hottest path in the
// emulator, and one that does not fit in a register travels through memory
// every time it is passed, returned or assigned.
//
// A byte-wide value is held in the low byte with the high byte zero. Every
// path that produces one truncates to a byte, which is what lets a consumer
// widen it by doing nothing. What the type no longer carries is the width, and
// the width is what sign extension needs - so that is passed explicitly, from
// the opcode table entry the caller already has.
typedef uint16_t YCPUOperandValue;

// An operand.
typedef struct YCPUOperand {
  // Address of the operand.
  YCPUOperandAddress address;
  // Value of the operand.
  YCPUOperandValue value;
} YCPUOperand;

// Instruction types.

struct YCPUOpcodeMetadata;

// Context during instruction execution.
typedef struct {
  YCPUState* cpu;
  const YCPUInstruction* instruction;
  const struct YCPUOpcodeMetadata* metadata;
} YCPUInstructionContext;

// Handler function for an opcode.
typedef YCPUInstructionResult (*YCPUOpcodeHandler)(
    const YCPUInstructionContext* context);

// An entry in the opcode lookup table.
typedef struct YCPUOpcodeMetadata {
  // What the instruction costs before the effective address computation and
  // its time on the data bus - see cycles.c for how the three fit together.
  uint8_t base_cycles;

  // Instruction has ModR/M byte.
  bool has_modrm;

  // Number of immediate data bytes: 0, 1, 2, or 4.
  uint8_t immediate_size;

  // Width of the instruction's operands.
  YCPUWidth width : 8;

  // Handler function.
  YCPUOpcodeHandler handler;
} YCPUOpcodeMetadata;

#endif  // Y_CPU_TYPES_H
