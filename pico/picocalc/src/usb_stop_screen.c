#include "usb_stop_screen.h"

#include <stddef.h>
#include <stdint.h>

#include "display.h"
#include "ui_box.h"
#include "ui_page.h"
#include "ui_status.h"

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

static uint8_t g_chosen;
// Whether what is on the panel is out of date.
static bool g_dirty;
static bool* g_confirmed;

ScreenResult USBStopScreenOpen(bool* confirmed) {
  g_confirmed = confirmed;
  g_chosen = kStopCancel;
  g_dirty = true;
  return ScreenOpen(&kUSBStopScreen);
}

static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  // A key held down repeats, which moves through the items as a press does.
  if (event.state == kSouthbridgeKeyReleased) {
    return ScreenStays();
  }
  UIStatusClearProblem();
  if (UIPageIsMoveKey(event.key)) {
    if (UIPageMove(event.key, kStopItemCount, &g_chosen)) {
      g_dirty = true;
    }
    return ScreenStays();
  }
  switch (event.key) {
    case kKeyCodeEnter:
    case kKeyCodeSpace:
      if (g_chosen == kStopConfirm) {
        *g_confirmed = true;
      }
      return ScreenBack();
    case kKeyCodeBackspace:
    case kKeyCodeEsc:
      return ScreenBack();
    default:
      return ScreenStays();
  }
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
      menu, "USB disk mode",
      (const char* const[]){
          "The computer has not ejected the",
          "disk, and may not have finished",
          "writing to it.",
      },
      3, kStopItems, kStopItemCount, g_chosen, true);
  DisplayEndMenu();
  g_dirty = false;
}

// Nothing opens over the question.
static ScreenResult Resume(void) {
  g_dirty = true;
  return ScreenStays();
}

static const char* Hint(void) {
  return "\x18\x19=Move  Enter=Select  Esc=Back";
}

static const char* PageName(void) { return "usb_stop"; }

const Screen kUSBStopScreen = {
    .handle_key = HandleKey,
    .draw = Draw,
    .resume = Resume,
    .hint = Hint,
    .page_name = PageName,
};
