#include "vm_manager_screen.h"

#include <stdbool.h>
#include <stddef.h>

#include "display.h"
#include "vm_manager.h"
#include "ui_box.h"
#include "ui_page.h"
#include "vm_config_screen.h"
#include "vm_manager.h"
#include "vm_screen.h"

static const UIBoxItem kNewVMRow = {"New VM", true};

extern const Screen kVMManagerScreen;

// What the screens under Manage VMs share.
static VMManager g_vm_manager;
// The item chosen, and whether the page is out of date.
static UIPage g_page;
// The row at the top of the list.
static uint8_t g_top;

// The rows: a VM each, then New VM, after a line if there are any VMs.
static uint8_t RowCount(void) {
  return (uint8_t)(g_vm_manager.vms->count +
                   (g_vm_manager.vms->count > 0 ? 2 : 1));
}

// Keep the chosen item among the rows.
static void ScrollToChosen(void) {
  uint8_t row = g_page.chosen;
  if (g_vm_manager.vms->count > 0 && g_page.chosen >= g_vm_manager.vms->count) {
    ++row;
  }
  UIPageScrollTo(row, &g_top);
}

// Show the list from its top, with the item at index chosen.
static void ShowChosen(uint8_t index) {
  g_top = 0;
  g_page.chosen = index;
  g_page.dirty = true;
  ScrollToChosen();
}

ScreenResult VMManagerScreenOpen(
    VMList* vms, Settings* settings, uint8_t* copy_buffer,
    uint32_t copy_buffer_size) {
  g_vm_manager = (VMManager){
      .vms = vms,
      .settings = settings,
      .copy_buffer = copy_buffer,
      .copy_buffer_size = copy_buffer_size,
  };
  ShowChosen(0);
  return ScreenOpen(&kVMManagerScreen);
}

static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  const uint8_t count = (uint8_t)(g_vm_manager.vms->count + 1);
  switch (UIPageTakeEvent(&g_page, event, count)) {
    case kUIPageKeyMoved:
      ScrollToChosen();
      return ScreenStays();
    case kUIPageKeyChoose:
      if (g_page.chosen == g_vm_manager.vms->count) {
        return VMConfigScreenOpenNew(&g_vm_manager);
      }
      return VMScreenOpen(&g_vm_manager, g_page.chosen);
    case kUIPageKeyBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

// Back from a VM's page or New VM, with the list shown from its top again
// and the item that opened them chosen. After the card changed, read the
// VMs afresh, and choose the VM the change named instead, or the first if
// it is not there, showing its page too if it said so. The settings are not
// read again: what changes them, a rename or delete of the default VM,
// updates them as it saves them.
static __noinline ScreenResult Resume(void) {
  if (!g_vm_manager.changed) {
    ShowChosen(g_page.chosen);
    return ScreenStays();
  }
  g_vm_manager.changed = false;
  VMListRead(g_vm_manager.vms);
  const uint8_t index = VMListFind(g_vm_manager.vms, g_vm_manager.chosen);
  const bool found = index < g_vm_manager.vms->count;
  ShowChosen(found ? index : 0);
  if (g_vm_manager.show_vm && found) {
    return VMScreenOpen(&g_vm_manager, index);
  }
  return ScreenStays();
}

// The VMs in yax86/vm, a row each, then New VM after a line if there are
// any, with arrows on the border where there are more above or below.
static void Draw(Y_UNUSED uint64_t now_us) {
  DisplayMenu* menu = UIPageBeginDraw(&g_page);
  if (menu == NULL) {
    return;
  }
  const VMList* vms = g_vm_manager.vms;
  UIBoxPlace(menu, kUIPageNarrowColumns, kUIPageListRows + 2);
  UIBoxEdgeRow(&menu->text[0], menu->columns, true, "Manage VMs");
  for (uint8_t i = 0; i < kUIPageListRows; ++i) {
    DisplayTextRow* row = &menu->text[1 + i];
    const uint8_t index = (uint8_t)(g_top + i);
    if (index >= RowCount()) {
      UIBoxBlankRow(row, menu->columns);
    } else if (index < vms->count) {
      const UIBoxItem item = {vms->vms[index].name, true};
      UIBoxMenuItemRow(row, menu->columns, &item, index == g_page.chosen);
    } else if (vms->count > 0 && index == vms->count) {
      UIBoxSeparatorRow(row, menu->columns);
    } else {
      UIBoxMenuItemRow(
          row, menu->columns, &kNewVMRow, g_page.chosen == vms->count);
    }
  }
  UIBoxEdgeRow(&menu->text[kUIPageListRows + 1], menu->columns, false, NULL);
  UIPageDrawScrollArrows(menu, g_top, RowCount());
  UIPageEndDraw(&g_page);
}

static const char* Hint(void) { return kUIPageHint; }

static const char* PageName(void) { return "vm_manager"; }

const Screen kVMManagerScreen = {
    .handle_key = HandleKey,
    .draw = Draw,
    .resume = Resume,
    .hint = Hint,
    .page_name = PageName,
};
