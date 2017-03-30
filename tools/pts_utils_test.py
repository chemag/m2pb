#!/usr/bin/env python

"""Unit tests for pts_util.py."""

from pts_utils import kPtsMaxValue, kHalfkPtsMaxValue, kPtsInvalid
from pts_utils import pts_wrap_correction, pts_cmp, pts_add, pts_diff, pts_sub
from pts_utils import pts_cmp_range_closed, pts_cmp_range_closed_open
from pts_utils import pts_range_overlap, map_pts_into_same_timeline

import unittest


class MyTest(unittest.TestCase):
  def testPtsWrapCorrection(self):
    """A test of the pts_wrap_correction() function."""
    self.assertEqual(0, pts_wrap_correction(0))
    self.assertEqual(90000, pts_wrap_correction(90000))
    self.assertEqual(0, pts_wrap_correction(kPtsMaxValue + 1))
    self.assertEqual(90000, pts_wrap_correction(kPtsMaxValue + 1 + 90000))
    self.assertEqual(kPtsMaxValue, pts_wrap_correction(kPtsInvalid))
    self.assertEqual(kPtsMaxValue - 1, pts_wrap_correction(-2))
    self.assertEqual(0, pts_wrap_correction(2 * (kPtsMaxValue + 1) + 0))
    self.assertEqual(1, pts_wrap_correction(3 * (kPtsMaxValue + 1) + 1))
    self.assertEqual(0, pts_wrap_correction(-kPtsMaxValue - 1))
    self.assertEqual(0, pts_wrap_correction(-(2 * (kPtsMaxValue + 1)) + 0))
    self.assertEqual(1, pts_wrap_correction(-(3 * (kPtsMaxValue + 1)) + 1))

  def testPtsCmp(self):
    self.assertEqual(0, pts_cmp(0, 0))
    self.assertEqual(0, pts_cmp(0, kPtsMaxValue + 1))
    self.assertEqual(0, pts_cmp(90000, kPtsMaxValue + 1 + 90000))
    self.assertEqual(-1, pts_cmp(0, 1))
    self.assertEqual(1, pts_cmp(1, 0))
    self.assertEqual(-1, pts_cmp(kPtsMaxValue, 0))
    self.assertEqual(1, pts_cmp(0, kPtsMaxValue))
    self.assertEqual(-1, pts_cmp(0, (kPtsMaxValue + 1) >> 1))
    self.assertEqual(1, pts_cmp(0, ((kPtsMaxValue + 1) >> 1) + 1))

  def testPtsCmpRangeClosed(self):
    self.assertEqual(-1, pts_cmp_range_closed(89999, 90000, 91000))
    self.assertEqual(0, pts_cmp_range_closed(90000, 90000, 91000))
    self.assertEqual(0, pts_cmp_range_closed(90001, 90000, 91000))
    self.assertEqual(0, pts_cmp_range_closed(90999, 90000, 91000))
    self.assertEqual(0, pts_cmp_range_closed(91000, 90000, 91000))
    self.assertEqual(1, pts_cmp_range_closed(91001, 90000, 91000))
    self.assertEqual(-1, pts_cmp_range_closed(kPtsMaxValue, 0, 1))
    self.assertEqual(0, pts_cmp_range_closed(0, 0, 1))
    self.assertEqual(0, pts_cmp_range_closed(1, 0, 1))
    self.assertEqual(1, pts_cmp_range_closed(2, 0, 1))
    self.assertEqual(1, pts_cmp_range_closed(((kPtsMaxValue + 1) >> 1) - 1,
                                             0, 1))
    self.assertEqual(-1, pts_cmp_range_closed((kPtsMaxValue + 1) >> 1, 0, 1))

  def testPtsCmpRangeClosedOpen(self):
    self.assertEqual(-1, pts_cmp_range_closed_open(89999, 90000, 91000))
    self.assertEqual(0, pts_cmp_range_closed_open(90000, 90000, 91000))
    self.assertEqual(0, pts_cmp_range_closed_open(90001, 90000, 91000))
    self.assertEqual(0, pts_cmp_range_closed_open(90999, 90000, 91000))
    self.assertEqual(1, pts_cmp_range_closed_open(91000, 90000, 91000))
    self.assertEqual(1, pts_cmp_range_closed_open(91001, 90000, 91000))
    self.assertEqual(-1, pts_cmp_range_closed_open(kPtsMaxValue, 0, 1))
    self.assertEqual(0, pts_cmp_range_closed_open(0, 0, 1))
    self.assertEqual(1, pts_cmp_range_closed_open(1, 0, 1))
    self.assertEqual(1, pts_cmp_range_closed_open(2, 0, 1))
    self.assertEqual(1, pts_cmp_range_closed_open(((kPtsMaxValue + 1) >> 1) - 1,
                                                  0, 1))
    self.assertEqual(-1, pts_cmp_range_closed_open((kPtsMaxValue + 1) >> 1,
                                                  0, 1))

  def testPtsRangeOverlap(self):
    y1 = 1000
    y2 = 2000
    test_arr = [
        # [x1, x2] covers [y1, y2]
        [0, 4000, True],
        [1000, 2000, True],
        [999, 2000, True],
        [1000, 2001, True],
        [999, 2001, True],
        # [x1, x2] is covered by [y1, y2]
        [1001, 1999, True],
        [1500, 1501, True],
        # y1 is overlapped
        [900, 1500, True],
        # y2 is overlapped
        [1500, 2100, True],
        # non overlap
        [900,  999, False],
        [2001, 2100, False],
    ]
    for item in test_arr:
      x1, x2, expected_overlap = item
      self.assertEqual(expected_overlap, pts_range_overlap(x1, x2, y1, y2))
      # overlap is a commutative operation
      self.assertEqual(expected_overlap, pts_range_overlap(y1, y2, x1, x2))

  def testPtsAdd(self):
    self.assertEqual(0, pts_add(0, 0))
    self.assertEqual(123, pts_add(23, 100))
    self.assertEqual(100, pts_add(-100, 200))
    self.assertEqual(kPtsInvalid, pts_add(kPtsInvalid, 100))
    self.assertEqual(kPtsInvalid, pts_add(100, kPtsInvalid))
    self.assertEqual(kPtsInvalid, pts_add(kPtsInvalid, kPtsInvalid))

  def testPtsDiff(self):
    self.assertEqual(0, pts_diff(0, 0))
    self.assertEqual(23, pts_diff(123, 100))
    self.assertEqual(kPtsMaxValue + 1 - 23, pts_diff(100, 123))
    self.assertEqual(123456, pts_diff(kPtsMaxValue, kPtsMaxValue - 123456))
    self.assertEqual(kPtsMaxValue+1-123456,
                     pts_diff(kPtsMaxValue - 123456, kPtsMaxValue))
    self.assertEqual(9, pts_diff(pts_wrap_correction(kPtsMaxValue + 9),
                     kPtsMaxValue))
    self.assertEqual(kPtsMaxValue+1-9,
                     pts_diff(kPtsMaxValue,
                     pts_wrap_correction(kPtsMaxValue + 9)))
    self.assertEqual(16234, pts_diff(15000, pts_wrap_correction(-1234)))
    self.assertEqual(kHalfkPtsMaxValue, pts_diff(kHalfkPtsMaxValue, 0))
    self.assertEqual(kPtsMaxValue + 1 - kHalfkPtsMaxValue,
                     pts_diff(0, kHalfkPtsMaxValue))
    self.assertEqual(kPtsInvalid, pts_diff(kPtsInvalid, 100))
    self.assertEqual(kPtsInvalid, pts_diff(100, kPtsInvalid))
    self.assertEqual(kPtsInvalid, pts_diff(kPtsInvalid, kPtsInvalid))

  def testPtsSub(self):
    self.assertEqual(0, pts_sub(0, 0))
    self.assertEqual(23, pts_sub(123, 100))
    self.assertEqual(-23, pts_sub(100, 123))
    self.assertEqual(123456, pts_sub(kPtsMaxValue, kPtsMaxValue - 123456))
    self.assertEqual(-123456, pts_sub(kPtsMaxValue - 123456, kPtsMaxValue))
    self.assertEqual(9, pts_sub(pts_wrap_correction(kPtsMaxValue + 9),
                                kPtsMaxValue))
    self.assertEqual(-9, pts_sub(kPtsMaxValue,
                                 pts_wrap_correction(kPtsMaxValue + 9)))
    self.assertEqual(16234, pts_sub(15000, pts_wrap_correction(-1234)))
    self.assertEqual(kHalfkPtsMaxValue, pts_sub(kHalfkPtsMaxValue, 0))
    self.assertEqual(-kHalfkPtsMaxValue, pts_sub(0, kHalfkPtsMaxValue))
    self.assertEqual(kPtsInvalid, pts_sub(kPtsInvalid, 100))
    self.assertEqual(kPtsInvalid, pts_sub(100, kPtsInvalid))
    self.assertEqual(kPtsInvalid, pts_sub(kPtsInvalid, kPtsInvalid))

  def testComparePtsEq(self):
    # Compare same PTS value
    self.assertEqual(0, pts_cmp(0, 0))
    self.assertEqual(0, pts_cmp(10, 10))
    self.assertEqual(0, pts_cmp(3423410, 3423410))
    self.assertEqual(0, pts_cmp(898798798, 898798798))
    self.assertEqual(0, pts_cmp(kPtsMaxValue, kPtsMaxValue))

  def testComparePtsLarger(self):
    # first PTS larger than second PTS by 1
    self.assertEqual(1, pts_cmp(1, 0))
    self.assertEqual(1, pts_cmp(10, 9))
    self.assertEqual(1, pts_cmp(3423410, 3423409))
    self.assertEqual(1, pts_cmp(898798798, 898798797))
    self.assertEqual(1, pts_cmp(kPtsMaxValue, kPtsMaxValue - 1))
    # first PTS larger than second PTS by a lot
    self.assertEqual(1, pts_cmp(10, 5))
    self.assertEqual(1, pts_cmp(3423410, 342340))
    self.assertEqual(1, pts_cmp(898798798, 689879877))
    self.assertEqual(1, pts_cmp(kPtsMaxValue, kPtsMaxValue - 110000))

  def testComparePtsSmaller(self):
    # first PTS smaller than second PTS by 1
    self.assertEqual(-1, pts_cmp(0, 1))
    self.assertEqual(-1, pts_cmp(10, 11))
    self.assertEqual(-1, pts_cmp(3423410, 3423411))
    self.assertEqual(-1, pts_cmp(898798798, 898798799))
    self.assertEqual(-1, pts_cmp(kPtsMaxValue - 1, kPtsMaxValue))

    # first PTS smaller than second PTS by a lot
    self.assertEqual(-1, pts_cmp(0, 1000))
    self.assertEqual(-1, pts_cmp(10, 11000))
    self.assertEqual(-1, pts_cmp(423410, 3423411))
    self.assertEqual(-1, pts_cmp(498798798, 898798799))
    self.assertEqual(-1, pts_cmp(kPtsMaxValue - 100000, kPtsMaxValue))

  def testComparePtsWrapSmaller(self):
    # first PTS smaller than second PTS with wrap around
    # Test edge limit on the 1st PTS
    self.assertEqual(-1, pts_cmp(kPtsMaxValue, 0))
    self.assertEqual(-1, pts_cmp(kPtsMaxValue, 1))
    self.assertEqual(-1, pts_cmp(kPtsMaxValue, 1100))
    self.assertEqual(-1, pts_cmp(kPtsMaxValue, 99999))

    # Test edge limit on the 2nd PTS
    self.assertEqual(-1, pts_cmp(kPtsMaxValue - 1, 0))
    self.assertEqual(-1, pts_cmp(kPtsMaxValue - 2, 0))
    self.assertEqual(-1, pts_cmp(kPtsMaxValue - 1000, 0))
    self.assertEqual(-1, pts_cmp(kPtsMaxValue - 9999, 0))

    # Test close to edge limit on the 2nd PTS
    self.assertEqual(-1, pts_cmp(kPtsMaxValue - 1, 1))
    self.assertEqual(-1, pts_cmp(kPtsMaxValue - 2, 1))
    self.assertEqual(-1, pts_cmp(kPtsMaxValue - 1000, 1))
    self.assertEqual(-1, pts_cmp(kPtsMaxValue - 9999, 1))

    self.assertEqual(-1, pts_cmp(kPtsMaxValue - 1, 134234))
    self.assertEqual(-1, pts_cmp(kPtsMaxValue - 2, 43213123))
    self.assertEqual(-1, pts_cmp(kPtsMaxValue - 1000, 212321))
    self.assertEqual(-1, pts_cmp(kPtsMaxValue - 9999, 7842341))

  def testComparePtsWrapBigger(self):
    # first PTS bigger than second PTS with wrap around
    # Test edge limit on the 2nd PTS
    self.assertEqual(1, pts_cmp(0, kPtsMaxValue))
    self.assertEqual(1, pts_cmp(1, kPtsMaxValue))
    self.assertEqual(1, pts_cmp(1100, kPtsMaxValue))
    self.assertEqual(1, pts_cmp(999999, kPtsMaxValue))

    # Test edge limit on the first PTS
    self.assertEqual(1, pts_cmp(0, kPtsMaxValue - 1))
    self.assertEqual(1, pts_cmp(0, kPtsMaxValue - 2))
    self.assertEqual(1, pts_cmp(0, kPtsMaxValue - 1000))
    self.assertEqual(1, pts_cmp(0, kPtsMaxValue - 9999))

    # Test close to edge limit on the first PTS
    self.assertEqual(1, pts_cmp(1, kPtsMaxValue - 1))
    self.assertEqual(1, pts_cmp(1, kPtsMaxValue - 2))
    self.assertEqual(1, pts_cmp(1, kPtsMaxValue - 1000))
    self.assertEqual(1, pts_cmp(1, kPtsMaxValue - 9999))

    self.assertEqual(1, pts_cmp(13434, kPtsMaxValue - 1))
    self.assertEqual(1, pts_cmp(134234234, kPtsMaxValue - 2))
    self.assertEqual(1, pts_cmp(342341, kPtsMaxValue - 1000))
    self.assertEqual(1, pts_cmp(743451, kPtsMaxValue - 9999))

  def testMapPtsIntoSameTimeline(self):
    delta_pts = [
        -kHalfkPtsMaxValue, -kHalfkPtsMaxValue+1, -kHalfkPtsMaxValue+3,
        -kHalfkPtsMaxValue/2, -kHalfkPtsMaxValue/4, -kHalfkPtsMaxValue/8,
        -256, -100, -10, -3, -1, 0, 1, 3, 10, 100, 256,
        kHalfkPtsMaxValue/8, kHalfkPtsMaxValue/4, kHalfkPtsMaxValue/2,
        kHalfkPtsMaxValue-3, kHalfkPtsMaxValue-1, kHalfkPtsMaxValue,
    ]
    ref_pts = [
        0, 1, 3, 10, 100, 256,
        kPtsMaxValue/16-1, kPtsMaxValue/16, kPtsMaxValue/16+1,
        kPtsMaxValue/8-1, kPtsMaxValue/8, kPtsMaxValue/8+1,
        kPtsMaxValue/4-1, kPtsMaxValue/4, kPtsMaxValue/4+1,
        kPtsMaxValue/2-1, kPtsMaxValue/2, kPtsMaxValue/2+1,
        kPtsMaxValue-1, kPtsMaxValue,
    ]

    for r in ref_pts:
      for d in delta_pts:
        pts = pts_wrap_correction(r + d)
        self.assertGreaterEqual(pts, 0)
        self.assertLessEqual(pts, kPtsMaxValue)
        mapped_pts = map_pts_into_same_timeline(pts, r)
        self.assertEqual(r + d, mapped_pts)


if __name__ == '__main__':
  unittest.main()

