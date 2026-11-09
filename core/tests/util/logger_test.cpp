#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "src/util/log.h"

using namespace std;

namespace {

// Modules used by these tests. IDs here are local to the test and do not
// need to match those of any module.
const YLogModule kModuleA = {0, "a"};
const YLogModule kModuleB = {1, "b"};

// A single line captured by the test sink.
struct CapturedLine {
  YLogKind kind;
  const YLogModule* module;
  YLogLevel level;
  string line;
  size_t length;
};

// Test harness wiring a logger up to a capturing sink.
class TestLogger {
 public:
  TestLogger() {
    config_.context = this;
    config_.write_line = WriteLine;
    config_.enabled_modules =
        YLogModuleMask(&kModuleA) | YLogModuleMask(&kModuleB);
    config_.min_level = Y_LOG_LEVEL_DEBUG;
    YLoggerInit(&logger_, &config_);
  }

  YLogger* logger() { return &logger_; }
  YLoggerConfig* config() { return &config_; }
  const vector<CapturedLine>& lines() const { return lines_; }

  // Give the logger a source answering with tick_ and location_.
  void SetSource() {
    const YLoggerSource source = {this, GetTick, GetLocation};
    YLoggerSetSource(&logger_, &source);
  }

  uint64_t tick_ = 0;
  bool has_location_ = true;
  uint16_t cs_ = 0;
  uint16_t ip_ = 0;

 private:
  static void WriteLine(
      void* context, YLogKind kind, const YLogModule* module, YLogLevel level,
      const char* line, size_t length) {
    TestLogger* self = static_cast<TestLogger*>(context);
    self->lines_.push_back({kind, module, level, string(line), length});
  }

  static uint64_t GetTick(void* context) {
    return static_cast<TestLogger*>(context)->tick_;
  }

  static bool GetLocation(void* context, uint16_t* cs, uint16_t* ip) {
    const TestLogger* self = static_cast<TestLogger*>(context);
    *cs = self->cs_;
    *ip = self->ip_;
    return self->has_location_;
  }

  YLoggerConfig config_ = {};
  YLogger logger_ = {};
  vector<CapturedLine> lines_;
};

}  // namespace

class LoggerTest : public ::testing::Test {};

TEST_F(LoggerTest, WritesAnEventWithItsFields) {
  TestLogger test_logger;

  Y_LOG(
      test_logger.logger(), &kModuleA, Y_LOG_LEVEL_WARN, "unmapped_port_read",
      Y_LOG_HEX16("port", 0x201), Y_LOG_UINT("count", 3),
      Y_LOG_INT("delta", -7), Y_LOG_BOOL("fatal", false),
      Y_LOG_STRING("path", "a b"));

  ASSERT_EQ(test_logger.lines().size(), 1u);
  const CapturedLine& line = test_logger.lines()[0];
  EXPECT_EQ(line.kind, Y_LOG_KIND_LOG);
  EXPECT_EQ(line.module, &kModuleA);
  EXPECT_EQ(line.level, Y_LOG_LEVEL_WARN);
  // No trailing newline - the host decides how lines are separated.
  EXPECT_EQ(
      line.line,
      "kind=log level=warn module=a event=unmapped_port_read port=0x0201 "
      "count=3 delta=-7 fatal=false path=\"a b\"");
  EXPECT_EQ(line.length, line.line.size());
}

TEST_F(LoggerTest, AnEventNeedsNoFields) {
  TestLogger test_logger;
  Y_LOG(test_logger.logger(), &kModuleB, Y_LOG_LEVEL_ERROR, "missing_vram");
  ASSERT_EQ(test_logger.lines().size(), 1u);
  EXPECT_EQ(
      test_logger.lines()[0].line,
      "kind=log level=error module=b event=missing_vram");
}

TEST_F(LoggerTest, FieldWidthsFollowTheirMacros) {
  TestLogger test_logger;
  Y_LOG(
      test_logger.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG, "widths",
      Y_LOG_HEX8("b", 0x9), Y_LOG_HEX16("w", 0x9), Y_LOG_HEX20("a", 0x9),
      Y_LOG_HEX32("d", 0x9));
  ASSERT_EQ(test_logger.lines().size(), 1u);
  EXPECT_EQ(
      test_logger.lines()[0].line,
      "kind=log level=debug module=a event=widths b=0x09 w=0x0009 a=0x00009 "
      "d=0x00000009");
}

TEST_F(LoggerTest, TheSourceAddsTickAndLocation) {
  TestLogger test_logger;
  test_logger.SetSource();
  test_logger.tick_ = 12345;
  test_logger.cs_ = 0xF000;
  test_logger.ip_ = 0xE0A3;

  Y_LOG(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_INFO, "here");
  test_logger.has_location_ = false;
  Y_LOG(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_INFO, "nowhere");

  ASSERT_EQ(test_logger.lines().size(), 2u);
  EXPECT_EQ(
      test_logger.lines()[0].line,
      "kind=log level=info module=a tick=12345 cs=0xF000 ip=0xE0A3 "
      "event=here");
  EXPECT_EQ(
      test_logger.lines()[1].line,
      "kind=log level=info module=a tick=12345 event=nowhere");
}

TEST_F(LoggerTest, SuppressesDisabledModule) {
  TestLogger test_logger;
  test_logger.config()->enabled_modules = YLogModuleMask(&kModuleB);

  Y_LOG(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_ERROR, "suppressed");
  EXPECT_TRUE(test_logger.lines().empty());
  EXPECT_FALSE(
      YLoggerIsEnabled(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_ERROR));

  Y_LOG(test_logger.logger(), &kModuleB, Y_LOG_LEVEL_ERROR, "emitted");
  ASSERT_EQ(test_logger.lines().size(), 1u);
  EXPECT_EQ(test_logger.lines()[0].module, &kModuleB);
  EXPECT_TRUE(
      YLoggerIsEnabled(test_logger.logger(), &kModuleB, Y_LOG_LEVEL_ERROR));
}

TEST_F(LoggerTest, SuppressesLogsAboveMinLevel) {
  TestLogger test_logger;
  test_logger.config()->min_level = Y_LOG_LEVEL_INFO;

  Y_LOG(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_ERROR, "error");
  Y_LOG(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_WARN, "warn");
  Y_LOG(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_INFO, "info");
  Y_LOG(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG, "debug");

  ASSERT_EQ(test_logger.lines().size(), 3u);
  EXPECT_EQ(test_logger.lines()[0].level, Y_LOG_LEVEL_ERROR);
  EXPECT_EQ(test_logger.lines()[1].level, Y_LOG_LEVEL_WARN);
  EXPECT_EQ(test_logger.lines()[2].level, Y_LOG_LEVEL_INFO);
  EXPECT_FALSE(
      YLoggerIsEnabled(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG));
}

TEST_F(LoggerTest, RepliesAreNeverFiltered) {
  TestLogger test_logger;
  test_logger.SetSource();
  test_logger.config()->enabled_modules = 0;
  test_logger.config()->min_level = Y_LOG_LEVEL_ERROR;

  Y_LOG_REPLY(test_logger.logger(), 7, "type", Y_LOG_UINT("keys", 4));
  Y_LOG_REPLY(test_logger.logger(), 7, "type", Y_LOG_STRING("status", "ok"));

  ASSERT_EQ(test_logger.lines().size(), 2u);
  const CapturedLine& line = test_logger.lines()[0];
  EXPECT_EQ(line.kind, Y_LOG_KIND_REPLY);
  EXPECT_EQ(line.module, nullptr);
  // A reply carries neither a tick nor a location: it is an answer, not
  // something that happened in the guest.
  EXPECT_EQ(line.line, "kind=reply id=7 cmd=type keys=4");
  EXPECT_EQ(test_logger.lines()[1].line, "kind=reply id=7 cmd=type status=ok");
}

TEST_F(LoggerTest, ALineTooLongIsTruncatedAndStillValid) {
  TestLogger test_logger;
  const string long_value(Y_LOG_MAX_LINE_LENGTH, 'x');

  Y_LOG(
      test_logger.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG, "long",
      Y_LOG_UINT("before", 1), Y_LOG_STRING("value", long_value.c_str()),
      Y_LOG_UINT("after", 2));

  ASSERT_EQ(test_logger.lines().size(), 1u);
  const CapturedLine& line = test_logger.lines()[0];
  EXPECT_EQ(
      line.line,
      "kind=log level=debug module=a event=long before=1 truncated=true");
  EXPECT_EQ(line.length, line.line.size());
}

TEST_F(LoggerTest, ToleratesMissingCallbacksAndState) {
  TestLogger test_logger;

  // No write_line callback - nothing is emitted and nothing crashes.
  test_logger.config()->write_line = NULL;
  Y_LOG(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG, "no_sink");
  Y_LOG_REPLY(test_logger.logger(), 1, "no_sink");
  EXPECT_TRUE(test_logger.lines().empty());
  EXPECT_FALSE(
      YLoggerIsEnabled(test_logger.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG));

  // A logger with no config, and no logger at all.
  YLogger unconfigured = {};
  YLoggerInit(&unconfigured, NULL);
  Y_LOG(&unconfigured, &kModuleA, Y_LOG_LEVEL_ERROR, "no_config");
  Y_LOG(NULL, &kModuleA, Y_LOG_LEVEL_ERROR, "no_logger");
  Y_LOG_REPLY(&unconfigured, 1, "no_config");
  Y_LOG_REPLY(NULL, 1, "no_logger");
  EXPECT_FALSE(YLoggerIsEnabled(&unconfigured, &kModuleA, Y_LOG_LEVEL_ERROR));
  EXPECT_FALSE(YLoggerIsEnabled(NULL, &kModuleA, Y_LOG_LEVEL_ERROR));
}

TEST_F(LoggerTest, LoggersAreIndependent) {
  TestLogger first;
  TestLogger second;
  second.config()->enabled_modules = YLogModuleMask(&kModuleB);

  Y_LOG(first.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG, "first");
  Y_LOG(second.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG, "dropped");
  Y_LOG(second.logger(), &kModuleB, Y_LOG_LEVEL_DEBUG, "second");

  ASSERT_EQ(first.lines().size(), 1u);
  EXPECT_EQ(first.lines()[0].line, "kind=log level=debug module=a event=first");
  ASSERT_EQ(second.lines().size(), 1u);
  EXPECT_EQ(
      second.lines()[0].line, "kind=log level=debug module=b event=second");
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

TEST_F(LoggerTest, DoesNotEvaluateTheFieldsOfASuppressedLog) {
  TestLogger test_logger;
  test_logger.config()->enabled_modules = 0;

  // The macro must not evaluate to a call that builds the fields. If it did,
  // this counter would be incremented.
  int calls = 0;
  auto count_call = [&calls]() {
    ++calls;
    return 0;
  };
  Y_LOG(
      test_logger.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG, "counted",
      Y_LOG_UINT("n", count_call()));
  EXPECT_EQ(calls, 0);

  YLoggerEnableModule(test_logger.logger(), &kModuleA);
  Y_LOG(
      test_logger.logger(), &kModuleA, Y_LOG_LEVEL_DEBUG, "counted",
      Y_LOG_UINT("n", count_call()));
  EXPECT_EQ(calls, 1);
}

TEST_F(LoggerTest, LevelNames) {
  EXPECT_STREQ(YLogLevelName(Y_LOG_LEVEL_ERROR), "error");
  EXPECT_STREQ(YLogLevelName(Y_LOG_LEVEL_WARN), "warn");
  EXPECT_STREQ(YLogLevelName(Y_LOG_LEVEL_INFO), "info");
  EXPECT_STREQ(YLogLevelName(Y_LOG_LEVEL_DEBUG), "debug");
}
