// The menu's file browser: the folders and disk images on the card, for
// choosing an image for a floppy drive, for a new VM's drive, or to share over
// USB. menu.c shows it as one of its pages and carries out what is chosen.
#ifndef YAX86_PICOCALC_MENU_BROWSE_H
#define YAX86_PICOCALC_MENU_BROWSE_H

#include <stdbool.h>
#include <stdint.h>

#include "display.h"
#include "menu.h"

// What the browser asks of the menu that shows it. Each callback is called
// as the browser needs it, with the menu's own state still in place.
typedef struct MenuBrowseConfig {
  // The image in a floppy drive, which the browser starts beside and marks.
  const DiskImage* (*drive_fn)(FloppyDrive drive);
  // Puts the image at path in a floppy drive, when one is chosen for it.
  void (*insert_drive_fn)(FloppyDrive drive, const char* path);
  // Shows the browser's page with the entry at chosen marked, when the
  // browser has opened a folder.
  void (*show_page_fn)(uint8_t chosen);
  // Marks the entry at chosen, when going up a folder finds the one left.
  void (*set_chosen_fn)(uint8_t chosen);
  // Called when an image is chosen for a new VM's drive, or to share over
  // USB, to take it and show the page that asked for it.
  void (*chose_for_new_vm_fn)(uint8_t new_drive, const char* path);
  void (*chose_for_usb_fn)(const char* path, uint32_t size);
  // Called when the browser is left without choosing, to show the page it
  // was opened from.
  void (*back_to_new_vm_fn)(void);
  void (*back_to_usb_fn)(void);
  void (*back_to_drive_fn)(FloppyDrive drive);
  // Closes the menu, once an image is in a drive.
  void (*close_menu_fn)(void);
} MenuBrowseConfig;

void MenuBrowseInit(const MenuBrowseConfig* config);

// Start browsing for an image for a floppy drive, in the folder of the image
// in it, or else of the one in the other drive, or else yax86.
void MenuBrowseForDrive(FloppyDrive drive);

// Start browsing for an image of either kind to share over USB.
void MenuBrowseForUSB(void);

// Start browsing for an image for a new VM's drive: a floppy for its floppy
// drives, a hard disk for its hard disks. image is the one chosen for the
// drive so far, or empty, and must stay as it is while the browser is open.
void MenuBrowseForNewVM(uint8_t new_drive, const char* image);

// How many entries the folder showing has.
uint8_t MenuBrowseItemCount(void);

// The browser: the folder as the title, and a row for each entry that
// shows, with arrows on the border where there is more above or below.
void MenuBrowseDraw(DisplayMenu* menu, uint8_t chosen);

// Open the chosen entry of the browser: a folder to look in, or an image to
// put in the drive, which closes the menu.
MenuAction MenuBrowseChoose(uint8_t chosen);

// Leave the browser for the page it was opened from.
MenuAction MenuBrowseBack(void);

// Go up a folder for Backspace, until the card's root. Returns false at the
// root, where Backspace goes back a page instead.
bool MenuBrowseHandleBackspace(void);

// Keep the chosen entry among the browser's rows.
void MenuBrowseScrollTo(uint8_t chosen);

#endif  // YAX86_PICOCALC_MENU_BROWSE_H
