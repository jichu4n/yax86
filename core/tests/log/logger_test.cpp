#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "log.h"

using namespace std;

namespace {

// Categories used by these tests. IDs here are local to the test and do not
// need to match those of any module.
const LogCategory kCategoryA = {0, "A"};
const LogCategory kCategoryB = {1, "B"};

// A single message captured by the test sink.
struct CapturedMessage {
  string category_name;
  LogLevel level;
  uint64_t tick;
  string message;
  size_t length;
};

// Test harness wiring a logger up to a capturing sink.
class TestLogger {
 public:
  TestLogger() {
    config_.context = this;
    config_.write_line = WriteLine;
    config_.get_tick = GetTick;
    config_.enabled_categories =
        LogCategoryMask(&kCategoryA) | LogCategoryMask(&kCategoryB);
    config_.min_level = kLogLevelDebug;
    LoggerInit(&logger_, &config_);
  }

  Logger* logger() { return &logger_; }
  LoggerConfig* config() { return &config_; }
  const vector<CapturedMessage>& messages() const { return messages_; }

  uint64_t tick_ = 0;

 private:
  static void WriteLine(
      void* context, const LogCategory* category, LogLevel level, uint64_t tick,
      const char* message, size_t length) {
    TestLogger* self = static_cast<TestLogger*>(context);
    self->messages_.push_back(
        {category->name, level, tick, string(message), length});
  }

  static uint64_t GetTick(void* context) {
    return static_cast<TestLogger*>(context)->tick_;
  }

  LoggerConfig config_ = {};
  Logger logger_ = {};
  vector<CapturedMessage> messages_;
};

}  // namespace

class LoggerTest : public ::testing::Test {};

TEST_F(LoggerTest, WritesFormattedMessage) {
  TestLogger test_logger;
  test_logger.tick_ = 12345;

  YAX86_LOG(
      test_logger.logger(), &kCategoryA, kLogLevelWarn, "value=%04X n=%d",
      0x1234, -7);

  ASSERT_EQ(test_logger.messages().size(), 1u);
  const CapturedMessage& message = test_logger.messages()[0];
  EXPECT_EQ(message.category_name, "A");
  EXPECT_EQ(message.level, kLogLevelWarn);
  EXPECT_EQ(message.tick, 12345u);
  // No prefix and no trailing newline - the host composes the output line.
  EXPECT_EQ(message.message, "value=1234 n=-7");
  EXPECT_EQ(message.length, message.message.size());
}

TEST_F(LoggerTest, SuppressesDisabledCategory) {
  TestLogger test_logger;
  test_logger.config()->enabled_categories = LogCategoryMask(&kCategoryB);

  YAX86_LOG(test_logger.logger(), &kCategoryA, kLogLevelError, "suppressed");
  EXPECT_TRUE(test_logger.messages().empty());
  EXPECT_FALSE(
      LoggerIsEnabled(test_logger.logger(), &kCategoryA, kLogLevelError));

  YAX86_LOG(test_logger.logger(), &kCategoryB, kLogLevelError, "emitted");
  ASSERT_EQ(test_logger.messages().size(), 1u);
  EXPECT_EQ(test_logger.messages()[0].message, "emitted");
  EXPECT_TRUE(
      LoggerIsEnabled(test_logger.logger(), &kCategoryB, kLogLevelError));
}

TEST_F(LoggerTest, SuppressesMessagesAboveMinLevel) {
  TestLogger test_logger;
  test_logger.config()->min_level = kLogLevelWarn;

  YAX86_LOG(test_logger.logger(), &kCategoryA, kLogLevelError, "error");
  YAX86_LOG(test_logger.logger(), &kCategoryA, kLogLevelWarn, "warn");
  YAX86_LOG(test_logger.logger(), &kCategoryA, kLogLevelDebug, "debug");

  ASSERT_EQ(test_logger.messages().size(), 2u);
  EXPECT_EQ(test_logger.messages()[0].message, "error");
  EXPECT_EQ(test_logger.messages()[1].message, "warn");

  EXPECT_TRUE(
      LoggerIsEnabled(test_logger.logger(), &kCategoryA, kLogLevelError));
  EXPECT_TRUE(
      LoggerIsEnabled(test_logger.logger(), &kCategoryA, kLogLevelWarn));
  EXPECT_FALSE(
      LoggerIsEnabled(test_logger.logger(), &kCategoryA, kLogLevelDebug));
}

TEST_F(LoggerTest, TruncatesLongMessages) {
  TestLogger test_logger;
  const string long_message(kLogMaxLineLength * 2, 'x');

  YAX86_LOG(
      test_logger.logger(), &kCategoryA, kLogLevelDebug, "%s",
      long_message.c_str());

  ASSERT_EQ(test_logger.messages().size(), 1u);
  const CapturedMessage& message = test_logger.messages()[0];
  // Truncated to the buffer size, leaving room for the terminating null byte.
  EXPECT_EQ(message.message.size(), static_cast<size_t>(kLogMaxLineLength - 1));
  EXPECT_EQ(message.length, static_cast<size_t>(kLogMaxLineLength - 1));
  EXPECT_EQ(message.message, long_message.substr(0, kLogMaxLineLength - 1));
}

TEST_F(LoggerTest, ExactlyFillsBuffer) {
  TestLogger test_logger;
  const string message(kLogMaxLineLength - 1, 'y');

  YAX86_LOG(
      test_logger.logger(), &kCategoryA, kLogLevelDebug, "%s", message.c_str());

  ASSERT_EQ(test_logger.messages().size(), 1u);
  EXPECT_EQ(test_logger.messages()[0].message, message);
  EXPECT_EQ(
      test_logger.messages()[0].length,
      static_cast<size_t>(kLogMaxLineLength - 1));
}

TEST_F(LoggerTest, ToleratesMissingCallbacksAndState) {
  TestLogger test_logger;

  // No get_tick callback - the reported tick is 0.
  test_logger.tick_ = 99;
  test_logger.config()->get_tick = NULL;
  YAX86_LOG(test_logger.logger(), &kCategoryA, kLogLevelDebug, "no tick");
  ASSERT_EQ(test_logger.messages().size(), 1u);
  EXPECT_EQ(test_logger.messages()[0].tick, 0u);

  // No write_line callback - nothing is emitted and nothing crashes.
  test_logger.config()->write_line = NULL;
  YAX86_LOG(test_logger.logger(), &kCategoryA, kLogLevelDebug, "no sink");
  EXPECT_EQ(test_logger.messages().size(), 1u);
  EXPECT_FALSE(
      LoggerIsEnabled(test_logger.logger(), &kCategoryA, kLogLevelDebug));

  // A logger with no config, and no logger at all.
  Logger unconfigured = {};
  LoggerInit(&unconfigured, NULL);
  YAX86_LOG(&unconfigured, &kCategoryA, kLogLevelError, "no config");
  YAX86_LOG(NULL, &kCategoryA, kLogLevelError, "no logger");
  EXPECT_FALSE(LoggerIsEnabled(&unconfigured, &kCategoryA, kLogLevelError));
  EXPECT_FALSE(LoggerIsEnabled(NULL, &kCategoryA, kLogLevelError));
}

TEST_F(LoggerTest, LoggersAreIndependent) {
  TestLogger first;
  TestLogger second;
  second.config()->enabled_categories = LogCategoryMask(&kCategoryB);

  YAX86_LOG(first.logger(), &kCategoryA, kLogLevelDebug, "first");
  YAX86_LOG(second.logger(), &kCategoryA, kLogLevelDebug, "dropped");
  YAX86_LOG(second.logger(), &kCategoryB, kLogLevelDebug, "second");

  ASSERT_EQ(first.messages().size(), 1u);
  EXPECT_EQ(first.messages()[0].message, "first");
  ASSERT_EQ(second.messages().size(), 1u);
  EXPECT_EQ(second.messages()[0].message, "second");
}

TEST_F(LoggerTest, EnableAndDisableCategory) {
  TestLogger test_logger;
  test_logger.config()->enabled_categories = 0;

  EXPECT_FALSE(
      LoggerIsEnabled(test_logger.logger(), &kCategoryA, kLogLevelDebug));

  LoggerEnableCategory(test_logger.logger(), &kCategoryA);
  EXPECT_TRUE(
      LoggerIsEnabled(test_logger.logger(), &kCategoryA, kLogLevelDebug));
  // Enabling one category leaves the others alone.
  EXPECT_FALSE(
      LoggerIsEnabled(test_logger.logger(), &kCategoryB, kLogLevelDebug));

  LoggerDisableCategory(test_logger.logger(), &kCategoryA);
  EXPECT_FALSE(
      LoggerIsEnabled(test_logger.logger(), &kCategoryA, kLogLevelDebug));

  // Safe on a logger with no config, and on no logger at all.
  Logger unconfigured = {};
  LoggerInit(&unconfigured, NULL);
  LoggerEnableCategory(&unconfigured, &kCategoryA);
  LoggerDisableCategory(&unconfigured, &kCategoryA);
  LoggerEnableCategory(NULL, &kCategoryA);
  LoggerDisableCategory(NULL, &kCategoryA);
}

TEST_F(LoggerTest, DoesNotFormatSuppressedMessages) {
  TestLogger test_logger;
  test_logger.config()->enabled_categories = 0;

  // The macro must not evaluate to a call that formats the arguments. If it
  // did, this counter would be incremented.
  int calls = 0;
  auto count_call = [&calls]() {
    ++calls;
    return 0;
  };
  YAX86_LOG(
      test_logger.logger(), &kCategoryA, kLogLevelDebug, "%d", count_call());
  EXPECT_EQ(calls, 0);

  LoggerEnableCategory(test_logger.logger(), &kCategoryA);
  YAX86_LOG(
      test_logger.logger(), &kCategoryA, kLogLevelDebug, "%d", count_call());
  EXPECT_EQ(calls, 1);
}
