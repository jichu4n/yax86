// The runtime menu, opened by holding both Shift keys, which nothing in DOS
// does and typing never does. The VM is paused while it is open.
//
// It is a box drawn over the guest's screen, with a page at a time: the main
// page's items, a page for each floppy drive and the browser that finds it an
// image, the status page, and a question before a reset or a power off.
// Up and Down move, Enter or Space chooses, and Esc or Backspace goes back a
// page, or closes the menu from the main one. Both Shift keys again close it
// too.
//
// This holds the menu's state and draws it; what its items do is the
// caller's, through the actions in MenuConfig.
#ifndef YAX86_PICOCALC_MENU_H
#define YAX86_PICOCALC_MENU_H

#include <stdbool.h>
#include <stdint.h>

#include "disk_image.h"
#include "floppy.h"
#include "southbridge.h"

enum {
  // The status page's lines, and the longest value one can have.
  kMenuStatusLines = 6,
  kMenuStatusValueColumns = 68,
};

// One line of the status page.
typedef struct MenuStatusLine {
  const char* label;
  char value[kMenuStatusValueColumns + 1];
} MenuStatusLine;

typedef struct MenuConfig {
  // Fill in the status page's lines.
  void (*describe_status)(MenuStatusLine lines[kMenuStatusLines]);
  // How many floppy drives the machine has: floppy disk 2's page is offered
  // only with two.
  uint8_t (*floppy_drives)(void);
  // Restart the VM from power on, with the same disks. Does not return.
  void (*reset)(void);
  // Turn the VM off and go back to the list of VMs, which waits for a key.
  // Does not return. Offered only while can_power_off() says there is a list
  // to go back to.
  void (*power_off)(void);
  bool (*can_power_off)(void);
  // Draw the guest's screen in full, under the menu, and return whether core
  // 1 took it; false means try again later. The menu calls it when its box
  // changes size, before drawing the new one, so that nothing of the old one
  // is left showing.
  bool (*redraw_guest)(void);
  // A floppy drive's image, to show what is in the drive and where the
  // browser starts; take its disk out; and put the image at a path in, in
  // its place.
  const DiskImage* (*drive)(FloppyDrive drive);
  void (*eject_drive)(FloppyDrive drive);
  void (*insert_drive)(FloppyDrive drive, const char* path);
} MenuConfig;

// What a key did to the menu.
typedef enum MenuAction {
  // Nothing the caller need act on.
  kMenuActionNone = 0,
  kMenuActionOpened,
  kMenuActionClosed,
} MenuAction;

void MenuInit(const MenuConfig* config);

bool MenuIsOpen(void);
const char* MenuPageName(void);

// Hand the menu a key event - every one, whether the menu is open or not,
// since the hotkey is two keys held together. While the menu is closed, only
// the hotkey does anything here, and the caller still passes the key on to
// the guest.
MenuAction MenuHandleKey(SouthbridgeKeyEvent event);

// Draw the menu if it has changed, and the status page again if a second has
// passed. Call every pass of the main loop while the menu is open.
void MenuDraw(uint64_t now_us);

// The keys to show on the status bar's message row while the menu is open,
// as Key=Action pairs.
const char* MenuHint(void);

#endif  // YAX86_PICOCALC_MENU_H
