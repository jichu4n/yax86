#include "usb_storage_disconnect_screen.h"

#include <stddef.h>
#include <stdint.h>

#include "box.h"
#include "display.h"
#include "list_box.h"

typedef enum StopItem {
  kStopCancel = 0,
  kStopConfirm,
  kStopItemCount,
} StopItem;

static const BoxItem kStopItems[kStopItemCount] = {
    [kStopCancel] = {"Cancel", false},
    [kStopConfirm] = {"Disconnect anyway", false},
};

extern const Screen kUSBStorageDisconnectScreen;

// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;
static bool* g_confirmed;

ScreenResult USBStorageDisconnectScreenOpen(bool* confirmed) {
  g_confirmed = confirmed;
  g_list_box.selected = kStopCancel;
  g_list_box.dirty = true;
  return ScreenOpen(&kUSBStorageDisconnectScreen);
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxHandleKeyEvent(&g_list_box, event, kStopItemCount)) {
    case kListBoxActionSelect:
      if (g_list_box.selected == kStopConfirm) {
        *g_confirmed = true;
      }
      return ScreenBack();
    case kListBoxActionBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

static bool OnDraw(DisplayBox* box, Y_UNUSED uint64_t now_us) {
  if (!ListBoxTakeDirty(&g_list_box)) {
    return false;
  }
  ListBoxDrawItems(
      box, "Unsafe disconnect",
      (const char* const[]){
          "Not ejected by the host.",
          "Writes may still be pending.",
      },
      2, kStopItems, kStopItemCount, g_list_box.selected, true);
  return true;
}

// Nothing opens over the question.
static ScreenResult OnResume(void) {
  g_list_box.dirty = true;
  return ScreenStays();
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return "usb_storage_disconnect"; }

const Screen kUSBStorageDisconnectScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .on_resume = OnResume,
    .hint = Hint,
    .page_name = PageName,
};
