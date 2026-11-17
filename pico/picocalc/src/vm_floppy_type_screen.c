#include "vm_floppy_type_screen.h"

#include <stddef.h>
#include <stdint.h>

#include "box.h"
#include "display.h"
#include "list_box.h"
#include "vm_config.h"

enum {
  kNumDriveTypes = 4,
};

static const YFDCDriveType kDriveTypes[kNumDriveTypes] = {
    Y_FDC_DRIVE_TYPE_360KB,
    Y_FDC_DRIVE_TYPE_720KB,
    Y_FDC_DRIVE_TYPE_1200KB,
    Y_FDC_DRIVE_TYPE_1440KB,
};

extern const Screen kVMFloppyTypeScreen;

// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;
// The drive's choice for its mechanism type.
static YFDCDriveType* g_chosen;

ScreenResult VMFloppyTypeScreenOpen(YFDCDriveType* chosen) {
  g_chosen = chosen;
  g_list_box.selected = 0;
  for (uint8_t i = 0; i < kNumDriveTypes; ++i) {
    if (kDriveTypes[i] == *chosen) {
      g_list_box.selected = i;
      break;
    }
  }
  return ScreenOpen(&kVMFloppyTypeScreen);
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxHandleKeyEvent(&g_list_box, event, kNumDriveTypes)) {
    case kListBoxActionSelect:
      *g_chosen = kDriveTypes[g_list_box.selected];
      return ScreenBack();
    case kListBoxActionBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

static void OnDraw(DisplayBox* box) {
  BoxItem items[kNumDriveTypes];
  for (uint8_t i = 0; i < kNumDriveTypes; ++i) {
    items[i] = (BoxItem){VMConfigFloppyDriveTypeLabel(kDriveTypes[i]), false};
  }
  ListBoxDrawItems(
      box, "Drive type", NULL, 0, items, kNumDriveTypes, g_list_box.selected,
      true);
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return "vm_floppy_type"; }

const Screen kVMFloppyTypeScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .hint = Hint,
    .page_name = PageName,
    .is_dirty = &g_list_box.is_dirty,
};
