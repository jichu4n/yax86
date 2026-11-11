#include "psram.h"

#include <string.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/sync.h"
#include "pico/time.h"
#include "psram.pio.h"

// The LCD has pio0.
static PIO const kPsramPIO = pio1;

enum {
  kPsramPinCS = 20,
  kPsramPinSCK = 21,
  kPsramPinMOSI = 2,
  kPsramPinMISO = 3,

  // The serial clock. The chip is rated to 133MHz, but the board is not:
  // 83MHz and 125MHz read back wrong, and 62.5MHz is what the system clocks
  // the PicoCalc runs at - 125MHz and 250MHz - divide to evenly.
  kPsramSerialHz = 62500000,
  kPsramCyclesPerBit = 2,

  kPsramCommandFastRead = 0x0B,
  kPsramCommandWrite = 0x02,
  kPsramCommandReadID = 0x9F,
  // The ID's second byte, known good die, which a working chip reads as.
  kPsramKnownGoodDie = 0x5D,

  // The most a transfer moves with chip select held low. The chip must see
  // it raised within 8us, so that it can refresh, and 32 bytes at 62.5MHz is
  // 4.8us with the command - where 64 would be 8.8.
  kPsramBurstBytes = 32,

  // How long a transfer may take before it is given up, many times what one
  // takes.
  kPsramTimeoutUs = 100,

  // The self-test's block, and where it writes it: the ends and the middle.
  kPsramTestBytes = 512,
  kPsramTestBlocks = 3,
  kMicrosecondsPerMillisecond = 1000,
  // How long the chip needs after power before its first command.
  kPsramPowerUpUs = 150,
};

static uint g_sm;
static uint g_offset;
static PsramStats g_stats;

static const uint32_t kPsramTestAddresses[kPsramTestBlocks] = {
    0, kPsramSize / 2 - kPsramTestBytes / 2, kPsramSize - kPsramTestBytes};

const char* PsramStateName(PsramState state) {
  switch (state) {
    case kPsramOff:
      return "off";
    case kPsramOk:
      return "ok";
    case kPsramUnsupportedClock:
      return "unsupported_clock";
    case kPsramNoChip:
      return "no_chip";
    case kPsramTestFailed:
      return "test_failed";
    case kPsramTimedOut:
      return "timed_out";
    case kPsramSkippedAfterHang:
      return "skipped_after_hang";
  }
  return "unknown";
}

// One transaction with chip select held low: the command word, then
// out_count words written, then in_count words read. Returns false if the
// state machine stopped keeping up, leaving chip select raised either way.
//
// Each word goes out most significant bit first, so memory's little-endian
// words are swapped on the way out and on the way in.
static bool __not_in_flash_func(PsramTransfer)(
    uint32_t command, const uint32_t* out, uint32_t out_count, uint32_t* in,
    uint32_t in_count) {
  const uint32_t write_bits = (1 + out_count) * 32;
  const uint32_t read_bits = in_count * 32;
  io_rw_32* const txf = &kPsramPIO->txf[g_sm];
  io_ro_32* const rxf = &kPsramPIO->rxf[g_sm];
  const uint32_t tx_full = 1u << (PIO_FSTAT_TXFULL_LSB + g_sm);
  const uint32_t tx_empty = 1u << (PIO_FSTAT_TXEMPTY_LSB + g_sm);
  const uint32_t rx_empty = 1u << (PIO_FSTAT_RXEMPTY_LSB + g_sm);

  // Nothing may hold chip select low past the chip's 8us, which an interrupt
  // taken part way through would: USB's ran past the timeout, the state
  // machine stalled on its FIFO meanwhile, and words came back wrong. A burst
  // is 5us, which is what interrupts wait at most.
  const uint32_t interrupts = save_and_disable_interrupts();
  gpio_put(kPsramPinCS, 0);
  *txf = ((write_bits - 1) << 16) | (read_bits ? read_bits - 1 : 0);
  *txf = command;
  const uint32_t start_us = time_us_32();
  bool ok = true;
  while (out_count > 0 || in_count > 0) {
    const uint32_t fstat = kPsramPIO->fstat;
    if (out_count > 0 && !(fstat & tx_full)) {
      *txf = __builtin_bswap32(*out++);
      --out_count;
    } else if (in_count > 0 && !(fstat & rx_empty)) {
      *in++ = __builtin_bswap32(*rxf);
      --in_count;
    } else if (time_us_32() - start_us > kPsramTimeoutUs) {
      ok = false;
      break;
    }
  }
  // A write is over once the state machine has shifted out its last word and
  // gone back for the next header, which it waits for at the top of the
  // program.
  if (ok && read_bits == 0) {
    while (!(kPsramPIO->fstat & tx_empty) ||
           pio_sm_get_pc(kPsramPIO, g_sm) != g_offset) {
      if (time_us_32() - start_us > kPsramTimeoutUs) {
        ok = false;
        break;
      }
    }
  }
  gpio_put(kPsramPinCS, 1);
  restore_interrupts(interrupts);
  if (!ok) {
    ++g_stats.timeouts;
    g_stats.state = kPsramTimedOut;
    // Whatever it was part way through is abandoned: the FIFOs emptied, and
    // the program started again from the top.
    pio_sm_set_enabled(kPsramPIO, g_sm, false);
    pio_sm_clear_fifos(kPsramPIO, g_sm);
    pio_sm_restart(kPsramPIO, g_sm);
    pio_sm_exec(kPsramPIO, g_sm, pio_encode_jmp(g_offset));
    pio_sm_set_enabled(kPsramPIO, g_sm, true);
  }
  return ok;
}

// The bytes from address to the end of its burst, or size if fewer: a burst
// never crosses a kPsramBurstBytes boundary, and so never the chip's 1KB
// pages.
static inline uint32_t PsramBurstSize(uint32_t address, uint32_t size) {
  const uint32_t to_boundary =
      kPsramBurstBytes - (address & (kPsramBurstBytes - 1));
  return size < to_boundary ? size : to_boundary;
}

static bool __not_in_flash_func(PsramReadUnchecked)(
    uint32_t address, void* data, uint32_t size) {
  uint32_t* words = (uint32_t*)data;
  while (size > 0) {
    const uint32_t burst = PsramBurstSize(address, size);
    if (!PsramTransfer(
            ((uint32_t)kPsramCommandFastRead << 24) | address, NULL, 0, words,
            burst / 4)) {
      return false;
    }
    address += burst;
    words += burst / 4;
    size -= burst;
  }
  return true;
}

static bool __not_in_flash_func(PsramWriteUnchecked)(
    uint32_t address, const void* data, uint32_t size) {
  const uint32_t* words = (const uint32_t*)data;
  while (size > 0) {
    const uint32_t burst = PsramBurstSize(address, size);
    if (!PsramTransfer(
            ((uint32_t)kPsramCommandWrite << 24) | address, words, burst / 4,
            NULL, 0)) {
      return false;
    }
    address += burst;
    words += burst / 4;
    size -= burst;
  }
  return true;
}

bool __not_in_flash_func(PsramRead)(
    uint32_t address, void* data, uint32_t size) {
  if (g_stats.state != kPsramOk) {
    return false;
  }
  g_stats.bytes_read += size;
  return PsramReadUnchecked(address, data, size);
}

bool __not_in_flash_func(PsramWrite)(
    uint32_t address, const void* data, uint32_t size) {
  if (g_stats.state != kPsramOk) {
    return false;
  }
  g_stats.bytes_written += size;
  return PsramWriteUnchecked(address, data, size);
}

// Start the state machine. The input synchronizer is bypassed: it would add
// two cycles to a path already a bit late - see src/psram.pio.
static void PsramInitSerial(uint32_t divider) {
  g_offset = (uint)pio_add_program(kPsramPIO, &psram_program);
  g_sm = (uint)pio_claim_unused_sm(kPsramPIO, true);
  pio_sm_config config = psram_program_get_default_config(g_offset);
  sm_config_set_out_pins(&config, kPsramPinMOSI, 1);
  sm_config_set_in_pins(&config, kPsramPinMISO);
  sm_config_set_sideset_pins(&config, kPsramPinSCK);
  // Whole words, most significant bit first, both ways.
  sm_config_set_out_shift(&config, false, true, 32);
  sm_config_set_in_shift(&config, false, true, 32);
  sm_config_set_clkdiv_int_frac8(&config, divider, 0);
  pio_gpio_init(kPsramPIO, kPsramPinSCK);
  pio_gpio_init(kPsramPIO, kPsramPinMOSI);
  pio_gpio_init(kPsramPIO, kPsramPinMISO);
  gpio_set_slew_rate(kPsramPinSCK, GPIO_SLEW_RATE_FAST);
  gpio_set_drive_strength(kPsramPinSCK, GPIO_DRIVE_STRENGTH_8MA);
  gpio_set_slew_rate(kPsramPinMOSI, GPIO_SLEW_RATE_FAST);
  pio_sm_set_pins_with_mask(
      kPsramPIO, g_sm, 0, (1u << kPsramPinSCK) | (1u << kPsramPinMOSI));
  pio_sm_set_pindirs_with_mask(
      kPsramPIO, g_sm, (1u << kPsramPinSCK) | (1u << kPsramPinMOSI),
      (1u << kPsramPinSCK) | (1u << kPsramPinMOSI) | (1u << kPsramPinMISO));
  hw_set_bits(&kPsramPIO->input_sync_bypass, 1u << kPsramPinMISO);
  pio_sm_init(kPsramPIO, g_sm, g_offset, &config);
  pio_sm_set_enabled(kPsramPIO, g_sm, true);
}

// Write a block of noise to each test address, read them back, and time the
// first of each.
static PsramState PsramSelfTest(void) {
  uint32_t written[kPsramTestBytes / 4];
  uint32_t read[kPsramTestBytes / 4];
  uint32_t seed = 0x2545F491;
  for (uint32_t block = 0; block < kPsramTestBlocks; ++block) {
    for (uint32_t i = 0; i < kPsramTestBytes / 4; ++i) {
      seed ^= seed << 13;
      seed ^= seed >> 17;
      seed ^= seed << 5;
      written[i] = seed;
    }
    const uint32_t address = kPsramTestAddresses[block];
    const uint32_t write_start_us = time_us_32();
    if (!PsramWriteUnchecked(address, written, sizeof(written))) {
      return kPsramTimedOut;
    }
    const uint32_t read_start_us = time_us_32();
    if (!PsramReadUnchecked(address, read, sizeof(read))) {
      return kPsramTimedOut;
    }
    const uint32_t end_us = time_us_32();
    if (block == 0) {
      g_stats.write_512_us = read_start_us - write_start_us;
      g_stats.read_512_us = end_us - read_start_us;
    }
    if (memcmp(written, read, sizeof(written)) != 0) {
      return kPsramTestFailed;
    }
  }
  return kPsramOk;
}

bool PsramInit(bool skip) {
  if (skip) {
    g_stats.state = kPsramSkippedAfterHang;
    return false;
  }
  const uint32_t sys_hz = clock_get_hz(clk_sys);
  const uint32_t cycles_per_bit_at_rate = kPsramCyclesPerBit * kPsramSerialHz;
  if (sys_hz % cycles_per_bit_at_rate != 0) {
    g_stats.state = kPsramUnsupportedClock;
    return false;
  }
  g_stats.serial_hz = kPsramSerialHz;

  gpio_init(kPsramPinCS);
  gpio_set_dir(kPsramPinCS, GPIO_OUT);
  gpio_put(kPsramPinCS, 1);
  busy_wait_us(kPsramPowerUpUs);
  PsramInitSerial(sys_hz / cycles_per_bit_at_rate);

  // The ID follows the address with no dummy cycles, so the nine cycles a
  // read skips take its first byte, the manufacturer's, and leave the known
  // good die byte first.
  uint32_t id = 0;
  if (!PsramTransfer((uint32_t)kPsramCommandReadID << 24, NULL, 0, &id, 1)) {
    return false;
  }
  g_stats.kgd = (uint8_t)id;
  if (g_stats.kgd != kPsramKnownGoodDie) {
    g_stats.state = kPsramNoChip;
    return false;
  }
  g_stats.state = PsramSelfTest();
  return g_stats.state == kPsramOk;
}

PsramStats PsramGetStats(void) { return g_stats; }
