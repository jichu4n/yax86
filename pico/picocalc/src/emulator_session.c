#include "emulator_session.h"

#include <stddef.h>

#include "floppy.h"

uint8_t EmulatorSessionFloppyDrives(const EmulatorSession* session) {
  return session->config != NULL && !session->config->present[kVMDriveFD2]
             ? 1
             : kFloppyDrives;
}

bool EmulatorSessionCanPowerOff(const EmulatorSession* session) {
  return session->config != NULL;
}
