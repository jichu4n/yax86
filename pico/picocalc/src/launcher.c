#include "launcher.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "disk_image.h"
#include "hardware/watchdog.h"
#include "launcher_card.h"
#include "launcher_menu_screen.h"
#include "launcher_status.h"
#include "pico/stdlib.h"
#include "runtime_log.h"
#include "screen.h"
#include "southbridge.h"
#include "status_bar.h"
#include "typing.h"
#include "vm_list.h"

enum {
  // The part of the guest's RAM a clone's files are copied through, before a
  // VM starts and the RAM is the guest's: 64 of the card's blocks, which go to
  // and from it a run at a time.
  kCopyBufferSize = 32 * 1024,
  // Written to g_start_note's marker by a reset or a power off, for the
  // start after the reboot - see LeaveStartNote().
  kStartNoteMarker = 0x5747A27E,

  // How often the status rows are composed again, and how long the loop
  // sleeps between passes, as the VM's loop does while its menu is open.
  kStatusUpdateMs = 100,
  kPollMs = 10,

  kMillisecondsPerSecond = 1000,
};

typedef struct StartNote {
  // kStartNoteMarker while the note is to be acted on.
  uint32_t marker;
  StartIntent intent;
  // The VM's name.
  char name[kVMNameLength + 1];
} StartNote;

// Deliberately not zeroed at startup, so that it survives the reboot - see
// LeaveStartNote().
static StartNote __uninitialized_ram(g_start_note);

// Restart the board, leaving a note for the start after it, which takes it
// - see TakeStartNote(). Every start, whether from power on, a reset or a power
// off, begins from the same state. Nothing is lost: the keyboard controller
// keeps its power, and the guest's disks have every sector it wrote on the
// card.
//
// The note is in RAM that startup does not clear, as g_overclock_marker is: it
// survives the reboot but not a power cycle. It is only ever set across the
// reboot, and the start after takes it whatever it does next, so a later
// reboot - picotool's, or the power button's - never finds one left from
// before.
void LauncherLeaveStartNote(StartIntent intent, const char* name) {
  g_start_note.intent = intent;
  memcpy(g_start_note.name, name, sizeof(g_start_note.name));
  g_start_note.marker = kStartNoteMarker;
  watchdog_reboot(0, 0, 0);
  for (;;) {
    tight_loop_contents();
  }
}

// Take the note a reset or power off left, if one did: returns whether there
// was one.
static bool TakeStartNote(StartNote* note) {
  if (g_start_note.marker != kStartNoteMarker) {
    return false;
  }
  g_start_note.marker = 0;
  note->intent = g_start_note.intent;
  // The name may have been left unterminated by RAM it never wrote.
  g_start_note.name[kVMNameLength] = '\0';
  memcpy(note->name, g_start_note.name, sizeof(note->name));
  return true;
}

// What the start found to do.
typedef enum StartResult {
  // A VM was taken without the list: the one restarted, or the default with
  // no wait.
  kStartTookVM = 0,
  // There is no card, so the built-in floppy boots.
  kStartNoCard,
  // The list shows, and waits for a key or the countdown.
  kStartShowsList,
} StartResult;

// Read the card's VMs and settings, and the note a reset or power off left,
// and either take a VM at once into *chosen or show the list. Kept out of
// line, so that what only the start needs is off core 0's stack while the
// list shows.
static __noinline StartResult
Start(VMList* vms, Settings* settings, uint8_t* copy_buffer, VMConfig* chosen) {
  StartNote note = {0};
  const bool noted = TakeStartNote(&note);
  VMListRead(vms);
  uint8_t highlighted = 0;
  const bool settings_read =
      LauncherCardReadSettings(vms, settings, &highlighted);
  // Without a card there is nothing to choose, or to put on USB.
  if (DiskImageMountCard() != kDiskImageOpen) {
    return kStartNoCard;
  }
  if (noted) {
    SYSTEM_INFO(
        note.intent == kStartIntentRestart ? "restarting" : "powered_off",
        Y_LOG_STRING("name", note.name));
    const uint8_t index = VMListFind(vms, note.name);
    if (index < kVMListMax) {
      highlighted = index;
      if (note.intent == kStartIntentRestart &&
          LauncherCardTakeVM(vms, settings, index, chosen)) {
        return kStartTookVM;
      }
    }
  } else if (
      settings_read && settings->boot_delay == 0 && vms->count > 0 &&
      LauncherCardTakeVM(vms, settings, highlighted, chosen)) {
    return kStartTookVM;
  }
  const bool counting = !noted && settings->boot_delay > 0 && vms->count > 0;
  ScreenShow(LauncherMenuScreenOpen(
      vms, settings, copy_buffer, kCopyBufferSize, highlighted,
      counting ? (uint32_t)settings->boot_delay * kMillisecondsPerSecond : 0,
      chosen));
  return kStartShowsList;
}

bool LauncherRun(
    VMList* vms, Settings* settings, uint8_t* copy_buffer,
    const CommandsHandlers* commands, VMConfig* chosen) {
  switch (Start(vms, settings, copy_buffer, chosen)) {
    case kStartTookVM:
      // A problem shown at the start is the launcher's, and goes with it.
      StatusBarClearProblem();
      return true;
    case kStartNoCard:
      return false;
    case kStartShowsList:
      break;
  }
  absolute_time_t next_status_at = get_absolute_time();
  for (;;) {
    CommandsPoll(commands, NULL);
    SouthbridgeKeyEvent key;
    const bool pressed = SouthbridgePollKey(&key) || TypingNext(&key);
    if (pressed) {
      ScreenHandleKey(key);
    }
    if (ScreenIsOpen()) {
      ScreenTick();
    }
    // The list closes once a VM is chosen, and a problem it showed goes
    // with it: the emulator's rows have nothing to clear it.
    if (!ScreenIsOpen()) {
      StatusBarClearProblem();
      return true;
    }
    // The keys for the page a key led to are offered at once rather than at
    // the next update.
    if (pressed || time_reached(next_status_at)) {
      next_status_at = make_timeout_time_ms(kStatusUpdateMs);
      LauncherStatusOffer(ScreenHint());
    }
    ScreenDraw(time_us_64());
    sleep_ms(kPollMs);
  }
}
