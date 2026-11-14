#include "vm_manager_screen.h"

#include <stdbool.h>
#include <stddef.h>

#include "display.h"
#include "list_box.h"
#include "menu_box.h"
#include "vm_config_screen.h"
#include "vm_manager.h"
#include "vm_screen.h"

static const MenuBoxItem kNewVMRow = {"New VM", true};

extern const Screen kVMManagerScreen;

// What the screens under Manage VMs share.
static VMManager g_vm_manager;
// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;
// The row at the top of the list.
static uint8_t g_top;

// The rows: a VM each, then New VM, after a line if there are any VMs.
static uint8_t RowCount(void) {
  return (uint8_t)(g_vm_manager.vms->count +
                   (g_vm_manager.vms->count > 0 ? 2 : 1));
}

// Keep the selected item among the rows.
static void ScrollToSelected(void) {
  uint8_t row = g_list_box.selected;
  if (g_vm_manager.vms->count > 0 &&
      g_list_box.selected >= g_vm_manager.vms->count) {
    ++row;
  }
  ListBoxScrollTo(row, &g_top);
}

// Show the list from its top, with the item at index selected.
static void ShowSelected(uint8_t index) {
  g_top = 0;
  g_list_box.selected = index;
  g_list_box.dirty = true;
  ScrollToSelected();
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
  ShowSelected(0);
  return ScreenOpen(&kVMManagerScreen);
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  const uint8_t count = (uint8_t)(g_vm_manager.vms->count + 1);
  switch (ListBoxHandleKeyEvent(&g_list_box, event, count)) {
    case kListBoxActionMoved:
      ScrollToSelected();
      return ScreenStays();
    case kListBoxActionSelect:
      if (g_list_box.selected == g_vm_manager.vms->count) {
        return VMConfigScreenOpenNew(&g_vm_manager);
      }
      return VMScreenOpen(&g_vm_manager, g_list_box.selected);
    case kListBoxActionBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

// Back from a VM's page or New VM, with the list shown from its top again
// and the item that opened them selected. After the card changed, read the
// VMs afresh, and select the VM the change named instead, or the first if
// it is not there, showing its page too if it said so. The settings are not
// read again: what changes them, a rename or delete of the default VM,
// updates them as it saves them.
static __noinline ScreenResult OnResume(void) {
  if (!g_vm_manager.changed) {
    ShowSelected(g_list_box.selected);
    return ScreenStays();
  }
  g_vm_manager.changed = false;
  VMListRead(g_vm_manager.vms);
  const uint8_t index = VMListFind(g_vm_manager.vms, g_vm_manager.to_select);
  const bool found = index < g_vm_manager.vms->count;
  ShowSelected(found ? index : 0);
  if (g_vm_manager.show_vm && found) {
    return VMScreenOpen(&g_vm_manager, index);
  }
  return ScreenStays();
}

// The VMs in yax86/vm, a row each, then New VM after a line if there are
// any, with arrows on the border where there are more above or below.
static void OnDraw(Y_UNUSED uint64_t now_us) {
  DisplayMenu* menu = ListBoxBeginDraw(&g_list_box);
  if (menu == NULL) {
    return;
  }
  const VMList* vms = g_vm_manager.vms;
  MenuBoxPlace(menu, kListBoxNarrowColumns, kListBoxRows + 2);
  MenuBoxEdgeRow(&menu->text[0], menu->columns, true, "Manage VMs");
  for (uint8_t i = 0; i < kListBoxRows; ++i) {
    DisplayTextRow* row = &menu->text[1 + i];
    const uint8_t index = (uint8_t)(g_top + i);
    if (index >= RowCount()) {
      MenuBoxBlankRow(row, menu->columns);
    } else if (index < vms->count) {
      const MenuBoxItem item = {vms->vms[index].name, true};
      MenuBoxItemRow(row, menu->columns, &item, index == g_list_box.selected);
    } else if (vms->count > 0 && index == vms->count) {
      MenuBoxSeparatorRow(row, menu->columns);
    } else {
      MenuBoxItemRow(
          row, menu->columns, &kNewVMRow, g_list_box.selected == vms->count);
    }
  }
  MenuBoxEdgeRow(&menu->text[kListBoxRows + 1], menu->columns, false, NULL);
  ListBoxDrawScrollArrows(menu, g_top, RowCount());
  ListBoxEndDraw(&g_list_box);
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return "vm_manager"; }

const Screen kVMManagerScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .on_resume = OnResume,
    .hint = Hint,
    .page_name = PageName,
};
