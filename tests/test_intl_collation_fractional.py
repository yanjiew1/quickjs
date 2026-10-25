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

"""Logical source fixtures do not depend on installed ICU or host Unicode."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
import collation_fractional as fractional
from cldr_xml import DataError

SOURCE = '''[UCA version = 18.0.0]
[Unified_Ideograph 4E00..4E02]
[radical 1=一:丂一丁]
[radical end]
0000; [,,]
0061; [2A, 05, 05]
0041; [U+0061, 9C]
0062; [U+0061, 70, 20]
0063; [U+0041]
F900; [U+4E00]
F901; [U+4E01, 10]
F902; [U+4E02, 70, 20]
006C | 00B7; [, FB B6, 05]
FDD0 0034; [0F,,]
FDD1 5B57; [82 02 02, 05, 05]
[first primary ignorable [, 82, 05]]
[last primary ignorable [, FB B6, 05]]
[first tertiary in secondary non-ignorable [X, X, 03]]
[variable top = 0B FF FF FF]
[fixed secondary common byte 05]
[fixed tertiary common byte 05]
[top_byte 2A Latn]
[reorderingTokens Latn 2A=1]
'''


def parse(text=SOURCE):
    return fractional.parse(text.encode('utf-8'))


class FractionalTests(unittest.TestCase):
    def test_reference_fields_and_case(self):
        data = parse()
        rows = {row.text: row.elements for row in data['rows'] if not row.prefix}
        lower, upper = rows[(0x61,)][0], rows[(0x41,)][0]
        self.assertEqual(lower.primary, fractional.Primary('weight', (0x2a,)))
        self.assertEqual(upper.primary, lower.primary)
        self.assertEqual(upper.secondary, (5,))
        self.assertEqual(upper.tertiary, (0x9c,))
        # Removing independent case bits still leaves different ordinary T.
        self.assertNotEqual(upper.tertiary[0] & 0x3f, lower.tertiary[0])
        self.assertEqual(rows[(0x63,)], rows[(0x41,)])
        self.assertEqual(rows[(0x62,)][0].secondary, (0x70,))
        self.assertEqual(rows[(0xf900,)][0], fractional.Element(
            fractional.Primary('han', 0x4e00), (5,), (5,)))
        self.assertEqual(rows[(0xf901,)][0].secondary, (5,))
        self.assertEqual(rows[(0xf901,)][0].tertiary, (0x10,))
        self.assertEqual(rows[(0xf902,)][0].secondary, (0x70,))
        self.assertEqual(rows[(0xf902,)][0].tertiary, (0x20,))
        self.assertEqual(data['references'], 6)
        explicit = parse(SOURCE.replace('F900;', '4E00; [2B,05,05]\nF900;'))
        rows = {row.text: row.elements for row in explicit['rows'] if not row.prefix}
        self.assertEqual(rows[(0xf900,)], rows[(0x4e00,)])

    def test_context_synthetic_and_limits(self):
        data = parse()
        context = next(row for row in data['rows'] if row.prefix)
        self.assertEqual((context.prefix, context.text), ((0x6c,), (0xb7,)))
        self.assertEqual(context.elements[0].secondary, (0xfb, 0xb6))
        self.assertEqual(data['synthetic'][0].text, (0xfdd0, 0x34))
        self.assertEqual(data['synthetic'][0].elements[0].secondary, ())
        self.assertTrue(any(row.text == (0xfdd1, 0x5b57) for row in data['rows']))
        self.assertEqual(data['limits']['first tertiary in secondary non-ignorable'],
                         (None, None, (3,)))
        self.assertEqual(data['variable_top'], (0x0b, 0xff, 0xff, 0xff))
        self.assertEqual(fractional.summary(data)['prefixes'], 1)

    def test_declared_vs_radical_order(self):
        data = parse()
        self.assertEqual(data['ideographs'], ((0x4e00, 0x4e02),))
        self.assertEqual(data['han_order'], (0x4e02, 0x4e00, 0x4e01))
        self.assertEqual(parse(SOURCE.replace('丂一丁', '一-丂'))['han_order'],
                         (0x4e00, 0x4e01, 0x4e02))
        for bad in ('一丁', '一丁丁', '一丁七', '丂-一', '一-'):
            with self.subTest(radical=bad), self.assertRaises(DataError):
                parse(SOURCE.replace('丂一丁', bad))
        for bad in ('4E00..4E02 4E01', '4E02..4E00', 'D7FF..E001', ''):
            with self.subTest(ranges=bad), self.assertRaises(DataError):
                parse(SOURCE.replace('4E00..4E02', bad))

    def test_reference_rejection(self):
        for old, new in (
                ('U+0061, 9C', 'U+0063, 9C'),  # Forward reference.
                ('U+4E00]', 'U+4E03]'),        # Not declared implicit Han.
                ('U+0061, 9C', 'U+0061, 05, 05, 05'),
                ('U+0061, 9C', 'U+0061,,05'),  # P without S violates WF1.
                ('0061;', '006C | 0061;'),    # Prefix does not define plain scalar.
                ('[2A, 05, 05]', '[2A,05,05][2B,05,05]')):
            with self.subTest(replacement=new), self.assertRaises(DataError):
                parse(SOURCE.replace(old, new))

    def test_supplementary_and_distinct_prefixes(self):
        text = SOURCE.replace('4E00..4E02', '4E00..4E02 20000..20001')
        text = text.replace('丂一丁', '丂一丁𠀀-𠀁')
        text = text.replace('F900;', 'F903; [U+20001]\nF900;')
        data = parse(text)
        self.assertEqual(data['han_order'][-2:], (0x20000, 0x20001))
        self.assertEqual(next(row for row in data['rows'] if row.text == (0xf903,)).elements[0].primary,
                         fractional.Primary('han', 0x20001))
        text = SOURCE.replace('006C | 00B7; [, FB B6, 05]',
                              '006C | 00B7; [, FB B6, 05]\n0078 | 00B7; [, 82, 05]')
        contexts = [row for row in parse(text)['rows'] if row.prefix]
        self.assertEqual([row.prefix for row in contexts], [(0x6c,), (0x78,)])
        self.assertNotEqual(contexts[0].elements, contexts[1].elements)

    def test_mapping_rejection(self):
        for old, new in (
                ('0063;', '0061;'),
                ('006C | 00B7;', ' | 00B7;'),
                ('006C | 00B7;', '006C | ;'),
                ('006C | 00B7;', '006C | 0061 | 00B7;'),
                ('0063;', 'D800;'),
                ('[2A, 05, 05]', '[00,05,05]'),
                ('[2A, 05, 05]', '[2A,05]'),
                ('[2A, 05, 05]', '[2A,,05]'),
                ('[2A, 05, 05]', 'garbage[2A,05,05]'),
                ('[2A, 05, 05]', '[[2A,05,05]]')):
            with self.subTest(replacement=new), self.assertRaises(DataError):
                parse(SOURCE.replace(old, new))

    def test_version_and_directives(self):
        for old, new in (
                ('18.0.0', '17.0.0'),
                ('[radical end]', ''),
                ('[radical end]', '[radical end]\n[radical end]'),
                ('[variable top = 0B FF FF FF]', '[unknown directive]'),
                ('[variable top = 0B FF FF FF]', '[variable top = ]'),
                ('[fixed secondary common byte 05]', '[fixed secondary common byte 06]'),
                ('[top_byte 2A Latn]', '[top_byte 2A Latn'),
                ('[UCA version = 18.0.0]', '[UCA version = 18.0.0]\n[UCA version = 18.0.0]')):
            with self.subTest(replacement=new), self.assertRaises(DataError):
                parse(SOURCE.replace(old, new))
        with self.assertRaises(DataError):
            fractional.parse(b'\xff')

    def test_long_weights_and_source_owner(self):
        data = parse(SOURCE.replace('[2A, 05, 05]', '[2A 03 04 05 06,05,05]'))
        self.assertEqual(next(row for row in data['rows'] if row.text == (0x61,)).elements[0].primary.value,
                         (0x2a, 3, 4, 5, 6))
        class Sources:
            def read(self, path):
                self.path = path
                return SOURCE.encode('utf-8')
        source = Sources()
        self.assertEqual(fractional.collect(source), parse())
        self.assertEqual(source.path, fractional.SOURCE)


if __name__ == '__main__':
    unittest.main()
