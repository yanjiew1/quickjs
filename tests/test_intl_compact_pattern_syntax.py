#!/usr/bin/env python3
"""Compact divisors, literal exact-count forms and ordinary-format fallbacks."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import compact_pattern_syntax as c
import number_pattern_syntax as n
from resolve import BLOCK


class CompactPatternSyntaxTests(unittest.TestCase):
    def test_numeric_divisors_and_quoted_literals(self):
        result = c.compile_pattern("00 'K{#}'", 4)
        self.assertEqual(result[:3], (c.SCALED, 3, 2))
        self.assertEqual(result[3], ((n.NUMBER, ''), (n.LITERAL, ' K{#}')))
        self.assertEqual(c.compile_pattern('000K', 5)[:3], (c.SCALED, 3, 3))
        self.assertEqual(c.compile_pattern('0万', 4)[:3], (c.SCALED, 4, 1))
        self.assertEqual(c.compile_pattern("'0'0'0'", 3)[3],
                         ((n.LITERAL, '0'), (n.NUMBER, ''), (n.LITERAL, '0')))

    def test_currency_signs_and_ordinary_fallback(self):
        result = c.compile_pattern('¤00K;¤00K-', 4)
        self.assertEqual(result[:3], (c.SCALED, 3, 2))
        self.assertEqual(result[4][-1], (n.MINUS, ''))
        self.assertEqual(result[5][-1], (n.PLUS, ''))
        self.assertEqual(c.compile_pattern('0', 0)[:3], (c.FALLBACK, 0, 1))
        self.assertEqual(c.compile_pattern('0', 19)[:3], (c.FALLBACK, 0, 1))

    def test_literal_only_forms_retain_baseline_divisor(self):
        result = c.compile_pattern('mille', 3, 3)
        self.assertEqual(result[:3], (c.LITERAL, 3, 0))
        self.assertEqual(result[3], ((n.LITERAL, 'mille'),))
        self.assertEqual(result[4], ((n.MINUS, ''), (n.LITERAL, 'mille')))
        self.assertEqual(c.compile_pattern("'0'", 3, 3)[3], ((n.LITERAL, '0'),))
        self.assertEqual(c.compile_pattern('single', 0, 0)[:3], (c.LITERAL, 0, 0))
        with self.assertRaises(DataError):
            c.compile_pattern('mille', 3)

    def test_sparse_maps_and_optional_markers(self):
        entries = ((3, 'other', 'standard', '0K'), (3, '1', 'standard', 'mille'),
                   (3, 'other', 'alphaNextToNumber', BLOCK), (3, 'other', 'noCurrency', ''))
        result = c.compile_map(entries)
        self.assertEqual(len(result), 4)
        self.assertEqual(result[1][3][:3], (c.LITERAL, 3, 0))
        self.assertEqual(result[2][3], BLOCK)
        self.assertEqual(result[3][3], '')
        self.assertEqual(c.compile_map(()), ())
        self.assertEqual(c.compile_inventory({'maps': (entries, ())}), (result, ()))

    def test_preserves_independent_divisors_and_fallbacks(self):
        result = c.compile_map(((3, 'other', 'standard', '0K'),
                                (3, 'one', 'standard', '0'),
                                (3, 'other', 'alphaNextToNumber', '00K')))
        self.assertEqual(tuple(row[3][:3] for row in result),
                         ((c.SCALED, 3, 1), (c.FALLBACK, 0, 1), (c.SCALED, 2, 2)))
        result = c.compile_map(((3, 'other', 'standard', '0'),
                                (3, 'other', 'alphaNextToNumber', '¤ 0K')))
        self.assertEqual(tuple(row[3][1] for row in result), (0, 3))

    def test_literal_uses_its_currency_family_divisor(self):
        result = c.compile_map(((3, 'other', 'standard', '¤0K'),
                                (3, 'other', 'alphaNextToNumber', '¤ 00K'),
                                (3, '1', 'alphaNextToNumber', '¤ single')))
        self.assertEqual(result[2][3][:3], (c.LITERAL, 2, 0))
        with self.assertRaises(DataError):
            c.compile_map(((3, 'other', 'standard', '¤0K'),
                           (3, '1', 'alphaNextToNumber', '¤ single')))

    def test_rejects_malformed_shapes_and_divisors(self):
        for value in ('', '0.0K', '#0K', '#,##0K', '@@K', '0E0K', '00000K', '0 0K',
                      '%0K', '0K‰', "0'", '\ud800'):
            with self.assertRaises(DataError, msg=repr(value)):
                c.compile_pattern(value, 3)
        for magnitude in (-1, 0x100000000, True, 3.0):
            with self.assertRaises(DataError):
                c.compile_pattern('0K', magnitude)
        for fields in (
                ((3, 'one', 'standard', '0K'),),
                ((3, 'other', 'standard', BLOCK),),
                ((3, 'other', 'standard', 'mille'),),
                ((3, 'other', 'standard', '0K'), (3, 'other', 'standard', '0K')),
                ((3, 'other', 'standard', '0K'), (3, 'unknown', 'standard', '0K')),
                ((3, 'other', 'standard', '0K'), (3, 'one', 'unknown', '0K'))):
            with self.assertRaises(DataError, msg=repr(fields)):
                c.compile_map(fields)


if __name__ == '__main__':
    unittest.main()
