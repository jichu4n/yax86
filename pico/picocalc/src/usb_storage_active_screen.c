#include "usb_storage_active_screen.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "box.h"
#include "display.h"
#include "list_box.h"
#include "pico/time.h"
#include "runtime_log.h"
#include "sdcard.h"
#include "usb_storage.h"
#include "usb_storage_disconnect_screen.h"

enum {
  kStatusRefreshUs = 1000 * 1000,
  kBytesPerBlock = 512,
  kBytesPerKB = 1024,
  kBytesPerMB = 1024 * 1024,
  kBytesPerGB = 1024 * 1024 * 1024,
};

static const BoxItem kSharingItems[] = {{"Disconnect", false}};

extern const Screen kUSBStorageActiveScreen;

// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;
// When the page was last marked out of date, in microseconds since boot. It
// is marked again kStatusRefreshUs later, so that the host's state and the
// counts stay current.
static uint64_t g_refreshed_us;
// The parent's: the image exported, NULL for the card, and where to say
// exporting has ended.
static DiskImage* g_image;
static bool* g_did_stop;
// Whether Disconnect anyway was chosen, which the question sets.
static bool g_is_confirmed;

ScreenResult USBStorageActiveScreenOpen(DiskImage* image, bool* did_stop) {
  g_image = image;
  g_did_stop = did_stop;
  g_list_box.selected = 0;
  g_refreshed_us = time_us_64();
  return ScreenOpen(&kUSBStorageActiveScreen);
}

// Whether disconnecting would take the volume from a host that has it
// mounted.
static bool HostHasVolume(void) {
  const UsbStorageStats stats = UsbStorageGetStats();
  return stats.is_connected && stats.was_seen && !stats.was_ejected;
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
      Y_LOG_BOOL("was_ejected", stats.was_ejected),
      Y_LOG_UINT("num_blocks_read", stats.num_blocks_read),
      Y_LOG_UINT("num_blocks_written", stats.num_blocks_written),
      Y_LOG_UINT("num_errors", stats.num_errors));
  *g_did_stop = true;
  return ScreenBack();
}

// Disconnect: at once if no host has the volume mounted, and after asking if
// one has.
static ScreenResult Disconnect(void) {
  if (HostHasVolume()) {
    g_is_confirmed = false;
    return USBStorageDisconnectScreenOpen(&g_is_confirmed);
  }
  return StopSharing();
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxHandleKeyEvent(&g_list_box, event, 1)) {
    case kListBoxActionSelect:
    case kListBoxActionBack:
      return Disconnect();
    default:
      return ScreenStays();
  }
}

// Back from the question: disconnect anyway, or keep exporting.
static ScreenResult OnResume(void) {
  if (g_is_confirmed) {
    g_is_confirmed = false;
    return StopSharing();
  }
  return ScreenStays();
}

// Mark the page out of date once a second, so that it is drawn again with
// the host's state and the counts as they are now.
static ScreenResult OnTick(void) {
  const uint64_t now_us = time_us_64();
  if (now_us - g_refreshed_us >= kStatusRefreshUs) {
    g_refreshed_us = now_us;
    g_list_box.is_dirty = true;
  }
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
static void OnDraw(DisplayBox* box) {
  const UsbStorageStats stats = UsbStorageGetStats();
  const char* state = "Waiting for the host...";
  if (!stats.is_connected) {
    state = "No host connected.";
  } else if (stats.was_ejected) {
    state = "Ejected by the host.";
  } else if (stats.was_seen) {
    state = "Mounted by the host.";
  }
  // Room for two of the largest counts, which a box cuts to its width.
  char moved[64];
  snprintf(
      moved, sizeof(moved), "Read %lu KB, written %lu KB",
      (unsigned long)(stats.num_blocks_read / 2),
      (unsigned long)(stats.num_blocks_written / 2));
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
        (uint64_t)SdcardGetNumBlocks() * kBytesPerBlock, size, sizeof(size));
    snprintf(what, sizeof(what), "Exporting the SD card (%s).", size);
  }
  const char* const lines[] = {
      what,
      state,
      moved,
      HostHasVolume() ? "Eject it on the host first." : "",
  };
  ListBoxDrawItems(
      box, "USB mass storage", lines, sizeof(lines) / sizeof(lines[0]),
      kSharingItems, 1, g_list_box.selected, true);
}

static const char* Hint(void) { return "Enter=Disconnect"; }

static const char* PageName(void) { return "usb_storage_active"; }

const Screen kUSBStorageActiveScreen = {
    .on_key = OnKey,
    .on_tick = OnTick,
    .on_draw = OnDraw,
    .on_resume = OnResume,
    .hint = Hint,
    .page_name = PageName,
    .is_dirty = &g_list_box.is_dirty,
};
