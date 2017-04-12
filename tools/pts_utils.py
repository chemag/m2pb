#!/usr/bin/env python

# Copyright Google Inc. Apache 2.0.

kPtsMaxValue = (1 << 33) - 1
kHalfkPtsMaxValue = (kPtsMaxValue >> 1)
kPtsInvalid = -1
kPtsPerSecond = 90000
kMsecsPerSec = 1000
kUsecsPerSec = 1000000


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
