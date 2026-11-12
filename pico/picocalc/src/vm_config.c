#include "vm_config.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "config_file.h"

const char kVMsFolder[] = "yax86/vm";
static const char kConfigFile[] = "vm.cfg";

const char* const kVMDriveLabels[kVMDrives] = {
    [kVMDriveFD1] = "FD1",
    [kVMDriveFD2] = "FD2",
    [kVMDriveHD1] = "HD1",
    [kVMDriveHD2] = "HD2",
};

const char* const kVMDriveNames[kVMDrives] = {
    [kVMDriveFD1] = "Floppy disk 1",
    [kVMDriveFD2] = "Floppy disk 2",
    [kVMDriveHD1] = "Hard disk 1",
    [kVMDriveHD2] = "Hard disk 2",
};

static const char* const kDriveKeys[kVMDrives] = {
    [kVMDriveFD1] = "fd1",
    [kVMDriveFD2] = "fd2",
    [kVMDriveHD1] = "hd1",
    [kVMDriveHD2] = "hd2",
};

bool VMEntryMake(const char* name, VMEntry* entry) {
  if (strlen(name) > kVMNameLength) {
    return false;
  }
  for (size_t i = 0;; ++i) {
    entry->name[i] = (char)toupper((unsigned char)name[i]);
    if (name[i] == '\0') {
      break;
    }
  }
  const int length = snprintf(
      entry->path, sizeof(entry->path), "%s/%s/%s", kVMsFolder, entry->name,
      kConfigFile);
  return length >= 0 && (size_t)length < sizeof(entry->path);
}

// A VM's file being read, and the folder its paths are relative to.
typedef struct VMReading {
  VMConfig* config;
  char folder[kFSMaxPath + 1];
} VMReading;

static bool TakeVMPair(
    const char* key, const char* value, void* context,
    char problem[kVMProblemLength + 1]) {
  VMReading* reading = context;
  VMConfig* config = reading->config;
  uint8_t drive = 0;
  while (drive < kVMDrives && strcmp(key, kDriveKeys[drive]) != 0) {
    ++drive;
  }
  if (drive == kVMDrives) {
    snprintf(problem, kVMProblemLength + 1, "no setting %s", key);
    return false;
  }
  config->present[drive] = true;
  if (value[0] == '\0') {
    config->drives[drive][0] = '\0';
  } else if (!FSResolvePath(reading->folder, value, config->drives[drive])) {
    snprintf(problem, kVMProblemLength + 1, "%s is not a path here", key);
    config->drives[drive][0] = '\0';
    return false;
  }
  return true;
}

bool VMConfigRead(
    const VMEntry* vm, VMConfig* config, char problem[kVMProblemLength + 1]) {
  config->entry = *vm;
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    config->drives[drive][0] = '\0';
    config->present[drive] = false;
  }
  VMReading reading = {.config = config};
  FSFolderOf(vm->path, reading.folder);
  return ConfigFileRead(vm->path, TakeVMPair, &reading, problem) ==
         kConfigFileOk;
}

bool VMConfigSaveDrive(
    const VMConfig* config, uint8_t drive, const char* path,
    char problem[kVMProblemLength + 1]) {
  char value[kFSMaxPath + 2] = "";
  if (path != NULL && path[0] != '\0') {
    char folder[kFSMaxPath + 1];
    FSFolderOf(config->entry.path, folder);
    if (!FSRelativePath(folder, path, value)) {
      snprintf(problem, kVMProblemLength + 1, "path too long");
      return false;
    }
  }
  const ConfigFileSetting setting = {.key = kDriveKeys[drive], .value = value};
  return ConfigFileUpdate(config->entry.path, &setting, 1, problem);
}

bool VMConfigSaveDrives(
    const VMEntry* vm, const char* const drives[kVMDrives],
    char problem[kVMProblemLength + 1]) {
  // Kept off the stack, with ConfigFileUpdate() under them.
  static char values[kVMDrives][kFSMaxPath + 2];
  char folder[kFSMaxPath + 1];
  FSFolderOf(vm->path, folder);
  ConfigFileSetting settings[kVMDrives];
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    values[drive][0] = '\0';
    if (drives[drive] != NULL && drives[drive][0] != '\0' &&
        !FSRelativePath(folder, drives[drive], values[drive])) {
      snprintf(
          problem, kVMProblemLength + 1, "A disk's path would be too long");
      return false;
    }
    settings[drive].key = kDriveKeys[drive];
    settings[drive].value = drives[drive] != NULL ? values[drive] : NULL;
  }
  return ConfigFileUpdate(vm->path, settings, kVMDrives, problem);
}
