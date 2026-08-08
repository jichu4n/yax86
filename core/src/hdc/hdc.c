#ifndef YAX86_IMPLEMENTATION
#include "hdc_option_rom_data.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

void HDCInit(HDCState* hdc, HDCConfig* config) {
  static const HDCState zero_hdc_state = {0};
  *hdc = zero_hdc_state;

  hdc->config = config;
}

uint32_t HDCGetOptionROMSize(void) { return kHDCOptionROMDataSize; }

uint8_t HDCReadOptionROMByte(uint32_t offset) {
  if (offset >= kHDCOptionROMDataSize) {
    return kHDCOpenBusValue;
  }
  return kHDCOptionROMData[offset];
}
