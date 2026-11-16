#include "vm_config.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "config_file.h"
#include "pico/platform.h"
#include "runtime_log.h"

const char kVMsFolder[] = "yax86/vm";
static const char kConfigFile[] = "vm.cfg";
static const char kVideoAdapterKey[] = "video_adapter";
static const char kMemoryKBKey[] = "memory_kb";

const char* const kVMDriveLabels[kNumVMDrives] = {
    [kVMDriveFD1] = "FD1",
    [kVMDriveFD2] = "FD2",
    [kVMDriveHD1] = "HD1",
    [kVMDriveHD2] = "HD2",
};

const char* const kVMDriveNames[kNumVMDrives] = {
    [kVMDriveFD1] = "Floppy disk 1",
    [kVMDriveFD2] = "Floppy disk 2",
    [kVMDriveHD1] = "Hard disk 1",
    [kVMDriveHD2] = "Hard disk 2",
};

static const char* const kDriveKeys[kNumVMDrives] = {
    [kVMDriveFD1] = "fd1",
    [kVMDriveFD2] = "fd2",
    [kVMDriveHD1] = "hd1",
    [kVMDriveHD2] = "hd2",
};

static const char* const kVideoAdapterNames[Y_VIDEO_NUM_ADAPTERS] = {
    [Y_VIDEO_ADAPTER_MDA] = "mda",
    [Y_VIDEO_ADAPTER_CGA] = "cga",
};

static const char* const kVideoAdapterLabels[Y_VIDEO_NUM_ADAPTERS] = {
    [Y_VIDEO_ADAPTER_MDA] = "MDA",
    [Y_VIDEO_ADAPTER_CGA] = "CGA",
};

const char* VMConfigVideoAdapterName(YVideoAdapter adapter) {
  return kVideoAdapterNames
      [adapter < Y_VIDEO_NUM_ADAPTERS ? adapter : Y_VIDEO_ADAPTER_CGA];
}

const char* VMConfigVideoAdapterLabel(YVideoAdapter adapter) {
  return kVideoAdapterLabels
      [adapter < Y_VIDEO_NUM_ADAPTERS ? adapter : Y_VIDEO_ADAPTER_CGA];
}

static bool VideoAdapterFromName(const char* name, YVideoAdapter* adapter) {
  for (uint8_t i = 0; i < Y_VIDEO_NUM_ADAPTERS; ++i) {
    if (strcasecmp(name, kVideoAdapterNames[i]) == 0) {
      *adapter = (YVideoAdapter)i;
      return true;
    }
  }
  return false;
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

// A VM's file being read, and the folder its paths are relative to.
typedef struct VMReading {
  VMConfig* config;
  char folder[kFSMaxPath + 1];
} VMReading;

static __noinline void LogIgnoredKey(const char* path, const char* key) {
  SYSTEM_WARN(
      "vm_config_key_ignored", Y_LOG_STRING("path", path),
      Y_LOG_STRING("key", key));
}

static bool TakeVMPair(
    const char* key, const char* value, void* context,
    char problem[kVMProblemLength + 1]) {
  VMReading* reading = context;
  VMConfig* config = reading->config;
  if (strcmp(key, kVideoAdapterKey) == 0) {
    if (!VideoAdapterFromName(value, &config->video_adapter)) {
      snprintf(
          problem, kVMProblemLength + 1, "video_adapter is not cga or mda");
      return false;
    }
    return true;
  }
  if (strcmp(key, kMemoryKBKey) == 0) {
    uint64_t memory_kb = 0;
    if (!YLogfmtParseUInt(value, &memory_kb) || memory_kb < kVMMinMemoryKB ||
        memory_kb > kVMMaxMemoryKB || memory_kb % kVMMemoryStepKB != 0) {
      snprintf(
          problem, kVMProblemLength + 1,
          "memory_kb is not %u to %u in steps of %u", (unsigned)kVMMinMemoryKB,
          (unsigned)kVMMaxMemoryKB, (unsigned)kVMMemoryStepKB);
      return false;
    }
    config->memory_kb = (uint16_t)memory_kb;
    return true;
  }
  uint8_t drive = 0;
  while (drive < kNumVMDrives && strcmp(key, kDriveKeys[drive]) != 0) {
    ++drive;
  }
  if (drive == kNumVMDrives) {
    // A key this firmware does not know, perhaps one a later firmware wrote,
    // is left for whatever reads it, rather than stopping the reading.
    LogIgnoredKey(config->entry.path, key);
    return true;
  }
  config->is_present[drive] = true;
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
  for (uint8_t drive = 0; drive < kNumVMDrives; ++drive) {
    config->drives[drive][0] = '\0';
    config->is_present[drive] = false;
  }
  config->video_adapter = Y_VIDEO_ADAPTER_CGA;
  config->memory_kb = kVMDefaultMemoryKB;
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

bool VMConfigSave(
    const VMEntry* vm, const char* const drives[kNumVMDrives],
    const VMMachine* machine, char problem[kVMProblemLength + 1]) {
  // Kept off the stack, with ConfigFileUpdate() under them.
  static char values[kNumVMDrives][kFSMaxPath + 2];
  char folder[kFSMaxPath + 1];
  FSFolderOf(vm->path, folder);
  char memory_kb[8];
  snprintf(memory_kb, sizeof(memory_kb), "%u", (unsigned)machine->memory_kb);
  ConfigFileSetting settings[kNumVMDrives + 2];
  for (uint8_t drive = 0; drive < kNumVMDrives; ++drive) {
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
  settings[kNumVMDrives] = (ConfigFileSetting){
      .key = kVideoAdapterKey,
      .value = VMConfigVideoAdapterName(machine->video_adapter),
  };
  settings[kNumVMDrives + 1] = (ConfigFileSetting){
      .key = kMemoryKBKey,
      .value = memory_kb,
  };
  return ConfigFileUpdate(
      vm->path, settings, sizeof(settings) / sizeof(settings[0]), problem);
}
