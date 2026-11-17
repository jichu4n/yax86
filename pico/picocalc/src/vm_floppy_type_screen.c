#include "vm_floppy_type_screen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "box.h"
#include "display.h"
#include "list_box.h"
#include "vm_config.h"

enum {
  kNumDriveTypes = Y_FDC_NUM_DRIVE_TYPES - 1,
};

// Listed in menu order: 360KB first, the default, then the other mechanisms in
// ascending capacity.
static const YFDCDriveType kDriveTypes[kNumDriveTypes] = {
    Y_FDC_DRIVE_TYPE_360KB,
    Y_FDC_DRIVE_TYPE_720KB,
    Y_FDC_DRIVE_TYPE_1200KB,
    Y_FDC_DRIVE_TYPE_1440KB,
};

extern const Screen kVMFloppyTypeScreen;

// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;
// The drive's choice for its mechanism type, and whether it is there at all:
// NULL for a drive that always is, which has no Disabled row.
static YFDCDriveType* g_chosen;
static bool* g_is_present;

// The rows before the drive types: Disabled, for a drive that can be left out.
static uint8_t NumLeadingRows(void) { return g_is_present != NULL ? 1 : 0; }

ScreenResult VMFloppyTypeScreenOpen(YFDCDriveType* chosen, bool* is_present) {
  g_chosen = chosen;
  g_is_present = is_present;
  g_list_box.selected = 0;
  if (is_present == NULL || *is_present) {
    for (uint8_t i = 0; i < kNumDriveTypes; ++i) {
      if (kDriveTypes[i] == *chosen) {
        g_list_box.selected = NumLeadingRows() + i;
        break;
      }
    }
  }
  return ScreenOpen(&kVMFloppyTypeScreen);
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  const uint8_t num_leading_rows = NumLeadingRows();
  switch (ListBoxHandleKeyEvent(
      &g_list_box, event, num_leading_rows + kNumDriveTypes)) {
    case kListBoxActionSelect:
      if (g_list_box.selected < num_leading_rows) {
        *g_is_present = false;
      } else {
        *g_chosen = kDriveTypes[g_list_box.selected - num_leading_rows];
        if (g_is_present != NULL) {
          *g_is_present = true;
        }
      }
      return ScreenBack();
    case kListBoxActionBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

static void OnDraw(DisplayBox* box) {
  BoxItem items[1 + kNumDriveTypes];
  uint8_t num_items = 0;
  if (g_is_present != NULL) {
    items[num_items++] = (BoxItem){"Disabled", false};
  }
  for (uint8_t i = 0; i < kNumDriveTypes; ++i) {
    items[num_items++] =
        (BoxItem){VMConfigFloppyDriveTypeLabel(kDriveTypes[i]), false};
  }
  ListBoxDrawItems(
      box, "Drive type", NULL, 0, items, num_items, g_list_box.selected, true);
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
