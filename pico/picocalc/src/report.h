// Status reporting and host commands over USB for the PicoCalc runtime.
//
// Formats the 6-line Status page in the menu and replies to commands sent by a
// host over USB serial (report, screen, screenshot, key, ping).
#ifndef YAX86_PICOCALC_REPORT_H
#define YAX86_PICOCALC_REPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "command.h"
#include "core/platform.h"
#include "menu.h"
#include "runtime.h"
#include "vm_config.h"

// The runtime's state the report reads, all of it owned by main.c and read
// as it changes, so pointers to it rather than copies.
typedef struct ReportConfig {
  // The emulated machine, for its video state and the VM's idle skipping.
  const YPlatformState* platform;
  // Reads a byte of guest RAM. Guest RAM may be paged, so it is read through
  // the memory map, and before the VM is up it reads as 0.
  uint8_t (*read_guest_ram_fn)(uint32_t address);
  // How much guest RAM the VM has, in bytes, which depends on whether it is
  // paged.
  uint32_t (*guest_ram_size_fn)(void);
  // Whether guest RAM is paged, and whether paging is off because the last
  // start ended in a hard fault.
  const bool* paging;
  const bool* paging_off_after_fault;
  // The guest's video memory, which the screen command reads the text from.
  const uint8_t* vram;
  // How far the boot has got, and the clock the VM is paced by.
  const RuntimeStatus* status;
  const RunClock* clock;
  // The VMs on the card, the card's settings, and the VM started, which
  // *has_vm says there is one of.
  const VMList* vms;
  const VMSettings* settings;
  const VMConfig* vm;
  const bool* has_vm;
} ReportConfig;

// Initialize the reporting module with pointers to the runtime state.
void ReportInit(const ReportConfig* config);

// Paint the stacks of both cores to measure high-water mark usage.
void ReportPaintStacks(void);

// Format the 6 lines of the menu's status page.
void ReportDescribeStatus(MenuStatusLine lines[kMenuStatusLines]);

// Announce completion to a newly attached USB host if boot has finished.
void ReportAnnounceToNewHost(void);

// Log the current boot stage.
void ReportLogBootStage(void);

// Execute a command received from a host over USB.
void ReportRunCommand(const Command* command);

#endif  // YAX86_PICOCALC_REPORT_H
