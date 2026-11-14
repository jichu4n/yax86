#include "usb_storage_disconnect_screen.h"

#include <stddef.h>
#include <stdint.h>

#include "box.h"
#include "display.h"
#include "list_box.h"

typedef enum StopItem {
  kStopCancel = 0,
  kStopConfirm,
  kNumStopItems,
} StopItem;

static const BoxItem kStopItems[kNumStopItems] = {
    [kStopCancel] = {"Cancel", false},
    [kStopConfirm] = {"Disconnect anyway", false},
};

extern const Screen kUSBStorageDisconnectScreen;

// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;
static bool* g_is_confirmed;

ScreenResult USBStorageDisconnectScreenOpen(bool* is_confirmed) {
  g_is_confirmed = is_confirmed;
  g_list_box.selected = kStopCancel;
  return ScreenOpen(&kUSBStorageDisconnectScreen);
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxHandleKeyEvent(&g_list_box, event, kNumStopItems)) {
    case kListBoxActionSelect:
      if (g_list_box.selected == kStopConfirm) {
        *g_is_confirmed = true;
      }
      return ScreenBack();
    case kListBoxActionBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

static void OnDraw(DisplayBox* box) {
  ListBoxDrawItems(
      box, "Unsafe disconnect",
      (const char* const[]){
          "Not ejected by the host.",
          "Writes may still be pending.",
      },
      2, kStopItems, kNumStopItems, g_list_box.selected, true);
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return "usb_storage_disconnect"; }

const Screen kUSBStorageDisconnectScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .hint = Hint,
    .page_name = PageName,
    .is_dirty = &g_list_box.is_dirty,
};
