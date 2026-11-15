#include "vm_video_adapter_screen.h"

#include <stddef.h>
#include <stdint.h>

#include "box.h"
#include "display.h"
#include "list_box.h"
#include "vm_config.h"

// Listed in menu order: CGA first, the default, then MDA.
static const YVideoAdapter kAdapters[Y_VIDEO_NUM_ADAPTERS] = {
    Y_VIDEO_ADAPTER_CGA,
    Y_VIDEO_ADAPTER_MDA,
};

extern const Screen kVMVideoAdapterScreen;

// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;
// The form's choice for the video adapter.
static YVideoAdapter* g_chosen;

ScreenResult VMVideoAdapterScreenOpen(YVideoAdapter* chosen) {
  g_chosen = chosen;
  g_list_box.selected = 0;
  for (uint8_t i = 0; i < Y_VIDEO_NUM_ADAPTERS; ++i) {
    if (kAdapters[i] == *chosen) {
      g_list_box.selected = i;
      break;
    }
  }
  return ScreenOpen(&kVMVideoAdapterScreen);
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxHandleKeyEvent(&g_list_box, event, Y_VIDEO_NUM_ADAPTERS)) {
    case kListBoxActionSelect:
      *g_chosen = kAdapters[g_list_box.selected];
      return ScreenBack();
    case kListBoxActionBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

static void OnDraw(DisplayBox* box) {
  BoxItem items[Y_VIDEO_NUM_ADAPTERS];
  for (uint8_t i = 0; i < Y_VIDEO_NUM_ADAPTERS; ++i) {
    items[i] = (BoxItem){VMConfigVideoAdapterLabel(kAdapters[i]), false};
  }
  ListBoxDrawItems(
      box, "Video adapter", NULL, 0, items, Y_VIDEO_NUM_ADAPTERS,
      g_list_box.selected, true);
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return "vm_video_adapter"; }

const Screen kVMVideoAdapterScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .hint = Hint,
    .page_name = PageName,
    .is_dirty = &g_list_box.is_dirty,
};
