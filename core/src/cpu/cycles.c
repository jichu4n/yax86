#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "cycles.h"
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
// 1. A base cost per opcode, below. These are the published 8086 figures for
//    the register form, less the time the figure already accounts for on the
//    bus, which is charged separately in part 3.
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

// Cycles per byte transferred over the data bus.
enum { kBusCyclesPerByte = 4 };

// Base execution cost per opcode, excluding both the effective address
// calculation and time on the data bus.
//
// Where the published figure covers an instruction that necessarily touches
// memory - the stack instructions, the string instructions, the software
// interrupts - the bus time it includes has been taken back out, so that
// charging the traffic separately does not count it twice.
YAX86_PRIVATE const uint8_t kOpcodeBaseCycles[256] = {
    // 0x00: ALU r/m,r and r,r/m are 3; with an immediate, 4. PUSH sreg is 10
    // for a 2 byte write, POP sreg 8.
    3, 3, 3, 3, 4, 4, 2, 0,        // 00 ADD, 06 PUSH ES, 07 POP ES
    3, 3, 3, 3, 4, 4, 2, 0,        // 08 OR, 0E PUSH CS, 0F POP CS
    3, 3, 3, 3, 4, 4, 2, 0,        // 10 ADC, 16 PUSH SS, 17 POP SS
    3, 3, 3, 3, 4, 4, 2, 0,        // 18 SBB, 1E PUSH DS, 1F POP DS
    3, 3, 3, 3, 4, 4, 2, 4,        // 20 AND, 26 ES:, 27 DAA
    3, 3, 3, 3, 4, 4, 2, 4,        // 28 SUB, 2E CS:, 2F DAS
    3, 3, 3, 3, 4, 4, 2, 8,        // 30 XOR, 36 SS:, 37 AAA
    3, 3, 3, 3, 4, 4, 2, 8,        // 38 CMP, 3E DS:, 3F AAS
    // 0x40: INC and DEC of a 16 bit register are 2 each.
    2, 2, 2, 2, 2, 2, 2, 2,        // 40 INC r16
    2, 2, 2, 2, 2, 2, 2, 2,        // 48 DEC r16
    // 0x50: PUSH is 11 and POP 8, both less the 8 cycles of their word access.
    3, 3, 3, 3, 3, 3, 3, 3,        // 50 PUSH r16
    0, 0, 0, 0, 0, 0, 0, 0,        // 58 POP r16
    // 0x60: undocumented aliases of the conditional jumps at 0x70.
    4, 4, 4, 4, 4, 4, 4, 4,        // 60 Jcc alias
    4, 4, 4, 4, 4, 4, 4, 4,        // 68 Jcc alias
    // 0x70: not taken. A taken jump adds 12 for the flushed queue.
    4, 4, 4, 4, 4, 4, 4, 4,        // 70 Jcc
    4, 4, 4, 4, 4, 4, 4, 4,        // 78 Jcc
    // 0x80: group 1 with an immediate, TEST, XCHG, MOV.
    4, 4, 4, 4, 3, 3, 4, 4,        // 80 group 1, 84 TEST, 86 XCHG
    2, 2, 2, 2, 2, 2, 2, 0,        // 88 MOV, 8C MOV sreg, 8D LEA, 8F POP r/m
    // 0x90: NOP and XCHG with AX are 3. CALL far is 28 less its 4 byte write.
    3, 3, 3, 3, 3, 3, 3, 3,        // 90 NOP, 91 XCHG AX,r
    2, 5, 12, 3, 2, 0, 4, 4,       // 98 CBW, 99 CWD, 9A CALL far, 9C PUSHF
    // 0xA0: MOV to and from a direct address, and the string instructions,
    // all less their bus time.
    2, 2, 2, 2, 10, 10, 14, 14,    // A0 MOV moffs, A4 MOVS, A6 CMPS
    4, 4, 3, 3, 4, 4, 7, 7,        // A8 TEST, AA STOS, AC LODS, AE SCAS
    // 0xB0: MOV immediate into a register.
    4, 4, 4, 4, 4, 4, 4, 4,        // B0 MOV r8, imm8
    4, 4, 4, 4, 4, 4, 4, 4,        // B8 MOV r16, imm16
    // 0xC0: RET, LES, LDS, MOV r/m immediate.
    12, 8, 12, 8, 8, 8, 2, 2,      // C0 RET aliases, C2 RET, C4 LES, C6 MOV
    9, 10, 9, 10, 1, 1, 3, 32,     // C8 RETF aliases, CC INT3, CD INT, CF IRET
    // 0xD0: shifts and rotates. By CL adds 4 per bit, charged at execution.
    2, 2, 8, 8, 8, 8, 2, 11,       // D0 shift by 1, D2 shift by CL, D4 AAM
    0, 0, 0, 0, 0, 0, 0, 0,        // D8 ESC, no coprocessor is present
    // 0xE0: LOOP and the conditional jumps on CX, then IN, OUT, CALL and JMP.
    5, 6, 6, 6, 10, 10, 10, 10,    // E0 LOOPNZ, E3 JCXZ, E4 IN, E6 OUT
    11, 15, 15, 15, 8, 8, 8, 8,    // E8 CALL, E9 JMP, EC IN DX, EE OUT DX
    // 0xF0: prefixes, HLT, the group 3 and group 4/5 instructions.
    2, 2, 2, 2, 2, 2, 3, 3,        // F0 LOCK, F2 REPNZ, F4 HLT, F6 group 3
    2, 2, 2, 2, 2, 2, 3, 3,        // F8 CLC, FA CLI, FC CLD, FE group 4/5
};

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
YAX86_PRIVATE uint8_t GetEffectiveAddressCycles(const Instruction* instruction) {
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
