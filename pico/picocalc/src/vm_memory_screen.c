#include "vm_memory_screen.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "box.h"
#include "display.h"
#include "list_box.h"
#include "yax86_pico.h"

static const uint16_t kPresets[] = {
    128, 256, 384, 512, 640,
};

enum {
  kNumPresets = sizeof(kPresets) / sizeof(kPresets[0]),
  kLabelSize = 24,
};

extern const Screen kVMMemoryScreen;

// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;
// The form's choice for the memory size, in KB.
static uint16_t* g_chosen;

void VMMemoryFormat(uint16_t memory_kb, char* buffer, size_t size) {
  snprintf(
      buffer, size, "%uK%s", (unsigned)memory_kb,
      (uint32_t)memory_kb * 1024 > kPicoGuestSRAMSize ? " (paged)" : "");
}

// Index of the preset matching memory_kb, or the closest preset for a
// hand-edited value in vm.cfg.
static uint8_t PresetForMemoryKB(uint16_t memory_kb) {
  uint8_t best = 0;
  uint32_t best_distance = UINT32_MAX;
  for (uint8_t i = 0; i < kNumPresets; ++i) {
    const uint32_t distance = kPresets[i] > memory_kb
                                  ? (uint32_t)(kPresets[i] - memory_kb)
                                  : (uint32_t)(memory_kb - kPresets[i]);
    if (distance < best_distance) {
      best_distance = distance;
      best = i;
    }
  }
  return best;
}

ScreenResult VMMemoryScreenOpen(uint16_t* chosen) {
  g_chosen = chosen;
  g_list_box.selected = PresetForMemoryKB(*chosen);
  return ScreenOpen(&kVMMemoryScreen);
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxHandleKeyEvent(&g_list_box, event, kNumPresets)) {
    case kListBoxActionSelect:
      *g_chosen = kPresets[g_list_box.selected];
      return ScreenBack();
    case kListBoxActionBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

static void OnDraw(DisplayBox* box) {
  char labels[kNumPresets][kLabelSize];
  BoxItem items[kNumPresets];
  for (uint8_t i = 0; i < kNumPresets; ++i) {
    VMMemoryFormat(kPresets[i], labels[i], sizeof(labels[i]));
    items[i] = (BoxItem){labels[i], false};
  }
  ListBoxDrawItems(
      box, "Memory", NULL, 0, items, kNumPresets, g_list_box.selected, true);
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return "vm_memory"; }

const Screen kVMMemoryScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .hint = Hint,
    .page_name = PageName,
    .is_dirty = &g_list_box.is_dirty,
};
