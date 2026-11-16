#include "vm_memory_screen.h"

#include <stddef.h>
#include <stdint.h>

#include "box.h"
#include "display.h"
#include "list_box.h"

typedef struct MemoryPreset {
  const char* label;
  uint16_t memory_kb;
} MemoryPreset;

static const MemoryPreset kPresets[] = {
    {"128K", 128},
    {"256K", 256},
    {"512K", 512},
    {"640K", 640},
};

enum {
  kNumPresets = sizeof(kPresets) / sizeof(kPresets[0]),
};

extern const Screen kVMMemoryScreen;

// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;
// The form's choice for the memory size, in KB.
static uint16_t* g_chosen;

// Index of the preset matching memory_kb, or the closest preset for a
// hand-edited value in vm.cfg.
static uint8_t PresetForMemoryKB(uint16_t memory_kb) {
  uint8_t best = 0;
  uint32_t best_distance = UINT32_MAX;
  for (uint8_t i = 0; i < kNumPresets; ++i) {
    const uint32_t distance =
        kPresets[i].memory_kb > memory_kb
            ? (uint32_t)(kPresets[i].memory_kb - memory_kb)
            : (uint32_t)(memory_kb - kPresets[i].memory_kb);
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
      *g_chosen = kPresets[g_list_box.selected].memory_kb;
      return ScreenBack();
    case kListBoxActionBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

static void OnDraw(DisplayBox* box) {
  BoxItem items[kNumPresets];
  for (uint8_t i = 0; i < kNumPresets; ++i) {
    items[i] = (BoxItem){kPresets[i].label, false};
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
