// What the Raspberry Pi Pico, an RP2040 with 264KB of SRAM, gives the guest.
// pico/bench and pico/picocalc both build against these, so the bench
// measures the machine the runtime runs.
//
// A guest has one of two memory layouts, chosen at startup by whether the
// PSRAM answers (psram.h):
//
//   - With the PSRAM, kPicoPagedGuestRAMSize of conventional memory, all of
//     it paged (pager.h): the pages live in PSRAM and are brought into
//     kPicoGuestSRAMSize of frames in SRAM when the guest touches them.
//   - Without it, such as on a bare Pico, kPicoGuestSRAMSize of conventional
//     memory held directly in the same SRAM.
#ifndef YAX86_PICO_H
#define YAX86_PICO_H

enum {
  // The SRAM given to guest memory: conventional memory itself without the
  // PSRAM, and the pager's frames with it. 128KB is what fits alongside
  // video memory, the SDK, the USB stack and the runtime.
  kPicoGuestSRAMSize = 128 * 1024,

  // Conventional memory with the PSRAM: 640KB, the most a PC/XT has.
  kPicoPagedGuestRAMSize = 640 * 1024,

  // A page, and a frame, is 1 << kPicoPageShift bytes: 256, the smallest
  // whose page table, 2 bytes a page and so 5KB for 640KB, the SRAM has room
  // for. Larger pages spend more of the time in page faults, measured in
  // pico/picocalc/AGENTS.md, "Paging guest RAM".
  kPicoPageShift = 8,
};

#endif  // YAX86_PICO_H
