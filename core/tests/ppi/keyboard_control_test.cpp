#include <gtest/gtest.h>

#include "ppi.h"

namespace {

// Static trackers for the mock callback.
static int g_callback_count;
static bool g_last_kb_enable_clear;
static bool g_last_kb_clock;

// Mock callback function to capture calls and arguments.
static void MockSetKeyboardControl(
    void* context, bool keyboard_enable_clear, bool keyboard_clock) {
  g_callback_count++;
  g_last_kb_enable_clear = keyboard_enable_clear;
  g_last_kb_clock = keyboard_clock;
}

class KeyboardControlTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Reset trackers before each test.
    g_callback_count = 0;
    g_last_kb_enable_clear = false;
    g_last_kb_clock = false;

    // Initialize config and PPI state.
    config_ = {0};
    PPIInit(&ppi_, &config_);

    // Wire up the mock callback.
    config_.set_keyboard_control = MockSetKeyboardControl;
  }

  PPIState ppi_ = {0};
  PPIConfig config_ = {0};
};

TEST_F(KeyboardControlTest, NoChangeNoCallback) {
  // Arrange: Initial port B is 0.
  ASSERT_EQ(ppi_.port_b, 0);

  // Act: Write a value that doesn't affect bits 6 or 7.
  PPIWritePort(&ppi_, kPPIPortB, 0b00111111);

  // Assert: Callback should not have been called.
  EXPECT_EQ(g_callback_count, 0);
}

TEST_F(KeyboardControlTest, CallbackOnBit6Change) {
  // Act: Write a value to flip bit 6 on.
  PPIWritePort(&ppi_, kPPIPortB, kPPIPortBKeyboardClockLow);

  // Assert: Callback was called once with the correct state.
  EXPECT_EQ(g_callback_count, 1);
  EXPECT_FALSE(g_last_kb_enable_clear);
  EXPECT_TRUE(g_last_kb_clock);
}

TEST_F(KeyboardControlTest, CallbackOnBit7Change) {
  // Act: Write a value to flip bit 7 on.
  PPIWritePort(&ppi_, kPPIPortB, kPPIPortBKeyboardEnableClear);

  // Assert: Callback was called once with the correct state.
  EXPECT_EQ(g_callback_count, 1);
  EXPECT_TRUE(g_last_kb_enable_clear);
  EXPECT_FALSE(g_last_kb_clock);
}

TEST_F(KeyboardControlTest, CallbackOnBothBitsChange) {
  // Act: Write a value to flip both bits 6 and 7 on.
  const uint8_t both_bits =
      kPPIPortBKeyboardEnableClear | kPPIPortBKeyboardClockLow;
  PPIWritePort(&ppi_, kPPIPortB, both_bits);

  // Assert: Callback was called once with the correct state.
  EXPECT_EQ(g_callback_count, 1);
  EXPECT_TRUE(g_last_kb_enable_clear);
  EXPECT_TRUE(g_last_kb_clock);
}

TEST_F(KeyboardControlTest, CallbackOnFlipOff) {
  // Arrange: Start with both bits on.
  const uint8_t both_bits =
      kPPIPortBKeyboardEnableClear | kPPIPortBKeyboardClockLow;
  PPIWritePort(&ppi_, kPPIPortB, both_bits);
  // Reset trackers after setup.
  g_callback_count = 0;

  // Act: Write a value to flip both bits off.
  PPIWritePort(&ppi_, kPPIPortB, 0);

  // Assert: Callback was called once with the correct new state.
  EXPECT_EQ(g_callback_count, 1);
  EXPECT_FALSE(g_last_kb_enable_clear);
  EXPECT_FALSE(g_last_kb_clock);
}

TEST_F(KeyboardControlTest, NoCallbackIfNull) {
  // Arrange: Set the callback to nullptr.
  config_.set_keyboard_control = nullptr;

  // Act: Write a value that would normally trigger the callback.
  PPIWritePort(&ppi_, kPPIPortB, kPPIPortBKeyboardClockLow);

  // Assert: Callback was not called and the program did not crash.
  EXPECT_EQ(g_callback_count, 0);
}

}  // namespace
