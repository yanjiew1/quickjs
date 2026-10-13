"""Unexecuted boundary-policy fixtures with synthetic source pieces."""
import datetime as dt
from dataclasses import replace
import unittest
from prepare_pmo_months import EPOCH, InvalidSource, Month
from prepare_boundary_provider import merge, prove_sequence, window


def full_year(first, year):
    result, day = [], first
    for nominal in range(1, 13):
        length = 29 if nominal % 2 else 30
        result.append(Month(day, year, nominal, False, length))
        day += length
    return result, Month(day, year + 1, 1, False, 0)


class Fixtures(unittest.TestCase):
    def test_partial_primary_1899_can_complete_metadata_outside_window(self):
        first, unused_end = window('chinese')
        approximate, sentinel = full_year(first - 324, 1899)
        primary = [approximate[-1], replace(sentinel, length=29)]
        records, spans, bounds = merge(primary, approximate, sentinel, 'chinese', 1899)
        self.assertEqual(len(records), 13)
        self.assertEqual(spans, [[first, first + 59]])
        self.assertEqual(len(bounds['accepted_approximate_months']), 11)
        self.assertFalse(bounds['entire_mandatory_window_primary_covered'])

    def test_missing_required_month_cannot_be_filled_by_approximation(self):
        first, unused_end = window('chinese')
        approximate, sentinel = full_year(first - 324, 1899)
        with self.assertRaises(InvalidSource):
            merge([replace(sentinel, length=29)], approximate, sentinel, 'chinese', 1899)

    def test_one_day_join_gap_and_wrong_leap_fail(self):
        first, unused_end = window('chinese')
        approximate, sentinel = full_year(first - 324, 1899)
        for primary in ([replace(approximate[-1], epoch_day=first + 1, length=29), replace(sentinel, length=29)],
                        [replace(approximate[-1], is_leap=True), replace(sentinel, length=29)]):
            with self.assertRaises(InvalidSource):
                merge(primary, approximate, sentinel, 'chinese', 1899)

    def test_published_2050_piece_crosses_upper_bound_approximation_tail_does_not(self):
        unused_first, end = window('dangi')
        approximate, sentinel = full_year(end + 10 - 324, 2050)
        primary = approximate[:-1]
        records, spans, bounds = merge(primary, approximate, sentinel, 'dangi', 2050)
        self.assertEqual(records[-2].month, 12)
        self.assertEqual(records[-2].epoch_day, end + 10)
        self.assertEqual(spans[-1][1], end + 10)
        self.assertEqual(len(bounds['accepted_approximate_months']), 1)
        self.assertEqual(bounds['accepted_approximate_months'][0]['epoch_day'], end + 10)

    def test_next_m01_inside_required_window_needs_primary_point(self):
        first, unused_end = window('chinese')
        approximate, sentinel = full_year(first - 324, 1899)
        with self.assertRaises(InvalidSource):
            merge([approximate[-1]], approximate, sentinel, 'chinese', 1899)

    def test_wrong_arithmetic_year_and_duplicate_month_fail(self):
        first, unused_end = window('chinese')
        approximate, sentinel = full_year(first - 324, 1899)
        for bad in ([replace(approximate[0], lunar_year=1900)] + approximate[1:] + [sentinel],
                    approximate + [approximate[-1], sentinel]):
            with self.assertRaises(InvalidSource):
                prove_sequence(bad)


if __name__ == '__main__':
    unittest.main()
