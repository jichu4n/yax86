// The runtime menu, opened by holding both Shift keys.

#include "menu.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "display.h"
#include "harddisk.h"
#include "launcher.h"
#include "menu_browse.h"
#include "ui_box.h"

enum {
  kKeyLeftShift = 0xA2,
  kKeyRightShift = 0xA3,
  kKeyUp = 0xB5,
  kKeyDown = 0xB6,
  kKeyEnter = 0x0A,
  kKeySpace = ' ',
  kKeyEsc = 0xB1,
  kKeyBackspace = 0x08,

  kNarrowColumns = kUIBoxNarrowColumns,
  kWideColumns = kUIBoxWideColumns,
  kStatusLabelColumns = 9,
  kStatusRefreshUs = 1000 * 1000,
};

typedef enum MenuPage {
  kMenuPageMain = 0,
  kMenuPageStatus,
  kMenuPageReset,
  kMenuPagePowerOff,
  kMenuPageDrive,
  kMenuPageBrowse,
  kMenuPageCount,
} MenuPage;

typedef enum MainItem {
  kMainDriveA = 0,
  kMainDriveB,
  kMainStatus,
  kMainReset,
  kMainPowerOff,
  kMainItemCount,
} MainItem;

typedef enum DriveItem {
  kDriveSwap = 0,
  kDriveEject,
  kDriveInsert = 0,
  kDriveItemCount = 2,
} DriveItem;

typedef enum ResetItem {
  kResetConfirm = 0,
  kResetCancel,
  kResetItemCount,
} ResetItem;

typedef enum PowerOffItem {
  kPowerOffConfirm = 0,
  kPowerOffCancel,
  kPowerOffItemCount,
} PowerOffItem;

typedef struct MenuPageDef {
  const char* name;
  const char* (*get_name)(void);
  const char* hint;
  const char* (*get_hint)(void);
  uint8_t (*item_count)(void);
  void (*draw)(DisplayMenu* menu);
  MenuAction (*choose)(void);
  MenuAction (*back)(void);
} MenuPageDef;

static const MenuPageDef kPageDefs[kMenuPageCount];

static const MenuItem kMainItems[kMainItemCount] = {
    {"Floppy disk 1...", true},
    {"Floppy disk 2...", true},
    {"Status...", true},
    {"Reset...", true},
    {"Power off...", true},
};

static const MenuItem kResetItems[kResetItemCount] = {
    {"Reset", false},
    {"Resume", false},
};

static const MenuItem kPowerOffItems[kPowerOffItemCount] = {
    {"Power off", false},
    {"Resume", false},
};

static const MenuItem kDriveItems[kDriveItemCount] = {
    {"Swap disk image...", true},
    {"Eject", false},
};

static MenuConfig g_config;
static bool g_open;
static MenuPage g_page;
static uint8_t g_chosen;
static bool g_dirty;
static uint64_t g_status_drawn_us;
static uint8_t g_drawn_columns;
static uint8_t g_drawn_rows;
static bool g_box_drawn;

static FloppyDrive g_drive;
static bool g_left_shift_held;
static bool g_right_shift_held;

static void ShowPage(MenuPage page, uint8_t chosen) {
  g_page = page;
  g_chosen = chosen;
  g_dirty = true;
}

static void OnBrowseShowPage(uint8_t chosen) {
  ShowPage(kMenuPageBrowse, chosen);
}

static void OnBrowseBackForDrive(FloppyDrive drive) {
  (void)drive;
  ShowPage(kMenuPageDrive, kDriveInsert);
}

static void OnBrowseSetChosen(uint8_t chosen) {
  g_chosen = chosen;
  g_dirty = true;
}

static void OnBrowseSetDirty(void) {
  g_dirty = true;
}

static void OnBrowseCloseMenu(void) {
  g_open = false;
}

void MenuInit(const MenuConfig* config) {
  g_config = *config;
  const MenuBrowseConfig browse_config = {
      .drive = config->drive,
      .insert_drive = config->insert_drive,
      .show_browse_page = OnBrowseShowPage,
      .on_back_for_drive = OnBrowseBackForDrive,
      .set_chosen = OnBrowseSetChosen,
      .set_dirty = OnBrowseSetDirty,
      .close_menu = OnBrowseCloseMenu,
  };
  MenuBrowseInit(&browse_config);
}

bool MenuIsOpen(void) {
  return LauncherIsOpen() || g_open;
}

const char* MenuPageName(void) {
  if (LauncherIsOpen()) {
    return LauncherPageName();
  }
  const MenuPageDef* def = &kPageDefs[g_page];
  return def->get_name != NULL ? def->get_name() : def->name;
}

const char* MenuHint(void) {
  const MenuPageDef* def = &kPageDefs[g_page];
  return def->get_hint != NULL ? def->get_hint() : def->hint;
}

static bool DriveEmpty(void) {
  return g_config.drive(g_drive)->status == kDiskImageEjected;
}

static uint8_t MainItems(MainItem items[kMainItemCount]) {
  uint8_t count = 0;
  for (uint8_t item = 0; item < kMainItemCount; ++item) {
    if ((item == kMainDriveB && g_config.floppy_drives() < 2) ||
        (item == kMainPowerOff && !g_config.can_power_off())) {
      continue;
    }
    items[count++] = (MainItem)item;
  }
  return count;
}

static MainItem ChosenMainItem(void) {
  MainItem items[kMainItemCount];
  const uint8_t count = MainItems(items);
  return g_chosen < count ? items[g_chosen] : kMainItemCount;
}

static void ShowMainItem(MainItem item) {
  MainItem items[kMainItemCount];
  const uint8_t count = MainItems(items);
  uint8_t row = 0;
  for (uint8_t i = 0; i < count; ++i) {
    if (items[i] == item) {
      row = i;
    }
  }
  ShowPage(kMenuPageMain, row);
}

static uint8_t MainItemCount(void) {
  MainItem items[kMainItemCount];
  return MainItems(items);
}

static uint8_t StatusItemCount(void) {
  return 0;
}

static uint8_t ResetItemCount(void) {
  return kResetItemCount;
}

static uint8_t PowerOffItemCount(void) {
  return kPowerOffItemCount;
}

static uint8_t DriveItemCount(void) {
  return DriveEmpty() ? kDriveEject : kDriveItemCount;
}

static uint8_t PageItemCount(void) {
  const MenuPageDef* def = &kPageDefs[g_page];
  return def->item_count != NULL ? def->item_count() : 0;
}

static const char* DrivePageName(void) {
  static const char* const kNames[kFloppyDrives] = {"drive_a", "drive_b"};
  return kNames[g_drive];
}

static void DrawItems(
    DisplayMenu* menu, const char* title, const char* const* lines,
    uint8_t line_count, const MenuItem* items, uint8_t count) {
  UIBoxDrawItems(
      menu, title, lines, line_count, items, count, g_chosen, false);
}

static void DrawStatusPage(DisplayMenu* menu) {
  MenuStatusLine lines[kMenuStatusLines];
  g_config.describe_status(lines);
  UIBoxPlace(menu, kWideColumns, kMenuStatusLines + 4);
  uint8_t row = 0;
  UIBoxEdgeRow(&menu->text[row++], menu->columns, true, "Status");
  UIBoxBlankRow(&menu->text[row++], menu->columns);
  for (uint8_t i = 0; i < kMenuStatusLines; ++i) {
    DisplayTextRow* text_row = &menu->text[row++];
    UIBoxBlankRow(text_row, menu->columns);
    UIBoxPutText(
        text_row, menu->columns, 3, lines[i].label, kUIBoxAttrText);
    UIBoxPutText(
        text_row, menu->columns, 3 + kStatusLabelColumns, lines[i].value,
        kUIBoxAttrText);
  }
  UIBoxBlankRow(&menu->text[row++], menu->columns);
  UIBoxEdgeRow(&menu->text[row], menu->columns, false, NULL);
}

static void DrawResetPage(DisplayMenu* menu) {
  static const char* const kLines[] = {
      "Restart the VM from power on,",
      "with the disks it has now?",
  };
  DrawItems(
      menu, "Reset", kLines, sizeof(kLines) / sizeof(kLines[0]), kResetItems,
      sizeof(kResetItems) / sizeof(kResetItems[0]));
}

static void DrawPowerOffPage(DisplayMenu* menu) {
  static const char* const kLines[] = {
      "Turn the VM off,",
      "and go back to the list of VMs?",
  };
  DrawItems(
      menu, "Power Off", kLines, sizeof(kLines) / sizeof(kLines[0]),
      kPowerOffItems, sizeof(kPowerOffItems) / sizeof(kPowerOffItems[0]));
}

static void DrawDrivePage(DisplayMenu* menu) {
  const DiskImage* image = g_config.drive(g_drive);
  char line1[64];
  const char* lines[] = {line1};
  if (image->status == kDiskImageOpen) {
    char name[kDiskImageNameLength + 1];
    DiskImageName(image->path, name);
    snprintf(line1, sizeof(line1), "Disk: %s", name);
  } else {
    snprintf(line1, sizeof(line1), "Disk: (empty)");
  }
  MenuItem items[kDriveItemCount];
  items[kDriveSwap] = kDriveItems[kDriveSwap];
  if (DriveEmpty()) {
    items[kDriveSwap].text = "Choose disk image...";
  } else {
    items[kDriveEject] = kDriveItems[kDriveEject];
  }
  static const char* const kDriveTitles[kFloppyDrives] = {
      "Floppy Disk 1", "Floppy Disk 2"};
  DrawItems(
      menu, kDriveTitles[g_drive], lines, sizeof(lines) / sizeof(lines[0]),
      items, DriveItemCount());
}

static void DrawBrowsePage(DisplayMenu* menu) {
  MenuBrowseDraw(menu, g_chosen);
}

static MenuAction BackDrive(void) {
  ShowMainItem((MainItem)(kMainDriveA + g_drive));
  return kMenuActionNone;
}

static MenuAction BackReset(void) {
  ShowMainItem(kMainReset);
  return kMenuActionNone;
}

static MenuAction BackPowerOff(void) {
  ShowMainItem(kMainPowerOff);
  return kMenuActionNone;
}

static MenuAction Back(void) {
  const MenuPageDef* def = &kPageDefs[g_page];
  if (def->back != NULL) {
    return def->back();
  }
  g_open = false;
  return kMenuActionClosed;
}

static MenuAction ChooseMain(void) {
  switch (ChosenMainItem()) {
    case kMainDriveA:
    case kMainDriveB:
      g_drive = (FloppyDrive)(ChosenMainItem() - kMainDriveA);
      ShowPage(kMenuPageDrive, kDriveInsert);
      return kMenuActionNone;
    case kMainStatus:
      ShowPage(kMenuPageStatus, 0);
      return kMenuActionNone;
    case kMainReset:
      ShowPage(kMenuPageReset, kResetCancel);
      return kMenuActionNone;
    case kMainPowerOff:
      ShowPage(kMenuPagePowerOff, kPowerOffCancel);
      return kMenuActionNone;
    case kMainItemCount:
      break;
  }
  return kMenuActionNone;
}

static MenuAction ChooseReset(void) {
  if (g_chosen == kResetConfirm) {
    g_config.reset();
  }
  return BackReset();
}

static MenuAction ChoosePowerOff(void) {
  if (g_chosen == kPowerOffConfirm) {
    g_config.power_off();
  }
  return BackPowerOff();
}

static MenuAction ChooseDrive(void) {
  if (g_chosen == kDriveInsert) {
    MenuBrowseForDrive(g_drive);
  } else {
    g_config.eject_drive(g_drive);
    ShowPage(kMenuPageDrive, kDriveInsert);
  }
  return kMenuActionNone;
}

static MenuAction ChooseBrowse(void) {
  return MenuBrowseChoose(g_chosen);
}

static MenuAction Choose(void) {
  const MenuPageDef* def = &kPageDefs[g_page];
  if (def->choose != NULL) {
    return def->choose();
  }
  return Back();
}

static void DrawMainPage(DisplayMenu* menu) {
  MainItem main_items[kMainItemCount];
  const uint8_t count = MainItems(main_items);
  MenuItem items[kMainItemCount];
  for (uint8_t i = 0; i < count; ++i) {
    items[i] = kMainItems[main_items[i]];
  }
  DrawItems(menu, "Menu", NULL, 0, items, count);
}

static const MenuPageDef kPageDefs[kMenuPageCount] = {
    [kMenuPageMain] = {
        .name = "main",
        .hint = "\x18\x19=Move  Enter=Open  Esc=Resume",
        .item_count = MainItemCount,
        .draw = DrawMainPage,
        .choose = ChooseMain,
    },
    [kMenuPageStatus] = {
        .name = "status",
        .item_count = StatusItemCount,
        .draw = DrawStatusPage,
    },
    [kMenuPageReset] = {
        .name = "reset",
        .item_count = ResetItemCount,
        .draw = DrawResetPage,
        .choose = ChooseReset,
        .back = BackReset,
    },
    [kMenuPagePowerOff] = {
        .name = "power_off",
        .item_count = PowerOffItemCount,
        .draw = DrawPowerOffPage,
        .choose = ChoosePowerOff,
        .back = BackPowerOff,
    },
    [kMenuPageDrive] = {
        .get_name = DrivePageName,
        .item_count = DriveItemCount,
        .draw = DrawDrivePage,
        .choose = ChooseDrive,
        .back = BackDrive,
    },
    [kMenuPageBrowse] = {
        .name = "browse",
        .hint = "\x18\x19=Move  Enter=Open  Backspace=Up a folder  Esc=Back",
        .item_count = MenuBrowseItemCount,
        .draw = DrawBrowsePage,
        .choose = ChooseBrowse,
        .back = MenuBrowseBack,
    },
};

MenuAction MenuHandleKey(SouthbridgeKeyEvent event) {
  if (event.key == kKeyLeftShift) {
    g_left_shift_held = event.state == kSouthbridgeKeyPressed;
  } else if (event.key == kKeyRightShift) {
    g_right_shift_held = event.state == kSouthbridgeKeyPressed;
  }
  if (g_left_shift_held && g_right_shift_held) {
    g_left_shift_held = false;
    g_right_shift_held = false;
    g_open = !g_open;
    if (g_open) {
      ShowMainItem(kMainDriveA);
    }
    return g_open ? kMenuActionOpened : kMenuActionClosed;
  }
  if (!g_open || event.state != kSouthbridgeKeyPressed) {
    return kMenuActionNone;
  }
  const uint8_t count = PageItemCount();
  switch (event.key) {
    case kKeyUp:
      if (count > 0) {
        g_chosen = (uint8_t)((g_chosen + count - 1) % count);
        g_dirty = true;
      }
      return kMenuActionNone;
    case kKeyDown:
      if (count > 0) {
        g_chosen = (uint8_t)((g_chosen + 1) % count);
        g_dirty = true;
      }
      return kMenuActionNone;
    case kKeyEnter:
    case kKeySpace:
      return Choose();
    case kKeyBackspace:
      if (g_page == kMenuPageBrowse && MenuBrowseHandleBackspace()) {
        return kMenuActionNone;
      }
      return Back();
    case kKeyEsc:
      return Back();
    default:
      return kMenuActionNone;
  }
}

void MenuDraw(uint64_t now_us) {
  if (!g_open) {
    return;
  }
  if (g_page == kMenuPageStatus &&
      now_us - g_status_drawn_us >= kStatusRefreshUs) {
    g_dirty = true;
  }
  if (!g_dirty) {
    return;
  }
  DisplayMenu* menu = DisplayBeginMenu();
  if (menu == NULL) {
    return;
  }
  const MenuPageDef* def = &kPageDefs[g_page];
  if (def->draw != NULL) {
    def->draw(menu);
  }
  if (g_page == kMenuPageStatus) {
    g_status_drawn_us = now_us;
  }
  if (g_box_drawn &&
      (menu->columns != g_drawn_columns || menu->rows != g_drawn_rows) &&
      !g_config.redraw_guest()) {
    return;
  }
  DisplayEndMenu();
  g_drawn_columns = menu->columns;
  g_drawn_rows = menu->rows;
  g_box_drawn = true;
  g_dirty = false;
}
