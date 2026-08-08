#include "harddisk.h"

void HardDiskAttach(PlatformState* platform) {
  HDCAttachDrive(&platform->hdc, 0, &kHDCGeometry10MB);
}
