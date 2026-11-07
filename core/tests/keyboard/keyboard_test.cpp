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
    keyboard_ = {};
    keyboard_.config.context =
        nullptr;  // Context not used by these simple mocks
    keyboard_.config.send_scancode = MockSendScancode;
    keyboard_.config.raise_irq1 = MockRaiseIrq1;
    YKeyboardInit(&keyboard_);
  }

  // Member variables for the module's state
  YKeyboardState keyboard_ = {0};
};

TEST_F(KeyboardTest, Initialization) {
  // A new keyboard should be in a clean state.
  ASSERT_FALSE(keyboard_.enable_clear);
  ASSERT_TRUE(keyboard_.clock_low);
  ASSERT_EQ(keyboard_.clock_low_ms, 0);
  ASSERT_FALSE(keyboard_.waiting_for_ack);
  ASSERT_EQ(YKeyboardBufferLength(&keyboard_.buffer), 0);
}

TEST_F(KeyboardTest, ResetSequence) {
  // 1. Hold clock low to trigger reset detection.
  YKeyboardHandleControl(&keyboard_, false, false);
  for (int i = 0; i < 20; ++i) {
    YKeyboardTickMs(&keyboard_);
  }

  // Verify 0xAA is buffered but not sent yet.
  ASSERT_EQ(YKeyboardBufferLength(&keyboard_.buffer), 1);
  ASSERT_EQ(*YKeyboardBufferGet(&keyboard_.buffer, 0), 0xAA);
  ASSERT_TRUE(g_sent_scancodes.empty());

  // 2. Release clock and pulse enable_clear to signal ack.
  YKeyboardHandleControl(&keyboard_, false, true);  // Release clock
  YKeyboardHandleControl(&keyboard_, true, true);   // Pulse high
  YKeyboardHandleControl(&keyboard_, false, true);  // Pulse low (ack)

  // 3. Tick to allow the keyboard to send the scancode.
  YKeyboardTickMs(&keyboard_);

  // 4. Verify the 0xAA scancode was sent and an IRQ was raised.
  ASSERT_EQ(g_sent_scancodes.size(), 1);
  ASSERT_EQ(g_sent_scancodes[0], 0xAA);
  ASSERT_EQ(g_irq1_count, 1);
  ASSERT_TRUE(keyboard_.waiting_for_ack);
}

TEST_F(KeyboardTest, KeyPressAndAck) {
  // 1. Buffer a key press.
  YKeyboardHandleKeyPress(&keyboard_, 0x1E);  // 'A' key

  // 2. Tick to send the scancode.
  YKeyboardTickMs(&keyboard_);
  ASSERT_EQ(g_sent_scancodes.size(), 1);
  ASSERT_EQ(g_sent_scancodes[0], 0x1E);
  ASSERT_EQ(g_irq1_count, 1);
  ASSERT_TRUE(keyboard_.waiting_for_ack);

  // 3. Buffer another key press and tick again.
  YKeyboardHandleKeyPress(&keyboard_, 0x1F);  // 'S' key
  YKeyboardTickMs(&keyboard_);

  // Verify the second scancode is NOT sent because we're waiting for an ack.
  ASSERT_EQ(g_sent_scancodes.size(), 1);
  ASSERT_EQ(YKeyboardBufferLength(&keyboard_.buffer), 1);

  // 4. Simulate BIOS acknowledgement pulse.
  YKeyboardHandleControl(&keyboard_, true, true);   // Pulse high
  YKeyboardHandleControl(&keyboard_, false, true);  // Pulse low (ack)
  ASSERT_FALSE(keyboard_.waiting_for_ack);

  // 5. Tick again.
  YKeyboardTickMs(&keyboard_);

  // Verify the second scancode is now sent.
  ASSERT_EQ(g_sent_scancodes.size(), 2);
  ASSERT_EQ(g_sent_scancodes[1], 0x1F);
  ASSERT_EQ(g_irq1_count, 2);
}

TEST_F(KeyboardTest, BufferOverflow) {
  // Buffer more keys than the buffer can hold.
  for (int i = 0; i < kKeyboardBufferSize + 5; ++i) {
    YKeyboardHandleKeyPress(&keyboard_, i);
  }

  // Verify the buffer is full but not over-full.
  ASSERT_EQ(YKeyboardBufferLength(&keyboard_.buffer), kKeyboardBufferSize);
}

TEST_F(KeyboardTest, KeysPressedDuringResetAreDropped) {
  // The BIOS resets the keyboard by holding its clock line low. A keyboard
  // held in reset is not scanning, so a key pressed now never becomes a scan
  // code - and must not resurface once the reset ends, which is where the
  // BIOS's stuck key test would see it and report a stuck key.
  YKeyboardHandleControl(&keyboard_, false, false);
  for (int i = 0; i < kKeyboardResetThresholdMs; ++i) {
    YKeyboardTickMs(&keyboard_);
  }
  ASSERT_EQ(YKeyboardBufferLength(&keyboard_.buffer), 1);  // the 0xAA self test

  YKeyboardHandleKeyPress(&keyboard_, 0x1C);
  EXPECT_EQ(YKeyboardBufferLength(&keyboard_.buffer), 1);

  // Coming out of reset, the self test code is the only thing sent.
  YKeyboardHandleControl(&keyboard_, false, true);
  YKeyboardHandleControl(&keyboard_, true, true);
  YKeyboardHandleControl(&keyboard_, false, true);
  YKeyboardTickMs(&keyboard_);
  ASSERT_EQ(g_sent_scancodes.size(), 1);
  EXPECT_EQ(g_sent_scancodes[0], 0xAA);

  // And nothing follows it once that is acknowledged.
  YKeyboardHandleControl(&keyboard_, true, true);
  YKeyboardHandleControl(&keyboard_, false, true);
  YKeyboardTickMs(&keyboard_);
  EXPECT_EQ(g_sent_scancodes.size(), 1);
}

TEST_F(KeyboardTest, KeysPressedAfterAResetAreKept) {
  // Only the reset itself swallows keys. Once the keyboard is running again,
  // typing works normally - getting this wrong silently eats every keystroke
  // for the rest of the session, since a machine resets its keyboard at boot.
  YKeyboardHandleControl(&keyboard_, false, false);
  for (int i = 0; i < kKeyboardResetThresholdMs; ++i) {
    YKeyboardTickMs(&keyboard_);
  }
  YKeyboardHandleControl(&keyboard_, false, true);
  YKeyboardHandleControl(&keyboard_, true, true);
  YKeyboardHandleControl(&keyboard_, false, true);
  YKeyboardTickMs(&keyboard_);  // sends the 0xAA self test code
  YKeyboardHandleControl(&keyboard_, true, true);
  YKeyboardHandleControl(&keyboard_, false, true);

  YKeyboardHandleKeyPress(&keyboard_, 0x1E);
  YKeyboardTickMs(&keyboard_);
  ASSERT_EQ(g_sent_scancodes.size(), 2);
  EXPECT_EQ(g_sent_scancodes[1], 0x1E);
}

TEST_F(KeyboardTest, KeysPressedDuringAShortClockLowAreKept) {
  // Holding the clock line low briefly only stops the keyboard talking. It is
  // not a reset, so the keyboard keeps scanning and buffers what it sees.
  YKeyboardHandleControl(&keyboard_, false, false);
  for (int i = 0; i < kKeyboardResetThresholdMs - 1; ++i) {
    YKeyboardTickMs(&keyboard_);
  }

  YKeyboardHandleKeyPress(&keyboard_, 0x1E);
  EXPECT_EQ(YKeyboardBufferLength(&keyboard_.buffer), 1);

  YKeyboardHandleControl(&keyboard_, false, true);
  YKeyboardTickMs(&keyboard_);
  ASSERT_EQ(g_sent_scancodes.size(), 1);
  EXPECT_EQ(g_sent_scancodes[0], 0x1E);
}

TEST_F(KeyboardTest, KeysPressedWhileMerelyInhibitedAreKept) {
  // Being inhibited is not the same as being reset. The BIOS inhibits the
  // keyboard for most of POST with the clock line still enabled, and a key
  // pressed then is held until the keyboard is enabled again rather than lost.
  YKeyboardHandleControl(&keyboard_, true, true);
  YKeyboardHandleKeyPress(&keyboard_, 0x1E);
  EXPECT_EQ(YKeyboardBufferLength(&keyboard_.buffer), 1);

  YKeyboardHandleControl(&keyboard_, false, true);
  YKeyboardTickMs(&keyboard_);
  ASSERT_EQ(g_sent_scancodes.size(), 1);
  EXPECT_EQ(g_sent_scancodes[0], 0x1E);
}

TEST_F(KeyboardTest, InhibitedState) {
  // Buffer a key press.
  YKeyboardHandleKeyPress(&keyboard_, 0x20);

  // Inhibit the keyboard by setting enable_clear to true.
  YKeyboardHandleControl(&keyboard_, true, true);

  // Tick.
  YKeyboardTickMs(&keyboard_);

  // Verify no scancode was sent.
  ASSERT_TRUE(g_sent_scancodes.empty());
  ASSERT_EQ(YKeyboardBufferLength(&keyboard_.buffer), 1);
}

TEST_F(KeyboardTest, ShortClockLowDoesNotReset) {
  // 1. Buffer a key to ensure it's preserved.
  YKeyboardHandleKeyPress(&keyboard_, 0x1E);  // 'A' key
  ASSERT_EQ(YKeyboardBufferLength(&keyboard_.buffer), 1);

  // 2. Hold clock low for less than the reset threshold.
  YKeyboardHandleControl(&keyboard_, false, false);
  for (int i = 0; i < kKeyboardResetThresholdMs - 1; ++i) {
    YKeyboardTickMs(&keyboard_);
  }

  // 3. Verify that no reset was triggered and the buffer is intact.
  ASSERT_EQ(YKeyboardBufferLength(&keyboard_.buffer), 1);
  ASSERT_EQ(*YKeyboardBufferGet(&keyboard_.buffer, 0), 0x1E);
  ASSERT_TRUE(g_sent_scancodes.empty());  // Nothing should have been sent.

  // 4. Release the clock.
  YKeyboardHandleControl(&keyboard_, false, true);

  // 5. Tick to allow the keyboard to send the buffered scancode.
  YKeyboardTickMs(&keyboard_);

  // 6. Verify the original scancode is sent, not the reset code.
  ASSERT_EQ(g_sent_scancodes.size(), 1);
  ASSERT_EQ(g_sent_scancodes[0], 0x1E);
  ASSERT_EQ(g_irq1_count, 1);
}

}  // namespace
