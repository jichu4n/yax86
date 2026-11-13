#include "usb_stop_screen.h"

#include <stddef.h>
#include <stdint.h>

#include "display.h"
#include "ui_box.h"
#include "ui_page.h"

typedef enum StopItem {
  kStopCancel = 0,
  kStopConfirm,
  kStopItemCount,
} StopItem;

static const UIBoxItem kStopItems[kStopItemCount] = {
    [kStopCancel] = {"Cancel", false},
    [kStopConfirm] = {"Disconnect anyway", false},
};

extern const Screen kUSBStopScreen;

// The item chosen, and whether the page is out of date.
static UIPage g_page;
static bool* g_confirmed;

ScreenResult USBStopScreenOpen(bool* confirmed) {
  g_confirmed = confirmed;
  g_page.chosen = kStopCancel;
  g_page.dirty = true;
  return ScreenOpen(&kUSBStopScreen);
}

static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  switch (UIPageTakeEvent(&g_page, event, kStopItemCount)) {
    case kUIPageKeyChoose:
      if (g_page.chosen == kStopConfirm) {
        *g_confirmed = true;
      }
      return ScreenBack();
    case kUIPageKeyBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

static void Draw(Y_UNUSED uint64_t now_us) {
  DisplayMenu* menu = UIPageBeginDraw(&g_page);
  if (menu == NULL) {
    return;
  }
  UIPageDrawItems(
      menu, "USB disk mode",
      (const char* const[]){
          "The computer has not ejected the",
          "disk, and may not have finished",
          "writing to it.",
      },
      3, kStopItems, kStopItemCount, g_page.chosen, true);
  UIPageEndDraw(&g_page);
}

// Nothing opens over the question.
static ScreenResult Resume(void) {
  g_page.dirty = true;
  return ScreenStays();
}

static const char* Hint(void) { return kUIPageHint; }

static const char* PageName(void) { return "usb_stop"; }

const Screen kUSBStopScreen = {
    .handle_key = HandleKey,
    .draw = Draw,
    .resume = Resume,
    .hint = Hint,
    .page_name = PageName,
};
