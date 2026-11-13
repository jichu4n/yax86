#include "usb_storage_screen.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "browser_screen.h"
#include "disk_image.h"
#include "display.h"
#include "fs.h"
#include "list_box.h"
#include "menu_box.h"
#include "runtime_log.h"
#include "status_bar.h"
#include "text.h"
#include "usb_storage.h"
#include "usb_storage_active_screen.h"

typedef enum USBItem {
  kUSBCard = 0,
  kUSBImage,
  kUSBItemCount,
} USBItem;

static const MenuBoxItem kUSBItems[kUSBItemCount] = {
    [kUSBCard] = {"Entire SD card", false},
    [kUSBImage] = {"Disk image", true},
};

extern const Screen kUSBStorageScreen;

// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;
// The parent's, set once exporting has ended.
static bool* g_exported;
// Whether exporting has ended, which the active screen sets.
static bool g_stopped;
// The image found in the browser, empty for none, and its size.
static char g_found_path[kFSMaxPath + 1];
static uint32_t g_found_size;
// The disk image on USB, while one is, and its path.
static DiskImage g_image;
static char g_image_path[kFSMaxPath + 1];

ScreenResult USBStorageScreenOpen(bool* exported) {
  g_exported = exported;
  g_list_box.selected = kUSBCard;
  g_list_box.dirty = true;
  return ScreenOpen(&kUSBStorageScreen);
}

// Export the entire SD card: the card is unmounted first, as nothing here
// may touch it while the host has it.
static ScreenResult ShareCard(void) {
  DiskImageUnmountCard();
  UsbStorageExportCard();
  g_stopped = false;
  SYSTEM_INFO("usb_card_exported");
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
    StatusBarSetProblem(message);
    SYSTEM_WARN(
        "usb_image_not_exported", Y_LOG_STRING("path", g_image_path),
        Y_LOG_STRING("error", DiskImageDescribeStatus(status)));
    return BrowserScreenReopen();
  }
  UsbStorageExportImage(&g_image);
  g_stopped = false;
  SYSTEM_INFO("usb_image_exported", Y_LOG_STRING("path", g_image_path));
  return USBStorageActiveScreenOpen(&g_image, &g_stopped);
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxTakeEvent(&g_list_box, event, kUSBItemCount)) {
    case kListBoxKeySelect:
      if (g_list_box.selected == kUSBImage) {
        return BrowserScreenOpen(
            kFileBrowserEither, "yax86", NULL, g_found_path, &g_found_size);
      }
      return ShareCard();
    case kListBoxKeyBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

// Back from exporting, which has ended, or from the browser, with an image
// found or none.
static ScreenResult OnResume(void) {
  if (g_stopped) {
    g_stopped = false;
    *g_exported = true;
    return ScreenBack();
  }
  g_list_box.dirty = true;
  if (g_found_path[0] != '\0') {
    return ShareImage();
  }
  return ScreenStays();
}

static void OnDraw(Y_UNUSED uint64_t now_us) {
  DisplayMenu* menu = ListBoxBeginDraw(&g_list_box);
  if (menu == NULL) {
    return;
  }
  ListBoxDrawItems(
      menu, "USB mass storage", (const char* const[]){"Export to the host:"}, 1,
      kUSBItems, kUSBItemCount, g_list_box.selected, true);
  ListBoxEndDraw(&g_list_box);
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return "usb_storage"; }

const Screen kUSBStorageScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .on_resume = OnResume,
    .hint = Hint,
    .page_name = PageName,
};
