#ifndef YAX86_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

void FDCInit(FDCState* fdc, FDCConfig* config) {
  static const FDCState zero_fdc_state = {0};
  *fdc = zero_fdc_state;

  fdc->config = config;
}

uint8_t FDCReadPort(YAX86_UNUSED FDCState* fdc, YAX86_UNUSED uint16_t port) {
  // TODO: Implement FDC register reads.
  return 0xFF;  // Per convention for reads from unused/invalid ports.
}

void FDCWritePort(
    YAX86_UNUSED FDCState* fdc, YAX86_UNUSED uint16_t port,
    YAX86_UNUSED uint8_t value) {
  // TODO: Implement FDC register writes.
}

