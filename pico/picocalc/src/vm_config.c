#include "vm_config.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "config_file.h"
#include "core/platform.h"
#include "disk_image.h"
#include "ff.h"

static const char kVMsFolder[] = "yax86/vm";
static const char kConfigFile[] = "vm.cfg";
static const char kSettingsPath[] = "yax86/yax86.cfg";
static const char kBootDefaultKey[] = "boot_default";
static const char kFontKey[] = "font";

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

// Add a VM where it belongs, keeping the list sorted by name.
static void Insert(VMList* list, const VMEntry* entry) {
  if (list->count == kVMListMax) {
    list->truncated = true;
    return;
  }
  uint8_t i = list->count;
  while (i > 0 && strcmp(list->vms[i - 1].name, entry->name) > 0) {
    list->vms[i] = list->vms[i - 1];
    --i;
  }
  list->vms[i] = *entry;
  ++list->count;
}

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

uint8_t VMListFind(const VMList* list, const char* name) {
  for (uint8_t i = 0; i < list->count; ++i) {
    if (strcasecmp(list->vms[i].name, name) == 0) {
      return i;
    }
  }
  return kVMListMax;
}

// Add a folder of the VMs folder to the list, if it is a VM: a folder,
// neither hidden nor a system folder, with a vm.cfg in it.
static bool TakeVMFolder(const FILINFO* info, void* context) {
  VMList* list = context;
  VMEntry entry;
  FILINFO config;
  if ((info->fattrib & AM_DIR) && !(info->fattrib & (AM_HID | AM_SYS)) &&
      VMEntryMake(info->fname, &entry) &&
      f_stat(entry.path, &config) == FR_OK && !(config.fattrib & AM_DIR)) {
    Insert(list, &entry);
  }
  return true;
}

void VMListRead(VMList* list) {
  list->count = 0;
  list->truncated = false;
  if (DiskImageMountCard() != kDiskImageOpen) {
    return;
  }
  FSListFolder(kVMsFolder, TakeVMFolder, list);
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

static bool TakeSettingsPair(
    const char* key, const char* value, void* context,
    char problem[kVMProblemLength + 1]) {
  VMSettings* settings = context;
  if (strcmp(key, kBootDefaultKey) == 0) {
    VMEntry entry;
    settings->boot_default[0] = '\0';
    if (value[0] != '\0' && !VMEntryMake(value, &entry)) {
      snprintf(
          problem, kVMProblemLength + 1, "boot_default is not a VM's name");
      return false;
    }
    snprintf(
        settings->boot_default, sizeof(settings->boot_default), "%s", value);
    return true;
  }
  if (strcmp(key, "boot_delay") == 0) {
    uint64_t seconds = 0;
    if (!YLogfmtParseUInt(value, &seconds) || seconds > kVMMaxBootDelay) {
      snprintf(
          problem, kVMProblemLength + 1, "boot_delay is not 0 to %u",
          (unsigned)kVMMaxBootDelay);
      return false;
    }
    settings->boot_delay = (uint8_t)seconds;
    return true;
  }
  if (strcmp(key, kFontKey) == 0) {
    if (!Font4x10FromName(value, &settings->font)) {
      snprintf(problem, kVMProblemLength + 1, "no font %s", value);
      return false;
    }
    return true;
  }
  snprintf(problem, kVMProblemLength + 1, "no setting %s", key);
  return false;
}

bool VMSettingsRead(VMSettings* settings, char problem[kVMProblemLength + 1]) {
  settings->boot_default[0] = '\0';
  settings->boot_delay = kVMDefaultBootDelay;
  settings->font = kFont4x10Default;
  const ConfigFileResult result =
      ConfigFileRead(kSettingsPath, TakeSettingsPair, settings, problem);
  if (result == kConfigFileMissing) {
    problem[0] = '\0';
  }
  if (result == kConfigFileProblem) {
    return false;
  }
  VMEntry entry;
  if (settings->boot_default[0] != '\0' &&
      (!VMEntryMake(settings->boot_default, &entry) || !FSExists(entry.path))) {
    snprintf(
        problem, kVMProblemLength + 1, "no VM named %s",
        settings->boot_default);
    settings->boot_default[0] = '\0';
    return false;
  }
  return true;
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

bool VMSettingsSaveBootDefault(
    const char* name, char problem[kVMProblemLength + 1]) {
  const ConfigFileSetting setting = {.key = kBootDefaultKey, .value = name};
  return ConfigFileUpdate(kSettingsPath, &setting, 1, problem);
}

bool VMSettingsSaveFont(Font4x10 font, char problem[kVMProblemLength + 1]) {
  const ConfigFileSetting setting = {
      .key = kFontKey, .value = Font4x10Name(font)};
  return ConfigFileUpdate(kSettingsPath, &setting, 1, problem);
}
