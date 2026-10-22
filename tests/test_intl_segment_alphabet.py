#!/usr/bin/env python3
"""Overlapping classes, simultaneous endpoints and full code-point coverage."""
from pathlib import Path
import random
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import segment_alphabet as alphabet


def ranges(values):
    result = []
    for value in sorted(values):
        if result and result[-1][1] == value:
            result[-1] = result[-1][0], value + 1
        else:
            result.append((value, value + 1))
    return tuple(result)


class SegmentAlphabetTests(unittest.TestCase):
    def test_overlap_gaps_reused_classes_and_surrogate_endpoints(self):
        predicates = (((0, 3), (0xd800, 0xe000)), ((2, 5), (0xdfff, 0x10000)))
        result = alphabet.partition(predicates)
        self.assertEqual(result.predicates, predicates)
        self.assertEqual(result.masks, (1, 3, 2, 0))
        self.assertEqual(result.ranges, ((0, 2, 0), (2, 3, 1), (3, 5, 2), (5, 0xd800, 3),
                                         (0xd800, 0xdfff, 0), (0xdfff, 0xe000, 1),
                                         (0xe000, 0x10000, 2), (0x10000, 0x110000, 3)))
        for code in (0, 2, 4, 5, 0xd800, 0xdfff, 0xe000, 0x10000, 0x10ffff):
            expected = sum(1 << index for index, rows in enumerate(predicates)
                           if any(start <= code < end for start, end in rows))
            self.assertEqual(result.masks[alphabet.classify(result, code)], expected)

    def test_realized_signatures_match_independent_small_domain_oracle(self):
        randoms = random.Random(49018)
        for _ in range(100):
            sets = [{code for code in range(32) if randoms.randrange(2)} for _ in range(5)]
            result = alphabet.partition(tuple(ranges(values) for values in sets))
            for code in range(33):
                expected = sum(1 << i for i, values in enumerate(sets) if code in values)
                self.assertEqual(result.masks[alphabet.classify(result, code)], expected)
            self.assertEqual(result.ranges[0][0], 0)
            self.assertEqual(result.ranges[-1][1], 0x110000)
            self.assertTrue(all(left[1] == right[0] for left, right in zip(result.ranges, result.ranges[1:])))

    def test_empty_full_duplicate_and_simultaneously_changing_predicates(self):
        self.assertEqual(alphabet.partition(()).ranges, ((0, 0x110000, 0),))
        self.assertEqual(alphabet.partition(()).masks, (0,))
        self.assertEqual(alphabet.partition((((0, 0x110000),),)).masks, (1,))
        self.assertEqual(alphabet.partition(((), ())).masks, (0,))
        duplicate = alphabet.partition((((1, 2),), ((1, 2),)))
        self.assertEqual(duplicate.masks, (0, 3))
        adjacent = alphabet.partition((((0, 1),), ((1, 2),)))
        self.assertEqual(adjacent.ranges[:2], ((0, 1, 0), (1, 2, 1)))
        self.assertEqual(adjacent.masks, (1, 2, 0))

    def test_deterministic_output_does_not_depend_on_host_unicode(self):
        predicates = (((0, 1), (0x10ffff, 0x110000)), ((0xd800, 0xe000),))
        self.assertEqual(alphabet.partition(predicates), alphabet.partition(predicates))

    def test_all_budgets_and_invalid_input_fail_without_partial_output(self):
        predicates = (((0, 1), (4, 5)), ((2, 3),))
        for limits, message in [
                (alphabet.DEFAULT_LIMITS._replace(predicates=1), 'predicate budget'),
                (alphabet.DEFAULT_LIMITS._replace(pairs=2), 'pair budget'),
                (alphabet.DEFAULT_LIMITS._replace(endpoints=2), 'endpoint budget'),
                (alphabet.DEFAULT_LIMITS._replace(classes=1), 'class budget'),
                (alphabet.DEFAULT_LIMITS._replace(ranges=1), 'range budget'),
                (alphabet.DEFAULT_LIMITS._replace(work=1), 'work budget')]:
            with self.subTest(message=message), self.assertRaisesRegex(DataError, message):
                alphabet.partition(predicates, limits)
        for predicates in ((((True, 2),),), (((0, 1), (1, 2)),), (((-1, 2),),)):
            with self.subTest(predicates=predicates), self.assertRaises(DataError):
                alphabet.partition(predicates)
        with self.assertRaises(DataError):
            alphabet.partition((), alphabet.DEFAULT_LIMITS._replace(work=True))
        for code in (-1, 0x110000, True):
            with self.subTest(code=code), self.assertRaises(DataError):
                alphabet.classify(alphabet.partition(()), code)

    def test_remaining_pair_and_work_caps_precede_range_validation_copy(self):
        original = alphabet.checked_ranges
        with mock.patch.object(alphabet, 'checked_ranges', wraps=original) as validate:
            with self.assertRaisesRegex(DataError, 'pair budget'):
                alphabet.partition((((0, 1),), ((2, 3), (4, 5))),
                                   alphabet.DEFAULT_LIMITS._replace(pairs=2))
            self.assertEqual(validate.call_count, 1)
        large = (tuple((i * 2, i * 2 + 1) for i in range(1000)),)
        with mock.patch.object(alphabet, 'checked_ranges', wraps=original) as validate:
            with self.assertRaisesRegex(DataError, 'work budget'):
                alphabet.partition(large, alphabet.DEFAULT_LIMITS._replace(work=1))
            validate.assert_not_called()


if __name__ == '__main__':
    unittest.main()
