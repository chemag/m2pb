#!/usr/bin/env python

# Copyright Google Inc. Apache 2.0.

kPtsMaxValue = (1 << 33) - 1
kPtsInvalid = -1
kPtsPerSecond = 90000
kMsecsPerSec = 1000
kUsecsPerSec = 1000000

def pts_wrap_correction(pts):
  return ((pts % (kPtsMaxValue + 1)) + (kPtsMaxValue + 1)) % (kPtsMaxValue + 1)

def pts_add(x, y):
  if (x == kPtsInvalid or y == kPtsInvalid):
    return kPtsInvalid
  return pts_wrap_correction(x + y)

def pts_sub(x, y):
  if (x == kPtsInvalid or y == kPtsInvalid):
    return kPtsInvalid
  return pts_wrap_correction(x - y)

def pts_diff(x, y):
  if (x == kPtsInvalid or y == kPtsInvalid):
    return kPtsInvalid
  diff = pts_wrap_correction(x - y)
  if (diff > (kPtsMaxValue + 1) / 2):
    diff = -pts_sub(kPtsMaxValue, diff)
  return diff

# Returns an integer less than, equal to, or greater than zero if x is
# found, respectively, to be less than, to match, or be greater than y.
def pts_cmp(x, y):
  diff = pts_wrap_correction(y - x);
  if diff == 0:
    # y - x == 0
    return 0
  elif diff > ((kPtsMaxValue + 1) >> 1):
    # y - x < 0
    return 1
  else:
    # y - x > 0
    return -1

def pts_max(x, y):
  if x == kPtsInvalid:
    return y
  if y == kPtsInvalid:
    return x
  if pts_cmp(x, y) < 0:
    return y
  return x

def secs_to_usecs(secs):
  return secs * kUsecsPerSec

def usecs_to_secs(usecs):
  return usecs / kUsecsPerSec

def pts_to_seconds(pts):
  return pts / kPtsPerSecond

def seconds_to_pts(secs):
  return secs * kPtsPerSecond

def pts_to_millisecond(pts):
  return (pts * kMsecsPerSec) / kPtsPerSecond

def milliseconds_to_pts(usecs):
  return (usecs * kPtsPerSecond) / kMsecsPerSec

def pts_to_microsecond(pts):
  return (pts * kUsecsPerSec) / kPtsPerSecond

def microseconds_to_pts(usecs):
  return (usecs * kPtsPerSecond) / kUsecsPerSec
