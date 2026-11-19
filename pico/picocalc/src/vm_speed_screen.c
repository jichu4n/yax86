#include "vm_speed_screen.h"

#include <stddef.h>
#include <stdint.h>

#include "box.h"
#include "display.h"
#include "list_box.h"
#include "vm_config.h"

// Listed in menu order: 4.77MHz first, the default, then Max.
static const VMSpeed kSpeeds[kNumVMSpeeds] = {
    kVMSpeed477MHz,
    kVMSpeedMax,
};

extern const Screen kVMSpeedScreen;

// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;
// Where the chosen speed is written on selection.
static VMSpeed* g_chosen;

ScreenResult VMSpeedScreenOpen(VMSpeed* chosen) {
  g_chosen = chosen;
  g_list_box.selected = 0;
  for (uint8_t i = 0; i < kNumVMSpeeds; ++i) {
    if (kSpeeds[i] == *chosen) {
      g_list_box.selected = i;
      break;
    }
  }
  return ScreenOpen(&kVMSpeedScreen);
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxHandleKeyEvent(&g_list_box, event, kNumVMSpeeds)) {
    case kListBoxActionSelect:
      *g_chosen = kSpeeds[g_list_box.selected];
      return ScreenBack();
    case kListBoxActionBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

static void OnDraw(DisplayBox* box) {
  BoxItem items[kNumVMSpeeds];
  for (uint8_t i = 0; i < kNumVMSpeeds; ++i) {
    items[i] = (BoxItem){VMConfigSpeedLabel(kSpeeds[i]), false};
  }
  ListBoxDrawItems(
      box, "Speed", NULL, 0, items, kNumVMSpeeds, g_list_box.selected, true);
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return "vm_speed"; }

const Screen kVMSpeedScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .hint = Hint,
    .page_name = PageName,
    .is_dirty = &g_list_box.is_dirty,
};
