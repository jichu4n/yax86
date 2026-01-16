#ifndef YAX86_DMA_TEST_HELPER_H
#define YAX86_DMA_TEST_HELPER_H

#include <gtest/gtest.h>

#include "dma.h"

namespace {

// Mock memory buffer for testing.
static uint8_t g_mock_memory[128 * 1024];

// Storage for mock device interactions.
static uint8_t g_data_from_device;
static uint8_t g_data_to_device;

// Mock callback implementations
static uint8_t MockReadMemory(void* context, uint32_t address) {
  if (address < sizeof(g_mock_memory)) {
    return g_mock_memory[address];
  }
  return 0xFF;  // Out of bounds
}

static void MockWriteMemory(void* context, uint32_t address, uint8_t value) {
  if (address < sizeof(g_mock_memory)) {
    g_mock_memory[address] = value;
  }
}

static uint8_t MockReadDevice(void* context, uint8_t channel) {
  return g_data_from_device;
}

static void MockWriteDevice(void* context, uint8_t channel, uint8_t value) {
  g_data_to_device = value;
}

// Base test fixture for DMA tests.
class DMATest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Clear mock memory and device data before each test.
    memset(g_mock_memory, 0, sizeof(g_mock_memory));
    g_data_from_device = 0;
    g_data_to_device = 0;

    // Set up the DMA config with our mock callbacks.
    config_.context = this;
    config_.read_memory_byte = MockReadMemory;
    config_.write_memory_byte = MockWriteMemory;
    config_.read_device_byte = MockReadDevice;
    config_.write_device_byte = MockWriteDevice;

    // Initialize the DMA controller.
    DMAInit(&dma_, &config_);
  }

  DMAState dma_ = {0};
  DMAConfig config_ = {0};
};

}  // namespace

#endif  // YAX86_DMA_TEST_HELPER_H
