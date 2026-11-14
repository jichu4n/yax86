// FatFs's disk interface, on the SD card. FatFs is configured for one drive
// and 512 byte sectors, which are the card's blocks.

#include <stdbool.h>

// diskio.h uses FatFs's types without including ff.h, so ff.h has to come
// first, which sorting the includes would undo.
// clang-format off
#include "ff.h"
#include "diskio.h"
// clang-format on
#include "sdcard.h"

static bool g_is_card_up;

DSTATUS disk_status(BYTE pdrv) {
  return pdrv == 0 && g_is_card_up ? 0 : STA_NOINIT;
}

DSTATUS disk_initialize(BYTE pdrv) {
  if (pdrv != 0) {
    return STA_NOINIT;
  }
  g_is_card_up = SdcardInit();
  return g_is_card_up ? 0 : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
  if (pdrv != 0 || !g_is_card_up) {
    return RES_NOTRDY;
  }
  return SdcardReadBlocks(sector, buff, count) ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
  if (pdrv != 0 || !g_is_card_up) {
    return RES_NOTRDY;
  }
  return SdcardWriteBlocks(sector, buff, count) ? RES_OK : RES_ERROR;
}

// FatFs asks for nothing but a sync, with the configuration it has here.
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
  (void)buff;
  if (pdrv != 0 || !g_is_card_up) {
    return RES_NOTRDY;
  }
  if (cmd != CTRL_SYNC) {
    return RES_PARERR;
  }
  return SdcardSync() ? RES_OK : RES_ERROR;
}
