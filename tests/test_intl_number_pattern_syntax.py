#!/usr/bin/env python3
# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""Typed LDML numeric skeletons, quoted affixes and independent sign patterns."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import number_pattern_syntax as p
from resolve import BLOCK


class NumberPatternSyntaxTests(unittest.TestCase):
    def test_grouping_and_digit_metadata(self):
        numeric, positive, negative, plus = p.compile_pattern('#,##,##0.00##')
        self.assertEqual(numeric, (3, 2, 1, 6, 2, 4, 1, 0, 0))
        self.assertEqual(positive, ((p.NUMBER, ''),))
        self.assertEqual(negative, ((p.MINUS, ''), (p.NUMBER, '')))
        self.assertEqual(plus, ((p.PLUS, ''), (p.NUMBER, '')))
        self.assertEqual(p.compile_pattern('#,###,####')[0][:2], (4, 3))
        self.assertEqual(p.compile_pattern('0.')[0][4:7], (0, 0, 1))
        self.assertEqual(p.compile_pattern('#')[0][2:4], (0, 1))

    def test_scientific_skeleton_and_affixes(self):
        numeric, positive, negative, plus = p.compile_pattern('[#E0]')
        self.assertEqual(numeric, (0, 0, 0, 1, 0, 0, 0, 1, 0))
        self.assertEqual(positive, ((p.LITERAL, '['), (p.NUMBER, ''), (p.LITERAL, ']')))
        self.assertEqual(negative[1:], positive)
        self.assertEqual(p.compile_pattern('0.000000E+000')[0][-2:], (3, 1))
        self.assertEqual(p.compile_pattern('E0E')[1],
                         ((p.LITERAL, 'E'), (p.NUMBER, ''), (p.LITERAL, 'E')))
        self.assertEqual(p.compile_pattern('0E+')[1],
                         ((p.NUMBER, ''), (p.LITERAL, 'E'), (p.PLUS, '')))

    def test_quotes_braces_and_literal_hyphens(self):
        positive = p.compile_pattern("'{#¤-;}'0 o''clock")[1]
        self.assertEqual(positive, ((p.LITERAL, '{#¤-;}'), (p.NUMBER, ''),
                                    (p.LITERAL, " o'clock")))
        self.assertEqual(p.compile_pattern("'-'0")[2],
                         ((p.MINUS, ''), (p.LITERAL, '-'), (p.NUMBER, '')))
        self.assertEqual(p.compile_pattern("0';'")[1][-1], (p.LITERAL, ';'))
        self.assertEqual(p.compile_pattern("0'12'")[1][-1], (p.LITERAL, '12'))
        self.assertEqual(p.compile_pattern("0''")[1][-1], (p.LITERAL, "'"))

    def test_negative_affixes_do_not_change_numeric_settings(self):
        numeric, positive, negative, plus = p.compile_pattern('#,##0.0#;(#)')
        self.assertEqual(numeric[:6], (3, 3, 1, 4, 1, 2))
        self.assertEqual(negative, ((p.LITERAL, '('), (p.NUMBER, ''), (p.LITERAL, ')')))
        self.assertEqual(plus, positive)
        self.assertEqual(p.compile_pattern('0.00;0')[2], ((p.NUMBER, ''),))
        self.assertEqual(p.compile_pattern('0;')[1:], p.compile_pattern('0;;;')[1:])
        self.assertEqual(p.compile_pattern('0+;0')[3],
                         ((p.NUMBER, ''), (p.PLUS, '')))

    def test_suffix_sign_and_currency_widths(self):
        for count, kind in ((1, p.CURRENCY), (2, p.CURRENCY_ISO),
                            (3, p.CURRENCY_NAME), (5, p.CURRENCY_NARROW)):
            value = '¤' * count + ' 0;¤' + '0-'
            positive, negative, plus = p.compile_pattern(value)[1:]
            self.assertEqual(positive[0], (kind, ''))
            self.assertEqual(plus[-1], (p.PLUS, ''))
            self.assertEqual(negative[-1], (p.MINUS, ''))
        self.assertEqual(p.compile_pattern('% 0‰')[1],
                         ((p.PERCENT, ''), (p.LITERAL, ' '), (p.NUMBER, ''), (p.PERMILLE, '')))
        self.assertEqual(p.compile_pattern('\u200f0;\u200f-0')[3],
                         ((p.LITERAL, '\u200f'), (p.PLUS, ''), (p.NUMBER, '')))

    def test_inventory_preserves_optional_markers(self):
        data = {'rows': (('root', 'latn', ('0', None, '', BLOCK, '0;0')),)}
        self.assertEqual(set(p.compile_inventory(data)), {'0', '0;0'})

    def test_rejects_malformed_or_unsupported_syntax(self):
        for value in ('', 'text', "'0", "0'", '0;0;0', ';0', '0; ',
                      '0#', '#0#', '0.#0', ',0', '0,', '#,,0', '0..0', '0.0.0',
                      '#,##0E0', '0E00.0', '0E0#', "0'.'0", "0'0'0",
                      '@@', '0.05', '*x0', '¤¤¤¤0', '¤¤¤¤¤¤0', '0\0', '\ud8000'):
            with self.assertRaises(DataError, msg=repr(value)):
                p.compile_pattern(value)


if __name__ == '__main__':
    unittest.main()
