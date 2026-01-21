#include "input.h"

#include <stdio.h>

#include "core/keyboard.h"

// Mapping from SDL keycodes to PC/XT scancodes.
// This is a minimal mapping.
static uint8_t SDLToXTScancode(SDL_Keycode key) {
  switch (key) {
    case SDLK_ESCAPE:
      return 0x01;
    case SDLK_1:
      return 0x02;
    case SDLK_2:
      return 0x03;
    case SDLK_3:
      return 0x04;
    case SDLK_4:
      return 0x05;
    case SDLK_5:
      return 0x06;
    case SDLK_6:
      return 0x07;
    case SDLK_7:
      return 0x08;
    case SDLK_8:
      return 0x09;
    case SDLK_9:
      return 0x0A;
    case SDLK_0:
      return 0x0B;
    case SDLK_MINUS:
      return 0x0C;
    case SDLK_EQUALS:
      return 0x0D;
    case SDLK_BACKSPACE:
      return 0x0E;
    case SDLK_TAB:
      return 0x0F;
    case SDLK_Q:
      return 0x10;
    case SDLK_W:
      return 0x11;
    case SDLK_E:
      return 0x12;
    case SDLK_R:
      return 0x13;
    case SDLK_T:
      return 0x14;
    case SDLK_Y:
      return 0x15;
    case SDLK_U:
      return 0x16;
    case SDLK_I:
      return 0x17;
    case SDLK_O:
      return 0x18;
    case SDLK_P:
      return 0x19;
    case SDLK_LEFTBRACKET:
      return 0x1A;
    case SDLK_RIGHTBRACKET:
      return 0x1B;
    case SDLK_RETURN:
      return 0x1C;
    case SDLK_LCTRL:
      return 0x1D;
    case SDLK_A:
      return 0x1E;
    case SDLK_S:
      return 0x1F;
    case SDLK_D:
      return 0x20;
    case SDLK_F:
      return 0x21;
    case SDLK_G:
      return 0x22;
    case SDLK_H:
      return 0x23;
    case SDLK_J:
      return 0x24;
    case SDLK_K:
      return 0x25;
    case SDLK_L:
      return 0x26;
    case SDLK_SEMICOLON:
      return 0x27;
    case SDLK_APOSTROPHE:
      return 0x28;
    case SDLK_GRAVE:
      return 0x29;
    case SDLK_LSHIFT:
      return 0x2A;
    case SDLK_BACKSLASH:
      return 0x2B;
    case SDLK_Z:
      return 0x2C;
    case SDLK_X:
      return 0x2D;
    case SDLK_C:
      return 0x2E;
    case SDLK_V:
      return 0x2F;
    case SDLK_B:
      return 0x30;
    case SDLK_N:
      return 0x31;
    case SDLK_M:
      return 0x32;
    case SDLK_COMMA:
      return 0x33;
    case SDLK_PERIOD:
      return 0x34;
    case SDLK_SLASH:
      return 0x35;
    case SDLK_RSHIFT:
      return 0x36;
    case SDLK_PRINTSCREEN:
      return 0x37;
    case SDLK_LALT:
      return 0x38;
    case SDLK_SPACE:
      return 0x39;
    case SDLK_CAPSLOCK:
      return 0x3A;
    case SDLK_F1:
      return 0x3B;
    case SDLK_F2:
      return 0x3C;
    case SDLK_F3:
      return 0x3D;
    case SDLK_F4:
      return 0x3E;
    case SDLK_F5:
      return 0x3F;
    case SDLK_F6:
      return 0x40;
    case SDLK_F7:
      return 0x41;
    case SDLK_F8:
      return 0x42;
    case SDLK_F9:
      return 0x43;
    case SDLK_F10:
      return 0x44;
    case SDLK_NUMLOCKCLEAR:
      return 0x45;
    case SDLK_SCROLLLOCK:
      return 0x46;
    case SDLK_KP_7:
      return 0x47;
    case SDLK_KP_8:
      return 0x48;
    case SDLK_KP_9:
      return 0x49;
    case SDLK_KP_MINUS:
      return 0x4A;
    case SDLK_KP_4:
      return 0x4B;
    case SDLK_KP_5:
      return 0x4C;
    case SDLK_KP_6:
      return 0x4D;
    case SDLK_KP_PLUS:
      return 0x4E;
    case SDLK_KP_1:
      return 0x4F;
    case SDLK_KP_2:
      return 0x50;
    case SDLK_KP_3:
      return 0x51;
    case SDLK_KP_0:
      return 0x52;
    case SDLK_KP_PERIOD:
      return 0x53;
    case SDLK_F11:
      return 0x57;
    case SDLK_F12:
      return 0x58;

    // Extended keys (using standard scancodes, often prefixed by E0 in real
    // hardware but here passing simplified make codes if supported or just
    // basic ones) For simplicity, mapping to primary block or similar if
    // appropriate. Up/Down/Left/Right are often E0 prefixed. XT standard: Up:
    // 48 (same as KP 8) Left: 4B (same as KP 4) Right: 4D (same as KP 6) Down:
    // 50 (same as KP 2)
    case SDLK_UP:
      return 0x48;
    case SDLK_LEFT:
      return 0x4B;
    case SDLK_RIGHT:
      return 0x4D;
    case SDLK_DOWN:
      return 0x50;

    default:
      return 0;
  }
}

void InputHandleEvent(const SDL_Event* event, PlatformState* platform) {
  if (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) {
    uint8_t scancode = SDLToXTScancode(event->key.key);
    if (scancode) {
      if (event->type == SDL_EVENT_KEY_UP) {
        scancode |= 0x80;  // Break code
      }
      printf(
          "Key %s: SDL keycode=%d, scancode=0x%02X\n",
          event->type == SDL_EVENT_KEY_DOWN ? "DOWN" : "UP", event->key.key,
          scancode);
      KeyboardHandleKeyPress(&platform->keyboard, scancode);
    }
  }
}
