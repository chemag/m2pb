// Copyright Google Inc. Apache 2.0.

#include <stdio.h>  // for remove
#include <string.h>  // for memset
#include <unistd.h>  // for usleep

#include <tuple>
#include <vector>

#include <gtest/gtest.h>
#include "modulo.h"


class TestableModulo : public Modulo {
 public:
  TestableModulo(int64_t max_value, int64_t invalid)
   : Modulo(max_value, invalid) {}

  using Modulo::WrapCorrection;
};


class ModuloTest : public ::testing::Test {
 protected:
  void SetUp() override {
    modulo_ = new TestableModulo(kMaxValue, kInvalidValue);
  }
  void TearDown() override {
    delete modulo_;
  }
  TestableModulo* modulo_;

  const int64_t kMaxValue = (1LL << 33) - 1;
  const int64_t kInvalidValue = -1;
};

TEST_F(ModuloTest, TestWrapCorrection) {
  // A test of the WrapCorrection() method.
  ASSERT_EQ(0, modulo_->WrapCorrection(0));
  ASSERT_EQ(90000, modulo_->WrapCorrection(90000));
  ASSERT_EQ(0, modulo_->WrapCorrection(kMaxValue + 1));
  ASSERT_EQ(90000, modulo_->WrapCorrection(kMaxValue + 1 + 90000));
  ASSERT_EQ(kMaxValue, modulo_->WrapCorrection(kInvalidValue));
  ASSERT_EQ(kMaxValue - 1, modulo_->WrapCorrection(-2));
  ASSERT_EQ(0, modulo_->WrapCorrection(2 * (kMaxValue + 1) + 0));
  ASSERT_EQ(1, modulo_->WrapCorrection(3 * (kMaxValue + 1) + 1));
  ASSERT_EQ(0, modulo_->WrapCorrection(-kMaxValue - 1));
  ASSERT_EQ(0, modulo_->WrapCorrection(-(2 * (kMaxValue + 1)) + 0));
  ASSERT_EQ(1, modulo_->WrapCorrection(-(3 * (kMaxValue + 1)) + 1));
}

TEST_F(ModuloTest, TestAdd) {
  ASSERT_EQ(0, modulo_->Add(0, 0));
  ASSERT_EQ(123, modulo_->Add(23, 100));
  ASSERT_EQ(100, modulo_->Add(-100, 200));
  ASSERT_EQ(kInvalidValue, modulo_->Add(kInvalidValue, 100));
  ASSERT_EQ(kInvalidValue, modulo_->Add(100, kInvalidValue));
  ASSERT_EQ(kInvalidValue, modulo_->Add(kInvalidValue, kInvalidValue));
}

TEST_F(ModuloTest, TestDiff) {
  const int64_t kHalfMaxValue = kMaxValue >> 1;
  ASSERT_EQ(0, modulo_->Diff(0, 0));
  ASSERT_EQ(23, modulo_->Diff(123, 100));
  ASSERT_EQ(kMaxValue + 1 - 23, modulo_->Diff(100, 123));
  ASSERT_EQ(123456, modulo_->Diff(kMaxValue, kMaxValue - 123456));
  ASSERT_EQ(kMaxValue + 1 - 123456,
            modulo_->Diff(kMaxValue - 123456, kMaxValue));
  ASSERT_EQ(9, modulo_->Diff(modulo_->WrapCorrection(kMaxValue + 9),
                             kMaxValue));
  ASSERT_EQ(kMaxValue + 1 - 9,
            modulo_->Diff(kMaxValue, modulo_->WrapCorrection(kMaxValue + 9)));
  ASSERT_EQ(16234, modulo_->Diff(15000, modulo_->WrapCorrection(-1234)));
  ASSERT_EQ(kHalfMaxValue, modulo_->Diff(kHalfMaxValue, 0));
  ASSERT_EQ(kMaxValue + 1 - kHalfMaxValue,
            modulo_->Diff(0, kHalfMaxValue));
  ASSERT_EQ(kInvalidValue, modulo_->Diff(kInvalidValue, 100));
  ASSERT_EQ(kInvalidValue, modulo_->Diff(100, kInvalidValue));
  ASSERT_EQ(kInvalidValue, modulo_->Diff(kInvalidValue, kInvalidValue));
}

TEST_F(ModuloTest, TestSub) {
  const int64_t kHalfMaxValue = kMaxValue >> 1;
  ASSERT_EQ(0, modulo_->Sub(0, 0));
  ASSERT_EQ(23, modulo_->Sub(123, 100));
  ASSERT_EQ(-23, modulo_->Sub(100, 123));
  ASSERT_EQ(123456, modulo_->Sub(kMaxValue, kMaxValue - 123456));
  ASSERT_EQ(-123456, modulo_->Sub(kMaxValue - 123456, kMaxValue));
  ASSERT_EQ(9, modulo_->Sub(modulo_->WrapCorrection(kMaxValue + 9), kMaxValue));
  ASSERT_EQ(-9, modulo_->Sub(kMaxValue,
                             modulo_->WrapCorrection(kMaxValue + 9)));
  ASSERT_EQ(16234, modulo_->Sub(15000, modulo_->WrapCorrection(-1234)));
  ASSERT_EQ(kHalfMaxValue, modulo_->Sub(kHalfMaxValue, 0));
  ASSERT_EQ(-kHalfMaxValue, modulo_->Sub(0, kHalfMaxValue));
  ASSERT_EQ(kInvalidValue, modulo_->Sub(kInvalidValue, 100));
  ASSERT_EQ(kInvalidValue, modulo_->Sub(100, kInvalidValue));
  ASSERT_EQ(kInvalidValue, modulo_->Sub(kInvalidValue, kInvalidValue));
}

TEST_F(ModuloTest, TestCmp) {
  ASSERT_EQ(0, modulo_->Cmp(0, 0));
  ASSERT_EQ(0, modulo_->Cmp(0, kMaxValue + 1));
  ASSERT_EQ(0, modulo_->Cmp(90000, kMaxValue + 1 + 90000));
  ASSERT_EQ(-1, modulo_->Cmp(0, 1));
  ASSERT_EQ(1, modulo_->Cmp(1, 0));
  ASSERT_EQ(-1, modulo_->Cmp(kMaxValue, 0));
  ASSERT_EQ(1, modulo_->Cmp(0, kMaxValue));
  ASSERT_EQ(-1, modulo_->Cmp(0, (kMaxValue + 1) >> 1));
  ASSERT_EQ(1, modulo_->Cmp(0, ((kMaxValue + 1) >> 1) + 1));
}

TEST_F(ModuloTest, TestCompareEq) {
  // Compare same value
  ASSERT_EQ(0, modulo_->Cmp(0, 0));
  ASSERT_EQ(0, modulo_->Cmp(10, 10));
  ASSERT_EQ(0, modulo_->Cmp(3423410, 3423410));
  ASSERT_EQ(0, modulo_->Cmp(898798798, 898798798));
  ASSERT_EQ(0, modulo_->Cmp(kMaxValue, kMaxValue));
}

TEST_F(ModuloTest, TestCompareLarger) {
 // first value larger than second by 1
  ASSERT_EQ(1, modulo_->Cmp(1, 0));
  ASSERT_EQ(1, modulo_->Cmp(10, 9));
  ASSERT_EQ(1, modulo_->Cmp(3423410, 3423409));
  ASSERT_EQ(1, modulo_->Cmp(898798798, 898798797));
  ASSERT_EQ(1, modulo_->Cmp(kMaxValue, kMaxValue - 1));
  // first value larger than second by a lot
  ASSERT_EQ(1, modulo_->Cmp(10, 5));
  ASSERT_EQ(1, modulo_->Cmp(3423410, 342340));
  ASSERT_EQ(1, modulo_->Cmp(898798798, 689879877));
  ASSERT_EQ(1, modulo_->Cmp(kMaxValue, kMaxValue - 110000));
}

TEST_F(ModuloTest, TestCompareSmaller) {
  // first value smaller than second by 1
  ASSERT_EQ(-1, modulo_->Cmp(0, 1));
  ASSERT_EQ(-1, modulo_->Cmp(10, 11));
  ASSERT_EQ(-1, modulo_->Cmp(3423410, 3423411));
  ASSERT_EQ(-1, modulo_->Cmp(898798798, 898798799));
  ASSERT_EQ(-1, modulo_->Cmp(kMaxValue - 1, kMaxValue));
  // first value smaller than second by a lot
  ASSERT_EQ(-1, modulo_->Cmp(0, 1000));
  ASSERT_EQ(-1, modulo_->Cmp(10, 11000));
  ASSERT_EQ(-1, modulo_->Cmp(423410, 3423411));
  ASSERT_EQ(-1, modulo_->Cmp(498798798, 898798799));
  ASSERT_EQ(-1, modulo_->Cmp(kMaxValue - 100000, kMaxValue));
}

TEST_F(ModuloTest, TestCompareWrapSmaller) {
  // first value smaller than second with wrap around
  // Test edge limit on the 1st value
  ASSERT_EQ(-1, modulo_->Cmp(kMaxValue, 0));
  ASSERT_EQ(-1, modulo_->Cmp(kMaxValue, 1));
  ASSERT_EQ(-1, modulo_->Cmp(kMaxValue, 1100));
  ASSERT_EQ(-1, modulo_->Cmp(kMaxValue, 99999));

  // Test edge limit on the 2nd value
  ASSERT_EQ(-1, modulo_->Cmp(kMaxValue - 1, 0));
  ASSERT_EQ(-1, modulo_->Cmp(kMaxValue - 2, 0));
  ASSERT_EQ(-1, modulo_->Cmp(kMaxValue - 1000, 0));
  ASSERT_EQ(-1, modulo_->Cmp(kMaxValue - 9999, 0));

  // Test close to edge limit on the 2nd value
  ASSERT_EQ(-1, modulo_->Cmp(kMaxValue - 1, 1));
  ASSERT_EQ(-1, modulo_->Cmp(kMaxValue - 2, 1));
  ASSERT_EQ(-1, modulo_->Cmp(kMaxValue - 1000, 1));
  ASSERT_EQ(-1, modulo_->Cmp(kMaxValue - 9999, 1));

  ASSERT_EQ(-1, modulo_->Cmp(kMaxValue - 1, 134234));
  ASSERT_EQ(-1, modulo_->Cmp(kMaxValue - 2, 43213123));
  ASSERT_EQ(-1, modulo_->Cmp(kMaxValue - 1000, 212321));
  ASSERT_EQ(-1, modulo_->Cmp(kMaxValue - 9999, 7842341));
}

TEST_F(ModuloTest, TestCompareWrapBigger) {
  // first value bigger than second with wrap around
  // Test edge limit on the 2nd value
  ASSERT_EQ(1, modulo_->Cmp(0, kMaxValue));
  ASSERT_EQ(1, modulo_->Cmp(1, kMaxValue));
  ASSERT_EQ(1, modulo_->Cmp(1100, kMaxValue));
  ASSERT_EQ(1, modulo_->Cmp(999999, kMaxValue));

  // Test edge limit on the first value
  ASSERT_EQ(1, modulo_->Cmp(0, kMaxValue - 1));
  ASSERT_EQ(1, modulo_->Cmp(0, kMaxValue - 2));
  ASSERT_EQ(1, modulo_->Cmp(0, kMaxValue - 1000));
  ASSERT_EQ(1, modulo_->Cmp(0, kMaxValue - 9999));

  // Test close to edge limit on the first value
  ASSERT_EQ(1, modulo_->Cmp(1, kMaxValue - 1));
  ASSERT_EQ(1, modulo_->Cmp(1, kMaxValue - 2));
  ASSERT_EQ(1, modulo_->Cmp(1, kMaxValue - 1000));
  ASSERT_EQ(1, modulo_->Cmp(1, kMaxValue - 9999));

  ASSERT_EQ(1, modulo_->Cmp(13434, kMaxValue - 1));
  ASSERT_EQ(1, modulo_->Cmp(134234234, kMaxValue - 2));
  ASSERT_EQ(1, modulo_->Cmp(342341, kMaxValue - 1000));
  ASSERT_EQ(1, modulo_->Cmp(743451, kMaxValue - 9999));
}

TEST_F(ModuloTest, TestCmpRangeClosed) {
  ASSERT_EQ(-1, modulo_->CmpRangeClosed(89999, 90000, 91000));
  ASSERT_EQ(0, modulo_->CmpRangeClosed(90000, 90000, 91000));
  ASSERT_EQ(0, modulo_->CmpRangeClosed(90001, 90000, 91000));
  ASSERT_EQ(0, modulo_->CmpRangeClosed(90999, 90000, 91000));
  ASSERT_EQ(0, modulo_->CmpRangeClosed(91000, 90000, 91000));
  ASSERT_EQ(1, modulo_->CmpRangeClosed(91001, 90000, 91000));
  ASSERT_EQ(-1, modulo_->CmpRangeClosed(kMaxValue, 0, 1));
  ASSERT_EQ(0, modulo_->CmpRangeClosed(0, 0, 1));
  ASSERT_EQ(0, modulo_->CmpRangeClosed(1, 0, 1));
  ASSERT_EQ(1, modulo_->CmpRangeClosed(2, 0, 1));
  ASSERT_EQ(1, modulo_->CmpRangeClosed(((kMaxValue + 1) >> 1) - 1, 0, 1));
  ASSERT_EQ(-1, modulo_->CmpRangeClosed((kMaxValue + 1) >> 1, 0, 1));
}

TEST_F(ModuloTest, TestCmpRangeClosedOpen) {
  ASSERT_EQ(-1, modulo_->CmpRangeClosedOpen(89999, 90000, 91000));
  ASSERT_EQ(0, modulo_->CmpRangeClosedOpen(90000, 90000, 91000));
  ASSERT_EQ(0, modulo_->CmpRangeClosedOpen(90001, 90000, 91000));
  ASSERT_EQ(0, modulo_->CmpRangeClosedOpen(90999, 90000, 91000));
  ASSERT_EQ(1, modulo_->CmpRangeClosedOpen(91000, 90000, 91000));
  ASSERT_EQ(1, modulo_->CmpRangeClosedOpen(91001, 90000, 91000));
  ASSERT_EQ(-1, modulo_->CmpRangeClosedOpen(kMaxValue, 0, 1));
  ASSERT_EQ(0, modulo_->CmpRangeClosedOpen(0, 0, 1));
  ASSERT_EQ(1, modulo_->CmpRangeClosedOpen(1, 0, 1));
  ASSERT_EQ(1, modulo_->CmpRangeClosedOpen(2, 0, 1));
  ASSERT_EQ(1, modulo_->CmpRangeClosedOpen(((kMaxValue + 1) >> 1) - 1, 0, 1));
  ASSERT_EQ(-1, modulo_->CmpRangeClosedOpen((kMaxValue + 1) >> 1, 0, 1));
}

TEST_F(ModuloTest, TestRangeOverlap) {
  const uint64_t y1 = 1000;
  const uint64_t y2 = 2000;
  std::vector<std::tuple<int, uint64_t, uint64_t, bool>> test_arr = {
      // [x1, x2] covers [y1, y2]
      std::make_tuple(__LINE__, 0, 4000, true),
      std::make_tuple(__LINE__, 1000, 2000, true),
      std::make_tuple(__LINE__, 999, 2000, true),
      std::make_tuple(__LINE__, 1000, 2001, true),
      std::make_tuple(__LINE__, 999, 2001, true),
      // [x1, x2] is covered by [y1, y2]
      std::make_tuple(__LINE__, 1001, 1999, true),
      std::make_tuple(__LINE__, 1500, 1501, true),
      // y1 is overlapped
      std::make_tuple(__LINE__, 900, 1500, true),
      // y2 is overlapped
      std::make_tuple(__LINE__, 1500, 2100, true),
      // non overlap
      std::make_tuple(__LINE__, 900, 999, false),
      std::make_tuple(__LINE__, 2001, 2100, false),
  };
  for (const auto &item : test_arr) {
    int line;
    uint64_t x1, x2;
    bool expected_overlap;
    std::tie(line, x1, x2, expected_overlap) = item;
    ASSERT_EQ(expected_overlap, modulo_->RangeOverlap(x1, x2, y1, y2))
      << "Line: " << line;
    // overlap is a commutative operation
    ASSERT_EQ(expected_overlap, modulo_->RangeOverlap(y1, y2, x1, x2))
      << "Line: " << line;
  }
}


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
