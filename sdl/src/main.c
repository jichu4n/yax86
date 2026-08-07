#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "core/platform.h"
#include "core/video.h"
#include "display.h"
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

// CPU Speed: ~4.77 MHz
// Target Instructions Per Frame (at 60 FPS):
// Approx 4,770,000 / 60 = 79,500 cycles.
// Assuming ~4-10 cycles per instruction on average for 8086.
// Let's be conservative/simple and run a fixed batch.
// 20,000 instructions per frame is a reasonable starting point for smooth
// operation without blocking the UI thread too long.
#define INSTRUCTIONS_PER_FRAME 20000

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
    YAX86_UNUSED struct MDAState* mda, uint32_t address) {
  return MainReadMemory(&g_platform, 0xB0000 + address);
}

static void MainWriteVRAM(
    YAX86_UNUSED struct MDAState* mda, uint32_t address, uint8_t value) {
  MainWriteMemory(&g_platform, 0xB0000 + address, value);
}

static void MainWritePixel(
    YAX86_UNUSED struct MDAState* mda, Position position, RGB rgb) {
  DisplayPutPixel(position.x, position.y, rgb.r, rgb.g, rgb.b);
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
      InputHandleEvent(&event, &g_platform);
    }
  }

  if (!g_running) return;

  // 2. Run CPU Instructions
  for (int i = 0; i < INSTRUCTIONS_PER_FRAME && !g_cpu_stopped; ++i) {
    PlatformRunStatus status = PlatformTick(&g_platform);
    // kPlatformInvalid is deliberately not fatal here. The 8088 has no invalid
    // opcode exception, so a real machine would keep going; CPUTick() has
    // already logged it. Anything else means the machine cannot make progress,
    // so stop ticking it - but keep rendering so the final screen stays up.
    if (status != kPlatformRunning && status != kPlatformInvalid) {
      YAX86_LOG(
          &g_platform.logger, &kLogModuleApp, kLogLevelError,
          "execution stopped at %04X:%04X with status %d",
          g_platform.cpu.registers[kCS], g_platform.cpu.registers[kIP],
          (int)status);
      g_cpu_stopped = true;
    }
  }

  // 3. Render
  MDARender(&g_platform.mda);  // Update virtual buffer
  DisplayRender();             // Update screen
}

int main(YAX86_UNUSED int argc, YAX86_UNUSED char* argv[]) {
  if (!DisplayInit()) {
    fprintf(stderr, "Failed to init display\n");
    return 1;
  }

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

  if (!PlatformInit(&g_platform, &config)) {
    fprintf(stderr, "Failed to init platform\n");
    DisplayQuit();
    return 1;
  }

  YAX86_LOG(
      &g_platform.logger, &kLogModuleApp, kLogLevelDebug,
      "yax86 started with %u KB of conventional memory",
      config.physical_memory_size / 1024);

  // Hook up video callback
  // PlatformInit initializes sub-modules. We override the MDA config callback.
  g_platform.mda_config.read_vram_byte = MainReadVRAM;
  g_platform.mda_config.write_vram_byte = MainWriteVRAM;
  g_platform.mda_config.write_pixel = MainWritePixel;

#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop(MainTick, 0, 1);
#else
  while (g_running) {
    MainTick();
    // 4. Delay
    SDL_Delay(16);  // ~60 FPS cap
  }
#endif

  DisplayQuit();
  return 0;
}
