#include "launcher_usb.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "disk_image.h"
#include "display.h"
#include "fs.h"
#include "launcher.h"
#include "runtime_log.h"
#include "ui_box.h"
#include "ui_browser.h"
#include "ui_page.h"
#include "usb_disk.h"

enum {
  kStatusRefreshUs = 1000 * 1000,
};

typedef enum UsbPage {
  // What to share: the whole card, or an image found with the browser.
  kUsbPageMenu = 0,
  // The card or image while it is on USB, and the question before taking it
  // back while a computer is still using it.
  kUsbPageSharing,
  kUsbPageStopSharing,
  kUsbPageBrowse,
} UsbPage;

static const char* const kPageNames[] = {
    [kUsbPageMenu] = "usb",
    [kUsbPageSharing] = "usb_sharing",
    [kUsbPageStopSharing] = "usb_stop",
    [kUsbPageBrowse] = "browse",
};

typedef enum USBItem {
  kUSBCard = 0,
  kUSBImage,
  kUSBItemCount,
} USBItem;

static const UIBoxItem kUSBItems[kUSBItemCount] = {
    [kUSBCard] = {"Whole SD card", false},
    [kUSBImage] = {"Disk image", true},
};

static const UIBoxItem kSharingItems[] = {{"Disconnect", false}};

typedef enum StopSharingItem {
  kStopSharingCancel = 0,
  kStopSharingConfirm,
  kStopSharingItemCount,
} StopSharingItem;

static const UIBoxItem kStopSharingItems[kStopSharingItemCount] = {
    [kStopSharingCancel] = {"Cancel", false},
    [kStopSharingConfirm] = {"Disconnect anyway", false},
};

static UsbPage g_page;
static uint8_t g_chosen;
// Whether what is on the panel is out of date, and when the card's page was
// last drawn.
static bool g_dirty;
static uint64_t g_status_drawn_us;
// The browser, finding an image to put on USB.
static UIBrowser g_browser;
// The disk image on USB, while one is, and its path.
static DiskImage g_usb_image;
static char g_usb_image_path[kFSMaxPath + 1];

static void ShowPage(UsbPage page, uint8_t chosen) {
  g_page = page;
  g_chosen = chosen;
  g_dirty = true;
}

void LauncherUsbOpen(void) { ShowPage(kUsbPageMenu, kUSBCard); }

const char* LauncherUsbPageName(void) { return kPageNames[g_page]; }

const char* LauncherUsbHint(void) {
  if (g_page == kUsbPageSharing) {
    return "Enter=Disconnect";
  }
  if (g_page == kUsbPageBrowse) {
    return "\x18\x19=Move  Enter=Open  Backspace=Up a folder  Esc=Back";
  }
  return "\x18\x19=Move  Enter=Select  Esc=Back";
}

// Whether disconnecting would take the card from a computer still using it.
static bool ComputerHasCard(void) {
  const UsbDiskStats stats = UsbDiskGetStats();
  return stats.connected && stats.seen && !stats.ejected;
}

// Share the SD card over USB: the card is unmounted first, as
// nothing here may touch it while the computer has it.
static void ShareCard(void) {
  DiskImageUnmountCard();
  UsbDiskShareCard();
  ShowPage(kUsbPageSharing, 0);
  SYSTEM_INFO("usb_card_shared");
}

// Put the disk image chosen in the browser on USB, the card staying mounted
// to read it through; or say why it could not be, staying in the browser.
static __noinline void ShareImage(const char* path, uint32_t size) {
  FSCopyPath(path, g_usb_image_path);
  const DiskImageStatus status =
      DiskImageOpen(&g_usb_image, g_usb_image_path, size);
  if (status != kDiskImageOpen) {
    char name[kDiskImageNameLength + 1];
    DiskImageName(g_usb_image_path, name);
    char message[kTextColumns + 1];
    snprintf(
        message, sizeof(message), "Could not open %s: %s", name,
        DiskImageDescribeStatus(status));
    LauncherSetProblem(message);
    SYSTEM_WARN(
        "usb_image_not_shared", Y_LOG_STRING("path", g_usb_image_path),
        Y_LOG_STRING("error", DiskImageDescribeStatus(status)));
    return;
  }
  UsbDiskShareImage(&g_usb_image);
  ShowPage(kUsbPageSharing, 0);
  SYSTEM_INFO("usb_image_shared", Y_LOG_STRING("path", g_usb_image_path));
}

// Take the card or image back from USB. An image is closed, writing back the
// sector it holds. The launcher then reads the card afresh, and shows the list
// again without counting down.
static __noinline void StopSharing(void) {
  const UsbDiskStats stats = UsbDiskGetStats();
  UsbDiskStopSharing();
  if (stats.image_path != NULL) {
    DiskImageClose(&g_usb_image);
  }
  SYSTEM_INFO(
      "usb_returned",
      Y_LOG_STRING("image", stats.image_path != NULL ? stats.image_path : ""),
      Y_LOG_BOOL("ejected", stats.ejected),
      Y_LOG_UINT("blocks_read", stats.blocks_read),
      Y_LOG_UINT("blocks_written", stats.blocks_written),
      Y_LOG_UINT("errors", stats.errors));
}

// Disconnect the card: at once if no computer is using it, and after asking if
// one is.
static LauncherUsbResult Disconnect(void) {
  if (ComputerHasCard()) {
    ShowPage(kUsbPageStopSharing, kStopSharingCancel);
    return kLauncherUsbStays;
  }
  StopSharing();
  return kLauncherUsbReturned;
}

// Go back a page, or to the list from the first.
static LauncherUsbResult Back(void) {
  switch (g_page) {
    case kUsbPageMenu:
      return kLauncherUsbLeft;
    case kUsbPageBrowse:
      ShowPage(kUsbPageMenu, kUSBImage);
      break;
    case kUsbPageSharing:
      return Disconnect();
    case kUsbPageStopSharing:
      ShowPage(kUsbPageSharing, 0);
      break;
  }
  return kLauncherUsbStays;
}

// Open the chosen entry of the browser: a folder to look in, or an image to
// put on USB.
static void OpenEntry(void) {
  uint8_t chosen = g_chosen;
  char path[kFSMaxPath + 1];
  uint32_t size = 0;
  switch (UIBrowserChoose(&g_browser, &chosen, path, &size)) {
    case kUIBrowserNothing:
      return;
    case kUIBrowserOpened:
      ShowPage(kUsbPageBrowse, chosen);
      return;
    case kUIBrowserChoseImage:
      break;
  }
  ShareImage(path, size);
}

static LauncherUsbResult Choose(void) {
  switch (g_page) {
    case kUsbPageMenu:
      if (g_chosen == kUSBImage) {
        g_browser.images = kUIBrowserEither;
        g_browser.current = NULL;
        for (uint8_t drive = 0; drive < kFloppyDrives; ++drive) {
          g_browser.in_drives[drive] = NULL;
        }
        ShowPage(kUsbPageBrowse, UIBrowserOpen(&g_browser, "yax86"));
        return kLauncherUsbStays;
      }
      ShareCard();
      return kLauncherUsbStays;
    case kUsbPageBrowse:
      OpenEntry();
      return kLauncherUsbStays;
    case kUsbPageSharing:
      return Disconnect();
    case kUsbPageStopSharing:
      if (g_chosen == kStopSharingConfirm) {
        StopSharing();
        return kLauncherUsbReturned;
      }
      return Back();
  }
  return kLauncherUsbStays;
}

static uint8_t PageItemCount(void) {
  switch (g_page) {
    case kUsbPageMenu:
      return kUSBItemCount;
    case kUsbPageSharing:
      return 1;
    case kUsbPageStopSharing:
      return kStopSharingItemCount;
    case kUsbPageBrowse:
      return UIBrowserCount(&g_browser);
  }
  return 0;
}

LauncherUsbResult LauncherUsbHandleKey(SouthbridgeKeyEvent event) {
  if (event.state != kSouthbridgeKeyReleased && LauncherClearProblem()) {
    g_dirty = true;
  }
  // A key held down repeats, which moves through the items as a press does.
  if (event.state == kSouthbridgeKeyReleased) {
    return kLauncherUsbStays;
  }
  if (UIPageIsMoveKey(event.key)) {
    if (UIPageMove(event.key, PageItemCount(), &g_chosen)) {
      g_dirty = true;
    }
    if (g_page == kUsbPageBrowse) {
      UIBrowserScrollTo(&g_browser, g_chosen);
    }
    return kLauncherUsbStays;
  }
  switch (event.key) {
    case kKeyCodeEnter:
    case kKeyCodeSpace:
      return Choose();
    case kKeyCodeBackspace:
      // Up a folder in the browser, until the card's root.
      if (g_page == kUsbPageBrowse && !UIBrowserAtRoot(&g_browser)) {
        ShowPage(kUsbPageBrowse, UIBrowserUp(&g_browser));
        return kLauncherUsbStays;
      }
      return Back();
    case kKeyCodeEsc:
      return Back();
    default:
      return kLauncherUsbStays;
  }
}

// ============================================================================
// Drawing
// ============================================================================

// The card while it is on USB: who has it, and how much has moved.
static void DrawSharingPage(DisplayMenu* menu) {
  const UsbDiskStats stats = UsbDiskGetStats();
  const char* state = "Waiting for the computer...";
  if (!stats.connected) {
    state = "Connect a computer by USB.";
  } else if (stats.ejected) {
    state = "Ejected by the computer.";
  } else if (stats.seen) {
    state = "In use by the computer.";
  }
  // Room for two of the largest counts, which a box cuts to its width.
  char moved[64];
  snprintf(
      moved, sizeof(moved), "Read %lu KB, written %lu KB",
      (unsigned long)(stats.blocks_read / 2),
      (unsigned long)(stats.blocks_written / 2));
  char what[kUIPageNarrowColumns];
  if (stats.image_path != NULL) {
    char name[kDiskImageNameLength + 1];
    DiskImageName(stats.image_path, name);
    snprintf(what, sizeof(what), "Sharing %s over USB.", name);
  } else {
    snprintf(what, sizeof(what), "Sharing the SD card over USB.");
  }
  const char* const lines[] = {
      what,
      state,
      moved,
      ComputerHasCard() ? "Eject it on the computer first." : "",
  };
  UIPageDrawItems(
      menu, "USB disk mode", lines, sizeof(lines) / sizeof(lines[0]),
      kSharingItems, 1, g_chosen, true);
}

void LauncherUsbDraw(uint64_t now_us) {
  if (g_page == kUsbPageSharing &&
      now_us - g_status_drawn_us >= kStatusRefreshUs) {
    g_dirty = true;
  }
  if (!g_dirty) {
    return;
  }
  DisplayMenu* menu = DisplayBeginMenu();
  if (menu == NULL) {
    return;
  }
  switch (g_page) {
    case kUsbPageMenu:
      UIPageDrawItems(
          menu, "USB disk mode", NULL, 0, kUSBItems, kUSBItemCount, g_chosen,
          true);
      break;
    case kUsbPageSharing:
      DrawSharingPage(menu);
      g_status_drawn_us = now_us;
      break;
    case kUsbPageStopSharing:
      UIPageDrawItems(
          menu, "USB disk mode",
          (const char* const[]){
              "The computer has not ejected the",
              "disk, and may not have finished",
              "writing to it.",
          },
          3, kStopSharingItems, kStopSharingItemCount, g_chosen, true);
      break;
    case kUsbPageBrowse:
      UIBrowserDraw(&g_browser, menu, g_chosen);
      break;
  }
  // Drawn the list's size, as every page from the list is, so the box never
  // changes size.
  DisplayEndMenu();
  g_dirty = false;
}
