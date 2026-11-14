#include "usb_storage_active_screen.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "display.h"
#include "list_box.h"
#include "runtime_log.h"
#include "sdcard.h"
#include "ui_box.h"
#include "usb_storage.h"
#include "usb_storage_disconnect_screen.h"

enum {
  kStatusRefreshUs = 1000 * 1000,
  kBytesPerBlock = 512,
  kBytesPerKB = 1024,
  kBytesPerMB = 1024 * 1024,
  kBytesPerGB = 1024 * 1024 * 1024,
};

static const UIBoxItem kSharingItems[] = {{"Disconnect", false}};

extern const Screen kUSBStorageActiveScreen;

// The list box: the item selected, and whether it is out of date. And when
// it was last drawn.
static ListBox g_list_box;
static uint64_t g_drawn_us;
// The parent's: the image exported, NULL for the card, and where to say
// exporting has ended.
static DiskImage* g_image;
static bool* g_stopped;
// Whether Disconnect anyway was chosen, which the question sets.
static bool g_confirmed;

ScreenResult USBStorageActiveScreenOpen(DiskImage* image, bool* stopped) {
  g_image = image;
  g_stopped = stopped;
  g_list_box.selected = 0;
  g_list_box.dirty = true;
  return ScreenOpen(&kUSBStorageActiveScreen);
}

// Whether disconnecting would take the volume from a host that has it
// mounted.
static bool HostHasVolume(void) {
  const UsbStorageStats stats = UsbStorageGetStats();
  return stats.connected && stats.seen && !stats.ejected;
}

// Take the card or image back from the host. An image is closed, writing
// back the sector it holds.
static ScreenResult StopSharing(void) {
  const UsbStorageStats stats = UsbStorageGetStats();
  UsbStorageStopExporting();
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

// Disconnect: at once if no host has the volume mounted, and after asking if
// one has.
static ScreenResult Disconnect(void) {
  if (HostHasVolume()) {
    g_confirmed = false;
    return USBStorageDisconnectScreenOpen(&g_confirmed);
  }
  return StopSharing();
}

static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  switch (ListBoxHandleKeyEvent(&g_list_box, event, 1)) {
    case kListBoxActionSelect:
    case kListBoxActionBack:
      return Disconnect();
    default:
      return ScreenStays();
  }
}

// Back from the question: disconnect anyway, or keep exporting.
static ScreenResult Resume(void) {
  if (g_confirmed) {
    g_confirmed = false;
    return StopSharing();
  }
  g_list_box.dirty = true;
  return ScreenStays();
}

// size bytes in the largest unit it is at least one of, binary as the
// counts are: KB, MB, or GB to a tenth.
static void FormatSize(uint64_t size, char* text, size_t length) {
  if (size >= kBytesPerGB) {
    snprintf(
        text, length, "%lu.%lu GB", (unsigned long)(size / kBytesPerGB),
        (unsigned long)(size % kBytesPerGB * 10 / kBytesPerGB));
  } else if (size >= kBytesPerMB) {
    snprintf(text, length, "%lu MB", (unsigned long)(size / kBytesPerMB));
  } else {
    snprintf(text, length, "%lu KB", (unsigned long)(size / kBytesPerKB));
  }
}

// What is exported and its size, the host's state, and how much has moved.
static void Draw(uint64_t now_us) {
  if (now_us - g_drawn_us >= kStatusRefreshUs) {
    g_list_box.dirty = true;
  }
  DisplayMenu* menu = ListBoxBeginDraw(&g_list_box);
  if (menu == NULL) {
    return;
  }
  const UsbStorageStats stats = UsbStorageGetStats();
  const char* state = "Waiting for the host...";
  if (!stats.connected) {
    state = "No host connected.";
  } else if (stats.ejected) {
    state = "Ejected by the host.";
  } else if (stats.seen) {
    state = "Mounted by the host.";
  }
  // Room for two of the largest counts, which a box cuts to its width.
  char moved[64];
  snprintf(
      moved, sizeof(moved), "Read %lu KB, written %lu KB",
      (unsigned long)(stats.blocks_read / 2),
      (unsigned long)(stats.blocks_written / 2));
  // Room for the longest name and size, which a box cuts to its width.
  char what[64];
  char size[16];
  if (stats.image_path != NULL) {
    char name[kDiskImageNameLength + 1];
    DiskImageName(stats.image_path, name);
    FormatSize(g_image->size, size, sizeof(size));
    snprintf(what, sizeof(what), "Exporting %s (%s).", name, size);
  } else {
    FormatSize(
        (uint64_t)SdcardGetBlockCount() * kBytesPerBlock, size, sizeof(size));
    snprintf(what, sizeof(what), "Exporting the SD card (%s).", size);
  }
  const char* const lines[] = {
      what,
      state,
      moved,
      HostHasVolume() ? "Eject it on the host first." : "",
  };
  ListBoxDrawItems(
      menu, "USB mass storage", lines, sizeof(lines) / sizeof(lines[0]),
      kSharingItems, 1, g_list_box.selected, true);
  g_drawn_us = now_us;
  ListBoxEndDraw(&g_list_box);
}

static const char* Hint(void) { return "Enter=Disconnect"; }

static const char* PageName(void) { return "usb_storage_active"; }

const Screen kUSBStorageActiveScreen = {
    .handle_key = HandleKey,
    .draw = Draw,
    .resume = Resume,
    .hint = Hint,
    .page_name = PageName,
};
