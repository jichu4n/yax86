#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "app_log.h"
#include "audio.h"
#include "core/platform.h"
#include "core/video.h"
#include "display.h"
#include "floppy.h"
#include "harddisk.h"
#include "input.h"

// 1MB of internal address space (covers conventional memory + video RAM + BIOS)
#define INTERNAL_RAM_SIZE (1024 * 1024)
static uint8_t g_memory[INTERNAL_RAM_SIZE];
static YPlatformState g_platform;
static bool g_running = true;
// Set once the emulated machine can no longer make progress. The window stays
// open and keeps rendering, but the platform is no longer ticked.
static bool g_cpu_stopped = false;

const YLogModule kAppLogModule = {
    .id = Y_LOG_FIRST_HOST_MODULE_ID,
    .name = "sdl",
};
YLogger g_app_logger;

static YLoggerConfig g_logger_config;

// Log sink. Under Emscripten, stdout is routed to the browser console.
static void MainWriteLogLine(
    Y_UNUSED void* context, Y_UNUSED YLogKind kind,
    Y_UNUSED const YLogModule* module, Y_UNUSED YLogLevel level,
    const char* line, Y_UNUSED size_t length) {
  printf("%s\n", line);
}

enum {
  // Nanoseconds in a second, for turning elapsed real time into guest cycles.
  kNanosecondsPerSecond = 1000000000,

  // The most guest time one pass may run, in nanoseconds. A gap longer than
  // this - a backgrounded tab, a stall - is dropped rather than made up, so
  // that coming back does not turn into a burst of catch-up emulation.
  kMaxCatchUpNs = kNanosecondsPerSecond / 10,

  // The least time that must have passed before there is anything worth doing.
  //
  // The emulated display refreshes sixty times a second, so stepping the
  // machine faster than that draws nothing new - and it is not free. With idle
  // skipping on, a pass costs about one trip round the guest's idle loop
  // whether it was handed a sixtieth of a second of guest time or a
  // two-hundredth, so an idle machine's cost tracks how often we are called
  // rather than how much time we run. requestAnimationFrame is meant to be the
  // display's rate, but on a machine whose compositor has no vsync to lock to
  // it can fire three times as often.
  //
  // Slightly under a sixtieth of a second, so that a callback arriving a hair
  // early is trimmed rather than deferred to the next one - deferring would
  // halve the rate instead of capping it.
  kMinStepNs = kNanosecondsPerSecond / 70,
};

// The video adapter installed in the emulated machine, and its metadata.
static YVideoAdapter g_video_adapter = Y_VIDEO_ADAPTER_CGA;
static const YVideoAdapterMetadata* g_video_adapter_metadata = NULL;

static void MainWritePixels(
    Y_UNUSED struct YVideoState* video, YVideoPosition origin,
    const YVideoRGB* pixels, uint8_t count) {
  DisplayPutPixels(origin.x, origin.y, pixels, count);
}

static void MainBeginRenderRegion(
    Y_UNUSED struct YVideoState* video, YVideoRegion region) {
  DisplayBeginRegion(
      region.origin.x, region.origin.y, region.width, region.height);
}

static void MainEndRenderRegion(Y_UNUSED struct YVideoState* video) {
  DisplayEndRegion();
}

static void MainSetPCSpeakerFrequency(
    Y_UNUSED YPlatformState* platform, uint32_t frequency_hz) {
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
  //
  // How much guest time to run comes from the wall clock, not from how often
  // the host calls us. Under Emscripten that is requestAnimationFrame, which
  // runs at whatever the display does - sixty times a second on one machine
  // and a hundred and forty on another - so a fixed budget per callback would
  // run the machine at whatever multiple of real time the display happens to
  // be, and the guest's clock along with it.
  static uint64_t last_ns = 0;
  const uint64_t now_ns = SDL_GetTicksNS();
  // Called again before there is a step's worth of time to run. Events have
  // been pumped, which is what keeps input responsive; there is nothing else
  // to do until the clock catches up.
  if (last_ns != 0 && now_ns - last_ns < kMinStepNs) {
    return;
  }
  uint64_t elapsed_ns = last_ns == 0 ? 0 : now_ns - last_ns;
  last_ns = now_ns;
  if (elapsed_ns > kMaxCatchUpNs) {
    elapsed_ns = kMaxCatchUpNs;
  }
  uint32_t remaining_ticks =
      (uint32_t)(elapsed_ns * Y_PLATFORM_CPU_CYCLES_PER_SECOND /
                 kNanosecondsPerSecond);
  while (!g_cpu_stopped && remaining_ticks > 0) {
    const uint32_t start_tick = g_platform.ticks;
    const YPlatformRunStatus status =
        YPlatformRun(&g_platform, remaining_ticks);
    // Unsigned subtraction, so this stays correct across a tick counter wrap.
    const uint32_t consumed_ticks = g_platform.ticks - start_tick;
    remaining_ticks -=
        consumed_ticks < remaining_ticks ? consumed_ticks : remaining_ticks;

    if (status == Y_PLATFORM_RUNNING) {
      // Ran the frame's full budget without stopping.
      break;
    }
    if (status == Y_PLATFORM_INVALID) {
      // Not fatal here. The 8088 has no invalid opcode exception, so a real
      // machine would keep going, and YCPUTick() has already logged it. The
      // offending tick was counted, so resuming makes progress.
      continue;
    }
    // Anything else means the machine cannot make progress. Stop ticking it,
    // but keep rendering so the final screen stays up.
    // Through the platform's logger, which adds where the CPU stopped.
    Y_LOG(
        &g_platform.logger, &kAppLogModule, Y_LOG_LEVEL_ERROR,
        "execution_stopped", Y_LOG_INT("status", status));
    g_cpu_stopped = true;
  }

  // 3. Render
  // Bring the CRT beam up to the current cycle before drawing from it. The
  // platform advances devices only when they have something to do, so without
  // this the blink phase would lag by however long the frame took.
  YPlatformSync(&g_platform);
  YVideoRender(&g_platform.video);  // Update virtual buffer
  DisplayRender();                  // Update screen
}

int main(int argc, char* argv[]) {
  // Logging comes first, so that anything after it can log. All modules are
  // enabled, but only errors are shown by default - raise min_level to
  // Y_LOG_LEVEL_WARN, Y_LOG_LEVEL_INFO or Y_LOG_LEVEL_DEBUG to see more, or
  // narrow enabled_modules to a specific module.
  g_logger_config.write_line = MainWriteLogLine;
  g_logger_config.enabled_modules = 0xFFFFFFFF;
  g_logger_config.min_level = Y_LOG_LEVEL_ERROR;
  YLoggerInit(&g_app_logger, &g_logger_config);

  const char* floppy_path = kDefaultFloppyImagePath;
  // The machine has a hard disk unless asked otherwise. Under Emscripten there
  // is no command line at all, so anything gated behind a flag would be
  // unreachable in the browser.
  bool attach_hard_disk = true;
  // NULL means a blank disk, which is what the default gives.
  const char* hard_disk_path = NULL;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--cga") == 0) {
      g_video_adapter = Y_VIDEO_ADAPTER_CGA;
    } else if (strcmp(argv[i], "--mda") == 0) {
      g_video_adapter = Y_VIDEO_ADAPTER_MDA;
    } else if (strcmp(argv[i], "--hdd") == 0) {
      attach_hard_disk = true;
      // An image path may follow, but --hdd on its own is a blank disk.
      if (i + 1 < argc && argv[i + 1][0] != '-') {
        hard_disk_path = argv[++i];
      }
    } else if (strcmp(argv[i], "--no-hdd") == 0) {
      attach_hard_disk = false;
    } else if (argv[i][0] == '-') {
      APP_ERROR("unknown_option", Y_LOG_STRING("option", argv[i]));
      fprintf(
          stderr,
          "Usage: %s [floppy image] [--mda|--cga] [--hdd [image]] "
          "[--no-hdd]\n",
          argv[0]);
      return 1;
    } else {
      floppy_path = argv[i];
    }
  }
  g_video_adapter_metadata = &YVideoAdapterMetadataTable[g_video_adapter];

  if (!DisplayInit(
          g_video_adapter_metadata->frame_buffer_width,
          g_video_adapter_metadata->frame_buffer_height)) {
    APP_ERROR("display_init_failed");
    return 1;
  }

  // A machine with no sound card still boots, so audio failing is not fatal.
  const bool audio_available = AudioInit();

  // Initialize Memory
  memset(g_memory, 0, INTERNAL_RAM_SIZE);

  // Initialize Platform. The caller zeroes the state and fills in its config;
  // nothing inside zeroes anything, because every module's state is a field of
  // this one.
  static const YPlatformState kEmptyPlatform = {0};
  g_platform = kEmptyPlatform;
  YPlatformConfig* config = &g_platform.config;
  config->logger_config = &g_logger_config;
  config->direct_memory_size =
      640 * 1024;  // Use max allowed conventional memory
  config->direct_memory = g_memory;
  // Video memory is a window into the same array, above conventional memory.
  config->vram = g_memory + g_video_adapter_metadata->vram_address;
  if (audio_available) {
    config->set_pc_speaker_frequency = MainSetPCSpeakerFrequency;
  }
  config->video_adapter = g_video_adapter;

  if (!YPlatformInit(&g_platform)) {
    APP_ERROR("platform_init_failed");
    AudioQuit();
    DisplayQuit();
    return 1;
  }
  // An idle DOS prompt otherwise costs a full frame's worth of emulation to
  // produce nothing, because DOS waits for a keystroke by polling rather than
  // halting. This machine has a host to be a good citizen on - in the browser
  // it is a background tab's share of a laptop - so it takes the skip.
  YPlatformSetDOSIdleSkip(&g_platform, true);

  APP_DEBUG(
      "started", Y_LOG_UINT("memory_kb", config->direct_memory_size / 1024));

  // Mount the boot floppy. Without one the BIOS still runs, so a failure here
  // is reported but not fatal.
  if (FloppyMount(&g_platform, floppy_path)) {
    APP_DEBUG("floppy_mounted", Y_LOG_STRING("path", floppy_path));
  }

  if (attach_hard_disk) {
    if (HardDiskAttach(&g_platform, hard_disk_path)) {
      APP_DEBUG(
          "hard_disk_attached",
          Y_LOG_STRING("path", hard_disk_path ? hard_disk_path : ""),
          Y_LOG_BOOL("blank", hard_disk_path == NULL));
    }
  }

  // YPlatformInit initializes sub-modules, including video. Video memory comes
  // from the config, but the display is ours, so hook up the pixel sink here.
  g_platform.video.config.write_pixels = MainWritePixels;
  g_platform.video.config.begin_render_region = MainBeginRenderRegion;
  g_platform.video.config.end_render_region = MainEndRenderRegion;

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
