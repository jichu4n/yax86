#ifndef VIDEO_TEST_HELPER_H
#define VIDEO_TEST_HELPER_H

#include <string>

#include "bios.h"

class VideoTestHelper {
 public:
  static constexpr size_t kMemorySizeKB = 16;
  static constexpr size_t kVRAMSizeKB = 16;
  static constexpr size_t kFramebufferSizeKB = 256;

  BIOSState bios_state_;

  VideoTestHelper();

  bool RenderToPPM(const std::string& file_path);

 private:
  BIOSConfig config_ = {0};
  uint8_t memory_[kMemorySizeKB * 1024] = {0};
  uint8_t vram_[kVRAMSizeKB * 1024] = {0};
  RGB framebuffer_[kFramebufferSizeKB * 1024] = {0};
};

#endif  // VIDEO_TEST_HELPER_H
