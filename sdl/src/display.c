#include "display.h"

#include <SDL3/SDL.h>

#include "core/video.h"

static SDL_Window* g_window = NULL;
static SDL_Renderer* g_renderer = NULL;
static SDL_Texture* g_texture = NULL;

// Pixel buffer for the screen.
// Format: ARGB8888 (or whatever SDL prefers, we'll map RGBA).
static uint32_t* g_pixel_buffer = NULL;
static int g_width = 0;
static int g_height = 0;

bool DisplayInit(void) {
  g_width = kMDAModeMetadata.width;
  g_height = kMDAModeMetadata.height;

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init(SDL_INIT_VIDEO) failed: %s", SDL_GetError());
    return false;
  }

  if (!SDL_CreateWindowAndRenderer("yax86", g_width * 2, g_height * 2, 0,
                                   &g_window, &g_renderer)) {
    SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
    return false;
  }

  g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, g_width, g_height);
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

void DisplayRender(void) {
  if (!g_renderer || !g_texture || !g_pixel_buffer) {
    return;
  }

  SDL_UpdateTexture(g_texture, NULL, g_pixel_buffer,
                    g_width * sizeof(uint32_t));
  SDL_RenderClear(g_renderer);
  SDL_RenderTexture(g_renderer, g_texture, NULL, NULL);
  SDL_RenderPresent(g_renderer);
}

void DisplayPutPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  if (x < 0 || x >= g_width || y < 0 || y >= g_height) {
    return;
  }
  // ARGB8888: A=255, R, G, B
  uint32_t color =
      (0xFF << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
  g_pixel_buffer[y * g_width + x] = color;
}
