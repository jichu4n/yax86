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

  // Render the current framebuffer to a PPM file.
  bool RenderToFile(const std::string& file_name_prefix);
  // Checks if a rendered PPM file matches the golden PPM file.
  bool CheckGolden(const std::string& file_name_prefix);
  // Render the current framebuffer to a PPM file and check against a golden
  // file.
  bool RenderToFileAndCheckGolden(const std::string& file_name_prefix) {
    return RenderToFile(file_name_prefix) && CheckGolden(file_name_prefix);
  }

 private:
  BIOSConfig config_ = {0};
  uint8_t memory_[kMemorySizeKB * 1024] = {0};
  uint8_t vram_[kVRAMSizeKB * 1024] = {0};
  RGB framebuffer_[kFramebufferSizeKB * 1024] = {0};

  std::string GetGoldenFilePath(const std::string& file_name) const;
};

#endif  // VIDEO_TEST_HELPER_H
