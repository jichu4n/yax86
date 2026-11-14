#include "launcher.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "debug_command.h"
#include "disk_image.h"
#include "display.h"
#include "font_4x10.h"
#include "hardware/watchdog.h"
#include "launcher_menu_screen.h"
#include "launcher_status_bar.h"
#include "pico/stdlib.h"
#include "runtime_log.h"
#include "screen.h"
#include "southbridge.h"
#include "status_bar.h"
#include "text.h"
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

  // How long a boot_default that could not be saved shows, as long as a
  // message would, and how often the rows are updated meanwhile.
  kProblemShownMs = 4000,
  kProblemPollMs = 100,

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

// main.c's, which LauncherRun() is given: the card's VMs and settings.
static VMList* g_vms;
static Settings* g_settings;
// Why the launcher menu closed.
static LauncherMenuExit g_exit;

// Read the card's settings, use their font, and set *default_vm to the
// index of the default VM in g_vms, or 0 without one. Returns false, with the
// problem shown on the launcher menu, if the settings had one. Kept out of
// line, so that its message is on core 0's stack only while it runs rather
// than in the frames of the callers it would be inlined into.
static __noinline bool ReadSettings(uint8_t* default_vm) {
  char problem[kVMProblemLength + 1];
  bool was_read = SettingsRead(g_settings, problem);
  DisplaySetFont(g_settings->font);
  *default_vm = 0;
  if (was_read && g_settings->boot_default[0] != '\0') {
    // Past the list's end, the first is the default instead.
    const uint8_t index = VMListFind(g_vms, g_settings->boot_default);
    if (index < kVMListMaxNumVMs) {
      *default_vm = index;
    }
  }
  if (!was_read) {
    char message[kTextColumns + 1];
    snprintf(message, sizeof(message), "YAX86.CFG: %s", problem);
    StatusBarSetProblem(message);
  }
  return was_read;
}

// Make the VM named name the one to start at the next power on, if it is not
// already. Writing the card only when it changes keeps a board that starts
// its default every time from writing at all.
static void SaveBootDefault(const char* name) {
  if (strcasecmp(g_settings->boot_default, name) == 0) {
    return;
  }
  char problem[kVMProblemLength + 1];
  if (SettingsSaveBootDefault(g_settings, name, problem)) {
    SYSTEM_INFO("boot_default_saved", Y_LOG_STRING("name", name));
    return;
  }
  char message[kTextColumns + 1];
  snprintf(
      message, sizeof(message), "Could not save boot_default: %.50s", problem);
  SYSTEM_WARN("boot_default_not_saved", Y_LOG_STRING("problem", problem));
  // Shown on the launcher's rows, for as long as a message is, before the
  // VM starts.
  StatusBarSetProblem(message);
  const absolute_time_t until = make_timeout_time_ms(kProblemShownMs);
  while (!time_reached(until)) {
    LauncherStatusBarUpdate(NULL);
    sleep_ms(kProblemPollMs);
  }
  StatusBarClearProblem();
}

// Load the VM at index in g_vms to start it: read its vm.cfg into *config,
// and make it the default. Returns false, with its problem shown on the
// launcher menu, if it does not read.
static __noinline bool LoadSelectedVM(uint8_t index, VMConfig* config) {
  const VMEntry* vm = &g_vms->vms[index];
  char problem[kVMProblemLength + 1];
  if (VMConfigRead(vm, config, problem)) {
    SYSTEM_INFO("vm_loaded", Y_LOG_STRING("path", vm->path));
    SaveBootDefault(vm->name);
    return true;
  }
  char message[kTextColumns + 1];
  snprintf(message, sizeof(message), "%s: %s", vm->name, problem);
  StatusBarSetProblem(message);
  SYSTEM_WARN(
      "vm_config_problem", Y_LOG_STRING("path", vm->path),
      Y_LOG_STRING("problem", problem));
  return false;
}

// Show the launcher menu. selected is the index in g_vms of the VM selected
// when it opens, and countdown_ms how long it counts down to starting that VM
// by itself, or 0 for no countdown. Kept out of line, so that its arguments are
// off core 0's stack while the menu shows.
static __noinline void OpenMenu(
    uint8_t* copy_buffer, uint8_t selected, uint32_t countdown_ms) {
  ScreenShow(LauncherMenuScreenOpen(
      g_vms, g_settings, copy_buffer, kCopyBufferSize, selected, countdown_ms,
      &g_exit));
}

// What the start found to do.
typedef enum StartResult {
  // A VM was loaded without the launcher menu: the one restarted, or the
  // default with no wait.
  kStartLoadedVM = 0,
  // There is no card, so the built-in floppy boots.
  kStartNoCard,
  // The launcher menu shows, and waits for a key or the countdown.
  kStartShowsMenu,
} StartResult;

// Read the card's VMs and settings, and the note a reset or power off left,
// and either load a VM to start at once into *vm or show the launcher menu.
// Kept out of line, so that what only the start needs is off core 0's stack
// while the launcher menu shows.
static __noinline StartResult Start(uint8_t* copy_buffer, VMConfig* vm) {
  StartNote note = {0};
  const bool was_noted = TakeStartNote(&note);
  VMListRead(g_vms);
  // The VM selected: the default, or the one a power off left.
  uint8_t selected = 0;
  const bool were_settings_read = ReadSettings(&selected);
  // Without a card there is no VM to start, or card to put on USB.
  if (DiskImageMountCard() != kDiskImageOpen) {
    return kStartNoCard;
  }
  if (was_noted) {
    SYSTEM_INFO(
        note.intent == kStartIntentRestart ? "restarting" : "powered_off",
        Y_LOG_STRING("name", note.name));
    const uint8_t index = VMListFind(g_vms, note.name);
    if (index < kVMListMaxNumVMs) {
      selected = index;
      if (note.intent == kStartIntentRestart && LoadSelectedVM(index, vm)) {
        return kStartLoadedVM;
      }
    }
  } else if (
      were_settings_read && g_settings->boot_delay == 0 && g_vms->num_vms > 0 &&
      LoadSelectedVM(selected, vm)) {
    return kStartLoadedVM;
  }
  const bool is_counting =
      !was_noted && g_settings->boot_delay > 0 && g_vms->num_vms > 0;
  OpenMenu(
      copy_buffer, selected,
      is_counting ? (uint32_t)g_settings->boot_delay * kMillisecondsPerSecond
                  : 0);
  return kStartShowsMenu;
}

// Act on why the launcher menu closed. Returns true when a VM is to start:
// the one selected, loaded into *vm.
//
// Otherwise it shows the menu again, without counting down, and returns
// false. That is after USB mass storage gave the card to the host, with the
// card read again; or when the selected VM's vm.cfg does not read, with its
// problem shown and it still selected.
static __noinline bool HandleMenuExit(uint8_t* copy_buffer, VMConfig* vm) {
  uint8_t selected = g_exit.vm;
  if (g_exit.reason == kLauncherMenuExitReadCard) {
    // The host may have added VMs, removed them, or changed the settings.
    VMListRead(g_vms);
    ReadSettings(&selected);
  } else if (LoadSelectedVM(g_exit.vm, vm)) {
    return true;
  }
  OpenMenu(copy_buffer, selected, 0);
  return false;
}

// Launcher mode's groups of a report, after the system's: the font, the VMs
// on the card, and the screen showing.
static void Report(const DebugCommand* command) {
  DebugCommandReportSystem(command);
  DEBUG_COMMAND_REPLY_GROUP(
      command, "display", Y_LOG_STRING("font", Font4x10Name(g_settings->font)));
  DEBUG_COMMAND_REPLY_GROUP(
      command, "vm", Y_LOG_STRING("name", ""),
      Y_LOG_UINT("vms", g_vms->num_vms));
  DEBUG_COMMAND_REPLY_GROUP(
      command, "menu", Y_LOG_BOOL("open", true),
      Y_LOG_STRING("page", ScreenPageName()));
  DebugCommandReplyOk(command);
}

// Answer a command: report here, and screen with no_guest, as no guest runs
// yet; any other with DebugCommandRunCommon().
static void RunCommand(const DebugCommand* command) {
  if (DebugCommandIs(command, "report")) {
    Report(command);
  } else if (DebugCommandIs(command, "screen")) {
    DebugCommandReplyError(command, "no_guest");
  } else {
    DebugCommandRunCommon(command);
  }
}

// Answer the command the host has sent, if a whole one is in. Kept out of
// line, so that the command is off core 0's stack while the loop runs.
static __noinline void PollCommand(void) {
  DebugCommand command;
  if (DebugCommandPoll(&command)) {
    RunCommand(&command);
  }
}

// Draw the screen showing, and send it to core 1 if it drew. Every box the
// launcher draws is the size of a list at least, so a new one covers the
// last, and there is no guest's screen to draw under it.
static void DrawScreen(void) {
  DisplayBox* box = DisplayBeginBox();
  if (box != NULL && ScreenDraw(box)) {
    DisplayEndBox();
  }
}

bool LauncherRun(
    VMList* vms, Settings* settings, uint8_t* copy_buffer, VMConfig* vm) {
  g_vms = vms;
  g_settings = settings;
  switch (Start(copy_buffer, vm)) {
    case kStartLoadedVM:
      // A problem shown at the start is the launcher's, and goes with it.
      StatusBarClearProblem();
      return true;
    case kStartNoCard:
      return false;
    case kStartShowsMenu:
      break;
  }
  absolute_time_t next_status_at = get_absolute_time();
  for (;;) {
    PollCommand();
    SouthbridgeKeyEvent key;
    const bool was_pressed = SouthbridgePollKey(&key) || TypingNext(&key);
    if (was_pressed) {
      ScreenHandleKey(key);
    }
    if (ScreenIsOpen()) {
      ScreenTick();
    }
    // The launcher menu closes once it has decided: a VM to start, which may
    // not load, or after USB mass storage, the card to be read again. A
    // problem it showed goes with it, as the emulator's rows have nothing to
    // clear it.
    if (!ScreenIsOpen() && HandleMenuExit(copy_buffer, vm)) {
      StatusBarClearProblem();
      return true;
    }
    // The rows are updated at once after a key, so that the keys of the
    // screen it led to show without waiting for the next update.
    if (was_pressed || time_reached(next_status_at)) {
      next_status_at = make_timeout_time_ms(kStatusUpdateMs);
      LauncherStatusBarUpdate(ScreenHint());
    }
    DrawScreen();
    sleep_ms(kPollMs);
  }
}
