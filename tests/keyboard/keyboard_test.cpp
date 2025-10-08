#include "keyboard.h"

#include <gtest/gtest.h>

#include <vector>

namespace {

// Mock state trackers
std::vector<uint8_t> g_sent_scancodes;
int g_irq1_count;

// Mock callback implementations
void MockSendScancode(void* context, uint8_t scancode) {
  g_sent_scancodes.push_back(scancode);
}
void MockRaiseIrq1(void* context) { ++g_irq1_count; }

class KeyboardTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Reset mock trackers before each test
    g_sent_scancodes.clear();
    g_irq1_count = 0;

    // Initialize the keyboard module with mock callbacks
    config_ = {};
    config_.context = nullptr;  // Context not used by these simple mocks
    config_.send_scancode = MockSendScancode;
    config_.raise_irq1 = MockRaiseIrq1;
    KeyboardInit(&keyboard_, &config_);
  }

  // Member variables for the module's state
  KeyboardState keyboard_ = {0};
  KeyboardConfig config_ = {0};
};

TEST_F(KeyboardTest, Initialization) {
  // A new keyboard should be in a clean state.
  ASSERT_FALSE(keyboard_.enable_clear);
  ASSERT_TRUE(keyboard_.clock_low);
  ASSERT_EQ(keyboard_.clock_low_ms, 0);
  ASSERT_FALSE(keyboard_.waiting_for_ack);
  ASSERT_EQ(KeyboardBufferLength(&keyboard_.buffer), 0);
}

TEST_F(KeyboardTest, ResetSequence) {
  // 1. Hold clock low to trigger reset detection.
  KeyboardHandleControl(&keyboard_, false, false);
  for (int i = 0; i < 20; ++i) {
    KeyboardTickMs(&keyboard_);
  }

  // Verify 0xAA is buffered but not sent yet.
  ASSERT_EQ(KeyboardBufferLength(&keyboard_.buffer), 1);
  ASSERT_EQ(*KeyboardBufferGet(&keyboard_.buffer, 0), 0xAA);
  ASSERT_TRUE(g_sent_scancodes.empty());

  // 2. Release clock and pulse enable_clear to signal ack.
  KeyboardHandleControl(&keyboard_, false, true);  // Release clock
  KeyboardHandleControl(&keyboard_, true, true);   // Pulse high
  KeyboardHandleControl(&keyboard_, false, true);  // Pulse low (ack)

  // 3. Tick to allow the keyboard to send the scancode.
  KeyboardTickMs(&keyboard_);

  // 4. Verify the 0xAA scancode was sent and an IRQ was raised.
  ASSERT_EQ(g_sent_scancodes.size(), 1);
  ASSERT_EQ(g_sent_scancodes[0], 0xAA);
  ASSERT_EQ(g_irq1_count, 1);
  ASSERT_TRUE(keyboard_.waiting_for_ack);
}

TEST_F(KeyboardTest, KeyPressAndAck) {
  // 1. Buffer a key press.
  KeyboardHandleKeyPress(&keyboard_, 0x1E);  // 'A' key

  // 2. Tick to send the scancode.
  KeyboardTickMs(&keyboard_);
  ASSERT_EQ(g_sent_scancodes.size(), 1);
  ASSERT_EQ(g_sent_scancodes[0], 0x1E);
  ASSERT_EQ(g_irq1_count, 1);
  ASSERT_TRUE(keyboard_.waiting_for_ack);

  // 3. Buffer another key press and tick again.
  KeyboardHandleKeyPress(&keyboard_, 0x1F);  // 'S' key
  KeyboardTickMs(&keyboard_);

  // Verify the second scancode is NOT sent because we're waiting for an ack.
  ASSERT_EQ(g_sent_scancodes.size(), 1);
  ASSERT_EQ(KeyboardBufferLength(&keyboard_.buffer), 1);

  // 4. Simulate BIOS acknowledgement pulse.
  KeyboardHandleControl(&keyboard_, true, true);   // Pulse high
  KeyboardHandleControl(&keyboard_, false, true);  // Pulse low (ack)
  ASSERT_FALSE(keyboard_.waiting_for_ack);

  // 5. Tick again.
  KeyboardTickMs(&keyboard_);

  // Verify the second scancode is now sent.
  ASSERT_EQ(g_sent_scancodes.size(), 2);
  ASSERT_EQ(g_sent_scancodes[1], 0x1F);
  ASSERT_EQ(g_irq1_count, 2);
}

TEST_F(KeyboardTest, BufferOverflow) {
  // Buffer more keys than the buffer can hold.
  for (int i = 0; i < kKeyboardBufferSize + 5; ++i) {
    KeyboardHandleKeyPress(&keyboard_, i);
  }

  // Verify the buffer is full but not over-full.
  ASSERT_EQ(KeyboardBufferLength(&keyboard_.buffer), kKeyboardBufferSize);
}

TEST_F(KeyboardTest, InhibitedState) {
  // Buffer a key press.
  KeyboardHandleKeyPress(&keyboard_, 0x20);

  // Inhibit the keyboard by setting enable_clear to true.
  KeyboardHandleControl(&keyboard_, true, true);

  // Tick.
  KeyboardTickMs(&keyboard_);

  // Verify no scancode was sent.
  ASSERT_TRUE(g_sent_scancodes.empty());
  ASSERT_EQ(KeyboardBufferLength(&keyboard_.buffer), 1);
}

TEST_F(KeyboardTest, ShortClockLowDoesNotReset) {
  // 1. Buffer a key to ensure it's preserved.
  KeyboardHandleKeyPress(&keyboard_, 0x1E);  // 'A' key
  ASSERT_EQ(KeyboardBufferLength(&keyboard_.buffer), 1);

  // 2. Hold clock low for less than the reset threshold.
  KeyboardHandleControl(&keyboard_, false, false);
  for (int i = 0; i < kKeyboardResetThresholdMs - 1; ++i) {
    KeyboardTickMs(&keyboard_);
  }

  // 3. Verify that no reset was triggered and the buffer is intact.
  ASSERT_EQ(KeyboardBufferLength(&keyboard_.buffer), 1);
  ASSERT_EQ(*KeyboardBufferGet(&keyboard_.buffer, 0), 0x1E);
  ASSERT_TRUE(g_sent_scancodes.empty());  // Nothing should have been sent.

  // 4. Release the clock.
  KeyboardHandleControl(&keyboard_, false, true);

  // 5. Tick to allow the keyboard to send the buffered scancode.
  KeyboardTickMs(&keyboard_);

  // 6. Verify the original scancode is sent, not the reset code.
  ASSERT_EQ(g_sent_scancodes.size(), 1);
  ASSERT_EQ(g_sent_scancodes[0], 0x1E);
  ASSERT_EQ(g_irq1_count, 1);
}

}  // namespace
