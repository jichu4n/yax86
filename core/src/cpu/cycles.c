#ifndef YAX86_IMPLEMENTATION
#include "cycles.h"

#include "../util/common.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

// ============================================================================
// Instruction timing
// ============================================================================
//
// How long each instruction takes, in 4.77MHz CPU clock cycles. This is not a
// cycle-accurate model - it does not track the prefetch queue, and it charges
// a whole instruction at its boundary rather than spreading it over the bus
// cycles it really occupies. What it does give is a clock that means
// something: the ratio between time spent executing and time measured by the
// PIT comes out right, so a guest that calibrates a delay loop against the
// timer arrives at roughly the figure real hardware would.
//
// The cost of an instruction is built from three parts.
//
// 1. A base cost per opcode, which is OpcodeMetadata.base_cycles in
//    opcode_table.c. These are the published 8086 figures for the register
//    form, less the time the figure already accounts for on the bus, which is
//    charged separately in part 3. Where the published figure covers an
//    instruction that necessarily touches memory - the stack instructions, the
//    string instructions, the software interrupts - that bus time has been
//    taken back out, so that charging the traffic separately does not count it
//    twice. PUSH ES is 2 rather than 10 for this reason, and POP r16 is 0.
//
// 2. The effective address calculation, for instructions that address memory
//    through a ModR/M byte.
//
// 3. Four cycles for every byte the instruction moves over the data bus. The
//    8088 has an 8-bit bus, so a word costs twice a byte - this is the main
//    reason it is slower than the 8086 it shares timings with, and it is the
//    dominant term for most instructions. Charging it from the actual
//    accesses rather than from a table means the string instructions, the
//    stack and the interrupt sequence all cost what their traffic costs,
//    including when REP runs them many times over.
//
// Instructions whose cost depends on more than their operands - a conditional
// jump that is taken, a shift by a count in CL, a multiply or divide - add the
// difference themselves through CPUAddCycles().

// Cycles to compute an effective address, by addressing mode. The 8086 pays
// for each component it has to add together.
enum {
  // A displacement on its own.
  kEACyclesDisplacementOnly = 6,
  // A single base or index register.
  kEACyclesBaseOrIndex = 5,
  // A base or index register plus a displacement.
  kEACyclesBaseOrIndexAndDisplacement = 9,
  // Base plus index. BP+DI and BX+SI cost one cycle less than the other two
  // pairings, which this does not distinguish.
  kEACyclesBaseAndIndex = 8,
  // Base plus index plus a displacement.
  kEACyclesBaseAndIndexAndDisplacement = 12,
  // A segment override prefix costs two more, since the address has to be
  // formed against a different segment base.
  kEACyclesSegmentOverride = 2,
};

// Cycles to compute the effective address of a ModR/M memory operand.
YAX86_PRIVATE uint8_t
GetEffectiveAddressCycles(const Instruction* instruction) {
  if (!instruction->has_mod_rm || instruction->mod_rm.mod == 0x03) {
    // A register operand needs no address computed.
    return 0;
  }

  const uint8_t mod = instruction->mod_rm.mod;
  const uint8_t rm = instruction->mod_rm.rm;
  const bool has_displacement =
      mod == 0x01 || mod == 0x02 || (mod == 0x00 && rm == 0x06);
  // R/M values 0 through 3 pair a base register with an index register. The
  // rest name a single register, except for the direct address at mod 0, rm 6.
  const bool has_base_and_index = rm <= 0x03;
  const bool is_direct_address = mod == 0x00 && rm == 0x06;

  uint8_t cycles;
  if (is_direct_address) {
    cycles = kEACyclesDisplacementOnly;
  } else if (has_base_and_index) {
    cycles = has_displacement ? kEACyclesBaseAndIndexAndDisplacement
                              : kEACyclesBaseAndIndex;
  } else {
    cycles = has_displacement ? kEACyclesBaseOrIndexAndDisplacement
                              : kEACyclesBaseOrIndex;
  }

  // Charged once for an instruction that carries a segment override, rather
  // than per override prefix. Only one can take effect, and real code never
  // emits more than one.
  if (instruction->segment_override != kNoSegmentOverride) {
    cycles += kEACyclesSegmentOverride;
  }
  return cycles;
}

YAX86_PRIVATE void AddBusCycles(CPUState* cpu, uint8_t num_bytes) {
  cpu->pending_cycles += (uint16_t)num_bytes * kBusCyclesPerByte;
}

void CPUAddCycles(CPUState* cpu, uint16_t cycles) {
  cpu->pending_cycles += cycles;
}
