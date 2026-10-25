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

"""Typed field aliases and inheritance markers, with authored record fixtures."""
from pathlib import Path
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[1] / 'tools' / 'intl'
sys.path.insert(0, str(TOOLS))
from cldr_xml import DataError
from parents import ParentGraph
from resolve import BLOCK, INHERIT, Records


def graph():
    return ParentGraph(('root', 'en', 'en-GB', 'en-001', 'de'),
                       {'en-GB': 'en-001'}, {'plurals': {}},
                       {'und': 'en-Latn-US'}, (), False)


class ResolutionTests(unittest.TestCase):
    def test_inherited_alias_restarts_at_requester(self):
        records = Records(graph(), (('short',), ('wide',)))
        records.alias('root', ('short',), ('wide',))
        records.add('root', ('wide',), 'root text')
        records.add('en', ('wide',), 'English text')
        records.add('en-GB', ('wide',), 'British text')
        result = records.require('en-GB', ('short',))
        self.assertEqual((result.value, result.locale, result.key),
                         ('British text', 'en-GB', ('wide',)))
        self.assertEqual(records.require('de', ('short',)).value, 'root text')

    def test_explicit_locale_alias_changes_resolution_anchor(self):
        records = Records(graph(), (('short',), ('wide',), ('full',)))
        records.alias('en-GB', ('short',), ('wide',), source='de')
        records.alias('root', ('wide',), ('full',))
        records.add('de', ('full',), 'German text')
        records.add('en-GB', ('full',), 'British text')
        self.assertEqual(records.require('en-GB', ('short',)).value, 'German text')

    def test_inheritance_and_empty_override_are_distinct(self):
        records = Records(graph(), (('field',), ('missing',), ('empty',)))
        records.add('root', ('field',), 'root text')
        records.add('en', ('field',), INHERIT)
        self.assertEqual(records.require('en', ('field',)).locale, 'root')
        records.add('en-GB', ('field',), BLOCK)
        result = records.resolve('en-GB', ('field',))
        self.assertTrue(result.blocked)
        self.assertIsNone(result.value)
        with self.assertRaises(DataError):
            records.require('en-GB', ('field',))
        self.assertIsNone(records.resolve('en', ('missing',)))
        records.add('en', ('empty',), '')
        self.assertEqual(records.require('en', ('empty',)).value, '')

    def test_component_parent_selection(self):
        main = Records(graph(), (('field',),))
        plural = Records(graph(), (('field',),), 'plurals')
        for records in (main, plural):
            records.add('en-001', ('field',), 'world')
            records.add('en', ('field',), 'English')
        self.assertEqual(main.require('en-GB', ('field',)).value, 'world')
        self.assertEqual(plural.require('en-GB', ('field',)).value, 'English')

    def test_cycles_and_unknown_references(self):
        records = Records(graph(), (('a',), ('b',)))
        records.alias('root', ('a',), ('b',))
        records.alias('root', ('b',), ('a',))
        with self.assertRaises(DataError):
            records.resolve('en-GB', ('a',))
        records = Records(graph(), (('a',), ('b',)))
        records.alias('en', ('a',), ('b',), 'de')
        records.alias('de', ('b',), ('a',), 'en')
        with self.assertRaises(DataError):
            records.resolve('en', ('a',))
        with self.assertRaises(DataError):
            records.alias('root', ('b',), ('unknown',))
        with self.assertRaises(DataError):
            records.alias('root', ('b',), ('a',), 'unknown')
        with self.assertRaises(DataError):
            records.add('root', ('b',), None)
        records.add('root', ('b',), 'value')
        with self.assertRaises(DataError):
            records.alias('root', ('b',), ('a',))

    def test_long_finite_alias_chain_is_iterative(self):
        keys = tuple((index,) for index in range(1001))
        records = Records(graph(), keys)
        for index in range(1000):
            records.alias('root', keys[index], keys[index + 1])
        records.add('root', keys[-1], 'last')
        self.assertEqual(records.require('en-GB', keys[0]).value, 'last')


if __name__ == '__main__':
    unittest.main()
