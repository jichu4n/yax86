#include "manage_screen.h"

#include <stdbool.h>
#include <stddef.h>

#include "display.h"
#include "manage.h"
#include "ui_box.h"
#include "ui_page.h"
#include "vm_form_screen.h"
#include "vm_screen.h"

static const UIBoxItem kNewVMRow = {"New VM", true};

extern const Screen kManageScreen;

// What the screens under Manage VMs share.
static ManageState g_manage;
// The item chosen, and whether the page is out of date.
static UIPage g_page;
// The row at the top of the list.
static uint8_t g_top;

// The rows: a VM each, then New VM, after a line if there are any VMs.
static uint8_t RowCount(void) {
  return (uint8_t)(g_manage.vms->count + (g_manage.vms->count > 0 ? 2 : 1));
}

// Keep the chosen item among the rows.
static void ScrollToChosen(void) {
  uint8_t row = g_page.chosen;
  if (g_manage.vms->count > 0 && g_page.chosen >= g_manage.vms->count) {
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

ScreenResult ManageScreenOpen(
    VMList* vms, Settings* settings, uint8_t* copy_buffer,
    uint32_t copy_buffer_size) {
  g_manage = (ManageState){
      .vms = vms,
      .settings = settings,
      .copy_buffer = copy_buffer,
      .copy_buffer_size = copy_buffer_size,
  };
  ShowChosen(0);
  return ScreenOpen(&kManageScreen);
}

static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  const uint8_t count = (uint8_t)(g_manage.vms->count + 1);
  switch (UIPageTakeEvent(&g_page, event, count)) {
    case kUIPageKeyMoved:
      ScrollToChosen();
      return ScreenStays();
    case kUIPageKeyChoose:
      if (g_page.chosen == g_manage.vms->count) {
        return VMFormScreenOpenNew(&g_manage);
      }
      return VMScreenOpen(&g_manage, g_page.chosen);
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
  if (!g_manage.changed) {
    ShowChosen(g_page.chosen);
    return ScreenStays();
  }
  g_manage.changed = false;
  VMListRead(g_manage.vms);
  const uint8_t index = VMListFind(g_manage.vms, g_manage.chosen);
  const bool found = index < g_manage.vms->count;
  ShowChosen(found ? index : 0);
  if (g_manage.show_vm && found) {
    return VMScreenOpen(&g_manage, index);
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
  const VMList* vms = g_manage.vms;
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

static const char* PageName(void) { return "manage"; }

const Screen kManageScreen = {
    .handle_key = HandleKey,
    .draw = Draw,
    .resume = Resume,
    .hint = Hint,
    .page_name = PageName,
};
