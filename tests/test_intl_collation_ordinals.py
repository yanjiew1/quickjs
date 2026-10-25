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

"""Fractional coordinates preserve logical classes, source order and virtual gaps."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'intl'))
from cldr_xml import DataError
import collation_fractional as fractional
import collation_ordinals as ordinals
from test_intl_collation_fractional import SOURCE

ROOT = SOURCE + '''
FFFE; [02,05,05]
0027; [04,05,05]
2019; [04,73,05]
[fixed first implicit byte E0]
[fixed last implicit byte E4]
[fixed first trail byte E5]
[fixed first ignorable secondary byte 80]
[fixed first ignorable tertiary byte 3C]
[first secondary ignorable [,,3D 02]]
[last secondary ignorable [,,3D 02]]
[last regular [7F,05,05]]
'''


def catalog(text=ROOT):
    return ordinals.Catalog(fractional.parse(text.encode('utf-8')))


def element(data, code):
    return next(row.elements[0] for row in data.data['rows'] if not row.prefix and row.text == (code,))


class OrdinalTests(unittest.TestCase):
    def test_source_order_and_bands(self):
        data = catalog()
        ordered = sorted(data.primaries)
        self.assertEqual([data.primaries[key] for key in ordered],
                         sorted(data.primaries.values()))
        self.assertEqual(data.secondary_common, 32)
        self.assertEqual(data.tertiary_common, 2)
        self.assertEqual(data.tertiaries[(3,)], 1)
        self.assertLess(data.secondary_common, data.secondary_boundary)
        self.assertLess(data.tertiary_common, data.tertiary_boundary)
        pi = fractional.Element(fractional.Primary('weight', ()), (0x82,), (5,))
        si = fractional.Element(fractional.Primary('weight', ()), (), (0x3d, 2))
        self.assertGreater(data.coordinate(pi).secondary, data.secondary_boundary)
        self.assertGreater(data.coordinate(si).tertiary, data.tertiary_boundary)
        self.assertEqual(data.coordinate(si).case, ordinals.UPPER)
        zero = data.coordinate(element(data, 0))
        self.assertEqual(tuple(zero), (0, 0, 0, ordinals.LOWER, False, False))

    def test_case_and_variable_policy(self):
        data = catalog()
        lower = data.coordinate(element(data, 0x61))
        upper = data.coordinate(element(data, 0x41))
        self.assertEqual(lower.primary, upper.primary)
        self.assertNotEqual(lower.tertiary, upper.tertiary)
        self.assertEqual((lower.case, upper.case), (ordinals.LOWER, ordinals.UPPER))
        quote = data.coordinate(element(data, 0x2019))
        self.assertTrue(quote.variable)
        self.assertLess(quote.secondary, data.secondary_boundary)
        separator = data.coordinate(element(data, 0xfffe))
        self.assertFalse(separator.variable)  # Shifted mode must retain U+FFFE.
        self.assertEqual(separator.primary, data.field_separator)
        # U+2019 remains ONE ordinary logical CE, never a synthetic PI fragment.
        row = next(row for row in data.data['rows'] if row.text == (0x2019,))
        self.assertEqual(len(row.elements), 1)
        self.assertEqual(data.last_regular, data.primaries[data.han_weight])
        self.assertNotEqual(data.last_regular, data.primaries[(0x7f,)])

    def test_virtual_han_and_generic(self):
        data = catalog()
        # Authored radical order is 丂,一,丁, deliberately not code-point order.
        self.assertEqual([data.implicit(code).primary for code in (0x4e02, 0x4e00, 0x4e01)],
                         [data.han_base + 1, data.han_base + 2, data.han_base + 3])
        self.assertEqual(data.coordinate(element(data, 0xf900)), data.implicit(0x4e00))
        for code in (0, 1, 0xd800, 0xe000, 0x10ffff):
            ce = data.implicit(code)
            self.assertEqual(ce.primary, data.generic_base + code + 1)
            self.assertLess(ce.primary, data.generic_limit)
            self.assertEqual((ce.secondary, ce.tertiary), (32, 2))
            self.assertTrue(ce.virtual)
            self.assertFalse(ce.variable)
        self.assertEqual(data.generic_limit, data.generic_base + 0x110001)
        self.assertLess(data.last_regular + len(data.han), data.implicit_first_marker)
        self.assertEqual(data.implicit_last_marker, data.primaries[(0xe4,)])
        self.assertEqual(data.generic_base, data.implicit_last_marker)
        self.assertLess(data.implicit_first_marker, data.generic_base)
        self.assertEqual(data.implicit(0).primary, data.generic_base + 1)
        self.assertEqual(data.implicit(0x10ffff).primary, data.generic_limit - 1)
        self.assertEqual(data.primaries[(0xe5,)], data.generic_limit)
        # The unassigned-group marker stays literal; it is not an implicit CE.
        marker = fractional.Element(fractional.Primary('weight', (0xe4,)), (5,), (5,))
        self.assertEqual(data.coordinate(marker).primary, data.generic_base)
        self.assertFalse(data.coordinate(marker).virtual)
        self.assertLess(data.primary_max, 0x80000000)
        # Generic implicit coverage is arithmetic; the literal dictionary stays small.
        self.assertLess(len(data.primaries), 32)
        for code in (-1, 0x110000, None):
            with self.subTest(code=code), self.assertRaises(DataError):
                data.implicit(code)

    def test_invalid_class_and_overlapping_domains(self):
        for extra in ('0342; [,70,05]', '0342; [2C,82,05]',
                      '0342; [,,05]', '0342; [2C,05,3D 02]',
                      '0342; [90,05,05]', '0342; [E1,05,05]',
                      '0342; [2C,05,C5]', '0342; [E4 01,05,05]'):
            with self.subTest(extra=extra), self.assertRaises(DataError):
                catalog(ROOT + extra + '\n')

    def test_required_metadata(self):
        for old, new in (('FDD1 5B57;', 'FDD1 5B58;'),
                         ('FDD0 0034;', 'FDD0 0035;'),
                         ('FDD0 0034; [0F,,]', 'FDD0 0034; [2A,,]'),
                         ('FFFE;', 'FFFD;'),
                         ('[fixed first ignorable tertiary byte 3C]', ''),
                         ('[fixed first implicit byte E0]', '[fixed first implicit byte 80]')):
            with self.subTest(new=new), self.assertRaises(DataError):
                catalog(ROOT.replace(old, new))

    def test_complete_numeric_reservation(self):
        self.assertEqual(catalog().summary(),
                         catalog(ROOT.replace('FDD0 0034; [0F,,]',
                                              'FDD0 0034; [0F,05,05]')).summary())

    def test_variable_length_primary_order(self):
        data = catalog(ROOT + '0342; [0E FF,05,05]\n0343; [0F 03,05,05]\n')
        # The reserved numeric prefix precedes a longer ordinary key, while a
        # shorter previous lead byte still sorts before both.
        self.assertLess(data.primaries[(0x0e, 0xff)], data.numeric)
        self.assertLess(data.numeric, data.primaries[(0x0f, 3)])
        self.assertLess(data.primaries[(0x0f, 3)], data.primaries[(0x2a,)])


if __name__ == '__main__':
    unittest.main()
