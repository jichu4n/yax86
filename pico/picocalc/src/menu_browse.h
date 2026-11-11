// File browser page in the PicoCalc runtime menu.
//
// Browses the SD card filesystem for floppy and hard disk images to insert into
// emulated drives, share over USB, or configure for a new or edited VM.
#ifndef YAX86_PICOCALC_MENU_BROWSE_H
#define YAX86_PICOCALC_MENU_BROWSE_H

#include <stdbool.h>
#include <stdint.h>

#include "display.h"
#include "menu.h"

enum {
  kMenuBrowseRows = kDisplayMenuMaxRows - 2,
  kMenuBrowseNoteColumn = 18,
};

typedef struct MenuBrowseConfig {
  const DiskImage* (*drive)(FloppyDrive drive);
  void (*insert_drive)(FloppyDrive drive, const char* path);
  void (*show_browse_page)(uint8_t chosen);
  void (*on_selected_for_new_vm)(uint8_t new_drive, const char* path);
  void (*on_selected_for_usb)(const char* path, uint32_t size);
  void (*on_back_for_new_vm)(void);
  void (*on_back_for_usb)(void);
  void (*on_back_for_drive)(FloppyDrive drive);
  void (*set_chosen)(uint8_t chosen);
  void (*set_dirty)(void);
  void (*close_menu)(void);
} MenuBrowseConfig;

void MenuBrowseInit(const MenuBrowseConfig* config);

// Start browsing for a floppy drive.
void MenuBrowseForDrive(FloppyDrive drive);

// Start browsing for a disk image to share over USB.
void MenuBrowseForUSB(void);

// Start browsing for a floppy or hard disk image for a new/edited VM.
void MenuBrowseForNewVM(uint8_t new_drive, const char* current_path);

// Page callbacks for MenuPageDef.
uint8_t MenuBrowseItemCount(void);
void MenuBrowseDraw(DisplayMenu* menu, uint8_t chosen);
MenuAction MenuBrowseChoose(uint8_t chosen);
MenuAction MenuBrowseBack(void);

// Keyboard and scrolling helpers.
bool MenuBrowseHandleBackspace(void);
void MenuBrowseScrollTo(uint8_t chosen);

#endif  // YAX86_PICOCALC_MENU_BROWSE_H
