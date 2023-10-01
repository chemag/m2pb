// Copyright Google Inc. Apache 2.0.

#ifndef MODULO_H_
#define MODULO_H_

#include <stdint.h>  // for int64_t, uint8_t, uint16_t, etc

class Modulo {
 public:
  Modulo(int64_t max_value, int64_t invalid) {
    max_value_ = max_value;
    half_max_value_ = max_value_ >> 1;
    invalid_ = invalid;
  }

  // Returns (x+y) in the range [0..max_value_].
  inline int64_t Add(int64_t x, int64_t y) {
    if (x == invalid_ || y == invalid_) {
      return invalid_;
    }
    return WrapCorrection(x + y);
  }

  // Returns (x-y) in the range [0..max_value_].
  inline int64_t Diff(int64_t x, int64_t y) {
    if (x == invalid_ || y == invalid_) {
      return invalid_;
    }
    return WrapCorrection(x - y);
  }

  // Returns (x-y) in the range [-kHalfmax_value_..kHalfmax_value_].
  inline int64_t Sub(int64_t x, int64_t y) {
    if (x == invalid_ || y == invalid_) {
      return invalid_;
    }
    int64_t diff = Diff(x, y);
    if (diff > ((max_value_ + 1) >> 1)) {
      return diff - (max_value_ + 1);
    }
    return diff;
  }

  // Returns an integer less than, equal to, or greater than zero if x is
  // found, respectively, to be less than, to match, or be greater than y.
  inline int Cmp(int64_t x, int64_t y) {
    int64_t diff = WrapCorrection(y - x);
    if (diff == 0) {
      // y - x == 0
      return 0;
    } else if (diff > ((max_value_ + 1) >> 1)) {
      // y - x < 0
      return 1;
    } else {
      // y - x > 0
      return -1;
    }
  }

  // Returns an integer less than, equal to, or greater than zero if x is
  // found, respectively, to be less than y1, in [y1, y2], or greater than y2.
  inline int CmpRangeClosed(int64_t x, int64_t y1, int64_t y2) {
    if (Cmp(x, y1) < 0) {
      // x < y1
      return -1;
    } else if ((Cmp(x, y1) >= 0) && (Cmp(x, y2) <= 0)) {
      // y1 <= x <= y2
      return 0;
    } else {
      // y2 < x
      return 1;
    }
  }

  // Returns an integer less than, equal to, or greater than zero if x is
  // found, respectively, to be less than y1, in [y1, y2), or greater or
  // equal to y2.
  inline int CmpRangeClosedOpen(int64_t x, int64_t y1, int64_t y2) {
    if (Cmp(x, y1) < 0) {
      // x < y1
      return -1;
    } else if ((Cmp(x, y1) >= 0) && (Cmp(x, y2) < 0)) {
      // y1 <= x < y2
      return 0;
    } else {
      // y2 <= x
      return 1;
    }
  }

  // Returns whether the x ranges ([x1, x2] and [y1, y2]) overlap at all.
  inline bool RangeOverlap(int64_t x1, int64_t x2, int64_t y1, int64_t y2) {
    if (Cmp(y2, x1) < 0 || Cmp(y1, x2) > 0) {
      return false;
    }
    return true;
  }

 private:
  // Returns x in the range [0..max_value_].
  inline int64_t WrapCorrection(int64_t x) {
    return ((x % (max_value_ + 1)) + (max_value_ + 1)) % (max_value_ + 1);
  }

  int64_t max_value_;
  int64_t half_max_value_;
  int64_t invalid_;

  friend class TestableModulo;
};

#endif  // MODULO_H_
