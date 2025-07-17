#ifndef YAX86_IMPLEMENTATION
#include "platform.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

// Default MDA state.
static const MDAState kDefaultMDAState = {
    .config = NULL,
    .registers =
        {
            0x61,
            0x50,
            0x52,
            0x0F,
            0x19,
            0x06,
            0x19,
            0x19,
            0x02,
            0x0D,
            0x0B,
            0x0C,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
        },
    .selected_register = 0,
    // high resolution mode, video enable, blink enable
    .control_port = 0x29,
    .status_port = 0x00,
};

static inline uint8_t ReadVRAMByte(MDAState* mda, uint32_t address) {
  if (mda->config && mda->config->read_vram_byte &&
      address < kMDAModeMetadata.vram_size) {
    return mda->config->read_vram_byte(mda, address);
  }
  return 0xFF;
}

static inline void WriteVRAMByte(
    MDAState* mda, uint32_t address, uint8_t value) {
  if (mda->config && mda->config->write_vram_byte &&
      address < kMDAModeMetadata.vram_size) {
    mda->config->write_vram_byte(mda, address, value);
  }
}

// Initialize MDA state with the provided configuration.
void MDAInit(MDAState* mda, MDAConfig* config) {
  *mda = kDefaultMDAState;
  mda->config = config;

  for (uint32_t i = 0; i < kMDAModeMetadata.vram_size; i += 2) {
    WriteVRAMByte(mda, i, ' ');
    WriteVRAMByte(mda, i + 1, 0x07 /* default attr */);
  }
}

static uint8_t PlatformReadVRAMByte(MemoryMapEntry* entry, uint32_t address) {
  MDAState* mda = (MDAState*)entry->context;
  return ReadVRAMByte(mda, address);
}

static void PlatformWriteVRAMByte(
    MemoryMapEntry* entry, uint32_t address, uint8_t value) {
  MDAState* mda = (MDAState*)entry->context;
  WriteVRAMByte(mda, address, value);
}

static uint8_t PlatformReadPortByte(PortMapEntry* entry, uint16_t port) {
  MDAState* mda = (MDAState*)entry->context;
  switch (port) {
    case kMDAPortRegisterIndex:
      return mda->selected_register;
    case kMDAPortRegisterData:
      if (mda->selected_register < kMDANumRegisters) {
        return mda->registers[mda->selected_register];
      }
      return 0xFF;
    case kMDAPortControl:
      return mda->control_port;
    case kMDAPortStatus:
      return mda->status_port;
    default:
      return 0xFF;
  }
}

static void PlatformWritePortByte(
    PortMapEntry* entry, uint16_t port, uint8_t value) {
  MDAState* mda = (MDAState*)entry->context;
  switch (port) {
    case kMDAPortRegisterIndex:
      mda->selected_register = value;
      break;
    case kMDAPortRegisterData:
      if (mda->selected_register < kMDANumRegisters) {
        mda->registers[mda->selected_register] = value;
      }
      break;
    case kMDAPortControl:
      mda->control_port = value;
      break;
    case kMDAPortStatus:
      mda->status_port = value;
      break;
    default:
      break;
  }
}

// Register memory map and I/O ports.
bool MDASetup(MDAState* mda, PlatformState* platform) {
  bool status = true;

  MemoryMapEntry vram_entry = {
      .context = mda,
      .entry_type = kMemoryMapEntryMDAVRAM,
      .start = kMDAModeMetadata.vram_address,
      .end = kMDAModeMetadata.vram_address + kMDAModeMetadata.vram_size - 1,
      .read_byte = PlatformReadVRAMByte,
      .write_byte = PlatformWriteVRAMByte,
  };
  status = status && RegisterMemoryMapEntry(platform, &vram_entry);

  PortMapEntry port_entry = {
      .context = mda,
      .entry_type = kPortMapEntryMDA,
      .start = 0x3B0,
      .end = 0x3BF,
      .read_byte = PlatformReadPortByte,
      .write_byte = PlatformWritePortByte,
  };
  status = status && RegisterPortMapEntry(platform, &port_entry);

  return status;
}

enum {
  // Position of underline in MDA text mode.
  kMDAUnderlinePosition = 12,
};

// Render the current display. Invokes the write_pixel callback to do the actual
// pixel rendering.
bool MDARender(MDAState* mda) { return true; }
