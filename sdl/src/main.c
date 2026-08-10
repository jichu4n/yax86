#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "core/platform.h"
#include "core/video.h"
#include "audio.h"
#include "display.h"
#include "floppy.h"
#include "harddisk.h"
#include "input.h"

// 1MB of internal address space (covers conventional memory + video RAM + BIOS)
#define INTERNAL_RAM_SIZE (1024 * 1024)
static uint8_t g_memory[INTERNAL_RAM_SIZE];
static PlatformState g_platform;
static bool g_running = true;
// Set once the emulated machine can no longer make progress. The window stays
// open and keeps rendering, but the platform is no longer ticked.
static bool g_cpu_stopped = false;

// Log module for the SDL runtime itself.
enum {
  kLogModuleIDApp = 16,
};
static const LogModule kLogModuleApp = {
    .id = kLogModuleIDApp,
    .name = "APP",
};

static LoggerConfig g_logger_config;

static const char* MainLogLevelName(LogLevel level) {
  switch (level) {
    case kLogLevelError:
      return "ERROR";
    case kLogLevelWarn:
      return "WARN";
    case kLogLevelDebug:
      return "DEBUG";
    default:
      return "?";
  }
}

// Log sink. Under Emscripten, stdout is routed to the browser console.
static void MainWriteLogLine(
    YAX86_UNUSED void* context, const LogModule* module, LogLevel level,
    uint64_t tick, const char* message, YAX86_UNUSED size_t length) {
  printf(
      "[%llu] %-5s %-8s %s\n", (unsigned long long)tick,
      MainLogLevelName(level), module->name, message);
}

static uint64_t MainGetTick(YAX86_UNUSED void* context) {
  return g_platform.ticks;
}

// How much guest time to run per frame, in CPU cycles. A frame is a sixtieth
// of a second, so this is what a 4.77MHz 8088 gets through in that time, and
// running it keeps the emulated machine at roughly the speed of the real one.
#define CYCLES_PER_FRAME (kCPUCyclesPerSecond / 60)

static uint8_t MainReadMemory(
    YAX86_UNUSED PlatformState* platform, uint32_t address) {
  if (address < INTERNAL_RAM_SIZE) {
    return g_memory[address];
  }
  return 0xFF;
}

static void MainWriteMemory(
    YAX86_UNUSED PlatformState* platform, uint32_t address, uint8_t value) {
  if (address < INTERNAL_RAM_SIZE) {
    g_memory[address] = value;
  }
}

static uint8_t MainReadVRAM(
    YAX86_UNUSED struct VideoState* video, uint32_t address) {
  return MainReadMemory(&g_platform, kMDAModeMetadata.vram_address + address);
}

static void MainWriteVRAM(
    YAX86_UNUSED struct VideoState* video, uint32_t address, uint8_t value) {
  MainWriteMemory(&g_platform, kMDAModeMetadata.vram_address + address, value);
}

static void MainWritePixel(
    YAX86_UNUSED struct VideoState* video, Position position, RGB rgb) {
  DisplayPutPixel(position.x, position.y, rgb.r, rgb.g, rgb.b);
}

static void MainSetPCSpeakerFrequency(
    YAX86_UNUSED PlatformState* platform, uint32_t frequency_hz) {
  AudioSetFrequency(frequency_hz);
}

void MainTick(void) {
  SDL_Event event;

  // 1. Process Events
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_QUIT) {
      g_running = false;
#ifdef __EMSCRIPTEN__
      emscripten_cancel_main_loop();
#endif
    } else {
      // Browsers refuse to start audio until the user has interacted with the
      // page, so a beep during POST would be dropped and the device would stay
      // suspended. Any input is the gesture that lets it start.
      AudioResume();
      InputHandleEvent(&event, &g_platform);
    }
  }

  if (!g_running) return;

  // 2. Run CPU Instructions
  uint32_t remaining_ticks = CYCLES_PER_FRAME;
  while (!g_cpu_stopped && remaining_ticks > 0) {
    const uint32_t start_tick = g_platform.ticks;
    const PlatformRunStatus status = PlatformRun(&g_platform, remaining_ticks);
    // Unsigned subtraction, so this stays correct across a tick counter wrap.
    const uint32_t consumed_ticks = g_platform.ticks - start_tick;
    remaining_ticks -=
        consumed_ticks < remaining_ticks ? consumed_ticks : remaining_ticks;

    if (status == kPlatformRunning) {
      // Ran the frame's full budget without stopping.
      break;
    }
    if (status == kPlatformInvalid) {
      // Not fatal here. The 8088 has no invalid opcode exception, so a real
      // machine would keep going, and CPUTick() has already logged it. The
      // offending tick was counted, so resuming makes progress.
      continue;
    }
    // Anything else means the machine cannot make progress. Stop ticking it,
    // but keep rendering so the final screen stays up.
    YAX86_LOG(
        &g_platform.logger, &kLogModuleApp, kLogLevelError,
        "execution stopped at %04X:%04X with status %d",
        g_platform.cpu.registers[kCS], g_platform.cpu.registers[kIP],
        (int)status);
    g_cpu_stopped = true;
  }

  // 3. Render
  VideoRender(&g_platform.video);  // Update virtual buffer
  DisplayRender();                 // Update screen
}

int main(int argc, char* argv[]) {
  const char* floppy_path = kDefaultFloppyImagePath;
  // The machine has a hard disk unless asked otherwise. Under Emscripten there
  // is no command line at all, so anything gated behind a flag would be
  // unreachable in the browser.
  bool attach_hard_disk = true;
  // NULL means a blank disk, which is what the default gives.
  const char* hard_disk_path = NULL;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--hdd") == 0) {
      attach_hard_disk = true;
      // An image path may follow, but --hdd on its own is a blank disk.
      if (i + 1 < argc && argv[i + 1][0] != '-') {
        hard_disk_path = argv[++i];
      }
    } else if (strcmp(argv[i], "--no-hdd") == 0) {
      attach_hard_disk = false;
    } else if (argv[i][0] == '-') {
      fprintf(stderr, "Unknown option '%s'\n", argv[i]);
      fprintf(
          stderr, "Usage: %s [floppy image] [--hdd [image]] [--no-hdd]\n",
          argv[0]);
      return 1;
    } else {
      floppy_path = argv[i];
    }
  }

  if (!DisplayInit()) {
    fprintf(stderr, "Failed to init display\n");
    return 1;
  }

  // A machine with no sound card still boots, so audio failing is not fatal.
  const bool audio_available = AudioInit();

  // Initialize Memory
  memset(g_memory, 0, INTERNAL_RAM_SIZE);

  // Initialize logging. All modules are enabled, but only errors are shown
  // by default - raise min_level to kLogLevelWarn or kLogLevelDebug to see
  // more, or narrow enabled_modules to a specific module.
  g_logger_config.write_line = MainWriteLogLine;
  g_logger_config.get_tick = MainGetTick;
  g_logger_config.enabled_modules = 0xFFFFFFFF;
  g_logger_config.min_level = kLogLevelError;

  // Initialize Platform
  PlatformConfig config = {0};
  config.logger_config = &g_logger_config;
  config.physical_memory_size =
      640 * 1024;  // Use max allowed conventional memory
  config.read_physical_memory_byte = MainReadMemory;
  config.write_physical_memory_byte = MainWriteMemory;
  if (audio_available) {
    config.set_pc_speaker_frequency = MainSetPCSpeakerFrequency;
  }

  if (!PlatformInit(&g_platform, &config)) {
    fprintf(stderr, "Failed to init platform\n");
    AudioQuit();
    DisplayQuit();
    return 1;
  }

  YAX86_LOG(
      &g_platform.logger, &kLogModuleApp, kLogLevelDebug,
      "yax86 started with %u KB of conventional memory",
      config.physical_memory_size / 1024);

  // Mount the boot floppy. Without one the BIOS still runs, so a failure here
  // is reported but not fatal.
  if (FloppyMount(&g_platform, floppy_path)) {
    YAX86_LOG(
        &g_platform.logger, &kLogModuleApp, kLogLevelDebug,
        "mounted %s in floppy drive A", floppy_path);
  } else {
    YAX86_LOG(
        &g_platform.logger, &kLogModuleApp, kLogLevelError,
        "floppy drive A is empty - could not mount %s", floppy_path);
  }

  if (attach_hard_disk) {
    if (HardDiskAttach(&g_platform, hard_disk_path)) {
      YAX86_LOG(
          &g_platform.logger, &kLogModuleApp, kLogLevelDebug,
          "attached a %u MB hard disk (%s)",
          (unsigned)(kHDCGeometry10MBImageSize / (1024 * 1024)),
          hard_disk_path ? hard_disk_path : "blank");
    } else {
      YAX86_LOG(
          &g_platform.logger, &kLogModuleApp, kLogLevelError,
          "no hard disk attached - could not mount %s",
          hard_disk_path ? hard_disk_path : "blank disk");
    }
  }

  // Hook up video callback
  // PlatformInit initializes sub-modules. We override the video config
  // callbacks.
  g_platform.video_config.read_vram_byte = MainReadVRAM;
  g_platform.video_config.write_vram_byte = MainWriteVRAM;
  g_platform.video_config.write_pixel = MainWritePixel;

#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop(MainTick, 0, 1);
#else
  while (g_running) {
    MainTick();
    // 4. Delay
    SDL_Delay(16);  // ~60 FPS cap
  }
#endif

  AudioQuit();
  DisplayQuit();
  return 0;
}
