// The launcher menu: the first screen at power on, which lists the VMs on
// the card to choose one to start.
//
// It has a row for each VM, then three rows that each open a screen of their
// own over it: Manage VMs, USB mass storage and Settings. A separator line
// comes between the VMs and those three when there are VMs. One row is
// chosen at a time, marked as such. Up and Down choose another, and Enter
// or Space acts on the one chosen: it starts a VM, or opens a row's screen.
// Esc and Backspace do nothing, as there is no screen under it to go back
// to.
//
// It can count down to starting the chosen VM by itself, showing the
// seconds left on its bottom edge. Any key stops the countdown.
//
// The launcher menu only chooses. It closes when a VM is to start, by a key
// or the countdown, and when USB mass storage has given the card to the
// host, which may have changed it; why is written to a LauncherMenuExit.
// Starting the VM, and reading the card again, are the launcher's, in
// launcher.c.
#ifndef YAX86_PICOCALC_LAUNCHER_MENU_SCREEN_H
#define YAX86_PICOCALC_LAUNCHER_MENU_SCREEN_H

#include <stdint.h>

#include "screen.h"
#include "settings.h"
#include "vm_list.h"

// Why the launcher menu closed.
typedef enum LauncherMenuExitReason {
  // Start the chosen VM: Enter or Space was pressed on it, or the countdown
  // ran out.
  kLauncherMenuExitStartVM = 0,
  // Read the card again: USB mass storage gave it to the host, which may
  // have changed its VMs and settings.
  kLauncherMenuExitReadCard,
} LauncherMenuExitReason;

// Why the launcher menu closed, and for kLauncherMenuExitStartVM, which VM
// to start.
typedef struct LauncherMenuExit {
  LauncherMenuExitReason reason;
  // The VM to start, as an index in the VM list.
  uint8_t vm;
} LauncherMenuExit;

// Open the launcher menu on vms. chosen is the index in vms of the VM chosen
// when it opens.
//
// countdown_ms is how long to wait for a key before starting that VM by
// itself, or 0 for no countdown.
//
// The screens opened from the menu change vms and settings: the VM manager
// as it makes, renames and deletes VMs, and Settings as it saves the font.
// The VM manager also copies files through copy_buffer, copy_buffer_size
// bytes of guest RAM. All of these must outlive the menu.
//
// When the menu closes, why is written to *exit.
ScreenResult LauncherMenuScreenOpen(
    VMList* vms, Settings* settings, uint8_t* copy_buffer,
    uint32_t copy_buffer_size, uint8_t chosen, uint32_t countdown_ms,
    LauncherMenuExit* exit);

#endif  // YAX86_PICOCALC_LAUNCHER_MENU_SCREEN_H
