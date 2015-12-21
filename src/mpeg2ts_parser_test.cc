// Copyright Google Inc. Apache 2.0.

#include <stdio.h>  // for remove
#include <string.h>  // for memset
#include <unistd.h>  // for usleep

#include <gtest/gtest.h>

class TestableMpeg2TsParser : public Mpeg2TsParser {
 public:
  TestableMpeg2TsParser() : Mpeg2TsParser() {}
  using Mpeg2TsParser::stats_;
};

class Mpeg2TsParserTest : public ::testing::Test {
 protected:
  void SetUp() override {
  }

  void TearDown() override {
  }

  TestableMpeg2TsParser mpeg2ts_parser_;
};

// TODO(chema): add some tests. Really.

TEST_F(Mpeg2TsParserTest, SimpleTest) {
  EXPECT_EQ(0, 0);
}
