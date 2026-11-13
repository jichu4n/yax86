#include "usb_screen.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "browser_screen.h"
#include "disk_image.h"
#include "display.h"
#include "fs.h"
#include "runtime_log.h"
#include "status_bar.h"
#include "text.h"
#include "ui_box.h"
#include "ui_page.h"
#include "usb_disk.h"
#include "usb_sharing_screen.h"

typedef enum USBItem {
  kUSBCard = 0,
  kUSBImage,
  kUSBItemCount,
} USBItem;

static const UIBoxItem kUSBItems[kUSBItemCount] = {
    [kUSBCard] = {"Whole SD card", false},
    [kUSBImage] = {"Disk image", true},
};

extern const Screen kUSBScreen;

// The item chosen, and whether the page is out of date.
static UIPage g_page;
// The parent's, set once sharing has ended.
static bool* g_shared;
// Whether sharing has ended, which the sharing screen sets.
static bool g_stopped;
// The image found in the browser, empty for none, and its size.
static char g_found_path[kFSMaxPath + 1];
static uint32_t g_found_size;
// The disk image on USB, while one is, and its path.
static DiskImage g_image;
static char g_image_path[kFSMaxPath + 1];

ScreenResult USBScreenOpen(bool* shared) {
  g_shared = shared;
  g_page.chosen = kUSBCard;
  g_page.dirty = true;
  return ScreenOpen(&kUSBScreen);
}

// Share the SD card over USB: the card is unmounted first, as nothing here
// may touch it while the computer has it.
static ScreenResult ShareCard(void) {
  DiskImageUnmountCard();
  UsbDiskShareCard();
  g_stopped = false;
  SYSTEM_INFO("usb_card_shared");
  return SharingScreenOpen(NULL, &g_stopped);
}

// Put the disk image found in the browser on USB, the card staying mounted
// to read it through; or say why it could not be, back in the browser.
// Kept out of line, so that its message is on core 0's stack only while it
// runs.
static __noinline ScreenResult ShareImage(void) {
  FSCopyPath(g_found_path, g_image_path);
  const DiskImageStatus status =
      DiskImageOpen(&g_image, g_image_path, g_found_size);
  if (status != kDiskImageOpen) {
    char name[kDiskImageNameLength + 1];
    DiskImageName(g_image_path, name);
    char message[kTextColumns + 1];
    snprintf(
        message, sizeof(message), "Could not open %s: %s", name,
        DiskImageDescribeStatus(status));
    StatusBarSetProblem(message);
    SYSTEM_WARN(
        "usb_image_not_shared", Y_LOG_STRING("path", g_image_path),
        Y_LOG_STRING("error", DiskImageDescribeStatus(status)));
    return BrowserScreenReopen();
  }
  UsbDiskShareImage(&g_image);
  g_stopped = false;
  SYSTEM_INFO("usb_image_shared", Y_LOG_STRING("path", g_image_path));
  return SharingScreenOpen(&g_image, &g_stopped);
}

static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  switch (UIPageTakeEvent(&g_page, event, kUSBItemCount)) {
    case kUIPageKeyChoose:
      if (g_page.chosen == kUSBImage) {
        return BrowserScreenOpen(
            kUIBrowserEither, "yax86", NULL, g_found_path, &g_found_size);
      }
      return ShareCard();
    case kUIPageKeyBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

// Back from sharing, which has ended, or from the browser, with an image
// found or none.
static ScreenResult Resume(void) {
  if (g_stopped) {
    g_stopped = false;
    *g_shared = true;
    return ScreenBack();
  }
  g_page.dirty = true;
  if (g_found_path[0] != '\0') {
    return ShareImage();
  }
  return ScreenStays();
}

static void Draw(Y_UNUSED uint64_t now_us) {
  DisplayMenu* menu = UIPageBeginDraw(&g_page);
  if (menu == NULL) {
    return;
  }
  UIPageDrawItems(
      menu, "USB disk mode", NULL, 0, kUSBItems, kUSBItemCount, g_page.chosen,
      true);
  UIPageEndDraw(&g_page);
}

static const char* Hint(void) { return kUIPageHint; }

static const char* PageName(void) { return "usb"; }

const Screen kUSBScreen = {
    .handle_key = HandleKey,
    .draw = Draw,
    .resume = Resume,
    .hint = Hint,
    .page_name = PageName,
};
