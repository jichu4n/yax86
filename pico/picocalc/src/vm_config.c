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

static const char* const kFloppyDriveTypeKeys[kNumVMFloppyDrives] = {
    [kVMDriveFD1] = "fd1_type",
    [kVMDriveFD2] = "fd2_type",
};

static const char* const kFloppyDriveTypeNames[Y_FDC_NUM_DRIVE_TYPES] = {
    [Y_FDC_DRIVE_TYPE_NONE] = "360k",    [Y_FDC_DRIVE_TYPE_360KB] = "360k",
    [Y_FDC_DRIVE_TYPE_720KB] = "720k",   [Y_FDC_DRIVE_TYPE_1200KB] = "1.2m",
    [Y_FDC_DRIVE_TYPE_1440KB] = "1.44m",
};

static const char* const kFloppyDriveTypeLabels[Y_FDC_NUM_DRIVE_TYPES] = {
    [Y_FDC_DRIVE_TYPE_NONE] = "360KB",    [Y_FDC_DRIVE_TYPE_360KB] = "360KB",
    [Y_FDC_DRIVE_TYPE_720KB] = "720KB",   [Y_FDC_DRIVE_TYPE_1200KB] = "1.2MB",
    [Y_FDC_DRIVE_TYPE_1440KB] = "1.44MB",
};

static const char* const kVideoAdapterNames[Y_VIDEO_NUM_ADAPTERS] = {
    [Y_VIDEO_ADAPTER_MDA] = "mda",
    [Y_VIDEO_ADAPTER_CGA] = "cga",
};

static const char* const kVideoAdapterLabels[Y_VIDEO_NUM_ADAPTERS] = {
    [Y_VIDEO_ADAPTER_MDA] = "MDA",
    [Y_VIDEO_ADAPTER_CGA] = "CGA",
};

const char* VMConfigFloppyDriveTypeName(YFDCDriveType type) {
  return kFloppyDriveTypeNames
      [type < Y_FDC_NUM_DRIVE_TYPES ? type : Y_FDC_DRIVE_TYPE_360KB];
}

const char* VMConfigFloppyDriveTypeLabel(YFDCDriveType type) {
  return kFloppyDriveTypeLabels
      [type < Y_FDC_NUM_DRIVE_TYPES ? type : Y_FDC_DRIVE_TYPE_360KB];
}

static bool FloppyDriveTypeFromName(const char* name, YFDCDriveType* type) {
  for (uint8_t i = Y_FDC_DRIVE_TYPE_360KB; i < Y_FDC_NUM_DRIVE_TYPES; ++i) {
    if (strcasecmp(name, kFloppyDriveTypeNames[i]) == 0) {
      *type = (YFDCDriveType)i;
      return true;
    }
  }
  return false;
}

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
  for (uint8_t floppy = 0; floppy < kNumVMFloppyDrives; ++floppy) {
    if (strcmp(key, kFloppyDriveTypeKeys[floppy]) == 0) {
      if (!FloppyDriveTypeFromName(value, &config->floppy_types[floppy])) {
        snprintf(
            problem, kVMProblemLength + 1,
            "%s is not 360k, 720k, 1.2m or 1.44m", key);
        return false;
      }
      config->is_present[floppy] = true;
      return true;
    }
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
  for (uint8_t floppy = 0; floppy < kNumVMFloppyDrives; ++floppy) {
    config->floppy_types[floppy] = Y_FDC_DRIVE_TYPE_360KB;
  }
  config->video_adapter = Y_VIDEO_ADAPTER_CGA;
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
    const YFDCDriveType floppy_types[kNumVMFloppyDrives],
    YVideoAdapter video_adapter, char problem[kVMProblemLength + 1]) {
  // Kept off the stack, with ConfigFileUpdate() under them.
  static char values[kNumVMDrives][kFSMaxPath + 2];
  char folder[kFSMaxPath + 1];
  FSFolderOf(vm->path, folder);
  ConfigFileSetting settings[kNumVMDrives + kNumVMFloppyDrives + 1];
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
  for (uint8_t floppy = 0; floppy < kNumVMFloppyDrives; ++floppy) {
    settings[kNumVMDrives + floppy] = (ConfigFileSetting){
        .key = kFloppyDriveTypeKeys[floppy],
        .value = drives[floppy] != NULL
                     ? VMConfigFloppyDriveTypeName(floppy_types[floppy])
                     : NULL,
    };
  }
  settings[kNumVMDrives + kNumVMFloppyDrives] = (ConfigFileSetting){
      .key = kVideoAdapterKey,
      .value = VMConfigVideoAdapterName(video_adapter),
  };
  return ConfigFileUpdate(
      vm->path, settings, sizeof(settings) / sizeof(settings[0]), problem);
}
