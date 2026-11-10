// Machines: a folder each in `yax86/vm`, holding a configuration file,
// `vm.cfg`, that names the images in the machine's drives, and whichever of
// those images are its own. The list at power on is every folder in
// `yax86/vm` with a `vm.cfg`, and the default machine if it is elsewhere. A
// machine is named for its folder: `yax86/vm/dos330` is the machine DOS330.
//
//   drive_a=../../images/msdos330/disk01.img
//   drive_c=hdd.img
//   drive_d=/shared/tools.img
//
// Each line is logfmt - any number of key=value pairs - and a line starting
// with # is a comment. A path is relative to the folder the file is in, or
// to the card's root with a leading /. A drive not named is empty. Floppy
// images are 360KB and hard disk images the core's 10MB drive.
//
// The card's own settings are `yax86/yax86.cfg`, in the same form:
//
//   boot_default=vm/dos330/vm.cfg
//   boot_delay=5
//
// boot_default is the path of a machine's file, relative to the folder
// the settings are in or, with a leading /, to the card's root. It is the
// machine the list highlights, and starts by itself after boot_delay seconds
// with no key pressed; 0 starts it at once. Without settings, or with a
// boot_default that is not there, the first machine listed is the default,
// after 5 seconds.
#ifndef YAX86_PICOCALC_MACHINE_CONFIG_H
#define YAX86_PICOCALC_MACHINE_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "file_list.h"

enum {
  // Drives A and B, floppies, and C and D, the hard disk controller's master
  // and slave.
  kMachineDriveA = 0,
  kMachineDriveB,
  kMachineDriveC,
  kMachineDriveD,
  kMachineDrives,

  // The most machines listed: the first found past this are left out.
  kMachineListMax = 16,
  // A machine's name: its folder's, an 8.3 name.
  kMachineNameLength = 12,
  // The longest configuration file read, and the longest problem described.
  kMachineConfigMaxBytes = 2048,
  kMachineProblemLength = 60,
  // How long the list at power on waits for a key, in seconds, without
  // settings, and at most.
  kMachineDefaultBootDelay = 5,
  kMachineMaxBootDelay = 60,
};

typedef struct MachineSettings {
  // The path of the machine to highlight and start, from the card's root, or
  // empty for the first listed.
  char boot_default[kFileListMaxPath + 1];
  uint8_t boot_delay;
} MachineSettings;

// A machine: its name, and its file's path from the card's root.
typedef struct MachineEntry {
  char name[kMachineNameLength + 1];
  char path[kFileListMaxPath + 1];
} MachineEntry;

typedef struct MachineList {
  MachineEntry machines[kMachineListMax];
  uint8_t count;
  // Whether there were more machines than the list holds.
  bool truncated;
} MachineList;

typedef struct MachineConfig {
  MachineEntry machine;
  // Each drive's image, as a path from the card's root, or empty.
  char drives[kMachineDrives][kFileListMaxPath + 1];
} MachineConfig;

// List the machines in `yax86/vm`, sorted by name. An empty list, with
// no card or no such folder, means there are none to choose from.
void MachineListRead(MachineList* list);

// The index in the list of the machine whose file is at path, adding it at
// the end if it is not listed. Returns kMachineListMax if the list is full.
uint8_t MachineListFind(MachineList* list, const char* path);

// Read the machine whose file is at path, from the card's root. Returns
// false, with the problem described in problem, if the file could not be
// read or has a line that is not understood; config then holds what was
// understood before it.
bool MachineConfigRead(
    const char* path, MachineConfig* config,
    char problem[kMachineProblemLength + 1]);

// Read the card's settings, which without `yax86/yax86.cfg` are the
// defaults. Returns false, with the problem described, if the file has a line
// that is not understood, settings then holding what was understood before
// it; or if boot_default names no file, which is then left empty.
bool MachineSettingsRead(
    MachineSettings* settings, char problem[kMachineProblemLength + 1]);

// Resolve path against folder, both without a leading slash, into resolved:
// a path with a leading / starts at the card's root instead, and "." and
// ".." are followed. Returns false if the result would not fit, or climbs
// above the root.
bool MachineConfigResolvePath(
    const char* folder, const char* path, char resolved[kFileListMaxPath + 1]);

#endif  // YAX86_PICOCALC_MACHINE_CONFIG_H
