#include "video_test_helper.h"

#include <fstream>
#include <iomanip>

using namespace std;

VideoTestHelper::VideoTestHelper() {
  config_.context = this;
  config_.memory_size_kb = 16;
  config_.mda_config = kDefaultMDAConfig;
  config_.read_memory_byte = [](BIOSState* bios, uint32_t address) -> uint8_t {
    VideoTestHelper* helper =
        static_cast<VideoTestHelper*>(bios->config->context);
    if (address >= sizeof(memory_)) {
      return 0xFF;
    }
    return helper->memory_[address];
  };
  config_.write_memory_byte = [](BIOSState* bios, uint32_t address,
                                 uint8_t value) {
    VideoTestHelper* helper =
        static_cast<VideoTestHelper*>(bios->config->context);
    if (address >= sizeof(memory_)) {
      return;
    }
    helper->memory_[address] = value;
  };
  config_.read_vram_byte = [](BIOSState* bios, uint32_t address) -> uint8_t {
    VideoTestHelper* helper =
        static_cast<VideoTestHelper*>(bios->config->context);
    if (address >= sizeof(vram_)) {
      return 0xFF;
    }
    return helper->vram_[address];
  };
  config_.write_vram_byte = [](BIOSState* bios, uint32_t address,
                               uint8_t value) {
    VideoTestHelper* helper =
        static_cast<VideoTestHelper*>(bios->config->context);
    if (address >= sizeof(vram_)) {
      return;
    }
    helper->vram_[address] = value;
  };
  config_.write_pixel = [](BIOSState* bios, Position position, RGB rgb) {
    VideoTestHelper* helper =
        static_cast<VideoTestHelper*>(bios->config->context);
    const VideoModeMetadata* metadata = GetCurrentVideoModeMetadata(bios);
    size_t index = position.y * metadata->width + position.x;
    if (index >= sizeof(framebuffer_) / sizeof(RGB)) {
      return;
    }
    helper->framebuffer_[index] = rgb;
  };
  InitBIOS(&bios_state_, &config_);
}

bool VideoTestHelper::RenderToPPM(const string& file_path) {
  const VideoModeMetadata* metadata = GetCurrentVideoModeMetadata(&bios_state_);
  if (!metadata) {
    return false;
  }
  if (!RenderCurrentVideoPage(&bios_state_)) {
    return false;
  }

  ofstream file(file_path);
  if (!file) {
    return false;
  }
  file << "P3 " << metadata->width << " " << metadata->height << " 255\n";
  for (uint16_t y = 0; y < metadata->height; ++y) {
    for (uint16_t x = 0; x < metadata->width; ++x) {
      size_t index = y * metadata->width + x;
      if (index >= sizeof(framebuffer_) / sizeof(RGB)) {
        continue;
      }
      const RGB& pixel = framebuffer_[index];
      file << setw(3) << static_cast<int>(pixel.r) << " " << setw(3)
           << static_cast<int>(pixel.g) << " " << setw(3)
           << static_cast<int>(pixel.b) << "    ";
    }
    file << endl;
  }
  file.close();
  return true;
}
