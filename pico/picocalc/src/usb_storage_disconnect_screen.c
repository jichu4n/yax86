#include "usb_storage_disconnect_screen.h"

#include <stddef.h>
#include <stdint.h>

#include "display.h"
#include "list_box.h"
#include "ui_box.h"

typedef enum StopItem {
  kStopCancel = 0,
  kStopConfirm,
  kStopItemCount,
} StopItem;

static const UIBoxItem kStopItems[kStopItemCount] = {
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

static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  switch (ListBoxTakeEvent(&g_list_box, event, kStopItemCount)) {
    case kListBoxKeySelect:
      if (g_list_box.selected == kStopConfirm) {
        *g_confirmed = true;
      }
      return ScreenBack();
    case kListBoxKeyBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

static void Draw(Y_UNUSED uint64_t now_us) {
  DisplayMenu* menu = ListBoxBeginDraw(&g_list_box);
  if (menu == NULL) {
    return;
  }
  ListBoxDrawItems(
      menu, "Unsafe disconnect",
      (const char* const[]){
          "Not ejected by the host.",
          "Writes may still be pending.",
      },
      2, kStopItems, kStopItemCount, g_list_box.selected, true);
  ListBoxEndDraw(&g_list_box);
}

// Nothing opens over the question.
static ScreenResult Resume(void) {
  g_list_box.dirty = true;
  return ScreenStays();
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return "usb_storage_disconnect"; }

const Screen kUSBStorageDisconnectScreen = {
    .handle_key = HandleKey,
    .draw = Draw,
    .resume = Resume,
    .hint = Hint,
    .page_name = PageName,
};
