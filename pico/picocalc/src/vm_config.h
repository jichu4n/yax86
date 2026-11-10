// VMs: a folder each in `yax86/vm`, holding a configuration file,
// `vm.cfg`, that names the images in the VM's drives, and whichever of
// those images are its own. The list at power on is every folder in
// `yax86/vm` with a `vm.cfg`, and the default VM if it is elsewhere. A
// VM is named for its folder: `yax86/vm/dos330` is the VM DOS330.
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
// boot_default is the path of a VM's file, relative to the folder
// the settings are in or, with a leading /, to the card's root. It is the
// VM the list highlights, and starts by itself after boot_delay seconds
// with no key pressed; 0 starts it at once. Without settings, or with a
// boot_default that is not there, the first VM listed is the default,
// after 5 seconds. Whichever VM starts becomes boot_default for the next
// power on.
#ifndef YAX86_PICOCALC_VM_CONFIG_H
#define YAX86_PICOCALC_VM_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "config_file.h"
#include "file_list.h"

enum {
  // Drives A and B, floppies, and C and D, the hard disk controller's master
  // and slave.
  kVMDriveA = 0,
  kVMDriveB,
  kVMDriveC,
  kVMDriveD,
  kVMDrives,

  // The most VMs listed: the first found past this are left out.
  kVMListMax = 16,
  // A VM's name: its folder's, an 8.3 name.
  kVMNameLength = 12,
  // The longest problem described.
  kVMProblemLength = kConfigFileProblemLength,
  // How long the list at power on waits for a key, in seconds, without
  // settings, and at most.
  kVMDefaultBootDelay = 5,
  kVMMaxBootDelay = 60,
};

typedef struct VMSettings {
  // The path of the VM to highlight and start, from the card's root, or
  // empty for the first listed.
  char boot_default[kFileListMaxPath + 1];
  uint8_t boot_delay;
} VMSettings;

// A VM: its name, and its file's path from the card's root.
typedef struct VMEntry {
  char name[kVMNameLength + 1];
  char path[kFileListMaxPath + 1];
} VMEntry;

typedef struct VMList {
  VMEntry vms[kVMListMax];
  uint8_t count;
  // Whether there were more VMs than the list holds.
  bool truncated;
} VMList;

typedef struct VMConfig {
  VMEntry entry;
  // Each drive's image, as a path from the card's root, or empty.
  char drives[kVMDrives][kFileListMaxPath + 1];
} VMConfig;

// List the VMs in `yax86/vm`, sorted by name. An empty list, with
// no card or no such folder, means there are none to choose from.
void VMListRead(VMList* list);

// The index in the list of the VM whose file is at path, adding it at
// the end if it is not listed. Returns kVMListMax if the list is full.
uint8_t VMListFind(VMList* list, const char* path);

// Read the VM whose file is at path, from the card's root. Returns
// false, with the problem described in problem, if the file could not be
// read or has a line that is not understood; config then holds what was
// understood before it.
bool VMConfigRead(
    const char* path, VMConfig* config, char problem[kVMProblemLength + 1]);

// Read the card's settings, which without `yax86/yax86.cfg` are the
// defaults. Returns false, with the problem described, if the file has a line
// that is not understood, settings then holding what was understood before
// it; or if boot_default names no file, which is then left empty.
bool VMSettingsRead(VMSettings* settings, char problem[kVMProblemLength + 1]);

// Make the VM whose file is at path, from the card's root, the settings'
// boot_default, through ConfigFileUpdate(), written as VMConfigRelativePath()
// makes it from `yax86`. Returns false, with the problem described, if the
// settings could not be read, would be too long, or could not be written.
bool VMSettingsSaveBootDefault(
    const char* path, char problem[kVMProblemLength + 1]);

// Save the image in a VM's drive, a path from the card's root or NULL or
// empty for none, to its `vm.cfg`, through ConfigFileUpdate(), written as
// VMConfigRelativePath() makes it from the file's folder. Returns false, with
// the problem described, if the file could not be read, would be too long, or
// could not be written.
bool VMConfigSaveDrive(
    const VMConfig* config, uint8_t drive, const char* path,
    char problem[kVMProblemLength + 1]);

// Write path, from the card's root, as a configuration file in folder names
// it: relative to folder, climbing out with "..", when the two share a folder
// from the root, and from the root with a leading / when they share none - a
// path under `yax86` stays relative when the file moves within it. In lower
// case, FAT names being the same in either. Returns false if it would not
// fit.
bool VMConfigRelativePath(
    const char* folder, const char* path, char out[kFileListMaxPath + 2]);

// Resolve path against folder, both without a leading slash, into resolved:
// a path with a leading / starts at the card's root instead, and "." and
// ".." are followed. Returns false if the result would not fit, or climbs
// above the root.
bool VMConfigResolvePath(
    const char* folder, const char* path, char resolved[kFileListMaxPath + 1]);

#endif  // YAX86_PICOCALC_VM_CONFIG_H
