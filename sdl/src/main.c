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

// CPU Speed: ~4.77 MHz
// Target Instructions Per Frame (at 60 FPS):
// Approx 4,770,000 / 60 = 79,500 cycles.
// Assuming ~4-10 cycles per instruction on average for 8086.
// Let's be conservative/simple and run a fixed batch.
// 20,000 instructions per frame is a reasonable starting point for smooth operation
// without blocking the UI thread too long.
#define INSTRUCTIONS_PER_FRAME 100000

static uint8_t MainReadMemory(PlatformState* platform, uint32_t address) {
  (void)platform;
  if (address < INTERNAL_RAM_SIZE) {
    return g_memory[address];
  }
  return 0xFF;
}

static void MainWriteMemory(PlatformState* platform, uint32_t address,
                             uint8_t value) {
  (void)platform;
  if (address < INTERNAL_RAM_SIZE) {
    g_memory[address] = value;
  }
}

static uint8_t MainReadVRAM(struct MDAState* mda, uint32_t address) {
  (void)mda;
  return MainReadMemory(&g_platform, 0xB0000 + address);
}

static void MainWriteVRAM(struct MDAState* mda, uint32_t address, uint8_t value) {
  (void)mda;
  MainWriteMemory(&g_platform, 0xB0000 + address, value);
}

static void MainWritePixel(struct MDAState* mda, Position position, RGB rgb) {
  (void)mda;
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
  for (int i = 0; i < INSTRUCTIONS_PER_FRAME; ++i) {
    ExecuteStatus status = RunInstructionCycle(&g_platform.cpu);
    
    // PIT ticks at 1.19MHz, CPU at 4.77MHz.
    // 4.77 / 1.19 ~= 4.
    // Use a static counter to persist across calls.
    static uint32_t g_pit_tick_counter = 0;
    if (++g_pit_tick_counter % 4 == 0) {
        PITTick(&g_platform.pit);
    }

    if (status != kExecuteSuccess && status != kExecuteUnhandledInterrupt) {
      if (status == kExecuteHalt) {
        // CPU Halted (waiting for IRQ). Stop executing instructions for this frame, 
        // but keep the loop and hardware ticking.
        break;
      } else {
        printf("CPU Error: %d\n", status);
        g_running = false;
        break;
      }
    }

    if (i % 5000 == 0) {
      KeyboardTickMs(&g_platform.keyboard);
    }
  }

  // 3. Render
  MDARender(&g_platform.mda); // Update virtual buffer
  DisplayRender();         // Update screen
}

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  if (!DisplayInit()) {
    fprintf(stderr, "Failed to init display\n");
    return 1;
  }

  // Initialize Memory
  memset(g_memory, 0, INTERNAL_RAM_SIZE);

  // Initialize Platform
  PlatformConfig config = {0};
  config.physical_memory_size = 640 * 1024; // Use max allowed conventional memory
  config.read_physical_memory_byte = MainReadMemory;
  config.write_physical_memory_byte = MainWriteMemory;

  if (!PlatformInit(&g_platform, &config)) {
    fprintf(stderr, "Failed to init platform\n");
    DisplayQuit();
    return 1;
  }

  // Hook up video callback
  // PlatformInit initializes sub-modules. We override the MDA config callback.
  g_platform.mda_config.read_vram_byte = MainReadVRAM;
  g_platform.mda_config.write_vram_byte = MainWriteVRAM;
  g_platform.mda_config.write_pixel = MainWritePixel;

  // Initialize CPU Registers (Manual Boot)
  // Replicating PlatformBoot logic without the loop.
  g_platform.cpu.registers[kCS] = 0xF000;
  g_platform.cpu.registers[kIP] = 0xFFF0;
  g_platform.cpu.registers[kDS] = 0x0000;
  g_platform.cpu.registers[kSS] = 0x0000;
  g_platform.cpu.registers[kES] = 0x0000;
  g_platform.cpu.registers[kSP] = 0xFFFE;

#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop(MainTick, 0, 1);
#else
  while (g_running) {
    MainTick();
    // 4. Delay
    SDL_Delay(16); // ~60 FPS cap
  }
#endif

  DisplayQuit();
  return 0;
}
