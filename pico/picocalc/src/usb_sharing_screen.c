#include "usb_sharing_screen.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "display.h"
#include "runtime_log.h"
#include "status_bar.h"
#include "ui_box.h"
#include "ui_page.h"
#include "usb_disk.h"
#include "usb_stop_screen.h"

enum {
  kStatusRefreshUs = 1000 * 1000,
};

static const UIBoxItem kSharingItems[] = {{"Disconnect", false}};

extern const Screen kSharingScreen;

static uint8_t g_chosen;
// Whether what is on the panel is out of date, and when it was last drawn.
static bool g_dirty;
static uint64_t g_drawn_us;
// The parent's: the image on USB, NULL for the card, and where to say
// sharing has ended.
static DiskImage* g_image;
static bool* g_stopped;
// Whether Disconnect anyway was chosen, which the question sets.
static bool g_confirmed;

ScreenResult SharingScreenOpen(DiskImage* image, bool* stopped) {
  g_image = image;
  g_stopped = stopped;
  g_chosen = 0;
  g_dirty = true;
  return ScreenOpen(&kSharingScreen);
}

// Whether disconnecting would take the card from a computer still using it.
static bool ComputerHasCard(void) {
  const UsbDiskStats stats = UsbDiskGetStats();
  return stats.connected && stats.seen && !stats.ejected;
}

// Take the card or image back from USB. An image is closed, writing back
// the sector it holds.
static ScreenResult StopSharing(void) {
  const UsbDiskStats stats = UsbDiskGetStats();
  UsbDiskStopSharing();
  if (stats.image_path != NULL) {
    DiskImageClose(g_image);
  }
  SYSTEM_INFO(
      "usb_returned",
      Y_LOG_STRING("image", stats.image_path != NULL ? stats.image_path : ""),
      Y_LOG_BOOL("ejected", stats.ejected),
      Y_LOG_UINT("blocks_read", stats.blocks_read),
      Y_LOG_UINT("blocks_written", stats.blocks_written),
      Y_LOG_UINT("errors", stats.errors));
  *g_stopped = true;
  return ScreenBack();
}

// Disconnect: at once if no computer is using the card, and after asking if
// one is.
static ScreenResult Disconnect(void) {
  if (ComputerHasCard()) {
    g_confirmed = false;
    return USBStopScreenOpen(&g_confirmed);
  }
  return StopSharing();
}

static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  // A key held down repeats, as a press does.
  if (event.state == kSouthbridgeKeyReleased) {
    return ScreenStays();
  }
  StatusBarClearProblem();
  if (UIPageIsMoveKey(event.key)) {
    if (UIPageMove(event.key, 1, &g_chosen)) {
      g_dirty = true;
    }
    return ScreenStays();
  }
  switch (event.key) {
    case kKeyCodeEnter:
    case kKeyCodeSpace:
    case kKeyCodeBackspace:
    case kKeyCodeEsc:
      return Disconnect();
    default:
      return ScreenStays();
  }
}

// Back from the question: disconnect anyway, or keep sharing.
static ScreenResult Resume(void) {
  if (g_confirmed) {
    g_confirmed = false;
    return StopSharing();
  }
  g_dirty = true;
  return ScreenStays();
}

// Who has the card, and how much has moved.
static void Draw(uint64_t now_us) {
  if (now_us - g_drawn_us >= kStatusRefreshUs) {
    g_dirty = true;
  }
  if (!g_dirty) {
    return;
  }
  DisplayMenu* menu = DisplayBeginMenu();
  if (menu == NULL) {
    return;
  }
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
  DisplayEndMenu();
  g_drawn_us = now_us;
  g_dirty = false;
}

static const char* Hint(void) { return "Enter=Disconnect"; }

static const char* PageName(void) { return "usb_sharing"; }

const Screen kSharingScreen = {
    .handle_key = HandleKey,
    .draw = Draw,
    .resume = Resume,
    .hint = Hint,
    .page_name = PageName,
};
