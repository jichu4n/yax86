#include "src/util/snprintf.h"

#include <gtest/gtest.h>

TEST(SNPrintFTest, BasicString) {
  char buffer[100];
  int ret = SNPrintF(buffer, sizeof(buffer), "Hello World");
  EXPECT_EQ(ret, 11);
  EXPECT_STREQ(buffer, "Hello World");
}

TEST(SNPrintFTest, BasicInt) {
  char buffer[100];
  int ret = SNPrintF(buffer, sizeof(buffer), "Value: %d", 123);
  EXPECT_EQ(ret, 10);
  EXPECT_STREQ(buffer, "Value: 123");
}

TEST(SNPrintFTest, NegativeInt) {
  char buffer[100];
  int ret = SNPrintF(buffer, sizeof(buffer), "Value: %d", -123);
  EXPECT_EQ(ret, 11);
  EXPECT_STREQ(buffer, "Value: -123");
}

TEST(SNPrintFTest, Hex) {
  char buffer[100];
  int ret = SNPrintF(buffer, sizeof(buffer), "Hex: %x", 0xABCD);
  EXPECT_EQ(ret, 9);
  EXPECT_STREQ(buffer, "Hex: abcd");

  ret = SNPrintF(buffer, sizeof(buffer), "HEX: %X", 0xABCD);
  EXPECT_EQ(ret, 9);
  EXPECT_STREQ(buffer, "HEX: ABCD");
}

TEST(SNPrintFTest, Padding) {
  char buffer[100];
  int ret = SNPrintF(buffer, sizeof(buffer), "%05d", 123);
  EXPECT_EQ(ret, 5);
  EXPECT_STREQ(buffer, "00123");

  ret = SNPrintF(buffer, sizeof(buffer), "%5d", 123);
  EXPECT_EQ(ret, 5);
  EXPECT_STREQ(buffer, "  123");
}

TEST(SNPrintFTest, NegativePadding) {
  char buffer[100];
  int ret = SNPrintF(buffer, sizeof(buffer), "%05d", -12);
  EXPECT_EQ(ret, 5);
  EXPECT_STREQ(buffer, "-0012");
}

TEST(SNPrintFTest, Truncation) {
  char buffer[5];
  int ret = SNPrintF(buffer, sizeof(buffer), "Hello World");
  EXPECT_EQ(ret, 11); // Returns needed size
  EXPECT_STREQ(buffer, "Hell"); // Truncated
}

TEST(SNPrintFTest, Pointer) {
  char buffer[100];
  void* ptr = (void*)0x1234;
  int ret = SNPrintF(buffer, sizeof(buffer), "%p", ptr);
  EXPECT_GT(ret, 2);
  // Check for 0x prefix
  EXPECT_EQ(buffer[0], '0');
  EXPECT_EQ(buffer[1], 'x');
  // Check remaining characters are valid hex digits
  for (int i = 2; i < ret; ++i) {
    char c = buffer[i];
    bool is_hex =
        (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    EXPECT_TRUE(is_hex) << "Char at " << i << " is " << c;
  }
}

TEST(SNPrintFTest, Modifiers) {
  char buffer[100];
  long val = 1234567890L;
  int ret = SNPrintF(buffer, sizeof(buffer), "%ld", val);
  EXPECT_EQ(ret, 10);
  EXPECT_STREQ(buffer, "1234567890");
}

TEST(SNPrintFTest, SizeT) {
  char buffer[100];
  size_t val = 12345;
  int ret = SNPrintF(buffer, sizeof(buffer), "%zu", val);
  EXPECT_EQ(ret, 5);
  EXPECT_STREQ(buffer, "12345");
}

TEST(SNPrintFTest, StringPadding) {
    char buffer[100];
    int ret = SNPrintF(buffer, sizeof(buffer), "%5s", "Hi");
    EXPECT_EQ(ret, 5);
    EXPECT_STREQ(buffer, "   Hi");
}
