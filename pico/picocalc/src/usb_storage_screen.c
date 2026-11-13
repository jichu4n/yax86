#include "usb_storage_screen.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "browser_screen.h"
#include "disk_image.h"
#include "display.h"
#include "fs.h"
#include "runtime_log.h"
#include "text.h"
#include "ui_box.h"
#include "ui_page.h"
#include "ui_status.h"
#include "usb_storage.h"
#include "usb_storage_active_screen.h"

typedef enum USBItem {
  kUSBCard = 0,
  kUSBImage,
  kUSBItemCount,
} USBItem;

static const UIBoxItem kUSBItems[kUSBItemCount] = {
    [kUSBCard] = {"Entire SD card", false},
    [kUSBImage] = {"Disk image", true},
};

extern const Screen kUSBStorageScreen;

static uint8_t g_chosen;
// Whether what is on the panel is out of date.
static bool g_dirty;
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

ScreenResult USBStorageScreenOpen(bool* shared) {
  g_shared = shared;
  g_chosen = kUSBCard;
  g_dirty = true;
  return ScreenOpen(&kUSBStorageScreen);
}

// Export the entire SD card: the card is unmounted first, as nothing here
// may touch it while the host has it.
static ScreenResult ShareCard(void) {
  DiskImageUnmountCard();
  UsbStorageExportCard();
  g_stopped = false;
  SYSTEM_INFO("usb_card_shared");
  return USBStorageActiveScreenOpen(NULL, &g_stopped);
}

// Export the disk image found in the browser, the card staying mounted to
// read it through; or say why it could not be, back in the browser.
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
    UIStatusSetProblem(message);
    SYSTEM_WARN(
        "usb_image_not_shared", Y_LOG_STRING("path", g_image_path),
        Y_LOG_STRING("error", DiskImageDescribeStatus(status)));
    return BrowserScreenReopen();
  }
  UsbStorageExportImage(&g_image);
  g_stopped = false;
  SYSTEM_INFO("usb_image_shared", Y_LOG_STRING("path", g_image_path));
  return USBStorageActiveScreenOpen(&g_image, &g_stopped);
}

static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  // A key held down repeats, which moves through the items as a press does.
  if (event.state == kSouthbridgeKeyReleased) {
    return ScreenStays();
  }
  UIStatusClearProblem();
  if (UIPageIsMoveKey(event.key)) {
    if (UIPageMove(event.key, kUSBItemCount, &g_chosen)) {
      g_dirty = true;
    }
    return ScreenStays();
  }
  switch (event.key) {
    case kKeyCodeEnter:
    case kKeyCodeSpace:
      if (g_chosen == kUSBImage) {
        return BrowserScreenOpen(
            kUIBrowserEither, "yax86", NULL, g_found_path, &g_found_size);
      }
      return ShareCard();
    case kKeyCodeBackspace:
    case kKeyCodeEsc:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

// Back from exporting, which has ended, or from the browser, with an image
// found or none.
static ScreenResult Resume(void) {
  if (g_stopped) {
    g_stopped = false;
    *g_shared = true;
    return ScreenBack();
  }
  g_dirty = true;
  if (g_found_path[0] != '\0') {
    return ShareImage();
  }
  return ScreenStays();
}

static void Draw(Y_UNUSED uint64_t now_us) {
  if (!g_dirty) {
    return;
  }
  DisplayMenu* menu = DisplayBeginMenu();
  if (menu == NULL) {
    return;
  }
  UIPageDrawItems(
      menu, "USB mass storage", (const char* const[]){"Export to the host:"}, 1,
      kUSBItems, kUSBItemCount, g_chosen, true);
  DisplayEndMenu();
  g_dirty = false;
}

static const char* Hint(void) {
  return "\x18\x19=Move  Enter=Select  Esc=Back";
}

static const char* PageName(void) { return "usb_storage"; }

const Screen kUSBStorageScreen = {
    .handle_key = HandleKey,
    .draw = Draw,
    .resume = Resume,
    .hint = Hint,
    .page_name = PageName,
};
