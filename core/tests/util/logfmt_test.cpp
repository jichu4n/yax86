#include "src/util/logfmt.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

using namespace std;

namespace {

// A line built into a buffer of a given size.
class Line {
 public:
  explicit Line(size_t size = 256) : buffer_(size, '\xAA') {
    YLogfmtLineInit(&line_, &buffer_[0], buffer_.size());
  }

  YLogfmtLine* line() { return &line_; }

  string Finish() {
    const size_t length = YLogfmtLineFinish(&line_);
    EXPECT_EQ(length, string(line_.buffer).size());
    // Nothing is written past the terminating null byte.
    EXPECT_LT(length, buffer_.size());
    return string(line_.buffer, length);
  }

 private:
  string buffer_;
  YLogfmtLine line_;
};

string StringValue(const char* value) {
  Line line;
  YLogfmtLineAddString(line.line(), "k", value);
  return line.Finish();
}

}  // namespace

TEST(LogfmtTest, EmptyLine) {
  Line line;
  EXPECT_EQ(line.Finish(), "");
}

TEST(LogfmtTest, PairsAreSeparatedBySpaces) {
  Line line;
  YLogfmtLineAddString(line.line(), "kind", "log");
  YLogfmtLineAddUInt(line.line(), "tick", 42);
  YLogfmtLineAddBool(line.line(), "ok", true);
  EXPECT_EQ(line.Finish(), "kind=log tick=42 ok=true");
}

TEST(LogfmtTest, Numbers) {
  Line line;
  YLogfmtLineAddUInt(line.line(), "zero", 0);
  YLogfmtLineAddUInt(line.line(), "max", UINT64_MAX);
  YLogfmtLineAddInt(line.line(), "negative", -7);
  YLogfmtLineAddInt(line.line(), "min", INT64_MIN);
  YLogfmtLineAddInt(line.line(), "max_signed", INT64_MAX);
  EXPECT_EQ(
      line.Finish(),
      "zero=0 max=18446744073709551615 negative=-7 "
      "min=-9223372036854775808 max_signed=9223372036854775807");
}

TEST(LogfmtTest, HexIsPrefixedPaddedAndUpperCase) {
  Line line;
  YLogfmtLineAddHex(line.line(), "port", 0x201, 4);
  YLogfmtLineAddHex(line.line(), "byte", 0x9A, 2);
  YLogfmtLineAddHex(line.line(), "address", 0xF6000, 5);
  // A value wider than the digits asked for is written whole.
  YLogfmtLineAddHex(line.line(), "wide", 0x12345, 2);
  EXPECT_EQ(
      line.Finish(), "port=0x0201 byte=0x9A address=0xF6000 wide=0x12345");
}

TEST(LogfmtTest, Bools) {
  Line line;
  YLogfmtLineAddBool(line.line(), "yes", true);
  YLogfmtLineAddBool(line.line(), "no", false);
  EXPECT_EQ(line.Finish(), "yes=true no=false");
}

TEST(LogfmtTest, PlainStringsAreBare) {
  EXPECT_EQ(StringValue("unmapped_port_read"), "k=unmapped_port_read");
  EXPECT_EQ(StringValue("a/b.img"), "k=a/b.img");
}

TEST(LogfmtTest, StringsThatNeedQuotesAreQuoted) {
  EXPECT_EQ(StringValue(""), "k=\"\"");
  EXPECT_EQ(StringValue(nullptr), "k=\"\"");
  EXPECT_EQ(StringValue("A>DIR C:"), "k=\"A>DIR C:\"");
  EXPECT_EQ(StringValue("a=b"), "k=\"a=b\"");
}

TEST(LogfmtTest, QuotesAndBackslashesAreEscaped) {
  EXPECT_EQ(StringValue("say \"hi\""), "k=\"say \\\"hi\\\"\"");
  EXPECT_EQ(StringValue("C:\\DOS"), "k=\"C:\\\\DOS\"");
}

TEST(LogfmtTest, BytesOutsidePrintableASCIIAreEscaped) {
  EXPECT_EQ(StringValue("DIR\r"), "k=\"DIR\\x0D\"");
  EXPECT_EQ(StringValue("\t"), "k=\"\\x09\"");
  EXPECT_EQ(StringValue("\x7F"), "k=\"\\x7F\"");
  // CP437 box drawing, as the guest's screen holds it.
  EXPECT_EQ(StringValue("\xC9\xCD\xBB"), "k=\"\\xC9\\xCD\\xBB\"");
}

TEST(LogfmtTest, APairThatDoesNotFitIsDroppedAndTheLineMarked) {
  // Room for "a=1" and the marker, but not for "b=2" as well.
  Line line(sizeof("a=1") + Y_LOGFMT_RESERVED_LENGTH);
  YLogfmtLineAddUInt(line.line(), "a", 1);
  YLogfmtLineAddUInt(line.line(), "b", 2);
  EXPECT_EQ(line.Finish(), "a=1 truncated=true");
}

TEST(LogfmtTest, NothingIsAddedAfterAPairIsDropped) {
  Line line(sizeof("a=1") + Y_LOGFMT_RESERVED_LENGTH);
  YLogfmtLineAddString(line.line(), "long", "a value far too long to fit");
  // This one would fit, but the line already lost the pair before it.
  YLogfmtLineAddUInt(line.line(), "a", 1);
  EXPECT_EQ(line.Finish(), "truncated=true");
}

TEST(LogfmtTest, APairThatExactlyFillsTheRoomIsKept) {
  // The reserved length covers the marker and the terminating null byte, so
  // this leaves room for exactly the two pairs.
  Line line(sizeof("a=1 b=2") - 1 + Y_LOGFMT_RESERVED_LENGTH);
  YLogfmtLineAddUInt(line.line(), "a", 1);
  YLogfmtLineAddUInt(line.line(), "b", 2);
  EXPECT_EQ(line.Finish(), "a=1 b=2");
}

TEST(LogfmtTest, ABufferTooSmallForTheMarkerTakesNothing) {
  Line line(4);
  YLogfmtLineAddUInt(line.line(), "a", 1);
  EXPECT_EQ(line.Finish(), "");
}
