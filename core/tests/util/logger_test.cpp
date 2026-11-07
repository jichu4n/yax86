#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "src/util/log.h"

using namespace std;

namespace {

// Modules used by these tests. IDs here are local to the test and do not
// need to match those of any module.
const YLogModule kModuleA = {0, "A"};
const YLogModule kModuleB = {1, "B"};

// A single message captured by the test sink.
struct CapturedMessage {
  string module_name;
  YLogLevel level;
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
    config_.enabled_modules =
        YLogModuleMask(&kModuleA) | YLogModuleMask(&kModuleB);
    config_.min_level = Y_LOG_LEVEL_DEBUG;
    YLoggerInit(&logger_, &config_);
  }

  YLogger* logger() { return &logger_; }
  YLoggerConfig* config() { return &config_; }
  const vector<CapturedMessage>& messages() const { return messages_; }

  uint64_t tick_ = 0;

 private:
  static void WriteLine(
      void* context, const YLogModule* module, YLogLevel level, uint64_t tick,
      const char* message, size_t length) {
    TestLogger* self = static_cast<TestLogger*>(context);
    self->messages_.push_back(
        {module->name, level, tick, string(message), length});
  }

  static uint64_t GetTick(void* context) {
    return static_cast<TestLogger*>(context)->tick_;
  }

  YLoggerConfig config_ = {};
  YLogger logger_ = {};
  vector<CapturedMessage> messages_;
};

}  // namespace

class LoggerTest : public ::testing::Test {};

TEST_F(LoggerTest, WritesFormattedMessage) {
  TestLogger test_logger;
  test_logger.tick_ = 12345;

  YAX86_LOG(
      test_logger.logger(), &kModuleA, Y_LOG_LEVEL_WARN, "value=%04X n=%d",
      0x1234, -7);

  ASSERT_EQ(test_logger.messages().size(), 1u);
  const CapturedMessage& message = test_logger.messages()[0];
  EXPECT_EQ(message.module_name, "A");
  EXPECT_EQ(message.level, Y_LOG_LEVEL_WARN);
  EXPECT_EQ(message.tick, 12345u);
  // No prefix and no trailing newline - the host composes the output line.
  EXPECT_EQ(message.message, "value=1234 n=-7");
  EXPECT_EQ(message.length, message.message.size());
}

TEST_F(LoggerTest, SuppressesDisabledModule) {
  TestLogger test_logger;
  test_logger.config()->enabled_modules = YLogModuleMask(&kModuleB);

  YAX86_LOG(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_ERROR, "suppressed");
  EXPECT_TRUE(test_logger.messages().empty());
  EXPECT_FALSE(
      YLoggerIsEnabled(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_ERROR));

  YAX86_LOG(test_logger.logger(), &kModuleB, Y_LOG_LEVEL_ERROR, "emitted");
  ASSERT_EQ(test_logger.messages().size(), 1u);
  EXPECT_EQ(test_logger.messages()[0].message, "emitted");
  EXPECT_TRUE(
      YLoggerIsEnabled(test_logger.logger(), &kModuleB, Y_LOG_LEVEL_ERROR));
}

TEST_F(LoggerTest, SuppressesMessagesAboveMinLevel) {
  TestLogger test_logger;
  test_logger.config()->min_level = Y_LOG_LEVEL_WARN;

  YAX86_LOG(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_ERROR, "error");
  YAX86_LOG(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_WARN, "warn");
  YAX86_LOG(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG, "debug");

  ASSERT_EQ(test_logger.messages().size(), 2u);
  EXPECT_EQ(test_logger.messages()[0].message, "error");
  EXPECT_EQ(test_logger.messages()[1].message, "warn");

  EXPECT_TRUE(
      YLoggerIsEnabled(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_ERROR));
  EXPECT_TRUE(
      YLoggerIsEnabled(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_WARN));
  EXPECT_FALSE(
      YLoggerIsEnabled(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG));
}

TEST_F(LoggerTest, TruncatesLongMessages) {
  TestLogger test_logger;
  const string long_message(Y_LOG_MAX_LINE_LENGTH * 2, 'x');

  YAX86_LOG(
      test_logger.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG, "%s",
      long_message.c_str());

  ASSERT_EQ(test_logger.messages().size(), 1u);
  const CapturedMessage& message = test_logger.messages()[0];
  // Truncated to the buffer size, leaving room for the terminating null byte.
  EXPECT_EQ(
      message.message.size(), static_cast<size_t>(Y_LOG_MAX_LINE_LENGTH - 1));
  EXPECT_EQ(message.length, static_cast<size_t>(Y_LOG_MAX_LINE_LENGTH - 1));
  EXPECT_EQ(message.message, long_message.substr(0, Y_LOG_MAX_LINE_LENGTH - 1));
}

TEST_F(LoggerTest, ExactlyFillsBuffer) {
  TestLogger test_logger;
  const string message(Y_LOG_MAX_LINE_LENGTH - 1, 'y');

  YAX86_LOG(
      test_logger.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG, "%s",
      message.c_str());

  ASSERT_EQ(test_logger.messages().size(), 1u);
  EXPECT_EQ(test_logger.messages()[0].message, message);
  EXPECT_EQ(
      test_logger.messages()[0].length,
      static_cast<size_t>(Y_LOG_MAX_LINE_LENGTH - 1));
}

TEST_F(LoggerTest, ToleratesMissingCallbacksAndState) {
  TestLogger test_logger;

  // No get_tick callback - the reported tick is 0.
  test_logger.tick_ = 99;
  test_logger.config()->get_tick = NULL;
  YAX86_LOG(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG, "no tick");
  ASSERT_EQ(test_logger.messages().size(), 1u);
  EXPECT_EQ(test_logger.messages()[0].tick, 0u);

  // No write_line callback - nothing is emitted and nothing crashes.
  test_logger.config()->write_line = NULL;
  YAX86_LOG(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG, "no sink");
  EXPECT_EQ(test_logger.messages().size(), 1u);
  EXPECT_FALSE(
      YLoggerIsEnabled(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG));

  // A logger with no config, and no logger at all.
  YLogger unconfigured = {};
  YLoggerInit(&unconfigured, NULL);
  YAX86_LOG(&unconfigured, &kModuleA, Y_LOG_LEVEL_ERROR, "no config");
  YAX86_LOG(NULL, &kModuleA, Y_LOG_LEVEL_ERROR, "no logger");
  EXPECT_FALSE(YLoggerIsEnabled(&unconfigured, &kModuleA, Y_LOG_LEVEL_ERROR));
  EXPECT_FALSE(YLoggerIsEnabled(NULL, &kModuleA, Y_LOG_LEVEL_ERROR));
}

TEST_F(LoggerTest, LoggersAreIndependent) {
  TestLogger first;
  TestLogger second;
  second.config()->enabled_modules = YLogModuleMask(&kModuleB);

  YAX86_LOG(first.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG, "first");
  YAX86_LOG(second.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG, "dropped");
  YAX86_LOG(second.logger(), &kModuleB, Y_LOG_LEVEL_DEBUG, "second");

  ASSERT_EQ(first.messages().size(), 1u);
  EXPECT_EQ(first.messages()[0].message, "first");
  ASSERT_EQ(second.messages().size(), 1u);
  EXPECT_EQ(second.messages()[0].message, "second");
}

TEST_F(LoggerTest, EnableAndDisableModule) {
  TestLogger test_logger;
  test_logger.config()->enabled_modules = 0;

  EXPECT_FALSE(
      YLoggerIsEnabled(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG));

  YLoggerEnableModule(test_logger.logger(), &kModuleA);
  EXPECT_TRUE(
      YLoggerIsEnabled(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG));
  // Enabling one module leaves the others alone.
  EXPECT_FALSE(
      YLoggerIsEnabled(test_logger.logger(), &kModuleB, Y_LOG_LEVEL_DEBUG));

  YLoggerDisableModule(test_logger.logger(), &kModuleA);
  EXPECT_FALSE(
      YLoggerIsEnabled(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG));

  // Safe on a logger with no config, and on no logger at all.
  YLogger unconfigured = {};
  YLoggerInit(&unconfigured, NULL);
  YLoggerEnableModule(&unconfigured, &kModuleA);
  YLoggerDisableModule(&unconfigured, &kModuleA);
  YLoggerEnableModule(NULL, &kModuleA);
  YLoggerDisableModule(NULL, &kModuleA);
}

TEST_F(LoggerTest, DoesNotFormatSuppressedMessages) {
  TestLogger test_logger;
  test_logger.config()->enabled_modules = 0;

  // The macro must not evaluate to a call that formats the arguments. If it
  // did, this counter would be incremented.
  int calls = 0;
  auto count_call = [&calls]() {
    ++calls;
    return 0;
  };
  YAX86_LOG(
      test_logger.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG, "%d", count_call());
  EXPECT_EQ(calls, 0);

  YLoggerEnableModule(test_logger.logger(), &kModuleA);
  YAX86_LOG(
      test_logger.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG, "%d", count_call());
  EXPECT_EQ(calls, 1);
}
