#include "video_test_helper.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

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

bool VideoTestHelper::RenderToFile(const string& file_prefix) {
  const VideoModeMetadata* metadata = GetCurrentVideoModeMetadata(&bios_state_);
  if (!metadata) {
    return false;
  }
  if (!RenderCurrentVideoPage(&bios_state_)) {
    return false;
  }

  const string file_path = file_prefix + ".ppm";
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

bool VideoTestHelper::CheckGolden(const string& file_name_prefix) {
  const string rendered_file_path = file_name_prefix + ".ppm";
  ifstream rendered_file(rendered_file_path);
  if (!rendered_file) {
    cerr << "Rendered file not found: " << rendered_file_path << endl;
    return false;
  }

  const string golden_file_path =
      GetGoldenFilePath(file_name_prefix + "-golden.ppm");
  ifstream golden_file(golden_file_path);
  if (!golden_file) {
    cerr << "Golden file not found: " << golden_file_path << endl
         << "Copying rendered file to golden file." << endl;
    // Copy the rendered file to the golden file.
    ofstream golden_file(golden_file_path);
    if (!golden_file) {
      cerr << "Failed to copy rendered file to golden file." << endl;
      return false;
    }
    golden_file << rendered_file.rdbuf();
    golden_file.close();
    return true;
  }

  string golden_line, rendered_line;
  while (getline(golden_file, golden_line) &&
         getline(rendered_file, rendered_line)) {
    if (golden_line != rendered_line) {
      cerr << "Mismatch in PPM files" << endl
           << "Rendered file: " << rendered_file_path << endl
           << "Golden file: " << golden_file_path << endl;
      return false;
    }
  }

  return true;
}

std::string VideoTestHelper::GetGoldenFilePath(
    const std::string& file_name) const {
  const filesystem::path current_file_path = __FILE__;
  filesystem::path file_path =
      current_file_path.parent_path() / "testdata" / file_name;
  return file_path.string();
}
