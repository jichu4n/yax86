#include "display.h"

#include <SDL3/SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <stdio.h>
#endif

static SDL_Window* g_window = NULL;
static SDL_Renderer* g_renderer = NULL;
static SDL_Texture* g_texture = NULL;

// Pixel buffer for the screen.
// Format: ARGB8888 (or whatever SDL prefers, we'll map RGBA).
static uint32_t* g_pixel_buffer = NULL;
static int g_width = 0;
static int g_height = 0;
static SDL_Rect g_update_rect = {0};
static bool g_region_open = false;
static bool g_frame_updated = false;

enum {
  // Frame buffers no taller than this are line doubled in the window.
  kLineDoubledHeight = 200,
};

bool DisplayInit(int width, int height) {
  g_width = width;
  g_height = height;
  g_region_open = false;
  g_frame_updated = false;

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init(SDL_INIT_VIDEO) failed: %s", SDL_GetError());
    return false;
  }

  // The CGA's 200 line modes are line doubled, as they are on real hardware,
  // so that the window is not half the height of the MDA's.
  int original_scale_y = g_height <= kLineDoubledHeight ? 4 : 2;
  int target_width = (g_width * 3) / 2;
  int target_height = (g_height * original_scale_y * 3) / 4;

#ifdef __EMSCRIPTEN__
  char script[256];
  snprintf(
      script, sizeof(script),
      "var c = document.getElementById('canvas'); if(c) { "
      "c.style.width='%dpx'; c.style.height='%dpx'; c.width=%d; c.height=%d; }",
      target_width, target_height, target_width, target_height);
  emscripten_run_script(script);
#endif

  if (!SDL_CreateWindowAndRenderer(
          "yax86", target_width, target_height, 0, &g_window, &g_renderer)) {
    SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
    return false;
  }

  g_texture = SDL_CreateTexture(
      g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
      g_width, g_height);
  if (!g_texture) {
    SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
    return false;
  }

  g_pixel_buffer = (uint32_t*)SDL_malloc(g_width * g_height * sizeof(uint32_t));
  if (!g_pixel_buffer) {
    SDL_Log("Malloc failed for pixel buffer");
    return false;
  }

  // Clear buffer to black
  SDL_memset(g_pixel_buffer, 0, g_width * g_height * sizeof(uint32_t));

  return true;
}

void DisplayQuit(void) {
  if (g_pixel_buffer) {
    SDL_free(g_pixel_buffer);
    g_pixel_buffer = NULL;
  }
  if (g_texture) {
    SDL_DestroyTexture(g_texture);
    g_texture = NULL;
  }
  if (g_renderer) {
    SDL_DestroyRenderer(g_renderer);
    g_renderer = NULL;
  }
  if (g_window) {
    SDL_DestroyWindow(g_window);
    g_window = NULL;
  }
  SDL_Quit();
}

void DisplayBeginRegion(int x, int y, int width, int height) {
  g_update_rect = (SDL_Rect){.x = x, .y = y, .w = width, .h = height};
  g_region_open = true;
}

void DisplayEndRegion(void) {
  if (!g_region_open || !g_texture || !g_pixel_buffer) {
    return;
  }
  const uint32_t* pixels =
      &g_pixel_buffer[g_update_rect.y * g_width + g_update_rect.x];
  if (!SDL_UpdateTexture(
          g_texture, &g_update_rect, pixels, g_width * sizeof(uint32_t))) {
    SDL_Log("SDL_UpdateTexture failed: %s", SDL_GetError());
  } else {
    g_frame_updated = true;
  }
  g_region_open = false;
}

void DisplayRender(void) {
  if (!g_renderer || !g_texture || !g_pixel_buffer || !g_frame_updated) {
    return;
  }

  SDL_RenderClear(g_renderer);
  SDL_RenderTexture(g_renderer, g_texture, NULL, NULL);
  SDL_RenderPresent(g_renderer);
  g_frame_updated = false;
}

void DisplayPutPixels(int x, int y, const RGB* pixels, uint8_t count) {
  if (x < 0 || x >= g_width || y < 0 || y >= g_height) {
    return;
  }
  // Clip a span that runs past the right edge rather than dropping it whole.
  // The core never emits one - the frame buffer is sized from the adapter and
  // every renderer stops exactly at its width - but losing up to a batch of
  // otherwise valid pixels would leave a gap that the region upload then
  // presents as stale content.
  if (x + count > g_width) {
    count = (uint8_t)(g_width - x);
  }
  uint32_t* destination = &g_pixel_buffer[y * g_width + x];
  for (uint8_t i = 0; i < count; ++i) {
    RGB rgb = pixels[i];
    // ARGB8888: A=255, R, G, B
    destination[i] = (0xFF << 24) | ((uint32_t)rgb.r << 16) |
                     ((uint32_t)rgb.g << 8) | (uint32_t)rgb.b;
  }
}
