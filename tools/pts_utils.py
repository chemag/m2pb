#!/usr/bin/env python

# Copyright Google Inc. Apache 2.0.

kPtsMaxValue = (1 << 33) - 1
kHalfkPtsMaxValue = (kPtsMaxValue >> 1)
kPtsInvalid = -1
kPtsPerSecond = 90000
kMsecsPerSec = 1000
kUsecsPerSec = 1000000

# Returns pts in the range [0..kPtsMaxValue].
def pts_wrap_correction(pts):
  return ((pts % (kPtsMaxValue + 1)) + (kPtsMaxValue + 1)) % (kPtsMaxValue + 1)

# Returns (x+y) in the range [0..kPtsMaxValue].
def pts_add(x, y):
  if x == kPtsInvalid or y == kPtsInvalid:
    return kPtsInvalid
  return pts_wrap_correction(x + y)

# Returns (x-y) in the range [-kHalfkPtsMaxValue..kHalfkPtsMaxValue].
def pts_diff(x, y):
  if x == kPtsInvalid or y == kPtsInvalid:
    return kPtsInvalid
  return pts_wrap_correction(x - y)

# Returns (x-y) in the range [0..kPtsMaxValue].
def pts_sub(x, y):
  if x == kPtsInvalid or y == kPtsInvalid:
    return kPtsInvalid
  diff = pts_wrap_correction(x - y)
  if diff > ((kPtsMaxValue + 1) >> 1):
    return diff - (kPtsMaxValue + 1)
  return diff

# Returns an integer less than, equal to, or greater than zero if x is
# found, respectively, to be less than, to match, or be greater than y.
def pts_cmp(x, y):
  diff = pts_wrap_correction(y - x)
  if diff == 0:
    # y - x == 0
    return 0
  elif diff > ((kPtsMaxValue + 1) >> 1):
    # y - x < 0
    return 1
  else:
    # y - x > 0
    return -1

# Returns an integer less than, equal to, or greater than zero if x is
# found, respectively, to be less than y1, in [y1, y2], or greater than y2.
def pts_cmp_range_closed(x, y1, y2):
  if pts_cmp(x, y1) < 0:
    # x < y1
    return -1
  elif pts_cmp(x, y1) >= 0 and pts_cmp(x, y2) <= 0:
    # y1 <= x <= y2
    return 0
  else:
    # y2 < x
    return 1

# Returns an integer less than, equal to, or greater than zero if x is
# found, respectively, to be less than y1, in [y1, y2), or greater or equal to
# y2.
def pts_cmp_range_closed_open(x, y1, y2):
  if pts_cmp(x, y1) < 0:
    # x < y1
    return -1
  elif pts_cmp(x, y1) >= 0 and pts_cmp(x, y2) < 0:
    # y1 <= x < y2
    return 0
  else:
    # y2 <= x
    return 1

# Returns whether the pts ranges ([x1, x2] and [y1, y2]) overlap at all.
def pts_range_overlap(x1, x2, y1, y2):
  if pts_cmp(y2, x1) < 0 or pts_cmp(y1, x2) > 0:
    return False
  return True

# Map a pts into the same time line as a reference pts, to be able to compare
# them easily. A timeline is essentially one "run" from 0 to MAX_PTS, and
# if two pts values are separated by a wrap-around point, they are in two
# different timelines and cannot be compared directly.
# Note that the pts values cannot be apart by more MAX_PTS/2 or else it is
# not possible to correctly map them (aliasing effect).
# Returns the mapped pts value. In most cases (both are on the same
# timeline) it will be the same as the input pts, but in the wrapped
# case, the mapped pts may be negative or larger than MAX_PTS.
def map_pts_into_same_timeline(pts, ref_pts):
  # The two PTS values have a wrapping point between them if they are more
  # than MAX_PTS / 2 apart.
  if pts > ref_pts + kHalfkPtsMaxValue:
    # target -> wrap-point -> ref
    return pts - (kPtsMaxValue + 1)
  elif ref_pts > pts + kHalfkPtsMaxValue:
    # ref -> wrap-point -> target
    return pts + (kPtsMaxValue + 1)
  return pts

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
