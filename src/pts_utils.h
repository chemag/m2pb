// Copyright Google Inc. Apache 2.0.

#ifndef PTS_UTILS_H_
#define PTS_UTILS_H_

#include <stdint.h>  // for int64_t, uint8_t, uint16_t, etc

#include "time_utils.h"


const int64_t kPtsInvalid = -1;
const int64_t kPosInvalid = -1;
const int64_t kPtsPerSecond = 90000;
// Largest PTS number before roll over 2^33-1
const int64_t kPtsMaxValue = ((int64_t)1 << 33) - 1;
const int64_t kHalfkPtsMaxValue = (kPtsMaxValue >> 1);

//
// Time-conversion functions. Note that the PTS values do not need to be in the
// valid PTS range, they may be negative or larger than kPtsMaxValue.
//

static inline int64_t PtsToSeconds(int64_t pts) {
  return pts / kPtsPerSecond;
}

static inline int64_t SecondsToPts(int64_t secs) {
  return secs * kPtsPerSecond;
}

static inline int64_t SecondsToPtsD(double secs) {
  return (int64_t)(secs * kPtsPerSecond);
}

static inline int64_t PtsToMilliseconds(int64_t pts) {
  return (pts * kMsecsPerSec) / kPtsPerSecond;
}

static inline int64_t MillisecondsToPts(int64_t msecs) {
  return (msecs * kPtsPerSecond) / kMsecsPerSec;
}

static inline int64_t PtsToMicroseconds(int64_t pts) {
  return (pts * kUsecsPerSec) / kPtsPerSecond;
}

static inline int64_t MicrosecondsToPts(int64_t usecs) {
  return (usecs * kPtsPerSecond) / kUsecsPerSec;
}

static inline int64_t PtsWrapCorrection(int64_t pts) {
  return ((pts % (kPtsMaxValue + 1)) + (kPtsMaxValue + 1)) % (kPtsMaxValue + 1);
}

static inline int64_t PtsAdd(int64_t x, int64_t y) {
  if (x == kPtsInvalid || y == kPtsInvalid)
    return kPtsInvalid;
  return PtsWrapCorrection(x + y);
}

// Returns (x-y) in the range [0..kPtsMaxValue].
static inline int64_t PtsDiff(int64_t x, int64_t y) {
  if (x == kPtsInvalid || y == kPtsInvalid)
    return kPtsInvalid;
  return PtsWrapCorrection(x - y);
}

// Returns (x-y) in the range [-kHalfkPtsMaxValue..kHalfkPtsMaxValue].
static inline int64_t PtsSub(int64_t x, int64_t y) {
  if (x == kPtsInvalid || y == kPtsInvalid)
    return kPtsInvalid;
  int64_t diff = PtsDiff(x, y);
  if (diff > ((kPtsMaxValue + 1) >> 1))
    return diff - (kPtsMaxValue + 1);
  return diff;
}

// Returns an integer less than, equal to, or greater than zero if x is
// found, respectively, to be less than, to match, or be greater than y.
static inline int PtsCmp(int64_t x, int64_t y) {
  int64_t diff = PtsWrapCorrection(y - x);
  if (diff == 0)
    // y - x == 0
    return 0;
  else if (diff > ((kPtsMaxValue + 1) >> 1))
    // y - x < 0
    return 1;
  else
    // y - x > 0
    return -1;
}

// Returns an integer less than, equal to, or greater than zero if x is
// found, respectively, to be less than y1, in [y1, y2], or greater than y2.
static inline int PtsCmpRangeClosed(int64_t x, int64_t y1, int64_t y2) {
  if (PtsCmp(x, y1) < 0)
    // x < y1
    return -1;
  else if ((PtsCmp(x, y1) >= 0) && (PtsCmp(x, y2) <= 0))
    // y1 <= x <= y2
    return 0;
  else
    // y2 < x
    return 1;
}

// Returns an integer less than, equal to, or greater than zero if x is
// found, respectively, to be less than y1, in [y1, y2), or greater or equal to
// y2.
static inline int PtsCmpRangeClosedOpen(int64_t x, int64_t y1, int64_t y2) {
  if (PtsCmp(x, y1) < 0)
    // x < y1
    return -1;
  else if ((PtsCmp(x, y1) >= 0) && (PtsCmp(x, y2) < 0))
    // y1 <= x < y2
    return 0;
  else
    // y2 <= x
    return 1;
}

// Returns whether the pts ranges ([x1, x2] and [y1, y2]) overlap at all.
static inline bool PtsRangeOverlap(int64_t x1, int64_t x2, int64_t y1,
                                   int64_t y2) {
  if (PtsCmpRangeClosed(y1, x1, x2) == 0 || PtsCmpRangeClosed(y2, x1, x2) == 0)
    return true;
  return false;
}

#endif  // PTS_UTILS_H_
