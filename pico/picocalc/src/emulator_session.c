#include "emulator_session.h"

#include <stddef.h>
#include <stdio.h>

#include "disk_image.h"
#include "display.h"
#include "emulator_status_bar.h"
#include "fs.h"
#include "runtime_log.h"

// Each floppy drive's name in the logs.
static const char* const kDriveNames[kFloppyDrives] = {
    [kFloppyDriveA] = "fd1",
    [kFloppyDriveB] = "fd2",
};

uint8_t EmulatorSessionNumFloppyDrives(const EmulatorSession* session) {
  return session->config != NULL && !session->config->present[kVMDriveFD2]
             ? 1
             : kFloppyDrives;
}

bool EmulatorSessionCanPowerOff(const EmulatorSession* session) {
  return session->config != NULL;
}

// Save what a floppy drive now holds to the VM's vm.cfg, so that the next
// start has it. Without a VM there is nothing to save to.
static void SaveFloppy(EmulatorSession* session, FloppyDrive drive) {
  if (session->config == NULL) {
    return;
  }
  const DiskImage* image = FloppyGetImage(drive);
  const char* path = image->status == kDiskImageOpen ? image->path : "";
  char problem[kVMProblemLength + 1];
  if (!VMConfigSaveDrive(session->config, kVMDriveFD1 + drive, path, problem)) {
    char message[kTextColumns + 1];
    snprintf(
        message, sizeof(message), "Could not save %s: %.50s",
        kVMDriveLabels[drive], problem);
    EmulatorStatusBarPostMessage(message);
    SYSTEM_WARN(
        "drive_not_saved", Y_LOG_STRING("drive", kDriveNames[drive]),
        Y_LOG_STRING("problem", problem));
    return;
  }
  FSCopyPath(path, session->config->drives[kVMDriveFD1 + drive]);
  SYSTEM_INFO(
      "drive_saved", Y_LOG_STRING("drive", kDriveNames[drive]),
      Y_LOG_STRING("path", path));
}

void EmulatorSessionEjectFloppy(EmulatorSession* session, FloppyDrive drive) {
  FloppyEject(session->fdc, drive);
  char message[kTextColumns + 1];
  snprintf(
      message, sizeof(message), "Ejected the disk from %s",
      kVMDriveLabels[drive]);
  EmulatorStatusBarPostMessage(message);
  DISK_INFO("ejected", Y_LOG_STRING("drive", kDriveNames[drive]));
  SaveFloppy(session, drive);
}

void EmulatorSessionInsertFloppy(
    EmulatorSession* session, FloppyDrive drive, const char* path) {
  char name[kDiskImageNameLength + 1];
  DiskImageName(path, name);
  char message[kTextColumns + 1];
  const FloppyDrive holding = FloppyDriveHolding(path);
  if (holding != kFloppyDrives) {
    snprintf(
        message, sizeof(message), "%s is already in %s", name,
        kVMDriveLabels[holding]);
    EmulatorStatusBarPostMessage(message);
    DISK_WARN(
        "insert_failed", Y_LOG_STRING("drive", kDriveNames[drive]),
        Y_LOG_STRING("path", path), Y_LOG_STRING("error", "in_other_drive"));
    return;
  }
  const DiskImageStatus status = FloppyInsertImage(session->fdc, drive, path);
  if (status == kDiskImageOpen) {
    snprintf(
        message, sizeof(message), "Inserted %s in %s", name,
        kVMDriveLabels[drive]);
    DISK_INFO(
        "inserted", Y_LOG_STRING("drive", kDriveNames[drive]),
        Y_LOG_STRING("path", path));
  } else {
    snprintf(
        message, sizeof(message), "Could not insert %s: %s", name,
        DiskImageDescribeStatus(status));
    DISK_WARN(
        "insert_failed", Y_LOG_STRING("drive", kDriveNames[drive]),
        Y_LOG_STRING("path", path),
        Y_LOG_STRING("error", DiskImageDescribeStatus(status)));
  }
  EmulatorStatusBarPostMessage(message);
  // The drive is empty after an insert that failed.
  SaveFloppy(session, drive);
}
